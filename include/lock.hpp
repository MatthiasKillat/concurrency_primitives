#pragma once

#include <linux/futex.h>
#include <sys/syscall.h>
#include <unistd.h>

#include <atomic>

//or name it Mutex?
class Lock
{

private:
    enum State : int
    {
        UNLOCKED = 0, //unlocked, i.e. no one has the lock
        LOCKED = 1,   //locked and no one else waits for the lock
        CONTESTED = 2 //there are (possibly) other threads waiting for the lock
    };

    //TODO: could be made configurable
    static constexpr size_t MAX_SPINNING_ACQUIRE_ITERATIONS = 100;

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
    Lock()
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
        //sleep while it is actually contested

        while (exchangeState(CONTESTED) == CONTESTED)
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

#if 0

//previous implementation, but seems to be unecessily complicated for no gain
class Lock
{

private:
    enum State : int
    {
        UNLOCKED = 0, //unlocked, i.e. no one has the lock
        LOCKED = 1,   //locked and no one else waits for the lock
        CONTESTED = 2 //there are (possibly) other threads waiting for the lock
    };

    static constexpr size_t MAX_SPIN_ITERATIONS = 100;

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

    void sleepIfContested()
    {
        //we only sleep on the futexWord if the lock is contested
        //the last 3 arguments are unused by the FUTEX_WAIT call
        syscall(SYS_futex, futexWord, FUTEX_WAIT, CONTESTED, 0, 0, 0);
    }

    void wakeOne()
    {
        //we wake 1 thread waiting on the futexWord (if there is one waiting)
        //the last 3 arguments are unused by the FUTEX_WAKE call
        syscall(SYS_futex, futexWord, 1, 0, 0, 0);
    }

    bool acquireBySpinning(int &knownState, size_t iterations = 0)
    {
        for (size_t i = 0; i < iterations; ++i)
        {
            knownState = compareExchangeState(UNLOCKED, CONTESTED);
            if (knownState == UNLOCKED)
            {
                return true;
            }
            //optional delay is possible but not advised (better to skip spinning entirely)
        }
        return false;
    }

public:
    Lock()
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

        //if it is unlocked we can take the fast path and just lock it
        auto knownState = compareExchangeState(UNLOCKED, LOCKED);

        //if the previous state was unlocked we have acquired the lock and return
        if (knownState == UNLOCKED)
        {
            return;
        }

        //previous state was not unlocked, no fast path

        //try to spin for a while to acquire the lock
        //note that this spinning is optional and can be removed
        //depending on the contention it might avoid a context switch or actually be detrimental

        if (acquireBySpinning(knownState, MAX_SPIN_ITERATIONS))
        {
            return;
        }

        //spinning failed

        do
        {
            //set to contested if it not already was contested
            if (knownState == CONTESTED || compareExchangeState(LOCKED, CONTESTED) != UNLOCKED)
            {
                //sleep if it is still contested (another thread might have changed this)
                sleepIfContested();
            }

            //was unlocked or we woke up

            //try to lock it but this time by pessimistically setting it to contested since
            //we cannot be sure no one else is making progress towards locking it but has not set the state yet
            knownState = compareExchangeState(UNLOCKED, CONTESTED);
        } while (knownState != UNLOCKED);
    }

    //note if we use unlock before locking (undefined behavior), this leads to state = -1 temporarily
    //which is still ok since this is undefined behvior (and will be corrected in the store afterwards)
    //using the compare exchange approach below this could be avoided (at a performance loss)
    /*
    void unlock()
    {
        //was the lock contested?
        //if the fetch_sub returned LOCKED, the lock is (potentially) contested and we do need to wake someone
        //otherwise we have unlocked the lock and are done
        if (state.fetch_sub(1, std::memory_order_relaxed) != LOCKED)
        {
            //yes, so we need to unlock it and wake one other thread

            
            state.store(UNLOCKED, std::memory_order_relaxed);
            wakeOne();
        }
    }
*/
    void unlock()
    {
        constexpr bool notUnlocked = true;
        do
        {
            //optimistically assume it was locked and not contested
            auto knownState = compareExchangeState(LOCKED, UNLOCKED);
            if (knownState == LOCKED)
            {
                //was locked but NOT contested
                return;
            }

            //must have been contested then
            knownState = compareExchangeState(CONTESTED, UNLOCKED);
            if (knownState == CONTESTED)
            {
                //was contested, we need to wake someone
                wakeOne();
                return;
            }

            if (knownState == UNLOCKED)
            {
                return; //was already unlocked, should not happen (misuse case mitigated)
            }
        } while (notUnlocked);
    }
};

#endif