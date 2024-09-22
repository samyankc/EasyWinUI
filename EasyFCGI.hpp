#ifndef _EASY_FCGI_HPP
#define _EASY_FCGI_HPP
#include <cstddef>
#include <iterator>
#include <string_view>
#include <type_traits>
#define NO_FCGI_DEFINES
#include <fcgiapp.h>
#include <memory>
#include <concepts>
#include <utility>
#include <vector>
#include <ranges>
#include <algorithm>
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
#include "json.hpp"
// #include "EasyString.h"
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

namespace ParseUtil
{
    using namespace std::string_literals;
    using namespace std::string_view_literals;

    namespace RNG = std::ranges;
    namespace VIEW = std::views;

    using StrView = std::string_view;

    template<std::size_t N> struct FixedString
    {
        char Data[N];
        constexpr FixedString( const char ( &Src )[N] ) noexcept { std::copy_n( Src, N, Data ); }
        constexpr operator StrView() const noexcept { return { Data, N }; }
    };

    template<FixedString FSTR> constexpr auto operator""_FMT() noexcept
    {
        return []( auto&&... args ) { return std::format( FSTR, std::forward<decltype( args )>( args )... ); };
    }

    struct StrViewPattern
    {
        constexpr StrViewPattern( const StrViewPattern& Other )
            : CharPattern{ Other.CharPattern },
              Pattern{ Other.CharPattern == '\0' ? Other.Pattern : StrView{ &CharPattern, 1 } }
        {}
        constexpr StrViewPattern( StrView Pattern ) : Pattern{ Pattern }, CharPattern{ '\0' } {}
        constexpr StrViewPattern( char CharPattern ) : CharPattern{ CharPattern }, Pattern{ &this->CharPattern, 1 } {}
        constexpr operator StrView() const { return Pattern; }

      protected:
        StrView Pattern;
        char CharPattern;
    };

