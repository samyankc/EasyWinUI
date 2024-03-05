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

    template<typename T>
    concept DumpingString = requires( T&& t ) {
        {
            t.dump()
        } -> std::same_as<std::string>;
    };

    template<typename... Args>
    [[deprecated( "Use FCGI_Request.Response instead" )]] inline auto Send( std::format_string<Args...> fmt,
                                                                            Args&&... args )
    {
        return FCGI_puts( std::format( fmt, std::forward<Args>( args )... ).c_str() );
    }

    [[deprecated( "Use FCGI_Request.Response instead" )]] inline auto Send( std::string_view Content )
    {
        return FCGI_puts( std::c_str( Content ) );
    }

    template<typename T>
    concept HasSendableDump = requires( T t ) { Send( t.dump() ); };

    [[deprecated( "Use FCGI_Request.Response instead" )]] inline auto Send( HasSendableDump auto&& Content )
    {
        return Send( Content.dump() );
    }

    namespace HTTP
    {
        struct RequestMethod
        {
            enum class VerbType { INVALID = 0, GET, HEAD, POST, PUT, DELETE, CONNECT, OPTIONS, TRACE, PATCH };
            VerbType Verb;

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

        enum class StatusCode : int {
            OK = 200,
            Created = 201,
            Accepted = 202,
            NoContent = 204,
            BadRequest = 400,
            Unauthorized = 401,
            Forbidden = 403,
            NotFound = 404,
            MethodNotAllowed = 405,
            UnsupportedMediaType = 415,
            UnprocessableEntity = 422,
            InternalServerError = 500,
            NotImplemented = 501,
            ServiceUnavailable = 503,
        };

        struct ContentType
        {
            enum class MimeType {
                TEXT_PLAIN = 0,
                TEXT_HTML,
                TEXT_XML,
                TEXT_CSV,
                TEXT_CSS,
                APPLICATION_JSON,
                APPLICATION_X_WWW_FORM_URLENCODED,
                APPLICATION_OCTET_STREAM,
                MULTIPART_FORM_DATA,
                MULTIPART_BYTERANGES,
                UNKNOWN_MIME_TYPE,
            };
            MimeType Type;

            using enum MimeType;

            inline const static auto StrViewToType = BijectiveMap<std::string_view, MimeType>  //
                {
                    { "text/plain", TEXT_PLAIN },
                    { "text/html", TEXT_HTML },
                    { "text/xml", TEXT_XML },
                    { "application/json", APPLICATION_JSON },
                    { "application/x-www-form-urlencoded", APPLICATION_X_WWW_FORM_URLENCODED },
                    { "application/octet-stream", APPLICATION_OCTET_STREAM },
                    { "multipart/form-data", MULTIPART_FORM_DATA },
                    { "multipart/byteranges", MULTIPART_BYTERANGES },
                    { "", UNKNOWN_MIME_TYPE },
                };
            inline const static auto TypeToStrView = StrViewToType.Inverse();

            struct Text;
            struct Application;
            struct MultiPart;

            constexpr ContentType() = default;
            constexpr ContentType( const ContentType& ) = default;
            constexpr ContentType( MimeType Other ) : Type{ Other } {}
            constexpr ContentType( std::convertible_to<std::string_view> auto TypeName )
                : Type{ StrViewToType[Split( TypeName ).By( ';' ).front() | TrimSpace] }
            {
                // if( Type == UNKNOWN_MIME_TYPE ) Type = TEXT_PLAIN;
            }

            constexpr operator std::string_view() const { return TypeToStrView[Type]; }

            constexpr operator MimeType() const { return Type; }
        };

        struct ContentType::Text
        {
            constexpr static ContentType Plain = TEXT_PLAIN;
            constexpr static ContentType HTML = TEXT_HTML;
            constexpr static ContentType XML = TEXT_XML;
            constexpr static ContentType CSV = TEXT_CSV;
            constexpr static ContentType CSS = TEXT_CSS;
        };

        struct ContentType::Application
        {
            constexpr static ContentType Json = APPLICATION_JSON;
            constexpr static ContentType FormURLEncoded = APPLICATION_X_WWW_FORM_URLENCODED;
            constexpr static ContentType OctetStream = APPLICATION_OCTET_STREAM;
        };

        struct ContentType::MultiPart
        {
            constexpr static ContentType FormData = MULTIPART_FORM_DATA;
            constexpr static ContentType ByteRanges = MULTIPART_BYTERANGES;
        };

    }  // namespace HTTP

    inline auto ReadParam( const char* BuffPtr ) -> std::string_view
    {
        auto LoadParamFromEnv = getenv( BuffPtr );
        return LoadParamFromEnv ? LoadParamFromEnv : "";
    }

    inline auto ReadParam( std::string_view ParamName ) { return ReadParam( std::c_str( ParamName ) ); }

    struct FCGI_Response
    {
        HTTP::StatusCode StatusCode{ HTTP::StatusCode::OK };
        HTTP::ContentType ContentType{ HTTP::ContentType::Text::Plain };
        std::vector<std::string> Body;

        auto& Set( HTTP::StatusCode NewValue ) { return StatusCode = NewValue; }
        auto& Set( HTTP::ContentType NewValue ) { return ContentType = NewValue; }

        template<typename T>
        decltype( auto ) Append( T&& NewContent ) noexcept
        {
            if constexpr( DumpingString<T> )
            {
                Body.push_back( NewContent.dump() );
            }
            else
            {
                Body.push_back( std::forward<T>( NewContent ) );
            }
            return *this;
        }

        template<typename T>
        decltype( auto ) operator+=( T && NewContent ) noexcept
        {
            return Append( std::forward<T>( NewContent ) );
        }

        template<typename T>
        requires( ! std::same_as<std::remove_cvref_t<T>, FCGI_Response> )
        decltype( auto ) operator=( T && NewContent ) noexcept
        {
            Body.clear();
            return Append( std::forward<T>( NewContent ) );
        }

        template<typename T>
        operator T() const { return {}; }
    };

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
        HTTP::ContentType ContentType;
        std::string_view ScriptName;
        std::string_view RequestURI;
        std::string_view QueryString;
        std::string_view RequestBody;
        QueryExecutor<Json> Query;
        FCGI_Response Response;

        ~FCGI_Request()
        {
            if( Response.StatusCode == HTTP::StatusCode::NoContent )
            {
                FCGI_puts( "Status: 204\r\n" );
                return;
            }
            FCGI_puts( std::format( "Status: {}", std::to_underlying( Response.StatusCode ) ).c_str() );
            FCGI_puts(
                std::format( "Content-Type: {}; charset=UTF-8", std::string_view( Response.ContentType ) ).c_str() );
            FCGI_putchar( '\r' );
            FCGI_putchar( '\n' );
            for( auto&& Buffer : Response.Body ) FCGI_puts( Buffer.c_str() );
        }
    };

    inline auto QueryStringToJson( std::string_view Source )
    {
        auto Result = Json{};

        for( auto Segment : Source | SplitBy( '&' ) )                        //
            for( auto [Key, Value] : Segment | SplitBy( '=' ) | Bundle<2> )  //
                Result[DecodeURLFragment( Key )] = DecodeURLFragment( Value );

        return Result;
    }

    inline auto NextRequest() -> FCGI_Request
    {
        auto R = Json{};

        auto ContentLength = StrViewTo<std::size_t>( ReadParam( "CONTENT_LENGTH" ) ).value_or( 0 );
        auto RequestMethod = HTTP::RequestMethod( ReadParam( "REQUEST_METHOD" ) );
        auto ContentType = HTTP::ContentType( ReadParam( "CONTENT_TYPE" ) );
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

        if( ContentType == HTTP::ContentType::Application::Json ) try
            {
                Query = Json::parse( RequestBody );
            }
            catch( const Json::parse_error& e )
            {
                FCGI_puts( "Status: 400\r\nContent-Type: text/plain\r\n" );
                FCGI_puts( e.what() );
                // skip this iteration and move on
                if( FCGI_Accept() >= 0 ) return NextRequest();
                return {};  // or terminate?
            }
        else if( RequestMethod == HTTP::RequestMethod::GET ||
                 ContentType == HTTP::ContentType::Application::FormURLEncoded )
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