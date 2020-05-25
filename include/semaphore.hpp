#pragma once

#include <linux/futex.h>
#include <sys/syscall.h>
#include <unistd.h>

#include <atomic>
#include <limits>

//actually it is a bounded semahore (i.e. with a maximum value), but the bound is not configurable yet (which is easy to do)
class Semaphore
{
public:
    Semaphore(size_t initialValue = 0) : value{initialValue}
    {
        futexWord = reinterpret_cast<int *>(&value);
    }

    ~Semaphore()
    {
    }

    Semaphore(const Semaphore &) = delete;
    Semaphore(Semaphore &&) = delete;

    bool tryWait()
    {
        auto oldValue = value.load(std::memory_order_relaxed);

        do
        {
            if (oldValue == 0)
            {
                return false; //value is 0, we do not block but fail instead
            }

            //value > 0, try to decrement the value (ensure that it cannot fall below 0)
        } while (!value.compare_exchange_strong(oldValue, oldValue - 1, std::memory_order_relaxed, std::memory_order_relaxed));

        return true;
    }

    void wait()
    {
        if (tryWait())
        {
            return;
        }

        //value was 0, so we try again but may block now

        waitCount.fetch_add(1, std::memory_order_acq_rel);

        //it is imperative that the loads and stores here cannot pass the fences of the waitCount changes
        do
        {
            sleepIfValueIsZero();
        } while (!tryWait());

        waitCount.fetch_sub(1, std::memory_order_acq_rel);
    }

    size_t post(size_t increment = 1)
    {
        //a fetch_add would suffice if we would not need to ensure that value is at most MAX_VALUE
        //value.fetch_add(increment, std::memory_order_relaxed);

        //we cannot increment values higher than that without overflow
        size_t overflowBound = MAX_VALUE - increment;

        auto oldValue = value.load(std::memory_order_relaxed);

        constexpr bool notIncremented = true;
        do
        {
            if (oldValue > overflowBound)
            {
                //we would overflow
                if (oldValue == MAX_VALUE)
                {
                    return 0; //cannot increment, value is already MAX_VALUE
                }

                //cannot increment by full amount but a lower one up to MAX_VALUE
                if (value.compare_exchange_strong(oldValue, MAX_VALUE, std::memory_order_relaxed, std::memory_order_relaxed))
                {
                    increment = MAX_VALUE - oldValue; //lower than the desired increment
                    break;
                }
            }
            else
            {
                //no overflow
                if (value.compare_exchange_strong(oldValue, oldValue + increment))
                {
                    break;
                }
            }
            //the increment logic can probably be optimized somewhat, but some kind of overflow check is crucial
            //if the compare exchange fails, we retry (which implicitly loads oldValue again)
        } while (notIncremented);

        //we finished the increment (or returned because value is already MAX_VALUE)

        //on first sight there appears to be a subtle race (lost wakeup) between this call and wait,
        //but this should pose no problem (there may be other problems though, it still requires some careful analysis)

        //potential lost wake up should NOT be possible:
        //i) if we execute the load before anybody is waiting (waitCount == 0), this is fine and no one needs to be woken up
        //ii) if we execute the load after anyone waiting is already sleeping, this is also fine, wake will wake up someone waiting
        //    (not necessarily the last one waiting though, up to the futex_wake implementation which will probably use FIFO order)
        //iii) if we execute the load after waitCount dropped back to 0 there is definitely nobody sleeping and we do not need to
        //     wake up anyonne
        //iv) critical case: waitCount was incremented but the wait call did not put the caller to sleep yet (via futex_wait)
        // subcase A) value > 0 : because someone else called post, then wait will not go to sleep and the futex_wake(increment)
        // call is wasted but causes no harm
        // subcase B) value = 0 : cannot happen without another interfering wait or tryWait, we incremented the value above before
        // exiting the while loop (furthermore, the value must have been 0 otherwise we would not consider sleeping and
        // use the fast return path from tryWait instead)

        //TODO: rethink analysis, there might still be some (small?) problem

        //is someone waiting?
        if (waitCount.load(std::memory_order_acquire) != 0)
        {
            //we are only responsible for waking how many we actually incremented, waking more is not necessary
            //as this will be done by other calls of post
            wake(increment);
        }
        //we return the value we actually incremented (might be lower due to overflow protection)
        return increment;
    }

private:
    std::atomic<size_t> value;
    std::atomic<size_t> waitCount{0};

    //we could easily make this max limit configurable later, e.g. as template parameter or member set during construction
    static constexpr size_t MAX_VALUE = std::numeric_limits<size_t>::max();

    int *futexWord;

    void sleepIfValueIsZero()
    {
        syscall(SYS_futex, futexWord, FUTEX_WAIT, 0, 0, 0, 0);
    }

    void wake(size_t numToWake)
    {
        syscall(SYS_futex, futexWord, numToWake, 0, 0, 0);
    }
};