    inline namespace RangeAdaptor
    {
        struct CollapseToEndRA : RNG::range_adaptor_closure<CollapseToEndRA>
        {
            constexpr auto static operator()( StrView Input ) { return StrView{ Input.end(), Input.end() }; }
        };
        constexpr auto CollapseToEnd = CollapseToEndRA{};

        struct FrontRA : RNG::range_adaptor_closure<FrontRA>
        {
            constexpr auto static operator()( auto&& Range ) { return *std::begin( Range ); }
        };
        constexpr auto Front = FrontRA{};

        struct BoundaryRA : RNG::range_adaptor_closure<BoundaryRA>
        {
            constexpr auto static operator()( auto&& Range ) { return std::array{ RNG::begin( Range ), RNG::end( Range ) }; }
        };
        constexpr auto Boundary = BoundaryRA{};

        struct TrimSpaceRA : RNG::range_adaptor_closure<TrimSpaceRA>
        {
            constexpr static StrView operator()( StrView Input )
            {
                auto SpaceRemoved = Input                                            //
                                    | VIEW::drop_while( ::isspace ) | VIEW::reverse  //
                                    | VIEW::drop_while( ::isspace ) | VIEW::reverse;
                return { &*RNG::begin( SpaceRemoved ),                               //
                         &*RNG::rbegin( SpaceRemoved ) + 1 };
            }
        };
        constexpr auto TrimSpace = TrimSpaceRA{};

        struct Trim : StrViewPattern, RNG::range_adaptor_closure<Trim>
        {
            using StrViewPattern::StrViewPattern;
            constexpr StrView operator()( StrView Input ) const
            {
                while( Input.starts_with( Pattern ) ) Input.remove_prefix( Pattern.length() );
                while( Input.ends_with( Pattern ) ) Input.remove_suffix( Pattern.length() );
                return Input;
            }
        };

        struct Search : StrViewPattern, RNG::range_adaptor_closure<Search>
        {
            using StrViewPattern::StrViewPattern;
            constexpr StrView operator()( StrView Input ) const { return StrView{ RNG::search( Input, Pattern ) }; }
            constexpr StrView In( StrView Input ) const { return operator()( Input ); }
        };

        struct Before : StrViewPattern, RNG::range_adaptor_closure<Before>
        {
            using StrViewPattern::StrViewPattern;
            constexpr StrView operator()( StrView Input ) const
            {
                auto [InputBegin, InputEnd] = Input | Boundary;
                auto [MatchBegin, MatchEnd] = Input | Search( Pattern ) | Boundary;
                if( MatchBegin == InputEnd ) return { InputEnd, InputEnd };
                return { InputBegin, MatchBegin };
            }
        };

        struct After : StrViewPattern, RNG::range_adaptor_closure<After>
        {
            using StrViewPattern::StrViewPattern;
            constexpr StrView operator()( StrView Input ) const
            {
                auto [InputBegin, InputEnd] = Input | Boundary;
                auto [MatchBegin, MatchEnd] = Input | Search( Pattern ) | Boundary;
                return { MatchEnd, InputEnd };
            }
        };

        struct Between : RNG::range_adaptor_closure<Between>
        {
            StrViewPattern Left, Right;
            constexpr Between( auto&& Left, auto&& Right ) : Left{ StrViewPattern{ Left } }, Right{ StrViewPattern{ Right } } {}
            constexpr StrView operator()( StrView Input ) const { return Input | After( Left ) | Before( Right ); }
        };

        struct Count : StrViewPattern, RNG::range_adaptor_closure<Count>
        {
            using StrViewPattern::StrViewPattern;
            constexpr std::size_t operator()( StrView Input ) const
            {
                if( Input.empty() ) return 0;
                if( Pattern.empty() ) return Input.length();
                auto OverShoot = Input.ends_with( Pattern ) ? 0 : 1;
                auto Counter = 0uz;
                while( ! Input.empty() )
                {
                    Input = Input | After( Pattern );
                    ++Counter;
                }
                return Counter - OverShoot;
            }
        };

        struct SplitBy : StrViewPattern, RNG::range_adaptor_closure<SplitBy>
        {
            using StrViewPattern::StrViewPattern;
            constexpr auto operator()( StrView Input ) const
            {
                auto Result = std::vector<StrView>();
                Result.reserve( Input | Count( Pattern ) );
                if( Input.empty() )
                {
                    Result.push_back( Input );
                    return Result;
                }
                while( ! Input.empty() )
                {
                    auto Segment = Input | Before( Pattern );
                    if( Segment.end() == Input.end() ) Segment = Input;
                    Result.push_back( Segment );
                    Input = StrView{ Segment.end(), Input.end() } | After( Pattern );
                }
                return Result;
            }
        };

        struct Split
        {
            StrView Input;
            constexpr auto By( StrViewPattern Pattern ) const { return Input | SplitBy( Pattern ); }
        };

        template<typename NumericType, int BASE>  //
        struct ConvertToRA : RNG::range_adaptor_closure<ConvertToRA<NumericType, BASE>>
        {
            constexpr static auto operator()( StrView Input ) -> std::optional<NumericType>
            {
                Input = Input | TrimSpace | Trim( '+' ) | TrimSpace;
                NumericType Result;
                if constexpr( std::integral<NumericType> )
                {
                    if( std::from_chars( Input.data(), Input.data() + Input.size(), Result, BASE ) ) return Result;
                }
                else  // floating point
                {
                    if( std::from_chars( Input.data(), Input.data() + Input.size(), Result ) ) return Result;
                }
                return std::nullopt;
            }
        };
        template<typename NumericType, int BASE = 10>  //
        constexpr auto ConvertTo = ConvertToRA<NumericType, BASE>{};

        struct RestoreSpaceCharRA : RNG::range_adaptor_closure<RestoreSpaceCharRA>
        {
            constexpr static auto operator()( const auto& Input )
            {
                auto Result = static_cast<std::string>( Input );
                RNG::for_each( Result, []( char& c ) {
                    if( c == '+' ) c = ' ';
                } );
                return Result;
            };
        };
        constexpr auto RestoreSpaceChar = RestoreSpaceCharRA{};

        struct SliceAt : RNG::range_adaptor_closure<SliceAt>
        {
            std::size_t N;

            constexpr SliceAt( std::size_t N ) : N{ N } {}

            constexpr auto operator()( StrView Input ) const -> std::array<StrView, 2>
            {
                if( N >= Input.length() )
                    return { Input,  //
                             Input | CollapseToEnd };
                else
                    return { Input.substr( 0, N ),  //
                             Input.substr( N, Input.length() - N ) };
            }

            template<typename BaseRangeType>  //
            constexpr auto operator()( BaseRangeType&& BaseRange ) const -> std::array<std::remove_cvref_t<BaseRangeType>, 2>
            {
                if( N >= BaseRange.size() )
                    return { std::move( BaseRange ),  //
                             BaseRangeType{} };
                else
                {
                    auto [Begin, End] = BaseRange | Boundary;
                    auto Pivot = std::next( Begin, N );
                    return { BaseRangeType{ Begin, Pivot },  //
                             BaseRangeType{ Pivot, End } };
                }
            }
        };

    }  // namespace RangeAdaptor

