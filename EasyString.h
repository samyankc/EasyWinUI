#ifndef EASYSTRING_H
#define EASYSTRING_H

#include <algorithm>
#include <cstring>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>
#include <functional>

namespace EasyString
{
    using StrView = std::string_view;

    inline namespace Concepts
    {
        template<typename AdaptorType>
        concept StrViewAdaptable = requires( AdaptorType Adaptor, StrView SV ) { Adaptor( SV ); };
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
        constexpr StrViewUnit( std::convertible_to<StrView> auto Source ) noexcept : Text{ Source } {}

      protected:
        StrView Text;
    };

    struct StrViewPair
    {
        constexpr StrViewPair( std::convertible_to<StrView> auto LeftSource,
                               std::convertible_to<StrView> auto RightSource ) noexcept
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

    constexpr auto TrimSpace( StrView Input )
    {
        constexpr auto SpaceChar = []( char c ) {
            return c == ' ' || c == '\f' || c == '\n' || c == '\r' || c == '\t' || c == '\v';
        };
        return std::all_of( Input.begin(), Input.end(), SpaceChar )
                   ? StrView{}
                   : StrView{ std::find_if_not( Input.begin(), Input.end(), SpaceChar ),
                              std::find_if_not( Input.rbegin(), Input.rend(), SpaceChar ).base() };
    }

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

    constexpr auto Count( StrView Pattern )
    {
        struct Impl : StrViewUnit
        {
            constexpr auto In( StrView Input ) const
            {
                auto Count = 0uz;
                while( Input.contains( Text ) )
                {
                    ++Count;
                    Input |= After( Text );
                }
                return Count;

                // auto SearchNext = Search( Text );
                // auto Count = -1;
                // for( auto NewFound = Input.begin();  //
                //      NewFound != Input.end();        //
                //      NewFound = SearchNext.In( Input ) )
                // {
                //     ++Count;
                //     Input = { NewFound + Text.length(), Input.end() };
                // }
                // return Count;
            }

            constexpr auto operator()( StrView Input ) const { return In( Input ); }
        };
        return Impl{ Pattern };
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

    template<size_t N>
    constexpr auto Bundle = std::in_place_index<N>;

    template<size_t N>
    auto operator|( const auto& Container, std::in_place_index_t<N> )
    {
        return [&]<size_t... Is>( std::index_sequence<Is...> ) {
            return std::array{ Container[Is]... };
        }( std::make_index_sequence<N>() );
    }

    struct SplitBetween
    {
        StrView LeftDelimiter, RightDelimiter;

        struct InternalItorSentinel
        {};

        struct InternalItor
        {
            StrView BaseRange, LeftDelimiter, RightDelimiter;

            auto operator*() { return BaseRange | Between( "", RightDelimiter ); }
            auto operator!=( InternalItorSentinel ) { return ! BaseRange.empty(); }
            auto operator++()
            {
                BaseRange |= After( RightDelimiter );
                BaseRange |= After( LeftDelimiter );
                if( ! BaseRange.contains( RightDelimiter ) ) BaseRange.remove_prefix( BaseRange.length() );
            }
        };

        struct InternalRange
        {
            StrView BaseRange, LeftDelimiter, RightDelimiter;

            auto begin() { return InternalItor{ BaseRange | After( LeftDelimiter ), LeftDelimiter, RightDelimiter }; }
            auto end() { return InternalItorSentinel{}; }
        };

        friend auto operator|( StrView Source, const SplitBetween& Adaptor )
        {
            return InternalRange{ Source, Adaptor.LeftDelimiter, Adaptor.RightDelimiter };
        }
    };

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

}  // namespace EasyString

using EasyString::operator+;  // NOLINT

#endif