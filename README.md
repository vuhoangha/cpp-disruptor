# cpp-disruptor

## Cách CPU dự đoán rẽ nhánh trong code (Branch Prediction Cache)
1. CPU hiện đại sử dụng pipeline để xử lý song song nhiều lệnh. Thông thường một CPU có 5 giai đoạn xử lý:
    - Fetch (lấy lệnh)
    - Decode (giải mã)
    - Execute (thực thi)
    - Memory (truy cập bộ nhớ)
    - Writeback (ghi kết quả)
Trong 1 chu kỳ CPU (CPU clock sycle) thì mỗi giai đoạn trên chỉ có thể được xử lý 1 lần duy nhất (do giới hạn phần cứng). Nếu đã Fetch 1 lần thì không thể Fetch thêm. Thông thường mỗi task sẽ được chia nhỏ thành nhiều giai đoạn trên để xử lý song song. 1 chu kỳ CPU có thể xử lý Fetch của task 1, Decode của task 2 cùng lúc. Thành ra các task sẽ được xử lý song song thay vì phải xử lý các giai đoạn của các task 1 cách lần lượt (img/pipelining.png)
2. Ví dụ về cách dự đoán
Ta có đoạn code sau:
```C++
for (int i = 0; i < 1000; i++) {
    if (data[i] > threshold) {  // Điểm rẽ nhánh
        result++;               // Nhánh đúng
    }                           // Nhánh sai: không làm gì
}
```
Ở ví dụ trên, trong điều kiện IF, sau vài lần chạy thì chương trình nhận thấy phần lớn data[i] sẽ lớn hơn threshold. Vậy nên từ các lần tiếp theo, trong lúc CPU đang check xem "data[i] > threshold" không thì đồng thời nó cũng sẽ lấy giá trị "result" từ L1 trước. Nếu điều kiện đúng thì nó đã có sẵn "result" để tính toán rồi, nếu sai thì nó bỏ đi
3. Hoàn toàn có thể code để cho chương trình dễ được CPU đoán rẽ nhánh hơn. Search các tool AI để biết nhé


## Cách các Core CPU quản lý dữ liệu
1. Thanh ghi (Registers)
- Kích thước: Thường từ 32-bit đến 64-bit (4-8 byte) mỗi thanh ghi, với số lượng từ 8-32 thanh ghi tùy kiến trúc
- Tác dụng: Lưu trữ dữ liệu tạm thời mà CPU cần xử lý ngay lập tức, tốc độ truy cập nhanh nhất
- Ví dụ về việc CPU đang thực hiện một phép cộng đơn giản: A = B + C
    - Thêm dữ liệu:
        - CPU tải giá trị B (giả sử là 5) từ bộ nhớ vào thanh ghi R1
        - CPU tải giá trị C (giả sử là 7) từ bộ nhớ vào thanh ghi R2
        - CPU thực hiện phép cộng và lưu kết quả 12 vào thanh ghi R3
    - Xóa dữ liệu:
        - Khi CPU cần thực hiện phép tính khác, giá trị trong thanh ghi R1, R2, R3 sẽ bị ghi đè bởi dữ liệu mới
        - Ví dụ: nếu CPU bắt đầu tính D = E + F, các giá trị cũ sẽ bị thay thế

2. Bộ nhớ cache L1 (Cache Memory)
    - Kích thước: Thường từ 32KB đến 64KB cho mỗi core (chia thành cache lệnh và cache dữ liệu)
    - Tác dụng: Lưu trữ dữ liệu và lệnh được sử dụng thường xuyên, giảm thời gian truy cập bộ nhớ chính
    - Ví dụ về việc CPU đang thực hiện một phép cộng đơn giản: A = B + C
        - Thêm dữ liệu:
            - Khi CPU cần giá trị B, nó tìm trong L1 trước
            - Nếu không có trong L1 (cache miss), CPU sẽ lấy B từ bộ nhớ cao hơn (L2, L3 hoặc RAM) và lưu vào L1
            - Giả sử biến B nằm trong một mảng số, khi lấy B, toàn bộ dãy số gần B cũng được đưa vào L1 (vì nguyên tắc locality)
        - Xóa dữ liệu:
            - Khi L1 đã đầy và CPU cần lưu dữ liệu mới, thuật toán thay thế (như LRU - Least Recently Used) sẽ quyết định dữ liệu nào bị loại bỏ
            - Ví dụ: Nếu giá trị B không được sử dụng trong thời gian dài, nó có thể bị đẩy ra khỏi L1 để nhường chỗ cho dữ liệu mới hơn

