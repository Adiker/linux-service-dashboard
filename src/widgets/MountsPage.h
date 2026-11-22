#pragma once

#include "../models/MountModel.h"
#include "../services/MountProvider.h"

#include <QWidget>

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTableView;
class StatusBanner;
class TableFilterProxyModel;

class MountsPage : public QWidget {
    Q_OBJECT

  public:
    explicit MountsPage(MountProvider* provider, QWidget* parent = nullptr);

  public slots:
    void refresh();

  private:
    MountRow selectedRow() const;
    void updateActionStates();
    void showContextMenu(const QPoint& pos);

    MountModel* m_model = nullptr;
    TableFilterProxyModel* m_proxy = nullptr;
    QTableView* m_table = nullptr;
    QLineEdit* m_filter = nullptr;
    QComboBox* m_stateFilter = nullptr;
    QPushButton* m_openButton = nullptr;
    QPushButton* m_mountButton = nullptr;
    QPushButton* m_unmountButton = nullptr;
    QPushButton* m_saveProfileButton = nullptr;
    QPushButton* m_removeProfileButton = nullptr;
    StatusBanner* m_status = nullptr;
    MountProvider* m_provider = nullptr;
};
