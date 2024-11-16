#ifndef EASYSTRING_H
#define EASYSTRING_H

#include <algorithm>
#include <ranges>
#include <cstring>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>
#include <functional>
#include <format>
#include <charconv>
#include <variant>
#include <memory>

namespace
{
    template<typename T>
    concept Provide_c_str = requires( T t ) { t.c_str(); };

    template<typename T>
    concept NotProvide_c_str = ! Provide_c_str<T>;

    template<typename F>
    struct FunctionType : std::false_type
    {};

    template<typename R, typename... Args>
    struct FunctionType<R( Args... )> : std::true_type
    {};

    template<typename R, typename... Args>
    struct FunctionType<R ( * )( Args... )> : std::true_type
    {};

    template<typename F>
    concept FunctionPointer = FunctionType<F>::value;

    using SafeCharPtrBase = std::variant<const char*, std::unique_ptr<const char[]>>;
    struct SafeCharPtr : SafeCharPtrBase
    {
        using SafeCharPtrBase::SafeCharPtrBase;
        template<typename... Ts>
        struct Visitor : Ts...
        {
            using Ts::operator()...;
        };

        constexpr operator const char*() const
        {
            return std::visit( Visitor{ []( const char* Ptr ) { return Ptr; },                                   //
                                        []( const std::unique_ptr<const char[]>& Rest ) { return Rest.get(); },  //
                                        []( auto&& ) { return ""; } },                                           //
                               *this );
        }
    };
}  // namespace

namespace std
{
    constexpr auto c_str( const char* Source ) noexcept { return Source; }

    template<Provide_c_str T>
    constexpr auto c_str( T&& Source ) noexcept
    {
        return std::forward<T>( Source ).c_str();
    }

    template<NotProvide_c_str T>
    constexpr auto c_str( T&& Source ) noexcept -> SafeCharPtr
    {
        auto Buffer = std::data( std::forward<T>( Source ) );
        auto Size = std::size( std::forward<T>( Source ) );

        if( Size == 0 ) return "";
        if( Buffer[Size] == '\0' ) return Buffer;

        auto NewBuffer = new char[Size + 1];
        std::copy_n( Buffer, Size, NewBuffer );
        NewBuffer[Size] = '\0';
        return std::unique_ptr<const char[]>{ NewBuffer };
    }

}  // namespace std

namespace EasyString
{
    using namespace std::string_literals;
    using namespace std::string_view_literals;
    using StrView = std::string_view;

    inline namespace Concepts
    {
        template<typename AdaptorType>
        concept StrViewAdaptable = requires( AdaptorType Adaptor, StrView SV ) { Adaptor( SV ); };

        template<typename T>
        concept StrViewConvertible = std::convertible_to<T, StrView>;
    }

    template<StrViewAdaptable T>
    constexpr decltype( auto ) operator|( StrView Input, T&& Adaptor ) noexcept
    {
        return std::forward<T>( Adaptor )( Input );
    }

    template<StrViewAdaptable T>
    constexpr StrView& operator|=( StrView& Input, T&& Adaptor ) noexcept
    {
        return Input = Input | std::forward<T>( Adaptor );
    }

    inline auto LoadFileContent( const char* FileName ) -> std::string
    {
        using it_type = std::istreambuf_iterator<char>;
        std::ifstream Fin( FileName );
        if( Fin ) return { it_type( Fin ), it_type( /*default_sentinel*/ ) };
        return {};
    }

    struct StrViewUnit
    {
        constexpr StrViewUnit( StrViewConvertible auto Source ) noexcept : Text{ Source } {}

      protected:
        StrView Text;
    };

    struct StrViewPair
    {
        constexpr StrViewPair( StrViewConvertible auto LeftSource,  //
                               StrViewConvertible auto RightSource ) noexcept
            : Left{ LeftSource },
              Right{ RightSource }
        {}

      protected:
        StrView Left, Right;
    };

