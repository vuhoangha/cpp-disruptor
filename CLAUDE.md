# Disruptor++ (C++20)

A high-performance C++20 rewrite of the LMAX Disruptor, achieving 1.5–3x better throughput than the original Java implementation.

## Project Status

Code is functional and benchmarked. Current phase focuses on:
1. **Performance optimization**: Squeeze every last nanosecond — identify and eliminate remaining bottlenecks
2. **Code quality**: Refactor for readability, clarity, and maintainability
3. **Documentation**: Doxygen on all public APIs, inline comments on non-obvious logic, usage guide for end users

## Reference

- Original LMAX Disruptor (Java): https://github.com/LMAX-Exchange/disruptor
- This project is a C++20 rewrite, not a port — architecture may differ where C++ allows better performance (e.g., cache-line padding, memory ordering, template metaprogramming instead of runtime polymorphism)
- When optimizing, refer to the original for algorithmic intent but do NOT blindly replicate Java patterns that don't translate well to C++

## Tech Stack

- C++20 (concepts, coroutines, consteval, std::atomic_ref, etc.)
- Linux primary target (x86_64, ARM64)
- CMake build system
- GoogleTest for testing
- Google Benchmark for performance benchmarks
- No external dependencies for core library (header-only)

## Code Standards

- `snake_case` for functions, variables, namespaces
- `PascalCase` for classes, structs, enums
- `UPPER_SNAKE_CASE` for constants and macros
- Doxygen comments on all public APIs
- RAII everywhere — no raw `new`/`delete`, use smart pointers where ownership is needed
- Prefer `constexpr`, `consteval`, and `noexcept` aggressively
- No exceptions in hot path — error handling via return codes or `std::expected`
- No `std::shared_ptr` in hot path — prefer raw references or `std::unique_ptr`

## Performance Rules

These rules are critical — every code change must preserve or improve performance:
- **Measure before and after every change**: Run benchmarks, reject regressions
- **Cache-line awareness**: Align hot data to 64-byte boundaries, prevent false sharing with padding
- **Lock-free only**: No mutexes, no spinlocks in the critical path — `std::atomic` with appropriate memory ordering
- **Memory ordering matters**: Use `std::memory_order_relaxed` where safe, `acquire`/`release` for synchronization, `seq_cst` only when absolutely required
- **Zero allocation in hot path**: Pre-allocate everything, use ring buffer, no `new`/`malloc` during publish/consume
- **Branch prediction friendly**: Keep hot paths branchless where possible, use `[[likely]]`/`[[unlikely]]`
- **Compiler hints**: Use `__builtin_expect`, `[[nodiscard]]`, `alignas(64)` for cache-line alignment
- **SIMD/intrinsics**: Consider `_mm_pause()` for spin-wait, prefetch hints for sequential access patterns
- When refactoring for readability, NEVER sacrifice performance — always benchmark before and after

## Testing

- Unit tests for every public class/function
- Benchmark tests alongside unit tests — performance is a feature
- Test file naming: `test_<module_name>.cpp`
- Benchmark file naming: `bench_<module_name>.cpp`
- All tests must pass before any commit

## Git Conventions

- Conventional Commits: `feat:`, `fix:`, `test:`, `docs:`, `refactor:`, `perf:`, `bench:`
- One logical change per commit
- Feature branches: `feature/<short-description>`
- Performance changes always include benchmark results in commit message

## Workflow Rules

- After brainstorming completes and design is approved, append a short "Architecture Decisions" section to this file with:
  - One-line summary per major decision
  - Link to full design doc in docs/plans/
  - Do NOT copy the full design — keep it under 10 lines
- When starting a new session to continue existing work, read the Architecture Decisions section below and the linked design doc first
- Before any refactoring or optimization, run full benchmark suite and record baseline
- After refactoring or optimization, run full benchmark suite and compare — reject if regression detected
- When adding Doxygen or comments, do NOT change any logic or structure — documentation-only commits

## Priority Order

1. **Performance optimization** — profile, identify hotspots, optimize, benchmark
2. **Refactoring** — improve readability without changing behavior or hurting performance
3. **Documentation** — Doxygen comments, inline comments, usage guide, examples

---

<!-- Architecture Decisions will be appended here after brainstorming -->