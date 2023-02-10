// usage:
// for( auto _ : Benchmark("Title") ) ...

#ifndef EASYBENCHMARK_H
#define EASYBENCHMARK_H

#include <immintrin.h>  //__rdtsc

#include <chrono>
#include <format>
#include <vector>
#include <algorithm>

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
        std::size_t TotalTick{};
        std::size_t TotalIteration{};
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
            constexpr auto Header = std::string_view{ "Benchmark Summary" };
            constexpr auto HeaderSpace = Header.size() + 4;
            constexpr auto HeaderTitleWidth = Header.size() + 9;
            constexpr auto DigitWidth = 16uz;

            std::print(
                "\n    __{0:_^{2}}_"  //
                "\n   / {0: ^{2}} /"  //
                "\n  / {1: ^{2}} /"   //
                "\n /_{0:_^{2}}_/\n",
                "", Header, HeaderSpace );

            const auto TitleWidth =
                std::max( std::ranges::max_element( *this, {}, &BenchmarkResult::TitleLength )->Title.length(),
                          HeaderTitleWidth );
            const auto ThroughputBaseline = static_cast<double>( ( *this )[BaselinePos].Throughput() );

            auto PrintRow = [=]( std::string_view Title,  //
                                 const auto Latency,      //
                                 const auto Throughput,   //
                                 const auto Relative )    //
            {
                std::print( " {0:{1}}{2:>{5}}{3:>{5}}{4:>{5}.2}\n",  //
                            Title, TitleWidth, Latency, Throughput, Relative, DigitWidth );
            };

            auto PrintLine = [=] { std::print( "_{:_<{}}\n", "", TitleWidth + DigitWidth * 3 ); };

            PrintRow( "", "Latency", "Throughput", "Relative" );
            PrintLine();
            for( auto&& Result : *this )
                PrintRow( Result.Title, Result.Latency(), Result.Throughput(),
                          static_cast<double>( Result.Throughput() ) / ThroughputBaseline );
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
                  StartCycle{ GetTimeMark() }
            {}

            ~Iterator()
            {
                Base.Result.TotalTick = GetTimeMark() - StartCycle;
                Base.Result.TotalIteration = BaseRange::MaxIteration - RemainIteration;
            }
        };

        auto begin() { return Iterator{ *this }; }
        auto end() { return Sentinel{}; }
    };

    inline struct BenchmarkBaseLine_t
    {
    } AsBaseLine;

    inline auto Benchmark( std::string BenchmarkTitle )
    {
        std::print( "Benchmarking... {}\n", BenchmarkTitle );
        BenchmarkResults.push_back( { std::move( BenchmarkTitle ) } );
        return BenchmarkExecutor{ BenchmarkResults.back() };
    }

    inline auto Benchmark( std::string BenchmarkTitle, BenchmarkBaseLine_t )
    {
        BenchmarkResults.BaselinePos = BenchmarkResults.size();
        return Benchmark( std::move( BenchmarkTitle ) );
    }

}  // namespace EasyBenchmark

using EasyBenchmark::Benchmark;   // NOLINT
using EasyBenchmark::AsBaseLine;  // NOLINT
#endif                            /* BENCHMARK_H */