    [[nodiscard]]
    static auto HexToChar( StrView HexString ) noexcept
    {
        return std::bit_cast<char>( (HexString | ConvertTo<unsigned char, 16>).value_or( '?' ) );
    }

    [[nodiscard]]
    static auto DecodeURLFragment( StrView Fragment )
    {
        constexpr auto EncodeDigitWidth = 2;
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

};  // namespace ParseUtil

using ParseUtil::operator""_FMT;

// template<> inline constexpr bool std::ranges::enable_borrowed_range<ParseUtil::SplitByViews> = true;

namespace HTTP
{
    // using namespace EasyString;
    // using namespace ParseUtil;
    namespace PU = ParseUtil;

    struct RequestMethod
    {
        enum class EnumValue { INVALID = 0, GET, HEAD, POST, PUT, DELETE, CONNECT, OPTIONS, TRACE, PATCH };
        EnumValue Verb;
        using enum EnumValue;

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
        static constexpr auto ToStringView( EnumValue Verb ) -> std::string_view
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
            std::unreachable();
        }
#undef CASE_RETURN

        constexpr RequestMethod() = default;
        constexpr RequestMethod( const RequestMethod& ) = default;
        constexpr RequestMethod( EnumValue OtherVerb ) : Verb{ OtherVerb } {}
        constexpr RequestMethod( std::string_view VerbName ) : Verb{ FromStringView( VerbName ) } {}

        constexpr operator std::string_view() const { return ToStringView( Verb ); }
        constexpr auto EnumLiteral() const { return ToStringView( Verb ); }

        constexpr operator EnumValue() const { return Verb; }
    };

    enum class StatusCode : unsigned short {
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
        enum class EnumValue : unsigned short {
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
        EnumValue Type;

        static constexpr auto FromStringView( std::string_view TypeName )
        {
            using enum EnumValue;
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

        static constexpr auto ToStringView( EnumValue Type ) -> std::string_view
        {
            switch( Type )
            {
                using enum EnumValue;
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
                case UNKNOWN_MIME_TYPE :                 return ToStringView( TEXT_PLAIN );
            }
            return "";
        }

        struct Text;
        struct Application;
        struct MultiPart;

        constexpr ContentType() = default;
        constexpr ContentType( const ContentType& ) = default;
        constexpr ContentType( EnumValue Other ) : Type{ Other } {}
        constexpr ContentType( std::string_view TypeName ) : Type{ FromStringView( TypeName | PU::SplitBy( ';' ) | PU::Front | PU::TrimSpace ) }
        {
            // if( Type == UNKNOWN_MIME_TYPE ) Type = TEXT_PLAIN;
        }

        constexpr operator std::string_view() const { return ToStringView( Type ); }
        constexpr auto EnumLiteral() const { return ToStringView( Type ); }

        constexpr operator EnumValue() const { return Type; }
    };

