#include "ThemeManager.h"

#include <QApplication>

namespace ThemeManager {

namespace {

ThemePalette& activePaletteStorage() {
    static ThemePalette palette = paletteForPreference(QStringLiteral("Dark"), QPalette());
    return palette;
}

} // namespace

ThemePalette paletteForPreference(const QString& preference, const QPalette& systemPalette) {
    const bool systemDark = systemPalette.color(QPalette::Window).lightness() < 128;
    const bool oled = (preference == QStringLiteral("OLED"));
    const bool dark = oled || (preference == QStringLiteral("Dark")) || (preference == QStringLiteral("System") && systemDark);

    ThemePalette p;
    p.isDark = dark;
    p.isOled = oled;

    if (oled) {
        p.window = QStringLiteral("#000000");
        p.sidebar = QStringLiteral("#000000");
        p.surface = QStringLiteral("#060606");
        p.surfaceAlt = QStringLiteral("#101010");
        p.border = QStringLiteral("#2a2a2a");
        p.text = QStringLiteral("#f3f5f7");
        p.muted = QStringLiteral("#a7adb4");
        p.accent = QStringLiteral("#32d296");
        p.accentSoft = QStringLiteral("#113428");

        p.ok = QColor(QStringLiteral("#3be39b"));
        p.okBg = QColor(QStringLiteral("#0a2a1d"));
        p.warn = QColor(QStringLiteral("#ffd166"));
        p.warnBg = QColor(QStringLiteral("#2b2108"));
        p.error = QColor(QStringLiteral("#ff7b72"));
        p.errorBg = QColor(QStringLiteral("#2f1215"));
        p.mutedStatus = QColor(QStringLiteral("#a7adb4"));
        p.mutedStatusBg = QColor(QStringLiteral("#161616"));
    } else if (dark) {
        p.window = QStringLiteral("#161a20");
        p.sidebar = QStringLiteral("#11161d");
        p.surface = QStringLiteral("#20262e");
        p.surfaceAlt = QStringLiteral("#29313a");
        p.border = QStringLiteral("#38424d");
        p.text = QStringLiteral("#eef2f5");
        p.muted = QStringLiteral("#aeb8c2");
        p.accent = QStringLiteral("#5db6a3");
        p.accentSoft = QStringLiteral("#243f3f");

        p.ok = QColor(QStringLiteral("#7ce0a3"));
        p.okBg = QColor(QStringLiteral("#173326"));
        p.warn = QColor(QStringLiteral("#f6c96c"));
        p.warnBg = QColor(QStringLiteral("#3a2e14"));
        p.error = QColor(QStringLiteral("#ff8f85"));
        p.errorBg = QColor(QStringLiteral("#3c1c1e"));
        p.mutedStatus = QColor(QStringLiteral("#aeb8c2"));
        p.mutedStatusBg = QColor(QStringLiteral("#272f38"));
    } else {
        p.window = QStringLiteral("#f6f7f9");
        p.sidebar = QStringLiteral("#eef1f4");
        p.surface = QStringLiteral("#ffffff");
        p.surfaceAlt = QStringLiteral("#f1f4f7");
        p.border = QStringLiteral("#d5dce4");
        p.text = QStringLiteral("#1f2732");
        p.muted = QStringLiteral("#596573");
        p.accent = QStringLiteral("#267980");
        p.accentSoft = QStringLiteral("#dcefeb");

        p.ok = QColor(QStringLiteral("#146c38"));
        p.okBg = QColor(QStringLiteral("#dff4e7"));
        p.warn = QColor(QStringLiteral("#8f5200"));
        p.warnBg = QColor(QStringLiteral("#fcf0d6"));
        p.error = QColor(QStringLiteral("#b42318"));
        p.errorBg = QColor(QStringLiteral("#fde4e1"));
        p.mutedStatus = QColor(QStringLiteral("#4b5563"));
        p.mutedStatusBg = QColor(QStringLiteral("#e9edf2"));
    }

    return p;
}

QString buildStyleSheet(const ThemePalette& p) {
    QString qss = QStringLiteral(R"(
        QWidget {
            font-size: 10.5pt;
        }
        QMainWindow, QDialog, QMessageBox, QWidget#appRoot, QStackedWidget#contentStack {
            background: @window;
            color: @text;
        }
        QScrollArea, QScrollArea > QWidget > QWidget {
            background: transparent;
            border: 0;
        }
        QLabel, QCheckBox, QRadioButton {
            background: transparent;
            color: @text;
        }
        QTreeWidget#sidebar {
            background: @sidebar;
            border: 0;
            border-right: 1px solid @border;
            padding: 16px 10px;
        }
        QTreeWidget#sidebar::item {
            border-radius: 6px;
            padding: 6px 8px;
            margin: 1px 0;
            color: @muted;
            border: 1px solid transparent;
        }
        QTreeWidget#sidebar::item:hover {
            background: @surfaceAlt;
            color: @text;
        }
        QTreeWidget#sidebar::item:selected {
            background: @accentSoft;
            color: @text;
            border-color: @accent;
            font-weight: 700;
        }
        QLabel#pageTitle {
            font-size: 21pt;
            font-weight: 700;
            padding: 14px 4px 12px 4px;
        }
        QLabel#sectionTitle {
            color: @muted;
            font-weight: 700;
            padding: 2px 4px 0 4px;
        }
        QWidget#card, QFrame#settingsSection {
            background: @surface;
            border: 1px solid @border;
            border-radius: 8px;
            padding: 0px;
        }
        QLabel#cardNote, QLabel#smartHint {
            font-size: 9pt;
            color: @muted;
        }
        QLabel#cardValue {
            font-size: 18pt;
            font-weight: 700;
        }
        QTableView, QTreeView, QTableWidget {
            background: @surface;
            alternate-background-color: @surfaceAlt;
            gridline-color: @border;
            selection-background-color: @accentSoft;
            selection-color: @text;
            border: 1px solid @border;
            border-radius: 6px;
            outline: 0;
        }
        QHeaderView::section {
            background: @surfaceAlt;
            color: @text;
            border: 0;
            border-bottom: 1px solid @border;
            padding: 8px;
            font-weight: 700;
        }
        QTableCornerButton::section {
            background: @surfaceAlt;
            border: 0;
            border-bottom: 1px solid @border;
            border-right: 1px solid @border;
        }
        QTableView::item, QTableWidget::item {
            padding: 5px;
        }
        QTableView::item:selected, QTableWidget::item:selected {
            background: @accentSoft;
            color: @text;
        }
        QPushButton, QLineEdit, QSpinBox, QComboBox, QPlainTextEdit, QMenu {
            background: @surface;
            color: @text;
            border: 1px solid @border;
            border-radius: 6px;
            padding: 7px 9px;
        }
        QLineEdit, QSpinBox, QComboBox, QPlainTextEdit {
            selection-background-color: @accent;
            selection-color: @window;
        }
        QPushButton:hover {
            background: @surfaceAlt;
            border-color: @accent;
        }
        QPushButton:pressed {
            background: @accentSoft;
        }
        QPushButton:disabled, QLineEdit:disabled, QSpinBox:disabled, QComboBox:disabled {
            color: @muted;
        }
        QLineEdit:focus, QSpinBox:focus, QComboBox:focus, QPlainTextEdit:focus, QPushButton:focus {
            border-color: @accent;
        }
        QCheckBox {
            spacing: 8px;
            padding: 4px 2px;
            font-weight: 400;
        }
        QCheckBox:hover {
            color: @text;
        }
        QCheckBox::indicator {
            width: 16px;
            height: 16px;
            border: 1px solid @border;
            border-radius: 4px;
            background: @surface;
        }
        QCheckBox::indicator:hover {
            border-color: @accent;
        }
        QCheckBox::indicator:checked {
            background: @accentSoft;
            border-color: @accent;
            image: url(:/qt-project.org/styles/commonstyle/images/standardbutton-apply-16.png);
        }
        QComboBox::drop-down {
            border: 0;
            width: 24px;
        }
        QComboBox QAbstractItemView {
            background: @surface;
            color: @text;
            selection-background-color: @accentSoft;
            selection-color: @text;
            border: 1px solid @border;
            outline: 0;
        }
        QAbstractSpinBox::up-button, QAbstractSpinBox::down-button {
            background: @surfaceAlt;
            border: 0;
            width: 18px;
        }
        QAbstractSpinBox::up-button:hover, QAbstractSpinBox::down-button:hover {
            background: @accentSoft;
        }
        QScrollBar:vertical {
            background: @window;
            width: 12px;
            margin: 0;
        }
        QScrollBar:horizontal {
            background: @window;
            height: 12px;
            margin: 0;
        }
        QScrollBar::handle:vertical {
            background: @border;
            border-radius: 6px;
            min-height: 28px;
        }
        QScrollBar::handle:horizontal {
            background: @border;
            border-radius: 6px;
            min-width: 28px;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical,
        QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {
            height: 0;
            width: 0;
        }
        QMenu::item {
            padding: 7px 22px;
            border-radius: 4px;
        }
        QMenu::item:selected {
            background: @accentSoft;
        }
        QStatusBar, QStatusBar QLabel {
            background: @window;
            color: @muted;
        }
        QToolTip {
            background: @surface;
            color: @text;
            border: 1px solid @border;
        }
    )");

    qss.replace(QStringLiteral("@window"), p.window);
    qss.replace(QStringLiteral("@sidebar"), p.sidebar);
    qss.replace(QStringLiteral("@surfaceAlt"), p.surfaceAlt);
    qss.replace(QStringLiteral("@surface"), p.surface);
    qss.replace(QStringLiteral("@border"), p.border);
    qss.replace(QStringLiteral("@text"), p.text);
    qss.replace(QStringLiteral("@muted"), p.muted);
    qss.replace(QStringLiteral("@accentSoft"), p.accentSoft);
    qss.replace(QStringLiteral("@accent"), p.accent);
    return qss;
}

