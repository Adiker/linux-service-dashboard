#pragma once

#include <QDialog>
#include <QString>
#include <functional>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;

class LogViewerDialog : public QDialog {
    Q_OBJECT

  public:
    using RefreshCallback = std::function<void(int tailLines)>;

    explicit LogViewerDialog(const QString& title, const QString& text, QWidget* parent = nullptr, RefreshCallback refreshCallback = {});

    void setContent(const QString& text);

  private:
    void findNext(bool backwards = false);
    void updateMatchCount();
    void saveToFile();

    QPlainTextEdit* m_textEdit = nullptr;
    QLineEdit* m_searchEdit = nullptr;
    QLabel* m_matchLabel = nullptr;
    QCheckBox* m_wrapCheck = nullptr;
    QComboBox* m_tailCombo = nullptr;
    QPushButton* m_refreshButton = nullptr;
    RefreshCallback m_refreshCallback;
};
