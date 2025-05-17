#pragma once

#include "AbstractSequencer.hpp"
#include "Common.hpp"
#include "Util.hpp"
#include <unordered_map>
#include "InsufficientCapacityException.hpp"
#include <cassert>

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

	public:
		int64_t tryNext() override {
			return this->tryNext(1);
		}


		int64_t tryNext(const int n) override {
			assert(sameThread() && "Accessed by two threads - use ProducerType.MULTI!");

			if (n < 1 || n > bufferSize)
			{
				throw std::invalid_argument("n must be > 0 and < bufferSize");
			}

			if (!hasAvailableCapacity(n)) {
				throw InsufficientCapacityException();
			}

			int64_t nextSequence = this->nextValue += n;

			return nextSequence;
		}


		int64_t next() override {
			return this->next(1);
		}


		int64_t next(int n) override {
			assert(sameThread() && "Accessed by two threads - use ProducerType.MULTI!");

			if (n < 1 || n > bufferSize)
			{
				throw std::invalid_argument("n must be > 0 and < bufferSize");
			}

			int64_t nextValue = this->nextValue;
			int64_t nextSequence = nextValue + n;
			int64_t wrapPoint = nextSequence - bufferSize;
			int64_t cachedGatingSequence = this->cachedValue;

			// logic chi tiết đọc ở hasAvailableCapacity
			if (cachedGatingSequence < wrapPoint) {
				// chờ cho tới khi consumer xử lý xong để có chỗ trống ghi dữ liệu
				int64_t minSequence;
				while (wrapPoint > (minSequence = Util::getMinimumSequence(gatingSequences, nextValue))) {
					// TODO: Use waitStrategy to spin?
					std::this_thread::sleep_for(std::chrono::nanoseconds(1));
				}
				this->cachedValue = minSequence;
			}

			this->nextValue = nextSequence;

			return true;
		}


		int64_t remainingCapacity() const override {
			long consumed = Util::getMinimumSequence(this->gatingSequences, nextValue);
			long produced = this->nextValue;
			return this->bufferSize - (produced - consumed);
		}


		void claim(const int64_t sequence) override {
			this->nextValue = sequence;
		}


		void publish(const int64_t sequence) override {
			this->cursor.set(sequence);
			this->waitStrategy->signalAllWhenBlocking();
		}


		void publish(const int64_t lo, const int64_t hi) override {
			this->publish(hi);
		}


		bool isAvailable(const int64_t sequence) const override {
			int64_t currentSequence = this->cursor.get();
			return sequence <= currentSequence && sequence > currentSequence - bufferSize;
		}


		// trong các sequence barrier thì thường "nextSequence" là sequence mà barrier đang chờ, availableSequence là sequence đã được publish hoặc các dependency consmer xử lý xong
		// trong hàm waitFor của SequenceBarrier thì nextSequence <= availableSequence thì mới gọi tới đây
		int64_t getHighestPublishedSequence(int64_t nextSequence, int64_t availableSequence) const override {
			return availableSequence;
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


		friend std::ostream& operator<<(std::ostream& os, const SingleProducerSequencer& sequencer);


	};


	std::ostream& operator<<(std::ostream& os, const SingleProducerSequencer& sequencer) {
		os << "SingleProducerSequencer{"
			<< "bufferSize=" << sequencer.bufferSize
			<< ", waitStrategy=" << sequencer.waitStrategy->toString()
			<< ", cursor=" << sequencer.cursor
			<< ", gatingSequences=[";

		for (size_t i = 0; i < sequencer.gatingSequences.size(); ++i) {
			if (i > 0) {
				os << ", ";
			}
			os << sequencer.gatingSequences[i];
		}
		os << "]}";

		return os;
	}


}
