#include "MainWindow.h"
#include "AppIcon.h"

#include <QApplication>
#include <QCoreApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("LocalTools");
    QCoreApplication::setApplicationName("Linux Service Dashboard");
    QCoreApplication::setApplicationVersion("0.1.0");
    QGuiApplication::setDesktopFileName(QStringLiteral("io.github.Adiker.LinuxServiceDashboard"));
    QApplication::setStyle(QStringLiteral("Fusion"));
    QApplication::setWindowIcon(serviceDashboardIcon());

    MainWindow window;
    window.show();

    return app.exec();
}
