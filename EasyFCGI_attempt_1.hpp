#ifndef _EASY_FCGI_HPP
#define _EASY_FCGI_HPP

#include <mutex>
#include <type_traits>
#define NO_FCGI_DEFINES
#include <memory>
#include <fcgiapp.h>
#include <fcgios.h>
#include <concepts>
#include <string>
#include <string_view>
#include <vector>
#include <atomic>
#include <cstdlib>
#include <source_location>
#include <print>
#include <thread>
#include <sys/stat.h>
#include <sys/socket.h>
#include <csignal>
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

inline namespace EasyFCGI
{
    // static auto SocketDirectoryPrefix = std::string_view{"/tmp"};
    static auto SocketDirectoryPrefix = std::string_view{ "/dev/shm/" };
    static auto SocketExtensionSuffix = std::string_view{ ".sock" };

    static auto OptionSocketName( int argc, char** argv ) -> std::string_view
    {
        for( int i = 1; i < argc - 1; ++i )
            if( auto LaunchOption = std::string_view{ argv[i] };  //
                LaunchOption == "-s" || LaunchOption == "--socket" )
                return argv[i + 1];
        return {};
    }

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
        constexpr static auto DefaultBackLogNumber = 128;  // evetually capped by /proc/sys/net/core/somaxconncat
        using NativeFileDescriptorType = decltype( FCGX_OpenSocket( {}, {} ) );
        NativeFileDescriptorType FD;

        constexpr FileDescriptor() : FD{} {}

        // todo: use std::filesystem::path for SocketPath's type
        FileDescriptor( std::string_view SocketPath, int BackLogNumber = DefaultBackLogNumber )  //
            : FD{ SocketPath.empty() ? NativeFileDescriptorType{} : FCGX_OpenSocket( std::data( SocketPath ), BackLogNumber ) }
        {
            if( FD == -1 )
            {
                std::println( "Failed to open socket." );
                std::exit( -1 );
            }
            if( FD > 0 )
            {
                // todo: use std::filesystem::permissions
                chmod( std::data( SocketPath ), 0777 );
            }
        }

        constexpr operator NativeFileDescriptorType() const { return FD; }
    };

    inline auto FCGX_FinishRequest( FCGX_Request* reqDataPtr )
    {
        // definition from [ fcgiapp.c : 1316 ]
        struct FCGX_Stream_Data
        {
            unsigned char* buff;
            int bufflen;
            unsigned char* mBuff;
            unsigned char* buffStop;
            int type;
            int eorStop;
            int skip;
            int contentLen;
            int paddingLen;
            int isAnythingWritten;
            int rawWrite;
            FCGX_Request* reqDataPtr;
        };
        // Recouple interlink between FCGX_Request and FCGX_Stream before FCGX_Finish_r
        FCGX_Finish_r( static_cast<FCGX_Stream_Data*>( reqDataPtr->in->data )->reqDataPtr =   //
                       static_cast<FCGX_Stream_Data*>( reqDataPtr->out->data )->reqDataPtr =  //
                       static_cast<FCGX_Stream_Data*>( reqDataPtr->err->data )->reqDataPtr = reqDataPtr );
    }

    struct Response
    {};

    struct Request
    {
        // gcc-14's bug to issue internal linkage warning
        using FCGX_Request_Deleter_BUGGED = decltype( []( FCGX_Request* P ) {
            FCGX_Finish_r( P );
            delete P;
        } );

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

        Request() = default;
        // Request( const Request& ) = delete;
        // Request( Request&& Other ) : FCGX_Request_Ptr{ std::move( Other.FCGX_Request_Ptr ) } {};

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

        auto GetParam( std::string_view ParamName ) const
        {
            const char* Result = FCGX_GetParam( std::data( ParamName ), FCGX_Request_Ptr->envp );
            if( Result == nullptr ) Result = "Param Not Found";
            return std::string_view{ Result };
        }

        auto empty() const { return FCGX_Request_Ptr == nullptr; }

        // virtual ~Request() { FCGX_Finish_r( FCGX_Request_Ptr.get() ); }
    };

    struct Server
    {
        FileDescriptor SocketFD;

        Server( FileDescriptor FD ) : SocketFD{ FD }
        {
            if( FD == 0 )
            {
                std::println( "Using STDIN as server file descriptor." );
                std::println( "Be sure this application is launched through spawn-fcgi or equivalent." );
            }
            std::thread( [FD = SocketFD] {
                // todo: use CV nofity
                while( ! TerminationSignal ) std::this_thread::sleep_for( 500ms );
                FCGX_ShutdownPending();       // [os_unix.c:108] shutdownPending = TRUE;
                ::shutdown( FD, SHUT_RDWR );  // cause accept() to stop blocking
                // ::close( FD );
            } ).detach();
        }

        Server() : Server{ FileDescriptor{} } {}
        Server( std::string_view SocketPath ) : Server{ FileDescriptor{ SocketPath } } {}
        Server( std::string_view SocketPath, int BackLogNumber ) : Server{ FileDescriptor{ SocketPath, BackLogNumber } } {}

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
                auto operator++( int ) { return ++*this; }
            };

            auto begin() { return Iterator{ *this }; }
            auto end() { return Sentinel{}; }

            friend auto operator==( const Iterator& Iterator, const Sentinel& Sentinel )
            {
                auto& Queue = Iterator.BaseRange;
                auto& PendingRequest = Queue.PendingRequest;
                if( ! PendingRequest.empty() ) return false;

                // serve remaining request before exit
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
