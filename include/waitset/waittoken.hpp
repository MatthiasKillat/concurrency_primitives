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
        if (isValid())
        {
            m_waitNode->decrementRefCount();
        }
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
            m_waitNode->decrementRefCount();
            m_waitNode = nullptr;
            //todo: cannot call remove on WaitSet (as it is not fully defined and we cannot use virtual for an interface - design constraint)
            //hence we cannot trigger a cleanup in the waitset here
            //hence we have to give the token back to the waitset (which will then cleanup)
            //it will also clean up if itself gets destroyed, but if there are any token out there in this case
            //it is undefined behavior
        }
    }

private:
    WaitToken(WaitNode &node) : m_waitNode(&node)
    {
    }

    WaitNode *m_waitNode{nullptr};
};
