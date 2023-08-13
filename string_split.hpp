#ifndef H_STRING_SPLIT_
#define H_STRING_SPLIT_

#include <string>
#include <string_view>
#include <algorithm>
#include <vector>
#include <array>

struct Split
{
    std::string_view BaseRange;

    auto By( const char Delimiter ) const
    {
        auto RangeBegin = BaseRange.begin();
        auto RangeEnd = BaseRange.end();

        auto Result = std::vector<std::string_view>{};
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

template<size_t N>
struct Taker
{};

template<size_t N>
constexpr auto Take = Taker<N>{};

template<size_t N>
auto operator|( const auto& Container, Taker<N> )
{
    return [&]<size_t... Is>( std::index_sequence<Is...> ) {
        return std::array{ Container[Is]... };
    }( std::make_index_sequence<N>() );
}

#endif