    constexpr auto Write( StrView Content )
    {
        struct Impl : StrViewUnit
        {
            auto To( StrView Location ) const
            {
                auto File = std::ofstream( Location.data(), std::ofstream::trunc );
                auto Output = std::ostreambuf_iterator( File );
                if( File ) std::ranges::copy( Text, Output );
            }
        };
        return Impl{ Content };
    }

    constexpr auto Search( StrView Pattern )
    {
        struct Impl : StrViewUnit
        {
          protected:
            constexpr auto Search_impl( StrView Input ) const
            {
                if consteval  //
                {
                    return std::search( Input.begin(), Input.end(), Text.begin(), Text.end() );
                }
                else
                {
                    return std::search( Input.begin(), Input.end(),  //
                                        std::boyer_moore_searcher( Text.begin(), Text.end() ) );
                }
            }

          public:
            constexpr auto In( StrView Input ) const -> StrView
            {
                auto MatchBegin = Search_impl( Input );
                return { MatchBegin, MatchBegin == Input.end() ? 0 : Text.length() };
            }

            constexpr auto operator()( StrView Source ) const { return In( Source ); }
        };
        return Impl{ Pattern };
    }

    constexpr auto Trim( StrView ExcludeChars )
    {
        struct Impl : StrViewUnit
        {
            constexpr auto From( StrView Input ) const
            {
                if( ! Input.empty() )
                {
                    if( auto pos = Input.find_first_not_of( Text ); pos != Input.npos )
                        Input.remove_prefix( pos );
                    else
                        Input.remove_prefix( Input.length() );

                    if( auto pos = Input.find_last_not_of( Text ); pos != Input.npos )  //
                        Input.remove_suffix( Input.length() - pos - 1 );
                }
                return Input;
            }

            constexpr auto operator()( StrView Source ) const { return From( Source ); };
        };
        return Impl{ ExcludeChars };
    }

    constexpr auto TrimSpace( StrView Input ) -> StrView
    {
        Input = { std::ranges::find_if_not( Input, ::isspace ), Input.end() };
        if( Input.empty() ) return Input;
        return { Input.begin(), std::ranges::find_last_if_not( Input, ::isspace ).begin() + 1 };
    }

    constexpr auto After( StrView Pattern )
    {
        return [=]( StrView Input ) -> StrView { return { Search( Pattern ).In( Input ).end(), Input.end() }; };
    }

    constexpr auto Before( StrView Pattern )
    {
        return [=]( StrView Input ) -> StrView {
            auto RightBound = Search( Pattern ).In( Input ).begin();
            auto LeftBound = RightBound == Input.end() ? Input.end() : Input.begin();
            return { LeftBound, RightBound };
        };
    }

    constexpr auto Between( StrView Left, StrView Right )
    {
        return [=]( StrView Input ) {
            return Input            //
                   | After( Left )  //
                   | Before( Right );
        };
    }

    constexpr auto Count( StrView Pattern )
    {
        struct Impl : StrViewUnit
        {
            constexpr auto In( StrView Input ) const
            {
                auto Count = static_cast<std::size_t>( Input.ends_with( Text ) );
                while( ! ( Input |= After( Text ) ).empty() ) ++Count;
                return Count;
            }

            constexpr auto operator()( StrView Input ) const { return In( Input ); }
        };
        return Impl{ Pattern };
    }

