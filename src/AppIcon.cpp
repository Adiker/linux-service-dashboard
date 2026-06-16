#include "AppIcon.h"

#include <QSize>

QIcon serviceDashboardIcon()
{
    const QIcon themed = QIcon::fromTheme(QStringLiteral("io.github.Adiker.LinuxServiceDashboard"));
    if (!themed.isNull()) {
        return themed;
    }

    QIcon icon;
    icon.addFile(QStringLiteral(":/icons/linux-service-dashboard-16.png"), QSize(16, 16));
    icon.addFile(QStringLiteral(":/icons/linux-service-dashboard-22.png"), QSize(22, 22));
    icon.addFile(QStringLiteral(":/icons/linux-service-dashboard-24.png"), QSize(24, 24));
    icon.addFile(QStringLiteral(":/icons/linux-service-dashboard-32.png"), QSize(32, 32));
    icon.addFile(QStringLiteral(":/icons/linux-service-dashboard-48.png"), QSize(48, 48));
    icon.addFile(QStringLiteral(":/icons/linux-service-dashboard-64.png"), QSize(64, 64));
    icon.addFile(QStringLiteral(":/icons/linux-service-dashboard-128.png"), QSize(128, 128));
    icon.addFile(QStringLiteral(":/icons/linux-service-dashboard-256.png"), QSize(256, 256));
    if (!icon.isNull()) {
        return icon;
    }
    return icon;
}
