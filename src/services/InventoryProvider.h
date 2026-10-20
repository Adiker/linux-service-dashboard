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
    static QStringList headers(const QString& module);
    static QVector<CenterRecord> parse(const QString& module, const QString& tag, const QString& output, QString* error);
  signals:
    void ready(const QString& module, const QStringList& headers, const QVector<CenterRecord>& rows, const QString& error);

  private:
    CommandRunner m_runner;
    QHash<QString, int> m_pending;
    QHash<QString, QVector<CenterRecord>> m_rows;
    QHash<QString, QStringList> m_errors;
    void start(const QString& module, const QVector<InventoryCommand>& commands);
};
