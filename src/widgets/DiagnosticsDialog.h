#pragma once

#include <QDialog>

class QComboBox;
class QTableWidget;
class ServiceRegistry;

class DiagnosticsDialog : public QDialog {
    Q_OBJECT

  public:
    explicit DiagnosticsDialog(ServiceRegistry* registry, QWidget* parent = nullptr);

  private:
    void reloadTable();
    void copyReport();

    ServiceRegistry* m_registry = nullptr;
    QComboBox* m_kindFilter = nullptr;
    QTableWidget* m_table = nullptr;
};
