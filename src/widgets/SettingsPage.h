#pragma once

#include <QWidget>

class QCheckBox;
class QComboBox;
class QPlainTextEdit;
class QSpinBox;

class SettingsPage : public QWidget {
    Q_OBJECT

public:
    explicit SettingsPage(QWidget *parent = nullptr);

signals:
    void settingsChanged();

private:
    void load();
    void save();

    QSpinBox *m_refreshInterval = nullptr;
    QPlainTextEdit *m_services = nullptr;
    QCheckBox *m_systemd = nullptr;
    QCheckBox *m_docker = nullptr;
    QCheckBox *m_vpn = nullptr;
    QCheckBox *m_mounts = nullptr;
    QCheckBox *m_sensors = nullptr;
    QCheckBox *m_smart = nullptr;
    QCheckBox *m_smartScheduled = nullptr;
    QSpinBox *m_smartIntervalMinutes = nullptr;
    QComboBox *m_theme = nullptr;
};
