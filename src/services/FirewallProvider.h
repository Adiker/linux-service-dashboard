#pragma once
#include "../core/CenterTypes.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QObject>
#include <functional>

class FirewallProvider : public QObject {
    Q_OBJECT
  public:
    explicit FirewallProvider(QObject* parent = nullptr, const QDBusConnection& bus = QDBusConnection::systemBus());
    void refresh();
  signals:
    void ready(const QVector<CenterRecord>& rows, const QString& error);

  private:
    QDBusConnection m_bus;
    bool m_busy = false;
    int m_pending = 0;
    QVector<CenterRecord> m_rows;
    QStringList m_errors;
    void query(const QString& method, const QVariantList& arguments, const QString& action,
               std::function<void(const QDBusMessage&, const QString&)> callback);
    void finish();
};
