#include "gui/MainWindow.h"

#include <QApplication>
#include <QCoreApplication>
#include <QTimer>

int main(int argc, char *argv[])
{
    QApplication application(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("Ninja Log Analyzer"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.1.0"));
    QCoreApplication::setOrganizationName(QStringLiteral("Codex"));

    MainWindow window;
    window.resize(1280, 760);
    window.show();
    const QStringList arguments = application.arguments();
    if (arguments.size() > 1) {
        const QString startupPath = arguments.last();
        QTimer::singleShot(0, &window, [&window, startupPath] {
            window.analyzePath(startupPath);
        });
    }
    return application.exec();
}
