#include "ModuleSettings.h"

#include <QSettings>

ModuleSettings loadModuleSettings()
{
    QSettings settings;
    ModuleSettings modules;
    modules.systemd = settings.value(QStringLiteral("modules/systemd"), true).toBool();
    modules.docker = settings.value(QStringLiteral("modules/docker"), true).toBool();
    modules.vpn = settings.value(QStringLiteral("modules/vpn"), true).toBool();
    modules.mounts = settings.value(QStringLiteral("modules/mounts"), true).toBool();
    modules.sensors = settings.value(QStringLiteral("modules/sensors"), true).toBool();
    modules.smart = settings.value(QStringLiteral("modules/smart"), true).toBool();
    return modules;
}
