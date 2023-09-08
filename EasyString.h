#ifndef EASYSTRING_H
#define EASYSTRING_H

#include <algorithm>
#include <cstring>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>
#include <functional>
#include <format>
#include <charconv>

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
    constexpr decltype( auto ) operator|( StrView Input, T && Adaptor ) noexcept
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
        constexpr StrViewPair( StrViewConvertible auto LeftSource, StrViewConvertible auto RightSource ) noexcept
            : Left{ LeftSource }, Right{ RightSource }
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
                if consteval
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
                // if( MatchBegin == Input.end() )
                //     return { Input.end(), 0 };
                // else
                //     return { MatchBegin, Text.length() };
                return { MatchBegin, ( MatchBegin != Input.end() ) * Text.length() };
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

                    if( auto pos = Input.find_last_not_of( Text ); pos != Input.npos )
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
        constexpr auto SpaceChar = []( const char c ) {
            return c == ' ' || c == '\f' || c == '\n' || c == '\r' || c == '\t' || c == '\v';
        };
        return std::all_of( Input.begin(), Input.end(), SpaceChar )
                   ? StrView{ Input.end(), 0 }
                   : StrView{ std::find_if_not( Input.begin(), Input.end(), SpaceChar ),
                              std::find_if_not( Input.rbegin(), Input.rend(), SpaceChar ).base() };
    }

    constexpr auto After( StrView Pattern )
    {
        return [=]( StrView Input ) -> StrView { return { Search( Pattern ).In( Input ).end(), Input.end() }; };
    }

    constexpr auto Before( StrView Pattern )
    {
        return [=]( StrView Input ) -> StrView {
            auto Match = Search( Pattern ).In( Input );
            if( Match.empty() )
                return { Input.end(), 0 };
            else
                return { Input.begin(), Match.begin() };
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
                auto Count = std::size_t{ Input.ends_with( Text ) };
                while( ! ( Input |= After( Text ) ).empty() ) ++Count;
                return Count;
            }

            constexpr auto operator()( StrView Input ) const { return In( Input ); }
        };
        return Impl{ Pattern };
    }

    constexpr auto Split( StrView Pattern )
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
            };

            constexpr auto By( const char Delimiter ) const { return InternalRange{ Text, Delimiter }; }
        };

        return Impl{ Pattern };
    }

    constexpr auto SplitBy( char Delimiter )
    {
        return [=]( StrView Pattern ) { return Split( Pattern ).By( Delimiter ); };
    }

    template<std::integral INT>
    constexpr auto StrViewTo( StrView Source ) -> std::optional<INT>
    {
        Source |= TrimSpace;
        auto ConversionResult = INT{};
        auto [ptr, err] = std::from_chars( Source.data(), Source.data() + Source.size(), ConversionResult );
        if( err == std::errc{} ) return ConversionResult;
        return std::nullopt;
    }

    namespace Lagacy
    {

        constexpr auto After( StrView Pattern )
        {
            struct Impl : StrViewUnit
            {
                constexpr auto operator()( StrView Input ) const -> StrView
                {
                    return { Search( Text ).In( Input ).end(), Input.end() };

                    auto Match = Search( Text ).In( Input );
                    if( Match.begin() == Input.end() )
                        return { Input.end(), 0 };
                    else
                        return { Match.end(), Input.end() };
                }
            };
            return Impl{ Pattern };
            // or just use lambda?
        }

        constexpr auto Before( StrView Pattern )
        {
            struct Impl : StrViewUnit
            {
                constexpr auto operator()( StrView Input ) const -> StrView
                {
                    //if( Text.empty() ) return Input;
                    auto Match = Search( Text ).In( Input );
                    if( Match.begin() == Input.end() )
                        return { Input.end(), 0 };
                    else
                        return { Input.begin(), Match.begin() };
                }
            };
            return Impl{ Pattern };
        }

        constexpr auto Between( StrView LeftBound, StrView RightBound )
        {
            struct Impl : StrViewPair
            {
                using StrViewPair::StrViewPair;
                constexpr auto operator()( StrView Input ) const { return Input | After( Left ) | Before( Right ); }
            };

            return Impl{ LeftBound, RightBound };
        }

        struct Split_Eager : StrViewUnit
        {
            auto By( const char Delimiter ) const
            {
                auto RangeBegin = Text.begin();
                auto RangeEnd = Text.end();

                auto Result = std::vector<StrView>{};
                Result.reserve( static_cast<std::size_t>( std::count( RangeBegin, RangeEnd, Delimiter ) + 1 ) );

                while( RangeBegin != RangeEnd )
                {
                    auto DelimiterPos = std::find( RangeBegin, RangeEnd, Delimiter );
                    Result.emplace_back( RangeBegin, DelimiterPos );
                    if( DelimiterPos == RangeEnd )
                        RangeBegin = RangeEnd;
                    else
                        RangeBegin = DelimiterPos + 1;
                }

                return Result;
            }
        };

        struct Split_Lazy : StrViewUnit
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
                constexpr auto operator++()
                {
                    auto DelimiterPos = std::find( Text.begin(), Text.end(), Delimiter );
                    if( DelimiterPos == Text.end() )
                        Text.remove_suffix( Text.length() );
                    else
                        Text = StrView{ DelimiterPos + 1, Text.end() };
                    return *this;
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
            };

            constexpr auto By( const char Delimiter ) const { return InternalRange{ Text, Delimiter }; }
        };

        struct Eager
        {
            using Split = Split_Eager;
        };

        struct Lazy
        {
            using Split = Split_Lazy;
        };

        struct Split
        {
            using Eager = Split_Eager;
            using Lazy = Split_Lazy;
        };

    }  // namespace Lagacy

    template<size_t N>
    constexpr auto Bundle = std::in_place_index<N>;

    template<size_t N>
    constexpr auto operator|( const auto& Container, std::in_place_index_t<N> )
    {
        // return [&]<size_t... Is>( std::index_sequence<Is...> ) {return std::array{ Container[Is]... };}( std::make_index_sequence<N>() );
        using ValueType = std::decay_t<decltype( *Container.begin() )>;
        auto Result = std::array<ValueType, N>{};
        auto It = Container.begin();
        for( auto i = size_t{ 0 }; i < N; ++i ) Result[i] = *It++;
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

                constexpr auto begin() const
                {
                    return InternalItor{ BaseRange | After( LeftDelimiter ), LeftDelimiter, RightDelimiter };
                }
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

        friend auto operator|( auto SourceRange, const DropIf& Adaptor )
        {
            return InternalRange{ SourceRange, Adaptor.DropCondition };
        }
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

        friend auto operator|( auto SourceRange, const Take& Adaptor )
        {
            return InternalRange{ SourceRange, Adaptor.N };
        }
    };

    inline constexpr auto operator+( const std::string& LHS, const StrView& RHS ) -> std::string  //
    {
        return LHS + std::string{ RHS };
    }

    inline constexpr auto operator+( const StrView& LHS, const auto& RHS ) -> std::string  //
    {
        return std::string{ LHS } + RHS;
    }

    template<std::size_t N, typename CharT = char>
    struct FixedString
    {
        CharT Data[N];
        constexpr FixedString( const CharT ( &Src )[N] ) noexcept { std::copy_n( Src, N, Data ); }
        constexpr operator std::basic_string_view<CharT>() const noexcept { return { Data }; }
    };

    template<FixedString FSTR>
    struct FMT
    {
        constexpr static auto operator()( auto&&... args )
        {
            return std::format( FSTR, std::forward<decltype( args )>( args )... );
        }
    };

    template<FixedString FSTR>
    inline constexpr auto operator""_FMT() noexcept
    {
        return FMT<FSTR>{};
    }

}  // namespace EasyString

using EasyString::operator""_FMT;
using EasyString::operator+;  // NOLINT

#endif