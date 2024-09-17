#ifndef _EASY_FCGI_HPP
#define _EASY_FCGI_HPP
#include <iterator>
#define NO_FCGI_DEFINES
#include <memory>
#include <fcgiapp.h>
#include <fcgios.h>
#include <concepts>
#include <utility>
#include <vector>
#include <atomic>
#include <cstdlib>
#include <source_location>
#include <filesystem>
#include <print>
#include <thread>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <csignal>
#include "EasyString.h"
// #include <chrono>
using namespace std::chrono_literals;

// ErrorGuard( Function Call / ErrorCode )
// check ErrorCode :   0    =>  program proceeds
//                   other  =>  invoke std::exit(ErrorCode)
#define ErrorGuard( OP ) ErrorGuard_impl( OP, #OP )
auto ErrorGuard_impl( std::integral auto ErrorCode, std::string_view OperationTitle,  //
                      std::string_view CallerName = std::source_location::current().function_name() )
{
    if( ErrorCode == 0 ) return std::println( "[ OK ]  {} : {}", CallerName, OperationTitle );
    std::println( "[ Fail, Error = {} ]  {} : {}", ErrorCode, CallerName, OperationTitle );
    std::exit( ErrorCode );
}

namespace HTTP
{
    using namespace EasyString;
    struct RequestMethod
    {
        enum class ValueOption { INVALID = 0, GET, HEAD, POST, PUT, DELETE, CONNECT, OPTIONS, TRACE, PATCH };
        ValueOption Verb;

        using enum ValueOption;

        static constexpr auto FromStringView( std::string_view VerbName )
        {
            if( VerbName == "GET" ) return GET;
            if( VerbName == "PUT" ) return PUT;
            if( VerbName == "POST" ) return POST;
            if( VerbName == "HEAD" ) return HEAD;
            if( VerbName == "PATCH" ) return PATCH;
            if( VerbName == "TRACE" ) return TRACE;
            if( VerbName == "DELETE" ) return DELETE;
            if( VerbName == "OPTIONS" ) return OPTIONS;
            if( VerbName == "CONNECT" ) return CONNECT;
            return INVALID;
        }

#define CASE_RETURN( N ) \
    case N : return #N
        static constexpr auto ToStringView( ValueOption Verb ) -> std::string_view
        {
            switch( Verb )
            {
                CASE_RETURN( GET );
                CASE_RETURN( PUT );
                CASE_RETURN( POST );
                CASE_RETURN( HEAD );
                CASE_RETURN( PATCH );
                CASE_RETURN( TRACE );
                CASE_RETURN( DELETE );
                CASE_RETURN( CONNECT );
                CASE_RETURN( OPTIONS );
            deafult:
                CASE_RETURN( INVALID );
            }
        }
#undef CASE_RETURN

        constexpr RequestMethod() = default;
        constexpr RequestMethod( const RequestMethod& ) = default;
        constexpr RequestMethod( ValueOption OtherVerb ) : Verb{ OtherVerb } {}
        constexpr RequestMethod( std::string_view VerbName ) : Verb{ FromStringView( VerbName ) } {}

        constexpr operator std::string_view() const { return ToStringView( Verb ); }
        constexpr auto EnumLiteral() const { return ToStringView( Verb ); }

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

        static constexpr auto FromStringView( std::string_view TypeName )
        {
            using enum ValueOption;
            if( TypeName == "text/plain" ) return TEXT_PLAIN;
            if( TypeName == "text/html" ) return TEXT_HTML;
            if( TypeName == "text/xml" ) return TEXT_XML;
            if( TypeName == "text/csv" ) return TEXT_CSV;
            if( TypeName == "text/css" ) return TEXT_CSS;
            if( TypeName == "application/json" ) return APPLICATION_JSON;
            if( TypeName == "application/x-www-form-urlencoded" ) return APPLICATION_X_WWW_FORM_URLENCODED;
            if( TypeName == "application/octet-stream" ) return APPLICATION_OCTET_STREAM;
            if( TypeName == "multipart/form-data" ) return MULTIPART_FORM_DATA;
            if( TypeName == "multipart/byteranges" ) return MULTIPART_BYTERANGES;
            return UNKNOWN_MIME_TYPE;
        }

