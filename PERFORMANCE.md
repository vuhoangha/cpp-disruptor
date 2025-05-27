- Sử dụng cache line 128 thay vì 64 nhé, vì 1 số chip mới có cơ chế prefetching dẫn tới nó load cả cache line liền kề. Vậy nên khi cache line liền kề thay đổi thì core hiện tại cũng phải load lại
  - https://github.com/LMAX-Exchange/disruptor/issues/158
  - https://github.com/LMAX-Exchange/disruptor/issues/211
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