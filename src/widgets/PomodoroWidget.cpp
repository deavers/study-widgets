#include "PomodoroWidget.h"

#include "../DataStore.h"
#include "../WidgetRegistry.h"

#include <QComboBox>
#include <QDate>
#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSettings>
#include <QTimer>
#include <QVBoxLayout>

namespace {

QStringList defaultCategories()
{
    return
    {
        "Free study",
        "University",
        "Cybersecurity",
        "Digital Forensics",
        "Programming",
        "English"
    };
}

QStringList loadCategories()
{
    QSettings settings;

    const QStringList savedCategories =
        settings.value("pomodoro/categories").toStringList();

    if (!savedCategories.isEmpty())
    {
        return savedCategories;
    }

    return defaultCategories();
}

}

PomodoroWidget::PomodoroWidget(QWidget* parent)
    : WidgetBase("pomodoro", parent) {
    resize(370, 335);

    setStyleSheet(R"(
        QLabel {
            background: transparent;
        }

        QPushButton {
            background-color: #313641;
            border: none;
            border-radius: 8px;
            color: #e7eaf0;
            padding: 9px 12px;
        }

        QPushButton:hover {
            background-color: #414855;
        }

        QPushButton:pressed {
            background-color: #252a33;
        }

        QPushButton:checked {
            background-color: #315f9c;
            border: 1px solid #5184c9;
        }

        QComboBox {
            background-color: #262932;
            border: 1px solid #353945;
            border-radius: 8px;
            color: #e7eaf0;
            padding: 8px 10px;
        }

        QComboBox::drop-down {
            border: none;
            width: 26px;
        }

        QComboBox QAbstractItemView {
            background-color: #262932;
            border: 1px solid #353945;
            color: #e7eaf0;
            selection-background-color: #315f9c;
        }
    )");

    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(24, 20, 24, 20);
    rootLayout->setSpacing(10);

    auto* titleLabel = new QLabel("рџЌ… Pomodoro");
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(16);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);

    m_timeLabel = new QLabel(formatDuration(m_remainingSeconds));
    m_timeLabel->setAlignment(Qt::AlignCenter);

    QFont timeFont = m_timeLabel->font();
    timeFont.setPointSize(34);
    timeFont.setBold(true);
    m_timeLabel->setFont(timeFont);

    auto* durationLayout = new QHBoxLayout();
    durationLayout->setSpacing(8);

    m_25MinuteButton = new QPushButton("25 min");
    m_50MinuteButton = new QPushButton("50 min");

    m_25MinuteButton->setCheckable(true);
    m_50MinuteButton->setCheckable(true);

    m_25MinuteButton->setChecked(true);

    durationLayout->addWidget(m_25MinuteButton);
    durationLayout->addWidget(m_50MinuteButton);

    auto* categoryLabel = new QLabel("Category");
    categoryLabel->setStyleSheet(
        "color: #9aa3b2;"
        "font-size: 11px;"
    );

    m_categoryComboBox = new QComboBox();
    m_categoryComboBox->addItems(loadCategories());

    m_startPauseButton = new QPushButton("Start");
    m_stopSaveButton = new QPushButton("Stop and save");

    m_stopSaveButton->setEnabled(false);

    m_statusLabel = new QLabel(
        "Ready for a focused session."
    );

    m_statusLabel->setStyleSheet(
        "color: #9aa3b2;"
        "font-size: 12px;"
    );

    m_statusLabel->setWordWrap(true);

    m_todayLabel = new QLabel();
    m_todayLabel->setStyleSheet(
        "color: #62d28f;"
        "font-size: 12px;"
    );

    rootLayout->addWidget(titleLabel);
    rootLayout->addWidget(m_timeLabel);
    rootLayout->addLayout(durationLayout);
    rootLayout->addSpacing(4);
    rootLayout->addWidget(categoryLabel);
    rootLayout->addWidget(m_categoryComboBox);
    rootLayout->addWidget(m_startPauseButton);
    rootLayout->addWidget(m_stopSaveButton);
    rootLayout->addStretch();
    rootLayout->addWidget(m_statusLabel);
    rootLayout->addWidget(m_todayLabel);

    m_timer = new QTimer(this);
    m_timer->setInterval(1000);
    m_timer->setTimerType(Qt::PreciseTimer);

    connect(
        m_timer,
        &QTimer::timeout,
        this,
        [this]() {
            m_remainingSeconds = remainingSecondsFromDeadline();

            updateTimerDisplay();

            if (m_remainingSeconds == 0) {
                stopAndSaveSession(true);
            }
        }
    );

    connect(
        m_25MinuteButton,
        &QPushButton::clicked,
        this,
        [this]() {
            setDurationMinutes(25);
        }
    );

    connect(
        m_50MinuteButton,
        &QPushButton::clicked,
        this,
        [this]() {
            setDurationMinutes(50);
        }
    );

    connect(
        m_startPauseButton,
        &QPushButton::clicked,
        this,
        [this]() {
            if (m_state == SessionState::Running) {
                pauseSession();
            } else {
                startOrResumeSession();
            }
        }
    );

    connect(
        m_stopSaveButton,
        &QPushButton::clicked,
        this,
        [this]() {
            stopAndSaveSession(false);
        }
    );

    updateTodaySummary();
}

