#pragma once

#include "../models/DockerContainerModel.h"
#include "../services/DockerProvider.h"

#include <QWidget>

class QLabel;
class QTableView;

class DockerPage : public QWidget {
    Q_OBJECT

public:
    explicit DockerPage(QWidget *parent = nullptr);

public slots:
    void refresh();

private:
    DockerContainerRow selectedRow() const;
    void runAction(const QString &action);

    DockerContainerModel *m_model = nullptr;
    QTableView *m_table = nullptr;
    QLabel *m_status = nullptr;
    DockerProvider m_provider;
};
