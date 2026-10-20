#include "CenterPage.h"

#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSettings>
#include <QStyle>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <QVBoxLayout>
#include <algorithm>
#include <functional>

CenterPage::CenterPage(const QString& module, SystemCenter* center, QWidget* parent) : QWidget(parent), m_module(module), m_center(center) {
    auto* layout = new QVBoxLayout(this);
    auto* header = new QHBoxLayout;
    auto* title = new QLabel(module == "search" ? tr("Search results") : centerTitle(module));
    title->setObjectName("pageTitle");
    header->addWidget(title);
    header->addStretch();
    auto* refresh = new QPushButton(tr("Refresh"));
    header->addWidget(refresh);
    layout->addLayout(header);
    m_filter = new QLineEdit;
    m_filter->setPlaceholderText(tr("Search this view…"));
    layout->addWidget(m_filter);
    m_table = new QTreeWidget;
    m_table->setRootIsDecorated(true);
    m_table->setAlternatingRowColors(true);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSortingEnabled(true);
    m_table->header()->setSectionsMovable(true);
    layout->addWidget(m_table, 1);
    m_details = new QLabel;
    m_details->setWordWrap(true);
    m_details->setTextFormat(Qt::PlainText);
    m_details->setMaximumHeight(110);
    m_details->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(m_details);
    auto* actions = new QHBoxLayout;
    for (const auto& label : {tr("Pin / unpin"), tr("Open"), tr("View logs")}) {
        auto* b = new QPushButton(label);
        actions->addWidget(b);
        connect(b, &QPushButton::clicked, this, [this, label]() {
            const auto r = selected();
            if (r.id.isEmpty())
                return;
            const auto module = r.meta.value("module", m_module).toString();
            if (label == tr("Pin / unpin"))
                m_center->toggleFavorite(module, r.id);
            else if (label == tr("View logs"))
                m_center->logs(r);
            else if (m_module == "search" || m_module == "overview")
                emit navigate(module, r.id);
            else
                m_center->open(r);
        });
    }
    actions->addStretch();
    layout->addLayout(actions);
    m_status = new QLabel;
    m_status->setWordWrap(true);
    m_status->setTextFormat(Qt::PlainText);
    layout->addWidget(m_status);
    connect(refresh, &QPushButton::clicked, this, [this]() { m_center->refresh(m_module); });
    connect(m_filter, &QLineEdit::textChanged, this, &CenterPage::filter);
    connect(m_center, &SystemCenter::changed, this, [this](const QString& id) {
        if (id == m_module || m_module == "search" || m_module == "overview")
            render();
    });
    connect(m_table, &QTreeWidget::itemSelectionChanged, this, [this]() {
        const auto r = selected();
        m_details->setText(r.cells.join("\n").left(4000));
    });
    connect(m_table, &QTreeWidget::itemDoubleClicked, this, [this]() {
        const auto r = selected();
        if (!r.id.isEmpty() && (m_module == "search" || m_module == "overview"))
            emit navigate(r.meta.value("module").toString(), r.id);
    });
    connect(m_table->header(), &QHeaderView::sectionResized, this,
            [this]() { QSettings().setValue("centerTables/" + m_module, m_table->header()->saveState()); });
    render();
}
CenterRecord CenterPage::selected() const {
    auto* item = m_table->currentItem();
    const int row = item ? item->data(0, Qt::UserRole).toInt() - 1 : -1;
    return row >= 0 && row < m_data.rows.size() ? m_data.rows[row] : CenterRecord{};
}
void CenterPage::render() {
    const auto previous = selected().id;
    if (m_module == "search")
        m_data = m_center->search(property("query").toString());
    else if (m_module == "overview") {
        m_data = m_center->search("");
        m_data.rows.erase(std::remove_if(m_data.rows.begin(), m_data.rows.end(),
                                         [this](const auto& r) {
                                             return !r.attention && !m_center->isFavorite(r.meta.value("module").toString(), r.id);
                                         }),
                          m_data.rows.end());
    } else
        m_data = m_center->snapshot(m_module);
    m_table->setSortingEnabled(false);
    m_table->clear();
    m_table->setHeaderLabels(m_data.headers.isEmpty() ? QStringList{tr("Name"), tr("State"), tr("Details")} : m_data.headers);
    QHash<QString, QTreeWidgetItem*> groups;
    for (int i = 0; i < m_data.rows.size(); ++i) {
        const auto& row = m_data.rows[i];
        QTreeWidgetItem* parent = nullptr;
        QString key;
        auto groupNames = row.groups;
        if (m_module == "search" || m_module == "overview")
            groupNames = {row.cells.value(1)};
        for (const auto& group : groupNames) {
            key += "/" + group;
            if (!groups.contains(key))
                groups[key] = parent ? new QTreeWidgetItem(parent, {group}) : new QTreeWidgetItem(m_table, {group});
            parent = groups[key];
        }
        auto* item = parent ? new QTreeWidgetItem(parent, row.cells) : new QTreeWidgetItem(m_table, row.cells);
        item->setData(0, Qt::UserRole, i + 1);
        item->setIcon(0, style()->standardIcon(row.attention ? QStyle::SP_MessageBoxWarning : QStyle::SP_FileIcon));
        if (row.id == previous)
            m_table->setCurrentItem(item);
    }
    m_table->expandAll();
    m_table->setSortingEnabled(true);
    if (!property("restored").toBool() && !m_data.headers.isEmpty()) {
        m_table->header()->restoreState(QSettings().value("centerTables/" + m_module).toByteArray());
        setProperty("restored", true);
    }
    m_status->setText(
        (m_data.error.isEmpty() ? tr("%1 entries").arg(m_data.rows.size()) : tr("Unavailable / stale: %1").arg(m_data.error)) + " · " +
        tr("Last successful read: %1").arg(m_data.lastSuccess.isValid() ? m_data.lastSuccess.toString(Qt::ISODate) : tr("Never")));
    filter();
}
void CenterPage::filter() {
    const auto text = m_filter->text();
    std::function<bool(QTreeWidgetItem*)> visit = [&](auto* item) {
        bool visible = false;
        if (item->data(0, Qt::UserRole).toInt() > 0) {
            QString combined;
            for (int c = 0; c < m_table->columnCount(); ++c)
                combined += " " + item->text(c);
            visible = combined.contains(text, Qt::CaseInsensitive);
        }
        for (int i = 0; i < item->childCount(); ++i)
            visible = visit(item->child(i)) || visible;
        item->setHidden(!visible);
        return visible;
    };
    for (int i = 0; i < m_table->topLevelItemCount(); ++i)
        visit(m_table->topLevelItem(i));
}
void CenterPage::setSearch(const QString& text) {
    setProperty("query", text);
    render();
}
void CenterPage::selectRecord(const QString& id) {
    QTreeWidgetItemIterator it(m_table);
    while (*it) {
        const int i = (*it)->data(0, Qt::UserRole).toInt() - 1;
        if (i >= 0 && m_data.rows.value(i).id == id) {
            m_table->setCurrentItem(*it);
            m_table->scrollToItem(*it);
            return;
        }
        ++it;
    }
}
