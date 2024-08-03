#ifndef H_EASY_FCGI_
#define H_EASY_FCGI_

#define NO_FCGI_DEFINES
#include <fcgi_stdio.h>
#include <memory>
#include <filesystem>

#include "json.hpp"
#include "BijectiveMap.hpp"
#include "EasyString.h"

//helper functions
namespace
{
    using namespace EasyString;

    [[nodiscard]]
    auto HexToChar( std::string_view HexString ) noexcept
    {
        return std::bit_cast<char>( StrViewTo<unsigned char, 16>( HexString ).value_or( '?' ) );
    }

    [[nodiscard]]
    auto DecodeURLFragment( std::string_view Fragment )
    {
        constexpr auto EncodeDigitWidth = 2;
        auto RestoreSpaceChar = ReplaceChar( '+' ).With( ' ' );

        auto Result = std::string{};

        auto [FirstPart, OtherParts] = Fragment | SplitBy( "%" ) | SliceAt( 1 );
        for( auto LeadingText : FirstPart ) Result += LeadingText | RestoreSpaceChar;
        for( auto Segment : OtherParts )
        {
            auto [Encoded, Unencoded] = Segment | SliceAt( EncodeDigitWidth );
            Result += Encoded.length() >= EncodeDigitWidth ? HexToChar( Encoded ) : '?';
            Result += Unencoded | RestoreSpaceChar;
        }
        return Result;
    }

    template<auto Deleter, typename T> constexpr auto ResourceGuard( T* Resource )
    {
        using DeleterType = decltype( []( T* p ) {
            if( p != NULL ) Deleter( p );
        } );
        return std::unique_ptr<T, DeleterType>( Resource );
    }

}  // namespace

