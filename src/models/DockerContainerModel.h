#pragma once

#include <QAbstractTableModel>
#include <QVector>

struct DockerContainerRow {
    QString name;
    QString image;
    QString status;
    QString state;
    QString ports;
    QString created;
    QString id;
};

class DockerContainerModel : public QAbstractTableModel {
    Q_OBJECT

public:
    explicit DockerContainerModel(QObject *parent = nullptr);
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    void setRows(const QVector<DockerContainerRow> &rows);
    DockerContainerRow rowAt(int row) const;

private:
    QVector<DockerContainerRow> m_rows;
};
