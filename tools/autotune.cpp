/**
 * Disruptor++ Auto-Tune Tool
 *
 * Automatically finds the optimal buffer size and wait strategy for your hardware.
 *
 * Usage:
 *   ./autotune                          # Interactive mode
 *   ./autotune --scenario 1P1C --event-size 64
 *   ./autotune --scenario 1P3C --event-size 128 --no-pin
 *   ./autotune --all --event-size 64    # Run all scenarios
 *
 * Scenarios: 1P1C, 1P2C, 1P3C, 2P1C, 3P1C, 2P2C
 * Event sizes: 8, 16, 32, 64, 128, 256, 512 (bytes)
 */
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <vector>
#include <memory>
#include <cstdint>
#include <algorithm>
#include <functional>
#include <fstream>
#include <string>
#include <sstream>
#include <array>
#include <set>
#include <map>

#include "ring_buffer/RingBuffer.hpp"
#include "sequencer/SingleProducerSequencer.hpp"
#include "sequencer/MultiProducerSequencer.hpp"
#include "barriers/ProcessingSequenceBarrier.hpp"
#include "processor/BatchEventProcessor.hpp"
#include "common/Common.hpp"
#include "common/Util.hpp"

using namespace disruptor;

// ═══════════════════════════════════════════════════════════
//  Event structs — pre-defined sizes (compile-time requirement)
// ═══════════════════════════════════════════════════════════
struct EV8    { int64_t value{0}; };
struct EV16   { int64_t value{0}; char pad[8]; };
struct EV32   { int64_t value{0}; char pad[24]; };
struct EV64   { int64_t value{0}; char pad[56]; };
struct EV128  { int64_t value{0}; char pad[120]; };
struct EV256  { int64_t value{0}; char pad[248]; };
struct EV512  { int64_t value{0}; char pad[504]; };

template<typename EV>
struct Handler { int64_t sum = 0; void operator()(EV& e, size_t, bool) { sum += e.value; } };

// ═══════════════════════════════════════════════════════════
//  CPU Topology Detection
// ═══════════════════════════════════════════════════════════
struct CoreInfo {
    int id;
    int physical_core;  // physical core id (siblings share this)
    int max_freq_khz;   // max frequency in KHz
    bool is_online;
};

struct CpuTopology {
    std::vector<CoreInfo> cores;
    std::vector<int> best_cores;  // sorted by freq desc, one per physical core
    int num_physical;
    int num_logical;
    size_t l1d_size;  // bytes
    size_t l2_size;
    size_t l3_size;
};

static std::string read_file_str(const char* path) {
    std::ifstream f(path);
    if (!f.is_open()) return "";
    std::string s;
    std::getline(f, s);
    return s;
}

static int read_file_int(const char* path) {
    auto s = read_file_str(path);
    return s.empty() ? -1 : std::atoi(s.c_str());
}

static size_t parse_cache_size(const std::string& s) {
    if (s.empty()) return 0;
    size_t val = std::atol(s.c_str());
    if (s.back() == 'K' || s.find("K") != std::string::npos) val *= 1024;
    else if (s.back() == 'M' || s.find("M") != std::string::npos) val *= 1024 * 1024;
    return val;
}

