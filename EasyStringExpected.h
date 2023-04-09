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
#include <expected>
#include <optional>

namespace EasyString
{
    enum class UnexpectedCondition { NotFound, EmptyInput, EmptyTarget, BeforeBegin, ReachingEnd };
    namespace Unexpected
    {
        constexpr static auto NotFound = std::unexpected{ UnexpectedCondition::NotFound };
        constexpr static auto EmptyInput = std::unexpected{ UnexpectedCondition::EmptyInput };
        constexpr static auto EmptyTarget = std::unexpected{ UnexpectedCondition::EmptyTarget };
        constexpr static auto BeforeBegin = std::unexpected{ UnexpectedCondition::BeforeBegin };
        constexpr static auto ReachingEnd = std::unexpected{ UnexpectedCondition::ReachingEnd };
    };  // namespace Unexpected

    using StrView = std::string_view;
    using ExStrView = std::expected<StrView, UnexpectedCondition>;

    template<std::same_as<ExStrView>... T>
    [[nodiscard]] constexpr auto ReportUnexpected( const T&... Expectation ) -> std::optional<ExStrView>
    {
        for( auto&& Ex : { Expectation... } )
            if( ! Ex ) return Ex;
        return std::nullopt;
    }

//#define PropagateUnexpected( ... ) if( auto U = ReportUnexpected( __VA_ARGS__ ) ) return std::unexpected( U->error() );
#define PropagateUnexpected( ... )     \
    for( auto&& Ex : { __VA_ARGS__ } ) \
        if( ! Ex ) return std::unexpected( Ex.error() )

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

    constexpr auto Write( ExStrView Text )
    {
        struct Impl : ExStrViewUnit
        {
            auto To( ExStrView Location ) const
            {
                if( Text.has_value() && Location.has_value() )
                    if( auto File = std::ofstream( Location->data(), std::ofstream::trunc ); File.is_open() )
                        std::ranges::copy( Text.value(), std::ostreambuf_iterator( File ) );
            }
        };
        return Impl{ Text };
    }

    constexpr auto Search( ExStrView Text )
    {
        struct Impl : ExStrViewUnit
        {
            constexpr auto Search_impl( ExStrView Input ) const
            {
                if consteval
                {
                    return std::search( Input->begin(), Input->end(), Text->begin(), Text->end() );
                }
                else
                {
                    return std::search( Input->begin(), Input->end(),  //
                                        std::boyer_moore_searcher( Text->begin(), Text->end() ) );
                }
            }

            constexpr auto In( ExStrView Input ) const -> ExStrView
            {
                PropagateUnexpected( Input, Text );
                if( Text->empty() ) return Unexpected::EmptyTarget;
                if( Input->empty() ) return Unexpected::EmptyInput;
                auto Match = Search_impl( Input );
                if( Match == Input->end() ) return Unexpected::NotFound;
                return StrView{ Match, Text->length() };
            }

            constexpr auto operator()( ExStrView Source ) const { return In( Source ); }
        };
        return Impl{ Text };
    }

    constexpr auto TrimAnyOf( ExStrView ExcludeChars )
    {
        struct Impl : ExStrViewUnit
        {
            constexpr auto From( ExStrView Input ) const -> ExStrView
            {
                PropagateUnexpected( Input );
                if( Input->empty() ) return Unexpected::EmptyInput;

                if( ! Text.has_value() ) return Input;

                if( auto pos = Input->find_first_not_of( Text.value() ); pos != Input->npos )
                    Input->remove_prefix( pos );
                else
                    Input->remove_prefix( Input->length() );

                if( auto pos = Input->find_last_not_of( Text.value() ); pos != Input->npos )
                    Input->remove_suffix( Input->length() - pos - 1 );

                if( Input->empty() ) return Unexpected::ReachingEnd;

                return Input;
            }

            constexpr auto operator()( ExStrView Source ) const { return From( Source ); };
        };
        return Impl{ ExcludeChars };
    }

    constexpr auto Trim( ExStrView ExcludePhase )
    {
        struct Impl : ExStrViewUnit
        {
            constexpr auto From( ExStrView Input ) const -> ExStrView
            {
                PropagateUnexpected( Input );
                if( Input->empty() ) return Unexpected::EmptyInput;

                if( ! Text.has_value() ) return Input;

                while( Input->starts_with( Text.value() ) ) Input->remove_prefix( Text->length() );
                while( Input->ends_with( Text.value() ) ) Input->remove_suffix( Text->length() );

                if( Input->empty() ) return Unexpected::ReachingEnd;

                return Input;
            }

            constexpr auto operator()( ExStrView Source ) const { return From( Source ); };
        };
        return Impl{ ExcludePhase };
    }

    constexpr auto TrimSpace( ExStrView Input ) { return TrimAnyOf( " \f\n\r\t\v" ).From( Input ); }

    [[deprecated]] constexpr auto TrimSpace_( ExStrView Input ) -> ExStrView
    {
        PropagateUnexpected( Input );

        constexpr auto SpaceChar = []( char c ) {
            return c == ' ' || c == '\f' || c == '\n' || c == '\r' || c == '\t' || c == '\v';
        };

        if( std::ranges::all_of( Input.value(), SpaceChar ) ) return Unexpected::ReachingEnd;

        return StrView{ std::find_if_not( Input->begin(), Input->end(), SpaceChar ),
                        std::find_if_not( Input->rbegin(), Input->rend(), SpaceChar ).base() };
    }

