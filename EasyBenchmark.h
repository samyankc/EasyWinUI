// usage:
// for( auto _ : Benchmark("Title") ) ...

#ifndef EASYBENCHMARK_H
#define EASYBENCHMARK_H

#include <chrono>
#include <format>
#include <vector>
#include <algorithm>

namespace EasyBenchmark
{
    using namespace std::chrono;
    using namespace std::chrono_literals;
    using Clock = high_resolution_clock;
    using TimePoint = Clock::time_point;
    using Duration = Clock::duration;

    //inline auto CurrentTimeMark() noexcept { return __rdtsc(); }

    struct BenchmarkResult
    {
        std::string Title;
        Duration TotalDuration{};
        ssize_t TotalIteration{};
        BenchmarkResult( std::string_view Title ) : Title{ Title } {}
        auto TitleLength() const noexcept { return Title.length(); }
        auto Latency() const noexcept { return TotalDuration.count() / TotalIteration; }
        auto Throughput() const noexcept { return Duration::period::den * TotalIteration / TotalDuration.count(); }
    };

    struct DefaultExecutor
    {
        using ResultContainer = BenchmarkResult;

        inline static auto MaxDuration = 3s;
        inline static auto MaxIteration = 1000z;

        ResultContainer& Result;

        struct [[maybe_unused]] UnusedIdentifier
        {};

        struct Sentinel
        {};

        struct Iterator
        {
            ResultContainer& Result;
            TimePoint BenchmarkEndTime;
            TimePoint StartTimeMark;
            ssize_t RemainIteration;

            auto operator*() { return UnusedIdentifier{}; }
            //auto operator*() { return nullptr; }
            auto operator++() { --RemainIteration; }
            auto operator!=( Sentinel ) { return RemainIteration > 0 && Clock::now() < BenchmarkEndTime; }

            Iterator( ResultContainer& Result_ )
                : Result{ Result_ },                               //
                  BenchmarkEndTime{ Clock::now() + MaxDuration },  //
                  StartTimeMark{ Clock::now() },                   //
                  RemainIteration{ MaxIteration }
            {
                std::print( "Benchmarking... {}\n", Result.Title );
            }

            ~Iterator()
            {
                Result.TotalDuration = Clock::now() - StartTimeMark;
                Result.TotalIteration = MaxIteration - RemainIteration;
            }
        };

        auto begin() { return Iterator{ Result }; }
        auto end() { return Sentinel{}; }

        auto AsBaseLine();  // requires BenchmarkResultAnalyzer to be complete
    };

    struct BenchmarkResultAnalyzer
    {
        std::vector<BenchmarkResult> Samples;
        std::size_t BaselinePos;
        BenchmarkResultAnalyzer() : Samples{}, BaselinePos{ 0uz } { Samples.reserve( 10 ); }

        ~BenchmarkResultAnalyzer()
        {
            if( Samples.empty() ) return;

            constexpr auto Header = std::string_view{ "Benchmark Summary" };
            constexpr auto HeaderSpace = Header.size() + 6;
            constexpr auto DigitWidth = 14uz;

            const auto TitleWidth = std::max(
                std::ranges::max_element( Samples, {}, &BenchmarkResult::TitleLength )->TitleLength(), HeaderSpace );
            const auto ThroughputBaseline = Samples[BaselinePos].Throughput();

            auto PrintLine = [=] { std::print( ">{:─<{}}<\n", "", TitleWidth + DigitWidth * 3 + 4 ); };
            auto PrintRow = [=]( std::string_view Title,                                           //
                                 const auto Latency, const auto Throughput, const auto Relative )  //
            {
                std::print( "   {0:{1}}{2:>{5}}{3:>{5}}{4:>{5}}\n",  //
                            Title, TitleWidth, Latency, Throughput, Relative, DigitWidth );
            };

            std::print(
                "\n┌{0:─^{2}}┐"  //
                "\n│{1: ^{2}}│"  //
                "\n└{0:─^{2}}┘\n",
                "", Header, HeaderSpace );
            PrintRow( "", "Latency", "Throughput", "Relative %" );
            PrintLine();
            for( auto&& Result : Samples )
                PrintRow( Result.Title, Result.Latency(), Result.Throughput(),
                          100 * Result.Throughput() / ThroughputBaseline );
            PrintLine();
        }
    };

    inline BenchmarkResultAnalyzer Analyzer{};

    inline auto DefaultExecutor::AsBaseLine()
    {
        Analyzer.BaselinePos = static_cast<std::size_t>( &Result - Analyzer.Samples.data() );
        return *this;
    }

    //using BenchmarkExecutor = DefaultExecutor;
    template<typename BenchmarkExecutor = DefaultExecutor>
    auto Benchmark( std::string_view BenchmarkTitle ) noexcept
    {
        return BenchmarkExecutor{ Analyzer.Samples.emplace_back( BenchmarkTitle ) };
    }

}  // namespace EasyBenchmark

using EasyBenchmark::Benchmark;  // NOLINT
#endif                           /* BENCHMARK_H */