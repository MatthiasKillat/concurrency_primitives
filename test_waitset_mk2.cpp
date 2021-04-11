#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>

#include "waitset_mk2/trigger.hpp"
#include "waitset_mk2/waitset.hpp"

int main(int argc, char **argv)
{
    ws::Trigger t;
    ws::WaitSet<16> ws;

    ws.attach(t);

    ws.wait();

    t.trigger();

    ws.detach(t);
    return 0;
}