    constexpr auto Split__deprecated( StrView Pattern )
    {
        struct Impl : StrViewUnit
        {
            struct InternalItorSentinel
            {};

            struct InternalItor : StrViewUnit
            {
                const char Delimiter;

                constexpr auto operator*() const
                {
                    auto DelimiterPos = std::find( Text.begin(), Text.end(), Delimiter );
                    return StrView{ Text.begin(), DelimiterPos };
                }

                constexpr auto operator!=( InternalItorSentinel ) const { return ! Text.empty(); }
                constexpr auto& operator++()
                {
                    auto DelimiterPos = std::find( Text.begin(), Text.end(), Delimiter );
                    if( DelimiterPos == Text.end() )
                        Text.remove_suffix( Text.length() );
                    else
                        Text = StrView{ DelimiterPos + 1, Text.end() };
                    return *this;
                }
                constexpr auto operator++( int )
                {
                    auto CacheIt = *this;
                    this->operator++();
                    return CacheIt;
                }
            };

            struct InternalRange : StrViewUnit
            {
                const char Delimiter;

                constexpr auto begin() const { return InternalItor{ Text, Delimiter }; }
                constexpr auto end() const { return InternalItorSentinel{}; }
                constexpr auto size() const
                {
                    return ! Text.ends_with( Delimiter )  // ending delim adjustment
                           + std::count( Text.begin(), Text.end(), Delimiter );
                }
                constexpr auto front() const { return *begin(); }
            };

            constexpr auto By( const char Delimiter ) const { return InternalRange{ Text, Delimiter }; }
        };

        return Impl{ Pattern };
    }

    constexpr auto Split( StrView Pattern )
    {
        struct Impl : StrViewUnit
        {
            using DelimiterType = std::string_view;
            struct InternalItorSentinel
            {};

            struct InternalItor : StrViewUnit
            {
                DelimiterType Delimiter;

                constexpr auto operator*() const { return StrView{ Text.begin(), Search( Delimiter ).In( Text ).begin() }; }

                constexpr auto operator!=( InternalItorSentinel ) const { return ! Text.empty(); }
                constexpr auto& operator++()
                {
                    auto DelimiterPos = Search( Delimiter ).In( Text ).begin();
                    if( DelimiterPos == Text.end() )
                        Text.remove_prefix( Text.length() );
                    else
                        Text = StrView{ DelimiterPos + Delimiter.length(), Text.end() };
                    return *this;
                }
                constexpr auto operator++( int )
                {
                    auto CacheIt = *this;
                    this->operator++();
                    return CacheIt;
                }
            };

            struct InternalRange : StrViewUnit
            {
                DelimiterType Delimiter;

                constexpr auto begin() const { return InternalItor{ Text, Delimiter }; }
                constexpr auto end() const { return InternalItorSentinel{}; }
                constexpr auto size() const
                {
                    return ! Text.ends_with( Delimiter )  // ending delim adjustment
                           + Count( Delimiter ).In( Text );
                }
                constexpr auto front() const { return *begin(); }
            };

            constexpr auto By( DelimiterType Delimiter ) const { return InternalRange{ Text, Delimiter }; }
            constexpr auto operator()( DelimiterType Delimiter ) const { return By( Delimiter ); }
        };

        return Impl{ Pattern };
    }

    constexpr auto SplitBy( std::string_view Delimiter )
    {
        return [=]( StrView Pattern ) { return Split( Pattern ).By( Delimiter ); };
    }

    template<typename NumericType, int BASE = 10>
    constexpr auto StrViewTo( StrView Source ) -> std::optional<NumericType>
    {
        constexpr auto Successful = []( const std::from_chars_result& R ) { return R.ec == std::errc{}; };
        Source |= TrimSpace;
        Source |= Trim( "+" );
        NumericType Result;
        if constexpr( std::integral<NumericType> )
        {
            if( Successful( std::from_chars( Source.data(), Source.data() + Source.size(), Result, BASE ) ) )  //
                return Result;
        }
        else  // floating point
        {
            if( Successful( std::from_chars( Source.data(), Source.data() + Source.size(), Result ) ) )  //
                return Result;
        }
        return std::nullopt;
    }

    template<size_t N>
    constexpr auto Bundle = std::in_place_index<N>;

    template<size_t N>
    constexpr auto operator|( const auto& Container, std::in_place_index_t<N> )
    {
        // return [&]<size_t... Is>( std::index_sequence<Is...> ) {return std::array{ Container[Is]... };}( std::make_index_sequence<N>() );
        using ValueType = std::decay_t<decltype( *std::begin( Container ) )>;
        auto Result = std::array<ValueType, N>{};
        auto It = std::begin( Container );
        for( auto i = size_t{ 0 }; i < std::max( N, std::size( Container ) ); ++i ) Result[i] = *It++;
        return std::array<std::array<ValueType, N>, 1>{ Result };
    }

