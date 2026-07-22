#pragma once

#include "core/NinjaLogTypes.h"

#include <QString>

namespace ninja_analyzer {

class LogLocator final {
public:
    static LocateResult resolve(const QString &inputPath);
};

} // namespace ninja_analyzer
