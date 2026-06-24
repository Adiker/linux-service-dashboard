#include "DockerPage.h"

#include "../utils/TableLayoutPersistence.h"

#include "ConfirmActionDialog.h"
#include "LogViewerDialog.h"

#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTableView>
#include <QVBoxLayout>

DockerPage::DockerPage(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    auto *header = new QHBoxLayout;
    auto *title = new QLabel(QStringLiteral("Docker"), this);
    title->setObjectName(QStringLiteral("pageTitle"));
    auto *refreshButton = new QPushButton(QIcon::fromTheme(QStringLiteral("view-refresh")), QStringLiteral("Refresh"), this);
    header->addWidget(title);
    header->addStretch();
    header->addWidget(refreshButton);
    layout->addLayout(header);

    m_model = new DockerContainerModel(this);
    m_table = new QTableView(this);
    m_table->setModel(m_model);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->verticalHeader()->hide();
    m_table->setSortingEnabled(true);
    TableLayoutPersistence::bind(m_table, QStringLiteral("tables/docker/headerState"));
    layout->addWidget(m_table, 1);

    auto *actions = new QHBoxLayout;
    for (const QString &action : {QStringLiteral("start"), QStringLiteral("stop"), QStringLiteral("restart")}) {
        auto *button = new QPushButton(action.left(1).toUpper() + action.mid(1), this);
        connect(button, &QPushButton::clicked, this, [this, action]() { runAction(action); });
        actions->addWidget(button);
    }
    auto *logsButton = new QPushButton(QIcon::fromTheme(QStringLiteral("text-x-generic")), QStringLiteral("View logs"), this);
    auto *inspectButton = new QPushButton(QIcon::fromTheme(QStringLiteral("document-properties")), QStringLiteral("Inspect JSON"), this);
    actions->addWidget(logsButton);
    actions->addWidget(inspectButton);
    actions->addStretch();
    m_status = new QLabel(this);
    actions->addWidget(m_status);
    layout->addLayout(actions);

    connect(refreshButton, &QPushButton::clicked, this, &DockerPage::refresh);
    connect(logsButton, &QPushButton::clicked, this, [this]() {
        const auto row = selectedRow();
        if (!row.id.isEmpty()) m_provider.containerLogs(row.id);
    });
    connect(inspectButton, &QPushButton::clicked, this, [this]() {
        const auto row = selectedRow();
        if (!row.id.isEmpty()) m_provider.inspectContainer(row.id);
    });
    connect(&m_provider, &DockerProvider::containersReady, this, [this](const QVector<DockerContainerRow> &rows, const QString &error) {
        m_model->setRows(rows);
        m_status->setText(error.isEmpty() ? QStringLiteral("%1 containers").arg(rows.size()) : error);
    });
    connect(&m_provider, &DockerProvider::textReady, this, [this](const QString &title, const QString &text) {
        auto *dialog = new LogViewerDialog(title, text, this);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->show();
    });
    connect(&m_provider, &DockerProvider::actionFinished, this, [this](const QString &message, const QString &details) {
        QMessageBox::information(this, QStringLiteral("Docker"), details.isEmpty() ? message : message + QStringLiteral("\n\n") + details);
        refresh();
    });

    refresh();
}

DockerContainerRow DockerPage::selectedRow() const
{
    const QModelIndex index = m_table->currentIndex();
    if (!index.isValid()) return {};
    return m_model->rowAt(index.row());
}

void DockerPage::runAction(const QString &action)
{
    const auto row = selectedRow();
    if (row.id.isEmpty()) return;
    if (ConfirmActionDialog::confirm(this, QStringLiteral("Confirm Docker action"), QStringLiteral("Run docker %1 %2?").arg(action, row.name))) {
        m_provider.controlContainer(row.id, action);
    }
}

void DockerPage::refresh()
{
    m_status->setText(QStringLiteral("Refreshing..."));
    m_provider.refreshContainers();
}