static CpuTopology detect_topology() {
    CpuTopology topo{};
    topo.l1d_size = 0;
    topo.l2_size = 0;
    topo.l3_size = 0;

    // Detect number of logical CPUs
    int max_cpu = static_cast<int>(std::thread::hardware_concurrency());
    if (max_cpu <= 0) max_cpu = 128;

    std::set<int> physical_seen;

    for (int i = 0; i < max_cpu; ++i) {
        char path[256];
        snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu%d/online", i);
        int online = read_file_int(path);
        if (online == 0) continue;  // offline
        // cpu0 might not have 'online' file
        if (online == -1 && i > 0) continue;

        CoreInfo ci{};
        ci.id = i;
        ci.is_online = true;

        // Physical core ID
        snprintf(path, sizeof(path),
                 "/sys/devices/system/cpu/cpu%d/topology/core_id", i);
        ci.physical_core = read_file_int(path);
        if (ci.physical_core < 0) ci.physical_core = i;

        // Max frequency
        snprintf(path, sizeof(path),
                 "/sys/devices/system/cpu/cpu%d/cpufreq/cpuinfo_max_freq", i);
        ci.max_freq_khz = read_file_int(path);
        if (ci.max_freq_khz < 0) {
            // Try scaling_max_freq as fallback
            snprintf(path, sizeof(path),
                     "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_max_freq", i);
            ci.max_freq_khz = read_file_int(path);
        }

        physical_seen.insert(ci.physical_core);
        topo.cores.push_back(ci);
    }

    topo.num_logical = static_cast<int>(topo.cores.size());
    topo.num_physical = static_cast<int>(physical_seen.size());

    // Detect cache sizes from cpu0
    for (int idx = 0; idx < 10; ++idx) {
        char path[256];
        snprintf(path, sizeof(path),
                 "/sys/devices/system/cpu/cpu0/cache/index%d/type", idx);
        auto type = read_file_str(path);
        if (type.empty()) break;

        snprintf(path, sizeof(path),
                 "/sys/devices/system/cpu/cpu0/cache/index%d/size", idx);
        auto size_str = read_file_str(path);
        size_t sz = parse_cache_size(size_str);

        snprintf(path, sizeof(path),
                 "/sys/devices/system/cpu/cpu0/cache/index%d/level", idx);
        int level = read_file_int(path);

        if (level == 1 && (type == "Data" || type == "Unified")) topo.l1d_size = sz;
        else if (level == 2) topo.l2_size = sz;
        else if (level == 3) topo.l3_size = sz;
    }

    // Select best cores: one per physical core, sorted by max freq desc
    // Group by physical_core, pick highest freq from each group
    std::map<int, CoreInfo> best_per_physical;
    for (auto& c : topo.cores) {
        auto it = best_per_physical.find(c.physical_core);
        if (it == best_per_physical.end() || c.max_freq_khz > it->second.max_freq_khz) {
            best_per_physical[c.physical_core] = c;
        }
    }

    std::vector<CoreInfo> candidates;
    for (auto& [_, ci] : best_per_physical) candidates.push_back(ci);

    // Sort by frequency descending
    std::sort(candidates.begin(), candidates.end(),
              [](const CoreInfo& a, const CoreInfo& b) {
                  return a.max_freq_khz > b.max_freq_khz;
              });

    for (auto& c : candidates) topo.best_cores.push_back(c.id);

    return topo;
}

static void print_topology(const CpuTopology& topo) {
    printf("  Logical CPUs:  %d\n", topo.num_logical);
    printf("  Physical cores: %d\n", topo.num_physical);
    if (topo.l1d_size > 0)
        printf("  L1d cache:     %zu KB\n", topo.l1d_size / 1024);
    if (topo.l2_size > 0)
        printf("  L2 cache:      %zu KB\n", topo.l2_size / 1024);
    if (topo.l3_size > 0)
        printf("  L3 cache:      %zu MB\n", topo.l3_size / (1024 * 1024));

    // Show top cores
    int show = std::min(8, static_cast<int>(topo.best_cores.size()));
    printf("  Best cores:    ");
    for (int i = 0; i < show; ++i) {
        int cid = topo.best_cores[i];
        // Find freq
        for (auto& c : topo.cores) {
            if (c.id == cid) {
                printf("cpu%d(%.1fGHz)", cid,
                       c.max_freq_khz > 0 ? c.max_freq_khz / 1e6 : 0);
                break;
            }
        }
        if (i < show - 1) printf(", ");
    }
    printf("\n");
}

// ═══════════════════════════════════════════════════════════
//  Global config
// ═══════════════════════════════════════════════════════════
static CpuTopology g_topo;
static bool g_pin = true;

static void pin(int core_idx) {
    if (!g_pin) return;
    if (core_idx < static_cast<int>(g_topo.best_cores.size())) {
        Util::pin_thread_to_core(g_topo.best_cores[core_idx]);
    }
}

// ═══════════════════════════════════════════════════════════
//  Benchmark templates
// ═══════════════════════════════════════════════════════════

