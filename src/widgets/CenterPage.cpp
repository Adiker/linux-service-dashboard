#include "CenterPage.h"

#include "MetricChart.h"

#include <QCryptographicHash>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFrame>
#include <QGridLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSaveFile>
#include <QSettings>
#include <QSignalBlocker>
#include <QStyle>
#include <QToolButton>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <QUrl>
#include <QVBoxLayout>
#include <algorithm>
#include <functional>

CenterPage::CenterPage(const QString& module, SystemCenter* center, QWidget* parent) : QWidget(parent), m_module(module), m_center(center) {
    auto* layout = new QVBoxLayout(this);
    auto* header = new QHBoxLayout;
    auto* title = new QLabel(module == "search" ? tr("Search results") : centerTitle(module));
    title->setObjectName("pageTitle");
    title->setWordWrap(true);
    header->addWidget(title, 1);
    auto* refresh = new QPushButton(tr("Refresh"));
    header->addWidget(refresh);
    layout->addLayout(header);
    if (m_module == "overview") {
        auto* grid = new QGridLayout;
        const QStringList ids{"systemd", "docker", "backups", "smart", "vpn", "mounts"};
        for (int i = 0; i < ids.size(); ++i) {
            const auto id = ids[i];
            auto* card = new QFrame;
            card->setObjectName("card");
            auto* box = new QVBoxLayout(card);
            box->setContentsMargins(10, 8, 10, 8);
            box->setSpacing(4);
            auto* top = new QHBoxLayout;
            auto* title = new QLabel(centerTitle(id));
            title->setWordWrap(true);
            top->addWidget(title, 1);
            auto* open = new QToolButton;
            open->setText("↗");
            open->setToolTip(tr("Open"));
            top->addWidget(open);
            box->addLayout(top);
            auto* value = new QLabel(tr("Waiting for data"));
            value->setWordWrap(true);
            value->setObjectName("cardValue");
            m_cards[id] = value;
            box->addWidget(value);
            if (id == "backups" || id == "smart") {
                auto* note = new QLabel(id == "backups" ? tr("Integrity not verified") : tr("SMART: manual or opt-in schedule"));
                note->setWordWrap(true);
                note->setObjectName("cardNote");
                box->addWidget(note);
            }
            connect(open, &QToolButton::clicked, this, [this, id]() { emit navigate(id, {}); });
            grid->addWidget(card, i / 3, i % 3);
        }
        layout->addLayout(grid);
        m_activity = new QLabel;
        m_activity->setWordWrap(true);
        layout->addWidget(m_activity);
        layout->addWidget(new QLabel(tr("Pinned items and things requiring attention")));
    }
    if (m_module == "metrics") {
        m_chart = new MetricChart;
        layout->addWidget(m_chart);
    }
    m_filter = new QLineEdit;
    m_filter->setPlaceholderText(tr("Search this view…"));
    layout->addWidget(m_filter);
    m_table = new QTreeWidget;
    m_table->setObjectName("centerTable_" + module);
    setProperty("centerModule", module);
    m_table->setRootIsDecorated(true);
    m_table->setMinimumHeight(140);
    m_table->header()->setSortIndicator(0, Qt::AscendingOrder);
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
    m_actions = actions;
    for (const auto& label : {tr("Pin / unpin"), tr("Open"), tr("View logs")}) {
        auto* b = new QPushButton(label);
        actions->addWidget(b);
        if (label == tr("Pin / unpin") && m_module == "events")
            b->setEnabled(false);
        if (label == tr("Open")) {
            m_open = b;
            b->setEnabled(false);
        }
        if (label == tr("View logs")) {
            m_logs = b;
            b->setEnabled(false);
        }
        connect(b, &QPushButton::clicked, this, [this, label]() {
            const auto r = selected();
            if (r.id.isEmpty())
                return;
            const auto module = r.meta.value("module", m_module).toString();
            if (label == tr("Pin / unpin"))
                m_center->toggleFavorite(module, r.id);
            else if (label == tr("View logs"))
                m_center->logs(r);
            else if (m_module == "search" || m_module == "overview" || m_module == "events")
                emit navigate(module, r.id);
            else
                m_center->open(r);
        });
    }
    actions->addStretch();
    layout->addLayout(actions);
    m_status = new QLabel;
    m_status->setWordWrap(true);
    m_status->setMaximumHeight(90);
    m_status->setTextFormat(Qt::PlainText);
    layout->addWidget(m_status);
    connect(refresh, &QPushButton::clicked, this, [this]() { m_center->refresh(m_module); });
    connect(m_filter, &QLineEdit::textChanged, this, &CenterPage::filter);
    connect(m_center, &SystemCenter::changed, this, [this](const QString& id) {
        if (m_chart && id == "metrics" && isVisible()) {
            const auto rows = m_center->snapshot("metrics").rows;
            double cpu = -1, memory = -1;
            for (const auto& r : rows) {
                if (r.id == "cpu" && r.meta.contains("percent"))
                    cpu = r.meta.value("percent").toDouble();
                if (r.id == "memory")
                    memory = r.meta.value("percent").toDouble();
            }
            if (cpu >= 0 && memory >= 0)
                m_chart->addSample(cpu, memory);
        }
        if (isVisible() && (id == m_module || m_module == "overview" || (m_module == "search" && !property("query").toString().isEmpty())))
            render();
    });
    connect(m_table, &QTreeWidget::itemSelectionChanged, this, [this]() {
        const auto r = selected();
        m_details->setText(r.cells.join("\n").left(4000));
        const auto key = "panels/" + QString::fromLatin1(QCryptographicHash::hash(r.id.toUtf8(), QCryptographicHash::Sha256).toHex());
        if (m_open)
            m_open->setEnabled(!r.id.isEmpty() &&
                               (m_module == "overview" || m_module == "search" || m_module == "events" || r.meta.contains("desktop") ||
                                r.meta.contains("program") || r.meta.contains("path") || !QSettings().value(key).toString().isEmpty()));
        if (m_logs)
            m_logs->setEnabled(r.meta.contains("unit") || r.meta.contains("container"));
    });
    connect(m_table, &QTreeWidget::itemDoubleClicked, this, [this]() {
        const auto r = selected();
        if (!r.id.isEmpty() && (m_module == "search" || m_module == "overview" || m_module == "events"))
            emit navigate(r.meta.value("module").toString(), r.id);
    });
    connect(m_table->header(), &QHeaderView::sectionMoved, this, [this]() {
        if (property("restored").toBool())
            QSettings().setValue("centerTables/" + m_module, m_table->header()->saveState());
    });
    connect(m_table->header(), &QHeaderView::sortIndicatorChanged, this, [this]() {
        if (property("restored").toBool())
            QSettings().setValue("centerTables/" + m_module, m_table->header()->saveState());
    });
    connect(m_table->header(), &QHeaderView::sectionResized, this, [this]() {
        if (property("restored").toBool())
            QSettings().setValue("centerTables/" + m_module, m_table->header()->saveState());
    });
    m_table->header()->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_table->header(), &QHeaderView::customContextMenuRequested, this, [this](QPoint pos) {
        QMenu menu;
        for (int c = 0; c < m_table->columnCount(); ++c) {
            auto* action = menu.addAction(m_table->headerItem()->text(c));
            action->setCheckable(true);
            action->setChecked(!m_table->isColumnHidden(c));
            connect(action, &QAction::triggered, this, [this, c](bool show) {
                int visible = 0;
                for (int j = 0; j < m_table->columnCount(); ++j)
                    if (!m_table->isColumnHidden(j))
                        ++visible;
                if (show || visible > 1)
                    m_table->setColumnHidden(c, !show);
                if (property("restored").toBool())
                    QSettings().setValue("centerTables/" + m_module, m_table->header()->saveState());
            });
        }
        menu.exec(m_table->header()->mapToGlobal(pos));
    });
    if (m_module == "servers" || m_module == "sync")
        addActionButton(tr("Configure panel address"), [this](const CenterRecord& r) {
            const auto key = "panels/" + QString::fromLatin1(QCryptographicHash::hash(r.id.toUtf8(), QCryptographicHash::Sha256).toHex());
            bool ok = false;
            const auto text =
                QInputDialog::getText(this, tr("Panel address"), tr("HTTP(S) address without credentials or query parameters"),
                                      QLineEdit::Normal, QSettings().value(key).toString(), &ok);
            if (!ok)
                return;
            const QUrl url(text);
            if (!text.isEmpty() && (!url.isValid() || url.host().isEmpty() || !QStringList{"http", "https"}.contains(url.scheme()) ||
                                    !url.userInfo().isEmpty() || url.hasQuery() || url.hasFragment())) {
                QMessageBox::warning(this, tr("Panel address"), tr("Invalid panel address"));
                return;
            }
            QSettings().setValue(key, text);
            render();
        });
    if (m_module == "snapshots") {
        auto* pick = new QPushButton(tr("Select repository configuration"));
        layout->insertWidget(1, pick);
        connect(pick, &QPushButton::clicked, this, [this]() {
            const auto path = QFileDialog::getOpenFileName(this, tr("Existing Kopia configuration"), {},
                                                           tr("Configuration files (*.config *.json);;All files (*)"));
            if (!path.isEmpty()) {
                QSettings().setValue("backups/kopiaConfig", path);
                m_center->refresh("snapshots");
            }
        });
    }
    if (m_module == "events") {
        auto* report = new QPushButton(tr("Diagnostic report…"));
        layout->insertWidget(1, report);
        connect(report, &QPushButton::clicked, this, &CenterPage::showReport);
    }
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
        m_data = m_center->overview();
        for (auto it = m_cards.cbegin(); it != m_cards.cend(); ++it) {
            const auto snapshot = m_center->snapshot(it.key());
            const bool enabled = centerModuleEnabled(it.key());
            it.value()->parentWidget()->setVisible(enabled);
            QString value = !snapshot.error.isEmpty()         ? tr("Unavailable / stale")
                            : !snapshot.lastSuccess.isValid() ? tr("Waiting for data")
                                                              : QString::number(snapshot.rows.size());
            it.value()->setText(value);
        }
        m_activity->setText(tr("Active collections: %1 · Items requiring attention: %2")
                                .arg(m_center->activeCollections())
                                .arg(m_center->attentionCount()));
    } else
        m_data = m_center->snapshot(m_module);
    const QSignalBlocker headerSignals(m_table->header());
    m_table->setSortingEnabled(false);
    m_table->clear();
    m_table->setHeaderLabels(m_data.headers.isEmpty() ? QStringList{tr("Name"), tr("State"), tr("Details")} : m_data.headers);
    QHash<QString, QTreeWidgetItem*> groups;
    for (int i = 0; i < m_data.rows.size(); ++i) {
        const auto& row = m_data.rows[i];
        QTreeWidgetItem* parent = nullptr;
        QString key;
        auto groupNames = row.groups;
        if (m_module == "search")
            groupNames = {row.cells.value(1)};
        for (const auto& group : groupNames) {
            key += "/" + group;
            if (!groups.contains(key))
                groups[key] = parent ? new QTreeWidgetItem(parent, {group}) : new QTreeWidgetItem(m_table, {group});
            parent = groups[key];
        }
        auto* item = parent ? new QTreeWidgetItem(parent, row.cells) : new QTreeWidgetItem(m_table, row.cells);
        item->setData(0, Qt::UserRole, i + 1);
        item->setIcon(0, row.meta.contains("icon")
                             ? QIcon::fromTheme(row.meta.value("icon").toString(), style()->standardIcon(QStyle::SP_FileIcon))
                             : style()->standardIcon(row.attention ? QStyle::SP_MessageBoxWarning : QStyle::SP_FileIcon));
        if (row.id == previous)
            m_table->setCurrentItem(item);
    }
    m_table->expandAll();
    m_table->setSortingEnabled(true);
    if (!property("restored").toBool() && !m_data.headers.isEmpty()) {
        for (int c = 0; c < m_table->columnCount(); ++c)
            m_table->setColumnWidth(c, c == 0 ? 240 : 165);
        if (m_module == "overview" || m_module == "search")
            m_table->setColumnWidth(2, 480);
        if (m_module == "apps") {
            m_table->setColumnWidth(3, 320);
            m_table->setColumnWidth(4, 220);
        }
        m_table->header()->restoreState(QSettings().value("centerTables/" + m_module).toByteArray());
        setProperty("restored", true);
    }
    m_status->setText(
        (m_data.error.isEmpty() ? tr("%1 entries").arg(m_data.rows.size()) : tr("Unavailable / stale: %1").arg(m_data.error.left(800))) +
        " · " + tr("Last successful read: %1").arg(m_data.lastSuccess.isValid() ? m_data.lastSuccess.toString(Qt::ISODate) : tr("Never")));
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

