#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>

#include "waitset/waitset.hpp"

std::atomic<bool> run{true};

int a = 1;
int b = 0;

bool always_true()
{
    return true;
}

WaitSet waitSet(10);

//add a condition and an optional callback, receive a token as a proxy object to access the waitset
//the advantage is that the token is linked to the condition,
//it is also possible to notify the waitset directly

auto token1 = waitSet.add(always_true).value();
auto token2 = waitSet.add([&]() { return a == b; }, []() { std::cout << "condition2 callback"; }).value();
auto guard = waitSet.add([&]() { return run == false; }, []() { std::cout << "guard callback"; }).value();

//note: we assume only one waiter for now, but we can also add functionality to wake all or some number n

void notify()
{
    while (run)
    {
        b = (b + 1) % 2;
        std::this_thread::sleep_for(std::chrono::seconds(1));
        token1.notify();  //condition is always true
        token2.notify();  //true every second time
        waitSet.notify(); //notifies the waitset
    }
    guard.notify(); //free the potentially waiting waiter thread
}

void wait()
{
    while (run)
    {
        auto ids = waitSet.wait();
        std::cout << "woke up with ids: ";
        for (auto id : ids)
        {
            std::cout << id << " ";
        }
        std::cout << std::endl;
    }
}

int main(int argc, char **argv)
{
    std::thread waiter(wait);
    std::thread notifier(notify);

    std::this_thread::sleep_for(std::chrono::seconds(10));
    run = false;

    waiter.join();
    notifier.join();
    return 0;
}