#include "LogViewerDialog.h"

#include <QDialogButtonBox>
#include <QPlainTextEdit>
#include <QVBoxLayout>

LogViewerDialog::LogViewerDialog(const QString &title, const QString &text, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(title);
    resize(900, 600);

    auto *layout = new QVBoxLayout(this);
    m_textEdit = new QPlainTextEdit(this);
    m_textEdit->setReadOnly(true);
    m_textEdit->setPlainText(text);
    layout->addWidget(m_textEdit);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}