inline namespace EasyFCGI
{
    using namespace EasyString;

    template<typename T>
    concept DumpingString = requires( T&& t ) {
        { t.dump() } -> std::same_as<std::string>;
    };

    template<typename... Args>
    [[deprecated( "Use FCGI_Request.Response instead" )]]
    inline auto Send( std::format_string<Args...> fmt, Args&&... args )
    {
        return FCGI_puts( std::format( fmt, std::forward<Args>( args )... ).c_str() );
    }

    [[deprecated( "Use FCGI_Request.Response instead" )]]
    inline auto Send( std::string_view Content )
    {
        return FCGI_puts( std::c_str( Content ) );
    }

    template<typename T>
    concept HasSendableDump = requires( T t ) { Send( t.dump() ); };

    [[deprecated( "Use FCGI_Request.Response instead" )]]
    inline auto Send( HasSendableDump auto&& Content )
    {
        return Send( Content.dump() );
    }

    namespace QueryKey
    {
        namespace FileUpload
        {
            constexpr auto FileName = "filename"sv;
            constexpr auto ContentType = "content-type"sv;
            constexpr auto TemporaryPath = "temp-path"sv;
        }
    }  // namespace QueryKey

    namespace HTTP
    {
        struct RequestMethod
        {
            enum class ValueOption { INVALID = 0, GET, HEAD, POST, PUT, DELETE, CONNECT, OPTIONS, TRACE, PATCH };
            ValueOption Verb;

            using enum ValueOption;
            inline const static auto StrViewToVerb = BijectiveMap<std::string_view, ValueOption>     //
                {                                                                                    //
                  { "GET", GET },  { "POST", POST },    { "CONNECT", CONNECT }, { "TRACE", TRACE },  //
                  { "PUT", PUT },  { "HEAD", HEAD },    { "OPTIONS", OPTIONS }, { "PATCH", PATCH },  //
                  { "", INVALID }, { "DELETE", DELETE }
                };

            inline const static auto VerbToStrView = StrViewToVerb.Inverse();

            constexpr RequestMethod() = default;
            constexpr RequestMethod( const RequestMethod& ) = default;
            constexpr RequestMethod( ValueOption OtherVerb ) : Verb{ OtherVerb } {}
            constexpr RequestMethod( std::convertible_to<std::string_view> auto VerbName ) : Verb{ StrViewToVerb[VerbName] } {}

            constexpr operator std::string_view() const { return VerbToStrView[Verb]; }
            constexpr auto EnumLiteral() const { return VerbToStrView[Verb]; }

            constexpr operator ValueOption() const { return Verb; }
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
            enum class ValueOption {
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
            ValueOption Type;

            using enum ValueOption;

            inline const static auto StrViewToType = BijectiveMap<std::string_view, ValueOption>  //
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
            constexpr ContentType( ValueOption Other ) : Type{ Other } {}
            constexpr ContentType( std::convertible_to<std::string_view> auto TypeName ) : Type{ StrViewToType[Split( TypeName ).By( ";" ).front() | TrimSpace] }
            {
                // if( Type == UNKNOWN_MIME_TYPE ) Type = TEXT_PLAIN;
            }

            constexpr operator std::string_view() const { return TypeToStrView[Type]; }
            constexpr auto EnumLiteral() const { return TypeToStrView[Type]; }

            constexpr operator ValueOption() const { return Type; }
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

        auto Reset()
        {
            Set( HTTP::StatusCode::OK );
            Set( HTTP::ContentType::Text::Plain );
            Body.clear();
        }

        template<typename T> decltype( auto ) Append( T&& NewContent ) noexcept
        {
            if constexpr( DumpingString<T> ) { Body.push_back( NewContent.dump() ); }
            else { Body.push_back( static_cast<std::string>( std::forward<T>( NewContent ) ) ); }
            return *this;
        }

        template<typename T> decltype( auto ) operator+=( T&& NewContent ) noexcept { return Append( std::forward<T>( NewContent ) ); }

        template<typename T> requires( ! std::same_as<std::remove_cvref_t<T>, FCGI_Response> )
        decltype( auto ) operator=( T&& NewContent ) noexcept
        {
            Body.clear();
            return Append( std::forward<T>( NewContent ) );
        }

        template<std::default_initializable T> operator T() const { return {}; }
    };

    constexpr auto TempPathTemplate = "/dev/shm/XXXXXX"sv;
    constexpr auto TempPathTemplatePrefix = TempPathTemplate | Before( "XXXXXX" );

    using Json = nlohmann::json;

    template<typename StorageEngine> struct QueryExecutor
    {
        StorageEngine Json;
        auto contains( std::string_view Key ) const { return Json.contains( Key ); }
        auto operator[]( std::string_view Key ) const
        {
            if( Json.contains( Key ) ) return Json[Key].template get<std::string>();
            else return std::string{};
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

        // auto& operator=( const FCGI_Request& Other )
        // {
        //     RequestMethod = Other.RequestMethod;
        //     ContentLength = Other.ContentLength;
        //     ContentType = Other.ContentType;
        //     ScriptName = Other.ScriptName;
        //     RequestURI = Other.RequestURI;
        //     QueryString = Other.QueryString;
        //     RequestBody = Other.RequestBody;
        //     Query.Json = std::move( Other.Query.Json );
        //     Response = Other.Response;
        //     return *this;
        // }

        auto FlushResponse() -> void
        {
            if( Response.StatusCode == HTTP::StatusCode::NoContent ) { FCGI_puts( "Status: 204\r\n" ); }
            else
            {
                FCGI_puts( "Status: {}"_FMT( std::to_underlying( Response.StatusCode ) ).c_str() );
                FCGI_puts( "Content-Type: {}; charset=UTF-8"_FMT( Response.ContentType.EnumLiteral() ).c_str() );
                FCGI_putchar( '\r' );
                FCGI_putchar( '\n' );
                for( auto&& Buffer : Response.Body ) FCGI_puts( Buffer.c_str() );
            }
            // Response.Reset();  // seems not necessary anymore
        }

        ~FCGI_Request()
        {
            FlushResponse();
            namespace fs = std::filesystem;
            for( auto&& [Key, Entry] : Query.Json.items() )
                if( Entry.contains( QueryKey::FileUpload::TemporaryPath ) )
                    if( auto PathString = Entry[QueryKey::FileUpload::TemporaryPath].get<std::string>();  //
                        PathString.contains( TempPathTemplatePrefix ) )
                        if( auto Path = fs::path( PathString );                                           //
                            fs::exists( Path ) )
                            fs::remove( Path );
        }
    };

    inline auto QueryStringToJson( std::string_view Source )
    {
        auto Result = Json{};

        for( auto Segment : Source | SplitBy( "&" ) )                        //
            for( auto [Key, Value] : Segment | SplitBy( "=" ) | Bundle<2> )  //
                Result[DecodeURLFragment( Key )] = DecodeURLFragment( Value );

        return Result;
    }

    struct RequestQueue
    {
        // NRVO for this function may cause issue regarding flushing response later on
        // always return a temp constructed FCGI_Request object for proper flushing behaviour
        static auto NextRequest() -> FCGI_Request
        {
            auto RequestMethod = HTTP::RequestMethod( ReadParam( "REQUEST_METHOD" ) );
            auto ContentLength = StrViewTo<std::size_t>( ReadParam( "CONTENT_LENGTH" ) ).value_or( 0 );
            auto ContentType = HTTP::ContentType( ReadParam( "CONTENT_TYPE" ) );
            // auto ScriptName = ReadParam( "SCRIPT_NAME" );
            // auto RequestURI = ReadParam( "REQUEST_URI" );
            auto QueryString = ReadParam( "QUERY_STRING" );
            auto RequestBody = ReadParam( "REQUEST_BODY" );
            auto QueryJson = Json{};
            // $request_body for fcgi has very low size limit (<64k)

            auto RequestBodyCache = std::string{};
            RequestBodyCache.resize_and_overwrite(       //
                ContentLength,
                []( char* Buffer, size_t BufferSize ) {  //
                    // reading request body from FCGI_stdin is only affected by client_max_body_size
                    return FCGI_fread( Buffer, sizeof( 1 [Buffer] ), BufferSize, FCGI_stdin );
                }  //
            );
            RequestBody = RequestBodyCache;

            // in case of using client_body_in_file_only
            // auto RequestBodyFile = ReadParam( "REQUEST_BODY_FILE" );
            // if( ! RequestBodyFile.empty() )
            // {
            //     RequestBodyCache = LoadFileContent( RequestBodyFile.data() );
            //     RequestBody = RequestBodyCache | Trim( "\r\n" ) | TrimSpace;
            // }

            // ignore original request body in case of GET
            switch( RequestMethod )
            {
                case HTTP::RequestMethod::GET :  RequestBody = QueryString; break;
                case HTTP::RequestMethod::POST : QueryString = RequestBody; break;
                default :                        QueryString = RequestBody; break;
            }

            switch( ContentType )
            {
                case HTTP::ContentType::Application::Json :
                {
                    QueryJson = Json::parse( RequestBody, nullptr, false );  // disable exception
                    if( QueryJson.is_discarded() )                           // parse error
                    {
                        // early response with error message
                        // caller does not see this iteration
                        // give caller the next request
                        FCGI_puts( "Status:400\r\n\r\nJson Parse Error Occured" );
                        if( FCGI_Accept() >= 0 ) return NextRequest();
                    }
                    break;
                }
                case HTTP::ContentType::MultiPart::FormData :
                {
                    constexpr auto BetweenQuote = Between( "\"", "\"" );
                    auto Boundary = ReadParam( "CONTENT_TYPE" ) | After( "boundary=" ) | TrimSpace;
                    auto FullBody = std::string_view{ RequestBody.data(), ContentLength } | TrimSpace;
                    if( FullBody.ends_with( "--" ) ) FullBody.remove_suffix( 2 );
                    for( auto&& Section : FullBody | SplitBy( Boundary ) | Skip( 1 ) )
                        for( auto&& [Header, Body] : Section | SplitBy( "\r\n\r\n" ) | Bundle<2> )
                        {
                            auto Name = Header | After( "name=" ) | BetweenQuote;
                            if( Name.empty() ) break;  // should never happen ?
                            auto FileName = Header | After( "filename=" ) | BetweenQuote;
                            auto ContentType = Header | After( "\r\n" ) | After( "Content-Type:" ) | TrimSpace;

                            if( Body.ends_with( "\r\n--" ) ) Body.remove_suffix( 4 );
                            if( ContentType.empty() ) { QueryJson[Name] = Body; }
                            else
                            {
                                // char TempPath[]{ "/dev/shm/XXXXXX" };
                                auto TempPathString = std::string( TempPathTemplate );
                                auto TempPath = TempPathString.data();
                                auto FD = mkstemp( TempPath );
                                if( FD == -1 )
                                {
                                    QueryJson[Name] = "Upload Failure, Unable to open : {}"_FMT( TempPath );
                                    continue;
                                }
                                if constexpr( true )                                  // using legacy API
                                {
                                    (void)! ::write( FD, Body.data(), Body.size() );  // silent unused result warning
                                    ::close( FD );
                                }
                                else
                                {
                                    ::close( FD );
                                    auto TempFile = fopen( TempPath, "wb" );
                                    auto TempFile_RG = ResourceGuard<std::fclose>( TempFile );
                                    std::fwrite( Body.data(), sizeof( 1 [Body.data()] ), Body.size(), TempFile );
                                }
                                QueryJson[Name] = Json{ { QueryKey::FileUpload::FileName, FileName },
                                                        { QueryKey::FileUpload::ContentType, ContentType },
                                                        { QueryKey::FileUpload::TemporaryPath, TempPath } };
                            }
                        }
                    break;
                }
                default : QueryJson = QueryStringToJson( QueryString ); break;
            }

            return FCGI_Request{
                .RequestMethod = RequestMethod,
                .ContentLength = ContentLength,
                .ContentType = ContentType,
                .ScriptName = ReadParam( "SCRIPT_NAME" ),
                .RequestURI = ReadParam( "REQUEST_URI" ),
                .QueryString = QueryString,
                .RequestBody = RequestBody,
                .Query = { std::move( QueryJson ) },
            };
        }

        struct RequestSentinel
        {};
        struct RequestIter
        {
            using difference_type = std::ptrdiff_t;
            using value_type = decltype( NextRequest() );

            auto operator*() const { return NextRequest(); }
            auto operator==( RequestSentinel ) const { return FCGI_Accept() < 0; }
            constexpr auto& operator++() const { return *this; }
            constexpr auto operator++( int ) const { return *this; }
        };

        constexpr auto end() const { return RequestSentinel{}; }
        auto begin() const { return RequestIter{}; }
        auto empty() const { return begin() == end(); }
        auto front() const { return *begin(); }
    };

}  // namespace EasyFCGI

// template<> inline constexpr bool std::ranges::enable_borrowed_range<EasyFCGI::RequestQueue> = true;

constexpr auto FCGI_RequestQueue = EasyFCGI::RequestQueue{};

#endif
