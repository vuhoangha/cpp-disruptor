#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <thread>
#include <vector>

#include "../include/processor/BatchEventProcessor.hpp"
#include "../include/sequencer/SingleProducerSequencer.hpp"
#include "../include/sequencer/MultiProducerSequencer.hpp"
#include "../include/barriers/ProcessingSequenceBarrier.hpp"
#include "../include/ring_buffer/RingBuffer.hpp"
#include "../include/common/Util.hpp"
#include "../include/wait_strategy/WaitStrategyType.hpp"
#include "../include/event.hpp"

// ============================================================================
// Benchmark infrastructure
// ============================================================================

struct BenchmarkResult {
    double median_ops_per_sec;
    double min_ops_per_sec;
    double max_ops_per_sec;
    double stddev_ops_per_sec;
    std::vector<double> all_ops_per_sec;
};

// Prevent compiler from optimizing away a value
template<typename T>
__attribute__((always_inline)) inline void do_not_optimize(T const& value) {
    asm volatile("" : : "r,m"(value) : "memory");
}

BenchmarkResult compute_stats(std::vector<double>& samples) {
    std::sort(samples.begin(), samples.end());

    const size_t n = samples.size();
    const double median = (n % 2 == 0)
        ? (samples[n / 2 - 1] + samples[n / 2]) / 2.0
        : samples[n / 2];

    const double sum = std::accumulate(samples.begin(), samples.end(), 0.0);
    const double mean = sum / n;

    double sq_sum = 0.0;
    for (const double s : samples) {
        sq_sum += (s - mean) * (s - mean);
    }
    const double stddev = std::sqrt(sq_sum / n);

    return {
        .median_ops_per_sec = median,
        .min_ops_per_sec = samples.front(),
        .max_ops_per_sec = samples.back(),
        .stddev_ops_per_sec = stddev,
        .all_ops_per_sec = samples
    };
}

void print_result(const char* name, const BenchmarkResult& r) {
    std::cout << "  " << std::left << std::setw(20) << name
              << " median: " << std::right << std::setw(8) << std::fixed << std::setprecision(1)
              << (r.median_ops_per_sec / 1e6) << "M ops/s"
              << "  [min=" << std::setprecision(1) << (r.min_ops_per_sec / 1e6)
              << "M, max=" << std::setprecision(1) << (r.max_ops_per_sec / 1e6)
              << "M, stddev=" << std::setprecision(1) << (r.stddev_ops_per_sec / 1e6)
              << "M]  runs: [";

    for (size_t i = 0; i < r.all_ops_per_sec.size(); ++i) {
        if (i > 0) std::cout << ", ";
        std::cout << std::setprecision(1) << (r.all_ops_per_sec[i] / 1e6);
    }
    std::cout << "]M" << std::endl;
}

// Map logical thread index to physical P-core CPU id (skip hyperthreads)
// i9-14900K: CPU 0,2,4,6,8,10,12,14 are P-core first threads
//            CPU 16-23 are E-cores (slower, avoid)
constexpr bool USE_AFFINITY = true;

constexpr int physical_core_cpu(int logical_index) {
    return logical_index * 2; // 0->0, 1->2, 2->4, 3->6, etc.
}

void maybe_pin_core(int logical_index) {
    if constexpr (USE_AFFINITY) {
        disruptor::Util::pin_thread_to_core(physical_core_cpu(logical_index));
    }
}

// Lightweight handler — no atomic counter overhead, just prevent optimization
struct NoOpHandler {
    void operator()(disruptor::Event& event, size_t seq, bool end_of_batch) const {
        do_not_optimize(event.get_value());
    }
};

// Handler matching LMAX's ValueAdditionEventHandler for fair comparison
// LMAX uses PaddedLong (plain long + padding), no volatile/atomic — just accumulation
struct ValueAdditionHandler {
    size_t sum = 0;
    void operator()(disruptor::Event& event, size_t seq, bool end_of_batch) {
        sum += event.get_value();
    }
};

// Same as ValueAddition but with do_not_optimize to test compiler barrier effect
struct ValueAdditionDoNotOptHandler {
    size_t sum = 0;
    void operator()(disruptor::Event& event, size_t seq, bool end_of_batch) {
        sum += event.get_value();
        do_not_optimize(sum);
    }
};

// ============================================================================
// 1P-1C benchmark (SingleProducerSequencer)
// ============================================================================

