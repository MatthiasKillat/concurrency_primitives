#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>

#include "waitset/waitset.hpp"

//just dummies, without queues and very trivial data sharing but the basic principle should apply
class Subscriber
{
public:
    //could also register the token in a delayed fashion
    Subscriber(int id) : m_id(id)
    {
    }

    std::optional<WaitToken> registerWaitSet(WaitSet &waitSet)
    {
        m_token = waitSet.add([&]() { return this->hasData(); },
                              [&]() { std::cout << "subscriber id " << m_id << " received " << m_data << std::endl; });
        return m_token;
    }

    void deliver(int data)
    {
        m_hasData = true;
        m_data = data;
        if (m_token.has_value())
            (*m_token).notify();
    }

    bool hasData()
    {
        return m_hasData;
    }

private:
    int m_id;
    bool m_hasData{false};
    int m_data;
    std::optional<WaitToken> m_token;
};

class Publisher
{
public:
    Publisher()
    {
    }

    void registerSubscriber(Subscriber &sub)
    {
        m_subscribers.push_back(&sub);
    }

    void publish(int data)
    {
        for (auto subscriber : m_subscribers)
        {
            subscriber->deliver(data);
        }
    }

private:
    std::vector<Subscriber *> m_subscribers;
};

std::atomic<bool> run{true};
WaitSet waitSet(10);
Publisher p1;
Publisher p2;
Subscriber s1(1);
Subscriber s2(2);
Subscriber s3(3);

auto guard = waitSet.add([&]() { return run == false; }, []() { std::cout << "\nguard callback\n"; }).value();

void publish()
{
    int i = 0;
    while (run)
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        p1.publish(++i);
        if (i % 2 == 0)
        {
            p2.publish(2 * i); //publishes slower
        }
    }
    guard.notify(); //free the potentially waiting waiter thread
}

void waitForData()
{
    while (run)
    {
        auto ids = waitSet.wait();
        //auto ids = waitSet.wait(myFilter);

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
    s1.registerWaitSet(waitSet);
    s2.registerWaitSet(waitSet);
    s3.registerWaitSet(waitSet);

    p1.registerSubscriber(s1);
    p1.registerSubscriber(s2);
    p2.registerSubscriber(s3);

    std::thread waiter(waitForData);
    std::thread publisher(publish);

    std::this_thread::sleep_for(std::chrono::seconds(11));
    run = false;

    waiter.join();
    publisher.join();
    return 0;
}