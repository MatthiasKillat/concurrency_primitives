#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>

#include "waitset_mk2/trigger.hpp"
#include "waitset_mk2/waitset.hpp"

class Subscriber
{
public:
    ws::Trigger trigger;
};

//later not via inheritance, with template args etc.
class SubscriberAwareWaitSet : public ws::WaitSet<16>
{
    bool attachSubscriber(Subscriber &subscriber)
    {
        return this->attach(subscriber.trigger);
    }
};

int main(int argc, char **argv)
{
    ws::Trigger t;
    ws::WaitSet<16> ws;

    ws.attach(t);

    ws.wait();

    ws.notify();
    //ws.notify(1); //private, as intended

    t.trigger();

    ws.detach(t);
    return 0;
}