// 1P-NC (single producer, N consumers)
template<typename EV, size_t BUF, int NC, WaitStrategyType W, size_t ITER>
long run_1pNc() {
    using H = Handler<EV>;
    using RB = RingBuffer<EV, BUF>;
    using S = SingleProducerSequencer<EV, BUF, NC>;
    using B = ProcessingSequenceBarrier<W, 1, S>;
    using P = BatchEventProcessor<EV, BUF, H, B>;

    auto rb = std::make_unique<RB>([] { return EV(); });
    auto s = std::make_unique<S>(*rb);
    auto cr = std::ref(s->get_cursor());

    std::unique_ptr<B> barriers[NC];
    H handlers[NC];
    std::unique_ptr<P> procs[NC];
    for (int i = 0; i < NC; ++i) {
        barriers[i] = std::unique_ptr<B>(new B(true, {cr}, *s));
        procs[i] = std::make_unique<P>(*barriers[i], handlers[i], *rb);
    }

    // Gating sequences — must use initializer_list (compile-time size)
    if constexpr (NC == 1) {
        s->add_gating_sequences({std::ref(procs[0]->get_cursor())});
    } else if constexpr (NC == 2) {
        s->add_gating_sequences({std::ref(procs[0]->get_cursor()),
                                  std::ref(procs[1]->get_cursor())});
    } else if constexpr (NC == 3) {
        s->add_gating_sequences({std::ref(procs[0]->get_cursor()),
                                  std::ref(procs[1]->get_cursor()),
                                  std::ref(procs[2]->get_cursor())});
    }

    size_t exp = BUF + ITER;
    std::vector<std::thread> threads;
    for (int i = 0; i < NC; ++i)
        threads.emplace_back([&procs, i] { pin(1 + i); procs[i]->run(); });

    pin(0);
    auto st = std::chrono::high_resolution_clock::now();
    for (size_t i = 0; i < ITER; ++i) {
        auto sq = s->next(1);
        rb->get(sq).value = static_cast<int64_t>(i);
        s->publish(sq);
    }
    for (int i = 0; i < NC; ++i)
        while (procs[i]->get_cursor().get_with_acquire() < exp) std::this_thread::yield();
    auto en = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < NC; ++i) procs[i]->halt();
    for (auto& t : threads) t.join();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(en - st).count();
    return ms > 0 ? static_cast<long>(ITER * 1000L / ms) : 0;
}

// NP-1C (N producers, single consumer)
template<typename EV, size_t BUF, int NP, WaitStrategyType W, size_t PP>
long run_Np1c() {
    using H = Handler<EV>;
    constexpr size_t TOT = PP * NP;
    using RB = RingBuffer<EV, BUF>;
    using S = MultiProducerSequencer<EV, BUF, 1>;
    using B = ProcessingSequenceBarrier<W, 1, S>;
    using P = BatchEventProcessor<EV, BUF, H, B>;

    auto rb = std::make_unique<RB>([] { return EV(); });
    auto s = std::make_unique<S>(*rb);
    auto cr = std::ref(s->get_cursor());
    auto b = std::unique_ptr<B>(new B(true, {cr}, *s));
    H h;
    auto p = std::make_unique<P>(*b, h, *rb);
    s->add_gating_sequences({std::ref(p->get_cursor())});

    size_t exp = BUF + TOT;
    std::thread tc([&] { pin(NP); p->run(); });

    auto st = std::chrono::high_resolution_clock::now();
    std::vector<std::thread> ps;
    for (int i = 0; i < NP; ++i)
        ps.emplace_back([&s, &rb, i] {
            pin(i);
            for (size_t j = 0; j < PP; ++j) {
                auto sq = s->next(1);
                rb->get(sq).value = static_cast<int64_t>(j);
                s->publish(sq);
            }
        });
    for (auto& t : ps) t.join();
    while (p->get_cursor().get_with_acquire() < exp) std::this_thread::yield();
    auto en = std::chrono::high_resolution_clock::now();

    p->halt();
    tc.join();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(en - st).count();
    return ms > 0 ? static_cast<long>(TOT * 1000L / ms) : 0;
}

