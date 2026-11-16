#pragma once

#include "../WidgetBase.h"
#include "../api/DeaveApiClient.h"
#include "../DataStore.h"

#include <QList>
#include <QString>

class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QStackedWidget;
class QVBoxLayout;

class NotesWidget : public WidgetBase
{
    Q_OBJECT

public:
    explicit NotesWidget(QWidget* parent = nullptr);
    ~NotesWidget() override;

    void setApiBaseUrl(const QString& baseUrl);
    void setApiTokenPath(const QString& path);
    void setConfirmBeforeSend(bool confirm);

    QString draftText() const;
    void setDraftText(const QString& text);

    QString selectedTarget() const;
    void setSelectedTarget(const QString& target);

    void triggerCapture();
    int dayRulesCount() const;

    void readNote(const QString& source, const QString& path);

protected:
    void buildContextMenu(QMenu* menu) override;

private:
    void setupUi();
    void setupCaptureTab(QWidget* container);
    void setupRulesTab(QWidget* container);

    void switchTab(int index);
    void setTarget(const QString& target);

    void loadDraftFromStore();
    void persistDraftToStore();

    QString resolveTokenPath() const;
    QString readTokenFromDisk(const QString& path) const;

    void executeCapture();
    void checkDaemonHealth();
    void updateDaemonStatus(bool online);

    void rebuildDayRulesList();
    void addDayRuleFromInput();
    void resetAllDayRules();

    // Tab buttons
    QPushButton* m_notesTabBtn = nullptr;
    QPushButton* m_rulesTabBtn = nullptr;
    QStackedWidget* m_stack = nullptr;

    // Tab 0 (Capture / Notes)
    QPushButton* m_inboxBtn = nullptr;
    QPushButton* m_timelogBtn = nullptr;
    QLabel* m_daemonStatusLabel = nullptr;

    QLineEdit* m_readerSourceInput = nullptr;
    QLineEdit* m_readerPathInput = nullptr;
    QPushButton* m_readBtn = nullptr;
    QLabel* m_readerStatusLabel = nullptr;

    QPlainTextEdit* m_draftEditor = nullptr;
    QPushButton* m_sendBtn = nullptr;
    QPushButton* m_clearBtn = nullptr;
    QLabel* m_captureFeedbackLabel = nullptr;

    // Tab 1 (Day Rules)
    QLineEdit* m_newRuleInput = nullptr;
    QPushButton* m_addRuleBtn = nullptr;
    QWidget* m_rulesListContainer = nullptr;
    QVBoxLayout* m_rulesListLayout = nullptr;
    QLabel* m_rulesSummaryLabel = nullptr;

    // Network & config
    DeaveApiClient* m_apiClient = nullptr;
    QString m_customTokenPath;
    QString m_cachedToken;
    QString m_currentTarget = QStringLiteral("inbox");
    bool m_confirmBeforeSend = true;
    bool m_isSending = false;
};
