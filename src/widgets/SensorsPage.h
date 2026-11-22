#pragma once

#include "../models/SensorModel.h"
#include "../services/SensorProvider.h"

#include <QWidget>

class QComboBox;
class QLabel;
class QLineEdit;
class QTableView;
class StatusBanner;
class TableFilterProxyModel;

class SensorsPage : public QWidget {
    Q_OBJECT

  public:
    explicit SensorsPage(SensorProvider* provider, QWidget* parent = nullptr);

  public slots:
    void refresh();

  private:
    void showContextMenu(const QPoint& pos);

    SensorModel* m_model = nullptr;
    TableFilterProxyModel* m_proxy = nullptr;
    QTableView* m_table = nullptr;
    QLineEdit* m_filter = nullptr;
    QComboBox* m_stateFilter = nullptr;
    StatusBanner* m_status = nullptr;
    SensorProvider* m_provider = nullptr;
};
