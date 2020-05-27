#pragma once

#include "semaphore.hpp"
#include "lock.hpp"

#include <mutex>      //only for lock_guard which can easily be implemented on its own
#include <functional> //for the predicate, can be dropped if we pass the callable via template as e.g. std::thread does

class ConditionVariable
{

private:
    struct WaitNode
    {
        WaitNode *next{nullptr};
        Semaphore semaphore;
    };

    WaitNode *waitList{nullptr};

    Lock waitListLock;

public:
    ConditionVariable() = default;

    ConditionVariable(const ConditionVariable &) = delete;
    ConditionVariable(ConditionVariable &&) = delete;

    void wait()
    {
        auto node = new WaitNode;

        {
            std::lock_guard<Lock> guard(waitListLock);
            node->next = waitList;
            waitList = node;
        }

        //note: notification requires waitListLock, so it could happen here while we are not waiting for the semaphore yet
        //but this is no problem, we will see the incremented semaphore during semaphore.wait()

        //TODO: analysis how this is useful/safe without external lock

        node->semaphore.wait();
        delete node;
    }

    //Precondition: lock must be locked before the call (this can be dropped as far as I can see)
    //Postcondition: lock acquired

    //semantics:
    //during the wait call we relase the lock (if we hold it) and wait for a semaphore (possibly yield the thread, depends on semaphore)
    //once we are notified, we try to reqacquire the lock until we succeed and return

    //note: if a condition we are monitoring can only change while holding this lock,
    // and we check that it holds once we wake up, we can guarantee that it still holds while we are holding the lock
    // note: if the precondition is violated, the custom lock implementation should be able to deal with it
    // (even if lock is not locked it can be unlocked without error)
    template <typename LockType>
    void wait(LockType &lock)
    {

        //note that these could come from a pool instead of new, limiting the number of possible waiting threads
        //in a directly controllable way (technically we are also limited now by the OS and available memory)
        auto node = new WaitNode;
        {
            std::lock_guard<Lock> guard(waitListLock);
            node->next = waitList;
            waitList = node;
        }

        lock.unlock();

        node->semaphore.wait();
        delete node;

        //note that if the lock is not available we will proceed once it is, we were still woken up
        lock.lock();
    }

    //TODO: perfect forwarding with arbitrary predicate arguments (syntactic sugar)
    template <typename LockType>
    void wait(LockType &lock, std::function<bool(void)> predicate)
    {
        if (predicate())
        {
            return;
        }

        auto node = new WaitNode;

        do
        {
            //node must be reinserted when we wake up and sleep again if the condition is not true
            //node cannot be removed here, because then multiple wake ups of the same node could happen
            //it must be removed by the notify call under waitListLock
            //major todo: there is potential for a lost wake up, further analysis required

            {
                std::lock_guard<Lock> guard(waitListLock);
                node->next = waitList;
                waitList = node;
            }

            lock.unlock();
            node->semaphore.wait();

            // important to lock before checking the predicate
            // if this predicate can only change during lock (contract) we are sure that it holds after the wait call returns
            lock.lock();
        } while (!predicate()); //if there there is a spurious wake up we release the lock and wait again

        delete node;
    }

    void
    notifyOne()
    {
        std::lock_guard<Lock> guard(waitListLock);
        if (waitList)
        {
            waitList->semaphore.post();
            waitList = waitList->next;
        }
    }

    void notifyAll()
    {
        std::lock_guard<Lock> guard(waitListLock);
        while (waitList)
        {
            waitList->semaphore.post();
            waitList = waitList->next;
        }
    }
};