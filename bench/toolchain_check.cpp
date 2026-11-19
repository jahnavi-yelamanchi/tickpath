#include <benchmark/benchmark.h>
#include <hdr/hdr_histogram.h>

#include <cstdint>

namespace {

void bm_hdr_record(benchmark::State& state) {
    hdr_histogram* h = nullptr;
    if (hdr_init(1, INT64_C(1'000'000'000), 3, &h) != 0) {
        state.SkipWithError("hdr_init failed");
        return;
    }
    std::int64_t v = 1;
    for (auto _ : state) {
        hdr_record_value(h, v);
        v = (v * 7) % 1'000'000 + 1;
    }
    benchmark::DoNotOptimize(hdr_value_at_percentile(h, 99.9));
    hdr_close(h);
}
BENCHMARK(bm_hdr_record);

} // namespace

BENCHMARK_MAIN();