        static constexpr auto ToStringView( ValueOption Type ) -> std::string_view
        {
            switch( Type )
            {
                using enum ValueOption;
                case TEXT_PLAIN :                        return "text/plain";
                case TEXT_HTML :                         return "text/html";
                case TEXT_XML :                          return "text/xml";
                case TEXT_CSV :                          return "text/csv";
                case TEXT_CSS :                          return "text/css";
                case APPLICATION_JSON :                  return "application/json";
                case APPLICATION_X_WWW_FORM_URLENCODED : return "application/x-www-form-urlencoded";
                case APPLICATION_OCTET_STREAM :          return "application/octet-stream";
                case MULTIPART_FORM_DATA :               return "multipart/form-data";
                case MULTIPART_BYTERANGES :              return "multipart/byteranges";
                // default :
                case UNKNOWN_MIME_TYPE :                 return "";
            }
            return "";
        }

        struct Text;
        struct Application;
        struct MultiPart;

        constexpr ContentType() = default;
        constexpr ContentType( const ContentType& ) = default;
        constexpr ContentType( ValueOption Other ) : Type{ Other } {}
        constexpr ContentType( std::string_view TypeName ) : Type{ FromStringView( Split( TypeName ).By( ";" ).front() | TrimSpace ) }
        {
            // if( Type == UNKNOWN_MIME_TYPE ) Type = TEXT_PLAIN;
        }

        constexpr operator std::string_view() const { return ToStringView( Type ); }
        constexpr auto EnumLiteral() const { return ToStringView( Type ); }

        constexpr operator ValueOption() const { return Type; }
    };

    struct ContentType::Text
    {
        constexpr static ContentType Plain = ValueOption::TEXT_PLAIN;
        constexpr static ContentType HTML = ValueOption::TEXT_HTML;
        constexpr static ContentType XML = ValueOption::TEXT_XML;
        constexpr static ContentType CSV = ValueOption::TEXT_CSV;
        constexpr static ContentType CSS = ValueOption::TEXT_CSS;
    };

    struct ContentType::Application
    {
        constexpr static ContentType Json = ValueOption::APPLICATION_JSON;
        constexpr static ContentType FormURLEncoded = ValueOption::APPLICATION_X_WWW_FORM_URLENCODED;
        constexpr static ContentType OctetStream = ValueOption::APPLICATION_OCTET_STREAM;
    };

    struct ContentType::MultiPart
    {
        constexpr static ContentType FormData = ValueOption::MULTIPART_FORM_DATA;
        constexpr static ContentType ByteRanges = ValueOption::MULTIPART_BYTERANGES;
    };

}  // namespace HTTP

namespace EasyFCGI
{

    inline namespace Concept
    {
        template<typename T>
        concept DumpingString = requires( T&& t ) {
            { t.dump() } -> std::same_as<std::string>;
        };
    }

    namespace std_fs = std::filesystem;
    struct Config
    {
        // evetually capped by /proc/sys/net/core/somaxconncat,eg.4096
        constexpr static auto DefaultBackLogNumber = 128;

        // static auto SocketDirectoryPrefix = std::string_view{"/tmp"};
        constexpr static auto SocketDirectoryPrefix = std::string_view{ "/dev/shm/" };
        constexpr static auto SocketExtensionSuffix = std::string_view{ ".sock" };

        constexpr static auto CommandLineBufferMax = 4096uz;
        inline static auto CommandLineBuffer = std::vector<char>{};
        inline static auto CommandLine = [] {
            auto Buffer = std::array<char, CommandLineBufferMax>{ '\0' };
            auto fin = std::fopen( "/proc/self/cmdline", "rb" );
            auto Loaded = std::fread( Buffer.data(), sizeof( Buffer[0] ), Buffer.size(), fin );
            std::fclose( fin );
            if( Loaded )
            {
                CommandLineBuffer = std::vector( Buffer.begin(), std::next( Buffer.begin(), Loaded ) );
                CommandLineBuffer.resize( Loaded - 1 );  // bury the last \0
            }

            return std::views::split( CommandLineBuffer, '\0' )  //
                   | std::views::transform( std::ranges::data )  //
                   | std::ranges::to<std::vector>();
        }();

        inline static auto DefaultSocketPath = std_fs::path( Config::SocketDirectoryPrefix ) /  //
                                               std_fs::path( CommandLine[0] )
                                                   .filename()                                  //
                                                   .replace_extension( Config::SocketExtensionSuffix );

