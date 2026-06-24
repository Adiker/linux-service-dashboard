#pragma once

struct ModuleSettings {
    bool systemd = true;
    bool docker = true;
    bool vpn = true;
    bool mounts = true;
    bool sensors = true;
    bool smart = true;
};

ModuleSettings loadModuleSettings();