    struct ContentType::Text
    {
        constexpr static ContentType Plain = EnumValue::TEXT_PLAIN;
        constexpr static ContentType HTML = EnumValue::TEXT_HTML;
        constexpr static ContentType XML = EnumValue::TEXT_XML;
        constexpr static ContentType CSV = EnumValue::TEXT_CSV;
        constexpr static ContentType CSS = EnumValue::TEXT_CSS;
    };

    struct ContentType::Application
    {
        constexpr static ContentType Json = EnumValue::APPLICATION_JSON;
        constexpr static ContentType FormURLEncoded = EnumValue::APPLICATION_X_WWW_FORM_URLENCODED;
        constexpr static ContentType OctetStream = EnumValue::APPLICATION_OCTET_STREAM;
    };

    struct ContentType::MultiPart
    {
        constexpr static ContentType FormData = EnumValue::MULTIPART_FORM_DATA;
        constexpr static ContentType ByteRanges = EnumValue::MULTIPART_BYTERANGES;
    };

}  // namespace HTTP

namespace EasyFCGI
{
    namespace FS = std::filesystem;
    namespace PU = ParseUtil;
    namespace RNG = std::ranges;
    namespace VIEW = std::views;
    using Json = nlohmann::json;
    using StrView = std::string_view;

    inline namespace Concept
    {
        template<typename T>
        concept DumpingString = requires( T&& t ) {
            { t.dump() } -> std::same_as<std::string>;
        };
    }

    struct Config
    {
        // evetually capped by /proc/sys/net/core/somaxconncat,eg.4096
        constexpr static auto DefaultBackLogNumber = 128;

        // static auto SocketDirectoryPrefix = StrView{"/tmp"};
        constexpr static auto SocketDirectoryPrefix = StrView{ "/dev/shm/" };
        constexpr static auto SocketExtensionSuffix = StrView{ ".sock" };

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

            return VIEW::split( CommandLineBuffer, '\0' )  //
                   | VIEW::transform( RNG::data )          //
                   | RNG::to<std::vector>();
        }();

        inline static auto DefaultSocketPath = FS::path( Config::SocketDirectoryPrefix ) /  //
                                               FS::path( CommandLine[0] )
                                                   .filename()                              //
                                                   .replace_extension( Config::SocketExtensionSuffix );

        inline static auto OptionSocketPath = [] -> std::optional<FS::path> {
            for( auto&& [Option, OptionArg] : CommandLine | VIEW::adjacent<2> )
                if( Option == StrView{ "-s" } )  //
                    return OptionArg;
            return {};
        }();
    };

    static auto TerminationSignal = std::atomic<bool>{ false };
    static auto TerminationHandler = []( int Signal ) {
        std::println( "\nReceiving Signal : {}", Signal );
        TerminationSignal = true;
    };

    extern "C" void OS_LibShutdown();  // <fcgios.h>
    static auto ServerInitializationComplete = std::atomic<bool>{ false };
    static auto ServerInitialization = [] {
        if( ServerInitializationComplete ) return;
        auto FALSE = false;
        if( ! ServerInitializationComplete.compare_exchange_strong( FALSE, true ) ) return;

        ErrorGuard( FCGX_Init() );
        std::atexit( OS_LibShutdown );
        std::atexit( [] { std::println( "Porgram exiting from standard exit pathway." ); } );
        std::signal( SIGINT, TerminationHandler );
        std::signal( SIGTERM, TerminationHandler );
    };

    struct FileDescriptor
    {
        using NativeFileDescriptorType = decltype( FCGX_OpenSocket( {}, {} ) );
        NativeFileDescriptorType FD;

        constexpr FileDescriptor() : FD{} {}

        FileDescriptor( const FS::path& SocketPath )  //
            : FD{ SocketPath.empty()                  //
                      ? NativeFileDescriptorType{}
                      : FCGX_OpenSocket( SocketPath.c_str(), Config::DefaultBackLogNumber ) }
        {
            if( FD == -1 )
            {
                std::println( "Failed to open socket." );
                std::exit( -1 );
            }
            if( FD > 0 ) { FS::permissions( SocketPath, FS::perms::all ); }
        }

