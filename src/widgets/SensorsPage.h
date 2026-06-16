#pragma once

#include "../models/SensorModel.h"
#include "../services/SensorProvider.h"

#include <QWidget>

class QLabel;
class QTableView;

class SensorsPage : public QWidget {
    Q_OBJECT

public:
    explicit SensorsPage(QWidget *parent = nullptr);

public slots:
    void refresh();

private:
    SensorModel *m_model = nullptr;
    QTableView *m_table = nullptr;
    QLabel *m_status = nullptr;
    SensorProvider m_provider;
};
