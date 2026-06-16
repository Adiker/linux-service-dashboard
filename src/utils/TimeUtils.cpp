#include "TimeUtils.h"

#include <QDateTime>

QString currentTimestamp()
{
    return QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
}