        inline static auto OptionSocketPath = [] -> std::optional<std_fs::path> {
            for( auto&& [Option, OptionArg] : CommandLine | std::views::adjacent<2> )
                if( Option == std::string_view{ "-s" } )  //
                    return OptionArg;
            return {};
        }();
    };

    static auto TerminationSignal = std::atomic<bool>{ false };
    static auto TerminationHandler = []( int Signal ) {
        std::println( "\nReceiving Signal : {}", Signal );
        TerminationSignal = true;
    };

    static auto InitializationExecution = [] {
        ErrorGuard( FCGX_Init() );
        std::atexit( OS_LibShutdown );
        std::atexit( [] { std::println( "Porgram exiting from standard exit pathway." ); } );
        std::signal( SIGINT, TerminationHandler );
        std::signal( SIGTERM, TerminationHandler );
        return 0;
    }();

    struct FileDescriptor
    {
        using NativeFileDescriptorType = decltype( FCGX_OpenSocket( {}, {} ) );
        NativeFileDescriptorType FD;

        constexpr FileDescriptor() : FD{} {}

        FileDescriptor( const std_fs::path& SocketPath )  //
            : FD{ SocketPath.empty()                      //
                      ? NativeFileDescriptorType{}
                      : FCGX_OpenSocket( SocketPath.c_str(), Config::DefaultBackLogNumber ) }
        {
            if( FD == -1 )
            {
                std::println( "Failed to open socket." );
                std::exit( -1 );
            }
            if( FD > 0 ) { std_fs::permissions( SocketPath, std_fs::perms::all ); }
        }

        constexpr operator NativeFileDescriptorType() const { return FD; }

        auto UnixSocketName() const -> std_fs::path
        {
            auto UnixAddr = sockaddr_un{};
            auto UnixAddrLen = socklen_t{ sizeof( UnixAddr ) };

            if( getsockname( FD, (sockaddr*)&UnixAddr, &UnixAddrLen ) == 0 )
            {
                switch( UnixAddr.sun_family )
                {
                    case AF_UNIX :
                        return std_fs::canonical( UnixAddr.sun_path );
                        // case AF_INET : return "0.0.0.0";
                }
            }
            return {};
        }
    };

    struct Response
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

        template<typename T> requires( ! std::same_as<std::remove_cvref_t<T>, Response> )
        decltype( auto ) operator=( T&& NewContent ) noexcept
        {
            Body.clear();
            return Append( std::forward<T>( NewContent ) );
        }

