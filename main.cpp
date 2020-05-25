#include <iostream>
#include <thread>
#include <chrono>

#include "semaphore.hpp"
#include "condition_variable.hpp"

Lock lock;
ConditionVariable cv;

bool doSomething = false;

bool isSomethingToDo()
{
    return doSomething;
}

void wait()
{
    std::cout << "wait" << std::endl;
    //lock.lock(); //not necessary to hold the lock in this implementation
    cv.wait(lock, isSomethingToDo);

    //we hold the lock and the condition is definitely true (since we changed it before notification)

    std::cout << "woke up" << std::endl;
    lock.unlock();
}

void notify()
{
    std::this_thread::sleep_for(std::chrono::seconds(1));
    std::cout << "notify" << std::endl;

    lock.lock();
    doSomething = true; //condition should only change under lock
    lock.unlock();

    cv.notifyAll();
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
