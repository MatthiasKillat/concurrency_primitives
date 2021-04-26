#pragma once

#include <linux/futex.h>
#include <sys/syscall.h>
#include <unistd.h>

#include <atomic>
#include "semaphore.hpp"

//can be used to build a recursive mutex if id is a unique thread id
class IdAwareLock
{
public:
    //guaranteed to fit into an int64_t
    using id_t = uint32_t;

private:
    using state_t = int64_t;

    static constexpr state_t UNLOCKED = -1;
    static constexpr state_t CONTESTED = -2;

    const uint32_t MAX_SPINNING_ACQUIRE_ITERATIONS{1000};

    //if we do not use the futex anymore (but a semaphore instead) this can be relaxed
    std::atomic<state_t> state{UNLOCKED};
    std::atomic<int64_t> lockingId{UNLOCKED};

    Semaphore semaphore;

    state_t compareExchangeState(state_t expected, state_t desired)
    {
        state.compare_exchange_strong(expected, desired, std::memory_order_acq_rel, std::memory_order_acquire);
        return expected; //always returns the old value (which is loaded by compare_exchange in the failure case)
    }

    state_t exchangeState(state_t desired)
    {
        return state.exchange(desired, std::memory_order_acq_rel);
    }

public:
    IdAwareLock(uint32_t maxSpinIterations = 1)
        : MAX_SPINNING_ACQUIRE_ITERATIONS(maxSpinIterations > 0 ? maxSpinIterations : 1)
    {
    }

    IdAwareLock(const IdAwareLock &) = delete;
    IdAwareLock(IdAwareLock &&) = delete;

    //only positive values (>0), can foolproof this later
    void lock(id_t id = 0)
    {
        //try to acquire the lock by spinning
        for (uint32_t i = 0; i < MAX_SPINNING_ACQUIRE_ITERATIONS; ++i)
        {
            auto knownState = compareExchangeState(UNLOCKED, id);
            if (knownState == UNLOCKED || knownState == id) // for recursive locking
            {
                lockingId.store(id, std::memory_order_relaxed);
                return;
            }
            else if (knownState == CONTESTED)
            {
                if (getLockingId() == id) // for recursive locking
                {
                    return;
                }
                //contested, do not try to spin any more and sleep instead
                //(promotes fairness with respect to threads trying to acquire the lock)
                semaphore.wait();
                break;
            }
        }

        //spinning failed, assume the lock is contested and change its state accordingly,
        //sleep while it is actually contested or locked

        //change to contested, waiting for it to be unlocked
        while (exchangeState(CONTESTED) != UNLOCKED)
        {
            //note that the contested state can be a false positive, i.e. might not be contested anymore when
            //we set it to contested, but then we do not sleep here since we already issued a post,
            //i.e. this is just a pessimistic but safe assumption which optimzes the logic

            //note that we also do not sleep when someone sets it back to UNLOCKED before the exchange
            //and just set it to CONTESTED (false positive) and return, having acquired the lock
            semaphore.wait();
        }

        lockingId.store(id, std::memory_order_relaxed);
    }

    void unlock()
    {
        lockingId.store(UNLOCKED); //slightly out of sync, but has to be done before exchange

        //change the lock state back to unlocked and wake someone if it was contested
        if (exchangeState(UNLOCKED) == CONTESTED)
        {
            semaphore.post();
        }
    }

    void unlock(id_t id)
    {
        if (getLockingId() != id)
        {
            std::cout << "incorrect unlock id" << std::endl;
            std::terminate(); //protocol error
        }

        lockingId.store(UNLOCKED); //slightly out of sync, but has to be done before exchange

        //change the lock state back to unlocked and wake someone if it was contested
        if (exchangeState(UNLOCKED) == CONTESTED)
        {
            semaphore.post();
        }
    }

    //return -1 for unlocked
    int64_t getLockingId()
    {
        return lockingId.load(std::memory_order_relaxed);
    }
};