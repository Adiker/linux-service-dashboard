#include "MountsPage.h"

#include "../core/MountProfileStore.h"
#include "../utils/TableLayoutPersistence.h"
#include "../utils/TableTools.h"
#include "ConfirmActionDialog.h"

#include <QDesktopServices>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTableView>
#include <QUrl>
#include <QVBoxLayout>

MountsPage::MountsPage(MountProvider& provider, QWidget* parent) : QWidget(parent), m_provider(provider) {
    auto* layout = new QVBoxLayout(this);
    auto* header = new QHBoxLayout;
    auto* title = new QLabel(tr("Mounts"), this);
    title->setObjectName(QStringLiteral("pageTitle"));
    auto* refreshButton = new QPushButton(QIcon::fromTheme(QStringLiteral("view-refresh")), tr("Refresh"), this);
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
    m_table->verticalHeader()->hide();
    m_table->setColumnWidth(0, 250);
    TableLayoutPersistence::bind(m_table, QStringLiteral("tables/mounts/headerState"));
    layout->addWidget(m_table, 1);

    auto* actions = new QHBoxLayout;
    auto* openButton = new QPushButton(QIcon::fromTheme(QStringLiteral("folder-open")), tr("Open path"), this);
    auto* unmountButton = new QPushButton(tr("Unmount"), this);
    auto* saveProfileButton = new QPushButton(tr("Save profile"), this);
    actions->addWidget(openButton);
    actions->addWidget(unmountButton);
    actions->addWidget(saveProfileButton);
    actions->addStretch();
    m_status = new QLabel(this);
    m_status->setTextFormat(Qt::PlainText);
    m_status->setWordWrap(true);
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
        if (row.target.isEmpty()) {
            return;
        }
        if (row.status != QStringLiteral("Mounted")) {
            // Configured (fstab/profile) rows are not currently mounted; refuse to
            // run umount against them.
            m_status->setText(tr("%1 is not mounted.").arg(row.target));
            return;
        }
        if (ConfirmActionDialog::confirm(this, tr("Confirm unmount"), tr("Unmount %1?").arg(row.target))) {
            m_provider.unmount(row.target);
        }
    });
    connect(saveProfileButton, &QPushButton::clicked, this, [this]() {
        const MountRow row = selectedRow();
        if (row.target.isEmpty()) {
            m_status->setText(tr("Select a mount to save as profile."));
            return;
        }
        bool ok = false;
        const QString name =
            QInputDialog::getText(this, tr("Save mount profile"), tr("Profile name"), QLineEdit::Normal, row.target, &ok).trimmed();
        if (!ok || name.isEmpty()) {
            return;
        }
        MountProfileStore::saveProfile(row, name);
        m_status->setText(tr("Saved profile %1.").arg(name));
        refresh();
    });
    connect(&m_provider, &MountProvider::mountsReady, this, [this](const QVector<MountRow>& rows, const QString& error) {
        if (error.isEmpty())
            m_model->setRows(rows);
        m_status->setText(error.isEmpty() ? tr("%1 mounts").arg(rows.size()) : error);
    });
    connect(&m_provider, &MountProvider::actionFinished, this, [this](const QString& message, const QString& details) {
        QMessageBox::information(this, tr("Mounts"), details.isEmpty() ? message : message + QStringLiteral("\n\n") + details);
        refresh();
    });
    TableTools::install(m_table, layout, true);
}

MountRow MountsPage::selectedRow() const {
    const QModelIndex index = m_table->currentIndex();
    if (!index.isValid())
        return {};
    return m_model->rowAt(TableTools::sourceRow(m_table));
}

void MountsPage::refresh() {
    m_status->setText(tr("Refreshing..."));
    m_provider.refreshMounts(true);
}
