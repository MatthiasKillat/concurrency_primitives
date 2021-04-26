#pragma once

#include <linux/futex.h>
#include <sys/syscall.h>
#include <unistd.h>

#include <atomic>
#include "semaphore.hpp"

using lock_id_t = int32_t;

class IdLock
{

private:
    enum State : int32_t
    {
        UNLOCKED = -1, //unlocked, i.e. no one has the lock
        CONTESTED = -2 //there are (possibly) other threads waiting for the lock
    };                 //everything else is locked

    const uint32_t MAX_SPINNING_ACQUIRE_ITERATIONS{1000};

    //must be 32 bit int for futex to work (make this explicit int32_t)
    //if we do not use the futex anymore (but a semaphore instead) this can be relaxed
    std::atomic<int32_t> state{UNLOCKED};
    std::atomic<lock_id_t> lockingId;

    //note: it is fairly obvious how to make this usable as interprocess mutex:
    //the futex word must be available in different address space,
    // i.e. the atomic must be shared via shared memory between processes
    // a semaphore implementation using a futex can do this as well
    //int32_t *futexWord;

    Semaphore semaphore;

    int compareExchangeState(int32_t expected, int32_t desired)
    {
        state.compare_exchange_strong(expected, desired, std::memory_order_relaxed, std::memory_order_relaxed);
        return expected; //always returns the old value (which is loaded by compare_exchange in the failure case)
    }

    int32_t exchangeState(int32_t desired)
    {
        return state.exchange(desired, std::memory_order_relaxed);
    }

    // void sleepIfContested()
    // {
    //     //we only sleep on the futexWord if the lock is contested
    //     //the last 3 arguments are unused by the FUTEX_WAIT call
    //     syscall(SYS_futex, futexWord, FUTEX_WAIT, CONTESTED, 0, 0, 0);
    // }

    // void wakeOne()
    // {
    //     //we wake 1 thread waiting on the futexWord if there is one waiting, which the API call can determine
    //     //the last 3 arguments are unused by the FUTEX_WAKE call
    //     syscall(SYS_futex, futexWord, 1, 0, 0, 0);
    // }

public:
    IdLock(uint32_t maxSpinIterations = 1)
        : MAX_SPINNING_ACQUIRE_ITERATIONS(maxSpinIterations > 0 ? maxSpinIterations : 1)
    {
        //futexWord = reinterpret_cast<int *>(&state);
    }

    IdLock(const IdLock &) = delete;
    IdLock(IdLock &&) = delete;

    //only positive values (>0), can foolproof this later (need int32_t for futexwords though)
    void lock(lock_id_t id = 1)
    {
        //try to acquire the lock by spinning
        for (uint32_t i = 0; i < MAX_SPINNING_ACQUIRE_ITERATIONS; ++i)
        {
            auto knownState = compareExchangeState(UNLOCKED, id);
            if (knownState == UNLOCKED)
            {
                lockingId.store(id, std::memory_order_relaxed);
                return;
            }
            else if (knownState == CONTESTED)
            {
                //contested, do not try to spin any more and sleep instead
                //(promotes fairness with respect to threads trying to acquire the lock)
                //sleepIfContested(); //could use a semaphore to wait as well
                semaphore.wait();
                break;
            }
        }

        //spinning failed, assume the lock is contested and change its state accordingly,
        //sleep while it is actually contested or locked

        while (exchangeState(CONTESTED) != UNLOCKED)
        {
            //note that the contested state can be a false positive, i.e. might not be contested anymore when
            //we set it to contested, but then we do not sleep here,
            //i.e. this is just a pessimistic but safe assumption which optimzes the logic

            //note that we also do not sleep when someone sets it back to UNLOCKED before the exchange
            //and just set it to CONTESTED (false positive) and return, having acquired the lock
            //sleepIfContested();
            semaphore.wait();
        }

        lockingId.store(id, std::memory_order_relaxed);
    }

    void unlock()
    {
        lockingId.store(0); //slightly out of sync, but has to be done before exchange
        //change the lock state back to unlocked and wake someone if it was contested
        if (exchangeState(UNLOCKED) == CONTESTED)
        {
            //wakeOne();
            semaphore.post();
        }
    }

    void unlock(lock_id_t id)
    {
        if (getLockingId() != id)
        {
            std::terminate(); //protocol error
        }
        lockingId.store(0); //slightly out of sync, but has to be done before exchange
        //change the lock state back to unlocked and wake someone if it was contested
        if (exchangeState(UNLOCKED) == CONTESTED)
        {
            //wakeOne();
            semaphore.post();
        }
    }

    lock_id_t getLockingId()
    {
        return lockingId.load(std::memory_order_relaxed);
    }
};