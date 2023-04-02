// https://reqres.in/
// L"https://reqres.in/api/login"

#ifndef EASYHTTP_H
#define EASYHTTP_H

#include <EasyString.h>
#include <windows.h>
#include <winhttp.h>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>
#include <charconv>
#include <utility>
#include <vector>
#include <memory>

#define ResponseBufferSize 32
#define WINHTTP_HTTP_VERSION NULL
#define WINHTTP_CONNECT_RESERVE_PARAM 0
#define WINHTTP_RECEICVE_RESERVE_PARAM NULL
#define DEFAULT_DOMAIN L"reqres.in"
#define DEFAULT_USER_AGENT L"UserAgent"

namespace Http
{
    using std::string, std::wstring, std::string_view, std::wstring_view, std::vector;
    using ResponseChunks = vector<string>;

    inline std::ostream& operator<<( std::ostream& out, const ResponseChunks& Buffers )
    {
        for( const auto& str : Buffers ) out << "[" << str.length() << "]\n";  // << str << '\n';
        return out;
    }

    constexpr auto IE10 =
        L"Mozilla/5.0 (compatible; MSIE 10.0; Windows NT 10.0; WOW64; Trident/7.0; .NET4.0C; .NET4.0E)";

    constexpr auto IE11 = L"Mozilla/5.0 (Windows NT 10.0; WOW64; Trident/7.0; .NET4.0C; .NET4.0E; rv:11.0) like Gecko";

    template<typename NewContainer>
    auto ConvertTo( const auto& Source )
    {
        return NewContainer{ Source.begin(), Source.end() };
    }

    template<auto Deleter, typename T>
    constexpr auto ResourceGuard( T* Resource )
    {
        using DeleterType = decltype( []( T* p ) { Deleter( p ); } );
        return std::unique_ptr<T, DeleterType>( Resource );
    }

    inline void Say( const string& MSG ) { std::cout << MSG << '\n'; }
    inline void Say( const wstring& MSG ) { std::wcout << MSG << '\n'; }
    inline void SayError( const char* MSG = "-" )
    {
        std::cout << "[error code " << GetLastError() << "] " << MSG << '\n';
    }

    template<typename T>
    decltype( auto ) As( T&& arg )
    {
        return std::forward<T>( arg );
    }

    struct Connection
    {
        HINTERNET hSession{ NULL };
        HINTERNET hConnect{ NULL };
        DWORD SecurityFlag{ 0 };

        auto MakeConnection( LPCWSTR Domain, LPCWSTR UserAgent, INTERNET_PORT Port )
        {
            hSession = WinHttpOpen( UserAgent,                          //
                                    WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,  //
                                    WINHTTP_NO_PROXY_NAME,              //
                                    WINHTTP_NO_PROXY_BYPASS,            //
                                    0 & WINHTTP_FLAG_ASYNC );
            if( ! hSession ) SayError( "Session Creation Failed" );

            hConnect = WinHttpConnect( hSession,  //
                                       Domain,    //
                                       Port,      //
                                       WINHTTP_CONNECT_RESERVE_PARAM );
            if( ! hConnect ) SayError( "Connection Failed" );

            return hConnect;
        }

        Connection() = default;

        Connection( string_view URL, LPCWSTR UserAgent = DEFAULT_USER_AGENT )
        {
            if( URL.back() == '/' ) URL.remove_suffix( 1 );
            if( URL.starts_with( "https" ) ) SecurityFlag = WINHTTP_FLAG_SECURE;

            if( const auto DomainStart = URL.find( "//" );  //
                DomainStart != URL.npos )
                URL.remove_prefix( 2 + DomainStart );

            auto Port = static_cast<INTERNET_PORT>( INTERNET_DEFAULT_PORT );
            if( const auto PortStart = URL.find( ':' );  //
                PortStart != URL.npos )
            {
                Port = static_cast<INTERNET_PORT>( std::stoul( string{ &URL[PortStart + 1] } ) );
                URL.remove_suffix( URL.length() - PortStart );
            }
            MakeConnection( ConvertTo<wstring>( URL ).c_str(), UserAgent, Port );
        }

        ~Connection()
        {
            if( hConnect ) WinHttpCloseHandle( hConnect );
            if( hSession ) WinHttpCloseHandle( hSession );
        }

        static auto GetResponseChunks( HINTERNET hRequest ) -> ResponseChunks
        {
            ResponseChunks Buffers;
            Buffers.reserve( ResponseBufferSize );

            DWORD PendingSize = 0;
            DWORD DownloadedSize = 0;

            while( WinHttpQueryDataAvailable( hRequest, &PendingSize ) && PendingSize > 0 )
            {
                Buffers.emplace_back();
                Buffers.back().resize( PendingSize );
                auto ResponseBuffer = Buffers.back().data();
                WinHttpReadData( hRequest,                //
                                 (LPVOID)ResponseBuffer,  //
                                 PendingSize,             //
                                 &DownloadedSize );
                if( PendingSize != DownloadedSize )
                    std::cout << "Download Error: " << PendingSize - DownloadedSize << " missing" << std::endl;
                // std::cout << "New Content: [ " << DownloadedSize << " ] " << ResponseBuffer << '\n';
            }

            if( Buffers.empty() ) SayError( "Get Response Failed" );

            return Buffers;
        }

