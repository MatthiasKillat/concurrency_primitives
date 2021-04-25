#pragma once

#include <atomic>

template <typename Semaphore>
class AutoResetEvent;

//for ctor injection, assumes semaphore exists throughout lifetime
template <typename Semaphore>
class GenericAutoResetEvent
{
public:
    GenericAutoResetEvent(Semaphore &semaphore, int64_t initialCount = 0) : m_semaphore(&semaphore), m_count(initialCount)
    {
        if (m_count > 1)
        {
            m_count = 1;
        }
        // allow negative initial count?
    }

    GenericAutoResetEvent(int64_t initialCount = 0) : m_count(initialCount)
    {
        if (m_count > 1)
        {
            m_count = 1;
        }
        // allow negative initial count?
    }

    void signal()
    {
        auto count = m_count.load(std::memory_order_relaxed);
        do
        {
            auto newCount = count < 1 ? count + 1 : 1; // saturates
            //we sync the memory in the success case even if we do not increment if it is already 1
            if (m_count.compare_exchange_weak(count, newCount, std::memory_order_release, std::memory_order_relaxed))
            {
                break;
            }

        } while (true);

        if (count < 0)
        {
            // slow path
            // someone was (and possibly is) waiting, wake one of them up
            m_semaphore->post();
        }
    }

    void wait()
    {
        auto count = m_count.fetch_sub(1, std::memory_order_relaxed);

        // if it was 1, we decrement and skip the wait (it was already signaled before the fetch_sub)
        // (fast path)
        if (count < 1)
        {
            // slow path
            //otherwise <= 0 and we wait (if signalled in the meantime the we continue right away)
            m_semaphore->wait();
        }
    }

private:
    Semaphore *m_semaphore; //after construction will always be a pointer (life time not controlled here though)
    //m_count is always <= 1, with 1 indicating it was signalled
    //                             0 not signaled, no waiting threads
    //                             -n, n<0, n threads waiting for a signal

    // could protect against underflow but not needed if there is only a well known number of waiters (e.g. 1)
    std::atomic<int64_t> m_count;

    template <typename S>
    friend class AutoResetEvent;

protected:
    void setSemaphore(Semaphore &semaphore)
    {
        m_semaphore = &semaphore;
    }
};

template <typename Semaphore>
class AutoResetEvent : public GenericAutoResetEvent<Semaphore>
{
public:
    AutoResetEvent() : GenericAutoResetEvent<Semaphore>()
    {
        m_semaphore = new (std::nothrow) Semaphore(); //TODO: would be created in shared memory

        if (!m_semaphore)
        {
            std::terminate();
        }
        //we need to be able to guarantee that semaphore is valid in the object
        this->setSemaphore(*m_semaphore);
    }

    ~AutoResetEvent()
    {
        if (m_semaphore)
        {
            delete m_semaphore;
        }
        //base class is not allowed to use it anymore (misuse if e.g. someone is still waiting)
    }

private:
    Semaphore *m_semaphore; //responsible for semaphore lifetime
};