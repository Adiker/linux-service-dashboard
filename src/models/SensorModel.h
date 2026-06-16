#pragma once

#include <QAbstractTableModel>
#include <QVector>

struct SensorRow {
    QString chip;
    QString label;
    double current = 0.0;
    double high = 0.0;
    double critical = 0.0;
    QString status;
};

class SensorModel : public QAbstractTableModel {
    Q_OBJECT

public:
    explicit SensorModel(QObject *parent = nullptr);
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    void setRows(const QVector<SensorRow> &rows);
    QVector<SensorRow> rows() const;

private:
    QVector<SensorRow> m_rows;
};
