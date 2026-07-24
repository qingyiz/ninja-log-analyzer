#pragma once

#include "application/MachineLoadSnapshot.h"

namespace ninja_analyzer {

class MachineLoadProbe final {
public:
    static MachineLoadSnapshot capture();
};

} // namespace ninja_analyzer
