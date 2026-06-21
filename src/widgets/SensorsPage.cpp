#include "SensorsPage.h"

#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTableView>
#include <QVBoxLayout>

SensorsPage::SensorsPage(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    auto *header = new QHBoxLayout;
    auto *title = new QLabel(QStringLiteral("Sensors"), this);
    title->setObjectName(QStringLiteral("pageTitle"));
    auto *refreshButton = new QPushButton(QIcon::fromTheme(QStringLiteral("view-refresh")), QStringLiteral("Refresh"), this);
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
    layout->addWidget(m_table, 1);

    m_status = new QLabel(this);
    layout->addWidget(m_status);

    connect(refreshButton, &QPushButton::clicked, this, &SensorsPage::refresh);
    connect(&m_provider, &SensorProvider::sensorsReady, this, [this](const QVector<SensorRow> &rows, const QString &error) {
        m_model->setRows(rows);
        m_status->setText(error.isEmpty() ? QStringLiteral("%1 readings").arg(rows.size()) : error);
    });
}

void SensorsPage::refresh()
{
    m_status->setText(QStringLiteral("Refreshing..."));
    m_provider.refreshSensors();
}
