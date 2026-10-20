#include "EventStore.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUuid>
namespace {
QString label(const QString& code) {
    if (code == "refresh-failed")
        return QCoreApplication::translate("Events", "Collection unavailable");
    if (code == "recovered")
        return QCoreApplication::translate("Events", "Collection recovered");
    if (code == "attention")
        return QCoreApplication::translate("Events", "An item requires attention");
    if (code == "action-succeeded")
        return QCoreApplication::translate("Events", "Action completed");
    if (code == "action-failed")
        return QCoreApplication::translate("Events", "Action failed");
    return {};
}
} // namespace
EventStore::EventStore() {
    m_path = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/events.json";
    QFile file(m_path);
    if (!file.open(QIODevice::ReadOnly) || file.size() > 1024 * 1024)
        return;
    const auto array = QJsonDocument::fromJson(file.readAll()).array();
    for (const auto& v : array) {
        const auto o = v.toObject();
        const auto code = o["code"].toString(), module = o["module"].toString(), time = o["time"].toString();
        if (label(code).isEmpty() || !QDateTime::fromString(time, Qt::ISODate).isValid() || centerTitle(module) == module)
            continue;
        m_rows.append({QUuid::createUuid().toString(QUuid::WithoutBraces),
                       {time, centerTitle(module), label(code)},
                       {},
                       {{"module", module}, {"code", code}}});
        if (m_rows.size() >= 1000)
            break;
    }
}
void EventStore::append(const QString& module, const QString& code) {
    if (label(code).isEmpty() || centerTitle(module) == module)
        return;
    const auto time = QDateTime::currentDateTime().toString(Qt::ISODate);
    m_rows.prepend({QUuid::createUuid().toString(QUuid::WithoutBraces),
                    {time, centerTitle(module), label(code)},
                    {},
                    {{"module", module}, {"code", code}}});
    while (m_rows.size() > 1000)
        m_rows.removeLast();
    save();
}
QVector<CenterRecord> EventStore::rows() const {
    return m_rows;
}
void EventStore::save() const {
    QDir().mkpath(QFileInfo(m_path).absolutePath());
    QJsonArray out;
    for (const auto& r : m_rows)
        out.append(
            QJsonObject{{"time", r.cells[0]}, {"module", r.meta.value("module").toString()}, {"code", r.meta.value("code").toString()}});
    QSaveFile file(m_path);
    if (file.open(QIODevice::WriteOnly)) {
        file.setPermissions(QFile::ReadOwner | QFile::WriteOwner);
        file.write(QJsonDocument(out).toJson(QJsonDocument::Compact));
        file.commit();
    }
}
