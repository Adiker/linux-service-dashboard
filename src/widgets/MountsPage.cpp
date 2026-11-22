#include "MountsPage.h"

#include "../core/MountProfileStore.h"
#include "../models/TableFilterProxyModel.h"
#include "../ui/StatusBadgeDelegate.h"
#include "../ui/ThemeManager.h"
#include "../utils/TableLayoutPersistence.h"

#include "ConfirmActionDialog.h"
#include "StatusBanner.h"

#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QDesktopServices>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QTableView>
#include <QUrl>
#include <QVBoxLayout>

MountsPage::MountsPage(MountProvider *provider, QWidget *parent)
    : QWidget(parent)
    , m_provider(provider)
{
    auto *layout = new QVBoxLayout(this);
    auto *header = new QHBoxLayout;
    auto *title = new QLabel(QStringLiteral("Mounts"), this);
    title->setObjectName(QStringLiteral("pageTitle"));
    m_filter = new QLineEdit(this);
    m_filter->setPlaceholderText(QStringLiteral("Filter mounts..."));
    m_filter->setClearButtonEnabled(true);
    m_stateFilter = new QComboBox(this);
    m_stateFilter->addItems({QStringLiteral("All"), QStringLiteral("Mounted"), QStringLiteral("Configured")});
    auto *refreshButton = new QPushButton(ThemeManager::themedIcon(QStringLiteral("view-refresh"), QStyle::SP_BrowserReload), QStringLiteral("Refresh"), this);
    header->addWidget(title);
    header->addStretch();
    header->addWidget(m_filter, 1);
    header->addWidget(m_stateFilter);
    header->addWidget(refreshButton);
    layout->addLayout(header);

    m_model = new MountModel(this);
    m_proxy = new TableFilterProxyModel(this);
    m_proxy->setSourceModel(m_model);

    m_table = new QTableView(this);
    m_table->setModel(m_proxy);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setAlternatingRowColors(true);
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->verticalHeader()->hide();
    m_table->setSortingEnabled(true);
    m_table->setContextMenuPolicy(Qt::CustomContextMenu);
    m_table->setItemDelegateForColumn(4, new StatusBadgeDelegate(m_table));
    TableLayoutPersistence::bind(m_table, QStringLiteral("tables/mounts/headerState"), /*persistSortOrder=*/true);
    layout->addWidget(m_table, 1);

    auto *actions = new QHBoxLayout;
    m_openButton = new QPushButton(ThemeManager::themedIcon(QStringLiteral("folder-open"), QStyle::SP_DirOpenIcon), QStringLiteral("Open path"), this);
    m_mountButton = new QPushButton(QStringLiteral("Mount"), this);
    m_unmountButton = new QPushButton(QStringLiteral("Unmount"), this);
    m_saveProfileButton = new QPushButton(QStringLiteral("Save profile"), this);
    m_removeProfileButton = new QPushButton(QStringLiteral("Remove profile"), this);
    actions->addWidget(m_openButton);
    actions->addWidget(m_mountButton);
    actions->addWidget(m_unmountButton);
    actions->addWidget(m_saveProfileButton);
    actions->addWidget(m_removeProfileButton);
    actions->addStretch();
    layout->addLayout(actions);
    m_status = new StatusBanner(this);
    layout->addWidget(m_status);

    connect(m_filter, &QLineEdit::textChanged, m_proxy, &TableFilterProxyModel::setSearchText);
    connect(m_stateFilter, &QComboBox::currentTextChanged, this, [this](const QString &state) {
        if (state == QStringLiteral("All")) {
            m_proxy->setRowPredicate({});
        } else if (state == QStringLiteral("Mounted")) {
            m_proxy->setRowPredicate([](int sourceRow, const QAbstractItemModel *model) {
                return model->index(sourceRow, 4).data(Qt::DisplayRole).toString() == QStringLiteral("Mounted");
            });
        } else {
            m_proxy->setRowPredicate([](int sourceRow, const QAbstractItemModel *model) {
                return model->index(sourceRow, 4).data(Qt::DisplayRole).toString() != QStringLiteral("Mounted");
            });
        }
    });
    connect(refreshButton, &QPushButton::clicked, this, &MountsPage::refresh);
    connect(m_openButton, &QPushButton::clicked, this, [this]() {
        const MountRow row = selectedRow();
        if (!row.target.isEmpty()) {
            QDesktopServices::openUrl(QUrl::fromLocalFile(row.target));
        }
    });
    connect(m_mountButton, &QPushButton::clicked, this, [this]() {
        const MountRow row = selectedRow();
        if (row.target.isEmpty() || row.status == QStringLiteral("Mounted")) {
            return;
        }
        m_status->setMessage(QStringLiteral("Mounting %1...").arg(row.target), StatusBanner::Severity::Info);
        m_provider->mount(row.target);
    });
    connect(m_unmountButton, &QPushButton::clicked, this, [this]() {
        const MountRow row = selectedRow();
        if (row.target.isEmpty()) {
            return;
        }
        if (row.status != QStringLiteral("Mounted")) {
            m_status->setMessage(QStringLiteral("%1 is not mounted.").arg(row.target), StatusBanner::Severity::Warning);
            return;
        }
        if (ConfirmActionDialog::confirm(this, QStringLiteral("Confirm unmount"), QStringLiteral("Unmount %1?").arg(row.target))) {
            m_provider->unmount(row.target);
        }
    });
    connect(m_saveProfileButton, &QPushButton::clicked, this, [this]() {
        const MountRow row = selectedRow();
        if (row.target.isEmpty()) {
            m_status->setMessage(QStringLiteral("Select a mount to save as profile."), StatusBanner::Severity::Info);
            return;
        }
        bool ok = false;
        const QString name = QInputDialog::getText(this, QStringLiteral("Save mount profile"), QStringLiteral("Profile name"), QLineEdit::Normal, row.target, &ok).trimmed();
        if (!ok || name.isEmpty()) {
            return;
        }
        MountProfileStore::saveProfile(row, name);
        m_status->setMessage(QStringLiteral("Saved profile %1.").arg(name), StatusBanner::Severity::Ok);
        refresh();
    });
    connect(m_removeProfileButton, &QPushButton::clicked, this, [this]() {
        const MountRow row = selectedRow();
        const QString prefix = QStringLiteral("Profile: ");
        if (!row.status.startsWith(prefix)) {
            return;
        }
        const QString profileName = row.status.mid(prefix.size());
        if (ConfirmActionDialog::confirm(this, QStringLiteral("Remove mount profile"), QStringLiteral("Remove saved mount profile \"%1\"?").arg(profileName))) {
            MountProfileStore::removeProfile(profileName);
            m_status->setMessage(QStringLiteral("Removed profile %1.").arg(profileName), StatusBanner::Severity::Ok);
            refresh();
        }
    });
    connect(m_table->selectionModel(), &QItemSelectionModel::selectionChanged, this, &MountsPage::updateActionStates);
    connect(m_model, &QAbstractItemModel::modelReset, this, &MountsPage::updateActionStates);
    connect(m_table, &QTableView::doubleClicked, this, [this](const QModelIndex &) {
        const MountRow row = selectedRow();
        if (!row.target.isEmpty() && row.status == QStringLiteral("Mounted")) {
            QDesktopServices::openUrl(QUrl::fromLocalFile(row.target));
        }
    });
    connect(m_table, &QWidget::customContextMenuRequested, this, &MountsPage::showContextMenu);

    connect(m_provider, &MountProvider::mountsReady, this, [this](const QVector<MountRow> &rows, const QString &error) {
        m_model->setRows(rows);
        m_status->setResultMessage(QStringLiteral("%1 mounts").arg(rows.size()), error);
        updateActionStates();
    });
    connect(m_provider, &MountProvider::actionFinished, this, [this](const QString &message, const QString &details) {
        QMessageBox::information(this, QStringLiteral("Mounts"), details.isEmpty() ? message : message + QStringLiteral("\n\n") + details);
        refresh();
    });

    updateActionStates();
}

