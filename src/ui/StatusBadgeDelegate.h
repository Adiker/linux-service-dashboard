#pragma once

#include <QStyledItemDelegate>

enum class StatusTone {
    Ok,
    Warn,
    Error,
    Muted,
};

StatusTone classifyStatusTone(const QString &status);

class StatusBadgeDelegate : public QStyledItemDelegate {
    Q_OBJECT

public:
    explicit StatusBadgeDelegate(QObject *parent = nullptr);

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;
};
