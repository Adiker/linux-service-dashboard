#pragma once

#include "../models/DiskModel.h"
#include "../services/SmartProvider.h"

#include <QWidget>

class QLabel;
class QTableView;

class SmartRefreshScheduler;

class DisksPage : public QWidget {
    Q_OBJECT

  public:
    explicit DisksPage(QWidget* parent = nullptr);

  public slots:
    void refresh();
    void reloadSmartSchedule();

  private:
    DiskRow selectedRow() const;
    void runScheduledSmartChecks();
    // Returns the serial to key SMART history by, or an empty string (path key)
    // when the serial is shared by several visible disks so their histories are
    // not merged.
    QString historyKeySerial(const QString& serial) const;

    DiskModel* m_model = nullptr;
    QTableView* m_table = nullptr;
    QLabel* m_status = nullptr;
    SmartProvider m_provider;
    SmartRefreshScheduler* m_smartScheduler = nullptr;
};
