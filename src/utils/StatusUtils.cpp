#include "StatusUtils.h"

QString statusBadgeStyle(const QString &status)
{
    const QString normalized = status.toLower();
    if (normalized.contains(QStringLiteral("ok")) || normalized.contains(QStringLiteral("active")) || normalized.contains(QStringLiteral("running"))) {
        return QStringLiteral("color: #8fd694; font-weight: 700;");
    }
    if (normalized.contains(QStringLiteral("warn")) || normalized.contains(QStringLiteral("inactive")) || normalized.contains(QStringLiteral("exited"))) {
        return QStringLiteral("color: #f0c36d; font-weight: 700;");
    }
    if (normalized.contains(QStringLiteral("error")) || normalized.contains(QStringLiteral("critical")) || normalized.contains(QStringLiteral("failed"))) {
        return QStringLiteral("color: #ff8a80; font-weight: 700;");
    }
    return QStringLiteral("color: #aeb7c2; font-weight: 700;");
}

QString temperatureStatus(double value, double high, double critical)
{
    if (critical > 0.0 && value >= critical) {
        return QStringLiteral("Critical");
    }
    if (high > 0.0 && value >= high) {
        return QStringLiteral("Warning");
    }
    if (value > 0.0) {
        return QStringLiteral("OK");
    }
    return QStringLiteral("Unknown");
}
