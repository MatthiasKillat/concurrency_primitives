#pragma once

#include <vector>
#include <functional>
#include <stdint.h>

using id_t = uint32_t;

//storing more specific conditions can be done without std::function (and thus without dynamic memory)
using Callback = std::function<void(void)>;
using Condition = std::function<bool(void)>;

using WakeUpSet = std::vector<id_t>;

//in general a filter cannot just depend on a single id_t but the whole wake-up set
using Filter = std::function<WakeUpSet(WakeUpSet &)>;

class WaitSet;
class WaitToken;
class WaitNode;