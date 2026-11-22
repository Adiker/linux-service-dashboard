#pragma once

#include "../core/ModuleSettings.h"

#include <QHash>
#include <QString>
#include <QWidget>

class QGridLayout;
class OverviewCard;
class ServiceRegistry;

class OverviewPage : public QWidget {
    Q_OBJECT

  public:
    enum TargetPage {
        TargetSystemd = 1,
        TargetDocker = 2,
        TargetVpn = 3,
        TargetMounts = 4,
        TargetSensors = 5,
        TargetDisks = 6,
    };

    explicit OverviewPage(ServiceRegistry* registry, QWidget* parent = nullptr);

  public slots:
    void setModuleVisibility(const ModuleSettings& modules);
    void refresh(const ModuleSettings& modules = loadModuleSettings());

  signals:
    void refreshRequested();
    void navigateRequested(int pageIndex);

  private:
    OverviewCard* addCard(QGridLayout* grid, int row, int column, const QString& title, const QIcon& icon, int targetPage);
    void updateDisksCard();

    ServiceRegistry* m_registry = nullptr;
    OverviewCard* m_dockerCard = nullptr;
    OverviewCard* m_systemdCard = nullptr;
    OverviewCard* m_vpnCard = nullptr;
    OverviewCard* m_mountsCard = nullptr;
    OverviewCard* m_sensorsCard = nullptr;
    OverviewCard* m_disksCard = nullptr;

    int m_diskCount = 0;
    QString m_disksError;
    QHash<QString, QString> m_diskSmartHealth;
    QString m_lastSmartCheck;
};