PomodoroWidget::~PomodoroWidget()
{
    if (m_state != SessionState::Idle)
    {
        stopAndSaveSession(false);
    }
}

void PomodoroWidget::setDurationMinutes(int minutes)
{
    if (m_state != SessionState::Idle)
    {
        m_statusLabel->setText(
            "Finish or stop the current session before changing duration."
        );

        return;
    }

    m_durationSeconds = minutes * 60;
    m_remainingSeconds = m_durationSeconds;

    m_25MinuteButton->setChecked(minutes == 25);
    m_50MinuteButton->setChecked(minutes == 50);

    updateTimerDisplay();
}

void PomodoroWidget::startOrResumeSession()
{
    if (m_state == SessionState::Idle)
    {
        m_sessionStartedAt = QDateTime::currentDateTime();
        m_remainingSeconds = m_durationSeconds;
    }

    m_deadline = QDateTime::currentDateTime().addSecs(
        m_remainingSeconds
    );

    m_state = SessionState::Running;

    m_timer->start();

    m_startPauseButton->setText("Pause");
    m_stopSaveButton->setEnabled(true);

    m_statusLabel->setText(
        "Focus session running. Category can be changed if needed."
    );
}

void PomodoroWidget::pauseSession()
{
    if (m_state != SessionState::Running) {
        return;
    }

    m_remainingSeconds = remainingSecondsFromDeadline();

    m_timer->stop();

    m_state = SessionState::Paused;

    m_startPauseButton->setText("Resume");

    m_statusLabel->setText(
        "Session paused. Your focused time is preserved."
    );

    updateTimerDisplay();
}

void PomodoroWidget::stopAndSaveSession(bool completed)
{
    if (m_state == SessionState::Idle)
    {
        return;
    }

    if (m_state == SessionState::Running)
    {
        m_remainingSeconds = remainingSecondsFromDeadline();
    }

    m_timer->stop();

    const int studiedSeconds =
        m_durationSeconds - m_remainingSeconds;

    if (studiedSeconds <= 0)
    {
        m_statusLabel->setText(
            "No study time was recorded."
        );

        resetSessionUi();
        return;
    }

    StudySession session;

    session.category =
        m_categoryComboBox->currentText();

    session.startedAt = m_sessionStartedAt;
    session.finishedAt = QDateTime::currentDateTime();
    session.durationSeconds = studiedSeconds;
    session.completed = completed;
    session.note = QString();

    if (!DataStore::instance().addStudySession(session))
    {
        m_statusLabel->setText(
            "Could not save session: " +
            DataStore::instance().lastError()
        );

        return;
    }

    const QString outcome = completed
        ? "Completed"
        : "Saved early";

    m_statusLabel->setText(
        QString("%1: %2 in %3.")
            .arg(outcome)
            .arg(formatDuration(studiedSeconds))
            .arg(session.category)
    );

    resetSessionUi();
    updateTodaySummary();
}

void PomodoroWidget::updateTimerDisplay()
{
    m_timeLabel->setText(
        formatDuration(m_remainingSeconds)
    );
}

void PomodoroWidget::updateTodaySummary()
{
    const int totalSeconds =
        DataStore::instance().totalStudySecondsForDate(
            QDate::currentDate()
        );

    m_todayLabel->setText(
        QString("Today: %1")
            .arg(formatDuration(totalSeconds))
    );
}

void PomodoroWidget::resetSessionUi()
{
    m_timer->stop();

    m_state = SessionState::Idle;

    m_remainingSeconds = m_durationSeconds;

    m_startPauseButton->setText("Start");
    m_stopSaveButton->setEnabled(false);

    updateTimerDisplay();
}

int PomodoroWidget::remainingSecondsFromDeadline() const
{
    const qint64 millisecondsLeft =
        QDateTime::currentDateTime().msecsTo(m_deadline);

    if (millisecondsLeft <= 0)
    {
        return 0;
    }

    return static_cast<int>(
        (millisecondsLeft + 999) / 1000
    );
}

QString PomodoroWidget::formatDuration(
    int totalSeconds
) const
{
    const int minutes = totalSeconds / 60;
    const int seconds = totalSeconds % 60;

    return QString("%1:%2")
        .arg(minutes, 2, 10, QChar('0'))
        .arg(seconds, 2, 10, QChar('0'));
}

REGISTER_WIDGET(
    "pomodoro",
    "Pomodoro timer",
    PomodoroWidget
)
