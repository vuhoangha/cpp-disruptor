- các comment có đánh dấu "__PERF" là gợi ý tối ưu
- Tối ưu rẽ nhánh (Branch Prediction Cache)
- Tối ưu sử dụng SIMD
    + áp dụng cho getMinimumSequence
- Sử dụng "noexcept" nếu có thể
- Sử dụng "const", "constexpr" nếu có thể
- vì một số phần sử dụng đa hình nên cần tham chiếu đối tượng và con trỏ. Nhưng nếu muốn performance tốt hơn nữa thì sử dụng đối tượng trực tiếp ko cần đa hình. Nghiên cứu xem sao nhé
- Các nơi dùng Sequence dùng chung thường đi liền với std::shared_ptr<Sequence>. Việc này tuy đảm bảo bộ nhớ ko bị rò rỉ nhưng làm chậm performance đi nhiều. Tìm cách cải thiện nó
- Check xem máy chạy phần này có cache line là bao nhiêu để biết phần tránh false sharing. Thứ 2 là có vẻ chỉ giao thức MESI để giao tiếp giữa các CPU và bộ nhớ thì mới có tình trạng này. Tìm hiểu và tối ưu cẩn thận chỗ này nhé
- Tận dụng CPU Registers (thanh ghi CPU)
    - Registers là vùng lưu trữ siêu nhanh (1 chu kỳ CPU) ngay trong CPU
    - Truy cập nhanh hơn L1 cache khoảng 3-10 lần
- Tận dụng NUMA
- Phần false sharing cần chú ý nghiêm ngặt kiến trúc phần cứng và phiên bản C++. Ví dụ như trong Java có thể gặp tình trạng là với môi trường khác nhau sẽ ko hoạt động như ý
    + https://github.com/LMAX-Exchange/disruptor/issues/231
    + https://mechanical-sympathy.blogspot.com/2011/08/false-sharing-java-7.html
- [Ý tưởng] cũng trong vòng lặp đọc biến sequence mong chờ với relax, dùng câu lệnh if để ràng buộc, chống CPU tự sắp xếp câu lệnh.
- tận dụng Attribute [[likely]] và [[unlikely]]
```cpp
void processData(bool condition) {
    if (condition) [[likely]] {
        // Code được thực thi thường xuyên
        hotPath();
    } else [[unlikely]] {
        // Code ít khi được thực thi
        coldPath();
    }
}
- ```
- tận dụng [[gnu::hot]] và [[gnu::cold]]
```cpp
// Compiler có thể tối ưu khác nhau cho hot vs cold path
void hotMethod() [[gnu::hot]] {  // GCC specific
    // Compiler sẽ ưu tiên tối ưu hóa method này
    // Ví dụ: aggressive inlining, loop unrolling
}

void coldMethod() [[gnu::cold]] {  // GCC specific  
    // Tối ưu hóa ít hơn, ưu tiên code size
}
- ```
- Áp dụng kỹ thuật Profile-Guided Optimization (PGO) để compiler có thể tối ưu theo môi trường production thực tế nhé
- [Các kỹ thuật tối ưu khác cho c++ 20 nhé] (https://claude.ai/share/d4e61284-04a2-4744-a1bc-62c29fda21da)