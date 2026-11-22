#include "LogViewerDialog.h"

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFile>
#include <QFileDialog>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QIcon>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QShortcut>
#include <QTextCursor>
#include <QTextDocument>
#include <QVBoxLayout>

LogViewerDialog::LogViewerDialog(const QString &title,
                                 const QString &text,
                                 QWidget *parent,
                                 RefreshCallback refreshCallback)
    : QDialog(parent)
    , m_refreshCallback(std::move(refreshCallback))
{
    setWindowTitle(title);
    resize(960, 640);

    auto *layout = new QVBoxLayout(this);

    auto *toolbar = new QHBoxLayout;
    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText(QStringLiteral("Find in output (Ctrl+F)..."));
    m_searchEdit->setClearButtonEnabled(true);
    auto *prevButton = new QPushButton(QStringLiteral("Prev"), this);
    auto *nextButton = new QPushButton(QStringLiteral("Next"), this);
    m_matchLabel = new QLabel(this);
    m_matchLabel->setMinimumWidth(72);

    m_wrapCheck = new QCheckBox(QStringLiteral("Wrap"), this);
    m_wrapCheck->setChecked(true);

    toolbar->addWidget(m_searchEdit, 1);
    toolbar->addWidget(prevButton);
    toolbar->addWidget(nextButton);
    toolbar->addWidget(m_matchLabel);
    toolbar->addWidget(m_wrapCheck);

    if (m_refreshCallback) {
        m_tailCombo = new QComboBox(this);
        m_tailCombo->addItem(QStringLiteral("100 lines"), 100);
        m_tailCombo->addItem(QStringLiteral("300 lines"), 300);
        m_tailCombo->addItem(QStringLiteral("1000 lines"), 1000);
        m_tailCombo->setCurrentIndex(1);
        m_refreshButton = new QPushButton(QIcon::fromTheme(QStringLiteral("view-refresh")), QStringLiteral("Refresh"), this);
        toolbar->addWidget(m_tailCombo);
        toolbar->addWidget(m_refreshButton);
        connect(m_refreshButton, &QPushButton::clicked, this, [this]() {
            if (m_refreshCallback) {
                m_refreshButton->setEnabled(false);
                m_refreshCallback(m_tailCombo->currentData().toInt());
            }
        });
    }
    layout->addLayout(toolbar);

    m_textEdit = new QPlainTextEdit(this);
    m_textEdit->setReadOnly(true);
    m_textEdit->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    m_textEdit->setLineWrapMode(QPlainTextEdit::WidgetWidth);
    m_textEdit->setPlainText(text);
    layout->addWidget(m_textEdit, 1);

    auto *bottomRow = new QHBoxLayout;
    auto *copyButton = new QPushButton(QIcon::fromTheme(QStringLiteral("edit-copy")), QStringLiteral("Copy all"), this);
    auto *saveButton = new QPushButton(QIcon::fromTheme(QStringLiteral("document-save")), QStringLiteral("Save to file..."), this);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    bottomRow->addWidget(copyButton);
    bottomRow->addWidget(saveButton);
    bottomRow->addStretch();
    bottomRow->addWidget(buttons);
    layout->addLayout(bottomRow);

    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(copyButton, &QPushButton::clicked, this, [this]() {
        if (QClipboard *cb = QApplication::clipboard()) {
            cb->setText(m_textEdit->toPlainText());
        }
    });
    connect(saveButton, &QPushButton::clicked, this, &LogViewerDialog::saveToFile);
    connect(m_wrapCheck, &QCheckBox::toggled, this, [this](bool wrap) {
        m_textEdit->setLineWrapMode(wrap ? QPlainTextEdit::WidgetWidth : QPlainTextEdit::NoWrap);
    });
    connect(nextButton, &QPushButton::clicked, this, [this]() { findNext(false); });
    connect(prevButton, &QPushButton::clicked, this, [this]() { findNext(true); });
    connect(m_searchEdit, &QLineEdit::returnPressed, this, [this]() { findNext(false); });
    connect(m_searchEdit, &QLineEdit::textChanged, this, &LogViewerDialog::updateMatchCount);

    auto *findShortcut = new QShortcut(QKeySequence::Find, this);
    connect(findShortcut, &QShortcut::activated, this, [this]() {
        m_searchEdit->setFocus();
        m_searchEdit->selectAll();
    });

    updateMatchCount();
}

void LogViewerDialog::setContent(const QString &text)
{
    m_textEdit->setPlainText(text);
    if (m_refreshButton) {
        m_refreshButton->setEnabled(true);
    }
    updateMatchCount();
}

void LogViewerDialog::findNext(bool backwards)
{
    const QString query = m_searchEdit->text();
    if (query.isEmpty()) {
        return;
    }
    QTextDocument::FindFlags flags;
    if (backwards) {
        flags |= QTextDocument::FindBackward;
    }
    if (!m_textEdit->find(query, flags)) {
        QTextCursor cursor = m_textEdit->textCursor();
        cursor.movePosition(backwards ? QTextCursor::End : QTextCursor::Start);
        m_textEdit->setTextCursor(cursor);
        m_textEdit->find(query, flags);
    }
}

void LogViewerDialog::updateMatchCount()
{
    const QString query = m_searchEdit->text();
    if (query.isEmpty()) {
        m_matchLabel->clear();
        return;
    }
    const QString content = m_textEdit->toPlainText();
    int count = 0;
    qsizetype pos = 0;
    while ((pos = content.indexOf(query, pos, Qt::CaseInsensitive)) != -1) {
        ++count;
        pos += query.size();
    }
    m_matchLabel->setText(QStringLiteral("%1 matches").arg(count));
}

void LogViewerDialog::saveToFile()
{
    const QString path = QFileDialog::getSaveFileName(this, QStringLiteral("Save output"), QStringLiteral("output.log"), QStringLiteral("Log files (*.log *.txt *.json);;All files (*)"));
    if (path.isEmpty()) {
        return;
    }
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        QMessageBox::warning(this, QStringLiteral("Save output"), QStringLiteral("Could not open %1 for writing.").arg(path));
        return;
    }
    file.write(m_textEdit->toPlainText().toUtf8());
}
