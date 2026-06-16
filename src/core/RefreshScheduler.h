#pragma once

#include <QObject>
#include <QTimer>

class RefreshScheduler : public QObject {
    Q_OBJECT

public:
    explicit RefreshScheduler(QObject *parent = nullptr);
    void setIntervalSeconds(int seconds);
    int intervalSeconds() const;
    void start();
    void stop();

signals:
    void refreshRequested();

private:
    QTimer m_timer;
    int m_intervalSeconds = 30;
};
