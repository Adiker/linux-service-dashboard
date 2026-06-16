#include "MountsPage.h"

#include "ConfirmActionDialog.h"

#include <QDesktopServices>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTableView>
#include <QUrl>
#include <QVBoxLayout>

MountsPage::MountsPage(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    auto *header = new QHBoxLayout;
    auto *title = new QLabel(QStringLiteral("Mounts"), this);
    title->setObjectName(QStringLiteral("pageTitle"));
    auto *refreshButton = new QPushButton(QIcon::fromTheme(QStringLiteral("view-refresh")), QStringLiteral("Refresh"), this);
    header->addWidget(title);
    header->addStretch();
    header->addWidget(refreshButton);
    layout->addLayout(header);

    m_model = new MountModel(this);
    m_table = new QTableView(this);
    m_table->setModel(m_model);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->horizontalHeader()->setStretchLastSection(true);
    layout->addWidget(m_table, 1);

    auto *actions = new QHBoxLayout;
    auto *openButton = new QPushButton(QIcon::fromTheme(QStringLiteral("folder-open")), QStringLiteral("Open path"), this);
    auto *unmountButton = new QPushButton(QStringLiteral("Unmount"), this);
    actions->addWidget(openButton);
    actions->addWidget(unmountButton);
    actions->addStretch();
    m_status = new QLabel(this);
    actions->addWidget(m_status);
    layout->addLayout(actions);

    connect(refreshButton, &QPushButton::clicked, this, &MountsPage::refresh);
    connect(openButton, &QPushButton::clicked, this, [this]() {
        const MountRow row = selectedRow();
        if (!row.target.isEmpty()) {
            QDesktopServices::openUrl(QUrl::fromLocalFile(row.target));
        }
    });
    connect(unmountButton, &QPushButton::clicked, this, [this]() {
        const MountRow row = selectedRow();
        if (!row.target.isEmpty() && ConfirmActionDialog::confirm(this, QStringLiteral("Confirm unmount"), QStringLiteral("Unmount %1?").arg(row.target))) {
            m_provider.unmount(row.target);
        }
    });
    connect(&m_provider, &MountProvider::mountsReady, this, [this](const QVector<MountRow> &rows, const QString &error) {
        m_model->setRows(rows);
        m_status->setText(error.isEmpty() ? QStringLiteral("%1 mounts").arg(rows.size()) : error);
    });
    connect(&m_provider, &MountProvider::actionFinished, this, [this](const QString &message, const QString &details) {
        QMessageBox::information(this, QStringLiteral("Mounts"), details.isEmpty() ? message : message + QStringLiteral("\n\n") + details);
        refresh();
    });

    refresh();
}

MountRow MountsPage::selectedRow() const
{
    const QModelIndex index = m_table->currentIndex();
    if (!index.isValid()) return {};
    return m_model->rowAt(index.row());
}

void MountsPage::refresh()
{
    m_status->setText(QStringLiteral("Refreshing..."));
    m_provider.refreshMounts();
}
