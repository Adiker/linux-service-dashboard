#include "MainWindow.h"

#include <QApplication>
#include <QCoreApplication>
#include <QIcon>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("LocalTools");
    QCoreApplication::setApplicationName("Linux Service Dashboard");
    QCoreApplication::setApplicationVersion("0.1.0");
    QGuiApplication::setDesktopFileName(QStringLiteral("io.github.Adiker.LinuxServiceDashboard"));
    QApplication::setStyle(QStringLiteral("Fusion"));
    QApplication::setWindowIcon(QIcon(QStringLiteral(":/icons/linux-service-dashboard.svg")));

    MainWindow window;
    window.show();

    return app.exec();
}
