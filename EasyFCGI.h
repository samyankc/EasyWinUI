#ifndef H_EASY_FCGI_
#define H_EASY_FCGI_

#define NO_FCGI_DEFINES
#include <fcgi_stdio.h>
#include <memory>

#include "json.hpp"
#include "BijectiveMap.hpp"
#include "EasyString.h"

//helper functions
namespace
{
    [[nodiscard]] auto DecodeURLFragment( std::string_view Fragment )
    {
        constexpr auto EncodeDigitWidth = 2;
        auto Result = std::string{};

        auto InputIt = std::begin( Fragment );
        auto EndIt = std::end( Fragment );
        auto OutputIt = std::back_inserter( Result );

        while( InputIt != EndIt )
        {
            auto CurrentChar = *InputIt;
            if( CurrentChar == '+' )
                CurrentChar = ' ';
            else if( CurrentChar == '%' && std::distance( InputIt, EndIt ) > EncodeDigitWidth )
                CurrentChar = std::bit_cast<char>(
                    EasyString::StrViewTo<unsigned char, 16>( { ( ++InputIt )++, EncodeDigitWidth } ).value_or( '?' ) );
            OutputIt = CurrentChar;
            ++InputIt;
        }

        return Result;
    }
}  // namespace

inline namespace EasyFCGI
{
    using namespace EasyString;

    template<typename... Args>
    inline auto Send( std::format_string<Args...> fmt, Args&&... args )
    {
        return FCGI_puts( std::format( fmt, std::forward<Args>( args )... ).c_str() );
    }

    inline auto Send( std::string_view Content ) { return FCGI_puts( std::c_str( Content ) ); }

    template<typename T>
    concept HasSendableDump = requires( T t ) { Send( t.dump() ); };

    inline auto Send( HasSendableDump auto&& Content ) { return Send( Content.dump() ); }

    namespace HTTP
    {
        struct RequestMethod
        {
            enum class VerbType { INVALID = 0, GET, HEAD, POST, PUT, DELETE, CONNECT, OPTIONS, TRACE, PATCH } Verb;

            using enum VerbType;
            inline const static auto StrViewToVerb = BijectiveMap<std::string_view, VerbType>  //
                {                                                                              //
                  { "GET", GET },  { "POST", POST },    { "CONNECT", CONNECT }, { "TRACE", TRACE },
                  { "PUT", PUT },  { "HEAD", HEAD },    { "OPTIONS", OPTIONS }, { "PATCH", PATCH },
                  { "", INVALID }, { "DELETE", DELETE }
                };

            inline const static auto VerbToStrView = StrViewToVerb.Inverse();

            constexpr RequestMethod() = default;
            constexpr RequestMethod( const RequestMethod& ) = default;
            constexpr RequestMethod( VerbType OtherVerb ) : Verb{ OtherVerb } {}
            constexpr RequestMethod( std::convertible_to<std::string_view> auto VerbName )
                : Verb{ StrViewToVerb[VerbName] }
            {}

            constexpr operator std::string_view() const { return VerbToStrView[Verb]; }

            constexpr operator VerbType() const { return Verb; }
        };
    }  // namespace HTTP

    inline auto ReadParam( const char* BuffPtr ) -> std::string_view
    {
        auto LoadParamFromEnv = getenv( BuffPtr );
        return LoadParamFromEnv ? LoadParamFromEnv : "";
    }

    inline auto ReadParam( std::string_view ParamName ) { return ReadParam( std::c_str( ParamName ) ); }

    using Json = nlohmann::json;

    template<typename StorageEngine>
    struct QueryExecutor
    {
        StorageEngine Json;
        auto operator[]( std::string_view Key ) const
        {
            if( Json.contains( Key ) )
                return Json[Key].template get<std::string>();
            else
                return std::string{};
        }
    };

    // template<typename Json = nlohmann::json>
    struct FCGI_Request
    {
        HTTP::RequestMethod RequestMethod;
        std::size_t ContentLength;
        std::string_view ContentType;
        std::string_view ScriptName;
        std::string_view RequestURI;
        std::string_view QueryString;
        std::string_view RequestBody;
        QueryExecutor<Json> Query;
    };

    inline auto QueryStringToJson( std::string_view Source )
    {
        auto Result = Json{};

        for( auto Segment : Source | SplitBy( '&' ) )                        //
            for( auto [Key, Value] : Segment | SplitBy( '=' ) | Bundle<2> )  //
                Result[DecodeURLFragment( Key )] = DecodeURLFragment( Value );

        return Result;
    }

    inline auto NextRequest()
    {
        auto R = Json{};

        auto ContentLength = StrViewTo<std::size_t>( ReadParam( "CONTENT_LENGTH" ) ).value_or( 0 );
        auto RequestMethod = HTTP::RequestMethod( ReadParam( "REQUEST_METHOD" ) );
        auto ContentType = ReadParam( "CONTENT_TYPE" );
        auto ScriptName = ReadParam( "SCRIPT_NAME" );
        auto RequestURI = ReadParam( "REQUEST_URI" );
        auto QueryString = ReadParam( "QUERY_STRING" );
        auto RequestBody = ReadParam( "REQUEST_BODY" );
        auto Query = Json{};

        // ignore original request body in case of GET
        if( RequestMethod == HTTP::RequestMethod::GET )
            RequestBody = QueryString;
        else
            QueryString = RequestBody;

        if( ContentType.contains( "application/json" ) )
            Query = Json::parse( RequestBody );
        else if( RequestMethod == HTTP::RequestMethod::GET ||
                 ContentType.contains( "application/x-www-form-urlencoded" ) )
            Query = QueryStringToJson( QueryString );
        else
            Query = QueryStringToJson( QueryString );

        return FCGI_Request{ .RequestMethod = RequestMethod,
                             .ContentLength = ContentLength,
                             .ContentType = ContentType,
                             .ScriptName = ScriptName,
                             .RequestURI = RequestURI,
                             .QueryString = QueryString,
                             .RequestBody = RequestBody,
                             .Query = { Query } };
    }

    struct RequestQueue
    {
        struct InternalItor
        {
            using difference_type = std::ptrdiff_t;
            using value_type = decltype( NextRequest() );

            auto operator*() const { return NextRequest(); }
            auto operator==( InternalItor ) const { return FCGI_Accept() < 0; }
            constexpr auto& operator++() { return *this; }
            constexpr auto operator++( int ) const { return *this; }
        };

        constexpr auto begin() const { return InternalItor{}; }
        constexpr auto end() const { return InternalItor{}; }
        constexpr auto empty() const { return begin() == end(); }
        auto front() const { return NextRequest(); }
    };
}  // namespace EasyFCGI

// template<> inline constexpr bool std::ranges::enable_borrowed_range<EasyFCGI::RequestQueue> = true;

constexpr auto FCGI_RequestQueue = EasyFCGI::RequestQueue{};

template<auto Deleter, typename T>
constexpr auto ResourceGuard( T* Resource )
{
    using DeleterType = decltype( []( T* p ) { Deleter( p ); } );
    return std::unique_ptr<T, DeleterType>( Resource );
}

#endif