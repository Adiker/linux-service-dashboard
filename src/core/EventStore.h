#pragma once
#include "CenterTypes.h"
class EventStore {
  public:
    EventStore();
    void append(const QString& module, const QString& code);
    QVector<CenterRecord> rows() const;

  private:
    QVector<CenterRecord> m_rows;
    QString m_path;
    void save() const;
};
