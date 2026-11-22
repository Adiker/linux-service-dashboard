#pragma once

#include "../models/DockerContainerModel.h"
#include "../services/DockerProvider.h"

#include <QHash>
#include <QPointer>
#include <QWidget>

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTableView;
class LogViewerDialog;
class StatusBanner;
class TableFilterProxyModel;

class DockerPage : public QWidget {
    Q_OBJECT

public:
    explicit DockerPage(DockerProvider *provider, QWidget *parent = nullptr);

public slots:
    void refresh();

private:
    DockerContainerRow selectedRow() const;
    void runAction(const QString &action);
    void updateActionStates();
    void showContextMenu(const QPoint &pos);

    DockerContainerModel *m_model = nullptr;
    TableFilterProxyModel *m_proxy = nullptr;
    QTableView *m_table = nullptr;
    QLineEdit *m_filter = nullptr;
    QComboBox *m_stateFilter = nullptr;
    QPushButton *m_startButton = nullptr;
    QPushButton *m_stopButton = nullptr;
    QPushButton *m_restartButton = nullptr;
    QPushButton *m_logsButton = nullptr;
    QPushButton *m_inspectButton = nullptr;
    StatusBanner *m_status = nullptr;
    QHash<QString, QPointer<LogViewerDialog>> m_textDialogs;
    DockerProvider *m_provider = nullptr;
};
