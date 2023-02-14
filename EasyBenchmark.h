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
        inline auto CurrentTimeMark() noexcept { return __rdtsc(); }
    }

    struct BenchmarkResult
    {
        constexpr static auto TickPerSecond = 1'000'000'000;
        std::string Title;
        std::size_t TotalTick{};
        std::size_t TotalIteration{};
        auto TitleLength() const noexcept { return Title.length(); }
        auto Latency() const noexcept { return TotalTick / TotalIteration; }
        auto Throughput() const noexcept { return TickPerSecond * TotalIteration / TotalTick; }
    };

    struct BenchmarkResultAnalyzer
    {
        std::vector<BenchmarkResult> Samples;
        BenchmarkResult* BaselineResultPtr;
        BenchmarkResultAnalyzer() : Samples{}, BaselineResultPtr{ nullptr } { Samples.reserve( 10 ); }

        auto PresentResult()
        {
            constexpr auto Header = std::string_view{ "Benchmark Summary" };
            constexpr auto HeaderSpace = Header.size() + 4;
            constexpr auto HeaderTitleWidth = Header.size() + 7;
            constexpr auto DigitWidth = 14uz;

            std::print(
                "\n    __{0:_^{2}}_"  //
                "\n   / {0: ^{2}} /"  //
                "\n  / {1: ^{2}} /"   //
                "\n /_{0:_^{2}}_/\n",
                "", Header, HeaderSpace );

            const auto TitleWidth =
                std::max( std::ranges::max_element( Samples, {}, &BenchmarkResult::TitleLength )->Title.length(),
                          HeaderTitleWidth );
            const auto ThroughputBaseline = static_cast<double>( BaselineResultPtr ? BaselineResultPtr->Throughput()
                                                                                   : Samples.front().Throughput() );

            auto PrintLine = [=] { std::print( "_{:_<{}}\n", "", TitleWidth + DigitWidth * 3 ); };
            auto PrintRow = [=]( std::string_view Title,  //
                                 const auto Latency,      //
                                 const auto Throughput,   //
                                 const auto Relative )    //
            {
                std::print( std::is_floating_point_v<decltype( Relative )>  //
                                ? " {0:{1}}{2:>{5}}{3:>{5}}{4:>{5}.2f}\n"
                                : " {0:{1}}{2:>{5}}{3:>{5}}{4:>{5}}\n",
                            Title, TitleWidth, Latency, Throughput, Relative, DigitWidth );
            };

            PrintRow( "", "Latency", "Throughput", "Relative" );
            PrintLine();
            for( auto&& Result : Samples )
                PrintRow( Result.Title, Result.Latency(), Result.Throughput(),
                          static_cast<double>( Result.Throughput() ) / ThroughputBaseline );
            PrintLine();
        }

        ~BenchmarkResultAnalyzer()
        {
            if( Samples.empty() ) return;
            PresentResult();
        }
    };

    inline static auto BenchmarkResults = BenchmarkResultAnalyzer{};

    using namespace std::chrono_literals;
    struct BenchmarkExecutor
    {
        using clock = std::chrono::steady_clock;
        using time_point = clock::time_point;

        constexpr static auto MaxDuration = 3s;
        constexpr static auto MaxIteration = 10000uz;

        BenchmarkResult& Result;

        struct [[maybe_unused]] UnusedIdentifier
        {};

        struct Sentinel
        {};

        struct Iterator
        {
            using BaseRange = BenchmarkExecutor;
            BaseRange& Base;
            time_point EndTime;
            std::size_t RemainIteration;
            std::size_t StartTimeMark;

            auto operator*() { return UnusedIdentifier{}; }
            //auto operator*() { return nullptr; }
            auto operator++() { --RemainIteration; }
            auto operator!=( Sentinel ) { return RemainIteration > 0 && clock::now() < EndTime; }

            Iterator( BaseRange& Base_ )
                : Base{ Base_ },                                     //
                  EndTime{ clock::now() + BaseRange::MaxDuration },  //
                  RemainIteration{ BaseRange::MaxIteration },        //
                  StartTimeMark{ CurrentTimeMark() }
            {}

            ~Iterator()
            {
                Base.Result.TotalTick = CurrentTimeMark() - StartTimeMark;
                Base.Result.TotalIteration = BaseRange::MaxIteration - RemainIteration;
            }
        };

        auto begin() { return Iterator{ *this }; }
        auto end() { return Sentinel{}; }

        auto AsBaseLine()
        {
            BenchmarkResults.BaselineResultPtr = &Result;
            return *this;
        }
    };

    inline auto Benchmark( std::string BenchmarkTitle )
    {
        std::print( "Benchmarking... {}\n", BenchmarkTitle );
        BenchmarkResults.Samples.push_back( { std::move( BenchmarkTitle ) } );
        return BenchmarkExecutor{ BenchmarkResults.Samples.back() };
    }

}  // namespace EasyBenchmark

using EasyBenchmark::Benchmark;  // NOLINT
#endif                           /* BENCHMARK_H */