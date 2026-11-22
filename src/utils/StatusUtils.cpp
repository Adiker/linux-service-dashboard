#include "StatusUtils.h"

QString temperatureStatus(double value, double high, double critical) {
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