3. Bộ nhớ cache L2 (Cache Memory)
    - Kích thước: Thường từ 256KB đến 512KB cho mỗi core
    - Tác dụng: Bộ nhớ đệm thứ cấp, lớn hơn L1 nhưng chậm hơn một chút, lưu trữ dữ liệu và lệnh ít được sử dụng hơn so với L1
    - Ví dụ về việc CPU đang thực hiện một phép cộng đơn giản: A = B + C
        - Thêm dữ liệu:
            - Khi CPU tìm giá trị C trong L1 nhưng không có (cache miss), nó tìm trong L2
            - Nếu C có trong L2, CPU sẽ sao chép C vào L1 và đồng thời giữ bản sao trong L2
            - Nếu C không có trong L2, CPU sẽ lấy từ L3 hoặc RAM, sau đó lưu vào cả L1 và L2
        - Xóa dữ liệu:
            - Tương tự như L1, L2 sử dụng thuật toán thay thế khi đầy
            - Ví dụ: nếu chương trình chuyển sang xử lý một bộ dữ liệu hoàn toàn khác (như chuyển từ tính toán số học sang xử lý văn bản), nhiều dữ liệu cũ trong L2 sẽ bị loại bỏ dần dần để phục vụ cho tác vụ mới

4. Bộ nhớ cache L3
    - Kích thước: Thường từ 2MB đến 32MB, được chia sẻ giữa tất cả các core trong cùng CPU
    - Tác dụng: Bộ nhớ đệm lớn nhất, chậm hơn L1 và L2, nhưng giúp các core chia sẻ dữ liệu với nhau
    - Ví dụ: Tưởng tượng một ứng dụng chỉnh sửa ảnh đang xử lý cùng lúc một tấm ảnh lớn
        - Thêm dữ liệu:
            - CPU core 1 đang xử lý phần trên của ảnh, và đã nạp dữ liệu vào L1, L2 của nó
            - CPU core 2 cần xử lý phần giữa ảnh, nên nạp dữ liệu đó vào L1, L2 của riêng nó
            - Toàn bộ tấm ảnh được lưu trong L3 để tất cả các core có thể truy cập nhanh
        - Xóa dữ liệu:
            - Khi bạn mở một tệp văn bản lớn, dữ liệu ảnh trong L3 sẽ dần bị thay thế
            - Ví dụ: phần dữ liệu ảnh ít được truy cập (như metadata) sẽ bị loại bỏ trước

## Cách các biến được sử dụng trong nhiều thread chạy trên nhiều Core CPU
Khi thread A trên core 1 cập nhật biến x, và thread B trên core 2 cần đọc biến x, hệ thống sử dụng giao thức cache coherence để đảm bảo thread B nhìn thấy giá trị mới nhất. Đây là cách hoạt động:
1. Quy trình cập nhật (Khi thread A thay đổi biến x)
    - Core 1 muốn ghi vào biến x
    - Core 1 phát tín hiệu "invalidate" cho tất cả các core khác
    - Tất cả các core khác (bao gồm core 2) đánh dấu bản sao của x trong cache của chúng là "Invalid"
    - Core 1 cập nhật x trong cache của nó và đánh dấu là "Modified"
    - (Tùy vào chính sách write-back/write-through, giá trị có thể được cập nhật vào RAM ngay lập tức hoặc sau đó)

2. Quy trình đọc (Khi thread B đọc biến x)
    - Core 2 cố gắng đọc biến x
    - Nó kiểm tra cache của mình và thấy x đã bị đánh dấu "Invalid"
    - Core 2 phát tín hiệu "read request" lên bus
    - Core 1 thấy yêu cầu này và biết nó có bản sao mới nhất của x
    - Core 1 cung cấp dữ liệu cho core 2 (cache-to-cache transfer)
    - Cả hai core bây giờ đều có x trong trạng thái "Shared"

## Order Memory
Nguyên nhân:
- CPU có thể thực thi các lệnh không theo thứ tự trong mã nguồn để tối ưu hiệu suất. Ví dụ:
```C++
int a = 1;
int b = 2;
int c = a + b;

// Có thể được thực thi thành:
int b = 2;
int a = 1;
int c = a + b;
```
- Mỗi core CPU có cache riêng, và chúng phải đồng bộ với nhau qua các giao thức phức tạp. Từ đó dẫn tới việc các thread chạy trên các core khác nhau có thể không nhìn thấy giá trị mới nhất của biến.
- Theo thông thường thì mặc định sẽ là memory_order_seq_cst nhưng nó là loại tốn CPU nhất, nên cần có các loại khác để tối ưu hơn.

