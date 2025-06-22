<!-- -------------------------LEFT TAB------------------------------------- -->

- Incl. (Inclusive Cost)
Tổng chi phí (ví dụ: số chu kỳ CPU, cache miss, hoặc thời gian) của hàm này và tất cả các hàm con mà nó gọi.
Nó phản ánh tác động tổng thể của hàm đến toàn chương trình.
Dùng để tìm "hàm đầu sỏ" tiêu tốn CPU khi gọi nhiều hàm khác.

- Self (Self Cost)
Chi phí chỉ trong chính hàm đó, không bao gồm hàm con.
Dùng để xác định đoạn code nội tại tốn thời gian thực sự.

- Called
Số lần hàm này được gọi.
Cực kỳ hữu ích để tìm các hàm bị gọi lặp lại quá nhiều lần — có thể là nguyên nhân gây nghẽn hiệu năng.



<!-- -------------------------RIGHT TAB------------------------------------- -->

- Types: Loại đơn vị đang phân tích (Function, File, Object, Line...)

- Callers: Danh sách các hàm đã gọi đến hàm hiện tại. Dùng để trace ngược đường gọi.

- All Callers: Như trên, nhưng tính cả các lời gọi gián tiếp qua nhiều cấp.

- Callee Map: Đồ thị thể hiện các hàm được gọi bởi hàm này, theo kiểu sơ đồ cây.

- Source Code: Mã nguồn cụ thể (nếu có debug symbol và mã nguồn).

- Ir (Instruction Retired): Số lượng câu lệnh CPU đã được “retired” (tức là đã chạy hoàn tất). Đây là đơn vị chính cho cost.

- Ir per Call: Trung bình số câu lệnh được thực thi mỗi lần gọi hàm. → Đo độ nặng mỗi call.

- Count: Số lần lời gọi (call site) được thực hiện. Không nhất thiết là số lần hàm bị gọi.

- Callee: Danh sách các hàm mà hàm hiện tại gọi đến. Dùng để trace xuôi.