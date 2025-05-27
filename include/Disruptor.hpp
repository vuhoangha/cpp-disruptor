#pragma once
#include "RingBuffer.hpp"

namespace disruptor {
    template<typename T, size_t N>
    class Disruptor {
        RingBuffer<T> ringBuffer;

tiep tuc disruptor nhes

        đoạn này hiểu nguyên lý của nó rồi
        giờ phải tạo các interface processor. Batch processor chỉ là kế thừa
        với hàm after thì truyền vào processor, từ đó tạo ra sequence barrier tương ứng để lắng nghe processor trong after
        trong handleEventsWith thì lấy sequence barrier tạo trước đó add vào các processor sau này để lắng nghe sự kiện

    };
}
