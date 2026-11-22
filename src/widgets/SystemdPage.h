#pragma once

#include "../models/ServiceModel.h"
#include "../services/SystemdServiceProvider.h"

#include <QHash>
#include <QPointer>
#include <QWidget>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTableView;
class LogViewerDialog;
class StatusBanner;
class TableFilterProxyModel;

class SystemdPage : public QWidget {
    Q_OBJECT

public:
    explicit SystemdPage(SystemdServiceProvider *provider, QWidget *parent = nullptr);

public slots:
    void refresh();
    void reloadGroupSelector();

private:
    ServiceRow selectedRow() const;
    QStringList watchedServices() const;
    void runAction(const QString &action);
    void updateActionStates();
    void showContextMenu(const QPoint &pos);

    ServiceModel *m_model = nullptr;
    TableFilterProxyModel *m_proxy = nullptr;
    QTableView *m_table = nullptr;
    QLineEdit *m_filter = nullptr;
    QComboBox *m_stateFilter = nullptr;
    QComboBox *m_groupSelector = nullptr;
    QCheckBox *m_allServicesCheck = nullptr;
    QPushButton *m_startButton = nullptr;
    QPushButton *m_stopButton = nullptr;
    QPushButton *m_restartButton = nullptr;
    QPushButton *m_logsButton = nullptr;
    StatusBanner *m_status = nullptr;
    QHash<QString, QPointer<LogViewerDialog>> m_logDialogs;
    SystemdServiceProvider *m_provider = nullptr;
};