void CenterPage::addActionButton(const QString& label, std::function<void(const CenterRecord&)> action) {
    if (!m_extraActions) {
        m_extraActions = new QHBoxLayout;
        m_extraActions->addStretch();
        auto* box = qobject_cast<QVBoxLayout*>(layout());
        box->insertLayout(box->count() - 2, m_extraActions);
    }
    auto* button = new QPushButton(label);
    m_extraActions->insertWidget(0, button);
    connect(button, &QPushButton::clicked, this, [this, action]() {
        const auto r = selected();
        if (!r.id.isEmpty())
            action(r);
    });
}

void CenterPage::showReport() {
    auto* dialog = new QDialog(this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowTitle(tr("Diagnostic report"));
    dialog->resize(760, 580);
    auto* layout = new QVBoxLayout(dialog);
    auto* note =
        new QLabel(tr("Preview before saving. Command output, settings values, paths, account names and network addresses are excluded."));
    note->setWordWrap(true);
    layout->addWidget(note);
    auto* text = new QPlainTextEdit(QString::fromUtf8(m_center->diagnosticReport().toJson()));
    text->setReadOnly(true);
    layout->addWidget(text);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Close);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::close);
    connect(buttons->button(QDialogButtonBox::Save), &QPushButton::clicked, dialog, [this, dialog, text]() {
        const auto path = QFileDialog::getSaveFileName(dialog, tr("Save diagnostic report"), "system-center-report.json", "JSON (*.json)");
        if (path.isEmpty())
            return;
        QSaveFile file(path);
        if (!file.open(QIODevice::WriteOnly)) {
            QMessageBox::warning(dialog, tr("Save report"), file.errorString());
            return;
        }
        file.setPermissions(QFile::ReadOwner | QFile::WriteOwner);
        file.write(text->toPlainText().toUtf8());
        if (!file.commit())
            QMessageBox::warning(dialog, tr("Save report"), file.errorString());
    });
    dialog->show();
}
