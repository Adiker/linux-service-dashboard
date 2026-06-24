#include "SettingsPage.h"

#include "../core/ServiceGroupSettings.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLineEdit>
#include <QLabel>
#include <QMessageBox>
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
    auto *groupRow = new QHBoxLayout;
    m_serviceGroup = new QComboBox(servicesGroup);
    auto *addGroupButton = new QPushButton(QStringLiteral("Add group"), servicesGroup);
    auto *renameGroupButton = new QPushButton(QStringLiteral("Rename"), servicesGroup);
    groupRow->addWidget(new QLabel(QStringLiteral("Group"), servicesGroup));
    groupRow->addWidget(m_serviceGroup, 1);
    groupRow->addWidget(addGroupButton);
    groupRow->addWidget(renameGroupButton);
    servicesLayout->addLayout(groupRow);
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
    connect(m_serviceGroup, &QComboBox::currentTextChanged, this, &SettingsPage::loadGroupServices);
    connect(addGroupButton, &QPushButton::clicked, this, &SettingsPage::addGroup);
    connect(renameGroupButton, &QPushButton::clicked, this, &SettingsPage::renameGroup);
    load();
}

void SettingsPage::reloadGroups()
{
    const QString current = m_serviceGroup->currentText();
    m_serviceGroup->blockSignals(true);
    m_serviceGroup->clear();
    for (const QString &group : ServiceGroupSettings::groupNames()) {
        m_serviceGroup->addItem(group);
    }
    const int index = m_serviceGroup->findText(current);
    m_serviceGroup->setCurrentIndex(index >= 0 ? index : 0);
    m_serviceGroup->blockSignals(false);
}

QStringList SettingsPage::editorServices() const
{
    QStringList services = m_services->toPlainText().split('\n', Qt::SkipEmptyParts);
    for (QString &service : services) {
        service = service.trimmed();
    }
    services.removeAll(QString());
    return services;
}

void SettingsPage::stageCurrentEditor()
{
    // Hold the visible group's edits in memory (not QSettings) so they survive a
    // selector switch but are not persisted until the user clicks Save.
    if (!m_loadedGroup.isEmpty()) {
        m_stagedGroupEdits.insert(m_loadedGroup, editorServices());
    }
}

void SettingsPage::loadGroupServices()
{
    const QString group = m_serviceGroup->currentText();
    if (group.isEmpty()) {
        return;
    }
    if (m_loadedGroup != group) {
        stageCurrentEditor();
    }
    const QStringList services = m_stagedGroupEdits.contains(group)
        ? m_stagedGroupEdits.value(group)
        : ServiceGroupSettings::servicesForGroup(group);
    m_services->setPlainText(services.join('\n'));
    m_loadedGroup = group;
}

void SettingsPage::addGroup()
{
    bool ok = false;
    const QString name = QInputDialog::getText(this, QStringLiteral("Add service group"), QStringLiteral("Group name"), QLineEdit::Normal, QString(), &ok).trimmed();
    if (!ok || name.isEmpty()) {
        return;
    }
    if (ServiceGroupSettings::groupNames().contains(name)) {
        // Writing an empty group here would clobber the existing one's unit list
        // (an empty watched list means "show all"). Just select the existing group.
        QMessageBox::warning(this, QStringLiteral("Add service group"),
                             QStringLiteral("A group named \"%1\" already exists.").arg(name));
        reloadGroups();
        m_serviceGroup->setCurrentText(name);
        loadGroupServices();
        return;
    }
    ServiceGroupSettings::setServicesForGroup(name, {});
    reloadGroups();
    m_serviceGroup->setCurrentText(name);
    loadGroupServices();
}

void SettingsPage::renameGroup()
{
    const QString current = m_serviceGroup->currentText();
    if (current.isEmpty()) {
        return;
    }
    bool ok = false;
    const QString name = QInputDialog::getText(this, QStringLiteral("Rename service group"), QStringLiteral("New name"), QLineEdit::Normal, current, &ok).trimmed();
    if (!ok || name.isEmpty() || name == current) {
        return;
    }
    if (ServiceGroupSettings::groupNames().contains(name)) {
        // Renaming onto an existing group would overwrite its unit list and leave
        // a duplicate entry in the selector. Require a free name instead.
        QMessageBox::warning(this, QStringLiteral("Rename service group"),
                             QStringLiteral("A group named \"%1\" already exists. Choose a different name.").arg(name));
        return;
    }
    ServiceGroupSettings::renameGroup(current, name);
    // Carry any staged edits over to the new name and point the tracker at it, so
    // the next selector change neither loses the edits nor resurrects the old name.
    if (m_stagedGroupEdits.contains(current)) {
        m_stagedGroupEdits.insert(name, m_stagedGroupEdits.take(current));
    }
    m_loadedGroup = name;
    reloadGroups();
    m_serviceGroup->setCurrentText(name);
}

void SettingsPage::load()
{
    QSettings settings;
    m_refreshInterval->setValue(settings.value(QStringLiteral("refresh/intervalSeconds"), 30).toInt());
    reloadGroups();
    loadGroupServices();
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
    // Flush every staged group edit now that the user committed with Save. Write the
    // currently selected group last so its list also lands in the legacy mirror.
    stageCurrentEditor();
    const QString currentGroup = m_serviceGroup->currentText();
    for (auto it = m_stagedGroupEdits.constBegin(); it != m_stagedGroupEdits.constEnd(); ++it) {
        if (it.key() != currentGroup) {
            ServiceGroupSettings::setServicesForGroup(it.key(), it.value());
        }
    }
    if (!currentGroup.isEmpty()) {
        ServiceGroupSettings::setServicesForGroup(currentGroup, editorServices());
    }
    settings.setValue(QStringLiteral("modules/systemd"), m_systemd->isChecked());
    settings.setValue(QStringLiteral("modules/docker"), m_docker->isChecked());
    settings.setValue(QStringLiteral("modules/vpn"), m_vpn->isChecked());
    settings.setValue(QStringLiteral("modules/mounts"), m_mounts->isChecked());
    settings.setValue(QStringLiteral("modules/sensors"), m_sensors->isChecked());
    settings.setValue(QStringLiteral("modules/smart"), m_smart->isChecked());
    settings.setValue(QStringLiteral("theme/preference"), m_theme->currentText());
    emit settingsChanged();
}
