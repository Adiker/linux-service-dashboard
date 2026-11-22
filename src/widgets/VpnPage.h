#pragma once

#include "../services/NetworkProvider.h"

#include <QVector>
#include <QWidget>

class QLabel;
class QPushButton;
class QTableWidget;

class VpnPage : public QWidget {
    Q_OBJECT

public:
    explicit VpnPage(NetworkProvider *provider, QWidget *parent = nullptr);

public slots:
    void refresh();

private:
    void updateActionStates();
    VpnProfile selectedProfile() const;

    QLabel *m_connected = nullptr;
    QLabel *m_name = nullptr;
    QLabel *m_device = nullptr;
    QLabel *m_state = nullptr;
    QLabel *m_lastRefresh = nullptr;
    QLabel *m_error = nullptr;
    QTableWidget *m_profilesTable = nullptr;
    QPushButton *m_connectButton = nullptr;
    QPushButton *m_disconnectButton = nullptr;
    QVector<VpnProfile> m_profiles;
    NetworkProvider *m_provider = nullptr;
};
