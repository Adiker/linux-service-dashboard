#pragma once

#include "../models/DiskModel.h"
#include "../services/SmartProvider.h"

#include <QWidget>

class QLabel;
class QLineEdit;
class QPushButton;
class QTableView;
class SmartRefreshScheduler;
class StatusBanner;
class TableFilterProxyModel;

class DisksPage : public QWidget {
    Q_OBJECT

  public:
    explicit DisksPage(SmartProvider* provider, QWidget* parent = nullptr);

  public slots:
    void refresh();
    void reloadSmartSchedule();

  private:
    DiskRow selectedRow() const;
    void runScheduledSmartChecks();
    void updateActionStates();
    void showContextMenu(const QPoint& pos);
    void openHistoryForSelected();
    // Returns the serial to key SMART history by, or an empty string (path key)
    // when the serial is shared by several visible disks so their histories are
    // not merged.
    QString historyKeySerial(const QString& serial) const;

    DiskModel* m_model = nullptr;
    TableFilterProxyModel* m_proxy = nullptr;
    QTableView* m_table = nullptr;
    QLineEdit* m_filter = nullptr;
    QPushButton* m_smartButton = nullptr;
    QPushButton* m_smartAllButton = nullptr;
    QPushButton* m_historyButton = nullptr;
    StatusBanner* m_status = nullptr;
    SmartProvider* m_provider = nullptr;
    SmartRefreshScheduler* m_smartScheduler = nullptr;
};
