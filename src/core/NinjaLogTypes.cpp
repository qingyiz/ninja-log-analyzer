#include "core/NinjaLogTypes.h"

namespace ninja_analyzer {

QString categoryDisplayName(StepCategory category)
{
    switch (category) {
    case StepCategory::CCompile:
        return QStringLiteral("C 编译");
    case StepCategory::CxxCompile:
        return QStringLiteral("C++ 编译");
    case StepCategory::CudaCompile:
        return QStringLiteral("CUDA 编译");
    case StepCategory::QtAutogen:
        return QStringLiteral("Qt 自动生成");
    case StepCategory::Resource:
        return QStringLiteral("资源生成");
    case StepCategory::StaticLink:
        return QStringLiteral("静态库链接");
    case StepCategory::SharedLink:
        return QStringLiteral("共享库链接");
    case StepCategory::ExecutableLink:
        return QStringLiteral("可执行文件链接");
    case StepCategory::CustomCommand:
        return QStringLiteral("自定义命令");
    case StepCategory::Other:
        return QStringLiteral("其他");
    }
    return QStringLiteral("其他");
}

QString classificationSourceDisplayName(ClassificationSource source)
{
    return source == ClassificationSource::ManifestRule
        ? QStringLiteral("build.ninja rule")
        : QStringLiteral("输出路径推断");
}

} // namespace ninja_analyzer
