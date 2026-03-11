# Disruptor++ Auto-Tune Tool

Tool tự động tìm **buffer size** và **wait strategy** tối ưu nhất cho phần cứng của bạn.

Mỗi máy chủ có CPU, cache, memory bandwidth khác nhau → thông số tối ưu cũng khác nhau.
Tool này sweep tất cả combinations rồi tìm ra config cho throughput cao nhất.

---

## Table of Contents

1. [Tổng quan](#1-tổng-quan)
2. [Compile](#2-compile)
3. [Cách sử dụng](#3-cách-sử-dụng)
4. [Output giải thích](#4-output-giải-thích)
5. [Áp dụng kết quả vào code](#5-áp-dụng-kết-quả-vào-code)
6. [Tips để benchmark chính xác](#6-tips-để-benchmark-chính-xác)
7. [Thông số kỹ thuật](#7-thông-số-kỹ-thuật)
8. [FAQ](#8-faq)

---

## 1. Tổng quan

### Tool làm gì?

1. **Auto-detect CPU** — đọc `/sys/devices/system/cpu/` để biết:
   - Số cores vật lý / logical
   - Tần số max mỗi core (phân biệt P-core vs E-core trên Intel hybrid)
   - Cache sizes (L1d, L2, L3)
   - Tự chọn best cores để pin thread (ưu tiên cores có tần số cao nhất,
     1 thread per physical core, tránh Hyper-Threading siblings)

2. **Sweep configurations** — chạy benchmark qua:
   - 9 buffer sizes: 4K, 8K, 16K, 32K, 64K, 128K, 256K, 512K, 1M
   - 3 wait strategies: Adaptive, Yield, BusySpin
   - Tổng: **27 configurations** per scenario
   - Mỗi config: 2 warmup runs + 5 measurement runs → lấy **median**

3. **Report** — hiển thị:
   - Optimal config (buffer size + wait strategy + throughput)
   - Top 5 configurations
   - Suggested C++ code

### Scenarios hỗ trợ

| Scenario | Mô tả | Cores cần |
|----------|-------|-----------|
| `1P1C` | 1 producer, 1 consumer | 2 |
| `1P2C` | 1 producer, 2 consumers | 3 |
| `1P3C` | 1 producer, 3 consumers | 4 |
| `2P1C` | 2 producers, 1 consumer | 3 |
| `3P1C` | 3 producers, 1 consumer | 4 |
| `2P2C` | 2 producers, 2 consumers | 4 |

### Event sizes hỗ trợ

`8`, `16`, `32`, `64`, `128`, `256`, `512` bytes.

Chọn size gần nhất với `sizeof(YourEventStruct)`. Ví dụ nếu event struct của bạn
là 96 bytes → chọn `128` (round up to nearest supported size).

---

## 2. Compile

### Từ project root:

```bash
g++ -std=c++20 -O3 -march=native -DNDEBUG \
    -Iinclude \
    -o tools/autotune tools/autotune.cpp \
    -lpthread
```

### Giải thích flags:

| Flag | Tại sao |
|------|---------|
| `-O3` | Optimization level cao nhất — benchmark phải dùng cùng level với production |
| `-march=native` | Sử dụng instruction set của CPU hiện tại (AVX2, SSE4.2, etc.) |
| `-DNDEBUG` | Tắt `assert()` — bản release, không có debug overhead |
| `-Iinclude` | Include path tới header files của Disruptor++ |
| `-lpthread` | Link pthread cho multi-threading |

### Compile trên máy khác:

```bash
# Clone repo
git clone <repo-url>
cd cpp-disruptor

# Compile
g++ -std=c++20 -O3 -march=native -DNDEBUG \
    -Iinclude \
    -o tools/autotune tools/autotune.cpp \
    -lpthread

# Chạy
./tools/autotune
```

> **Lưu ý**: Phải compile trên máy đích (không cross-compile) vì `-march=native`
> tạo binary tối ưu cho CPU cụ thể đó.

---

## 3. Cách sử dụng

### 3.1 Interactive mode (đơn giản nhất)

Chỉ cần chạy không arguments:

```bash
./tools/autotune
```

Tool sẽ hỏi lần lượt:
1. Chọn scenario (1-7)
2. Nhập event size (bytes)
3. Bật/tắt thread pinning

**Ví dụ session:**

```
Disruptor++ Auto-Tune (interactive mode)

Select scenario:
  1) 1P1C  — 1 producer, 1 consumer
  2) 1P2C  — 1 producer, 2 consumers
  3) 1P3C  — 1 producer, 3 consumers
  4) 2P1C  — 2 producers, 1 consumer
  5) 3P1C  — 3 producers, 1 consumer
  6) 2P2C  — 2 producers, 2 consumers
  7) ALL   — run all scenarios

Choice [1-7]: 1

Event struct size in bytes:
  8, 16, 32, 64, 128, 256, 512

Size [bytes]: 64

Enable thread pinning? [Y/n]: Y
```

### 3.2 CLI mode (cho scripting / CI)

```bash
# Một scenario cụ thể
./tools/autotune --scenario 1P1C --event-size 64

# Không thread pinning (ví dụ chạy trong container)
./tools/autotune --scenario 1P1C --event-size 64 --no-pin

# Chạy TẤT CẢ scenarios
./tools/autotune --all --event-size 64

# Xem help
./tools/autotune --help
```

### 3.3 Các tình huống sử dụng phổ biến

**"Tôi có 1 producer ghi market data, 1 consumer xử lý"**

```bash
# Market data event thường ~64-128 bytes
./tools/autotune --scenario 1P1C --event-size 128
```

**"Tôi có 1 producer, 3 consumer xử lý song song (multicast)"**

```bash
./tools/autotune --scenario 1P3C --event-size 64
```

**"Tôi có 3 nguồn data ghi vào 1 buffer, 1 consumer tổng hợp"**

```bash
./tools/autotune --scenario 3P1C --event-size 32
```

**"Tôi chạy trong Docker container, không muốn pin threads"**

```bash
./tools/autotune --scenario 1P1C --event-size 64 --no-pin
```

**"Tôi muốn test tất cả scenarios trên server mới để so sánh"**

```bash
# Chạy tất cả, lưu output
./tools/autotune --all --event-size 64 2>&1 | tee autotune_results.txt
```

---

## 4. Output giải thích

### CPU Topology

```
  CPU Topology:
  Logical CPUs:  32
  Physical cores: 24
  L1d cache:     48 KB
  L2 cache:      2048 KB
  L3 cache:      36 MB
  Best cores:    cpu8(6.0GHz), cpu10(6.0GHz), cpu2(5.7GHz), ...
```

- **Logical vs Physical**: Logical > Physical → có Hyper-Threading
- **Best cores**: Sorted by frequency desc, tool tự động pin threads vào đây
- **Cache sizes**: Ảnh hưởng trực tiếp đến optimal buffer size.
  Rule of thumb: tổng buffer memory nên nằm trong L2-L3 range

### Sweep output

```
  Sweeping 1P1C with 64B events...
    BUF=    4K  Adaptive= 428M  Yield= 441M  BSpin= 441M
    BUF=    8K  Adaptive= 454M  Yield= 447M  BSpin= 447M
    ...
    BUF= 1024K  Adaptive= 254M  Yield=  73M  BSpin= 256M
```

- Mỗi dòng = 1 buffer size, 3 wait strategies
- Đơn vị: **M ops/s** (triệu operations per second)
- Quan sát: throughput tăng rồi giảm khi buffer quá lớn (cache overflow)

### Optimal Configuration

```
╔════════════════════════════════════════════════╗
║           OPTIMAL CONFIGURATION              ║
╠════════════════════════════════════════════════╣
║  Scenario:       1P1C                        ║
║  Event size:     64 bytes                    ║
║  Buffer size:    8K (8192 slots)             ║
║  Wait strategy:  Adaptive                    ║
║  Thread pinning: YES                         ║
║  Buffer memory:  512 KB                      ║
║  Throughput:     454 M ops/s                 ║
╚════════════════════════════════════════════════╝
```

### Top 5

```
  Top 5 configurations:
  Rank  Scen   BufSize    Strategy   Memory   Throughput
  ───── ────── ────────── ────────── ──────── ──────────
  #1    1P1C   8K         Adaptive   512KB    454M ops/s
  #2    1P1C   256K       BusySpin   16MB     454M ops/s
  ...
```

> Nếu nhiều configs có throughput gần nhau, **ưu tiên config dùng ít memory hơn**
> (ví dụ 8K 512KB vs 256K 16MB cùng 454M → chọn 8K).

### Suggested C++ code

```
  Suggested C++ configuration:

    constexpr size_t BUFFER_SIZE = 8192;
    constexpr auto WAIT = WaitStrategyType::ADAPTIVE;
```

Copy-paste thẳng vào code.

---

## 5. Áp dụng kết quả vào code

### Trước khi tune

```cpp
// Chưa tối ưu — dùng giá trị mặc định
constexpr size_t BUFFER_SIZE = 1024;

using MyRingBuffer = RingBuffer<MyEvent, BUFFER_SIZE>;
using MySequencer = SingleProducerSequencer<MyEvent, BUFFER_SIZE, 1>;
using MyBarrier = ProcessingSequenceBarrier<WaitStrategyType::YIELD, 1, MySequencer>;
using MyProcessor = BatchEventProcessor<MyEvent, BUFFER_SIZE, MyHandler, MyBarrier>;
```

### Sau khi tune

```cpp
// Tối ưu cho server XYZ (autotune results 2026-03-11)
constexpr size_t BUFFER_SIZE = 131072;  // 128K — optimal cho 64B events trên i9-14900K

using MyRingBuffer = RingBuffer<MyEvent, BUFFER_SIZE>;
using MySequencer = SingleProducerSequencer<MyEvent, BUFFER_SIZE, 1>;
using MyBarrier = ProcessingSequenceBarrier<WaitStrategyType::ADAPTIVE, 1, MySequencer>;
using MyProcessor = BatchEventProcessor<MyEvent, BUFFER_SIZE, MyHandler, MyBarrier>;
```

### Thread pinning trong production

```cpp
#include "common/Util.hpp"

// Pin producer thread vào core tốt nhất
std::thread producer([&] {
    Util::pin_thread_to_core(8);  // P-core 6.0 GHz
    // ... produce events
});

// Pin consumer thread vào core khác
std::thread consumer([&] {
    Util::pin_thread_to_core(10);  // P-core 6.0 GHz
    processor.run();
});
```

> **Tip**: Lấy core IDs từ output "Best cores" của autotune.

---

## 6. Tips để benchmark chính xác

### Trước khi chạy

1. **Đóng hết các ứng dụng nặng** (browser, IDE, build processes)

2. **Set CPU governor** (nếu có quyền root):
   ```bash
   sudo cpupower frequency-set -g performance
   ```

3. **Disable turbo boost** cho kết quả ổn định hơn (optional):
   ```bash
   echo 1 | sudo tee /sys/devices/system/cpu/intel_pstate/no_turbo
   ```

4. **Để CPU nguội** — nếu vừa chạy benchmark nặng, đợi 30-60 giây

### Khi chạy

5. **Không chạy nhiều benchmark cùng lúc** — chúng tranh nhau CPU và cache

6. **Chạy lại nếu kết quả bất thường** — thermal throttling có thể gây sai lệch,
   đặc biệt ở BusySpin strategy

### Đánh giá kết quả

7. **Nếu Top 5 có throughput gần nhau** (< 10% chênh lệch):
   - Chọn config dùng **ít memory nhất**
   - Ưu tiên **Adaptive** strategy (CPU-friendly nhất)

8. **Multi-producer scenarios** (2P1C, 3P1C, 2P2C):
   - Buffer size thường **không ảnh hưởng** (bottleneck = atomic contention)
   - Chọn 16K-64K là đủ, tiết kiệm memory

9. **Kết quả khác nhau giữa các lần chạy**:
   - Bình thường — do thermal, OS scheduling, background processes
   - Tool dùng median của 5 runs để giảm noise
   - Chênh lệch < 15% giữa các lần chạy là acceptable

---

## 7. Thông số kỹ thuật

### Benchmark methodology

| Parameter | Value |
|-----------|-------|
| Warmup runs | 2 (kết quả bỏ đi) |
| Measurement runs | 5 |
| Metric | Median |
| Event count (8-16B) | 50M |
| Event count (32-64B) | 30M |
| Event count (128B) | 20M |
| Event count (256B) | 10M |
| Event count (512B) | 5M |

Event count giảm theo event size để giữ thời gian chạy hợp lý.

### Buffer sizes tested

| Slots | Memory (8B event) | Memory (64B event) | Memory (256B event) |
|:-----:|:---:|:---:|:---:|
| 4K | 32 KB | 256 KB | 1 MB |
| 8K | 64 KB | 512 KB | 2 MB |
| 16K | 128 KB | 1 MB | 4 MB |
| 32K | 256 KB | 2 MB | 8 MB |
| 64K | 512 KB | 4 MB | 16 MB |
| 128K | 1 MB | 8 MB | 32 MB |
| 256K | 2 MB | 16 MB | 64 MB |
| 512K | 4 MB | 32 MB | 128 MB |
| 1M | 8 MB | 64 MB | 256 MB |

### Wait strategies

| Strategy | Behavior | CPU | Best for |
|----------|----------|-----|----------|
| **Adaptive** | Spin → Yield → Sleep cycle | Moderate | Best overall |
| **Yield** | Spin 200x then yield | High | Low-latency |
| **BusySpin** | Non-stop spin | 100% | Ultra-low latency, short bursts |

### Event struct layout

Tool sử dụng struct đơn giản: `int64_t value` + padding.
Throughput thực tế có thể khác nếu event handler phức tạp hơn (ví dụ memcpy data).
Tuy nhiên, optimal buffer size / wait strategy thường **không đổi** vì phụ thuộc
vào cache topology chứ không phải handler logic.

---

## 8. FAQ

### "Thời gian chạy bao lâu?"

- 1 scenario: ~3-5 phút (27 configs × 7 runs mỗi config)
- `--all` (6 scenarios): ~20-30 phút

### "Event size của tôi là 100 bytes, không có trong danh sách?"

Round up: chọn `128`. Kết quả vẫn representative vì optimal buffer size phụ thuộc
vào tổng memory footprint, và 100B vs 128B chênh lệch không đáng kể.

### "Tôi có 4 producers, tool chỉ hỗ trợ tối đa 3?"

Tool hiện hỗ trợ tối đa 3P. Với 4P+, kết quả 3P1C vẫn representative vì
multi-producer throughput bị bottleneck bởi atomic contention — thêm producer
thường không cải thiện (có thể còn giảm). Dùng kết quả 3P1C làm baseline.

### "Tại sao BusySpin chậm hơn Adaptive?"

Bình thường trên hầu hết systems. BusySpin 100% CPU → thermal throttling → CPU
giảm tần số. Adaptive tự điều tiết nên CPU giữ được tần số cao hơn.
BusySpin chỉ lợi cho micro-burst scenarios (data đến rất nhanh, không sustained load).

### "Kết quả trên server A và server B khác nhau nhiều?"

Đúng, đó là lý do tool này tồn tại. Ví dụ:
- Server với L3 cache 36MB → buffer lớn hơn vẫn fast
- Server với L3 cache 8MB → cần buffer nhỏ hơn
- AMD CPU → cache hierarchy khác Intel → optimal point khác

### "Chạy trong container/VM có chính xác không?"

Có, nhưng:
- Dùng `--no-pin` nếu container bị giới hạn CPU cores
- Kết quả phản ánh performance thực tế trong container (bao gồm cả overhead VM/container)
- Thread pinning có thể conflict với container orchestrator (Kubernetes CPU limits)

### "Tôi cần thêm event size khác (ví dụ 1024 bytes)?"

Thêm vào source code `tools/autotune.cpp`:
1. Thêm struct: `struct EV1024 { int64_t value{0}; char pad[1016]; };`
2. Thêm handler: `struct H1024 { ... };` (copy pattern từ existing handlers)
3. Thêm vào `DISPATCH_EV` macro: `else if (ev_bytes == 1024) { ... }`
4. Recompile
