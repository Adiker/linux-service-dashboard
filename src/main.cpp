#include "AppIcon.h"
#include "BuildInfo.h"
#include "MainWindow.h"

#include <QApplication>
#include <QCoreApplication>
#include <QLibraryInfo>
#include <QLocale>
#include <QSettings>
#include <QTranslator>
#include <cstdio>
#include <cstring>

int main(int argc, char* argv[]) {
    // Version queries must work without a display, providers, or settings writes.
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--version") == 0) {
            std::printf("Linux Service Dashboard %s (%s)\n", LSD_VERSION, LSD_REVISION);
            return 0;
        }
    }
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("LocalTools");
    QCoreApplication::setApplicationName("Linux Service Dashboard");
    QCoreApplication::setApplicationVersion(QStringLiteral(LSD_VERSION " (" LSD_REVISION ")"));
    QGuiApplication::setDesktopFileName(QStringLiteral("io.github.Adiker.LinuxServiceDashboard"));
    QApplication::setStyle(QStringLiteral("Fusion"));
    QApplication::setWindowIcon(serviceDashboardIcon());

    QTranslator translator, qtTranslator;
    const QString preference = QSettings().value("ui/language", "System").toString();
    const bool polish = preference == "pl" || (preference == "System" && QLocale::system().language() == QLocale::Polish);
    if (polish) {
        if (translator.load(":/i18n/dashboard_pl.qm"))
            app.installTranslator(&translator);
        if (qtTranslator.load("qtbase_pl", QLibraryInfo::path(QLibraryInfo::TranslationsPath)))
            app.installTranslator(&qtTranslator);
    }
    MainWindow window;
    window.show();

    return app.exec();
}