void applyToApplication(const QString& preference, const QPalette& systemPalette) {
    const ThemePalette p = paletteForPreference(preference, systemPalette);
    activePaletteStorage() = p;

    QPalette colors = systemPalette;
    colors.setColor(QPalette::Window, QColor(p.window));
    colors.setColor(QPalette::WindowText, QColor(p.text));
    colors.setColor(QPalette::Base, QColor(p.surface));
    colors.setColor(QPalette::AlternateBase, QColor(p.surfaceAlt));
    colors.setColor(QPalette::Text, QColor(p.text));
    colors.setColor(QPalette::Button, QColor(p.surface));
    colors.setColor(QPalette::ButtonText, QColor(p.text));
    colors.setColor(QPalette::ToolTipBase, QColor(p.surface));
    colors.setColor(QPalette::ToolTipText, QColor(p.text));
    colors.setColor(QPalette::Highlight, QColor(p.accent));
    colors.setColor(QPalette::HighlightedText, p.isDark ? QColor("#101010") : QColor("#ffffff"));
    colors.setColor(QPalette::Mid, QColor(p.border));
    colors.setColor(QPalette::Disabled, QPalette::Text, QColor(p.muted));
    colors.setColor(QPalette::Disabled, QPalette::WindowText, QColor(p.muted));
    colors.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(p.muted));
    QApplication::setPalette(colors);

    if (qApp) {
        qApp->setStyleSheet(buildStyleSheet(p));
    }
}

const ThemePalette& currentPalette() {
    return activePaletteStorage();
}

QIcon themedIcon(const QString& themeName, QStyle::StandardPixmap fallback) {
    QIcon icon = QIcon::fromTheme(themeName);
    if (!icon.isNull()) {
        return icon;
    }
    if (qApp && qApp->style()) {
        return qApp->style()->standardIcon(fallback);
    }
    return {};
}

} // namespace ThemeManager
