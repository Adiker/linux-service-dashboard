#include "ConfirmActionDialog.h"

#include <QMessageBox>

bool ConfirmActionDialog::confirm(QWidget *parent, const QString &title, const QString &message)
{
    return QMessageBox::question(parent, title, message, QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel) == QMessageBox::Yes;
}
