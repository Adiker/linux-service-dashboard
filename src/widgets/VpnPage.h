#pragma once

#include "../services/NetworkProvider.h"

#include <QWidget>

class QLabel;

class VpnPage : public QWidget {
    Q_OBJECT

public:
    explicit VpnPage(QWidget *parent = nullptr);

public slots:
    void refresh();

private:
    QLabel *m_connected = nullptr;
    QLabel *m_name = nullptr;
    QLabel *m_device = nullptr;
    QLabel *m_state = nullptr;
    QLabel *m_lastRefresh = nullptr;
    QLabel *m_error = nullptr;
    NetworkProvider m_provider;
};
