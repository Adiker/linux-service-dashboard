#include "SettingsPage.h"

#include "../core/ServiceGroupSettings.h"
#include "../ui/ThemeManager.h"
#include "ConfirmActionDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QSpinBox>
#include <QStyle>
#include <QVBoxLayout>

SettingsPage::SettingsPage(QWidget* parent) : QWidget(parent) {
    auto* outerLayout = new QVBoxLayout(this);
    outerLayout->setSpacing(10);

    auto* title = new QLabel(QStringLiteral("Settings"), this);
    title->setObjectName(QStringLiteral("pageTitle"));
    outerLayout->addWidget(title);

    auto* scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    auto* scrollContent = new QWidget(scrollArea);
    auto* layout = new QVBoxLayout(scrollContent);
    layout->setContentsMargins(0, 0, 8, 8);
    layout->setSpacing(10);

    auto* generalTitle = new QLabel(QStringLiteral("Refresh"), scrollContent);
    generalTitle->setObjectName(QStringLiteral("sectionTitle"));
    layout->addWidget(generalTitle);
    auto* general = new QFrame(scrollContent);
    general->setObjectName(QStringLiteral("settingsSection"));
    auto* form = new QFormLayout(general);
    m_refreshInterval = new QSpinBox(general);
    m_refreshInterval->setRange(5, 3600);
    m_refreshInterval->setSuffix(QStringLiteral(" seconds"));
    form->addRow(QStringLiteral("Refresh interval"), m_refreshInterval);
    layout->addWidget(general);

    auto* servicesTitle = new QLabel(QStringLiteral("Watched systemd services"), scrollContent);
    servicesTitle->setObjectName(QStringLiteral("sectionTitle"));
    layout->addWidget(servicesTitle);
    auto* servicesGroup = new QFrame(scrollContent);
    servicesGroup->setObjectName(QStringLiteral("settingsSection"));
    auto* servicesLayout = new QVBoxLayout(servicesGroup);
    auto* groupRow = new QHBoxLayout;
    m_serviceGroup = new QComboBox(servicesGroup);
    auto* addGroupButton = new QPushButton(QStringLiteral("Add group"), servicesGroup);
    auto* renameGroupButton = new QPushButton(QStringLiteral("Rename"), servicesGroup);
    m_deleteGroupButton = new QPushButton(QStringLiteral("Delete"), servicesGroup);
    groupRow->addWidget(new QLabel(QStringLiteral("Group"), servicesGroup));
    groupRow->addWidget(m_serviceGroup, 1);
    groupRow->addWidget(addGroupButton);
    groupRow->addWidget(renameGroupButton);
    groupRow->addWidget(m_deleteGroupButton);
    servicesLayout->addLayout(groupRow);
    m_services = new QPlainTextEdit(servicesGroup);
    m_services->setMinimumHeight(128);
    m_services->setPlaceholderText(QStringLiteral("One unit per line"));
    servicesLayout->addWidget(m_services);
    layout->addWidget(servicesGroup);

    auto* modulesTitle = new QLabel(QStringLiteral("Modules"), scrollContent);
    modulesTitle->setObjectName(QStringLiteral("sectionTitle"));
    layout->addWidget(modulesTitle);
    auto* modules = new QFrame(scrollContent);
    modules->setObjectName(QStringLiteral("settingsSection"));
    auto* modulesLayout = new QVBoxLayout(modules);
    m_systemd = new QCheckBox(QStringLiteral("Systemd"), modules);
    m_docker = new QCheckBox(QStringLiteral("Docker"), modules);
    m_vpn = new QCheckBox(QStringLiteral("VPN"), modules);
    m_mounts = new QCheckBox(QStringLiteral("Mounts"), modules);
    m_sensors = new QCheckBox(QStringLiteral("Sensors"), modules);
    m_smart = new QCheckBox(QStringLiteral("SMART"), modules);
    for (QCheckBox* box : {m_systemd, m_docker, m_vpn, m_mounts, m_sensors, m_smart}) {
        modulesLayout->addWidget(box);
    }
    layout->addWidget(modules);

    auto* smartTitle = new QLabel(QStringLiteral("SMART scheduling"), scrollContent);
    smartTitle->setObjectName(QStringLiteral("sectionTitle"));
    layout->addWidget(smartTitle);
    auto* smartGroup = new QFrame(scrollContent);
    smartGroup->setObjectName(QStringLiteral("settingsSection"));
    auto* smartLayout = new QFormLayout(smartGroup);
    m_smartScheduled = new QCheckBox(QStringLiteral("Enable scheduled SMART refresh"), smartGroup);
    smartLayout->addRow(m_smartScheduled);
    m_smartIntervalMinutes = new QSpinBox(smartGroup);
    m_smartIntervalMinutes->setRange(5, 1440);
    m_smartIntervalMinutes->setSuffix(QStringLiteral(" minutes"));
    smartLayout->addRow(QStringLiteral("SMART interval"), m_smartIntervalMinutes);
    layout->addWidget(smartGroup);

    auto* windowTitle = new QLabel(QStringLiteral("Window & notifications"), scrollContent);
    windowTitle->setObjectName(QStringLiteral("sectionTitle"));
    layout->addWidget(windowTitle);
    auto* windowGroup = new QFrame(scrollContent);
    windowGroup->setObjectName(QStringLiteral("settingsSection"));
    auto* windowLayout = new QVBoxLayout(windowGroup);
    m_notificationsEnabled =
        new QCheckBox(QStringLiteral("Enable desktop tray notifications (failed services, high sensors, SMART, VPN)"), windowGroup);
    m_closeToTray = new QCheckBox(QStringLiteral("Close window to system tray instead of quitting"), windowGroup);
    m_startMinimizedToTray = new QCheckBox(QStringLiteral("Start minimized to system tray"), windowGroup);
    for (QCheckBox* box : {m_notificationsEnabled, m_closeToTray, m_startMinimizedToTray}) {
        windowLayout->addWidget(box);
    }
    layout->addWidget(windowGroup);

    auto* themeTitle = new QLabel(QStringLiteral("Theme"), scrollContent);
    themeTitle->setObjectName(QStringLiteral("sectionTitle"));
    layout->addWidget(themeTitle);
    auto* themeGroup = new QFrame(scrollContent);
    themeGroup->setObjectName(QStringLiteral("settingsSection"));
    auto* themeLayout = new QFormLayout(themeGroup);
    m_theme = new QComboBox(themeGroup);
    m_theme->addItems({QStringLiteral("System"), QStringLiteral("Light"), QStringLiteral("Dark"), QStringLiteral("OLED")});
    themeLayout->addRow(QStringLiteral("Preference"), m_theme);
    layout->addWidget(themeGroup);
    layout->addStretch();

    scrollArea->setWidget(scrollContent);
    outerLayout->addWidget(scrollArea, 1);

    auto* footer = new QHBoxLayout;
    m_dirtyLabel = new QLabel(this);
    m_dirtyLabel->setObjectName(QStringLiteral("dirtyHint"));
    auto* saveButton = new QPushButton(ThemeManager::themedIcon(QStringLiteral("document-save"), QStyle::SP_DialogSaveButton),
                                       QStringLiteral("Save"), this);
    saveButton->setObjectName(QStringLiteral("primaryButton"));
    footer->addWidget(m_dirtyLabel);
    footer->addStretch();
    footer->addWidget(saveButton);
    outerLayout->addLayout(footer);

    connect(saveButton, &QPushButton::clicked, this, &SettingsPage::save);
    connect(m_serviceGroup, &QComboBox::currentTextChanged, this, &SettingsPage::loadGroupServices);
    connect(addGroupButton, &QPushButton::clicked, this, &SettingsPage::addGroup);
    connect(renameGroupButton, &QPushButton::clicked, this, &SettingsPage::renameGroup);
    connect(m_deleteGroupButton, &QPushButton::clicked, this, &SettingsPage::deleteGroup);

    connect(m_refreshInterval, &QSpinBox::valueChanged, this, &SettingsPage::markDirty);
    connect(m_services, &QPlainTextEdit::textChanged, this, &SettingsPage::markDirty);
    connect(m_smartIntervalMinutes, &QSpinBox::valueChanged, this, &SettingsPage::markDirty);
    connect(m_theme, &QComboBox::currentTextChanged, this, &SettingsPage::markDirty);
    for (QCheckBox* box : {m_systemd, m_docker, m_vpn, m_mounts, m_sensors, m_smart, m_smartScheduled, m_notificationsEnabled,
                           m_closeToTray, m_startMinimizedToTray}) {
        connect(box, &QCheckBox::toggled, this, &SettingsPage::markDirty);
    }

    load();
}

