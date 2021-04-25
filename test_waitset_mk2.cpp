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

using namespace std;
ws::WaitSet<16> w;

void notify(std::chrono::seconds time)
{
    std::this_thread::sleep_for(time);
    w.notify();
    cout << "notify 1" << std::endl;
    //w.notify(); //may be ignored due to autoreset event (waitset will not have woken up yet in general)
    std::this_thread::sleep_for(time);
    w.notify();
    cout << "notify 2" << std::endl;
}

int main(int argc, char **argv)
{
    ws::Trigger t;

    w.attach(t);

    std::thread thread(&notify, std::chrono::seconds(2));

    int wakeups = 0;
    do
    {
        cout << "waiting" << endl;
        auto wakeupReasons = w.wait();
        wakeups++;
        cout << "woke up " << wakeups << " due to " << wakeupReasons[0]->index << std::endl;
    } while (wakeups < 2);

    //ws.notify(1); //private, as intended

    // t.trigger();

    // w.detach(t);

    thread.join();
    return 0;
}