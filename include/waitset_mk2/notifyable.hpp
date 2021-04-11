#pragma once

#include "types.hpp"

namespace ws
{

class Notifyable
{
public:
    virtual void notify() = 0;
    virtual void notify(index_t) = 0;
};

} // namespace ws