double bench_1p1c_single_run(const size_t num_events) {
    constexpr size_t BUFFER_SIZE = 1024;

    disruptor::RingBuffer<disruptor::Event, BUFFER_SIZE> ring_buffer(
        []() { return disruptor::Event(); });

    disruptor::SingleProducerSequencer<disruptor::Event, BUFFER_SIZE, 1> sequencer(ring_buffer);
    auto cursor_ref = std::ref(sequencer.get_cursor());

    // Consumer
    disruptor::ProcessingSequenceBarrier<WaitStrategyType::ADAPTIVE, 1, decltype(sequencer)> barrier(
        true, {cursor_ref}, sequencer);

    NoOpHandler handler;
    disruptor::BatchEventProcessor processor(barrier, handler, ring_buffer);

    const size_t expected_final_sequence = BUFFER_SIZE + num_events;
    sequencer.add_gating_sequences({std::ref(processor.get_cursor())});
    std::thread consumer_thread([&processor]() {
        maybe_pin_core(1);
        processor.run();
    });

    // Pin producer to physical P-core 0
    maybe_pin_core(0);
    std::this_thread::sleep_for(std::chrono::milliseconds(5));

    // Produce
    auto start = std::chrono::high_resolution_clock::now();

    for (size_t i = 0; i < num_events; ++i) {
        const size_t seq = sequencer.next(1);
        disruptor::Event& event = ring_buffer.get(seq);
        event.set_value(seq);
        sequencer.publish(seq);
    }

    // Wait for consumer to finish — check cursor instead of atomic counter
    while (processor.get_cursor().get() < expected_final_sequence) {
        std::this_thread::yield();
    }

    auto end = std::chrono::high_resolution_clock::now();

    processor.halt();
    consumer_thread.join();

    const double seconds = std::chrono::duration<double>(end - start).count();
    return static_cast<double>(num_events) / seconds;
}

BenchmarkResult bench_1p1c(const size_t num_events, const int warmup_runs, const int measured_runs) {
    for (int i = 0; i < warmup_runs; ++i) {
        bench_1p1c_single_run(num_events);
    }

    std::vector<double> samples;
    samples.reserve(measured_runs);
    for (int i = 0; i < measured_runs; ++i) {
        samples.push_back(bench_1p1c_single_run(num_events));
    }

    return compute_stats(samples);
}

// ============================================================================
// NP-1C benchmark (MultiProducerSequencer)
// ============================================================================

