#pragma once
#include "../core/SystemCenter.h"

#include <QWidget>
class QTreeWidget;
class QLineEdit;
class QLabel;
class QPushButton;
class CenterPage : public QWidget {
    Q_OBJECT
  public:
    explicit CenterPage(const QString& module, SystemCenter* center, QWidget* parent = nullptr);
    void render();
    void setSearch(const QString& text);
    void selectRecord(const QString& id);
  signals:
    void navigate(const QString& module, const QString& id);

  private:
    QString m_module;
    SystemCenter* m_center;
    QTreeWidget* m_table;
    QLineEdit* m_filter;
    QLabel* m_status;
    QLabel* m_details;
    CenterSnapshot m_data;
    CenterRecord selected() const;
    void filter();
};