#### memory_order_relaxed
- đảm bảo tính atomic của thao tác.
- không ngăn cản việc sắp xếp lại các thao tác trước hoặc sau nó.
- Example về tính nguyên tử
```C++
std::atomic<int> counter{0};

void increment() {
    for (int i = 0; i < 1000; i++) {
        // Chỉ đảm bảo rằng việc tăng counter là atomic
        // Không có đảm bảo về thứ tự thực hiện với các thao tác khác
        counter.fetch_add(1, std::memory_order_relaxed);
    }
}

int main() {
    std::thread t1(increment);
    std::thread t2(increment);
    
    t1.join();
    t2.join();
    
    std::cout << "Counter: " << counter.load() << std::endl; // Luôn là 2000
    return 0;
}
```
- Example về thứ tự thao tác
```C++
std::atomic<bool> data_ready{false};
int shared_data = 0;

void producer() {
    // Chuẩn bị dữ liệu
    shared_data = 42;
    
    // Thông báo dữ liệu đã sẵn sàng
    data_ready.store(true, std::memory_order_relaxed);
}

void consumer() {
    // Chờ đến khi dữ liệu sẵn sàng
    while (!data_ready.load(std::memory_order_relaxed)) {
        // Đợi cho đến khi dữ liệu sẵn sàng
    }
    
    std::cout << "Dữ liệu nhận được: " << shared_data << std::endl; // Có thể là 42 hoặc 0
}
```

#### memory_order_acquire
- đảm bảo tính atomic của thao tác.
- Sử dụng với thao tác đọc (load).
- Đảm bảo rằng mọi thao tác đọc/ghi sau lệnh này không bị đưa lên trên nó.
- Nhưng các thao tác đọc/ghi phía trước lệnh hiện tại vẫn có thể bị đẩy xuống phía sau.
- Nó chỉ ngăn chặn 1 chiều từ sau ra trước
- Example:
```C++
#include <atomic>
#include <thread>
#include <iostream>

std::atomic<bool> data_ready{false};
int shared_data = 0;

void producer() {
    // Chuẩn bị dữ liệu
    shared_data = 42;
    
    // Thông báo dữ liệu đã sẵn sàng
    data_ready.store(true, std::memory_order_release);
}

void consumer() {
    // Chờ đến khi dữ liệu sẵn sàng với acquire semantics
    while (!data_ready.load(std::memory_order_acquire)) {
        // Đợi cho đến khi dữ liệu sẵn sàng
    }
    
    // Sau acquire, chắc chắn có thể thấy mọi thay đổi được thực hiện
    // trước thao tác release tương ứng
    std::cout << "Dữ liệu nhận được: " << shared_data << std::endl; // Luôn là 42
}

int main() {
    std::thread t1(producer);
    std::thread t2(consumer);
    
    t1.join();
    t2.join();
    
    return 0;
}
```

#### memory_order_release
- đảm bảo tính atomic của thao tác.
- Sử dụng với thao tác ghi (store).
- Đảm bảo rằng mọi thao tác đọc/ghi trước lệnh này không bị đưa xuống sau nó.
- Nhưng các thao tác đọc/ghi phía sau lệnh hiện tại vẫn có thể bị đẩy lên trước.
- Nó chỉ ngăn chặn 1 chiều từ trước ra sau
- Example:
```C++
#include <atomic>
#include <thread>
#include <iostream>

std::atomic<bool> data_ready{false};
int shared_data = 0;

void producer() {
    // Chuẩn bị dữ liệu
    shared_data = 42;
    
    // Thông báo dữ liệu đã sẵn sàng. Chắc chắn shared_data đã được cập nhật rồi
    data_ready.store(true, std::memory_order_release);
}

void consumer() {
    // Chờ đến khi dữ liệu sẵn sàng với acquire semantics
    while (!data_ready.load(std::memory_order_acquire)) {
        // Đợi cho đến khi dữ liệu sẵn sàng
    }
    
    // Sau acquire, chắc chắn có thể thấy mọi thay đổi được thực hiện
    // trước thao tác release tương ứng
    std::cout << "Dữ liệu nhận được: " << shared_data << std::endl; // Luôn là 42
}

int main() {
    std::thread t1(producer);
    std::thread t2(consumer);
    
    t1.join();
    t2.join();
    
    return 0;
}
```

