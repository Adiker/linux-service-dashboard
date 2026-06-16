#include "SettingsPage.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QFrame>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QVBoxLayout>

SettingsPage::SettingsPage(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setSpacing(10);
    auto *title = new QLabel(QStringLiteral("Settings"), this);
    title->setObjectName(QStringLiteral("pageTitle"));
    layout->addWidget(title);

    auto *generalTitle = new QLabel(QStringLiteral("Refresh"), this);
    generalTitle->setObjectName(QStringLiteral("sectionTitle"));
    layout->addWidget(generalTitle);
    auto *general = new QFrame(this);
    general->setObjectName(QStringLiteral("settingsSection"));
    auto *form = new QFormLayout(general);
    m_refreshInterval = new QSpinBox(general);
    m_refreshInterval->setRange(5, 3600);
    m_refreshInterval->setSuffix(QStringLiteral(" seconds"));
    form->addRow(QStringLiteral("Refresh interval"), m_refreshInterval);
    layout->addWidget(general);

    auto *servicesTitle = new QLabel(QStringLiteral("Watched systemd services"), this);
    servicesTitle->setObjectName(QStringLiteral("sectionTitle"));
    layout->addWidget(servicesTitle);
    auto *servicesGroup = new QFrame(this);
    servicesGroup->setObjectName(QStringLiteral("settingsSection"));
    auto *servicesLayout = new QVBoxLayout(servicesGroup);
    m_services = new QPlainTextEdit(servicesGroup);
    m_services->setMinimumHeight(128);
    m_services->setPlaceholderText(QStringLiteral("One unit per line"));
    servicesLayout->addWidget(m_services);
    layout->addWidget(servicesGroup);

    auto *modulesTitle = new QLabel(QStringLiteral("Modules"), this);
    modulesTitle->setObjectName(QStringLiteral("sectionTitle"));
    layout->addWidget(modulesTitle);
    auto *modules = new QFrame(this);
    modules->setObjectName(QStringLiteral("settingsSection"));
    auto *modulesLayout = new QVBoxLayout(modules);
    m_systemd = new QCheckBox(QStringLiteral("Systemd"), modules);
    m_docker = new QCheckBox(QStringLiteral("Docker"), modules);
    m_vpn = new QCheckBox(QStringLiteral("VPN"), modules);
    m_mounts = new QCheckBox(QStringLiteral("Mounts"), modules);
    m_sensors = new QCheckBox(QStringLiteral("Sensors"), modules);
    m_smart = new QCheckBox(QStringLiteral("SMART"), modules);
    for (QCheckBox *box : {m_systemd, m_docker, m_vpn, m_mounts, m_sensors, m_smart}) {
        modulesLayout->addWidget(box);
    }
    layout->addWidget(modules);

    auto *themeTitle = new QLabel(QStringLiteral("Theme"), this);
    themeTitle->setObjectName(QStringLiteral("sectionTitle"));
    layout->addWidget(themeTitle);
    auto *themeGroup = new QFrame(this);
    themeGroup->setObjectName(QStringLiteral("settingsSection"));
    auto *themeLayout = new QFormLayout(themeGroup);
    m_theme = new QComboBox(themeGroup);
    m_theme->addItems({QStringLiteral("System"), QStringLiteral("Light"), QStringLiteral("Dark"), QStringLiteral("OLED")});
    themeLayout->addRow(QStringLiteral("Preference"), m_theme);
    layout->addWidget(themeGroup);

    auto *saveButton = new QPushButton(QIcon::fromTheme(QStringLiteral("document-save")), QStringLiteral("Save"), this);
    layout->addWidget(saveButton, 0, Qt::AlignRight);
    layout->addStretch();

    connect(saveButton, &QPushButton::clicked, this, &SettingsPage::save);
    load();
}

void SettingsPage::load()
{
    QSettings settings;
    m_refreshInterval->setValue(settings.value(QStringLiteral("refresh/intervalSeconds"), 30).toInt());
    const QStringList services = settings.value(QStringLiteral("systemd/watchedServices"),
                                                QStringList{QStringLiteral("docker.service"), QStringLiteral("NetworkManager.service"), QStringLiteral("sshd.service"), QStringLiteral("postgresql.service"), QStringLiteral("redis.service")})
        .toStringList();
    m_services->setPlainText(services.join('\n'));
    m_systemd->setChecked(settings.value(QStringLiteral("modules/systemd"), true).toBool());
    m_docker->setChecked(settings.value(QStringLiteral("modules/docker"), true).toBool());
    m_vpn->setChecked(settings.value(QStringLiteral("modules/vpn"), true).toBool());
    m_mounts->setChecked(settings.value(QStringLiteral("modules/mounts"), true).toBool());
    m_sensors->setChecked(settings.value(QStringLiteral("modules/sensors"), true).toBool());
    m_smart->setChecked(settings.value(QStringLiteral("modules/smart"), true).toBool());
    m_theme->setCurrentText(settings.value(QStringLiteral("theme/preference"), QStringLiteral("System")).toString());
}

void SettingsPage::save()
{
    QSettings settings;
    settings.setValue(QStringLiteral("refresh/intervalSeconds"), m_refreshInterval->value());
    QStringList services = m_services->toPlainText().split('\n', Qt::SkipEmptyParts);
    for (QString &service : services) {
        service = service.trimmed();
    }
    services.removeAll(QString());
    settings.setValue(QStringLiteral("systemd/watchedServices"), services);
    settings.setValue(QStringLiteral("modules/systemd"), m_systemd->isChecked());
    settings.setValue(QStringLiteral("modules/docker"), m_docker->isChecked());
    settings.setValue(QStringLiteral("modules/vpn"), m_vpn->isChecked());
    settings.setValue(QStringLiteral("modules/mounts"), m_mounts->isChecked());
    settings.setValue(QStringLiteral("modules/sensors"), m_sensors->isChecked());
    settings.setValue(QStringLiteral("modules/smart"), m_smart->isChecked());
    settings.setValue(QStringLiteral("theme/preference"), m_theme->currentText());
    emit settingsChanged();
}