MountRow MountsPage::selectedRow() const
{
    const QModelIndex index = m_table->currentIndex();
    if (!index.isValid()) {
        return {};
    }
    return m_model->rowAt(m_proxy->mapToSource(index).row());
}

void MountsPage::updateActionStates()
{
    const MountRow row = selectedRow();
    const bool hasSelection = !row.target.isEmpty();
    const bool isMounted = hasSelection && row.status == QStringLiteral("Mounted");
    const bool isProfile = hasSelection && row.status.startsWith(QStringLiteral("Profile: "));
    m_openButton->setEnabled(hasSelection);
    m_mountButton->setEnabled(hasSelection && !isMounted);
    m_unmountButton->setEnabled(isMounted);
    m_saveProfileButton->setEnabled(hasSelection);
    m_removeProfileButton->setEnabled(isProfile);
}

void MountsPage::showContextMenu(const QPoint &pos)
{
    const QModelIndex index = m_table->indexAt(pos);
    if (!index.isValid()) {
        return;
    }
    m_table->selectRow(index.row());
    const MountRow row = selectedRow();
    if (row.target.isEmpty()) {
        return;
    }

    QMenu menu(this);
    menu.addAction(QIcon::fromTheme(QStringLiteral("folder-open")), QStringLiteral("Open path"), this, [target = row.target]() {
        QDesktopServices::openUrl(QUrl::fromLocalFile(target));
    });
    if (row.status == QStringLiteral("Mounted")) {
        menu.addAction(QStringLiteral("Unmount"), this, [this]() { m_unmountButton->click(); });
    } else {
        menu.addAction(QStringLiteral("Mount"), this, [this]() { m_mountButton->click(); });
    }
    menu.addAction(QStringLiteral("Save profile"), this, [this]() { m_saveProfileButton->click(); });
    if (row.status.startsWith(QStringLiteral("Profile: "))) {
        menu.addAction(QStringLiteral("Remove profile"), this, [this]() { m_removeProfileButton->click(); });
    }
    menu.addSeparator();
    menu.addAction(QStringLiteral("Copy target path"), this, [target = row.target]() {
        if (QClipboard *cb = QApplication::clipboard()) {
            cb->setText(target);
        }
    });
    menu.addAction(QStringLiteral("Copy source"), this, [source = row.source]() {
        if (QClipboard *cb = QApplication::clipboard()) {
            cb->setText(source);
        }
    });
    menu.exec(m_table->viewport()->mapToGlobal(pos));
}

void MountsPage::refresh()
{
    m_status->setMessage(QStringLiteral("Refreshing..."), StatusBanner::Severity::Info);
    m_provider->refreshMounts(true);
}
