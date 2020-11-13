#include <iostream>
#include <thread>
#include <chrono>

#include "waitset/waitset.hpp"

bool myCond()
{
    return true;
}

int main(int argc, char **argv)
{
    int a = 2;
    int b = 3;
    WaitSet waitSet(10);

    auto token1 = waitSet.add(myCond).value();
    auto token2 = waitSet.add([&]() { return a == b; }, []() { std::cout << "condition2 callback"; }).value();

    std::cout << "token1 " << token1.id() << " condition evaluates to " << token1() << std::endl;
    std::cout << "token2 " << token2.id() << " condition evaluates to " << token2() << std::endl;

    b = 2;
    std::cout << "token2 " << token2.id() << " condition evaluates to " << token2() << std::endl;
    return 0;
}