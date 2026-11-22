#include "SmartRefreshScheduler.h"

#include <QSettings>

SmartRefreshScheduler::SmartRefreshScheduler(QObject* parent) : QObject(parent) {
    m_timer.setSingleShot(false);
    connect(&m_timer, &QTimer::timeout, this, &SmartRefreshScheduler::smartRefreshRequested);
}

void SmartRefreshScheduler::reloadFromSettings() {
    QSettings settings;
    const bool enabled = settings.value(QStringLiteral("smart/scheduledRefreshEnabled"), false).toBool();
    const int minutes = qMax(5, settings.value(QStringLiteral("smart/scheduledRefreshMinutes"), 30).toInt());
    if (enabled) {
        m_timer.start(minutes * 60 * 1000);
    } else {
        m_timer.stop();
    }
}
