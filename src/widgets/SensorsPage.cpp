#include "SensorsPage.h"

#include "../utils/TableLayoutPersistence.h"
#include "../utils/TableTools.h"

#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTableView>
#include <QVBoxLayout>

SensorsPage::SensorsPage(SensorProvider& provider, QWidget* parent) : QWidget(parent), m_provider(provider) {
    auto* layout = new QVBoxLayout(this);
    auto* header = new QHBoxLayout;
    auto* title = new QLabel(tr("Sensors"), this);
    title->setObjectName(QStringLiteral("pageTitle"));
    auto* refreshButton = new QPushButton(QIcon::fromTheme(QStringLiteral("view-refresh")), tr("Refresh"), this);
    header->addWidget(title);
    header->addStretch();
    header->addWidget(refreshButton);
    layout->addLayout(header);

    m_model = new SensorModel(this);
    m_table = new QTableView(this);
    m_table->setModel(m_model);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->verticalHeader()->hide();
    m_table->setSortingEnabled(true);
    m_table->setColumnWidth(0, 250);
    TableLayoutPersistence::bind(m_table, QStringLiteral("tables/sensors/headerState"));
    layout->addWidget(m_table, 1);

    m_status = new QLabel(this);
    m_status->setTextFormat(Qt::PlainText);
    m_status->setWordWrap(true);
    layout->addWidget(m_status);

    connect(refreshButton, &QPushButton::clicked, this, &SensorsPage::refresh);
    connect(&m_provider, &SensorProvider::sensorsReady, this, [this](const QVector<SensorRow>& rows, const QString& error) {
        if (error.isEmpty())
            m_model->setRows(rows);
        m_status->setText(error.isEmpty() ? tr("%1 readings").arg(rows.size()) : error);
    });
    TableTools::install(m_table, layout, true);
}

void SensorsPage::refresh() {
    m_status->setText(tr("Refreshing..."));
    m_provider.refreshSensors();
}
