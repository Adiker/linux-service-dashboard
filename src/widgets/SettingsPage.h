#pragma once

#include <QHash>
#include <QString>
#include <QStringList>
#include <QWidget>

class QCheckBox;
class QComboBox;
class QPlainTextEdit;
class QSpinBox;

class SettingsPage : public QWidget {
    Q_OBJECT

  public:
    explicit SettingsPage(QWidget* parent = nullptr);

  signals:
    void settingsChanged();

  private:
    void load();
    void save();
    void reloadGroups();
    void loadGroupServices();
    void addGroup();
    void renameGroup();
    QStringList editorServices() const;
    void stageCurrentEditor();

    QSpinBox* m_refreshInterval = nullptr;
    QComboBox* m_serviceGroup = nullptr;
    QString m_loadedGroup;
    QHash<QString, QStringList> m_stagedGroupEdits;
    QPlainTextEdit* m_services = nullptr;
    QCheckBox* m_systemd = nullptr;
    QCheckBox* m_docker = nullptr;
    QCheckBox* m_vpn = nullptr;
    QCheckBox* m_mounts = nullptr;
    QCheckBox* m_sensors = nullptr;
    QCheckBox* m_smart = nullptr;
    QComboBox* m_theme = nullptr;
};
