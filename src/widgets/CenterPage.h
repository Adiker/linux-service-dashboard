#pragma once
#include "../core/SystemCenter.h"

#include <QWidget>
#include <functional>
class QHBoxLayout;
class MetricChart;
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

  protected:
    void addActionButton(const QString& label, std::function<void(const CenterRecord&)> action);
    CenterRecord selected() const;

  private:
    QHBoxLayout* m_actions = nullptr;
    QHash<QString, QLabel*> m_cards;
    QHBoxLayout* m_extraActions = nullptr;
    QLabel* m_activity = nullptr;
    MetricChart* m_chart = nullptr;
    QPushButton* m_open = nullptr;
    QPushButton* m_logs = nullptr;
    void showReport();
    QString m_module;
    SystemCenter* m_center;
    QTreeWidget* m_table;
    QLineEdit* m_filter;
    QLabel* m_status;
    QLabel* m_details;
    CenterSnapshot m_data;
    void filter();
};
