#pragma once

#include "../WidgetBase.h"

#include <QDateTime>

class QComboBox;
class QLabel;
class QPushButton;
class QTimer;

class PomodoroWidget : public WidgetBase 
{
public:
    explicit PomodoroWidget(QWidget* parent = nullptr);
    ~PomodoroWidget() override;

private:
    enum class SessionState 
    {
        Idle,
        Running,
        Paused
    };

    void setDurationMinutes(int minutes);

    void startOrResumeSession();
    void pauseSession();
    void stopAndSaveSession(bool completed);

    void updateTimerDisplay();
    void updateTodaySummary();
    void resetSessionUi();

    QString formatDuration(int totalSeconds) const;
    int remainingSecondsFromDeadline() const;

    QLabel* m_timeLabel = nullptr;
    QLabel* m_statusLabel = nullptr;
    QLabel* m_todayLabel = nullptr;

    QComboBox* m_categoryComboBox = nullptr;

    QPushButton* m_25MinuteButton = nullptr;
    QPushButton* m_50MinuteButton = nullptr;
    QPushButton* m_startPauseButton = nullptr;
    QPushButton* m_stopSaveButton = nullptr;

    QTimer* m_timer = nullptr;

    SessionState m_state = SessionState::Idle;

    QDateTime m_sessionStartedAt;
    QDateTime m_deadline;

    int m_durationSeconds = 25 * 60;
    int m_remainingSeconds = 25 * 60;
};