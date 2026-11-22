#pragma once

#include "CommandResult.h"

#include <QHash>
#include <QObject>
#include <QString>
#include <QStringList>

class CommandRunner : public QObject {
    Q_OBJECT

  public:
    explicit CommandRunner(QObject* parent = nullptr);
    ~CommandRunner() override;

    QString run(const QString& program, const QStringList& arguments = {}, int timeoutMs = 15000, const QString& context = {});

    [[nodiscard]] QStringList failedCommandLog() const;

  signals:
    void commandFinished(const QString& id, const CommandResult& result, const QString& context);

  private:
    QStringList m_failedCommandLog;
    QHash<QString, QString> m_pending;
};
