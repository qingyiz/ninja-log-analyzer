#pragma once

#include "core/NinjaLogTypes.h"

#include <QString>

namespace ninja_analyzer {

class NinjaLogParser final {
public:
    static ParseResult parse(const QString &logPath);
};

} // namespace ninja_analyzer