// 2P-2C
template<typename EV, size_t BUF, WaitStrategyType W, size_t PP>
long run_2p2c() {
    using H = Handler<EV>;
    constexpr size_t TOT = PP * 2;
    using RB = RingBuffer<EV, BUF>;
    using S = MultiProducerSequencer<EV, BUF, 2>;
    using B = ProcessingSequenceBarrier<W, 1, S>;
    using P = BatchEventProcessor<EV, BUF, H, B>;

    auto rb = std::make_unique<RB>([] { return EV(); });
    auto s = std::make_unique<S>(*rb);
    auto cr = std::ref(s->get_cursor());
    auto b0 = std::unique_ptr<B>(new B(true, {cr}, *s));
    auto b1 = std::unique_ptr<B>(new B(true, {cr}, *s));
    H h0, h1;
    auto p0 = std::make_unique<P>(*b0, h0, *rb);
    auto p1 = std::make_unique<P>(*b1, h1, *rb);
    s->add_gating_sequences({std::ref(p0->get_cursor()), std::ref(p1->get_cursor())});

    size_t exp = BUF + TOT;
    std::thread tc0([&] { pin(2); p0->run(); });
    std::thread tc1([&] { pin(3); p1->run(); });

    auto st = std::chrono::high_resolution_clock::now();
    std::thread tp0([&] {
        pin(0);
        for (size_t j = 0; j < PP; ++j) {
            auto sq = s->next(1); rb->get(sq).value = static_cast<int64_t>(j); s->publish(sq);
        }
    });
    std::thread tp1([&] {
        pin(1);
        for (size_t j = 0; j < PP; ++j) {
            auto sq = s->next(1); rb->get(sq).value = static_cast<int64_t>(j); s->publish(sq);
        }
    });
    tp0.join(); tp1.join();
    while (p0->get_cursor().get_with_acquire() < exp) std::this_thread::yield();
    while (p1->get_cursor().get_with_acquire() < exp) std::this_thread::yield();
    auto en = std::chrono::high_resolution_clock::now();

    p0->halt(); p1->halt();
    tc0.join(); tc1.join();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(en - st).count();
    return ms > 0 ? static_cast<long>(TOT * 1000L / ms) : 0;
}

// ═══════════════════════════════════════════════════════════
//  Bench runner with warmup + median
// ═══════════════════════════════════════════════════════════
static constexpr int WARMUP = 2;
static constexpr int RUNS = 5;

long bench(std::function<long()> fn) {
    for (int i = 0; i < WARMUP; ++i) fn();
    std::vector<long> res;
    for (int i = 0; i < RUNS; ++i) res.push_back(fn());
    std::sort(res.begin(), res.end());
    return res[RUNS / 2];
}

// ═══════════════════════════════════════════════════════════
//  Result collection
// ═══════════════════════════════════════════════════════════
struct Result {
    const char* scenario;
    int event_bytes;
    size_t buf_slots;
    const char* wait_strategy;
    bool pinned;
    long ops_per_sec;
    size_t total_memory;  // ring buffer memory in bytes
};

// ═══════════════════════════════════════════════════════════
//  Dispatch macro — instantiate all buffer sizes for one (EV, scenario, wait)
//  Buffer sizes: 4K, 8K, 16K, 32K, 64K, 128K, 256K, 512K, 1M
// ═══════════════════════════════════════════════════════════

// Determine event count based on event size to keep runtime reasonable
// Larger events = fewer iterations
template<int EV_BYTES>
constexpr size_t iter_count() {
    if constexpr (EV_BYTES <= 16) return 50'000'000;
    else if constexpr (EV_BYTES <= 64) return 30'000'000;
    else if constexpr (EV_BYTES <= 128) return 20'000'000;
    else if constexpr (EV_BYTES <= 256) return 10'000'000;
    else return 5'000'000;
}

template<int EV_BYTES>
constexpr size_t mp_per_producer() {
    if constexpr (EV_BYTES <= 16) return 5'000'000;
    else if constexpr (EV_BYTES <= 64) return 3'000'000;
    else if constexpr (EV_BYTES <= 128) return 2'000'000;
    else return 1'000'000;
}

