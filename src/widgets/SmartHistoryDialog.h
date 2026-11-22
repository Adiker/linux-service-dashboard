#pragma once

#include <QDialog>
#include <QString>

class QTableWidget;

class SmartHistoryDialog : public QDialog {
    Q_OBJECT

public:
    explicit SmartHistoryDialog(const QString &serialKey,
                                const QString &devicePath,
                                QWidget *parent = nullptr);

private:
    void reload();
    void exportCsv();
    void clearHistory();

    QString m_serialKey;
    QString m_devicePath;
    QTableWidget *m_table = nullptr;
};
