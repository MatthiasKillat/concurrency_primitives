#include <iostream>
#include <thread>
#include <chrono>

#include "semaphore.hpp"
//#include "condition_variable.hpp"
#include "timeout_condition_variable.hpp"
using ConditionVariable = TimeoutConditionVariable;

Lock lock;
ConditionVariable cv;

bool doSomething = false;

bool isSomethingToDo()
{
    return doSomething;
}

void workAfterWakeUp()
{
    if (isSomethingToDo())
    {
        std::cout << "do something" << std::endl;
        doSomething = false; //only one thread will do something ...
    }
    else
    {
        std::cout << "do nothing" << std::endl; //the other will do nothing
    }
}

void wait()
{
    std::cout << "wait" << std::endl;
    lock.lock(); //not necessary to hold the lock in this implementation

    auto time = std::chrono::milliseconds(2000);
    bool status = cv.wait(lock, isSomethingToDo, time);

    //status is true if the predicate was true at the time of wake up

    //we hold the lock after wake up and the condition is definitly true (since we changed it before notification)

    std::cout << "woke up" << std::endl;

    workAfterWakeUp();

    lock.unlock();

    std::cout << "work done" << std::endl;
}

void notify()
{
    std::this_thread::sleep_for(std::chrono::seconds(1));
    std::cout << "notify" << std::endl;

    lock.lock();
    doSomething = true; //condition should only change under lock
    lock.unlock();

    cv.notifyOne();
    std::cout << "notify done" << std::endl;
}

int main(int argc, char **argv)
{
    std::thread t1(wait);
    std::thread t2(wait);
    std::thread t3(notify);

    t1.join();
    t2.join();
    t3.join();

    return 0;
}
