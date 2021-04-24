#pragma once

#include <cstdint>
#include <vector>
#include <atomic>

#include "trigger.hpp"
#include "notifyable.hpp"
#include "semaphore.hpp"
#include "container.hpp"
#include "autoreset.hpp"

namespace ws
{

struct NotificationInfo
{
    index_t id;
};

struct TriggerInfo
{
    NotificationInfo notificationInfo;
    id_t id;
    uint64_t numNotified{0};
    //std::atomic<uint64_t> notified{0};
};

//we want to use some Signaller "concept"
//cannot be stored in shared memory like this, can be redesigned to use a non-virtual interface if needed
template <uint32_t MaxTriggers = 128, typename Signaller = AutoResetEvent<Semaphore>>
class WaitSet
    : public Notifyable
{
public:
    WaitSet() : triggerInfoContainer(MaxTriggers)
    {
    }

    ~WaitSet()
    {
    }

    void notify() override
    {
        notify(RESERVED_INDEX);
    }

    //TODO: make attach/detach thread safe
    bool attach(Trigger &trigger)
    {
        //todo: check whether it is attached

        index_t index = getFreeIndex();
        if (index == INVALID_INDEX)
        {
            return false;
        }

        trigger.id = generateTriggerId();
        return true;
    }

    void detach(Trigger &trigger)
    {
        index_t index = trigger.index;
        if (index == INVALID_INDEX)
        {
            return;
        }

        //if(trigger.id matches id in notification array of the same index)

        trigger.index = INVALID_INDEX;

        //remove from container if it exists

        trigger.index == INVALID_INDEX;
    }

    std::vector<NotificationInfo *> wait()
    {
        std::vector<NotificationInfo *> result = collectNotifications();

        if (!result.empty())
        {
            return result;
        }

        signaller.wait();

        result = collectNotifications();

        return result;
    }

    //TODO timed wait

private:
    friend class Trigger;
    Signaller signaller;

    //reserve index 0 as internal wake up trigger

    //TODO: if using a lock-free index pool, could be made lock-free
    IndexedContainer<TriggerInfo> triggerInfoContainer; //TODO: size template arg for container

    static std::atomic<id_t> s_triggerId;

    static id_t generateTriggerId()
    {
        id_t id;
        do
        {
            id = s_triggerId.fetch_add(1, std::memory_order_relaxed);
        } while (id == INVALID_ID);
        return id;
    }

    std::vector<NotificationInfo *> collectNotifications()
    {
        //TODO: iterate over registered triggers, subtract current known trigger count (soft reset)
        std::vector<NotificationInfo *> notifications;
        return notifications;
    }

    void notify(index_t index) override
    {
        TriggerInfo &info = triggerInfoContainer[index];
        info.numNotified++;
        signaller.signal();
    }

    static index_t getFreeIndex()
    {
        return INVALID_INDEX;
    }
};

//monotonic, to be reasonably sure we idenfified the correct trigger (modulo wraparound)
template <uint32_t M, typename S>
std::atomic<id_t> WaitSet<M, S>::s_triggerId{1};

} // namespace ws
