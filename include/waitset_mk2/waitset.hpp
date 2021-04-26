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
    index_t index;
};

struct TriggerInfo
{
    TriggerInfo() = default;
    NotificationInfo notificationInfo;
    id_t id;
    uint64_t numNotified{0};
    //std::atomic<uint64_t> notified; //fix forwarding to use this
};

//we want to use some Signaller "concept"
//cannot be stored in shared memory like this, can be redesigned to use a non-virtual interface if needed
template <uint32_t MaxTriggers = 128, typename Signaller = AutoResetEvent<Semaphore>>
class WaitSet
    : public Notifyable
{
public:
    WaitSet() : m_triggerInfoContainer(MaxTriggers)
    {
        attach(m_internalTrigger); //TODO ensure it is the 0 trigger
    }

    ~WaitSet()
    {
    }

    void notify() override
    {
        //or just signal but then we cannot have the non-empty wakeup container
        notify(RESERVED_INDEX);
    }

    //TODO: make attach/detach thread safe (lock-free?)
    bool attach(Trigger &trigger)
    {
        //TODO check whether it is already attached
        auto result = m_triggerInfoContainer.emplace();
        if (!result.has_value())
        {
            return false;
        }

        index_t index = result.value();
        TriggerInfo &info = m_triggerInfoContainer[index];

        trigger.id = generateTriggerId();
        info.id = trigger.id;
        info.notificationInfo.index = index;

        m_triggerIndices.push_back(index); //TODO cannot remove currently, implement detach
        return true;
    }

    void detach(Trigger &trigger)
    {
        //TODO implement
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

        while (result.empty())
        {
            signaller.wait();
            result = collectNotifications();
        }
        //always non-empty
        return result;
    }

    //TODO timed wait

private:
    friend class Trigger;
    Signaller signaller;
    Trigger m_internalTrigger; //reserve index 0 for internal wake up trigger

    //TODO: if using a lock-free index pool, could be made lock-free
    IndexedContainer<TriggerInfo> m_triggerInfoContainer; //TODO: size template arg for container
    std::vector<index_t> m_triggerIndices;

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
        std::vector<NotificationInfo *> notifications;
        for (auto index : m_triggerIndices)
        {
            //TODO better container abstraction for use case
            auto &info = m_triggerInfoContainer[index];
            //TODO fetch_sub when atomics are used (fix forwarding)
            if (info.numNotified > 0)
            {
                info.numNotified--;
                notifications.push_back(&info.notificationInfo);
            }
        }
        return notifications;
    }

    void notify(index_t index) override
    {
        TriggerInfo &info = m_triggerInfoContainer[index];

        //TODO check for id match
        info.numNotified++;
        signaller.signal();
    }

}; // namespace ws

//monotonic, to be reasonably sure we idenfified the correct trigger (modulo wraparound)
template <uint32_t M, typename S>
std::atomic<id_t> WaitSet<M, S>::s_triggerId{1};

} // namespace ws
