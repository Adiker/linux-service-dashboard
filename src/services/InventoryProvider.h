#pragma once
#include "../core/CenterTypes.h"
#include "../core/CommandRunner.h"

#include <QHash>
#include <QSet>

struct InventoryCommand {
    QString program;
    QStringList arguments;
    QString tag;
};
class InventoryProvider : public QObject {
    Q_OBJECT
  public:
    explicit InventoryProvider(QObject* parent = nullptr);
    void refresh(const QString& module);
    void setServices(const QVector<CenterRecord>& rows) { m_services = rows; }
    void setTimers(const QVector<CenterRecord>& rows) { m_timers = rows; }
    void setMounts(const QVector<CenterRecord>& rows) { m_mounts = rows; }
    static QStringList headers(const QString& module);
    static QVector<CenterRecord> parse(const QString& module, const QString& tag, const QString& output, QString* error);
  signals:
    void ready(const QString& module, const QStringList& headers, const QVector<CenterRecord>& rows, const QString& error);

  private:
    QVector<CenterRecord> m_services, m_timers, m_mounts;
    CommandRunner m_runner;
    QHash<QString, int> m_pending;
    QHash<QString, QVector<CenterRecord>> m_rows;
    QHash<QString, QStringList> m_errors;
    void start(const QString& module, const QVector<InventoryCommand>& commands);
};