    constexpr auto SplitBetween( StrView LeftDelimiter, StrView RightDelimiter )
    {
        struct Impl : StrViewPair
        {
            using StrViewPair::StrViewPair;

            struct InternalItorSentinel
            {};

            struct InternalItor
            {
                StrView BaseRange, LeftDelimiter, RightDelimiter;

                constexpr auto operator*() const { return BaseRange | Before( RightDelimiter ); }
                constexpr auto operator!=( InternalItorSentinel ) const { return ! BaseRange.empty(); }
                constexpr auto& operator++()
                {
                    BaseRange |= After( RightDelimiter );
                    BaseRange |= After( LeftDelimiter );
                    if( ! BaseRange.contains( RightDelimiter ) ) BaseRange.remove_prefix( BaseRange.length() );
                    return *this;
                }
                constexpr auto operator++( int )
                {
                    auto CacheIt = *this;
                    this->operator++();
                    return CacheIt;
                }
            };

            struct InternalRange
            {
                StrView BaseRange, LeftDelimiter, RightDelimiter;

                constexpr auto begin() const { return InternalItor{ BaseRange | After( LeftDelimiter ), LeftDelimiter, RightDelimiter }; }
                constexpr auto end() const { return InternalItorSentinel{}; }
            };

            constexpr auto operator()( StrView Input ) const { return InternalRange{ Input, Left, Right }; }
        };

        return Impl{ LeftDelimiter, RightDelimiter };
    }

    template<typename Predicate>
    struct DropIf
    {
        Predicate DropCondition;

        template<typename AncestorRange>
        struct InternalRange
        {
            AncestorRange BaseRange;
            Predicate DropCondition;

            auto begin()
            {
                auto First = BaseRange.begin();
                while( ( First != BaseRange.end() ) && DropCondition( *First ) ) ++First;
                return InternalItor{ *this, First };
            }
            auto end() { return BaseRange.end(); }
        };

        template<typename AncestorRange, typename AncestorItor>
        struct InternalItor
        {
            AncestorRange EnclosingRange;
            AncestorItor BaseItor;

            auto operator*() { return *BaseItor; }
            auto operator!=( const auto& RHS ) { return BaseItor != RHS; }
            auto operator++()
            {
                ++BaseItor;
                while( ( BaseItor != EnclosingRange.end() ) && EnclosingRange.DropCondition( *BaseItor ) ) ++BaseItor;
            }
        };

        friend auto operator|( auto SourceRange, const DropIf& Adaptor ) { return InternalRange{ SourceRange, Adaptor.DropCondition }; }
    };

    struct Take
    {
        int N;

        template<typename AncestorRange>
        struct InternalRange
        {
            AncestorRange BaseRange;
            int N;
            auto begin() { return InternalItor{ BaseRange.begin(), N }; }
            auto end() { return BaseRange.end(); }
        };

        template<typename AncestorItor>
        struct InternalItor
        {
            AncestorItor BaseItor;
            int N;

            auto operator*() { return *BaseItor; }
            auto operator!=( const auto& RHS ) { return N > 0 && BaseItor != RHS; }
            auto operator++()
            {
                if( N-- > 0 ) ++BaseItor;
            }
        };

        friend auto operator|( auto SourceRange, const Take& Adaptor ) { return InternalRange{ SourceRange, Adaptor.N }; }
    };

    struct Skip
    {
        int N;

        template<typename AncestorRange>
        struct InternalRange
        {
            AncestorRange BaseRange;
            int N;
            auto begin()
            {
                auto Begin = std::begin( BaseRange );
                auto End = std::end( BaseRange );
                while( N-- > 0 && Begin != End ) ++Begin;
                return Begin;
            }
            auto end() { return std::end( BaseRange ); }
        };

        friend auto operator|( auto SourceRange, const Skip& Adaptor ) { return InternalRange{ SourceRange, Adaptor.N }; }
    };

