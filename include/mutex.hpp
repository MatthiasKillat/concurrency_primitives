#pragma once

#include <semaphore.hpp>
#include <atomic>

//a simple mutex (without spinlock optimization) based on our semaphore implementation

class Mutex
{
private:
    std::atomic<int> contenders{0};
    Semaphore semaphore{0};

public:
    Mutex() = default;

    Mutex(const Mutex &) = delete;
    Mutex(Mutex &&) = delete;

    Mutex &operator=(const Mutex &) = delete;
    Mutex &operator=(Mutex &&) = delete;

    void lock()
    {
        if (contenders.fetch_add(1, std::memory_order_acquire) > 0)
        {
            semaphore.wait();
        }
    }

    void unlock()
    {
        if (contenders.fetch_sub(1, std::memory_order_release) > 1)
        {
            semaphore.post();
        }
    }
};