void SettingsPage::markDirty() {
    if (!m_loading && m_dirtyLabel) {
        m_dirtyLabel->setText(QStringLiteral("Unsaved changes"));
    }
}

void SettingsPage::reloadGroups() {
    const QString current = m_serviceGroup->currentText();
    m_serviceGroup->blockSignals(true);
    m_serviceGroup->clear();
    const QStringList groups = ServiceGroupSettings::groupNames();
    for (const QString& group : groups) {
        m_serviceGroup->addItem(group);
    }
    const int index = m_serviceGroup->findText(current);
    m_serviceGroup->setCurrentIndex(index >= 0 ? index : 0);
    m_serviceGroup->blockSignals(false);
    if (m_deleteGroupButton) {
        m_deleteGroupButton->setEnabled(groups.size() > 1);
    }
}

QStringList SettingsPage::editorServices() const {
    QStringList services = m_services->toPlainText().split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (QString& service : services) {
        service = service.trimmed();
    }
    services.removeAll(QString());
    return services;
}

void SettingsPage::stageCurrentEditor() {
    if (!m_loadedGroup.isEmpty()) {
        m_stagedGroupEdits.insert(m_loadedGroup, editorServices());
    }
}

void SettingsPage::loadGroupServices() {
    const QString group = m_serviceGroup->currentText();
    if (group.isEmpty()) {
        return;
    }
    if (m_loadedGroup != group) {
        stageCurrentEditor();
    }
    const QStringList services =
        m_stagedGroupEdits.contains(group) ? m_stagedGroupEdits.value(group) : ServiceGroupSettings::servicesForGroup(group);
    const bool wasLoading = m_loading;
    m_loading = true;
    m_services->setPlainText(services.join(QLatin1Char('\n')));
    m_loading = wasLoading;
    m_loadedGroup = group;
}

