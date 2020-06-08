#pragma once

#include "semaphore.hpp"
#include "lock.hpp"
#include "time.h"
#include "timer.hpp"

#include <string.h>
#include <signal.h>
#include <time.h>
#include <iostream>

#include <mutex>      //only for lock_guard which can easily be implemented on its own
#include <functional> //for the predicate

//general idea of timed_wait

//assume we hold the lock

//if the condition is satisfied, return immediately (and we still hold the lock)
//
//acquire waitListLock
//create a waitnode which also contains a timer, the timer also knows the node
//insert node into waitlist
//release waitListLock
//
//release lock
//
//wait on the semaphore of the node
//
//when we wake up we acquire the lock
//check the condition

//only a working title to distinguish different Condtion Variables
//note that we could also create a TimeOutSemaphore in a similar way (building on a regular Semaphore)
class TimeoutConditionVariable
{
private:
    struct WaitNode
    {
        WaitNode *prev{nullptr};
        WaitNode *next{nullptr};
        Semaphore semaphore;
        Timer *timer{nullptr};

        bool createTimer(TimeoutConditionVariable *condVar, std::chrono::nanoseconds &waitTime)
        {
            auto callback = [=] { condVar->notify(this); };

            //the timer should fire and delete itself afterwards (seems hard to do it
            //externally, because we cannot be sure whether the callback was already initiated
            //and the callback requires access to the Timer object

            timer = new Timer(callback, true);
            if (timer)
            {
                timer->arm(waitTime);
                return true;
            }

            return false;
        }

        bool timedOut()
        {
            if (timer)
            {
                return timer->timedOut();
            }
            return false;
        }

        ~WaitNode()
        {
        }
    };

    WaitNode *waitList{nullptr};
    Lock waitListLock;

    void insertWaitNode(WaitNode *node)
    {
        std::lock_guard<Lock> guard(waitListLock);

        //todo: for fairness should probably be inserted at the end instead
        //could abstract waitlist for this (but then we have problems with fine grained locks ...)

        node->next = waitList;
        if (waitList)
        {
            waitList->prev = node;
        }
        waitList = node;
    }

    void removeFirstWaitNode()
    {
        waitList = waitList->next;
        if (waitList)
        {
            waitList->prev = nullptr;
        }
    }

    //needed if it is to be removed by timer, can be in the middle of the list
    void removeWaitNode(WaitNode *node)
    {
        if (node->prev == nullptr)
        {
            removeFirstWaitNode();
            return;
        }

        node->prev->next = node->next;

        if (node->next)
        {
            node->next->prev = node->prev;
        }
    }

    bool nodeExistsInList(WaitNode *node)
    {
        auto listNode = waitList;
        while (listNode)
        {
            //could have been recycled from heap manager ... do we have this problem?
            if (listNode == node)
            {
                return true;
            }
            listNode = listNode->next;
        }
        return false;
    }

    void notify(WaitNode *node)
    {
        std::lock_guard<Lock> guard(waitListLock);

        //we know the node must still be there, because otherwise the timer would have been disarmed
        //but it can have been removed due to normal wakeup and we might miss this ...
        //we need to check whether the node still exists ... inefficient (can it be avoided?)

        if (nodeExistsInList(node))
        {
            //we hold the lock so node will still be there
            node->semaphore.post();
            removeWaitNode(node);
        }
    }

public:
    TimeoutConditionVariable() = default;

    ~TimeoutConditionVariable()
    {
        //todo: can be debated, without this, the nodes will never wake up if it was no timedWait that
        //created them (we will also have "memory leaks" on those waitnodes, but one could argue they are still needed
        //so it is not a leak per se)

        notifyAll();
    }

    TimeoutConditionVariable(const TimeoutConditionVariable &) = delete;
    TimeoutConditionVariable(TimeoutConditionVariable &&) = delete;

    //the other variants without timer are straightforward if this one works
    template <typename LockType>
    bool wait(LockType &lock, std::function<bool(void)> predicate, std::chrono::nanoseconds waitTime)
    {
        if (predicate())
        {
            return true; // we still hold the lock (if we held it upon entering as required, but this is not enforcable)
        }

        auto node = new WaitNode;

        if (!node || !node->createTimer(this, waitTime))
        {
            //we could not create a timer and cannot fullfill the contract without it (could block indefinitely)
            //we therefore return (todo: can be enhanced with some expected, to signal the error)
            //if the predicate changes only under lock, it has to be false (we still have the lock and checked it before)
            return false;
        }

        bool predicateResult;
        do
        {
            insertWaitNode(node);

            lock.unlock();
            node->semaphore.wait();

            // important to lock before checking the predicate
            // if this predicate can only change during lock (contract) we are sure that it holds after the wait call returns

            lock.lock();
            predicateResult = predicate();

            //when there is a timeout, we acuire the lock, evaluate the predicate and then return
            //it could be argued that we could release the lock when the predicate is false
            if (node->timedOut())
            {
                break;
            }

        } while (!predicateResult); //if there is a spurious wake up we release the lock and wait again

        //the node was already removed from the list by the notify or the timeout call, so it can be safely deleted
        //even if the other still fires, it will find no node in the list and thus not work on deleted data
        //TODO: but, in principle, it seems it could work on a newly created node on the heap with the same address,
        //which would not be correct (kind of an ABA problem with pointers...)
        //could give nodes ascending ids

        std::cout << "deleting node " << node << std::endl;
        delete node;

        //do we want to hold the lock even if predicate was false (and we timed out?)
        //note that when we timedOut and the predicate was true we still return true...

        return predicateResult; //false: timeout AND predicate is false, true otherwise
    }

    void notifyOne()
    {
        if (waitList)
        {
            waitList->semaphore.post();
            std::lock_guard<Lock> guard(waitListLock);

            if (waitList->timer)
            {
                waitList->timer->disarm();
            }

            removeFirstWaitNode();
        }
    }

    void notifyAll()
    {
        if (waitList)
        {
            std::lock_guard<Lock> guard(waitListLock);
            while (waitList)
            {
                if (waitList->timer)
                {
                    waitList->timer->disarm();
                }

                waitList->semaphore.post();
                removeFirstWaitNode();
            }
        }
    }
};