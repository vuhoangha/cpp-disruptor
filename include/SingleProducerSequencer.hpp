#pragma once

#include "AbstractSequencer.hpp"
#include "Common.hpp"
#include "Util.hpp"

namespace disruptor {
	class SingleProducerSequencer : public AbstractSequencer {
	private:
		// sắp xếp 2 biến này vào 1 cache line riêng vì chúng thường được dùng cùng nhau, cùng 1 thread
		// seq gần nhất đã được publisher claim
		alignas(CACHE_LINE_SIZE) int64_t nextValue{ INITIAL_CURSOR_VALUE };

		// seq nhỏ nhất đã được các gatingSequences xử lý
		int64_t cachedValue{ INITIAL_CURSOR_VALUE };

		// Padding sau value để đảm bảo không có biến nào của đối tượng khác nằm trên cùng cache line
		char padding[CACHE_LINE_SIZE - sizeof(std::atomic<int64_t>) * 2];

		// kiểm tra xem 1 vị trí trong RingBuffer đã được các consumer xử lý xong chưa để publisher ghi dữ liệu vào
		bool hasAvailableCapacity(const int requiredCapacity) {
			int64_t nextValue = this->nextValue;
			int64_t wrapPoint = nextValue + requiredCapacity - bufferSize;
			int64_t cachedGatingSequence = this->cachedValue;

			// wrapPoint: chính là vị trí lớn nhất sẽ được publisher lấy nhưng tua lại 1 vòng
			//              Khi các consumer chưa xử lý xong vị trí wrapPoint này thì đồng nghĩa ko thể ghi đè dữ liệu vào đươc
			// dưới đây tôi có bỏ bớt logic so với mã nguồn vì các case họ cover cho tính năng thì tôi ko support tính năng ấy
			// https://github.com/LMAX-Exchange/disruptor/issues/291
			// https://github.com/LMAX-Exchange/disruptor/issues/280
			if (cachedGatingSequence < wrapPoint) {
				int64_t minSequence = Util::getMinimumSequence(gatingSequences, nextValue); // truyền vào nextValue vì nó là sequence lớn nhất, sequence consumer xử lý sẽ <= nextValue
				if (minSequence < wrapPoint) {
					return false;
				}
			}
			return true;
		}

		bool sameThread() {
			return ProducerThreadAssertion::isSameThreadProducingTo(this);
		}


		/**
		* Only used when assertions are enabled.
		*/
		class ProducerThreadAssertion {
		private:
			/**
			 * Tracks the threads publishing to SingleProducerSequencers to identify if more than one
			 * thread accesses any SingleProducerSequencer.
			 * I.e. it helps developers detect early if they use the wrong ProducerType.
			 */
			static inline std::unordered_map<SingleProducerSequencer*, std::thread::id> PRODUCERS;
			static inline std::mutex producersMutex;

		public:
			static bool isSameThreadProducingTo(SingleProducerSequencer* singleProducerSequencer) {
				std::lock_guard<std::mutex> lock(producersMutex);

				const std::thread::id currentThread = std::this_thread::get_id();
				if (PRODUCERS.find(singleProducerSequencer) == PRODUCERS.end()) {
					PRODUCERS[singleProducerSequencer] = currentThread;
				}

				return PRODUCERS[singleProducerSequencer] == currentThread;
			}
		};

	};
}
