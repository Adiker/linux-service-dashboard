#include "StatusBanner.h"

#include "../core/CommandResult.h"
#include "../ui/ThemeManager.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QStyle>

StatusBanner::StatusBanner(QWidget* parent) : QFrame(parent) {
    setObjectName(QStringLiteral("statusBanner"));
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(8, 4, 8, 4);
    layout->setSpacing(8);

    m_iconLabel = new QLabel(this);
    m_iconLabel->setFixedSize(16, 16);
    m_textLabel = new QLabel(this);
    m_textLabel->setObjectName(QStringLiteral("statusBannerText"));
    m_textLabel->setWordWrap(true);

    layout->addWidget(m_iconLabel);
    layout->addWidget(m_textLabel, 1);
    setMessage(QStringLiteral("Ready"), Severity::Info);
}

void StatusBanner::setMessage(const QString& text, Severity severity) {
    QString severityName = QStringLiteral("info");
    QIcon icon;
    switch (severity) {
    case Severity::Ok:
        severityName = QStringLiteral("ok");
        icon = ThemeManager::themedIcon(QStringLiteral("dialog-ok-apply"), QStyle::SP_DialogApplyButton);
        break;
    case Severity::Warning:
        severityName = QStringLiteral("warning");
        icon = ThemeManager::themedIcon(QStringLiteral("dialog-warning"), QStyle::SP_MessageBoxWarning);
        break;
    case Severity::Error:
        severityName = QStringLiteral("error");
        icon = ThemeManager::themedIcon(QStringLiteral("dialog-error"), QStyle::SP_MessageBoxCritical);
        break;
    case Severity::Info:
        severityName = QStringLiteral("info");
        icon = ThemeManager::themedIcon(QStringLiteral("dialog-information"), QStyle::SP_MessageBoxInformation);
        break;
    }

    setProperty("severity", severityName);
    style()->unpolish(this);
    style()->polish(this);
    m_iconLabel->setPixmap(icon.pixmap(16, 16));
    m_textLabel->setText(text);
}

void StatusBanner::setResultMessage(const QString& summaryText, const QString& errorText) {
    if (errorText.isEmpty()) {
        setMessage(summaryText, Severity::Info);
        return;
    }
    if (isPermissionErrorText(errorText)) {
        setMessage(QStringLiteral("Permission required: %1").arg(errorText), Severity::Warning);
        return;
    }
    if (errorText.contains(QStringLiteral("not found"), Qt::CaseInsensitive) ||
        errorText.contains(QStringLiteral("install "), Qt::CaseInsensitive)) {
        setMessage(QStringLiteral("Missing tool: %1").arg(errorText), Severity::Warning);
        return;
    }
    setMessage(errorText, Severity::Error);
}
