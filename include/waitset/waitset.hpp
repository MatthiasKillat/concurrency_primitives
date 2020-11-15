#pragma once

#include "semaphore.hpp"
#include "container.hpp"

#include <vector>
#include <functional>
#include <optional>
#include <mutex>

using id_t = uint32_t;

//storing more specific conditions can be done without std::function (and thus without dynamic memory)
using Callback = std::function<void(void)>;
using Condition = std::function<bool(void)>;

using WakeUpSet = std::vector<id_t>;
//in general a filter cannot just depend on single a id_t but the whole wake-up set
using Filter = std::function<WakeUpSet(WakeUpSet &)>;

class WaitSet;

//internal to waitset, waitset must outlive the node (nodes will be owned and destroyed by waitset)
class WaitNode
{
public:
    WaitNode(WaitSet *waitSet, const Condition &condition) : m_waitSet(waitSet), m_condition(condition)
    {
    }

    WaitNode(WaitSet *waitSet, const Condition &condition, const Callback &callback) : m_waitSet(waitSet), m_condition(condition), m_callback(callback)
    {
    }

    bool evalMonotonic()
    {
        if (m_result)
        {
            return true; //was true and not reset yet
        }

        if (m_condition())
        {
            m_result = true; //monotonic, can be set to true but not to false (can be set to false by waitset)
            return true;
        }
        return false;
    }

    bool eval()
    {
        return m_condition();
    }

    bool getResult() const
    {
        return m_result;
    }

    void exec()
    {
        if (m_callback)
            m_callback();
    }

    void setCallback(const Callback &callback)
    {
        this->m_callback = callback;
    }

    void notify();

    void reset()
    {
        m_result = false;
    }

    id_t id() const
    {
        return m_id;
    }

    void setId(id_t id)
    {
        m_id = id;
    }

private:
    id_t m_id;
    WaitSet *m_waitSet;
    Condition m_condition;
    bool m_result{false}; //todo: may need to use an atomic
    Callback m_callback;
};

//proxy for client of waitset
//only created by WaitSet, but can be copied by anyone
//waitset must outlive the token (after the waitset is gone, the token should not be used)
//todo: can we make this reasonably safe?
class WaitToken
{
public:
    friend class WaitSet;

    //note: this makes it harder to implement something that tells the token that the waitset or node are gone
    //because any copies must be informed as well ... (probably not worth it and not necessary if used correctly)
    WaitToken(const WaitToken &) = default;
    WaitToken &operator=(const WaitToken &) = default;

    id_t id() const
    {
        return m_waitNode->id();
    }

    bool operator()()
    {
        return m_waitNode->eval();
    }

    void setCallback(const Callback &callback)
    {
        m_waitNode->setCallback(callback);
    }

    void notify()
    {
        m_waitNode->notify();
    }

private:
    WaitToken(WaitNode &node) : m_waitNode(&node)
    {
    }

    WaitNode *m_waitNode{nullptr};
};

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

        return WaitToken(node);
    }

    //todo: could later invalidate the returned token
    //(but there can still be copies, which is useful for copying objects containing tokens)
    bool remove(const WaitToken &token)
    {
        std::lock_guard g(m_nodesMutex);
        return m_nodes.remove(token.id());
    }

    //todo: we could also add a timed wait but in theory this can be done with a condition that is set to true by a timer

    //we can only have one waiter for proper operation (concurrent condition result reset would cause problems!)
    WakeUpSet wait()
    {
        m_semaphore.wait();

        // find the nodes whose conditions were true
        // (we have to iterate, we have no other information when we just use a single semaphore)
        // alternatively notify could prepare a wakeup set ... but we need to eliminate duplicates and so on,
        // losing any advantage for reasonably small numbers of conditions

        WakeUpSet wakeUpSet;

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

        return wakeUpSet;
    }

    //Note: filtering the active conditions is a little specific but may be useful
    //could also register the filter to the waitset
    WakeUpSet wait(Filter filter)
    {
        m_semaphore.wait();

        // find the nodes whose conditions were true
        // (we have to iterate, we have no other information when we just use a single semaphore)
        // alternatively notify could prepare a wakeup set ... but we need to eliminate duplicates and so on,
        // losing any advantage for reasonably small numbers of conditions

        WakeUpSet wakeUpSet;

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

        return wakeUpSet;
    }

    void notify()
    {
        //we do not need the container mutex here
        m_semaphore.post();
    }

private:
    uint64_t m_capacity;
    Semaphore m_semaphore; //must be interprocess if used across process boundaries
    Container<WaitNode> m_nodes;

    //protect m_nodes against concurrent modification
    //we can only block the application calling wait, add and remove,
    //but not the one calling notify
    //to avoid this mutex, we would either state the contract that the set can only be modified
    //in one thread while no one is notifying or waiting

    //major todo: invalidate token on removal of node, otherwise we get a problem at notify accessing invalid data

    //we do not want to add this to the container itself, we need a scoped lock for iteration
    std::mutex m_nodesMutex;
};

void WaitNode::notify()
{
    if (evalMonotonic()) //is or was true and not yet reset
    {
        //notify only if the condition is true
        m_waitSet->notify();
    }
}