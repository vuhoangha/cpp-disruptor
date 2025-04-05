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
- sử dụng với thao tác đọc (load).
- Đảm bảo rằng mọi thao tác sau thao tác này không bị đưa lên trước nó.
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
- Đảm bảo rằng mọi thao tác trước thao tác này không bị đưa xuống sau nó.
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
- đảm bảo thao tác trước thao tác này sẽ ko bị đưa về sau, thao tác sau ko bị đưa lên trước
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
- memory_order_acquire đảm bảo các lệnh phía sau không được thực thi trước nhưng memory_order_acquire lại chỉ áp dụng nếu thao tác hiện tại là đọc (load)
Ví dụ:
```C++
// lỗi vì memory_order_acquire chỉ áp dụng với thao tác load
data_ready.store(true, std::memory_order_acquire);
std::cout << "Dữ liệu nhận được: " << shared_data << std::endl;
```
- memory_order_release đảm bảo các lệnh phía trước không được thực thi sau nhưng memory_order_release lại chỉ áp dụng nếu thao tác hiện tại là ghi (store).
Ví dụ:
```C++
// lỗi vì memory_order_release chỉ áp dụng với thao tác store
std::cout << "Dữ liệu nhận được: " << shared_data << std::endl;
while (!data_ready.load(std::memory_order_release)) {
}
```
- các memory order trên đều chỉ áp dụng với riêng vị trí biến đó đứng, còn các dòng code phía trước hoặc phía sau có thể tự nó bị xáo trộn.
Ví dụ:
```C++
// B,C chắc chắn thực thi sau A nhưng C có thể thực thi trước B
A.load(std::memory_order_acquire);
B.load(std::memory_order_relaxed);
C.load(std::memory_order_relaxed);

// B,C chắc chắn thực thi trước A nhưng C có thể thực thi trước B
B.load(std::memory_order_relaxed);
C.load(std::memory_order_relaxed);
A.store(std::memory_order_release);
```
- với memory_order_seq_cst thì thứ tự thực thi của các dòng code sẽ luôn cố định thay vì chỉ cục bộ theo từng biến, từng dòng.
Ví dụ:
```C++
// chắc chắn sẽ thực thi theo thứ tự A --> B --> C
A.load(std::memory_order_seq_cst);
B.load(std::memory_order_seq_cst);
C.load(std::memory_order_seq_cst);
```
- Dưới đây là 1 ví dụ mà rất cần phải có "memory_order_seq_cst", nếu ko thread C có nguy cơ chạy vĩnh viễn:
```C++
#include <atomic>
#include <thread>
#include <iostream>
#include <vector>

// Ba biến atomic
std::atomic<int> token{0};
std::atomic<bool> data_ready_A{false};
std::atomic<bool> data_ready_B{false};

// Dữ liệu chung
std::vector<int> shared_data;

void thread_A() {
    // Thread A chuẩn bị dữ liệu
    shared_data.push_back(1);
    shared_data.push_back(2);
    
    // Đánh dấu dữ liệu A đã sẵn sàng
    data_ready_A.store(true, std::memory_order_seq_cst);
    
    // Kiểm tra xem dữ liệu B đã sẵn sàng chưa
    if (data_ready_B.load(std::memory_order_seq_cst)) {
        // Nếu B đã sẵn sàng, lấy token trước
        token.store(1, std::memory_order_seq_cst);
    }
}

void thread_B() {
    // Thread B chuẩn bị dữ liệu
    shared_data.push_back(3);
    shared_data.push_back(4);
    
    // Đánh dấu dữ liệu B đã sẵn sàng
    data_ready_B.store(true, std::memory_order_seq_cst);
    
    // Kiểm tra xem dữ liệu A đã sẵn sàng chưa
    if (data_ready_A.load(std::memory_order_seq_cst)) {
        // Nếu A đã sẵn sàng, lấy token trước
        token.store(2, std::memory_order_seq_cst);
    }
}

void thread_C() {
    // Đợi cho đến khi token được đặt (1 hoặc 2)
    while (token.load(std::memory_order_seq_cst) == 0) {
        std::this_thread::yield();
    }
    
    // Xử lý dữ liệu theo thứ tự dựa vào token
    if (token.load(std::memory_order_seq_cst) == 1) {
        std::cout << "Thread A đã lấy token trước, xử lý dữ liệu theo trình tự A->B" << std::endl;
        // Xử lý dữ liệu theo thứ tự A rồi đến B
    } else {
        std::cout << "Thread B đã lấy token trước, xử lý dữ liệu theo trình tự B->A" << std::endl;
        // Xử lý dữ liệu theo thứ tự B rồi đến A
    }
}

int main() {
    std::thread tA(thread_A);
    std::thread tB(thread_B);
    std::thread tC(thread_C);
    
    tA.join();
    tB.join();
    tC.join();
    
    return 0;
}
```