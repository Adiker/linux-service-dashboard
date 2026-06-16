#include "RefreshScheduler.h"

RefreshScheduler::RefreshScheduler(QObject *parent)
    : QObject(parent)
{
    connect(&m_timer, &QTimer::timeout, this, &RefreshScheduler::refreshRequested);
}

void RefreshScheduler::setIntervalSeconds(int seconds)
{
    m_intervalSeconds = qMax(5, seconds);
    m_timer.setInterval(m_intervalSeconds * 1000);
}

int RefreshScheduler::intervalSeconds() const
{
    return m_intervalSeconds;
}

void RefreshScheduler::start()
{
    setIntervalSeconds(m_intervalSeconds);
    m_timer.start();
}

void RefreshScheduler::stop()
{
    m_timer.stop();
}
