#pragma once

#include <semaphore.h>

#include <exception>

//light wrapper for benchmark, no error checking
class PosixSemaphore
{
public:
    PosixSemaphore(int initialValue = 0)
    {
        if (sem_init(&sem, 0, initialValue) == -1)
        {
            throw(std::exception());
        }
    }

    ~PosixSemaphore()
    {
        sem_destroy(&sem);
    }

    PosixSemaphore(const PosixSemaphore &) = delete;
    PosixSemaphore(PosixSemaphore &&) = delete;

    bool tryWait()
    {
        if (sem_trywait(&sem) == 0)
        {
            return true;
        }
        return false;
    }

    void wait()
    {
        sem_wait(&sem);
    }

    void post(int count = 1)
    {
        while (count-- > 0)
        {
            sem_post(&sem);
        }
    }

private:
    sem_t sem;
};