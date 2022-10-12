// usage:
// for( auto _ : Benchmark("Title") ) ...

#ifndef EASYBENCHMARK_H
#define EASYBENCHMARK_H

#include <immintrin.h>  //__rdtsc

#include <chrono>
#include <iomanip>
#include <ios>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>
#include <algorithm>

namespace
{
    struct BenchmarkResult
    {
        std::string Title;
        std::size_t TotalCycle;
        std::size_t TotalIteration;
        auto        Latency() const { return TotalCycle / TotalIteration; }
        auto        Throughput() const { return 1000000000 * TotalIteration / TotalCycle; }
        auto        operator<( const auto& RHS ) const { return Title.length() < RHS.Title.length(); }
    };

    struct BenchmarkAnalyzer : std::vector<BenchmarkResult>
    {
        std::size_t BaselinePos;
        BenchmarkAnalyzer() : std::vector<BenchmarkResult>{}, BaselinePos{ 0 } { reserve( 10 ); }

        ~BenchmarkAnalyzer()
        {
            const auto DigitWidth = 18;
            const auto TitleWidth = std::max( std::max_element( begin(), end() )->Title.length(),  //
                                              24ull );
            const auto ThroughputBaseline = static_cast<double>( ( *this )[BaselinePos].Throughput() );

            auto cout_row = [TW = std::setw( TitleWidth ), DW = std::setw( DigitWidth )](  //
                                std::string_view Title,                                    //
                                const auto       Latency,                                  //
                                const auto       Throughput,                               //
                                const auto       Relative,                                 //
                                const char       fill = ' ' )                                    //
            {
                std::cout << std::setfill( fill ) << std::left                       //
                          << TW << Title << std::right                               //
                          << DW << Latency                                           //
                          << DW << Throughput                                        //
                          << DW << std::setprecision( 2 ) << std::fixed << Relative  //
                          << std::setfill( ' ' ) << '\n';
            };

            auto cout_line = [cout_row] {
                cout_row( "", "", "", "", '_' );
                std::cout << '\n';
            };

            std::cout << "\n    ______________________"
                         "\n   /                     /"
                         "\n  /  Benchmark Summary  /\n";
            /**/ cout_row( " /_____________________/", "Latency", "Throughput", "Relative" );
            cout_line();
            for( auto&& Result : *this )
                cout_row( Result.Title,         //
                          Result.Latency(),     //
                          Result.Throughput(),  //
                          Result.Throughput() / ThroughputBaseline );
            cout_line();
        }
    };

    inline static auto BenchmarkResults = BenchmarkAnalyzer{};

    struct BenchmarkContainer
    {
        using clock = std::chrono::steady_clock;
        using time_point = std::chrono::time_point<clock>;

        constexpr static auto MaxDuration = std::chrono::milliseconds{ 2000 };
        constexpr static auto MaxIteration = std::size_t{ 12345 };

        BenchmarkResult& Result;

        struct Sentinel
        {};

        template<typename BaseRange>
        struct Iterator
        {
            BaseRange&  Base;
            time_point  EndTime;
            std::size_t RemainIteration;
            std::size_t StartCycle;

            auto operator*() { return 0; }
            auto operator++() { --RemainIteration; }
            auto operator!=( Sentinel ) { return RemainIteration > 0 && clock::now() < EndTime; }

            Iterator( BaseRange& Base_ )
                : Base{ Base_ },                                     //
                  EndTime{ clock::now() + BaseRange::MaxDuration },  //
                  RemainIteration{ BaseRange::MaxIteration },        //
                  StartCycle{ __rdtsc() }
            {}

            ~Iterator()
            {
                Base.Result.TotalCycle = __rdtsc() - StartCycle;
                Base.Result.TotalIteration = BaseRange::MaxIteration - RemainIteration;
            }
        };

        auto begin() { return Iterator{ *this }; }
        auto end() { return Sentinel{}; }
    };

    inline auto Benchmark( std::string&& BenchmarkTitle )
    {
        std::cout << "Benchmarking... " << BenchmarkTitle << "\n";
        BenchmarkResults.push_back( { " " + BenchmarkTitle, 0, 0 } );
        return BenchmarkContainer{ BenchmarkResults.back() };
    }
}  // namespace

#endif /* BENCHMARK_H */


#ifdef TEST_CODE

#include <vector>

struct BigS
{
    int data[999];
};

int main()
{
    for( auto _ : Benchmark( "Without Reserve" ) )
    {
        auto v = std::vector<BigS>{};
        for( int i{ 0 }; i < 20; ++i ) v.push_back( BigS{} );
    }

    for( auto _ : Benchmark( "With Reserve" ) )
    {
        auto v = std::vector<BigS>{};
        v.reserve( 20 );
        for( int i{ 0 }; i < 20; ++i ) v.push_back( BigS{} );
    }
    for( auto _ : Benchmark( "volatile increment" ) )
    {
        for( volatile int i = 0; i < 100; ++i )
        {}
    }
    for( auto _ : Benchmark( "Random Stuff" ) ) [[maybe_unused]] int a = 0;
    for( auto _ : Benchmark( "What if I have an extremely long title ?" ) )
    {}
}

#endif