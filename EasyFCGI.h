#ifndef H_EASY_FCGI_
#define H_EASY_FCGI_

#define NO_FCGI_DEFINES
#include <fcgi_stdio.h>
#include <memory>

#include "BijectiveMap.hpp"
#include "EasyString.h"

inline namespace EasyFCGI
{
    using namespace EasyString;

    template<typename... Args>
    inline auto Send( std::format_string<Args...> fmt, Args&&... args )
    {
        return FCGI_puts( std::format( fmt, std::forward<Args>( args )... ).c_str() );
    }

    inline auto Send( std::string_view Content )
    {
        // rejected, string construction will be called regardless
        // return FCGI_puts( std::c_str( Content ).value_or( std::c_str( std::string( Content ) ) ) );
        auto opt_c_str = std::c_str( Content );
        return FCGI_puts( opt_c_str ? opt_c_str.value()  //
                                    : std::string( Content ).c_str() );
    }

    template<typename T>
    concept HasSendableDump = requires( T t ) { Send( t.dump() ); };

    inline auto Send( HasSendableDump auto&& Content ) { return Send( Content.dump() ); }

    namespace HTTP
    {
        struct ReuqestMethod
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

            constexpr ReuqestMethod() = default;
            constexpr ReuqestMethod( const ReuqestMethod& ) = default;
            constexpr ReuqestMethod( VerbType OtherVerb ) : Verb{ OtherVerb } {}
            constexpr ReuqestMethod( std::convertible_to<std::string_view> auto VerbName )
                : Verb{ StrViewToVerb[VerbName] }
            {}

            constexpr operator std::string_view() const { return VerbToStrView[Verb]; }

            constexpr operator VerbType() const { return Verb; }
        };
    }  // namespace HTTP

    struct FCGI_Request
    {
        HTTP::ReuqestMethod RequestMethod;
        size_t ContentLength;
        std::string_view ContentType;
        std::string_view ScriptName;
        std::string_view RequestURI;
        std::string QueryStringCache;
        BijectiveMap<std::string_view, std::string_view> Query;

        auto ReadParam( const char* BuffPtr ) const
        {
            auto LoadParamFromEnv = getenv( BuffPtr );
            return std::string_view{ LoadParamFromEnv ? LoadParamFromEnv : "No Content" };
        }

        auto ReadParam( std::string_view ParamName ) const
        {
            auto opt_c_str = std::c_str( ParamName );
            return ReadParam( opt_c_str ? opt_c_str.value()  //
                                        : std::string( ParamName ).c_str() );
            // return ReadParam( *ParamName.cend() == '\0' ? std::data( ParamName )
            //                                             : std::data( std::string( ParamName ) ) );
        }

        // auto operator[]( std::string_view Key ) const { return QueryString[Key]; }
    };

    inline auto MapQueryString( std::string_view Source )
    {
        auto Result = BijectiveMap<std::string_view, std::string_view>{};

        for( auto Segment : Source | SplitBy( '&' ) )                        //
            for( auto [Key, Value] : Segment | SplitBy( '=' ) | Bundle<2> )  //
                Result[Key] = Value;

        return Result;
    }

    inline auto NextRequest()
    {
        auto R = FCGI_Request{};
        R.ContentLength = std::atoi( getenv( "CONTENT_LENGTH" ) );
        R.RequestMethod = getenv( "REQUEST_METHOD" );
        R.ContentType = getenv( "CONTENT_TYPE" );
        R.ScriptName = getenv( "SCRIPT_NAME" );
        R.RequestURI = getenv( "REQUEST_URI" );

        if( R.RequestMethod == HTTP::ReuqestMethod::POST )
            R.QueryStringCache.resize_and_overwrite(  //
                R.ContentLength,
                []( char* Buffer, size_t BufferSize ) {  //
                    return FCGI_fread( Buffer, sizeof( 1 [Buffer] ), BufferSize, FCGI_stdin );
                }  //
            );
        else
            R.QueryStringCache = getenv( "QUERY_STRING" );

        R.Query = MapQueryString( R.QueryStringCache );

        return R;
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