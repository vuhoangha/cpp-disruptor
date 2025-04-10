/*
 * Copyright 2011 LMAX Ltd.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#pragma once

#include <atomic>
#include <vector>
#include <memory>
#include <string>
#include <stdexcept>
#include <bit>
#include <algorithm>

#include "Sequencer.hpp"
#include "Sequence.hpp"
#include "WaitStrategy.hpp"
#include "SequenceBarrier.hpp"
#include "ProcessingSequenceBarrier.hpp"
#include "DataProvider.hpp"
#include "Util.hpp"

namespace disruptor {

/**
 * Base class for the various sequencer types (single/multi). Provides
 * common functionality like the management of gating sequences (add/remove) and
 * ownership of the current cursor.
 */
class AbstractSequencer : public Sequencer {
protected:
    const int bufferSize;
    const std::shared_ptr<WaitStrategy> waitStrategy;
    Sequence cursor{INITIAL_CURSOR_VALUE};
    std::vector<std::shared_ptr<Sequence>> gatingSequences;

public:
    /**
     * Create with the specified buffer size and wait strategy.
     *
     * @param bufferSize   The total number of entries, must be a positive power of 2.
     * @param waitStrategy The wait strategy used by this sequencer
     */
    AbstractSequencer(int bufferSize, std::shared_ptr<WaitStrategy> waitStrategy)
        : bufferSize(bufferSize), waitStrategy(waitStrategy) {
        if (bufferSize < 1) {
            throw std::invalid_argument("bufferSize must not be less than 1");
        }
        if (std::popcount(static_cast<unsigned int>(bufferSize)) != 1) {
            throw std::invalid_argument("bufferSize must be a power of 2");
        }
    }

    virtual ~AbstractSequencer() = default;

    /**
     * @see Sequencer#getCursor()
     */
    int64_t getCursor() const override {
        return cursor.get();
    }

    /**
     * @see Sequencer#getBufferSize()
     */
    int getBufferSize() const override {
        return bufferSize;
    }

    /**
     * @see Sequencer#addGatingSequences(Sequence...)
     */
    void addGatingSequences(const std::vector<std::shared_ptr<Sequence>>& sequences) override {
        // Lấy giá trị cursor hiện tại
        int64_t cursorSequence = getCursor();
        
        // Dự trữ không gian cho vector
        gatingSequences.reserve(gatingSequences.size() + sequences.size());
        
        // Thiết lập giá trị và thêm các sequence mới
        for (const auto& sequence : sequences) {
            sequence->set(cursorSequence);
            gatingSequences.push_back(sequence);
        }
    }

    /**
     * @see Sequencer#removeGatingSequence(Sequence)
     */
    bool removeGatingSequence(const std::shared_ptr<Sequence>& sequence) override {
        // Kiểm tra xem sequence có tồn tại không
        auto it = std::find(gatingSequences.begin(), gatingSequences.end(), sequence);
        if (it == gatingSequences.end()) {
            return false;
        }
        
        // Xoá tất cả các phiên bản của sequence (nếu có nhiều hơn một)
        auto newEnd = std::remove(gatingSequences.begin(), gatingSequences.end(), sequence);
        gatingSequences.erase(newEnd, gatingSequences.end());
        
        return true;
    }

    /**
     * @see Sequencer#getMinimumSequence()
     */
    int64_t getMinimumSequence() const override {
        return Util::getMinimumSequence(gatingSequences, cursor.get());
    }

    /**
     * @see Sequencer#newBarrier(Sequence...)
     */
    std::shared_ptr<SequenceBarrier> newBarrier(
        const std::vector<std::shared_ptr<Sequence>>& sequencesToTrack) override {
        return std::make_shared<ProcessingSequenceBarrier>(
            this,
            waitStrategy,
            cursor,
            sequencesToTrack);
    }


    std::string toString() const {
        std::string result = "AbstractSequencer{waitStrategy=";
        result += waitStrategy->toString();
        result += ", cursor=" + cursor.toString();
        result += ", gatingSequences=[";
        
        for (size_t i = 0; i < gatingSequences.size(); ++i) {
            if (i > 0) {
                result += ", ";
            }
            result += gatingSequences[i]->toString();
        }
        result += "]}";
        
        return result;
    }
};

} // namespace disruptor
 