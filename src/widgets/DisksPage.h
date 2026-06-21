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
    explicit DisksPage(QWidget *parent = nullptr);

public slots:
    void refresh();
    void reloadSmartSchedule();

private:
    DiskRow selectedRow() const;
    void runScheduledSmartChecks();

    DiskModel *m_model = nullptr;
    QTableView *m_table = nullptr;
    QLabel *m_status = nullptr;
    SmartProvider m_provider;
    SmartRefreshScheduler *m_smartScheduler = nullptr;
};
