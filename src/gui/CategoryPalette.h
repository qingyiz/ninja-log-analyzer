#pragma once

#include "core/NinjaLogTypes.h"

#include <QColor>

inline QColor categoryColor(ninja_analyzer::StepCategory category)
{
    using ninja_analyzer::StepCategory;
    switch (category) {
    case StepCategory::CCompile:
        return QColor(QStringLiteral("#4C78A8"));
    case StepCategory::CxxCompile:
        return QColor(QStringLiteral("#0C9B8B"));
    case StepCategory::CudaCompile:
        return QColor(QStringLiteral("#59A14F"));
    case StepCategory::QtAutogen:
        return QColor(QStringLiteral("#9C6ADE"));
    case StepCategory::Resource:
        return QColor(QStringLiteral("#B279A2"));
    case StepCategory::StaticLink:
        return QColor(QStringLiteral("#F28E2B"));
    case StepCategory::SharedLink:
        return QColor(QStringLiteral("#E15759"));
    case StepCategory::ExecutableLink:
        return QColor(QStringLiteral("#D65F8B"));
    case StepCategory::CustomCommand:
        return QColor(QStringLiteral("#EDC948"));
    case StepCategory::Other:
        return QColor(QStringLiteral("#7B8A9F"));
    }
    return QColor(QStringLiteral("#7B8A9F"));
}
