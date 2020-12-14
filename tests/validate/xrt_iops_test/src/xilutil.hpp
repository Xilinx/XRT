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
        : m_generation(0)
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

        while (gen == m_generation)
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
