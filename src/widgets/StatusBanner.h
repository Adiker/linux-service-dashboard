#pragma once

#include <QFrame>
#include <QString>

class QLabel;

class StatusBanner : public QFrame {
    Q_OBJECT

  public:
    enum class Severity {
        Info,
        Ok,
        Warning,
        Error,
    };

    explicit StatusBanner(QWidget* parent = nullptr);

    void setMessage(const QString& text, Severity severity = Severity::Info);
    void setResultMessage(const QString& summaryText, const QString& errorText = {});

  private:
    QLabel* m_iconLabel = nullptr;
    QLabel* m_textLabel = nullptr;
};