    constexpr auto After( ExStrView Text )
    {
        struct Impl : ExStrViewUnit
        {
            constexpr auto operator()( ExStrView Input ) const -> ExStrView
            {
                PropagateUnexpected( Input, Text );
                if( Input->empty() ) return Unexpected::EmptyInput;
                if( Text->empty() ) return Unexpected::EmptyTarget;

                auto Match = Search( Text ).In( Input );
                if( ! Match ) return Unexpected::NotFound;

                if( Match->end() == Input->end() ) return Unexpected::ReachingEnd;

                return StrView{ Match->end(), Input->end() };
            }
        };
        return Impl{ Text };
        // or just use lambda?
    }

    constexpr auto Before( ExStrView Text )
    {
        struct Impl : ExStrViewUnit
        {
            constexpr auto operator()( ExStrView Input ) const -> ExStrView
            {
                PropagateUnexpected( Input, Text );
                if( Input->empty() ) return Unexpected::EmptyInput;
                if( Text->empty() ) return Unexpected::EmptyTarget;

                auto Match = Search( Text ).In( Input );
                if( ! Match ) return Unexpected::NotFound;

                if( Match->begin() == Input->begin() ) return Unexpected::BeforeBegin;

                return StrView{ Input->begin(), Match->begin() };
            }
        };
        return Impl{ Text };
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
                while( Input->contains( Text.value() ) )
                {
                    ++Count;
                    Input |= After( Text );
                }
                return Count;
            }

            constexpr auto operator()( ExStrView Input ) const { return In( Input ); }
        };
        return Impl{ Pattern };
    }

    // struct Split_
    // {
    //     ExStrView BaseRange;

    //     auto By( const char Delimiter ) const
    //     {
    //         auto RangeBegin = BaseRange.begin();
    //         auto RangeEnd = BaseRange.end();

    //         auto Result = std::vector<StrView>{};
    //         Result.reserve( static_cast<std::size_t>( std::count( RangeBegin, RangeEnd, Delimiter ) + 1 ) );

    //         while( RangeBegin != RangeEnd )
    //         {
    //             auto DelimiterPos = std::find( RangeBegin, RangeEnd, Delimiter );
    //             Result.emplace_back( RangeBegin, DelimiterPos );
    //             if( DelimiterPos == RangeEnd )
    //                 RangeBegin = RangeEnd;
    //             else
    //                 RangeBegin = DelimiterPos + 1;
    //         }

    //         return Result;
    //     }
    // };

    // struct Split
    // {
    //     ExStrView BaseRange;

    //     struct InternalItorSentinel
    //     {};

    //     struct InternalItor
    //     {
    //         ExStrView BaseRange;
    //         const char Delimiter;

    //         auto operator*()
    //         {
    //             auto DelimiterPos = std::find( BaseRange.begin(), BaseRange.end(), Delimiter );
    //             return ExStrView{ BaseRange.begin(), DelimiterPos };
    //         }

    //         auto operator!=( InternalItorSentinel ) { return ! BaseRange.empty(); }
    //         auto operator++()
    //         {
    //             auto DelimiterPos = std::find( BaseRange.begin(), BaseRange.end(), Delimiter );
    //             if( DelimiterPos == BaseRange.end() )
    //                 BaseRange.remove_suffix( BaseRange.length() );
    //             else
    //                 BaseRange = ExStrView{ DelimiterPos + 1, BaseRange.end() };
    //         }
    //     };

    //     struct InternalRange
    //     {
    //         ExStrView BaseRange;
    //         const char Delimiter;

    //         auto begin() { return InternalItor{ BaseRange, Delimiter }; }
    //         auto end() { return InternalItorSentinel{}; }
    //         auto size()
    //         {
    //             return ! BaseRange.ends_with( Delimiter )  // ending delim adjustment
    //                    + std::count( BaseRange.begin(), BaseRange.end(), Delimiter );
    //         }
    //     };

    //     auto By( const char Delimiter ) const { return InternalRange{ BaseRange, Delimiter }; }
    // };

    // struct SplitBetween
    // {
    //     ExStrView LeftDelimiter, RightDelimiter;

    //     struct InternalItorSentinel
    //     {};

    //     struct InternalItor
    //     {
    //         ExStrView BaseRange, LeftDelimiter, RightDelimiter;

    //         auto operator*() { return BaseRange | Between( "", RightDelimiter ); }
    //         auto operator!=( InternalItorSentinel ) { return ! BaseRange.empty(); }
    //         auto operator++()
    //         {
    //             BaseRange |= After( RightDelimiter );
    //             BaseRange |= After( LeftDelimiter );
    //             if( ! BaseRange.contains( RightDelimiter ) ) BaseRange.remove_prefix( BaseRange.length() );
    //         }
    //     };

    //     struct InternalRange
    //     {
    //         ExStrView BaseRange, LeftDelimiter, RightDelimiter;

    //         auto begin() { return InternalItor{ BaseRange | After( LeftDelimiter ), LeftDelimiter, RightDelimiter }; }
    //         auto end() { return InternalItorSentinel{}; }
    //     };

    //     friend auto operator|( ExStrView Source, const SplitBetween& Adaptor )
    //     {
    //         return InternalRange{ Source, Adaptor.LeftDelimiter, Adaptor.RightDelimiter };
    //     }
    // };

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

    constexpr auto operator+( const StrView& LHS, const auto& RHS ) -> std::string  //
    {
        return std::string{ LHS } + RHS;
    }

}  // namespace EasyString

using EasyString::operator+;  // NOLINT

#endif