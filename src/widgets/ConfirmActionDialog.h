#pragma once

#include <QDialog>

class QWidget;

class ConfirmActionDialog : public QDialog {
    Q_OBJECT

public:
    static bool confirm(QWidget *parent, const QString &title, const QString &message);
};
