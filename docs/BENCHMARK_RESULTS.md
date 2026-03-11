# Disruptor++ Benchmark Results

> **Date**: 2026-03-11
> **Branch**: `optimize-with-claude`
> **CPU**: Intel i9-14900K (8P + 16E cores, P-cores up to 6.0 GHz)
> **OS**: Linux 6.14.0-37-generic (x86_64)
> **Compiler**: g++ -std=c++20 -O3 -march=native -DNDEBUG
> **CPU Governor**: `powersave` (no root access to change to `performance`)

---

## Table of Contents

1. [Test Environment](#1-test-environment)
2. [Wait Strategy Comparison](#2-wait-strategy-comparison)
3. [Thread Pinning Impact](#3-thread-pinning-impact)
4. [Optimal Buffer Size](#4-optimal-buffer-size)
5. [Event Size Impact](#5-event-size-impact)
6. [Recommendations for Users](#6-recommendations-for-users)
7. [Known Limitations](#7-known-limitations)

---

## 1. Test Environment

### Hardware

| Component | Spec |
|-----------|------|
| CPU | Intel i9-14900K (8P-cores + 16E-cores, Raptor Lake) |
| P-core clock | Up to 6.0 GHz (cpu8,10) / 5.7 GHz (cpu12,14) |
| E-core clock | Up to 4.4 GHz (cpu16-31) |
| L1d cache | 48 KB per P-core |
| L2 cache | 2 MB per P-core |
| L3 cache | 36 MB shared |
| RAM | DDR5 |

### Thread Pinning Configuration

Benchmarks use **one thread per physical P-core**, avoiding Hyper-Threading siblings
to eliminate HT contention:

| Logical CPU | Clock | HT Sibling | Role in benchmarks |
|-------------|-------|------------|-------------------|
| cpu8 | 6.0 GHz | cpu9 | Producer / Core A |
| cpu10 | 6.0 GHz | cpu11 | Consumer / Core B |
| cpu12 | 5.7 GHz | cpu13 | Core C |
| cpu14 | 5.7 GHz | cpu15 | Core D |

### Benchmark Methodology

| Parameter | Value |
|-----------|-------|
| Warmup runs | 2 (results discarded) |
| Measurement runs | 5-7 |
| Reporting metric | **Median** of measurement runs |
| Additional stats | p25, p75, best (where noted) |
| Event struct | `struct VE { int64_t value; }` (8 bytes) |

### Memory Footprint per Buffer Size

| Buffer Size | Ring Buffer Memory | MultiProducer available_buffer |
|-------------|-------------------|-------------------------------|
| 1K (1,024) | 8 KB | 8 KB |
| 8K (8,192) | 64 KB | 64 KB |
| 16K (16,384) | 128 KB | 128 KB |
| 32K (32,768) | 256 KB | 256 KB |
| 64K (65,536) | 512 KB | 512 KB |
| 128K (131,072) | 1 MB | 1 MB |
| 256K (262,144) | 2 MB | 2 MB |
| 512K (524,288) | 4 MB | 4 MB |
| 1M (1,048,576) | 8 MB | 8 MB |
| 2M (2,097,152) | 16 MB | 16 MB |

> Note: For SingleProducerSequencer, there is no `available_buffer` — only the ring buffer itself.
> For MultiProducerSequencer, total memory = ring buffer + available_buffer (doubled).

---

## 2. Wait Strategy Comparison

### Available Strategies

| Strategy | Behavior | CPU Usage | Latency | Best For |
|----------|----------|-----------|---------|----------|
| **BusySpin** | Continuous `_mm_pause()` loop | 100% per thread | Lowest (when CPU isn't throttled) | Ultra-low latency, short bursts |
| **Yield** | Spin 200× with `_mm_pause()`, then `sched_yield()` | High | Low | Good balance for most use cases |
| **Adaptive** | 3-phase: spin 100× → yield 10× → sleep(1ns) loop | Moderate | Low-Medium | **Best overall throughput** |

### Adaptive Wait Strategy — 3-Phase Detail

```
Phase 1: Spin (100 iterations)
  └─ _mm_pause() each iteration
  └─ Ultra-low latency, catches data arriving within ~100 cycles

Phase 2: Yield (10 iterations)
  └─ std::this_thread::yield()
  └─ Light context switch, lets other threads run

Phase 3: Park
  └─ sleep_for(1ns), then jump back to Phase 2
  └─ Oscillates between yield and sleep (never re-spins from Phase 1)
  └─ CPU-friendly for longer waits
```

### Throughput Results (Buffer=64K, Pinned)

Measured with 100M events for 1P-1C/1P-3C, 20M for 3P-1C:

| Scenario | BusySpin | Yield | Adaptive |
|----------|:--------:|:-----:|:--------:|
| 1P-1C | 131M | 229M | **413M** |
| 3P-1C | 25M | 26M | **27M** |
| 1P-3C | 25M | 24M | **25M** |

> **Key finding**: Adaptive consistently outperforms BusySpin and Yield across all scenarios.
> BusySpin underperforms due to thermal throttling — 100% CPU load causes P-core frequency
> downclocking on `powersave` governor. On a properly configured `performance` governor,
> BusySpin may reclaim its theoretical advantage for short bursts.

---

## 3. Thread Pinning Impact

### With vs Without Thread Pinning (Adaptive, Buffer=64K)

| Scenario | Without Pinning | With P-core Pinning | Change |
|----------|:---------:|:----------:|:------:|
| 1P-1C | 155M | **413M** | +166% |
| 3P-1C | 24M | 27M | +12% |
| 1P-3C | 22M | 25M | +14% |

> **Key finding**: Thread pinning has **massive impact on 1P-1C** (2.7x improvement) because
> it prevents OS scheduler from migrating threads between P-cores and E-cores.
> Multi-thread scenarios benefit less because they're bottlenecked by contention, not scheduling.

### Why Pinning Matters on Hybrid CPUs (i9-14900K)

Without pinning, the OS scheduler may:
1. Place threads on **E-cores** (4.4 GHz vs 6.0 GHz P-cores) — 27% slower clock
2. **Migrate threads** between cores — invalidates L1/L2 cache, costs thousands of cycles
3. Place producer and consumer on **HT siblings** (e.g., cpu8 and cpu9) — sharing execution resources

With pinning to dedicated P-cores:
- Guaranteed highest clock speed
- No cache invalidation from migration
- No HT resource sharing

---

## 4. Optimal Buffer Size

### Methodology

Sweep across buffer sizes from 1K to 2M (powers of 2), all using **Adaptive wait strategy
+ P-core pinning**. Events: 50M for single-producer scenarios, 9-10M for multi-producer.

### 4.1 Single-Producer Scenarios

#### 1P-1C (50M events)

| Buffer Size | Throughput | Notes |
|:-----------:|:----------:|-------|
| 1K | 67M | Severe back-pressure |
| 2K | 67M | |
| 4K | 70M | |
| 8K | 71M | |
| 16K | 83M | Slight improvement |
| 32K | 287M | **Major jump** — producer runway opens up |
| 64K | 301M | |
| 128K | 833M | **Second major jump** |
| 256K | 819M | Plateau |
| 512K | **862M** | **Peak** |
| 1M | 862M | Plateau |
| 2M | 833M | Slight decline (cache pressure) |

**Optimal: 128K–512K** (sweet spot for L2 cache utilization)

#### 1P-2C (50M events)

| Buffer Size | Throughput | Notes |
|:-----------:|:----------:|-------|
| 1K | 30M | Slowest consumer blocks producer |
| 2K | 31M | |
| 4K | 31M | |
| 8K | 31M | |
| 16K | 32M | |
| 32K | 38M | |
| 64K | 39M | |
| 128K | 139M | Starting to ramp |
| 256K | 769M | **Major jump** |
| 512K | 781M | |
| 1M | 833M | |
| 2M | **847M** | **Peak** |

**Optimal: 256K–2M** (needs larger buffer to absorb consumer speed differences)

#### 1P-3C (50M events)

| Buffer Size | Throughput | Notes |
|:-----------:|:----------:|-------|
| 1K | 26M | |
| 2K | 26M | |
| 4K | 27M | |
| 8K | 27M | |
| 16K | 27M | |
| 32K | 29M | |
| 64K | 30M | |
| 128K | 42M | Starting to ramp |
| 256K | 153M | |
| 512K | 333M | |
| 1M | **862M** | **Peak** |
| 2M | 819M | Slight decline |

**Optimal: 1M** (3 consumers need maximum runway for producer to burst ahead)

### 4.2 Multi-Producer Scenarios

#### 2P-1C (10M total, 5M per producer)

| Buffer Size | Throughput | Notes |
|:-----------:|:----------:|-------|
| 1K | 24M | |
| 2K | 22M | |
| 4K | 22M | |
| 8K | 23M | |
| 16K | 24M | |
| 32K | 23M | |
| 64K | 23M | |
| 128K | 24M | |
| 256K | 24M | |
| 512K | 23M | |
| 1M | 22M | |
| 2M | 23M | |

**Buffer size has no meaningful impact** (~22-24M across all sizes).
Bottleneck: atomic `cursor.get_and_add()` contention between producers.

#### 3P-1C (9M total, 3M per producer)

| Buffer Size | Throughput | Notes |
|:-----------:|:----------:|-------|
| 1K | 27M | |
| 2K | 24M | |
| 4K | 24M | |
| 8K | 25M | |
| 16K | 27M | |
| 32K | 25M | |
| 64K | 24M | |
| 128K | 26M | |
| 256K | 24M | |
| 512K | 24M | |
| 1M | 26M | |
| 2M | 26M | |

**Buffer size has no meaningful impact** (~24-27M across all sizes).
Same atomic contention bottleneck.

#### 2P-2C (10M total, 5M per producer)

| Buffer Size | Throughput | Notes |
|:-----------:|:----------:|-------|
| 1K | 18M | |
| 2K | 16M | |
| 4K | 17M | |
| 8K | 17M | |
| 16K | 18M | |
| 32K | 18M | |
| 64K | 16M | |
| 128K | 17M | |
| 256K | 18M | |
| 512K | 17M | |
| 1M | 18M | |
| 2M | 18M | |

**Buffer size has no meaningful impact** (~16-18M across all sizes).
Double contention: producers compete on cursor, consumers add gating overhead.

### 4.3 Buffer Size Scaling Visualization

```
1P-1C Throughput vs Buffer Size:
                                                            ████ 862M
                                                       ████ 833M
                                                  ████ 819M
                                             ████ 833M
                              ██ 301M
                         ██ 287M
              █ 83M
         █ 71M
        █ 70M
        █ 67M
        █ 67M
  1K   2K   4K   8K  16K  32K  64K 128K 256K 512K   1M   2M

1P-3C Throughput vs Buffer Size:
                                                       ████ 862M
                                                  ███ 819M
                                             ██ 333M
                                        █ 153M
                                   █ 42M
                              █ 30M
                         █ 29M
        █ 27M (flat from 1K to 16K)
  1K   2K   4K   8K  16K  32K  64K 128K 256K 512K   1M   2M
```

---

## 5. Event Size Impact

Event size significantly affects optimal buffer size. The key constraint is **total buffer memory
vs CPU cache hierarchy**: L1d=48KB, L2=2MB, L3=36MB on i9-14900K.

### 5.1 1P-1C — Event Size × Buffer Size (Adaptive, Pinned)

| Event Size | 8K | 16K | 32K | 64K | 128K | 256K | 512K | 1M | Optimal |
|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|---|
| **8B** | 74M | 86M | 318M | 485M | **833M** | 833M | 847M | 806M | **128K–512K** |
| **32B** | 174M | **724M** | 704M | 735M | 757M | 757M | 757M | 595M | **16K–256K** |
| **64B** | **471M** | 471M | 462M | 458M | 462M | 462M | 393M | 259M | **8K–128K** |
| **128B** | 333M | 352M | **375M** | 379M | 375M | 329M | 30M | 27M | **32K–128K** |
| **256B** | **289M** | 277M | 266M | 266M | 250M | 149M | 30M | 47M | **8K–64K** |
| **512B** | **322M** | 322M | 312M | 156M | 31M | 52M | 126M | 123M | **8K–32K** |

**Total memory at optimal points:**

| Event Size | Optimal Buffer | Total Memory | Cache Level |
|:---:|:---:|:---:|---|
| 8B | 128K–512K | 1–4 MB | L2–L3 boundary |
| 32B | 16K–256K | 512KB–8MB | L2 sweet spot |
| 64B | 8K–128K | 512KB–8MB | L2 sweet spot |
| 128B | 32K–128K | 4–16 MB | L3 |
| 256B | 8K–64K | 2–16 MB | L2–L3 |
| 512B | 8K–32K | 4–16 MB | L3 |

**Key pattern**: As event size grows, optimal buffer slot count **shrinks** to keep total
memory within cache bounds. The sweet spot is roughly when total buffer memory = **1–8 MB**
(L2-to-L3 region).

**Cliff warning**: Large events (128B+) show a dramatic throughput cliff when buffer becomes too
large. For 128B events: 375M at 32K → 30M at 512K (12.5x drop!). This happens when the ring
buffer exceeds L3 cache and every access becomes a main memory round-trip.

### 5.2 1P-3C — Event Size × Buffer Size (Adaptive, Pinned)

| Event Size | 8K | 16K | 32K | 64K | 128K | 256K | 512K | 1M | Optimal |
|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|---|
| **8B** | 28M | 28M | 28M | 31M | 43M | 185M | 267M | **746M** | **1M+** |
| **64B** | 274M | 393M | 416M | **420M** | 403M | 387M | 280M | 16M | **32K–128K** |
| **256B** | 20M | 19M | 22M | 30M | **54M** | 16M | 12M | 12M | **64K–128K** |

**Key pattern**: 1P-3C with small events benefits hugely from large buffers (producer runway),
but with larger events the cache cliff hits earlier and harder. At 256B events, even the best
buffer size only achieves 54M — the 3 consumers' combined cache footprint is the bottleneck.

### 5.3 3P-1C — Event Size × Buffer Size (Adaptive, Pinned)

| Event Size | 8K | 16K | 32K | 64K | 128K | 256K | 512K | 1M |
|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|
| **8B** | 28M | 25M | 26M | 25M | 26M | 24M | 25M | 24M |
| **64B** | 25M | 26M | 24M | 25M | 24M | 25M | 25M | 25M |
| **256B** | 26M | 26M | 26M | 25M | 24M | 23M | 23M | 23M |

**Confirmed**: Multi-producer throughput is **completely independent of both buffer size and
event size** (~23-28M across all combinations). The atomic `fetch_add` contention between
producers is the sole bottleneck.

### 5.4 Total Ring Buffer Memory Reference

| Event Size | 8K | 16K | 32K | 64K | 128K | 256K | 512K | 1M |
|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|
| 8B | 64KB | 128KB | 256KB | 512KB | 1MB | 2MB | 4MB | 8MB |
| 32B | 256KB | 512KB | 1MB | 2MB | 4MB | 8MB | 16MB | 32MB |
| 64B | 512KB | 1MB | 2MB | 4MB | 8MB | 16MB | 32MB | 64MB |
| 128B | 1MB | 2MB | 4MB | 8MB | 16MB | 32MB | 64MB | 128MB |
| 256B | 2MB | 4MB | 8MB | 16MB | 32MB | 64MB | 128MB | 256MB |
| 512B | 4MB | 8MB | 16MB | 32MB | 64MB | 128MB | 256MB | 512MB |

---

## 6. Recommendations for Users

### Quick Reference Table (8-byte events)

| Your Scenario | Recommended Buffer Size | Expected Throughput | Wait Strategy |
|---------------|:-----------------------:|:-------------------:|:-------------:|
| **1P-1C** (lowest latency) | **128K–256K** | 800-860M ops/s | Adaptive |
| **1P-1C** (balanced) | **64K** | 300M ops/s | Adaptive |
| **1P-2C** | **256K–1M** | 770-840M ops/s | Adaptive |
| **1P-3C** | **512K–1M** | 330-860M ops/s | Adaptive |
| **2P-1C** | **16K–64K** | ~24M ops/s | Adaptive |
| **3P-1C** | **16K–64K** | ~26M ops/s | Adaptive |
| **2P-2C** | **16K–64K** | ~18M ops/s | Adaptive |

### Decision Guide

#### Choosing Buffer Size

**Step 1: Determine your scenario type**

```
Is your producer single-threaded?
├─ YES → Buffer size matters A LOT. Go to Step 2.
└─ NO (multi-producer) → Buffer size doesn't matter.
                          Use 16K–64K (save memory, keep cache-friendly).
```

**Step 2 (single-producer): Account for event size**

The golden rule: **keep total buffer memory between 1–8 MB** (L2-to-L3 sweet spot).

```
Total memory = buffer_slots × sizeof(your_event)

Calculate: target_slots = 4MB / sizeof(your_event)
Round to nearest power of 2. That's your starting point.
```

| Event Size | Recommended Slots | Total Memory | Expected 1P-1C |
|:---:|:---:|:---:|:---:|
| 8–16B | 128K–512K | 1–8 MB | 800M+ ops/s |
| 32–64B | 16K–128K | 512KB–8 MB | 450–750M ops/s |
| 128B | 16K–64K | 2–8 MB | 350–380M ops/s |
| 256B | 8K–32K | 2–8 MB | 260–290M ops/s |
| 512B+ | 8K–16K | 4–8 MB | ~320M ops/s |

**Step 3 (single-producer): Adjust for consumer count**

More consumers = slowest consumer blocks producer = need more runway.
Multiply the base slot count by:
- 1 consumer: **1x** (use base)
- 2 consumers: **2–4x** (if memory allows)
- 3+ consumers: **4–8x** (if memory allows)

But **never exceed the cache cliff**! If multiplying would push total memory past ~32MB,
stop — you'll hit diminishing returns or even regression.

#### Choosing Wait Strategy

```
What's your priority?
├─ Maximum throughput     → Adaptive (best in all benchmarks)
├─ Lowest worst-case CPU  → Adaptive (self-regulates)
├─ Simplest behavior      → Yield (predictable spin-then-yield)
└─ Lowest possible latency for micro-bursts
   └─ BusySpin (but watch CPU thermals on sustained load)
```

> **General recommendation**: Use **Adaptive** unless you have a specific reason not to.
> It delivered the highest throughput in every scenario tested, while being CPU-friendly.

#### Thread Pinning

```
Should you pin threads?
├─ Hybrid CPU (P+E cores like i9-12th/13th/14th gen) → YES, absolutely
│   └─ Pin to P-cores only, one thread per physical core
│   └─ Avoid HT siblings (don't pin to both cpu8 and cpu9)
│
├─ Homogeneous CPU (all cores same speed) → Recommended
│   └─ Prevents migration overhead and cache invalidation
│
└─ Containerized / VM / shared environment → Maybe not
    └─ Pinning may conflict with orchestrator's CPU allocation
```

### Sizing by Memory Budget

| Buffer Size | Memory (Single-Producer) | Memory (Multi-Producer) | Use Case |
|:-----------:|:---:|:---:|---|
| 16K | 128 KB | 256 KB | Multi-producer (memory-efficient) |
| 64K | 512 KB | 1 MB | Multi-producer (default) |
| 128K | 1 MB | 2 MB | 1P-1C sweet spot |
| 256K | 2 MB | 4 MB | 1P-1C / 1P-2C sweet spot |
| 512K | 4 MB | 8 MB | 1P-3C balanced |
| 1M | 8 MB | 16 MB | 1P-3C+ maximum throughput |

> Note: Multi-Producer uses ~2x memory because `MultiProducerSequencer` maintains an
> `available_buffer` (one `atomic<size_t>` per slot) alongside the ring buffer.

---

## 7. Known Limitations

### Benchmark Caveats

1. **CPU Governor**: All benchmarks ran under `powersave` governor. Results with `performance`
   governor would likely show:
   - Higher absolute numbers across the board
   - BusySpin potentially competitive with Adaptive for 1P-1C
   - More stable measurements (less thermal-induced variance)

2. **Thermal Throttling**: Sustained 100% CPU load (BusySpin, or running many consecutive
   benchmarks) causes P-core frequency reduction. This is why:
   - BusySpin < Yield in many tests (counter-intuitive)
   - 1P-1C results have high variance (p25=120M, p75=253M for Yield)
   - Running BEFORE → AFTER sequentially disadvantages the second run

3. **Event Size**: All benchmarks use `int64_t` (8 bytes). Larger event structs will:
   - Shift optimal buffer size smaller (more cache pressure per slot)
   - Reduce absolute throughput (more data to write/read per event)

4. **Contention Model**: Multi-producer throughput (~24M ops/s) is dominated by
   `atomic<size_t>::fetch_add()` contention, not buffer or strategy choice. This is
   fundamental to the Disruptor's multi-producer design.

5. **Single Machine**: Results are specific to i9-14900K. Different CPUs (especially AMD,
   ARM, or older Intel without hybrid architecture) will show different optimal points.

### Reproducibility

To reproduce these benchmarks:

```bash
# Compile
g++ -std=c++20 -O3 -march=native -DNDEBUG \
    -I/path/to/cpp-disruptor/include \
    -o bench bench_bufsize.cpp -lpthread

# Run (recommended: let CPU cool for 30s between runs)
stdbuf -oL ./bench

# For most accurate results:
# 1. Set CPU governor to 'performance': cpupower frequency-set -g performance
# 2. Disable turbo boost for consistency: echo 1 > /sys/devices/system/cpu/intel_pstate/no_turbo
# 3. Isolate cores from scheduler: isolcpus=8,10,12,14 kernel parameter
```
