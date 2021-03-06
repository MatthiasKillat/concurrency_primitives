#pragma once

#include "waitset_types.hpp"

//internal to waitset, waitset must outlive the node (nodes will be owned and destroyed by waitset)
class WaitNode
{
    friend class WaitToken;
    friend class WaitSet;

    using Deleter = std::function<void(id_t &)>;

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
        m_callback = callback;
    }

    //refactor this somewhat, e.g. construction time
    void setDeleter(const Deleter &deleter)
    {
        m_deleter = deleter;
    }

    void tryDelete()
    {
        if (m_deleter)
        {
            m_deleter(m_id);
        }
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

    uint64_t numReferences() const
    {
        return m_refCount;
    }

private:
    uint64_t m_refCount{0};
    id_t m_id;
    WaitSet *m_waitSet;
    Condition m_condition;
    bool m_result{false}; //todo: may need to use an atomic
    Callback m_callback;

    //needed since we want to call a delete method of WaitSet in here but WaitSet depends on WaitNode itself
    //(and want to avoid virtual interfaces)
    //function must not depend on the node itself, since it may trigger its deletion
    Deleter m_deleter;

    uint64_t incrementRefCount()
    {
        ++m_refCount;
        return m_refCount;
    }

    uint64_t decrementRefCount()
    {
        --m_refCount;
        return m_refCount;
    }
};