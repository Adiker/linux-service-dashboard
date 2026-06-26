#pragma once

#include "../core/ModuleSettings.h"
#include "../services/DockerProvider.h"
#include "../services/MountProvider.h"
#include "../services/NetworkProvider.h"
#include "../services/SensorProvider.h"
#include "../services/SmartProvider.h"
#include "../services/SystemdServiceProvider.h"

#include <QWidget>

class QLabel;
class QGridLayout;

class OverviewPage : public QWidget {
    Q_OBJECT

  public:
    explicit OverviewPage(QWidget* parent = nullptr);

  public slots:
    void refresh(const ModuleSettings& modules = loadModuleSettings());

  private:
    QLabel* addCard(QGridLayout* grid, int row, int column, const QString& title);
    void setCardVisible(QLabel* value, bool visible);
    QLabel* m_dockerValue = nullptr;
    QLabel* m_systemdValue = nullptr;
    QLabel* m_vpnValue = nullptr;
    QLabel* m_mountsValue = nullptr;
    QLabel* m_sensorsValue = nullptr;
    QLabel* m_disksValue = nullptr;
    DockerProvider m_docker;
    SystemdServiceProvider m_systemd;
    NetworkProvider m_network;
    MountProvider m_mounts;
    SensorProvider m_sensors;
    SmartProvider m_smart;
};
