#pragma once

#include "../models/DiskModel.h"
#include "../services/SmartProvider.h"

#include <QWidget>

class QLabel;
class QTableView;

class DisksPage : public QWidget {
    Q_OBJECT

public:
    explicit DisksPage(QWidget *parent = nullptr);

public slots:
    void refresh();

private:
    DiskRow selectedRow() const;

    DiskModel *m_model = nullptr;
    QTableView *m_table = nullptr;
    QLabel *m_status = nullptr;
    SmartProvider m_provider;
};
