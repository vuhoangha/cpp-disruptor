/*
 * Copyright 2012 LMAX Ltd.
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

#include <vector>
#include <memory>
#include <string>
#include <span>
#include <algorithm>
#include <stdexcept>
#include <format>

#include "Sequence.hpp"
#include "Util.hpp"

namespace disruptor
{

    /**
     * Hides a group of Sequences behind a single Sequence
     */
    class FixedSequenceGroup final : public Sequence
    {
    private:
        // Store sequences in a vector for better cache locality during get() calls
        std::vector<std::shared_ptr<Sequence>> sequences;

    public:
        /**
         * Constructor
         *
         * @param sequences the list of sequences to be tracked under this sequence group
         */
        explicit FixedSequenceGroup(std::span<const std::shared_ptr<Sequence>> sequencesSpan)
        {
            // Reserve exact size to avoid reallocations
            sequences.reserve(sequencesSpan.size());

            // Copy all sequences into our vector
            std::ranges::copy(sequencesSpan, std::back_inserter(sequences));
        }

        // Alternative constructor from vector (avoids unnecessary conversion)
        explicit FixedSequenceGroup(const std::vector<std::shared_ptr<Sequence>> &sequencesVec)
            : sequences(sequencesVec)
        {
        }

        // Move constructor for better performance
        explicit FixedSequenceGroup(std::vector<std::shared_ptr<Sequence>> &&sequencesVec) noexcept
            : sequences(std::move(sequencesVec))
        {
        }

        /**
         * Get the minimum sequence value for the group.
         *
         * @return the minimum sequence value for the group.
         */
        [[nodiscard]] inline int64_t get() const override
        {
            /**
             * __PERF:
             *     - với sequences.length >=3
             *     sử dụng thread local cho 2 biến giá trị nhỏ nhất và index tương ứng.
             *     chỗ này xem xét cache lại giá trị nhỏ nhất và index trong vector sequences tương ứng.
             *         Lúc vào ta sẽ check xem giá trị tại index kia có thay đổi ko. Nếu ko đổi thì trả về giá trị cũ luôn
             *         Nếu đổi thì ta sẽ lặp qua lần lượt từng sequence, cái nào có giá trị bằng giá trị đã trả trước đó
             *             thì trả luôn giá trị đó, cache lại index của sequence tương ứng
             *         Nếu không thì ta vẫn sẽ lặp qua hết sequences, sẽ có giá trị nhỏ nhất và index tương ứng. Cache lại và trả ra giá trị
             *     - với sequences.length = 1 hoặc 2 thì ta lấy trực tiếp luôn cho đơn giản
             *     - nghĩ cách để trình biên dịch biết luôn từ lúc biên dịch nhé
             */
            return Util::getMinimumSequence(sequences);
        }

        /**
         * String representation for debugging
         */
        [[nodiscard]] std::string toString() const override
        {
            std::string result = "[";
            bool first = true;

            for (const auto &seq : sequences)
            {
                if (!first)
                    result += ", ";

                if (seq)
                    result += std::to_string(seq->get());
                else
                    result += "null";

                first = false;
            }

            result += "]";
            return result;
        }

        /**
         * Not supported.
         */
        void set(int64_t value) override
        {
            throw std::runtime_error("FixedSequenceGroup does not support set operation");
        }

        /**
         * Not supported.
         */
        bool compareAndSet(int64_t expectedValue, int64_t newValue) override
        {
            throw std::runtime_error("FixedSequenceGroup does not support compareAndSet operation");
        }

        /**
         * Not supported.
         */
        int64_t incrementAndGet() override
        {
            throw std::runtime_error("FixedSequenceGroup does not support incrementAndGet operation");
        }

        /**
         * Not supported.
         */
        int64_t addAndGet(int64_t increment) override
        {
            throw std::runtime_error("FixedSequenceGroup does not support addAndGet operation");
        }
    };

} // namespace disruptor