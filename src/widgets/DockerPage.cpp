#include "DockerPage.h"

#include "ConfirmActionDialog.h"

#include <QMessageBox>
DockerPage::DockerPage(SystemCenter* center, QWidget* parent) : CenterPage("docker", center, parent) {
    for (const auto& action : {QString("start"), QString("stop"), QString("restart")}) {
        addActionButton(tr(action.toUtf8().constData()), [this, center, action](const auto& r) {
            if (ConfirmActionDialog::confirm(this, tr("Confirm Docker action"), tr("Run docker %1 %2?").arg(action, r.cells.value(0))))
                center->docker.controlContainer(r.id, action);
        });
    }
    addActionButton(tr("Inspect JSON"), [center](const auto& r) { center->docker.inspectContainer(r.id); });
    connect(&center->docker, &DockerProvider::actionFinished, this, [this, center](const auto& message, const auto& details) {
        QMessageBox::information(this, tr("Docker"), message + "\n" + details);
        center->refresh("docker");
    });
}