        template<std::default_initializable T> operator T() const { return {}; }
    };

    struct Request
    {
        // gcc-14's bug to issue internal linkage warning
        // using FCGX_Request_Deleter_BUGGED = decltype( []( FCGX_Request* P ) {
        //     FCGX_Finish_r( P );
        //     delete P;
        // } );

        // work around
        struct FCGX_Request_Deleter
        {
            static auto operator()( FCGX_Request* P )
            {
                FCGX_Finish_r( P );
                delete P;
            }
        };

        std::unique_ptr<FCGX_Request, FCGX_Request_Deleter> FCGX_Request_Ptr;
        struct Response Response;  // elaborated-type-specifier, silencing -Wchanges-meaning

        Request() = default;
        // Request( const Request& ) = delete;
        Request( Request&& Other ) = default;

        Request( FileDescriptor SocketFD ) : FCGX_Request_Ptr{ std::make_unique_for_overwrite<FCGX_Request>().release() }
        {
            if( FCGX_InitRequest( FCGX_Request_Ptr.get(), SocketFD, FCGI_FAIL_ACCEPT_ON_INTR ) == 0 &&  //
                FCGX_Accept_r( FCGX_Request_Ptr.get() ) == 0 )
                return;

            // fail to obtain valid request, reset residual request data & allocation
            FCGX_InitRequest( FCGX_Request_Ptr.get(), 0, 0 );
            FCGX_Request_Ptr.reset();
        }

        static auto AcceptFrom( FileDescriptor SocketFD ) { return Request{ SocketFD }; }

        Request& operator=( Request&& Other ) = default;
        // {
        //     FCGX_Request_Ptr = std::move( Other.FCGX_Request_Ptr );
        //     Response = std::move( Other.Response );
        //     return *this;
        // }

        auto GetParam( std::string_view ParamName ) const
        {
            const char* Result = FCGX_GetParam( std::data( ParamName ), FCGX_Request_Ptr->envp );
            if( Result == nullptr ) Result = "Param Not Found";
            return std::string_view{ Result };
        }

        auto empty() const { return FCGX_Request_Ptr == nullptr; }

        auto FlushResponse() -> void
        {
            auto out = FCGX_Request_Ptr->out;
            if( Response.StatusCode == HTTP::StatusCode::NoContent ) { FCGX_PutS( "Status: 204\r\n", out ); }
            else
            {
                FCGX_PutS( "Status: {}\r\n"_FMT( std::to_underlying( Response.StatusCode ) ).c_str(), out );
                FCGX_PutS( "Content-Type: {}; charset=UTF-8\r\n"_FMT( Response.ContentType.EnumLiteral() ).c_str(), out );
                FCGX_PutChar( '\r', out );
                FCGX_PutChar( '\n', out );
                for( auto&& Buffer : Response.Body ) FCGX_PutS( Buffer.c_str(), out );
            }
            // Response.Reset();  // seems not necessary anymore
        }

        virtual ~Request()
        {
            if( FCGX_Request_Ptr == nullptr ) return;
            FlushResponse();
            // namespace fs = std::filesystem;
            // for( auto&& [Key, Entry] : Query.Json.items() )
            //     if( Entry.contains( QueryKey::FileUpload::TemporaryPath ) )
            //         if( auto PathString = Entry[QueryKey::FileUpload::TemporaryPath].get<std::string>();  //
            //             PathString.contains( TempPathTemplatePrefix ) )
            //             if( auto Path = fs::path( PathString );                                           //
            //                 fs::exists( Path ) )
            //                 fs::remove( Path );
        }
    };

    struct Server
    {
        FileDescriptor SocketFD;

        Server( FileDescriptor FD ) : SocketFD{ FD }
        {
            std::println( "Server file descriptor : {}", static_cast<int>( SocketFD ) );
            std::println( "Unix Socket Path : {}", SocketFD.UnixSocketName().c_str() );
            std::println( "Ready to accept requests..." );

            std::thread( [FD = SocketFD] {
                // todo: use CV nofity
                while( ! TerminationSignal ) std::this_thread::sleep_for( 500ms );
                FCGX_ShutdownPending();       // [os_unix.c:108] shutdownPending = TRUE;
                ::shutdown( FD, SHUT_RDWR );  // cause accept() to stop blocking
                // ::close( FD );
            } ).detach();
        }

        // Server() : Server{ FileDescriptor{} }
        Server() : Server{ FileDescriptor{ Config::OptionSocketPath.value_or( Config::DefaultSocketPath ) } } {}
        Server( const std_fs::path& SocketPath ) : Server{ FileDescriptor{ SocketPath } } {}

        auto operator=( auto&& ) = delete;

        auto NextRequest() const { return Request::AcceptFrom( SocketFD ); }

        // RequestContainer is not thread safe by default.
        // It is expected to be handled by one managing thread
        // For handling RequestContainer by multiple threads,
        // users should manage their own mutex
        struct RequestContainer
        {
            FileDescriptor SocketFD;
            Request PendingRequest;

            struct Sentinel
            {};
            struct Iterator
            {
                RequestContainer& BaseRange;
                decltype( auto ) operator*() { return std::move( BaseRange.PendingRequest ); }
                auto operator++() { return *this; }
            };

            auto begin() { return Iterator{ *this }; }
            auto end() { return Sentinel{}; }

            friend auto operator==( const Iterator& Iterator, const Sentinel& Sentinel )
            {
                // serve remaining request before exit
                auto& Queue = Iterator.BaseRange;
                auto& PendingRequest = Queue.PendingRequest;
                if( ! PendingRequest.empty() ) return false;
                if( TerminationSignal.load() ) return true;

                // start accepting new request from socket
                PendingRequest = Request::AcceptFrom( Queue.SocketFD );
                return PendingRequest.empty();
            }

            friend auto operator==( const Sentinel& Sentinel, const Iterator& Iterator ) { return Iterator == Sentinel; }
        };

        auto RequestQueue() const { return RequestContainer{ .SocketFD = SocketFD }; }

        // virtual ~Server()
        // {
        //     shutdown( SocketFD, SHUT_RDWR );
        //     ::close( SocketFD );
        // }
    };

}  // namespace EasyFCGI

#endif
