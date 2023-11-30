/**
* Copyright (C) 2022 Xilinx, Inc
*
* Licensed under the Apache License, Version 2.0 (the "License"). You may
* not use this file except in compliance with the License. A copy of the
* License is located at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
* WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
* License for the specific language governing permissions and limitations
* under the License.
*/

/* Xilinx XRT test case utility header file */

#ifndef XIL_UTIL_HPP
#define XIL_UTIL_HPP

#include <mutex>
#include <condition_variable>

/* std::barrier can be used since c++20
 * Thanks boost library. Modified a little bit with pure c++11 code
 */
class barrier
{
    static inline unsigned int check_counter(unsigned int count) {
        if (count == 0)
            throw std::runtime_error("barrier count cannot be zero");
        return count;
    }

    public:
    barrier()
        : m_count(1)
        , m_generation(0)
        , m_count_reset_val(1)
    {}

    barrier(int count)
        : m_count(check_counter(count))
          , m_generation(0)
          , m_count_reset_val(count)
    {}

    void init(int count) {
        m_count = check_counter(count);
        m_count_reset_val = m_count;
    }

    void wait() {
        std::unique_lock<std::mutex> lk(m_mutex);
        int gen = m_generation;

        if (--m_count == 0) {
            m_generation++;
            m_count = m_count_reset_val;
            m_cv.notify_all();
            return;
        }

        while (static_cast<long unsigned int>(gen) == m_generation)
            m_cv.wait(lk);
    }

    private:
    std::mutex m_mutex;
    std::condition_variable m_cv;
    unsigned int m_count;
    unsigned int m_generation;
    unsigned int m_count_reset_val;
};

#endif
