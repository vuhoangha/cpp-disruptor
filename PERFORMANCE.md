- các comment có đánh dấu "__PERF" là gợi ý tối ưu
- Tối ưu rẽ nhánh (Branch Prediction Cache)
- Tối ưu sử dụng SIMD
    + áp dụng cho getMinimumSequence
- Sử dụng "noexcept" nếu có thể
- Sử dụng "const", "constexpr" nếu có thể
- vì một số phần sử dụng đa hình nên cần tham chiếu đối tượng và con trỏ. Nhưng nếu muốn performance tốt hơn nữa thì sử dụng đối tượng trực tiếp ko cần đa hình. Nghiên cứu xem sao nhé

