#pragma once

#include <QDialog>

class QPlainTextEdit;

class LogViewerDialog : public QDialog {
    Q_OBJECT

public:
    explicit LogViewerDialog(const QString &title, const QString &text, QWidget *parent = nullptr);

private:
    QPlainTextEdit *m_textEdit = nullptr;
};
