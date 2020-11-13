#pragma once

#include "semaphore.hpp"

#include <vector>
#include <functional>
#include <optional>

using Callback = std::function<void(void)>;
using Condition = std::function<bool(void)>;

class WaitSet;

//internal to waitset, waitset must outlive the node (nodes will be owned and destroyed by waitset)
class WaitNode
{
public:
    bool eval() const
    {
        return condition();
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

    WaitNode(WaitSet *waitset, const Condition &condition) : condition(condition)
    {
    }

    WaitNode(WaitSet *waitSet, const Condition &condition, const Callback &callback) : waitSet(waitSet), condition(condition), callback(callback)
    {
    }

    void notify();

private:
    WaitSet *waitSet;
    Condition condition;
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

    uint64_t id()
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
    WaitToken(WaitNode &node, uint64_t id) : m_waitNode(&node), m_id(id)
    {
    }

    WaitNode *m_waitNode{nullptr};
    uint64_t m_id;
};

class WaitSet
{
public:
    WaitSet(uint64_t capacity) : m_capacity(capacity)
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

    void wait()
    {
        m_semaphore.wait();
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
    waitSet->notify();
}