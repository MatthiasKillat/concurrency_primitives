#pragma once

#include "semaphore.hpp"
#include <atomic>

class AutoResetEvent
{
public:
    AutoResetEvent(int64_t initialCount = 0) : m_count(initialCount)
    {
        if (m_count > 1)
        {
            m_count = 1;
        }
    }

    void signal()
    {
        auto count = m_count.load(std::memory_order_relaxed);
        do
        {
            auto newCount = count < 1 ? count + 1 : 1;
            //we sync the memory even if we do not increment since it is already 1
            if (m_count.compare_exchange_weak(count, newCount, std::memory_order_release, std::memory_order_relaxed))
            {
                break;
            }

        } while (true);

        if (count < 0)
        {
            //someone must be waiting, wake up one of them
            m_semaphore.post();
        }
    }

    void wait()
    {
        auto count = m_count.fetch_sub(1, std::memory_order_relaxed);

        //if it was 1, we decrement and skip the wait (it was already signaled before the fetch_sub)
        if (count < 1)
        {
            //otherwise <= 0 and we wait
            m_semaphore.wait();
        }
    }

private:
    //m_count is always <= 1, with 1 indicating it was signalled
    //                             0 not signaled, no waiting threads
    //                             -n, n<0, n threads waiting for a signal
    std::atomic<int64_t> m_count;
    Semaphore m_semaphore;
};