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

bool always_false()
{
    return false;
}

//limited to 3 conditions for this test
WaitSet waitSet(3);

WakeUpSet myFilter(WakeUpSet &unfiltered)
{
    WakeUpSet filtered; //creating a new vector is more efficient than removal (for vector, set would be different)
    for (auto id : unfiltered)
    {
        if (id != 0)
        {
            filtered.push_back(id);
        }
    }
    return filtered;
}

//to avoid those capturing global variables warnings in the lambdas
bool condition2()
{
    return a == b;
}

void callback2()
{
    std::cout << "\ncondition2 callback a=" << a << " b=" << b << "\n";
}

bool guardCondition()
{
    return run == false;
}

//add a condition and an optional callback, receive a token as a proxy object to access the waitset
//the advantage is that the token is linked to the condition,
//it is also possible to notify the waitset directly

//capturing statics causes warnings here, but works (we could rewrite it it but is non-essential for demonstration)
auto token1 = waitSet.add(always_true).value();
auto token2 = waitSet.add(condition2, callback2).value();
auto token3 = token1; //we can copy tokens provided by the waitset (shallow copy)
auto guard = waitSet.add(guardCondition, []() { std::cout << "\nguard callback\n"; }).value();

//note: we assume only one waiter for now, but we can also add functionality to wake all or some number n

void notify()
{
    while (run)
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        b = (b + 1) % 2;
        token3.notify();  //condition is always true, notifies the waitset if the corresponding condition is true
        token2.notify();  //true every second time, notifies the waitset if the corresponding condition is true
        waitSet.notify(); //notifies the waitset
    }
    guard.notify(); //free the potentially waiting waiter thread
}

void wait()
{
    while (run)
    {
        //auto ids = waitSet.wait();
        auto ids = waitSet.wait(myFilter); //filter the ids before waking up

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
    auto maybeToken = waitSet.add(always_true);
    if (!maybeToken.has_value())
    {
        waitSet.remove(token3); //a copy of token1, if we do not remove it we cannot free the node (shared by token1 and token3)
        std::cout << "could not get another token" << std::endl;
        if (waitSet.remove(token1))
        {
            //we should have space again
            //maybeToken = waitSet.add(always_false);
            maybeToken = waitSet.add(always_true);
            if (maybeToken.has_value())
            {
                token1 = *maybeToken;
                token3 = token1;
                std::cout << "regenerated token1 and its copy token3" << std::endl;
            }
        }
    }

    std::thread waiter(wait);
    std::thread notifier(notify);

    std::this_thread::sleep_for(std::chrono::seconds(10));
    run = false;

    waiter.join();
    notifier.join();
    return 0;
}