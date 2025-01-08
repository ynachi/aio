#include <benchmark/benchmark.h>
#include <cstring>

static void BM_CopyN(benchmark::State& state)
{
    std::vector<char> src(1024, 'a');
    std::vector<char> dst(1024);
    for (auto _: state)
    {
        std::copy_n(src.begin(), 1024, dst.begin());
    }
}

static void BM_Memcpy(benchmark::State& state)
{
    std::vector<char> src(1024, 'a');
    std::vector<char> dst(1024);
    for (auto _: state)
    {
        std::memcpy(dst.data(), src.data(), 1024);
    }
}

BENCHMARK(BM_CopyN);
BENCHMARK(BM_Memcpy);
