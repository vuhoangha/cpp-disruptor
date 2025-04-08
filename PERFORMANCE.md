- các comment có đánh dấu "__PERF" là gợi ý tối ưu
- Tối ưu rẽ nhánh (Branch Prediction Cache)
- Tối ưu sử dụng SIMD
    + áp dụng cho getMinimumSequence
- Sử dụng "noexcept" nếu có thể
- Sử dụng "const", "constexpr" nếu có thể