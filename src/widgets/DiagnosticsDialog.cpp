#include "DiagnosticsDialog.h"

#include "../core/ServiceRegistry.h"

#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIcon>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

DiagnosticsDialog::DiagnosticsDialog(ServiceRegistry* registry, QWidget* parent) : QDialog(parent), m_registry(registry) {
    setWindowTitle(QStringLiteral("Command Diagnostics"));
    resize(920, 520);

    auto* layout = new QVBoxLayout(this);

    auto* topRow = new QHBoxLayout;
    topRow->addWidget(new QLabel(QStringLiteral("Filter by type:"), this));
    m_kindFilter = new QComboBox(this);
    m_kindFilter->addItem(QStringLiteral("All errors"), -1);
    m_kindFilter->addItem(commandErrorKindLabel(CommandErrorKind::MissingTool), static_cast<int>(CommandErrorKind::MissingTool));
    m_kindFilter->addItem(commandErrorKindLabel(CommandErrorKind::PermissionDenied), static_cast<int>(CommandErrorKind::PermissionDenied));
    m_kindFilter->addItem(commandErrorKindLabel(CommandErrorKind::Timeout), static_cast<int>(CommandErrorKind::Timeout));
    m_kindFilter->addItem(commandErrorKindLabel(CommandErrorKind::ExecutionFailed), static_cast<int>(CommandErrorKind::ExecutionFailed));
    topRow->addWidget(m_kindFilter);
    topRow->addStretch();
    layout->addLayout(topRow);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(5);
    m_table->setHorizontalHeaderLabels({
        QStringLiteral("Time"),
        QStringLiteral("Type"),
        QStringLiteral("Context"),
        QStringLiteral("Command"),
        QStringLiteral("Message"),
    });
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setAlternatingRowColors(true);
    m_table->verticalHeader()->hide();
    layout->addWidget(m_table, 1);

    auto* bottomRow = new QHBoxLayout;
    auto* copyButton = new QPushButton(QIcon::fromTheme(QStringLiteral("edit-copy")), QStringLiteral("Copy report"), this);
    auto* clearButton = new QPushButton(QIcon::fromTheme(QStringLiteral("edit-clear")), QStringLiteral("Clear"), this);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    bottomRow->addWidget(copyButton);
    bottomRow->addWidget(clearButton);
    bottomRow->addStretch();
    bottomRow->addWidget(buttons);
    layout->addLayout(bottomRow);

    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(copyButton, &QPushButton::clicked, this, &DiagnosticsDialog::copyReport);
    connect(clearButton, &QPushButton::clicked, this, [this]() {
        if (m_registry) {
            m_registry->clearFailedCommands();
        }
    });
    connect(m_kindFilter, &QComboBox::currentIndexChanged, this, &DiagnosticsDialog::reloadTable);
    if (m_registry) {
        connect(m_registry, &ServiceRegistry::failedCommandRecorded, this, &DiagnosticsDialog::reloadTable);
        connect(m_registry, &ServiceRegistry::failedCommandsCleared, this, &DiagnosticsDialog::reloadTable);
    }

    reloadTable();
}

void DiagnosticsDialog::reloadTable() {
    m_table->setRowCount(0);
    if (!m_registry) {
        return;
    }
    const int filterKind = m_kindFilter->currentData().toInt();
    const QVector<FailedCommandEntry> entries = m_registry->failedCommands();
    for (int i = entries.size() - 1; i >= 0; --i) {
        const FailedCommandEntry& entry = entries.at(i);
        if (filterKind >= 0 && static_cast<int>(entry.errorKind) != filterKind) {
            continue;
        }
        const int row = m_table->rowCount();
        m_table->insertRow(row);
        m_table->setItem(row, 0, new QTableWidgetItem(entry.timestamp));
        m_table->setItem(row, 1, new QTableWidgetItem(commandErrorKindLabel(entry.errorKind)));
        m_table->setItem(row, 2, new QTableWidgetItem(entry.context));
        m_table->setItem(row, 3, new QTableWidgetItem(entry.commandLine));
        m_table->setItem(row, 4, new QTableWidgetItem(entry.message));
    }
    m_table->resizeColumnsToContents();
}

void DiagnosticsDialog::copyReport() {
    if (!m_registry) {
        return;
    }
    QStringList lines;
    for (const FailedCommandEntry& entry : m_registry->failedCommands()) {
        lines.append(QStringLiteral("[%1] (%2) %3 — %4 -> %5")
                         .arg(entry.timestamp, commandErrorKindLabel(entry.errorKind), entry.context, entry.commandLine, entry.message));
    }
    if (QClipboard* cb = QApplication::clipboard()) {
        cb->setText(lines.isEmpty() ? QStringLiteral("No failed commands recorded.") : lines.join(QLatin1Char('\n')));
    }
}