double bench_np1c_single_run(const size_t num_producers, const size_t events_per_producer) {
    constexpr size_t BUFFER_SIZE = 1 << 15; // 32768

    disruptor::RingBuffer<disruptor::Event, BUFFER_SIZE> ring_buffer(
        []() { return disruptor::Event(); });

    disruptor::MultiProducerSequencer<disruptor::Event, BUFFER_SIZE, 1> sequencer(ring_buffer);
    auto cursor_ref = std::ref(sequencer.get_cursor());

    const size_t total_events = num_producers * events_per_producer;

    // Consumer
    disruptor::ProcessingSequenceBarrier<WaitStrategyType::ADAPTIVE, 1, decltype(sequencer)> barrier(
        true, {cursor_ref}, sequencer);

    NoOpHandler handler;
    disruptor::BatchEventProcessor processor(barrier, handler, ring_buffer);

    const size_t expected_final_sequence = BUFFER_SIZE + total_events;
    sequencer.add_gating_sequences({std::ref(processor.get_cursor())});
    std::thread consumer_thread([&processor]() {
        maybe_pin_core(0);
        processor.run();
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(5));

    // Producers
    auto start = std::chrono::high_resolution_clock::now();

    std::vector<std::thread> producers;
    producers.reserve(num_producers);
    for (size_t p = 0; p < num_producers; ++p) {
        producers.emplace_back([&sequencer, &ring_buffer, events_per_producer, p]() {
            maybe_pin_core(static_cast<int>(p + 1));
            for (size_t i = 0; i < events_per_producer; ++i) {
                const size_t seq = sequencer.next(1);
                disruptor::Event& event = ring_buffer.get(seq);
                event.set_value(seq);
                sequencer.publish(seq);
            }
        });
    }

    for (auto& t : producers) {
        t.join();
    }

    // Wait for consumer to finish — check cursor instead of atomic counter
    while (processor.get_cursor().get() < expected_final_sequence) {
        std::this_thread::yield();
    }

    auto end = std::chrono::high_resolution_clock::now();

    processor.halt();
    consumer_thread.join();

    const double seconds = std::chrono::duration<double>(end - start).count();
    return static_cast<double>(total_events) / seconds;
}

BenchmarkResult bench_np1c(const size_t num_producers, const size_t events_per_producer,
                           const int warmup_runs, const int measured_runs) {
    for (int i = 0; i < warmup_runs; ++i) {
        bench_np1c_single_run(num_producers, events_per_producer);
    }

    std::vector<double> samples;
    samples.reserve(measured_runs);
    for (int i = 0; i < measured_runs; ++i) {
        samples.push_back(bench_np1c_single_run(num_producers, events_per_producer));
    }

    return compute_stats(samples);
}

// ============================================================================
// 1P-NC benchmark (SingleProducerSequencer, multiple independent consumers)
// ============================================================================

template<size_t NUM_CONSUMERS>
double bench_1pnc_single_run(const size_t num_events) {
    constexpr size_t BUFFER_SIZE = 1024;

    disruptor::RingBuffer<disruptor::Event, BUFFER_SIZE> ring_buffer(
        []() { return disruptor::Event(); });

    disruptor::SingleProducerSequencer<disruptor::Event, BUFFER_SIZE, NUM_CONSUMERS> sequencer(ring_buffer);
    auto cursor_ref = std::ref(sequencer.get_cursor());

    using SequencerType = disruptor::SingleProducerSequencer<disruptor::Event, BUFFER_SIZE, NUM_CONSUMERS>;
    using Barrier = disruptor::ProcessingSequenceBarrier<WaitStrategyType::ADAPTIVE, 1, SequencerType>;
    using Processor = disruptor::BatchEventProcessor<disruptor::Event, BUFFER_SIZE, NoOpHandler, Barrier>;

    struct ConsumerState {
        std::unique_ptr<Barrier> barrier;
        std::unique_ptr<Processor> processor;
        std::thread thread;
    };

    std::array<ConsumerState, NUM_CONSUMERS> consumers;

    for (size_t i = 0; i < NUM_CONSUMERS; ++i) {
        consumers[i].barrier = std::make_unique<Barrier>(
            true,
            std::initializer_list<std::reference_wrapper<disruptor::Sequence>>{cursor_ref},
            sequencer);

        NoOpHandler handler;
        consumers[i].processor = std::make_unique<Processor>(
            *consumers[i].barrier, handler, ring_buffer);
    }

    const size_t expected_final_sequence = BUFFER_SIZE + num_events;

    // Add all consumer cursors as gating sequences
    [&]<size_t... Is>(std::index_sequence<Is...>) {
        sequencer.add_gating_sequences({std::ref(consumers[Is].processor->get_cursor())...});
    }(std::make_index_sequence<NUM_CONSUMERS>{});

    for (size_t i = 0; i < NUM_CONSUMERS; ++i) {
        consumers[i].thread = std::thread([&proc = *consumers[i].processor, i]() {
            maybe_pin_core(static_cast<int>(i + 1));
            proc.run();
        });
    }

    // Pin producer to physical P-core 0
    maybe_pin_core(0);
    std::this_thread::sleep_for(std::chrono::milliseconds(5));

    // Produce
    auto start = std::chrono::high_resolution_clock::now();

    for (size_t i = 0; i < num_events; ++i) {
        const size_t seq = sequencer.next(1);
        disruptor::Event& event = ring_buffer.get(seq);
        event.set_value(seq);
        sequencer.publish(seq);
    }

    // Wait for ALL consumers to finish — check cursors instead of atomic counters
    for (size_t i = 0; i < NUM_CONSUMERS; ++i) {
        while (consumers[i].processor->get_cursor().get() < expected_final_sequence) {
            std::this_thread::yield();
        }
    }

    auto end = std::chrono::high_resolution_clock::now();

    for (size_t i = 0; i < NUM_CONSUMERS; ++i) {
        consumers[i].processor->halt();
        consumers[i].thread.join();
    }

    const double seconds = std::chrono::duration<double>(end - start).count();
    return static_cast<double>(num_events) / seconds;
}

template<size_t NUM_CONSUMERS>
BenchmarkResult bench_1pnc(const size_t num_events, const int warmup_runs, const int measured_runs) {
    for (int i = 0; i < warmup_runs; ++i) {
        bench_1pnc_single_run<NUM_CONSUMERS>(num_events);
    }

    std::vector<double> samples;
    samples.reserve(measured_runs);
    for (int i = 0; i < measured_runs; ++i) {
        samples.push_back(bench_1pnc_single_run<NUM_CONSUMERS>(num_events));
    }

    return compute_stats(samples);
}

// ============================================================================
// Main
// ============================================================================

int main() {
    disruptor::Util::require_for_system_run_stable();

    constexpr size_t SP_EVENTS = 50'000'000;
    constexpr size_t MP_EVENTS_PER_PRODUCER = 10'000'000;
    constexpr int WARMUP = 2;
    constexpr int RUNS = 5;

    std::cout << "================================================================" << std::endl;
    std::cout << "  Disruptor C++ Benchmark" << std::endl;
    std::cout << "  SP events: " << SP_EVENTS << ", MP events/producer: " << MP_EVENTS_PER_PRODUCER << std::endl;
    std::cout << "  Warmup: " << WARMUP << ", Measured runs: " << RUNS << std::endl;
    std::cout << "================================================================" << std::endl;
    std::cout << std::endl;

    // 1P-1C
    std::cout << "[1P-1C] SingleProducerSequencer..." << std::endl;
    auto r1p1c = bench_1p1c(SP_EVENTS, WARMUP, RUNS);
    print_result("1P-1C", r1p1c);
    std::cout << std::endl;

    // 1P-NC (single producer, multiple consumers)
    constexpr size_t SP_MC_EVENTS = 50'000'000;

    auto run_1pnc = [&]<size_t N>(std::integral_constant<size_t, N>) {
        char label[32];
        std::snprintf(label, sizeof(label), "1P-%zuC", N);
        std::cout << "[" << label << "] SingleProducerSequencer..." << std::endl;
        auto r = bench_1pnc<N>(SP_MC_EVENTS, WARMUP, RUNS);
        print_result(label, r);
        std::cout << std::endl;
    };

    run_1pnc(std::integral_constant<size_t, 2>{});
    run_1pnc(std::integral_constant<size_t, 3>{});
    run_1pnc(std::integral_constant<size_t, 4>{});
    run_1pnc(std::integral_constant<size_t, 5>{});

    // NP-1C (multi-producer)
    for (size_t np : {2, 3, 5}) {
        std::cout << "[" << np << "P-1C] MultiProducerSequencer..." << std::endl;
        auto r = bench_np1c(np, MP_EVENTS_PER_PRODUCER, WARMUP, RUNS);
        char name[32];
        std::snprintf(name, sizeof(name), "%zuP-1C", np);
        print_result(name, r);
        std::cout << std::endl;
    }

    // ================================================================
    // LMAX-comparable benchmarks (same buffer sizes, handler with work)
    // ================================================================
    std::cout << "================================================================" << std::endl;
    std::cout << "  LMAX-comparable (ValueAddition handler, LMAX buffer sizes)" << std::endl;
    std::cout << "================================================================" << std::endl;

    // 1P-1C: LMAX uses BUFFER_SIZE=64K, 100M events, YieldingWaitStrategy
    {
        constexpr size_t BUF = 1024 * 64;
        constexpr size_t EVENTS = 100'000'000;

        auto run_once = [&]() -> double {
            disruptor::RingBuffer<disruptor::Event, BUF> rb([]() { return disruptor::Event(); });
            disruptor::SingleProducerSequencer<disruptor::Event, BUF, 1> seq(rb);
            auto cursor_ref = std::ref(seq.get_cursor());

            disruptor::ProcessingSequenceBarrier<WaitStrategyType::ADAPTIVE, 1, decltype(seq)> barrier(
                true, {cursor_ref}, seq);
            ValueAdditionHandler handler;
            disruptor::BatchEventProcessor processor(barrier, handler, rb);

            const size_t expected = BUF + EVENTS;
            seq.add_gating_sequences({std::ref(processor.get_cursor())});
            std::thread consumer([&processor]() { maybe_pin_core(1); processor.run(); });
            maybe_pin_core(0);
            std::this_thread::sleep_for(std::chrono::milliseconds(5));

            auto start = std::chrono::high_resolution_clock::now();
            for (size_t i = 0; i < EVENTS; ++i) {
                const size_t s = seq.next(1);
                rb.get(s).set_value(s);
                seq.publish(s);
            }
            while (processor.get_cursor().get() < expected) std::this_thread::yield();
            auto end = std::chrono::high_resolution_clock::now();

            processor.halt();
            consumer.join();
            return static_cast<double>(EVENTS) / std::chrono::duration<double>(end - start).count();
        };

        for (int i = 0; i < WARMUP; ++i) run_once();
        std::vector<double> samples;
        for (int i = 0; i < RUNS; ++i) samples.push_back(run_once());
        print_result("1P-1C (LMAX-eq)", compute_stats(samples));
    }

    // 3P-1C: LMAX uses BUFFER_SIZE=64K, 20M events total, BusySpinWaitStrategy
    {
        constexpr size_t BUF = 1024 * 64;
        constexpr size_t NUM_P = 3;
        constexpr size_t EVENTS_PER_P = 20'000'000 / NUM_P;
        constexpr size_t TOTAL = NUM_P * EVENTS_PER_P;

        using RB = disruptor::RingBuffer<disruptor::Event, BUF>;
        using Seq = disruptor::MultiProducerSequencer<disruptor::Event, BUF, 1>;
        using Bar = disruptor::ProcessingSequenceBarrier<WaitStrategyType::ADAPTIVE, 1, Seq>;

        auto run_once = [&]() -> double {
            auto rb = std::make_unique<RB>([]() { return disruptor::Event(); });
            auto seq = std::make_unique<Seq>(*rb);
            auto cursor_ref = std::ref(seq->get_cursor());

            Bar barrier(true, {cursor_ref}, *seq);
            ValueAdditionHandler handler;
            disruptor::BatchEventProcessor processor(barrier, handler, *rb);

            const size_t expected = BUF + TOTAL;
            seq->add_gating_sequences({std::ref(processor.get_cursor())});
            std::thread consumer([&processor]() { maybe_pin_core(0); processor.run(); });
            std::this_thread::sleep_for(std::chrono::milliseconds(5));

            auto start = std::chrono::high_resolution_clock::now();
            std::vector<std::thread> producers;
            for (size_t p = 0; p < NUM_P; ++p) {
                producers.emplace_back([&seq, &rb, p]() {
                    maybe_pin_core(static_cast<int>(p + 1));
                    for (size_t i = 0; i < EVENTS_PER_P; ++i) {
                        const size_t s = seq->next(1);
                        rb->get(s).set_value(s);
                        seq->publish(s);
                    }
                });
            }
            for (auto& t : producers) t.join();
            while (processor.get_cursor().get() < expected) std::this_thread::yield();
            auto end = std::chrono::high_resolution_clock::now();

            processor.halt();
            consumer.join();
            return static_cast<double>(TOTAL) / std::chrono::duration<double>(end - start).count();
        };

        for (int i = 0; i < WARMUP; ++i) run_once();
        std::vector<double> samples;
        for (int i = 0; i < RUNS; ++i) samples.push_back(run_once());
        print_result("3P-1C (LMAX-eq)", compute_stats(samples));
    }

    // 1P-3C: LMAX uses BUFFER_SIZE=8K, 100M events
    {
        constexpr size_t BUF = 1024 * 8;
        constexpr size_t EVENTS = 100'000'000;
        constexpr size_t NC = 3;

        using SeqT = disruptor::SingleProducerSequencer<disruptor::Event, BUF, NC>;
        using BarT = disruptor::ProcessingSequenceBarrier<WaitStrategyType::ADAPTIVE, 1, SeqT>;
        using ProcT = disruptor::BatchEventProcessor<disruptor::Event, BUF, ValueAdditionHandler, BarT>;

        struct CS { std::unique_ptr<BarT> barrier; std::unique_ptr<ProcT> processor; std::thread thread; };

        auto run_once = [&]() -> double {
            disruptor::RingBuffer<disruptor::Event, BUF> rb([]() { return disruptor::Event(); });
            SeqT seq(rb);
            auto cursor_ref = std::ref(seq.get_cursor());

            std::array<CS, NC> consumers;
            for (size_t i = 0; i < NC; ++i) {
                consumers[i].barrier = std::make_unique<BarT>(
                    true, std::initializer_list<std::reference_wrapper<disruptor::Sequence>>{cursor_ref}, seq);
                ValueAdditionHandler h;
                consumers[i].processor = std::make_unique<ProcT>(*consumers[i].barrier, h, rb);
            }

            const size_t expected = BUF + EVENTS;
            [&]<size_t... Is>(std::index_sequence<Is...>) {
                seq.add_gating_sequences({std::ref(consumers[Is].processor->get_cursor())...});
            }(std::make_index_sequence<NC>{});

            for (size_t i = 0; i < NC; ++i) {
                consumers[i].thread = std::thread([&proc = *consumers[i].processor, i]() {
                    maybe_pin_core(static_cast<int>(i + 1));
                    proc.run();
                });
            }
            maybe_pin_core(0);
            std::this_thread::sleep_for(std::chrono::milliseconds(5));

            auto start = std::chrono::high_resolution_clock::now();
            for (size_t i = 0; i < EVENTS; ++i) {
                const size_t s = seq.next(1);
                rb.get(s).set_value(s);
                seq.publish(s);
            }
            for (size_t i = 0; i < NC; ++i) {
                while (consumers[i].processor->get_cursor().get() < expected) std::this_thread::yield();
            }
            auto end = std::chrono::high_resolution_clock::now();

            for (size_t i = 0; i < NC; ++i) { consumers[i].processor->halt(); consumers[i].thread.join(); }
            return static_cast<double>(EVENTS) / std::chrono::duration<double>(end - start).count();
        };

        for (int i = 0; i < WARMUP; ++i) run_once();
        std::vector<double> samples;
        for (int i = 0; i < RUNS; ++i) samples.push_back(run_once());
        print_result("1P-3C (LMAX-eq)", compute_stats(samples));
    }

    // ================================================================
    // BusySpin strategy comparison (fair comparison with SkynetNext)
    // ================================================================
    std::cout << "================================================================" << std::endl;
    std::cout << "  BusySpin strategy (fair comparison with SkynetNext)" << std::endl;
    std::cout << "================================================================" << std::endl;

    // 1P-1C BusySpin: same as LMAX-eq but with BusySpinWaitStrategy
    {
        constexpr size_t BUF = 1024 * 64;
        constexpr size_t EVENTS = 100'000'000;

        auto run_once = [&]() -> double {
            disruptor::RingBuffer<disruptor::Event, BUF> rb([]() { return disruptor::Event(); });
            disruptor::SingleProducerSequencer<disruptor::Event, BUF, 1> seq(rb);
            auto cursor_ref = std::ref(seq.get_cursor());

            disruptor::ProcessingSequenceBarrier<WaitStrategyType::BUSY_SPIN, 1, decltype(seq)> barrier(
                true, {cursor_ref}, seq);
            ValueAdditionHandler handler;
            disruptor::BatchEventProcessor processor(barrier, handler, rb);

            const size_t expected = BUF + EVENTS;
            seq.add_gating_sequences({std::ref(processor.get_cursor())});
            std::thread consumer([&processor]() { maybe_pin_core(1); processor.run(); });
            maybe_pin_core(0);
            std::this_thread::sleep_for(std::chrono::milliseconds(5));

            auto start = std::chrono::high_resolution_clock::now();
            for (size_t i = 0; i < EVENTS; ++i) {
                const size_t s = seq.next(1);
                rb.get(s).set_value(s);
                seq.publish(s);
            }
            auto producer_done = std::chrono::high_resolution_clock::now();
            while (processor.get_cursor().get() < expected) std::this_thread::yield();
            auto end = std::chrono::high_resolution_clock::now();

            processor.halt();
            consumer.join();

            auto prod_ms = std::chrono::duration<double, std::milli>(producer_done - start).count();
            auto wait_ms = std::chrono::duration<double, std::milli>(end - producer_done).count();
            auto total_ms = std::chrono::duration<double, std::milli>(end - start).count();
            printf("    [detail] producer=%.0fms consumer_wait=%.0fms total=%.0fms\n", prod_ms, wait_ms, total_ms);
            return static_cast<double>(EVENTS) / std::chrono::duration<double>(end - start).count();
        };

        for (int i = 0; i < WARMUP; ++i) run_once();
        std::vector<double> samples;
        for (int i = 0; i < RUNS; ++i) samples.push_back(run_once());
        print_result("1P-1C (BusySpin)", compute_stats(samples));
    }

    // Producer-only speed test (no event write, minimal consumer)
    {
        constexpr size_t BUF = 1024 * 64;
        constexpr size_t EVENTS = 100'000'000;

        auto run_once = [&]() -> double {
            disruptor::RingBuffer<disruptor::Event, BUF> rb([]() { return disruptor::Event(); });
            disruptor::SingleProducerSequencer<disruptor::Event, BUF, 1> seq(rb);
            auto cursor_ref = std::ref(seq.get_cursor());

            disruptor::ProcessingSequenceBarrier<WaitStrategyType::BUSY_SPIN, 1, decltype(seq)> barrier(
                true, {cursor_ref}, seq);
            NoOpHandler handler;
            disruptor::BatchEventProcessor processor(barrier, handler, rb);

            const size_t expected = BUF + EVENTS;
            seq.add_gating_sequences({std::ref(processor.get_cursor())});
            std::thread consumer([&processor]() { maybe_pin_core(1); processor.run(); });
            maybe_pin_core(0);
            std::this_thread::sleep_for(std::chrono::milliseconds(5));

            auto start = std::chrono::high_resolution_clock::now();
            for (size_t i = 0; i < EVENTS; ++i) {
                const size_t s = seq.next(1);
                // NO set_value — skip ring buffer write
                seq.publish(s);
            }
            while (processor.get_cursor().get() < expected) std::this_thread::yield();
            auto end = std::chrono::high_resolution_clock::now();

            processor.halt();
            consumer.join();
            return static_cast<double>(EVENTS) / std::chrono::duration<double>(end - start).count();
        };

        for (int i = 0; i < WARMUP; ++i) run_once();
        std::vector<double> samples;
        for (int i = 0; i < RUNS; ++i) samples.push_back(run_once());
        print_result("1P-1C (NoWrite 64K)", compute_stats(samples));
    }

    // 1P-1C BusySpin with NoOp handler (raw throughput)
    {
        constexpr size_t BUF = 1024;
        constexpr size_t EVENTS = 50'000'000;

        auto run_once = [&]() -> double {
            disruptor::RingBuffer<disruptor::Event, BUF> rb([]() { return disruptor::Event(); });
            disruptor::SingleProducerSequencer<disruptor::Event, BUF, 1> seq(rb);
            auto cursor_ref = std::ref(seq.get_cursor());

            disruptor::ProcessingSequenceBarrier<WaitStrategyType::BUSY_SPIN, 1, decltype(seq)> barrier(
                true, {cursor_ref}, seq);
            NoOpHandler handler;
            disruptor::BatchEventProcessor processor(barrier, handler, rb);

            const size_t expected = BUF + EVENTS;
            seq.add_gating_sequences({std::ref(processor.get_cursor())});
            std::thread consumer([&processor]() { maybe_pin_core(1); processor.run(); });
            maybe_pin_core(0);
            std::this_thread::sleep_for(std::chrono::milliseconds(5));

            auto start = std::chrono::high_resolution_clock::now();
            for (size_t i = 0; i < EVENTS; ++i) {
                const size_t s = seq.next(1);
                rb.get(s).set_value(s);
                seq.publish(s);
            }
            while (processor.get_cursor().get() < expected) std::this_thread::yield();
            auto end = std::chrono::high_resolution_clock::now();

            processor.halt();
            consumer.join();
            return static_cast<double>(EVENTS) / std::chrono::duration<double>(end - start).count();
        };

        for (int i = 0; i < WARMUP; ++i) run_once();
        std::vector<double> samples;
        for (int i = 0; i < RUNS; ++i) samples.push_back(run_once());
        print_result("1P-1C (BusySpin NoOp)", compute_stats(samples));
    }

    // 1P-1C BusySpin with NoOp but 64K buffer (isolate buffer size effect)
    {
        constexpr size_t BUF = 1024 * 64;
        constexpr size_t EVENTS = 100'000'000;

        auto run_once = [&]() -> double {
            disruptor::RingBuffer<disruptor::Event, BUF> rb([]() { return disruptor::Event(); });
            disruptor::SingleProducerSequencer<disruptor::Event, BUF, 1> seq(rb);
            auto cursor_ref = std::ref(seq.get_cursor());

            disruptor::ProcessingSequenceBarrier<WaitStrategyType::BUSY_SPIN, 1, decltype(seq)> barrier(
                true, {cursor_ref}, seq);
            NoOpHandler handler;
            disruptor::BatchEventProcessor processor(barrier, handler, rb);

            const size_t expected = BUF + EVENTS;
            seq.add_gating_sequences({std::ref(processor.get_cursor())});
            std::thread consumer([&processor]() { maybe_pin_core(1); processor.run(); });
            maybe_pin_core(0);
            std::this_thread::sleep_for(std::chrono::milliseconds(5));

            auto start = std::chrono::high_resolution_clock::now();
            for (size_t i = 0; i < EVENTS; ++i) {
                const size_t s = seq.next(1);
                rb.get(s).set_value(s);
                seq.publish(s);
            }
            while (processor.get_cursor().get() < expected) std::this_thread::yield();
            auto end = std::chrono::high_resolution_clock::now();

            processor.halt();
            consumer.join();
            return static_cast<double>(EVENTS) / std::chrono::duration<double>(end - start).count();
        };

        for (int i = 0; i < WARMUP; ++i) run_once();
        std::vector<double> samples;
        for (int i = 0; i < RUNS; ++i) samples.push_back(run_once());
        print_result("1P-1C (BusySpin NoOp 64K)", compute_stats(samples));
    }

    // 1P-1C BusySpin with ValueAdd but 1K buffer (isolate handler effect)
    {
        constexpr size_t BUF = 1024;
        constexpr size_t EVENTS = 100'000'000;

        auto run_once = [&]() -> double {
            disruptor::RingBuffer<disruptor::Event, BUF> rb([]() { return disruptor::Event(); });
            disruptor::SingleProducerSequencer<disruptor::Event, BUF, 1> seq(rb);
            auto cursor_ref = std::ref(seq.get_cursor());

            disruptor::ProcessingSequenceBarrier<WaitStrategyType::BUSY_SPIN, 1, decltype(seq)> barrier(
                true, {cursor_ref}, seq);
            ValueAdditionHandler handler;
            disruptor::BatchEventProcessor processor(barrier, handler, rb);

            const size_t expected = BUF + EVENTS;
            seq.add_gating_sequences({std::ref(processor.get_cursor())});
            std::thread consumer([&processor]() { maybe_pin_core(1); processor.run(); });
            maybe_pin_core(0);
            std::this_thread::sleep_for(std::chrono::milliseconds(5));

            auto start = std::chrono::high_resolution_clock::now();
            for (size_t i = 0; i < EVENTS; ++i) {
                const size_t s = seq.next(1);
                rb.get(s).set_value(s);
                seq.publish(s);
            }
            while (processor.get_cursor().get() < expected) std::this_thread::yield();
            auto end = std::chrono::high_resolution_clock::now();

            processor.halt();
            consumer.join();
            return static_cast<double>(EVENTS) / std::chrono::duration<double>(end - start).count();
        };

        for (int i = 0; i < WARMUP; ++i) run_once();
        std::vector<double> samples;
        for (int i = 0; i < RUNS; ++i) samples.push_back(run_once());
        print_result("1P-1C (BusySpin ValAdd 1K)", compute_stats(samples));
    }

    // 1P-1C Yield strategy with ValAdd 64K (match SkynetNext's PerfTest)
    {
        constexpr size_t BUF = 1024 * 64;
        constexpr size_t EVENTS = 100'000'000;

        auto run_once = [&]() -> double {
            disruptor::RingBuffer<disruptor::Event, BUF> rb([]() { return disruptor::Event(); });
            disruptor::SingleProducerSequencer<disruptor::Event, BUF, 1> seq(rb);
            auto cursor_ref = std::ref(seq.get_cursor());

            disruptor::ProcessingSequenceBarrier<WaitStrategyType::YIELD, 1, decltype(seq)> barrier(
                true, {cursor_ref}, seq);
            ValueAdditionHandler handler;
            disruptor::BatchEventProcessor processor(barrier, handler, rb);

            const size_t expected = BUF + EVENTS;
            seq.add_gating_sequences({std::ref(processor.get_cursor())});
            std::thread consumer([&processor]() { maybe_pin_core(1); processor.run(); });
            maybe_pin_core(0);
            std::this_thread::sleep_for(std::chrono::milliseconds(5));

            auto start = std::chrono::high_resolution_clock::now();
            for (size_t i = 0; i < EVENTS; ++i) {
                const size_t s = seq.next(1);
                rb.get(s).set_value(s);
                seq.publish(s);
            }
            while (processor.get_cursor().get() < expected) std::this_thread::yield();
            auto end = std::chrono::high_resolution_clock::now();

            processor.halt();
            consumer.join();
            return static_cast<double>(EVENTS) / std::chrono::duration<double>(end - start).count();
        };

        for (int i = 0; i < WARMUP; ++i) run_once();
        std::vector<double> samples;
        for (int i = 0; i < RUNS; ++i) samples.push_back(run_once());
        print_result("1P-1C (Yield ValAdd 64K)", compute_stats(samples));
    }

    // 1P-1C BusySpin with ValAdd+do_not_optimize handler (test compiler barrier effect)
    {
        constexpr size_t BUF = 1024;
        constexpr size_t EVENTS = 100'000'000;

        auto run_once = [&]() -> double {
            disruptor::RingBuffer<disruptor::Event, BUF> rb([]() { return disruptor::Event(); });
            disruptor::SingleProducerSequencer<disruptor::Event, BUF, 1> seq(rb);
            auto cursor_ref = std::ref(seq.get_cursor());

            disruptor::ProcessingSequenceBarrier<WaitStrategyType::BUSY_SPIN, 1, decltype(seq)> barrier(
                true, {cursor_ref}, seq);
            ValueAdditionDoNotOptHandler handler;
            disruptor::BatchEventProcessor processor(barrier, handler, rb);

            const size_t expected = BUF + EVENTS;
            seq.add_gating_sequences({std::ref(processor.get_cursor())});
            std::thread consumer([&processor]() { maybe_pin_core(1); processor.run(); });
            maybe_pin_core(0);
            std::this_thread::sleep_for(std::chrono::milliseconds(5));

            auto start = std::chrono::high_resolution_clock::now();
            for (size_t i = 0; i < EVENTS; ++i) {
                const size_t s = seq.next(1);
                rb.get(s).set_value(s);
                seq.publish(s);
            }
            while (processor.get_cursor().get() < expected) std::this_thread::yield();
            auto end = std::chrono::high_resolution_clock::now();

            processor.halt();
            consumer.join();
            return static_cast<double>(EVENTS) / std::chrono::duration<double>(end - start).count();
        };

        for (int i = 0; i < WARMUP; ++i) run_once();
        std::vector<double> samples;
        for (int i = 0; i < RUNS; ++i) samples.push_back(run_once());
        print_result("1P-1C (BusySpin ValAdd+DNO 1K)", compute_stats(samples));
    }

    std::cout << "================================================================" << std::endl;
    std::cout << "  Benchmark complete." << std::endl;
    std::cout << "================================================================" << std::endl;

    return 0;
}
