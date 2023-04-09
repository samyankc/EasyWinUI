#ifndef EASYSTRING_H
#define EASYSTRING_H

#include <algorithm>
#include <cstring>
#include <fstream>
#include <initializer_list>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>
#include <functional>

namespace EasyString
{
    using ExStrView = std::string_view;

    inline namespace Concepts
    {
        template<typename AdaptorType>
        concept StrViewAdaptable = requires( AdaptorType Adaptor, ExStrView SV ) { Adaptor( SV ); };
    }

    template<StrViewAdaptable T>
    constexpr decltype( auto ) operator|( ExStrView Input, T&& Adaptor ) noexcept
    {
        return std::forward<T>( Adaptor )( Input );
    }

    template<StrViewAdaptable T>
    constexpr ExStrView& operator|=( ExStrView& Input, T&& Adaptor ) noexcept
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

    struct ExStrViewUnit
    {
        constexpr ExStrViewUnit( ExStrView Source ) noexcept : Text{ Source } {}

      protected:
        ExStrView Text;
    };

    struct ExStrViewPair
    {
        constexpr ExStrViewPair( ExStrView LeftSource, ExStrView RightSource ) noexcept
            : Left{ LeftSource }, Right{ RightSource }
        {}

      protected:
        ExStrView Left, Right;
    };

    constexpr auto Write( ExStrView Content )
    {
        struct Impl : ExStrViewUnit
        {
            auto To( ExStrView Location ) const
            {
                auto File = std::ofstream( Location.data(), std::ofstream::trunc );
                auto Output = std::ostreambuf_iterator( File );
                if( File ) std::ranges::copy( Text, Output );
            }
        };
        return Impl{ Content };
    }

    constexpr auto Search( ExStrView Pattern )
    {
        struct Impl : ExStrViewUnit
        {
            constexpr auto Search_impl( ExStrView Input ) const
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

            constexpr auto In( ExStrView Input ) const -> ExStrView
            {
                auto MatchBegin = Search_impl( Input );
                if( MatchBegin == Input.end() ) return { Input.end(), 0 };
                return { MatchBegin, Text.length() };
            }

            constexpr auto operator()( ExStrView Source ) const { return In( Source ); }
        };
        return Impl{ Pattern };
    }

    consteval auto Trim( ExStrView ExcludeChars )
    {
        struct Impl : ExStrViewUnit
        {
            constexpr auto From( ExStrView Input ) const
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

            constexpr auto operator()( ExStrView Source ) const { return From( Source ); };
        };
        return Impl{ ExcludeChars };
    }

    constexpr auto TrimSpace( ExStrView Input )
    {
        constexpr auto SpaceChar = []( char c ) {
            return c == ' ' || c == '\f' || c == '\n' || c == '\r' || c == '\t' || c == '\v';
        };
        return std::all_of( Input.begin(), Input.end(), SpaceChar )
                   ? ExStrView{}
                   : ExStrView{ std::find_if_not( Input.begin(), Input.end(), SpaceChar ),
                              std::find_if_not( Input.rbegin(), Input.rend(), SpaceChar ).base() };
    }

    constexpr auto After( ExStrView Pattern )
    {
        struct Impl : ExStrViewUnit
        {
            constexpr auto operator()( ExStrView Input ) const -> ExStrView
            {
                auto Match = Search( Text ).In( Input );
                if( Match.begin() == Input.end() ) return { Input.end(), 0 };
                return { Match.end(), Input.end() };
            }
        };
        return Impl{ Pattern };
        // or just use lambda?
    }

    constexpr auto Before( ExStrView Pattern )
    {
        struct Impl : ExStrViewUnit
        {
            constexpr auto operator()( ExStrView Input ) const -> ExStrView
            {
                //if( Text.empty() ) return Input;
                auto Match = Search( Text ).In( Input );
                if( Match.begin() == Input.end() ) return { Input.end(), 0 };
                return { Input.begin(), Match.begin() };
            }
        };
        return Impl{ Pattern };
    }

    constexpr auto Between( ExStrView LeftBound, ExStrView RightBound )
    {
        struct Impl : ExStrViewPair
        {
            using ExStrViewPair::ExStrViewPair;
            constexpr auto operator()( ExStrView Input ) const { return Input | After( Left ) | Before( Right ); }
        };

        return Impl{ LeftBound, RightBound };
    }

    constexpr auto Count( ExStrView Pattern )
    {
        struct Impl : ExStrViewUnit
        {
            constexpr auto In( ExStrView Input ) const
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

            constexpr auto operator()( ExStrView Input ) const { return In( Input ); }
        };
        return Impl{ Pattern };
    }

    struct Split_
    {
        ExStrView BaseRange;

        auto By( const char Delimiter ) const
        {
            auto RangeBegin = BaseRange.begin();
            auto RangeEnd = BaseRange.end();

            auto Result = std::vector<ExStrView>{};
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

    struct Split
    {
        ExStrView BaseRange;

        struct InternalItorSentinel
        {};

        struct InternalItor
        {
            ExStrView BaseRange;
            const char Delimiter;

            auto operator*()
            {
                auto DelimiterPos = std::find( BaseRange.begin(), BaseRange.end(), Delimiter );
                return ExStrView{ BaseRange.begin(), DelimiterPos };
            }

            auto operator!=( InternalItorSentinel ) { return ! BaseRange.empty(); }
            auto operator++()
            {
                auto DelimiterPos = std::find( BaseRange.begin(), BaseRange.end(), Delimiter );
                if( DelimiterPos == BaseRange.end() )
                    BaseRange.remove_suffix( BaseRange.length() );
                else
                    BaseRange = ExStrView{ DelimiterPos + 1, BaseRange.end() };
            }
        };

        struct InternalRange
        {
            ExStrView BaseRange;
            const char Delimiter;

            auto begin() { return InternalItor{ BaseRange, Delimiter }; }
            auto end() { return InternalItorSentinel{}; }
            auto size()
            {
                return ! BaseRange.ends_with( Delimiter )  // ending delim adjustment
                       + std::count( BaseRange.begin(), BaseRange.end(), Delimiter );
            }
        };

        auto By( const char Delimiter ) const { return InternalRange{ BaseRange, Delimiter }; }
    };

    struct SplitBetween
    {
        ExStrView LeftDelimiter, RightDelimiter;

        struct InternalItorSentinel
        {};

        struct InternalItor
        {
            ExStrView BaseRange, LeftDelimiter, RightDelimiter;

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
            ExStrView BaseRange, LeftDelimiter, RightDelimiter;

            auto begin() { return InternalItor{ BaseRange | After( LeftDelimiter ), LeftDelimiter, RightDelimiter }; }
            auto end() { return InternalItorSentinel{}; }
        };

        friend auto operator|( ExStrView Source, const SplitBetween& Adaptor )
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

    inline constexpr auto operator+( const std::string& LHS, const ExStrView& RHS ) -> std::string  //
    {
        return LHS + std::string{ RHS };
    }

    constexpr auto operator+( const ExStrView& LHS, const auto& RHS ) -> std::string  //
    {
        return std::string{ LHS } + RHS;
    }

}  // namespace EasyString

using EasyString::operator+;  // NOLINT

#endif