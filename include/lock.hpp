#pragma once

#include <linux/futex.h>
#include <sys/syscall.h>
#include <unistd.h>

#include <atomic>

//todo: interprocess lock with futex word relative to this

//major todo: correct size_t and int types were needed (has to work with futex API)
//adaptive spinning?

class Lock
{

private:
    enum State : int
    {
        UNLOCKED = 0, //unlocked, i.e. no one has the lock
        LOCKED = 1,   //locked and no one else waits for the lock
        CONTESTED = 2 //there are (possibly) other threads waiting for the lock
    };

    const size_t MAX_SPINNING_ACQUIRE_ITERATIONS{1000};

    //must be 32 bit int for futex to work (make this explicit int32_t)
    std::atomic<int> state{UNLOCKED};

    //note: it is fairly obvious how to make this usable as interprocess mutex:
    //the futex word must be available in different address space,
    // i.e. the atomic must be shared via shared memory between processes
    // a semaphore implementation using a futex can do this as well
    int *futexWord;

    int compareExchangeState(int expected, int desired)
    {
        state.compare_exchange_strong(expected, desired, std::memory_order_relaxed, std::memory_order_relaxed);
        return expected; //always returns the old value (which is loaded by compare_exchange in the failure case)
    }

    int exchangeState(int desired)
    {
        return state.exchange(desired, std::memory_order_relaxed);
    }

    void sleepIfContested()
    {
        //we only sleep on the futexWord if the lock is contested
        //the last 3 arguments are unused by the FUTEX_WAIT call
        syscall(SYS_futex, futexWord, FUTEX_WAIT, CONTESTED, 0, 0, 0);
    }

    void wakeOne()
    {
        //we wake 1 thread waiting on the futexWord if there is one waiting, which the API call can determine
        //the last 3 arguments are unused by the FUTEX_WAKE call
        syscall(SYS_futex, futexWord, 1, 0, 0, 0);
    }

public:
    Lock(size_t maxSpinIterations = 1)
        : MAX_SPINNING_ACQUIRE_ITERATIONS(maxSpinIterations > 0 ? maxSpinIterations : 1)
    {
        //note: check in the standard if the memory has to be interpretable this way (!extremely important!)
        //i.e. an atomic stores just raw memory for its data and nothing else
        //(or at least it has to start with this raw memory)
        //this is necessary to use it as the futex word to wait on
        futexWord = reinterpret_cast<int *>(&state);
    }

    Lock(const Lock &) = delete;
    Lock(Lock &&) = delete;

    void lock()
    {
        //try to acquire the lock by spinning
        for (size_t i = 0; i < MAX_SPINNING_ACQUIRE_ITERATIONS; ++i)
        {
            auto knownState = compareExchangeState(UNLOCKED, LOCKED);
            if (knownState == UNLOCKED)
            {
                return;
            }
            else if (knownState == CONTESTED)
            {
                //contested, do not try to spin any more and sleep instead
                //(promotes fairness with respect to threads trying to acquire the lock)
                sleepIfContested();
                break;
            }
            //it is only locked and not contested by others, try again for some fixed number of iterations
            //in the hope that the lock holder will unlock it soon,
            //possibly avoid context switch at the cost of CPU utilization without real progress

            //optional delay is possible but not advised (better to skip spinning entirely, i.e. try only once)
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
            sleepIfContested();
        }
    }

    void unlock()
    {
        //change the lock state back to unlocked and wake someone if it was contested
        if (exchangeState(UNLOCKED) == CONTESTED)
        {
            wakeOne();
        }
    }
};
