#pragma once

#include "notifyable.hpp"

namespace ws
{

template <uint32_t MaxTriggers, typename SemaphoreType>
class WaitSet;

//will access the Waitset (might not exist), do we want to prevent this?
class Trigger
{
public:
    void trigger()
    {
        notifyable->notify(index);
    }

private:
    template <uint32_t MaxTriggers, typename SemaphoreType>
    friend class WaitSet;

    Notifyable *notifyable;
    index_t index;
    id_t id; //make this a monotoic counter, set by the waitset as a static
};

} // namespace ws