// Sweep all buffer sizes × wait strategies for a given scenario + event type
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#define SWEEP_1PNC(EV, EV_SZ, NC, results) do { \
    constexpr size_t IT = iter_count<EV_SZ>(); \
    constexpr const char* scn = (NC==1) ? "1P1C" : (NC==2) ? "1P2C" : "1P3C"; \
    /* For each buffer size, run all 3 strategies */ \
    auto run_buf = [&](auto buf_tag, size_t buf_val) { \
        constexpr size_t B = decltype(buf_tag)::value; \
        long ra = bench(run_1pNc<EV, B, NC, WaitStrategyType::ADAPTIVE, IT>); \
        results.push_back({scn, EV_SZ, buf_val, "Adaptive", g_pin, ra, buf_val * EV_SZ}); \
        printf("    BUF=%5zuK  Adaptive=%4ldM", buf_val/1024, ra/1000000); \
        long ry = bench(run_1pNc<EV, B, NC, WaitStrategyType::YIELD, IT>); \
        results.push_back({scn, EV_SZ, buf_val, "Yield", g_pin, ry, buf_val * EV_SZ}); \
        printf("  Yield=%4ldM", ry/1000000); \
        long rb = bench(run_1pNc<EV, B, NC, WaitStrategyType::BUSY_SPIN, IT>); \
        results.push_back({scn, EV_SZ, buf_val, "BusySpin", g_pin, rb, buf_val * EV_SZ}); \
        printf("  BSpin=%4ldM\n", rb/1000000); \
    }; \
    run_buf(std::integral_constant<size_t, 4096>{}, 4096); \
    run_buf(std::integral_constant<size_t, 8192>{}, 8192); \
    run_buf(std::integral_constant<size_t, 16384>{}, 16384); \
    run_buf(std::integral_constant<size_t, 32768>{}, 32768); \
    run_buf(std::integral_constant<size_t, 65536>{}, 65536); \
    run_buf(std::integral_constant<size_t, 131072>{}, 131072); \
    run_buf(std::integral_constant<size_t, 262144>{}, 262144); \
    run_buf(std::integral_constant<size_t, 524288>{}, 524288); \
    run_buf(std::integral_constant<size_t, 1048576>{}, 1048576); \
} while(0)

#define SWEEP_NP1C(EV, EV_SZ, NP, results) do { \
    constexpr size_t PP = mp_per_producer<EV_SZ>(); \
    constexpr const char* scn = (NP==2) ? "2P1C" : "3P1C"; \
    auto run_buf = [&](auto buf_tag, size_t buf_val) { \
        constexpr size_t B = decltype(buf_tag)::value; \
        long ra = bench(run_Np1c<EV, B, NP, WaitStrategyType::ADAPTIVE, PP>); \
        results.push_back({scn, EV_SZ, buf_val, "Adaptive", g_pin, ra, buf_val * EV_SZ}); \
        printf("    BUF=%5zuK  Adaptive=%4ldM", buf_val/1024, ra/1000000); \
        long ry = bench(run_Np1c<EV, B, NP, WaitStrategyType::YIELD, PP>); \
        results.push_back({scn, EV_SZ, buf_val, "Yield", g_pin, ry, buf_val * EV_SZ}); \
        printf("  Yield=%4ldM", ry/1000000); \
        long rb = bench(run_Np1c<EV, B, NP, WaitStrategyType::BUSY_SPIN, PP>); \
        results.push_back({scn, EV_SZ, buf_val, "BusySpin", g_pin, rb, buf_val * EV_SZ}); \
        printf("  BSpin=%4ldM\n", rb/1000000); \
    }; \
    run_buf(std::integral_constant<size_t, 4096>{}, 4096); \
    run_buf(std::integral_constant<size_t, 8192>{}, 8192); \
    run_buf(std::integral_constant<size_t, 16384>{}, 16384); \
    run_buf(std::integral_constant<size_t, 32768>{}, 32768); \
    run_buf(std::integral_constant<size_t, 65536>{}, 65536); \
    run_buf(std::integral_constant<size_t, 131072>{}, 131072); \
    run_buf(std::integral_constant<size_t, 262144>{}, 262144); \
    run_buf(std::integral_constant<size_t, 524288>{}, 524288); \
    run_buf(std::integral_constant<size_t, 1048576>{}, 1048576); \
} while(0)

