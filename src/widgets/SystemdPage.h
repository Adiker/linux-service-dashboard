#pragma once

#include "../models/ServiceModel.h"
#include "../services/SystemdServiceProvider.h"

#include <QWidget>

class QComboBox;
class QLineEdit;
class QLabel;
class QSortFilterProxyModel;
class QTableView;

class SystemdPage : public QWidget {
    Q_OBJECT

public:
    explicit SystemdPage(QWidget *parent = nullptr);

public slots:
    void refresh();

private:
    ServiceRow selectedRow() const;
    QStringList watchedServices() const;
    void runAction(const QString &action);

    ServiceModel *m_model = nullptr;
    QSortFilterProxyModel *m_proxy = nullptr;
    QTableView *m_table = nullptr;
    QLineEdit *m_filter = nullptr;
    QComboBox *m_groupSelector = nullptr;
    QLabel *m_status = nullptr;
    SystemdServiceProvider m_provider;
};
