#pragma once

#include "semaphore.hpp"

#include <vector>
#include <functional>
#include <optional>

using Callback = std::function<void(void)>;
using Condition = std::function<bool(void)>;
using id_t = uint32_t;

class WaitSet;

//internal to waitset, waitset must outlive the node (nodes will be owned and destroyed by waitset)
class WaitNode
{
public:
    WaitNode(WaitSet *waitSet, const Condition &condition) : m_waitSet(waitSet), condition(condition)
    {
    }

    WaitNode(WaitSet *waitSet, const Condition &condition, const Callback &callback) : m_waitSet(waitSet), condition(condition), callback(callback)
    {
    }

    bool evalMonotonic()
    {
        if (result)
        {
            return true; //was true and not reset yet
        }

        if (condition())
        {
            result = true; //monotonic, can be set to true but not to false (can be set to false by waitset)
            return true;
        }
        return false;
    }

    bool eval()
    {
        return condition();
    }

    bool getResult() const
    {
        return result;
    }

    void exec()
    {
        if (callback)
            callback();
    }

    void setCallback(const Callback &callback)
    {
        this->callback = callback;
    }

    void notify();

    void reset()
    {
        result = false;
    }

private:
    WaitSet *m_waitSet;
    Condition condition;
    bool result{false}; //todo: may need to use an atomic
    Callback callback;
};

//proxy for client of waitset
//only created by WaitSet, but can be copied by anyone
//waitset must outlive the token (after the waitset is gone, the token should not be used)
//todo: can we make this reasonably safe?
class WaitToken
{
public:
    friend class WaitSet;

    WaitToken(const WaitToken &) = default;
    WaitToken &operator=(const WaitToken &) = default;

    id_t id()
    {
        return m_id;
    }

    bool operator()()
    {
        return m_waitNode->eval();
    }

    void setCalback(const Callback &callback)
    {
        m_waitNode->setCallback(callback);
    }

    void notify()
    {
        m_waitNode->notify();
    }

private:
    WaitToken(WaitNode &node, id_t id) : m_waitNode(&node), m_id(id)
    {
    }

    WaitNode *m_waitNode{nullptr};
    id_t m_id; //do we need this? only to locate the corresponding node in the vector
};

class WaitSet
{
public:
    WaitSet(id_t capacity) : m_capacity(capacity)
    {
        m_waitNodes.reserve(capacity);
    }

    //todo: removal of nodes/conditions
    std::optional<WaitToken> add(const Condition &condition)
    {
        if (m_waitNodes.size() >= m_capacity)
        {
            return std::nullopt;
        }
        auto id = m_waitNodes.size();
        m_waitNodes.emplace_back(this, condition);

        auto &node = m_waitNodes.back();
        return WaitToken(node, id);
    }

    std::optional<WaitToken> add(const Condition &condition, const Callback &callback)
    {
        if (m_waitNodes.size() >= m_capacity)
        {
            return std::nullopt;
        }

        auto id = m_waitNodes.size();
        m_waitNodes.emplace_back(this, condition);

        auto &node = m_waitNodes.back();
        node.setCallback(callback);

        return WaitToken(node, id);
    }

    std::vector<id_t> wait()
    {
        m_semaphore.wait();

        // find the nodes whose conditions were true
        // (we have to iterate, we have no other information when we just use a single semaphore)
        // alternatively notify could prepare a wakeup set ... but we need to eliminate duplicates and so on,
        // losing any advantage for reasonably small numbers of conditions

        std::vector<id_t> trueNodeIds;

        auto n = m_waitNodes.size();
        for (size_t id = 0; id < n; ++id)
        {
            WaitNode &node = m_waitNodes[id];
            if (node.getResult())
            {
                trueNodeIds.push_back(id);
                node.reset(); //set condition back to false
                // someone may be setting them to true for a second time right now, but we have not fully woken up
                // so that is ok (we can see that the condition was true, but not how many times it changed)
                // if a new node becomes true there is another notify were it is set to true OR
                // we registered it in already in this wakeup
            }
        }

        return trueNodeIds;
    }

    void notify()
    {
        m_semaphore.post();
    }

private:
    Semaphore m_semaphore;
    std::vector<WaitNode> m_waitNodes;
    uint64_t m_capacity;
};

void WaitNode::notify()
{
    if (evalMonotonic()) //is or was true and not yet reset
    {
        //notify if the condition is true
        m_waitSet->notify();
    }
}