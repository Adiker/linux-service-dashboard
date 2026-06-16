#pragma once

#include "../models/MountModel.h"
#include "../services/MountProvider.h"

#include <QWidget>

class QLabel;
class QTableView;

class MountsPage : public QWidget {
    Q_OBJECT

public:
    explicit MountsPage(QWidget *parent = nullptr);

public slots:
    void refresh();

private:
    MountRow selectedRow() const;

    MountModel *m_model = nullptr;
    QTableView *m_table = nullptr;
    QLabel *m_status = nullptr;
    MountProvider m_provider;
};