#### memory_order_acq_rel
- đảm bảo tính atomic của thao tác.
- kết hợp của acquire và release.
- đảm bảo thao tác đọc/ghi trước thao tác này sẽ ko bị đưa về sau, thao tác đọc/ghi sau ko bị đưa lên trước
- nếu sử dụng acquire và release kết hợp thì có thế có trường hợp lệnh nằm giữa 2 lệnh này. memory_order_acq_rel sẽ ko xảy ra trường hợp ấy
- Example:
```C++
#include <atomic>
#include <thread>
#include <iostream>
#include <vector>

std::atomic<int> token{0};  // Token để chuyển quyền điều khiển giữa các thread
std::vector<int> shared_data;

void thread1() {
    // Thread1 chuẩn bị dữ liệu
    shared_data.push_back(1);
    shared_data.push_back(2);
    
    // Thông báo thread2 rằng dữ liệu đã sẵn sàng bằng cách chuyển token từ 0 thành 1
    // và đồng thời lấy giá trị cũ (phải là 0)
    int old_value = token.exchange(1, std::memory_order_acq_rel);
    
    if (old_value != 0) {
        std::cout << "Thread1: Lỗi, token không phải là 0!" << std::endl;
    }
    
    // Đợi thread2 hoàn thành (token = 2)
    while (token.load(std::memory_order_acquire) != 2) {
        std::this_thread::yield();
    }
    
    // Với memory_order_acquire, thread1 sẽ thấy các thay đổi từ thread2
    std::cout << "Thread1: Dữ liệu cuối cùng có " << shared_data.size() << " phần tử" << std::endl;
}

void thread2() {
    // Đợi token chuyển thành 1 (thread1 đã chuẩn bị dữ liệu)
    while (token.load(std::memory_order_acquire) != 1) {
        std::this_thread::yield();
    }
    
    // Thread2 có thể thấy thay đổi từ thread1 nhờ memory_order_acquire
    std::cout << "Thread2: Thấy " << shared_data.size() << " phần tử từ thread1" << std::endl;
    
    // Thread2 thêm dữ liệu
    shared_data.push_back(3);
    shared_data.push_back(4);
    
    // Chuyển token từ 1 thành 2 để thông báo thread1
    // và đồng thời kiểm tra token có phải là 1 không
    int old_value = token.exchange(2, std::memory_order_acq_rel);
    
    if (old_value != 1) {
        std::cout << "Thread2: Lỗi, token không phải là 1!" << std::endl;
    }
}

int main() {
    std::thread t1(thread1);
    std::thread t2(thread2);
    
    t1.join();
    t2.join();
    
    return 0;
}
```

#### memory_order_seq_cst
- đảm bảo tính atomic của thao tác.
- hình dung mọi lệnh atomic có seq_cst của mọi luồng đặt thẳng lên một dòng thời gian duy nhất, từng lệnh một theo đúng thứ tự thực tế mà các luồng thấy.
- các thread sẽ thấy cùng 1 thứ tự thao tác nên giá trị sẽ giống nhau
- nhưng nếu store bằng seq_cst, load bằng relax thì sẽ ko chắc chắn nhìn thấy dữ liệu mới. Tôi thiểu load phải là acquire
- nếu chỉ là acq_rel thì mặc dù x cập nhật trước y nhưng 2 thread khác nhau sẽ nhìn thấy thứ tự khác. 1 thread sẽ thấy y mới x cũ, thread khác sẽ thấy y cũ x mới

## Alignment
- Trong C++ khi căn chỉnh padding 1 thuộc tính trong Struct thì Struct sẽ tự động được căn chỉnh theo "alignas" lớn nhất. Ví dụ
```c++
struct Wrapper
{
    char header;
    alignas(64) std::atomic<int64_t> safe_counter;
    alignas(32) char footer;
};
```
- Như ví dụ trên thì đối tượng "Wrapper" sẽ có địa chỉ bộ nhớ chia hết cho 64. safe_counter sẽ có địa chỉ chia hết cho 64, footer chia hết cho 32. Giữa header và safe_counter có padding 63 byte
- Điều này còn hoạt động với cả class trong class và class kế thừa class nữa.
- Ví dụ kế thừa:
```c++
// Base class
class Wrapper_1 {
public:
    char header_1;
    alignas(CACHE_LINE_SIZE) std::atomic<int64_t> safe_counter;
};

// Wrapper_2 kế thừa Wrapper_1
class Wrapper_2 : public Wrapper_1 {
public: char header_2;
};

// Wrapper_3 kế thừa Wrapper_2
class Wrapper_3 : public Wrapper_2 {
    public: char header_3;
};

// KẾT QUẢ: Wrapper_3 vẫn được căn chỉnh để có địa chỉ chia hết cho CACHE_LINE_SIZE
```
- Ví dụ class lồng nhau
```c++
// Ví dụ trong struct
struct Wrapper_1
{
    char header_1;
    alignas(CACHE_LINE_SIZE) std::atomic<int64_t> safe_counter;
};
struct Wrapper_2
{
    char header_2;
    Wrapper_1 wrapper_1;
};
struct Wrapper_3
{
    char header_3;
    Wrapper_2 wrapper_2;
};
Wrapper_3 wrapper_3;

// KẾT QUẢ wrapper_3, wrapper_2, wrapper_1 sẽ được căn chỉnh ở vị trí chia hết cho CACHE_LINE_SIZE
```