        constexpr operator NativeFileDescriptorType() const { return FD; }

        auto UnixSocketName() const -> FS::path
        {
            auto UnixAddr = sockaddr_un{};
            auto UnixAddrLen = socklen_t{ sizeof( UnixAddr ) };

            if( getsockname( FD, (sockaddr*)&UnixAddr, &UnixAddrLen ) == 0 )
            {
                switch( UnixAddr.sun_family )
                {
                    case AF_UNIX :
                        return FS::canonical( UnixAddr.sun_path );
                        // case AF_INET : return "0.0.0.0";
                }
            }
            return {};
        }
    };

    struct Response
    {
        std::vector<std::string> Body;
        HTTP::StatusCode StatusCode{ HTTP::StatusCode::OK };
        HTTP::ContentType ContentType{ HTTP::ContentType::Text::Plain };

        [[maybe_unused]] decltype( auto ) Set( HTTP::StatusCode NewValue ) { return StatusCode = NewValue, *this; }
        [[maybe_unused]] decltype( auto ) Set( HTTP::ContentType NewValue ) { return ContentType = NewValue, *this; }

        [[maybe_unused]] decltype( auto ) Reset()
        {
            Set( HTTP::StatusCode::OK );
            Set( HTTP::ContentType::Text::Plain );
            Body.clear();
            return *this;
        }

        template<typename T> [[maybe_unused]] decltype( auto ) Append( T&& NewContent ) noexcept
        {
            if constexpr( DumpingString<T> ) { Body.push_back( NewContent.dump() ); }
            else { Body.push_back( static_cast<std::string>( std::forward<T>( NewContent ) ) ); }
            return *this;
        }

        template<typename T> [[maybe_unused]] decltype( auto ) operator+=( T&& NewContent ) noexcept { return Append( std::forward<T>( NewContent ) ); }

        template<typename T> requires( ! std::same_as<std::remove_cvref_t<T>, Response> )
        [[maybe_unused]] decltype( auto ) operator=( T&& NewContent ) noexcept
        {
            Body.clear();
            return Append( std::forward<T>( NewContent ) );
        }

        // template<std::default_initializable T> operator T() const { return {}; }
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

        template<typename StorageEngine>  //
        struct QueryExecutor
        {
            StorageEngine Json;
            auto contains( StrView Key ) const { return Json.contains( Key ); }
            auto operator[]( StrView Key ) const
            {
                if( Json.contains( Key ) )
                {
                    auto& Slot = Json[Key];
                    if( Slot.is_string() ) return Slot.template get<std::string>();
                    else return Slot.dump();
                }
                else return std::string{};
            }
        };

        std::unique_ptr<FCGX_Request, FCGX_Request_Deleter> FCGX_Request_Ptr;
        HTTP::RequestMethod Method;
        HTTP::ContentType ContentType;
        QueryExecutor<Json> Query;
        // std::size_t ContentLength;
        // std::string_view ScriptName;
        // std::string_view RequestURI;
        // std::string_view QueryString;
        // std::string_view Body;
        struct Response Response;  // elaborated-type-specifier, silencing -Wchanges-meaning

        // Read FCGI envirnoment variables set up by upstream server
        auto GetParam( StrView ParamName ) const -> StrView
        {
            auto Result = FCGX_GetParam( std::data( ParamName ), FCGX_Request_Ptr->envp );
            if( Result == nullptr ) return {};
            else return Result;
        }

