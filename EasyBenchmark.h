// usage:
// for( auto _ : Benchmark("Title") ) ...

#ifndef EASYBENCHMARK_H
#define EASYBENCHMARK_H

#include <immintrin.h>  //__rdtsc

#include <chrono>
#include <iomanip>
#include <ios>
#include <iostream>
#include <format>
#include <string>
#include <string_view>
#include <vector>
#include <algorithm>
#include <ranges>

namespace EasyBenchmark
{
    // helpers
    namespace
    {
        inline auto GetTimeMark() noexcept { return __rdtsc(); }
    }

    constexpr auto TickPerSecond = 1'000'000'000;

    struct BenchmarkResult
    {
        std::string Title;
        std::size_t TotalTick;
        std::size_t TotalIteration;
        auto Latency() const noexcept { return TotalTick / TotalIteration; }
        auto Throughput() const noexcept { return TickPerSecond * TotalIteration / TotalTick; }
        auto TitleLength() const noexcept { return Title.length(); }
    };

    struct BenchmarkResultAnalyzer : std::vector<BenchmarkResult>
    {
        std::size_t BaselinePos;
        BenchmarkResultAnalyzer() : std::vector<BenchmarkResult>{}, BaselinePos{ 0 } { reserve( 10 ); }

        ~BenchmarkResultAnalyzer()
        {
            const auto DigitWidth = 18uz;
            const auto TitleWidth =
                std::max( std::ranges::max_element( *this, {}, &BenchmarkResult::TitleLength )->Title.length(), 24uz );
            const auto ThroughputBaseline = static_cast<double>( ( *this )[BaselinePos].Throughput() );

            auto PrintRow = [TW = std::setw( TitleWidth ), DW = std::setw( DigitWidth )](  //
                                std::string_view Title,                                    //
                                const auto Latency,                                        //
                                const auto Throughput,                                     //
                                const auto Relative,                                       //
                                const char fill = ' ' )                                    //
            {
                // std::cout << std::setfill( fill ) << std::left                       //
                //           << TW << Title << std::right                               //
                //           << DW << Latency                                           //
                //           << DW << Throughput                                        //
                //           << DW << std::setprecision( 2 ) << std::fixed << Relative  //
                //           << std::setfill( ' ' ) << '\n';
            };

            auto PrintLine = [PrintRow] {
                PrintRow( "", "", "", "", '_' );
                std::print("\n");
            };

            std::print(
                "\n    ______________________"
                "\n   /                     /"
                "\n  /  Benchmark Summary  /\n" );
            PrintRow( " /_____________________/", "Latency", "Throughput", "Relative" );
            PrintLine();
            for( auto&& Result : *this )
                PrintRow( Result.Title,         //
                          Result.Latency(),     //
                          Result.Throughput(),  //
                          Result.Throughput() / ThroughputBaseline );
            PrintLine();
        }
    };

    inline static auto BenchmarkResults = BenchmarkResultAnalyzer{};

    struct BenchmarkExecutor
    {
        using clock = std::chrono::steady_clock;
        using time_point = std::chrono::time_point<clock>;

        constexpr static auto MaxDuration = std::chrono::milliseconds{ 3000 };
        constexpr static auto MaxIteration = std::size_t{ 12345 };

        BenchmarkResult& Result;

        struct Sentinel
        {};

        template<typename BaseRange>
        struct Iterator
        {
            BaseRange& Base;
            time_point EndTime;
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
        return BenchmarkExecutor{ BenchmarkResults.back() };
    }  // namespace

}  // namespace EasyBenchmark

using EasyBenchmark::Benchmark;  // NOLINT

#endif /* BENCHMARK_H */