#define SWEEP_2P2C(EV, EV_SZ, results) do { \
    constexpr size_t PP = mp_per_producer<EV_SZ>(); \
    auto run_buf = [&](auto buf_tag, size_t buf_val) { \
        constexpr size_t B = decltype(buf_tag)::value; \
        long ra = bench(run_2p2c<EV, B, WaitStrategyType::ADAPTIVE, PP>); \
        results.push_back({"2P2C", EV_SZ, buf_val, "Adaptive", g_pin, ra, buf_val * EV_SZ}); \
        printf("    BUF=%5zuK  Adaptive=%4ldM", buf_val/1024, ra/1000000); \
        long ry = bench(run_2p2c<EV, B, WaitStrategyType::YIELD, PP>); \
        results.push_back({"2P2C", EV_SZ, buf_val, "Yield", g_pin, ry, buf_val * EV_SZ}); \
        printf("  Yield=%4ldM", ry/1000000); \
        long rb = bench(run_2p2c<EV, B, WaitStrategyType::BUSY_SPIN, PP>); \
        results.push_back({"2P2C", EV_SZ, buf_val, "BusySpin", g_pin, rb, buf_val * EV_SZ}); \
        printf("  BSpin=%4ldM\n", rb/1000000); \
    }; \
    run_buf(std::integral_constant<size_t, 4096>{}, 4096); \
    run_buf(std::integral_constant<size_t, 8192>{}, 8192); \
    run_buf(std::integral_constant<size_t, 16384>{}, 16384); \
    run_buf(std::integral_constant<size_t, 32768>{}, 32768); \
    run_buf(std::integral_constant<size_t, 65536>{}, 65536); \
    run_buf(std::integral_constant<size_t, 131072>{}, 131072); \
    run_buf(std::integral_constant<size_t, 262144>{}, 262144); \
    run_buf(std::integral_constant<size_t, 524288>{}, 524288); \
    run_buf(std::integral_constant<size_t, 1048576>{}, 1048576); \
} while(0)

// Dispatch by event size (runtime → compile-time)
void run_scenario(const char* scenario, int ev_bytes, std::vector<Result>& results) {
    printf("\n  Sweeping %s with %dB events...\n", scenario, ev_bytes);

    // Macro to dispatch EV type based on runtime ev_bytes
    #define DISPATCH_EV(BODY) do { \
        if (ev_bytes == 8)        { using EV = EV8;   constexpr int ES = 8;   BODY; } \
        else if (ev_bytes == 16)  { using EV = EV16;  constexpr int ES = 16;  BODY; } \
        else if (ev_bytes == 32)  { using EV = EV32;  constexpr int ES = 32;  BODY; } \
        else if (ev_bytes == 64)  { using EV = EV64;  constexpr int ES = 64;  BODY; } \
        else if (ev_bytes == 128) { using EV = EV128; constexpr int ES = 128; BODY; } \
        else if (ev_bytes == 256) { using EV = EV256; constexpr int ES = 256; BODY; } \
        else if (ev_bytes == 512) { using EV = EV512; constexpr int ES = 512; BODY; } \
        else { printf("  ERROR: Unsupported event size %d. Use 8,16,32,64,128,256,512.\n", ev_bytes); } \
    } while(0)

    if (strcmp(scenario, "1P1C") == 0) {
        DISPATCH_EV(SWEEP_1PNC(EV, ES, 1, results));
    } else if (strcmp(scenario, "1P2C") == 0) {
        DISPATCH_EV(SWEEP_1PNC(EV, ES, 2, results));
    } else if (strcmp(scenario, "1P3C") == 0) {
        DISPATCH_EV(SWEEP_1PNC(EV, ES, 3, results));
    } else if (strcmp(scenario, "2P1C") == 0) {
        DISPATCH_EV(SWEEP_NP1C(EV, ES, 2, results));
    } else if (strcmp(scenario, "3P1C") == 0) {
        DISPATCH_EV(SWEEP_NP1C(EV, ES, 3, results));
    } else if (strcmp(scenario, "2P2C") == 0) {
        DISPATCH_EV(SWEEP_2P2C(EV, ES, results));
    } else {
        printf("  ERROR: Unknown scenario '%s'\n", scenario);
        printf("  Valid: 1P1C, 1P2C, 1P3C, 2P1C, 3P1C, 2P2C\n");
    }

    #undef DISPATCH_EV
}
#pragma GCC diagnostic pop

