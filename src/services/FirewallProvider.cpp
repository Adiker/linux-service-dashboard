#include "FirewallProvider.h"

#include <QDBusArgument>
#include <QDBusMetaType>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusVariant>

namespace {
QString valueText(const QVariant& value) {
    if (value.metaType() == QMetaType::fromType<QDBusArgument>()) {
        const auto argument = qvariant_cast<QDBusArgument>(value);
        if (argument.currentSignature() == "as")
            return qdbus_cast<QStringList>(argument).join(' ');
        if (argument.currentSignature() == "aas") {
            QStringList text;
            for (const auto& fields : qdbus_cast<QList<QStringList>>(argument))
                text.append(fields.join('/'));
            return text.join(' ');
        }
        return QStringLiteral("DBus: %1").arg(argument.currentSignature());
    }
    if (value.metaType() == QMetaType::fromType<QStringList>())
        return value.toStringList().join(' ');
    return value.toString();
}
} // namespace

FirewallProvider::FirewallProvider(QObject* parent, const QDBusConnection& bus) : QObject(parent), m_bus(bus) {
    qDBusRegisterMetaType<QList<QStringList>>();
    qDBusRegisterMetaType<QMap<QString, QString>>();
}
void FirewallProvider::query(const QString& method, const QVariantList& arguments, const QString& action,
                             std::function<void(const QDBusMessage&, const QString&)> callback) {
    // firewalld itself allows interactive authorization, even when its caller does not.
    // Preflight the exact bus sender without interaction before each protected read.
    QDBusArgument subject;
    subject.beginStructure();
    subject << QStringLiteral("system-bus-name") << QVariantMap{{"name", m_bus.baseService()}};
    subject.endStructure();
    auto check = QDBusMessage::createMethodCall("org.freedesktop.PolicyKit1", "/org/freedesktop/PolicyKit1/Authority",
                                                "org.freedesktop.PolicyKit1.Authority", "CheckAuthorization");
    check.setInteractiveAuthorizationAllowed(false);
    check.setArguments({QVariant::fromValue(subject), action, QVariant::fromValue(QMap<QString, QString>{}), uint(0), QString{}});
    auto* watcher = new QDBusPendingCallWatcher(m_bus.asyncCall(check, 5000), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, method, arguments, callback](QDBusPendingCallWatcher* pending) {
        const QDBusMessage reply = pending->reply();
        pending->deleteLater();
        if (reply.type() == QDBusMessage::ErrorMessage) {
            callback({}, reply.errorMessage());
            return;
        }
        bool authorized = false, challenge = false;
        QMap<QString, QString> details;
        if (reply.signature() == "(bba{ss})") {
            const auto result = qvariant_cast<QDBusArgument>(reply.arguments().value(0));
            result.beginStructure();
            result >> authorized >> challenge >> details;
            result.endStructure();
        }
        if (!authorized) {
            callback({}, tr("Firewall inspection requires authorization; open firewall-config explicitly."));
            return;
        }
        auto request = QDBusMessage::createMethodCall("org.fedoraproject.FirewallD1", "/org/fedoraproject/FirewallD1",
                                                      "org.fedoraproject.FirewallD1.zone", method);
        request.setInteractiveAuthorizationAllowed(false);
        request.setArguments(arguments);
        auto* reader = new QDBusPendingCallWatcher(m_bus.asyncCall(request, 5000), this);
        connect(reader, &QDBusPendingCallWatcher::finished, this, [callback](QDBusPendingCallWatcher* read) {
            const auto result = read->reply();
            read->deleteLater();
            callback(result, result.type() == QDBusMessage::ErrorMessage ? result.errorMessage() : QString{});
        });
    });
}
void FirewallProvider::refresh() {
    if (m_busy)
        return;
    m_busy = true;
    m_rows.clear();
    m_errors.clear();
    query("getZones", {}, "org.fedoraproject.FirewallD1.info", [this](const QDBusMessage& message, const QString& error) {
        QDBusPendingReply<QStringList> reply = message;
        if (!error.isEmpty() || reply.isError()) {
            m_errors.append(error.isEmpty() ? reply.error().message() : error);
            finish();
            return;
        }
        auto zones = reply.value();
        if (zones.size() > 128) {
            m_errors.append(tr("Firewall zone data exceeds the display limit"));
            zones = zones.mid(0, 128);
        }
        m_pending = zones.size();
        if (zones.isEmpty()) {
            finish();
            return;
        }
        for (const auto& zone : zones)
            query("getZoneSettings2", {zone}, "org.fedoraproject.FirewallD1.config.info",
                  [this, zone](const QDBusMessage& message, const QString& error) {
                      QDBusPendingReply<QVariantMap> settings = message;
                      if (!error.isEmpty() || settings.isError())
                          m_errors.append(error.isEmpty() ? settings.error().message() : error);
                      else {
                          const auto values = settings.value();
                          for (auto it = values.cbegin(); it != values.cend(); ++it) {
                              if (m_rows.size() >= 4096) {
                                  m_errors.append(tr("Firewall zone data exceeds the display limit"));
                                  break;
                              }
                              m_rows.append(
                                  {zone + ":" + it.key(), {it.key(), valueText(it.value()).left(2048), tr("Read-only")}, {zone}, {}});
                          }
                      }
                      if (--m_pending == 0)
                          finish();
                  });
    });
}
void FirewallProvider::finish() {
    m_busy = false;
    m_errors.removeDuplicates();
    emit ready(m_rows, m_errors.join('\n'));
}
