#pragma once

#include "autoreset_event.hpp"
#include "container.hpp"

#include <vector>
#include <functional>
#include <optional>
#include <mutex>

#include "waitset_types.hpp"
#include "waittoken.hpp"
#include "waitnode.hpp"

class WaitSet
{
public:
    WaitSet(id_t capacity) : m_capacity(capacity), m_nodes(capacity)
    {
    }

    std::optional<WaitToken> add(const Condition &condition)
    {
        std::lock_guard g(m_nodesMutex);
        auto maybeId = m_nodes.emplace(this, condition);

        if (!maybeId.has_value())
        {
            return std::nullopt;
        }
        auto id = *maybeId;
        auto &node = m_nodes[id];
        node.setId(id);

        //todo: create factory function for nodes and token
        //we create a WaitToken and hence increment the refCount
        node.incrementRefCount();

        //todo: we need to be very careful here that this function call does not rely on anything it deletes...
        node.setDeleter([=](id_t id) { this->remove(id); });

        return WaitToken(node);
    }

    std::optional<WaitToken> add(const Condition &condition, const Callback &callback)
    {
        std::lock_guard g(m_nodesMutex);
        auto maybeId = m_nodes.emplace(this, condition, callback);

        if (!maybeId.has_value())
        {
            return std::nullopt;
        }
        auto id = *maybeId;
        auto &node = m_nodes[id];
        node.setId(id);

        //we create a WaitToken and hence increment the refCount
        node.incrementRefCount();

        //todo: we need to be very careful here that this function call does not rely on anything it deletes...
        node.setDeleter([=](id_t id) { this->remove(id); });

        return WaitToken(node);
    }

    //todo: we could also add a timed wait but in theory this can be done with a condition that is set to true by a timer

    //we can only have one waiter for proper operation (concurrent condition result reset would cause problems!)
    WakeUpSet wait()
    {
        WakeUpSet wakeUpSet;

        do
        {
            m_autoResetEvent.wait();

            // find the nodes whose conditions were true
            // (we have to iterate, we have no other information when we just use a single semaphore)
            // alternatively notify could prepare a wakeup set ... but we need to eliminate duplicates and so on,
            // losing any advantage for reasonably small numbers of conditions

            std::lock_guard g(m_nodesMutex);
            auto n = m_nodes.size();
            for (size_t id = 0; id < n; ++id)
            {
                WaitNode &node = m_nodes[id];
                if (node.getResult())
                {
                    node.exec();
                    wakeUpSet.push_back(id);
                    node.reset(); //set condition back to false
                    // someone may be setting them to true for a second time right now, but we have not fully woken up
                    // so that is ok (we can see that the condition was true, but not how many times it changed)
                    // if a new node becomes true there is another notify were it is set to true OR
                    // we registered it in already in this wakeup
                }
            }
        } while (wakeUpSet.empty()); //do not wake up when no conditions are true

        return wakeUpSet;
    }

    //Note: filtering the active conditions is a little specific but may be useful
    //could also register the filter to the waitset
    WakeUpSet wait(Filter filter)
    {
        WakeUpSet wakeUpSet;

        do
        {
            m_autoResetEvent.wait();
            {
                std::lock_guard g(m_nodesMutex);

                auto n = m_nodes.size();
                for (size_t id = 0; id < n; ++id)
                {
                    WaitNode &node = m_nodes[id];
                    if (node.getResult())
                    {
                        wakeUpSet.push_back(id);
                        node.reset(); //set condition back to false
                        // someone may be setting them to true for a second time right now, but we have not fully woken up
                        // so that is ok (we can see that the condition was true, but not how many times it changed)
                        // if a new node becomes true there is another notify were it is set to true OR
                        // we registered it in already in this wakeup
                    }
                }
            }

            wakeUpSet = filter(wakeUpSet);

            for (size_t id : wakeUpSet)
            {
                m_nodes[id].exec();
            }
        } while (wakeUpSet.empty());

        return wakeUpSet;
    }

    void notify()
    {
        //we do not need the container mutex here
        m_autoResetEvent.signal();
    }

private:
    uint64_t m_capacity;

    //autoreset event to limit the number of unecessary wake ups
    AutoResetEvent m_autoResetEvent; //must use interprocess internally if used across process boundaries
    Container<WaitNode> m_nodes;

    //protect m_nodes against concurrent modification
    //we can only block the application calling wait, add and remove,
    //but not the one calling notify
    //to avoid this mutex, we would either state the contract that the set can only be modified
    //in one thread while no one is notifying or waiting

    //major todo: invalidate token on removal of node, otherwise we get a problem at notify accessing invalid data

    //we do not want to add this to the container itself, we need a scoped lock for iteration
    std::mutex m_nodesMutex;

    bool remove(id_t id)
    {
        std::lock_guard g(m_nodesMutex);
        auto &node = m_nodes[id];

        //only remove it if no token references it anymore
        if (node.numReferences() <= 0)
        {
            return m_nodes.remove(id);
        }
        return false;
    }
};

//can only be defined when WaitSet is fully defined
void WaitNode::notify()
{
    if (evalMonotonic()) //is or was true and not yet reset
    {
        //notify only if the condition is true
        m_waitSet->notify();
    }
}