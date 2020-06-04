#pragma once
#include <string.h>
#include <signal.h>
#include <time.h>
#include <iostream>
#include <chrono>
#include <functional>
#include <atomic>

//needs to be improved and must be implemented according do capabilities of the underlying OS
// (here POSIX compliant linux)
class Timer
{
public:
    //the deleteSelfAfterTrigger flag could be an enum for clarity

    Timer(std::function<void(void)> callback, bool deleteSelfAfterTrigger = false) : callback(callback), deleteSelfAfterTrigger(deleteSelfAfterTrigger)
    {
        memset(&event, 0, sizeof(sigevent));
        memset(&triggerTime, 0, sizeof(itimerspec));
        event.sigev_notify = SIGEV_THREAD;
        event.sigev_notify_function = &Timer::timerHandler;
        event.sigev_value.sival_ptr = this;

        //todo: can fail ... need to deal with this later
        //via factory for example (avoid exceptions)
        timer_create(CLOCK_MONOTONIC, &event, &id);
    }

    ~Timer()
    {
        timer_delete(id); //if the call fails, can or do we need to do anything?
    }

    Timer(const Timer &) = delete;
    Timer(Timer &&) = delete;

    //other units are converted into nanoseconds implicitly
    bool arm(std::chrono::nanoseconds time)
    {
        toTimeSpec(time);
        timer_settime(id, 0, &triggerTime, nullptr); //can this fail?
        armed = true;
        timeOut = false;

        return true;
    }

    bool disarm()
    {
        //triggerTime.it_value.tv_nsec = 0;
        //triggerTime.it_value.tv_sec = 0;
        //timer_settime(id, 0, &triggerTime, nullptr); //can this fail?

        //we would need a feedback that the timer is disarmed and the callback is not running
        //as of now, the timerHandler will be called in any case and check whether the timer is armed
        //if so, it will call the trigger and afterwards check if the timer is supposed to be deleted

        armed = false;
        timeOut = false;

        return true;
    }

    bool isArmed()
    {
        return armed;
    }

    bool timedOut()
    {
        return timeOut;
    }

    void trigger()
    {
        timeOut = true;
        callback();
    }

private:
    timer_t id;
    sigevent event;
    itimerspec triggerTime;

    std::function<void(void)> callback;

    std::atomic<bool> armed{true};

    bool timeOut{false}; //todo: atomic?

    bool deleteSelfAfterTrigger{false};

    static void timerHandler(sigval sv)
    {
        Timer *timer = static_cast<Timer *>(sv.sival_ptr);

        //we do not want to trigger if the timer is not armed anymore
        if (timer->isArmed())
        {
            timer->trigger();
        }

        //this is a bit unfortunate, we will have the timerhandler called in any case
        //the problem is, that otherwise we cannot know whether it was called and somehow need to
        //delete the timer itself (but must ensure that it is not about to be called on a non-existing object)

        //this of course assumes it was created using dynamic memory, we will have a kind of pool later
        //but this problem remains, the deletion will just be done differently (not by calling delete)
        //probably with a deleter function we can provide
        if (timer->deleteSelfAfterTrigger)
        {
            std::cout << "deleting timer " << timer << std::endl;
            delete timer; //we do not need the object anymore afterwards
        }
    }

    void toTimeSpec(std::chrono::nanoseconds &ns)
    {
        constexpr uint64_t NANO = 1000000000;
        triggerTime.it_value.tv_nsec = ns.count() % NANO;
        triggerTime.it_value.tv_sec = ns.count() / NANO;
    }
};