void SettingsPage::addGroup() {
    bool ok = false;
    const QString name =
        QInputDialog::getText(this, QStringLiteral("Add service group"), QStringLiteral("Group name"), QLineEdit::Normal, QString(), &ok)
            .trimmed();
    if (!ok || name.isEmpty()) {
        return;
    }
    if (ServiceGroupSettings::groupNames().contains(name)) {
        QMessageBox::warning(this, QStringLiteral("Add service group"), QStringLiteral("A group named \"%1\" already exists.").arg(name));
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

void SettingsPage::renameGroup() {
    const QString current = m_serviceGroup->currentText();
    if (current.isEmpty()) {
        return;
    }
    bool ok = false;
    const QString name =
        QInputDialog::getText(this, QStringLiteral("Rename service group"), QStringLiteral("New name"), QLineEdit::Normal, current, &ok)
            .trimmed();
    if (!ok || name.isEmpty() || name == current) {
        return;
    }
    if (ServiceGroupSettings::groupNames().contains(name)) {
        QMessageBox::warning(this, QStringLiteral("Rename service group"),
                             QStringLiteral("A group named \"%1\" already exists. Choose a different name.").arg(name));
        return;
    }
    ServiceGroupSettings::renameGroup(current, name);
    if (m_stagedGroupEdits.contains(current)) {
        m_stagedGroupEdits.insert(name, m_stagedGroupEdits.take(current));
    }
    m_loadedGroup = name;
    reloadGroups();
    m_serviceGroup->setCurrentText(name);
}

void SettingsPage::deleteGroup() {
    const QString current = m_serviceGroup->currentText();
    if (current.isEmpty() || ServiceGroupSettings::groupNames().size() <= 1) {
        return;
    }
    if (!ConfirmActionDialog::confirm(this, QStringLiteral("Delete service group"),
                                      QStringLiteral("Delete systemd service group \"%1\"?").arg(current))) {
        return;
    }
    if (ServiceGroupSettings::deleteGroup(current)) {
        m_stagedGroupEdits.remove(current);
        m_loadedGroup.clear();
        reloadGroups();
        loadGroupServices();
        emit settingsChanged();
    }
}

void SettingsPage::load() {
    m_loading = true;
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
    m_smartScheduled->setChecked(settings.value(QStringLiteral("smart/scheduledRefreshEnabled"), false).toBool());
    m_smartIntervalMinutes->setValue(settings.value(QStringLiteral("smart/scheduledRefreshMinutes"), 30).toInt());
    m_notificationsEnabled->setChecked(settings.value(QStringLiteral("notifications/enabled"), true).toBool());
    m_closeToTray->setChecked(settings.value(QStringLiteral("window/closeToTray"), true).toBool());
    m_startMinimizedToTray->setChecked(settings.value(QStringLiteral("window/startMinimizedToTray"), false).toBool());
    m_theme->setCurrentText(settings.value(QStringLiteral("theme/preference"), QStringLiteral("System")).toString());
    m_loading = false;
    m_dirtyLabel->clear();
}

void SettingsPage::save() {
    QSettings settings;
    settings.setValue(QStringLiteral("refresh/intervalSeconds"), m_refreshInterval->value());
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
    settings.setValue(QStringLiteral("smart/scheduledRefreshEnabled"), m_smartScheduled->isChecked());
    settings.setValue(QStringLiteral("smart/scheduledRefreshMinutes"), m_smartIntervalMinutes->value());
    settings.setValue(QStringLiteral("notifications/enabled"), m_notificationsEnabled->isChecked());
    settings.setValue(QStringLiteral("window/closeToTray"), m_closeToTray->isChecked());
    settings.setValue(QStringLiteral("window/startMinimizedToTray"), m_startMinimizedToTray->isChecked());
    settings.setValue(QStringLiteral("theme/preference"), m_theme->currentText());
    m_dirtyLabel->clear();
    emit settingsChanged();
}
