#pragma once

#include <QColor>
#include <QIcon>
#include <QPalette>
#include <QString>
#include <QStyle>

struct ThemePalette {
    bool isDark = false;
    bool isOled = false;

    QString window;
    QString sidebar;
    QString surface;
    QString surfaceAlt;
    QString border;
    QString text;
    QString muted;
    QString accent;
    QString accentSoft;

    QColor ok;
    QColor okBg;
    QColor warn;
    QColor warnBg;
    QColor error;
    QColor errorBg;
    QColor mutedStatus;
    QColor mutedStatusBg;
};

namespace ThemeManager {

ThemePalette paletteForPreference(const QString& preference, const QPalette& systemPalette);
QString buildStyleSheet(const ThemePalette& palette);
void applyToApplication(const QString& preference, const QPalette& systemPalette);
const ThemePalette& currentPalette();

QIcon themedIcon(const QString& themeName, QStyle::StandardPixmap fallback);

} // namespace ThemeManager
