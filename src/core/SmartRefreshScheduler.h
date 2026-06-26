#pragma once

#include <QObject>
#include <QTimer>

class SmartRefreshScheduler : public QObject {
    Q_OBJECT

  public:
    explicit SmartRefreshScheduler(QObject* parent = nullptr);
    void reloadFromSettings();

  signals:
    void smartRefreshRequested();

  private:
    QTimer m_timer;
};
