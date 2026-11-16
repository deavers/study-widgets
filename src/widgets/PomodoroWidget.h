#pragma once

#include "../DataStore.h"
#include "../WidgetBase.h"

#include <QDateTime>
#include <QList>
#include <QMap>

class QLabel;
class QProgressBar;
class QPushButton;
class QTimer;
class SubjectTile;
class FlowLayout;

class PomodoroWidget : public WidgetBase
{
    Q_OBJECT

public:
    explicit PomodoroWidget(QWidget* parent = nullptr);
    ~PomodoroWidget() override;

    bool selectCategoryById(const QString& categoryId);

protected:
    void buildContextMenu(QMenu* menu) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    enum class SessionState {
        Idle,
        Running,
        Paused,
        BreakRunning
    };

    void setupUi();
    void buildSubjectTiles();
    void refreshSubjectTiles();

    void setDurationMinutes(int minutes);
    void startOrResumeSession();
    void pauseSession();
    void stopAndSaveSession(bool completed, bool isBreak = false);
    void adjustMinutes(int deltaMinutes);
    void undoLast();
    void openObsidianNotes();

    void updateTimerDisplay();
    void updateTodaySummary();
    void updateActiveSubjectDisplay();

    void checkSleepGuard();
    void saveRecoveryState();
    void restoreRecoveryState();
    void clearRecoveryState();

    QString formatDuration(int totalSeconds) const;
    QString formatHoursMinutes(int totalSeconds) const;

    // State
    SessionState m_state = SessionState::Idle;
    StudyCategory m_activeCategory;
    int m_targetDurationSeconds = 45 * 60;
    int m_remainingSeconds = 45 * 60;
    QDateTime m_sessionStartedAt;
    QDateTime m_sessionDeadline;
    int m_breakDurationSeconds = 5 * 60;
    bool m_isBreak = false;

    // Timers
    QTimer* m_tickTimer = nullptr;
    QTimer* m_recoveryTimer = nullptr;
    QDateTime m_lastTickTime;

    // UI Widgets
    QLabel* m_subjectTitleLabel = nullptr;
    QLabel* m_tierBadgeLabel = nullptr;
    QLabel* m_timerLabel = nullptr;
    QProgressBar* m_sessionProgressBar = nullptr;

    QPushButton* m_btn25 = nullptr;
    QPushButton* m_btn45 = nullptr;
    QPushButton* m_btn50 = nullptr;
    QPushButton* m_startPauseButton = nullptr;
    QPushButton* m_stopButton = nullptr;

    // Tiles
    QWidget* m_tierAContainer = nullptr;
    QWidget* m_tierBContainer = nullptr;
    QWidget* m_tierCContainer = nullptr;
    FlowLayout* m_tierALayout = nullptr;
    FlowLayout* m_tierBLayout = nullptr;
    FlowLayout* m_tierCLayout = nullptr;
    QList<SubjectTile*> m_tiles;

    // Footer stats & tools
    QLabel* m_todayStatsLabel = nullptr;
    QLabel* m_lastSessionLabel = nullptr;
    QLabel* m_tierAAdviceLabel = nullptr;
    QPushButton* m_undoButton = nullptr;
    QPushButton* m_notesButton = nullptr;
};
