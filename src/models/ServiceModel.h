#pragma once

#include <QAbstractTableModel>
#include <QVector>

struct ServiceRow {
    QString unit;
    QString loadState;
    QString activeState;
    QString subState;
    QString description;
    QString lastRefresh;
};

class ServiceModel : public QAbstractTableModel {
    Q_OBJECT

public:
    explicit ServiceModel(QObject *parent = nullptr);
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    void setRows(const QVector<ServiceRow> &rows);
    ServiceRow rowAt(int row) const;

private:
    QVector<ServiceRow> m_rows;
};