    struct SliceAt
    {
        int N;

        friend auto operator|( auto SourceRange, const SliceAt& Adaptor ) { return std::tuple{ SourceRange | Take( Adaptor.N ), SourceRange | Skip( Adaptor.N ) }; }

        friend constexpr auto operator|( std::string_view SourceRange, const SliceAt& Adaptor )
        {
            auto Begin = SourceRange.begin();
            auto End = SourceRange.end();
            if( std::distance( Begin, End ) >= Adaptor.N )
                return std::array{ std::string_view{ Begin, Begin + Adaptor.N }, std::string_view{ Begin + Adaptor.N, End } };
            else
                return std::array{ SourceRange, std::string_view{ End, End } };
        }
    };

    struct ReplaceChar
    {
        char OriginalChar;

        struct With
        {
            char OriginalChar;
            char ReplacementChar;

            friend auto operator|( std::string_view SourceRange, const struct With& Adaptor )
            {
                auto Result = std::string{ SourceRange };
                std::ranges::replace( Result, Adaptor.OriginalChar, Adaptor.ReplacementChar );
                return Result;
            }
        };

        constexpr auto With( char ReplacementChar ) const { return ( struct With ){ OriginalChar, ReplacementChar }; };
    };

    template<std::size_t N, typename CharT = char>
    struct FixedString
    {
        CharT Data[N];
        constexpr FixedString( const CharT ( &Src )[N] ) noexcept { std::copy_n( Src, N, Data ); }
        constexpr operator std::basic_string_view<CharT>() const noexcept { return { Data }; }
    };

    template<FixedString FSTR>
    constexpr auto operator""_FMT() noexcept
    {
        return []( auto&&... args ) { return std::format( FSTR, std::forward<decltype( args )>( args )... ); };
    }

    template<typename LeadingType, typename... Rest>
    constexpr auto EmptyCoalesce( LeadingType&& LeadingArg, Rest&&... RestArg )
    {
        if( ! std::empty( LeadingArg ) ) return std::forward<LeadingType>( LeadingArg );

        if constexpr( sizeof...( Rest ) == 0 )
            return LeadingType{};
        else
            return EmptyCoalesce( static_cast<LeadingType>( RestArg )... );
    }

    struct ToLowerCallableType
    {
        constexpr static auto operator()( char C )
        {
            return static_cast<char>( std::tolower( static_cast<unsigned char>( C ) ) );
        }
        constexpr static auto operator()( std::string_view Source )
        {
            auto Result = std::string( Source.length(), '\0' );
            std::ranges::transform( Source, Result.begin(), ToLowerCallableType{} );
            return Result;
        }
        template<FunctionPointer F>
        constexpr operator F() const
        {
            return operator();
        }
    };
    constexpr auto ToLower = ToLowerCallableType{};

    struct CaseInsensitiveEqualCallableType
    {
        constexpr static bool Equal_impl( char LHS, char RHS ) { return ToLower( LHS ) == ToLower( RHS ); }
        constexpr static bool operator()( char LHS, char RHS ) { return Equal_impl( LHS, RHS ); }
        constexpr static bool operator()( std::string_view LHS, std::string_view RHS )
        {
            if( LHS.length() != RHS.length() ) return false;
            return std::ranges::all_of( std::views::zip( LHS, RHS ), []( auto&& P ) { return Equal_impl( std::get<0>( P ), std::get<1>( P ) ); } );
        }
    };
    constexpr auto CaseInsensitiveEqual = CaseInsensitiveEqualCallableType{};

    struct CaseInsensitiveContainCallableType
    {
        constexpr static bool operator()( std::string_view Source, std::string_view Pattern )
        {  //
            return std::ranges::contains_subrange( Source, Pattern, CaseInsensitiveEqual );
        }
    };
    constexpr auto CaseInsensitiveContain = CaseInsensitiveContainCallableType{};

}  // namespace EasyString

using EasyString::operator""_FMT;

#endif