        static auto Transcript( const ResponseChunks& Source )
        {
            string Result;
            auto TotalSize = 0uz;
            for( auto&& s : Source ) TotalSize += s.length();
            Result.reserve( TotalSize );
            for( auto&& s : Source ) Result += s;
            return Result;
        }

        static auto GetResponse( HINTERNET hRequest ) -> string { return Transcript( GetResponseChunks( hRequest ) ); }

        [[nodiscard]] auto SendRequest( LPCWSTR RequestVerb, LPCWSTR Action, string_view FormString )
            -> std::invoke_result_t<decltype( GetResponse ), HINTERNET>
        {
            auto hRequestGuard = ResourceGuard<WinHttpCloseHandle>(  //
                WinHttpOpenRequest( hConnect, RequestVerb, Action,   //
                                    WINHTTP_HTTP_VERSION,            //
                                    WINHTTP_NO_REFERER,              //
                                    WINHTTP_DEFAULT_ACCEPT_TYPES,    //
                                    WINHTTP_FLAG_REFRESH | SecurityFlag ) );

            auto hRequest = hRequestGuard.get();

            auto RequestData = LPVOID{ WINHTTP_NO_REQUEST_DATA };
            auto RequestSize = DWORD{ 0 };

            if( ! FormString.empty() )
            {
                WinHttpAddRequestHeaders( hRequest, L"Content-Type: application/x-www-form-urlencoded\r\n",
                                          static_cast<DWORD>( -1 ),
                                          WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE );
                RequestData = const_cast<LPVOID>( static_cast<LPCVOID>( FormString.data() ) );
                RequestSize = static_cast<DWORD>( FormString.length() );
            }

            if( WinHttpSendRequest( hRequest,                          //
                                    WINHTTP_NO_ADDITIONAL_HEADERS, 0,  // Headers
                                    RequestData, RequestSize,          // Optional
                                    RequestSize, 0 ) &&                // short circuit
                WinHttpReceiveResponse( hRequest, WINHTTP_RECEICVE_RESERVE_PARAM ) )
                return GetResponse( hRequest );
            else
            {
                SayError( "Send Request Failed" );
                return {};
            }
        }

        auto SendRequest_DiscardReturnValue( LPCWSTR RequestVerb, LPCWSTR Action, string_view FormString ) -> void
        {
            auto hRequestGuard = ResourceGuard<WinHttpCloseHandle>(  //
                WinHttpOpenRequest( hConnect, RequestVerb, Action,   //
                                    WINHTTP_HTTP_VERSION,            //
                                    WINHTTP_NO_REFERER,              //
                                    WINHTTP_DEFAULT_ACCEPT_TYPES,    //
                                    WINHTTP_FLAG_REFRESH | SecurityFlag ) );

            auto hRequest = hRequestGuard.get();

            auto RequestData = LPVOID{ WINHTTP_NO_REQUEST_DATA };
            auto RequestSize = DWORD{ 0 };

            if( ! FormString.empty() )
            {
                WinHttpAddRequestHeaders( hRequest, L"Content-Type: application/x-www-form-urlencoded\r\n",
                                          static_cast<DWORD>( -1 ),
                                          WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE );
                RequestData = const_cast<LPVOID>( static_cast<LPCVOID>( FormString.data() ) );
                RequestSize = static_cast<DWORD>( FormString.length() );
            }

            if( WinHttpSendRequest( hRequest,                          //
                                    WINHTTP_NO_ADDITIONAL_HEADERS, 0,  // Headers
                                    RequestData, RequestSize,          // Optional
                                    RequestSize, 0 ) &&                // short circuit
                WinHttpReceiveResponse( hRequest, WINHTTP_RECEICVE_RESERVE_PARAM ) )
            {}
            else
                SayError( "Send Request Failed" );
        }

        auto POST( string_view Action, string_view Form )
        {
            return SendRequest( L"POST", ConvertTo<wstring>( Action ).c_str(), Form );
        }

        auto POST( string_view Action, string_view Form, void* ) -> void
        {
            SendRequest_DiscardReturnValue( L"POST", ConvertTo<wstring>( Action ).c_str(), Form );
        }

        auto GET( string_view Action, string_view Form )
        {
            return SendRequest(
                L"GET", ( ConvertTo<wstring>( Action ) + wchar_t( '?' ) + ConvertTo<wstring>( Form ) ).c_str(), {} );
        }

        auto GET( string_view Action, string_view Form, void* ) -> void
        {
            SendRequest_DiscardReturnValue(
                L"GET", ( ConvertTo<wstring>( Action ) + wchar_t( '?' ) + ConvertTo<wstring>( Form ) ).c_str(), {} );
        }
    };

}  // namespace Http

using Http::operator<<;  // NOLINT

#endif