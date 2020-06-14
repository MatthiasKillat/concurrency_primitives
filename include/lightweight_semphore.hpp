#pragma once

#include <atomic>

template <typename Semaphore, int MAX_SPIN = 100000>
class LightweightSemaphore
{
private:
    std::atomic<int> m_count;
    Semaphore m_semaphore;
    int m_spin{MAX_SPIN};

    // void waitWithSpinning()
    // {
    //     int oldCount;
    //     int spin = MAX_SPIN;
    //     while (spin--)
    //     {
    //         oldCount = m_count.load(std::memory_order_relaxed);
    //         if ((oldCount > 0) && m_count.compare_exchange_strong(oldCount, oldCount - 1, std::memory_order_acquire))
    //             return;
    //         std::atomic_signal_fence(std::memory_order_acquire); //prevent reordering
    //     }
    //     oldCount = m_count.fetch_sub(1, std::memory_order_acquire);
    //     if (oldCount <= 0)
    //     {
    //         m_semaphore.wait();
    //     }
    // }

    void increaseSpin(int a = 2)
    {
        m_spin *= a;
        m_spin = m_spin > MAX_SPIN ? MAX_SPIN : m_spin;
    }

    void decreaseSpin(int a = 2)
    {
        m_spin /= a;
        m_spin = m_spin < 1 ? 1 : m_spin;
    }

    //maybe not really better than always spinning max time?
    void waitWithAdaptiveSpinning()
    {
        int oldCount;
        int spin = m_spin;
        while (spin--)
        {
            oldCount = m_count.load(std::memory_order_relaxed);
            if ((oldCount > 0) && m_count.compare_exchange_strong(oldCount, oldCount - 1, std::memory_order_acquire))
            {
                increaseSpin(); //we were successful while spinning, increase the spin time
                return;
            }
            std::atomic_signal_fence(std::memory_order_acquire); //prevent reordering
        }
        oldCount = m_count.fetch_sub(1, std::memory_order_acquire);
        if (oldCount <= 0)
        {
            decreaseSpin(); //we were not successful while spinning, decrease the spin time
            m_semaphore.wait();
        }
    }

public:
    LightweightSemaphore(int initialCount = 0) : m_count(initialCount >= 0 ? initialCount : 0)
    {
    }

    bool tryWait()
    {
        int oldCount = m_count.load(std::memory_order_relaxed);
        return (oldCount > 0 && m_count.compare_exchange_strong(oldCount, oldCount - 1, std::memory_order_acquire));
    }

    void wait()
    {
        if (!tryWait())
        {
            waitWithAdaptiveSpinning();
        }
    }

    void post(int count = 1)
    {
        int oldCount = m_count.fetch_add(count, std::memory_order_release);
        int toRelease = -oldCount < count ? -oldCount : count;
        if (toRelease > 0)
        {
            m_semaphore.post(toRelease);
        }
    }
};