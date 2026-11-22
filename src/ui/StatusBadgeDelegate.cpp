#include "StatusBadgeDelegate.h"

#include "ThemeManager.h"

#include <QApplication>
#include <QPainter>

StatusTone classifyStatusTone(const QString &status)
{
    const QString s = status.trimmed().toLower();
    if (s.isEmpty() || s == QStringLiteral("-") || s == QStringLiteral("unknown")
        || s == QStringLiteral("unavailable") || s == QStringLiteral("disconnected")
        || s.startsWith(QStringLiteral("configured")) || s.startsWith(QStringLiteral("profile:"))) {
        return StatusTone::Muted;
    }
    if (s.contains(QStringLiteral("fail")) || s.contains(QStringLiteral("critical"))
        || s.contains(QStringLiteral("error")) || s.contains(QStringLiteral("denied"))
        || s.contains(QStringLiteral("missing"))) {
        return StatusTone::Error;
    }
    if (s.contains(QStringLiteral("warn")) || s.contains(QStringLiteral("inactive"))
        || s.contains(QStringLiteral("exited")) || s.contains(QStringLiteral("activating"))
        || s.contains(QStringLiteral("deactivating")) || s.contains(QStringLiteral("paused"))
        || s.contains(QStringLiteral("restarting")) || s.contains(QStringLiteral("authorization"))) {
        return StatusTone::Warn;
    }
    if (s.contains(QStringLiteral("active")) || s.contains(QStringLiteral("running"))
        || s.contains(QStringLiteral("passed")) || s == QStringLiteral("ok")
        || s == QStringLiteral("mounted") || s == QStringLiteral("connected")
        || s == QStringLiteral("activated")) {
        return StatusTone::Ok;
    }
    return StatusTone::Muted;
}

StatusBadgeDelegate::StatusBadgeDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
{
}

void StatusBadgeDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    QStyleOptionViewItem opt(option);
    initStyleOption(&opt, index);

    const QString text = opt.text;
    opt.text.clear();

    const QWidget *widget = opt.widget;
    QStyle *style = widget ? widget->style() : QApplication::style();
    style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, widget);

    if (text.isEmpty()) {
        return;
    }

    const ThemePalette &palette = ThemeManager::currentPalette();
    QColor fg = palette.mutedStatus;
    QColor bg = palette.mutedStatusBg;
    switch (classifyStatusTone(text)) {
    case StatusTone::Ok:
        fg = palette.ok;
        bg = palette.okBg;
        break;
    case StatusTone::Warn:
        fg = palette.warn;
        bg = palette.warnBg;
        break;
    case StatusTone::Error:
        fg = palette.error;
        bg = palette.errorBg;
        break;
    case StatusTone::Muted:
        fg = palette.mutedStatus;
        bg = palette.mutedStatusBg;
        break;
    }

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    QFont badgeFont = opt.font;
    badgeFont.setWeight(QFont::DemiBold);
    painter->setFont(badgeFont);

    const QFontMetrics fm(badgeFont);
    const int textWidth = fm.horizontalAdvance(text);
    const int padX = 10;
    const int pillHeight = qMin(opt.rect.height() - 6, fm.height() + 6);
    const int maxPillWidth = qMax(24, opt.rect.width() - 12);
    const int pillWidth = qMin(maxPillWidth, textWidth + padX * 2);

    const QRect pillRect(opt.rect.left() + 6,
                         opt.rect.center().y() - pillHeight / 2,
                         pillWidth,
                         pillHeight);

    painter->setPen(Qt::NoPen);
    painter->setBrush(bg);
    painter->drawRoundedRect(pillRect, 6, 6);

    painter->setPen(fg);
    const QString elided = fm.elidedText(text, Qt::ElideRight, qMax(8, pillWidth - padX));
    painter->drawText(pillRect, Qt::AlignCenter, elided);
    painter->restore();
}

QSize StatusBadgeDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    const QSize base = QStyledItemDelegate::sizeHint(option, index);
    return QSize(base.width() + 24, qMax(base.height(), 30));
}
