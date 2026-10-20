#include "ToolLauncher.h"

#include "SystemInventory.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
QString ToolLauncher::desktop(const QString& file) {
    bool trusted = false;
    for (const auto& root : SystemInventory::dataRoots())
        if (QDir::cleanPath(file).startsWith(QDir::cleanPath(root) + "/applications/"))
            trusted = true;
    if (!trusted || !QFileInfo(file).isFile() || !file.endsWith(".desktop"))
        return QCoreApplication::translate("ToolLauncher", "Desktop entry is outside the application catalog");
    if (!QProcess::startDetached("gio", {"launch", file}))
        return QCoreApplication::translate("ToolLauncher", "Could not launch desktop entry; check gio availability");
    return {};
}
QString ToolLauncher::program(const QString& name) {
    const QStringList allowed{"timeshift-launcher",
                              "coolercontrol",
                              "lact",
                              "corectrl",
                              "qpwgraph",
                              "easyeffects",
                              "pavucontrol",
                              "firewall-config",
                              "nm-connection-editor",
                              "gsmartcontrol",
                              "gnome-disks",
                              "plasma-systemmonitor",
                              "missioncenter",
                              "resources",
                              "VirtualBox",
                              "qtcreator",
                              "code"};
    const auto path = QStandardPaths::findExecutable(name);
    if (!allowed.contains(name) || path.isEmpty() || !QProcess::startDetached(path, {}))
        return QCoreApplication::translate("ToolLauncher", "Tool unavailable or could not be opened");
    return {};
}