        auto Parse()
        {
            // Query.Json.clear();
            Method = GetParam( "REQUEST_METHOD" );
            ContentType = GetParam( "CONTENT_TYPE" );

            auto QueryAppend = [&Result = Query.Json]( std::string Key, std::string Value ) {
                if( Result.contains( Key ) )
                {
                    auto& Slot = Result[Key];
                    if( Slot.is_array() ) { Slot.push_back( std::move( Value ) ); }
                    else
                    {
                        auto Temp = std::move( Slot );
                        Result.erase( Key );
                        auto& Array = Result[Key];
                        Array.push_back( std::move( Temp ) );
                        Array.push_back( std::move( Value ) );
                    }
                }
                else { Result[Key] = std::move( Value ); }
            };

            switch( Method )
            {
                case HTTP::RequestMethod::GET :
                {
                    for( auto Segment : StrView{ GetParam( "QUERY_STRING" ) } | PU::SplitBy( '&' ) )
                        for( auto [EncodedKey, EncodedValue] : Segment | PU::SplitBy( '=' ) | VIEW::adjacent<2> )
                            QueryAppend( PU::DecodeURLFragment( EncodedKey ), PU::DecodeURLFragment( EncodedValue ) );
                }
                break;

                default : break;
            }
        }

        Request() = default;
        // Request( const Request& ) = delete;
        Request( Request&& Other ) = default;

        Request( FileDescriptor SocketFD ) : FCGX_Request_Ptr{ std::make_unique_for_overwrite<FCGX_Request>().release() }
        {
            if( FCGX_InitRequest( FCGX_Request_Ptr.get(), SocketFD, FCGI_FAIL_ACCEPT_ON_INTR ) == 0 &&  //
                FCGX_Accept_r( FCGX_Request_Ptr.get() ) == 0 )
            {
                // FCGX_Request_Ptr ready, setup the rest of request object(parse request)
                Parse();
                return;
            }

            // fail to obtain valid request, reset residual request data & allocation
            FCGX_InitRequest( FCGX_Request_Ptr.get(), {}, {} );
            FCGX_Request_Ptr.reset();
        }

        static auto AcceptFrom( FileDescriptor SocketFD ) { return Request{ SocketFD }; }

        Request& operator=( Request&& Other ) = default;

        auto empty() const { return FCGX_Request_Ptr == nullptr; }

        auto FlushResponse() -> void
        {
            auto SendLine = [out = FCGX_Request_Ptr->out]( StrView Content = {} ) {
                if( ! Content.empty() ) FCGX_PutStr( Content.data(), Content.length(), out );
                FCGX_PutStr( "\r\n", 2, out );
            };
            if( Response.StatusCode == HTTP::StatusCode::NoContent ) { SendLine( "Status: 204\r\n" ); }
            else
            {
                SendLine( "Status: {}"_FMT( std::to_underlying( Response.StatusCode ) ) );
                SendLine( "Content-Type: {}; charset=UTF-8"_FMT( Response.ContentType.EnumLiteral() ) );
                SendLine();
                RNG::for_each( Response.Body, SendLine );
            }
            // Response.Reset();  // seems not necessary anymore
        }

        virtual ~Request()
        {
            if( FCGX_Request_Ptr == nullptr ) return;
            std::println( "{} , {} Request Destruction...", getpid(), std::this_thread::get_id() );
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

    // RequestContainer is not thread safe by default.
    // It is expected to be handled by one managing thread
    // For handling RequestContainer by multiple threads,
    // users should manage their own mutex
    struct PseudoRequestQueue
    {
        FileDescriptor SocketFD;
        Request PendingRequest;

        struct Sentinel
        {};
        struct Iterator
        {
            PseudoRequestQueue& BaseRange;
            Request&& operator*() { return std::move( BaseRange.PendingRequest ); }
            auto& operator++() & { return *this; }
        };

        auto begin() { return Iterator{ *this }; }
        auto end() { return Sentinel{}; }

        friend auto operator==( const Iterator& Iterator, const Sentinel& )
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

    struct Server
    {
        FileDescriptor SocketFD;

        Server( FileDescriptor FD ) : SocketFD{ FD }
        {
            ServerInitialization();

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
        Server( const FS::path& SocketPath ) : Server{ FileDescriptor{ SocketPath } } {}

        // auto operator=( auto&& ) = delete;

        auto NextRequest() const { return Request::AcceptFrom( SocketFD ); }

        auto RequestQueue() const { return PseudoRequestQueue{ .SocketFD = SocketFD }; }
    };

}  // namespace EasyFCGI

#endif