// ═══════════════════════════════════════════════════════════
//  Report
// ═══════════════════════════════════════════════════════════
void print_report(const std::vector<Result>& results) {
    if (results.empty()) return;

    // Find best overall
    auto best = std::max_element(results.begin(), results.end(),
                                  [](const Result& a, const Result& b) {
                                      return a.ops_per_sec < b.ops_per_sec;
                                  });

    printf("\n");
    printf("╔════════════════════════════════════════════════╗\n");
    printf("║           OPTIMAL CONFIGURATION              ║\n");
    printf("╠════════════════════════════════════════════════╣\n");
    printf("║  Scenario:       %-28s║\n", best->scenario);

    char ev_str[32];
    snprintf(ev_str, sizeof(ev_str), "%d bytes", best->event_bytes);
    printf("║  Event size:     %-28s║\n", ev_str);

    char buf_str[64];
    if (best->buf_slots >= 1048576)
        snprintf(buf_str, sizeof(buf_str), "%zuM (%zu slots)", best->buf_slots / (1024*1024), best->buf_slots);
    else
        snprintf(buf_str, sizeof(buf_str), "%zuK (%zu slots)", best->buf_slots / 1024, best->buf_slots);
    printf("║  Buffer size:    %-28s║\n", buf_str);

    printf("║  Wait strategy:  %-28s║\n", best->wait_strategy);
    printf("║  Thread pinning: %-28s║\n", best->pinned ? "YES" : "NO");

    char mem_str[32];
    if (best->total_memory >= 1024*1024)
        snprintf(mem_str, sizeof(mem_str), "%zu MB", best->total_memory / (1024*1024));
    else
        snprintf(mem_str, sizeof(mem_str), "%zu KB", best->total_memory / 1024);
    printf("║  Buffer memory:  %-28s║\n", mem_str);

    char perf_str[32];
    snprintf(perf_str, sizeof(perf_str), "%ld M ops/s", best->ops_per_sec / 1000000);
    printf("║  Throughput:     %-28s║\n", perf_str);
    printf("╚════════════════════════════════════════════════╝\n");

    // Top 5 configurations
    auto sorted = results;
    std::sort(sorted.begin(), sorted.end(),
              [](const Result& a, const Result& b) { return a.ops_per_sec > b.ops_per_sec; });

    printf("\n  Top 5 configurations:\n");
    printf("  %-5s %-6s %-10s %-10s %-8s %s\n",
           "Rank", "Scen", "BufSize", "Strategy", "Memory", "Throughput");
    printf("  ───── ────── ────────── ────────── ──────── ──────────\n");
    int shown = std::min(5, static_cast<int>(sorted.size()));
    for (int i = 0; i < shown; ++i) {
        auto& r = sorted[i];
        char bs[32], ms[32];
        if (r.buf_slots >= 1048576)
            snprintf(bs, sizeof(bs), "%zuM", r.buf_slots / (1024*1024));
        else
            snprintf(bs, sizeof(bs), "%zuK", r.buf_slots / 1024);
        if (r.total_memory >= 1024*1024)
            snprintf(ms, sizeof(ms), "%zuMB", r.total_memory / (1024*1024));
        else
            snprintf(ms, sizeof(ms), "%zuKB", r.total_memory / 1024);
        printf("  #%-4d %-6s %-10s %-10s %-8s %ldM ops/s\n",
               i + 1, r.scenario, bs, r.wait_strategy, ms, r.ops_per_sec / 1000000);
    }

    // Suggest C++ code
    const char* ws_enum =
        strcmp(best->wait_strategy, "Adaptive") == 0 ? "ADAPTIVE" :
        strcmp(best->wait_strategy, "Yield") == 0 ? "YIELD" : "BUSY_SPIN";

    printf("\n  Suggested C++ configuration:\n\n");
    printf("    constexpr size_t BUFFER_SIZE = %zu;\n", best->buf_slots);
    printf("    constexpr auto WAIT = WaitStrategyType::%s;\n", ws_enum);
    printf("\n");
}

// ═══════════════════════════════════════════════════════════
//  Main
// ═══════════════════════════════════════════════════════════
void print_usage() {
    printf("Usage: autotune [OPTIONS]\n\n");
    printf("Options:\n");
    printf("  --scenario <S>      Scenario: 1P1C, 1P2C, 1P3C, 2P1C, 3P1C, 2P2C\n");
    printf("  --event-size <N>    Event struct size in bytes: 8,16,32,64,128,256,512\n");
    printf("  --no-pin            Disable thread pinning\n");
    printf("  --all               Run all scenarios\n");
    printf("  --help              Show this help\n");
    printf("\nExamples:\n");
    printf("  autotune --scenario 1P1C --event-size 64\n");
    printf("  autotune --scenario 1P3C --event-size 128\n");
    printf("  autotune --all --event-size 32\n");
    printf("\nInteractive mode (no arguments): prompts for scenario and event size.\n");
}

