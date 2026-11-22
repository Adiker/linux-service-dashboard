#pragma once
#include "../services/DockerProvider.h"
#include "../services/MountProvider.h"
#include "../services/NetworkProvider.h"
#include "../services/SensorProvider.h"
#include "../services/SmartProvider.h"
#include "../services/SystemdServiceProvider.h"
#include "CenterTypes.h"

#include <QHash>

class SystemCenter : public QObject {
    Q_OBJECT
  public:
    explicit SystemCenter(QObject* parent = nullptr);
    DockerProvider docker;
    MountProvider mounts;
    NetworkProvider network;
    SensorProvider sensors;
    SmartProvider smart;
    SystemdServiceProvider systemd;
    CenterSnapshot snapshot(const QString& id) const;
    void publish(const QString& id, const QStringList& headers, const QVector<CenterRecord>& rows, const QString& error = {});
    void refresh(const QString& id);
    void refreshScheduled();
    CenterSnapshot search(const QString& text) const;
    void toggleFavorite(const QString& module, const QString& id);
    bool isFavorite(const QString& module, const QString& id) const;
    void open(const CenterRecord& record);
    void logs(const CenterRecord& record);
    int attentionCount() const;
  signals:
    void changed(const QString& id);
    void requestRefresh(const QString& id);
    void textReady(const QString& title, const QString& text);

  private:
    QHash<QString, CenterSnapshot> m_snapshots;
};
