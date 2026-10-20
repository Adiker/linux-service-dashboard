#include "CenterTypes.h"

#include <QCoreApplication>
#include <QSettings>

QVector<ModuleDescriptor> centerModules() {
    const auto t = [](const char* s) { return QCoreApplication::translate("Modules", s); };
    return {
        {"overview", t(QT_TRANSLATE_NOOP("Modules", "Overview")), t(QT_TRANSLATE_NOOP("Modules", "Dashboard")), ""},
        {"systemd", t(QT_TRANSLATE_NOOP("Modules", "Services")), t(QT_TRANSLATE_NOOP("Modules", "Services and tasks")), "systemctl"},
        {"timers", t(QT_TRANSLATE_NOOP("Modules", "Timers")), t(QT_TRANSLATE_NOOP("Modules", "Services and tasks")), "systemctl"},
        {"sockets", t(QT_TRANSLATE_NOOP("Modules", "Sockets")), t(QT_TRANSLATE_NOOP("Modules", "Services and tasks")), "systemctl"},
        {"autostart", t(QT_TRANSLATE_NOOP("Modules", "Autostart")), t(QT_TRANSLATE_NOOP("Modules", "Services and tasks")), "", true},
        {"docker", t(QT_TRANSLATE_NOOP("Modules", "Containers / Compose")), t(QT_TRANSLATE_NOOP("Modules", "Containers")), "docker"},
        {"images", t(QT_TRANSLATE_NOOP("Modules", "Images")), t(QT_TRANSLATE_NOOP("Modules", "Containers")), "docker", true},
        {"volumes", t(QT_TRANSLATE_NOOP("Modules", "Volumes")), t(QT_TRANSLATE_NOOP("Modules", "Containers")), "docker", true},
        {"network", t(QT_TRANSLATE_NOOP("Modules", "Interfaces and routes")), t(QT_TRANSLATE_NOOP("Modules", "Network and access")), "ip"},
        {"vpn", t(QT_TRANSLATE_NOOP("Modules", "VPN and tunnels")), t(QT_TRANSLATE_NOOP("Modules", "Network and access")), "nmcli"},
        {"firewall", t(QT_TRANSLATE_NOOP("Modules", "Firewall")), t(QT_TRANSLATE_NOOP("Modules", "Network and access")), "firewall-cmd"},
        {"ports", t(QT_TRANSLATE_NOOP("Modules", "Listening ports")), t(QT_TRANSLATE_NOOP("Modules", "Network and access")), "ss"},
        {"smart", t(QT_TRANSLATE_NOOP("Modules", "Disks / SMART")), t(QT_TRANSLATE_NOOP("Modules", "Data and disks")), "lsblk"},
        {"filesystems", t(QT_TRANSLATE_NOOP("Modules", "Partitions and capacity")), t(QT_TRANSLATE_NOOP("Modules", "Data and disks")),
         "lsblk"},
        {"mounts", t(QT_TRANSLATE_NOOP("Modules", "Mounts")), t(QT_TRANSLATE_NOOP("Modules", "Data and disks")), "findmnt"},
        {"backups", t(QT_TRANSLATE_NOOP("Modules", "Backups")), t(QT_TRANSLATE_NOOP("Modules", "Backups and synchronization")),
         "systemctl"},
        {"snapshots", t(QT_TRANSLATE_NOOP("Modules", "Kopia snapshots")), t(QT_TRANSLATE_NOOP("Modules", "Backups and synchronization")),
         "kopia", true},
        {"sync", t(QT_TRANSLATE_NOOP("Modules", "Synchronization and cloud drives")),
         t(QT_TRANSLATE_NOOP("Modules", "Backups and synchronization")), "systemctl"},
        {"metrics", t(QT_TRANSLATE_NOOP("Modules", "Performance")), t(QT_TRANSLATE_NOOP("Modules", "Hardware and session")), ""},
        {"sensors", t(QT_TRANSLATE_NOOP("Modules", "Sensors")), t(QT_TRANSLATE_NOOP("Modules", "Hardware and session")), "sensors"},
        {"audio", t(QT_TRANSLATE_NOOP("Modules", "Audio")), t(QT_TRANSLATE_NOOP("Modules", "Hardware and session")), "wpctl"},
        {"bluetooth", t(QT_TRANSLATE_NOOP("Modules", "Bluetooth")), t(QT_TRANSLATE_NOOP("Modules", "Hardware and session")),
         "bluetoothctl"},
        {"power", t(QT_TRANSLATE_NOOP("Modules", "Power")), t(QT_TRANSLATE_NOOP("Modules", "Hardware and session")), "upower"},
        {"printing", t(QT_TRANSLATE_NOOP("Modules", "Printing")), t(QT_TRANSLATE_NOOP("Modules", "Hardware and session")), "lpstat"},
        {"usb", t(QT_TRANSLATE_NOOP("Modules", "USB devices")), t(QT_TRANSLATE_NOOP("Modules", "Hardware and session")), "lsusb"},
        {"servers", t(QT_TRANSLATE_NOOP("Modules", "Local servers")), t(QT_TRANSLATE_NOOP("Modules", "Servers and sharing")), "systemctl"},
        {"apps", t(QT_TRANSLATE_NOOP("Modules", "Applications")), t(QT_TRANSLATE_NOOP("Modules", "Software and environments")), "gio",
         true},
        {"packages", t(QT_TRANSLATE_NOOP("Modules", "Packages / Flatpak")), t(QT_TRANSLATE_NOOP("Modules", "Software and environments")),
         "", true},
        {"vms", t(QT_TRANSLATE_NOOP("Modules", "Virtual machines")), t(QT_TRANSLATE_NOOP("Modules", "Software and environments")), "",
         true},
        {"tools", t(QT_TRANSLATE_NOOP("Modules", "Tools and environments")), t(QT_TRANSLATE_NOOP("Modules", "Software and environments")),
         "", true},
        {"events", t(QT_TRANSLATE_NOOP("Modules", "Events and diagnostics")), t(QT_TRANSLATE_NOOP("Modules", "Diagnostics and settings")),
         ""},
        {"settings", t(QT_TRANSLATE_NOOP("Modules", "Settings")), t(QT_TRANSLATE_NOOP("Modules", "Diagnostics and settings")), ""}};
}
QString centerTitle(const QString& id) {
    for (const auto& m : centerModules())
        if (m.id == id)
            return m.title;
    return id;
}
bool centerModuleEnabled(const QString& id) {
    return id == "overview" || id == "settings" || QSettings().value("modules/" + id, true).toBool();
}