int main(int argc, char* argv[]) {
    setbuf(stdout, NULL);

    const char* scenario = nullptr;
    int ev_bytes = 0;
    bool run_all = false;

    // Parse args
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--scenario") == 0 && i + 1 < argc) {
            scenario = argv[++i];
        } else if (strcmp(argv[i], "--event-size") == 0 && i + 1 < argc) {
            ev_bytes = std::atoi(argv[++i]);
        } else if (strcmp(argv[i], "--no-pin") == 0) {
            g_pin = false;
        } else if (strcmp(argv[i], "--all") == 0) {
            run_all = true;
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_usage();
            return 0;
        }
    }

    // Interactive mode if no args
    if (!scenario && !run_all && ev_bytes == 0) {
        printf("Disruptor++ Auto-Tune (interactive mode)\n\n");
        printf("Select scenario:\n");
        printf("  1) 1P1C  — 1 producer, 1 consumer\n");
        printf("  2) 1P2C  — 1 producer, 2 consumers\n");
        printf("  3) 1P3C  — 1 producer, 3 consumers\n");
        printf("  4) 2P1C  — 2 producers, 1 consumer\n");
        printf("  5) 3P1C  — 3 producers, 1 consumer\n");
        printf("  6) 2P2C  — 2 producers, 2 consumers\n");
        printf("  7) ALL   — run all scenarios\n");
        printf("\nChoice [1-7]: ");

        int choice = 0;
        if (scanf("%d", &choice) != 1) { printf("Invalid input.\n"); return 1; }

        static const char* scenarios[] = {"1P1C","1P2C","1P3C","2P1C","3P1C","2P2C"};
        if (choice >= 1 && choice <= 6) scenario = scenarios[choice - 1];
        else if (choice == 7) run_all = true;
        else { printf("Invalid choice.\n"); return 1; }

        printf("\nEvent struct size in bytes:\n");
        printf("  8, 16, 32, 64, 128, 256, 512\n");
        printf("\nSize [bytes]: ");
        if (scanf("%d", &ev_bytes) != 1) { printf("Invalid input.\n"); return 1; }

        printf("\nEnable thread pinning? [Y/n]: ");
        char pin_choice = 'Y';
        if (scanf(" %c", &pin_choice) == 1 && (pin_choice == 'n' || pin_choice == 'N'))
            g_pin = false;
    }

    // Validate event size
    std::set<int> valid_sizes = {8, 16, 32, 64, 128, 256, 512};
    if (valid_sizes.find(ev_bytes) == valid_sizes.end()) {
        printf("ERROR: Event size must be one of: 8, 16, 32, 64, 128, 256, 512\n");
        return 1;
    }

    // Detect CPU
    g_topo = detect_topology();

    printf("═══════════════════════════════════════════════════════════\n");
    printf("  Disruptor++ Auto-Tune\n");
    printf("═══════════════════════════════════════════════════════════\n");
    printf("\n  CPU Topology:\n");
    print_topology(g_topo);
    printf("\n  Config:\n");
    printf("  Event size:    %d bytes\n", ev_bytes);
    printf("  Thread pinning: %s\n", g_pin ? "YES" : "NO");
    printf("  Warmup:        %d runs\n", WARMUP);
    printf("  Measurement:   %d runs (median)\n", RUNS);
    printf("  Buffer sizes:  4K → 1M (9 steps)\n");
    printf("  Wait strategies: Adaptive, Yield, BusySpin\n");

    int need_cores = 4;  // max needed (2P2C or 1P3C)
    if (g_pin && static_cast<int>(g_topo.best_cores.size()) < need_cores) {
        printf("\n  WARNING: Only %zu cores detected, need %d for some scenarios.\n",
               g_topo.best_cores.size(), need_cores);
        printf("  Some scenarios may not get dedicated cores.\n");
    }

    std::vector<Result> results;

    if (run_all) {
        const char* all_scenarios[] = {"1P1C", "1P2C", "1P3C", "2P1C", "3P1C", "2P2C"};
        for (auto* s : all_scenarios) {
            run_scenario(s, ev_bytes, results);
        }
    } else {
        run_scenario(scenario, ev_bytes, results);
    }

    print_report(results);

    return 0;
}
