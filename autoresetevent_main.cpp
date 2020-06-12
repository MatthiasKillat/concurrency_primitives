#include <iostream>
#include <thread>
#include <chrono>

#include "autoreset_event.hpp"

AutoResetEvent event;

void wait(int id)
{
    std::cout << "thread " << id << " wait" << std::endl;
    event.wait();
    std::cout << "thread " << id << " woke up" << std::endl;
}

void signal()
{
    std::cout << "signal #1" << std::endl; //signal comes before wait is called, will let wait pass through
    event.signal();
    std::cout << "signal #2" << std::endl; //redundant, ignored when no one is waiting
    event.signal();

    std::this_thread::sleep_for(std::chrono::seconds(4));
    std::cout << "signal #3" << std::endl; //wake a second thread
    event.signal();

    std::this_thread::sleep_for(std::chrono::seconds(2));
    std::cout << "signal #4" << std::endl; //wake a third thread,
    event.signal();                        //note that the redundant signal does not count, unlike for a semaphore
}

int main(int argc, char **argv)
{
    std::thread t(signal);

    std::this_thread::sleep_for(std::chrono::seconds(2)); //should be enough to let t signal before the others are running

    std::thread t1(wait, 1);
    std::thread t2(wait, 2);
    std::thread t3(wait, 3);

    t.join();
    t1.join();
    t2.join();
    t3.join();

    return 0;
}