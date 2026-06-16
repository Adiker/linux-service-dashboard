#pragma once

#include <QAbstractTableModel>
#include <QVector>

struct DiskRow {
    QString path;
    QString model;
    QString serial;
    QString size;
    QString transport;
    QString smartHealth = "Unknown";
    QString temperature = "-";
    QString reallocated = "-";
    QString pending = "-";
    QString lastCheck = "Never";
};

class DiskModel : public QAbstractTableModel {
    Q_OBJECT

public:
    explicit DiskModel(QObject *parent = nullptr);
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    void setRows(const QVector<DiskRow> &rows);
    void updateSmart(const QString &path, const DiskRow &smartRow);
    DiskRow rowAt(int row) const;
    QVector<DiskRow> rows() const;

private:
    QVector<DiskRow> m_rows;
};
