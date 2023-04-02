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
    using StrView = std::string_view;

    inline namespace Concepts
    {
        template<typename AdaptorType>
        concept StrViewAdaptable = requires( AdaptorType Adaptor, StrView SV ) { Adaptor( SV ); };
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
        constexpr StrViewUnit( StrView Source ) noexcept : Text{ Source } {}

      protected:
        StrView Text;
    };

    struct StrViewPair
    {
        constexpr StrViewPair( StrView LeftSource, StrView RightSource ) noexcept
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
            constexpr auto In( StrView Input ) const
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

            constexpr auto operator()( StrView Source ) const { return In( Source ); }
        };
        return Impl{ Pattern };
    }

    consteval auto Trim( StrView ExcludeChars )
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
                auto NewStart = Search( Text ).In( Input );
                if( NewStart == Input.end() ) return {};
                return { NewStart + Text.length(), Input.end() };
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
                if( Text.empty() ) return Input;
                auto NewEnd = Search( Text ).In( Input );
                if( NewEnd == Input.end() ) return {};
                return { Input.begin(), NewEnd };
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

    struct Split_
    {
        StrView BaseRange;

        auto By( const char Delimiter ) const
        {
            auto RangeBegin = BaseRange.begin();
            auto RangeEnd = BaseRange.end();

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

    struct Split
    {
        StrView BaseRange;

        struct InternalItorSentinel
        {};

        struct InternalItor
        {
            StrView BaseRange;
            const char Delimiter;

            auto operator*()
            {
                auto DelimiterPos = std::find( BaseRange.begin(), BaseRange.end(), Delimiter );
                return StrView{ BaseRange.begin(), DelimiterPos };
            }

            auto operator!=( InternalItorSentinel ) { return ! BaseRange.empty(); }
            auto operator++()
            {
                auto DelimiterPos = std::find( BaseRange.begin(), BaseRange.end(), Delimiter );
                if( DelimiterPos == BaseRange.end() )
                    BaseRange.remove_suffix( BaseRange.length() );
                else
                    BaseRange = StrView{ DelimiterPos + 1, BaseRange.end() };
            }
        };

        struct InternalRange
        {
            StrView BaseRange;
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

    auto operator+( const std::string& LHS, const StrView& RHS ) -> std::string  //
    {
        return LHS + std::string{ RHS };
    }

    auto operator+( const StrView& LHS, const auto& RHS ) -> std::string  //
    {
        return std::string{ LHS } + RHS;
    }

}  // namespace EasyString

using EasyString::operator+;

#endif