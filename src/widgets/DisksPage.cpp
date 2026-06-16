#include "DisksPage.h"

#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTableView>
#include <QVBoxLayout>

DisksPage::DisksPage(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    auto *header = new QHBoxLayout;
    auto *title = new QLabel(QStringLiteral("Disks / SMART"), this);
    title->setObjectName(QStringLiteral("pageTitle"));
    auto *refreshButton = new QPushButton(QIcon::fromTheme(QStringLiteral("view-refresh")), QStringLiteral("Refresh"), this);
    header->addWidget(title);
    header->addStretch();
    header->addWidget(refreshButton);
    layout->addLayout(header);

    m_model = new DiskModel(this);
    m_table = new QTableView(this);
    m_table->setModel(m_model);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->setSortingEnabled(true);
    layout->addWidget(m_table, 1);

    auto *actions = new QHBoxLayout;
    auto *smartButton = new QPushButton(QIcon::fromTheme(QStringLiteral("drive-harddisk")), QStringLiteral("Check SMART"), this);
    actions->addWidget(smartButton);
    actions->addStretch();
    m_status = new QLabel(this);
    actions->addWidget(m_status);
    layout->addLayout(actions);

    connect(refreshButton, &QPushButton::clicked, this, &DisksPage::refresh);
    connect(smartButton, &QPushButton::clicked, this, [this]() {
        const DiskRow row = selectedRow();
        if (!row.path.isEmpty()) {
            m_status->setText(QStringLiteral("Checking %1...").arg(row.path));
            m_provider.checkSmart(row.path);
        }
    });
    connect(&m_provider, &SmartProvider::disksReady, this, [this](const QVector<DiskRow> &rows, const QString &error) {
        m_model->setRows(rows);
        m_status->setText(error.isEmpty() ? QStringLiteral("%1 disks").arg(rows.size()) : error);
    });
    connect(&m_provider, &SmartProvider::smartReady, this, [this](const QString &path, const DiskRow &smartRow, const QString &error) {
        if (error.isEmpty()) {
            m_model->updateSmart(path, smartRow);
            m_status->setText(QStringLiteral("SMART check complete for %1").arg(path));
        } else {
            m_status->setText(error);
        }
    });

    refresh();
}

DiskRow DisksPage::selectedRow() const
{
    const QModelIndex index = m_table->currentIndex();
    if (!index.isValid()) return {};
    return m_model->rowAt(index.row());
}

void DisksPage::refresh()
{
    m_status->setText(QStringLiteral("Refreshing..."));
    m_provider.refreshDisks();
}
