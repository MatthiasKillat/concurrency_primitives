#pragma once

#include "waitset_types.hpp"
#include "waitnode.hpp"

//proxy for client of waitset
//only created by WaitSet, but can be copied by anyone
//waitset must outlive the token (after the waitset is gone, the token should not be used)
//todo: can we make this reasonably safe?
class WaitToken
{
public:
    friend class WaitSet;

    WaitToken(const WaitToken &other) : m_waitNode(other.m_waitNode)
    {
        if (isValid())
        {
            m_waitNode->incrementRefCount();
        }
    }

    WaitToken &operator=(const WaitToken &rhs)
    {
        if (&rhs != this)
        {
            if (isValid())
            {
                m_waitNode->decrementRefCount();
            }

            m_waitNode = rhs.m_waitNode;
            if (isValid())
            {
                m_waitNode->incrementRefCount();
            }
        }
        return *this;
    }

    WaitToken(WaitToken &&other) : m_waitNode(other.m_waitNode)
    {
        other.m_waitNode = nullptr;
    }

    WaitToken &operator=(WaitToken &&rhs)
    {
        if (&rhs != this)
        {
            m_waitNode = rhs.m_waitNode;
            rhs.m_waitNode = nullptr;
        }
        return *this;
    }

    ~WaitToken()
    {
        invalidate();
    }

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

    bool isValid()
    {
        return m_waitNode != nullptr;
    }

    void invalidate()
    {
        if (isValid())
        {
            if (m_waitNode->decrementRefCount())
            {
                m_waitNode->tryDelete();
            }
            m_waitNode = nullptr;
        }
    }

private:
    WaitToken(WaitNode &node) : m_waitNode(&node)
    {
    }

    WaitNode *m_waitNode{nullptr};
};
