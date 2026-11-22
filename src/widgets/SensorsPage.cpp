#include "SensorsPage.h"

#include "../models/TableFilterProxyModel.h"
#include "../ui/StatusBadgeDelegate.h"
#include "../ui/ThemeManager.h"
#include "../utils/TableLayoutPersistence.h"
#include "StatusBanner.h"

#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPushButton>
#include <QTableView>
#include <QVBoxLayout>

SensorsPage::SensorsPage(SensorProvider *provider, QWidget *parent)
    : QWidget(parent)
    , m_provider(provider)
{
    auto *layout = new QVBoxLayout(this);
    auto *header = new QHBoxLayout;
    auto *title = new QLabel(QStringLiteral("Sensors"), this);
    title->setObjectName(QStringLiteral("pageTitle"));
    m_filter = new QLineEdit(this);
    m_filter->setPlaceholderText(QStringLiteral("Filter sensors..."));
    m_filter->setClearButtonEnabled(true);
    m_stateFilter = new QComboBox(this);
    m_stateFilter->addItems({QStringLiteral("All"), QStringLiteral("Warning / Critical")});
    auto *refreshButton = new QPushButton(ThemeManager::themedIcon(QStringLiteral("view-refresh"), QStyle::SP_BrowserReload), QStringLiteral("Refresh"), this);
    header->addWidget(title);
    header->addStretch();
    header->addWidget(m_filter, 1);
    header->addWidget(m_stateFilter);
    header->addWidget(refreshButton);
    layout->addLayout(header);

    m_model = new SensorModel(this);
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
    m_table->setItemDelegateForColumn(5, new StatusBadgeDelegate(m_table));
    TableLayoutPersistence::bind(m_table, QStringLiteral("tables/sensors/headerState"), /*persistSortOrder=*/true);
    layout->addWidget(m_table, 1);

    m_status = new StatusBanner(this);
    layout->addWidget(m_status);

    connect(m_filter, &QLineEdit::textChanged, m_proxy, &TableFilterProxyModel::setSearchText);
    connect(m_stateFilter, &QComboBox::currentTextChanged, this, [this](const QString &state) {
        if (state == QStringLiteral("All")) {
            m_proxy->setRowPredicate({});
        } else {
            m_proxy->setRowPredicate([](int sourceRow, const QAbstractItemModel *model) {
                const QString status = model->index(sourceRow, 5).data(Qt::DisplayRole).toString();
                return status == QStringLiteral("Warning") || status == QStringLiteral("Critical");
            });
        }
    });
    connect(refreshButton, &QPushButton::clicked, this, &SensorsPage::refresh);
    connect(m_table, &QWidget::customContextMenuRequested, this, &SensorsPage::showContextMenu);
    connect(m_provider, &SensorProvider::sensorsReady, this, [this](const QVector<SensorRow> &rows, const QString &error) {
        m_model->setRows(rows);
        m_status->setResultMessage(QStringLiteral("%1 readings").arg(rows.size()), error);
    });
}

void SensorsPage::showContextMenu(const QPoint &pos)
{
    const QModelIndex index = m_table->indexAt(pos);
    if (!index.isValid()) {
        return;
    }
    m_table->selectRow(index.row());
    const int sourceRow = m_proxy->mapToSource(index).row();
    const QVector<SensorRow> rows = m_model->rows();
    if (sourceRow < 0 || sourceRow >= rows.size()) {
        return;
    }
    const SensorRow &row = rows.at(sourceRow);

    QMenu menu(this);
    menu.addAction(QStringLiteral("Copy sensor label"), this, [chip = row.chip, label = row.label]() {
        if (QClipboard *cb = QApplication::clipboard()) {
            cb->setText(QStringLiteral("%1 / %2").arg(chip, label));
        }
    });
    menu.addAction(QStringLiteral("Copy reading"), this, [row]() {
        if (QClipboard *cb = QApplication::clipboard()) {
            cb->setText(QStringLiteral("%1 %2: %3 (%4)").arg(row.chip, row.label).arg(row.current, 0, 'f', 1).arg(row.status));
        }
    });
    menu.exec(m_table->viewport()->mapToGlobal(pos));
}

void SensorsPage::refresh()
{
    m_status->setMessage(QStringLiteral("Refreshing..."), StatusBanner::Severity::Info);
    m_provider->refreshSensors();
}
