#pragma once

#include <QAbstractTableModel>
#include <QVector>

struct MountRow {
    QString target;
    QString source;
    QString filesystemType;
    QString options;
    QString status;
};

class MountModel : public QAbstractTableModel {
    Q_OBJECT

public:
    explicit MountModel(QObject *parent = nullptr);
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    void setRows(const QVector<MountRow> &rows);
    MountRow rowAt(int row) const;

private:
    QVector<MountRow> m_rows;
};
