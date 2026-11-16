#include "PomodoroWidget.h"

#include "../AlarmPlayer.h"
#include "../CategoryConfigLoader.h"
#include "../GlobalHotkey.h"
#include "../Theme.h"
#include "../TimeLogWriter.h"
#include "../WidgetRegistry.h"
#include "../ui/FlowLayout.h"
#include "../ui/SubjectTile.h"
#include "../ui/WidgetChrome.h"

#include <QDesktopServices>
#include <QFrame>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QMenu>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>
#include <QVBoxLayout>

PomodoroWidget::PomodoroWidget(QWidget* parent)
    : WidgetBase(QStringLiteral("pomodoro"), parent)
    , m_tickTimer(new QTimer(this))
    , m_recoveryTimer(new QTimer(this))
{
    setBaseWidgetSize(390, 506);
    setChromeTitle(QStringLiteral("Фокус"), QStringLiteral("45:00"));
    setRootMargins(12, 10, 12, 10);

    setupUi();
    buildSubjectTiles();

    // Default active category
    auto categories = DataStore::instance().studyCategories(false);
    if (!categories.isEmpty()) {
        m_activeCategory = categories.first();
        updateActiveSubjectDisplay();
    }

    connect(m_tickTimer, &QTimer::timeout, this, [this]() {
        checkSleepGuard();
        if (m_state == SessionState::Running || m_state == SessionState::BreakRunning) {
            if (m_remainingSeconds > 0) {
                m_remainingSeconds--;
                updateTimerDisplay();
            } else {
                // Completed!
                const bool wasBreak = (m_state == SessionState::BreakRunning);
                AlarmPlayer::instance().playAlarm();
                stopAndSaveSession(true, wasBreak);

                if (!wasBreak) {
                    // Start break mode automatically or offer break
                    m_state = SessionState::BreakRunning;
                    m_isBreak = true;
                    m_targetDurationSeconds = m_breakDurationSeconds;
                    m_remainingSeconds = m_breakDurationSeconds;
                    m_sessionStartedAt = QDateTime::currentDateTime();
                    m_sessionDeadline = m_sessionStartedAt.addSecs(m_remainingSeconds);
                    updateTimerDisplay();
                    updateActiveSubjectDisplay();
                } else {
                    m_state = SessionState::Idle;
                    m_isBreak = false;
                    setDurationMinutes(45);
                }
            }
        }
    });

    connect(m_recoveryTimer, &QTimer::timeout, this, &PomodoroWidget::saveRecoveryState);
    m_recoveryTimer->start(15000); // every 15s

    // Connect Global Hotkey Ctrl+Alt+P
    connect(&GlobalHotkey::instance(), &GlobalHotkey::hotkeyTriggered, this, &PomodoroWidget::startOrResumeSession);
    GlobalHotkey::instance().registerHotkey('P', true, true);

    restoreRecoveryState();
    updateTodaySummary();
    updateTimerDisplay();
}

PomodoroWidget::~PomodoroWidget()
{
    saveRecoveryState();
}

void PomodoroWidget::setupUi()
{
    auto* root = bodyLayout();
    root->setSpacing(8);
    root->setContentsMargins(0, 0, 0, 0);

    // ==========================================
    // 1. FOCUS SESSION CARD (Header / Timer / Controls)
    // ==========================================
    auto* focusCard = new QFrame(this);
    focusCard->setObjectName("focusCard");
    focusCard->setStyleSheet(QStringLiteral(
        "QFrame#focusCard {"
        "  background: %1;"
        "  border: 1px solid %2;"
        "  border-radius: %3px;"
        "}"
    ).arg(Theme::surface1().name(QColor::HexArgb),
          Theme::stroke().name(QColor::HexArgb),
          QString::number(Theme::radiusCard())));

    auto* cardLayout = new QVBoxLayout(focusCard);
    cardLayout->setContentsMargins(12, 10, 12, 10);
    cardLayout->setSpacing(6);

    // Headline: Active Subject & Tier Badge
    auto* headRow = new QHBoxLayout();
    headRow->setSpacing(8);

    m_subjectTitleLabel = new QLabel(QStringLiteral("Free Study"), focusCard);
    m_subjectTitleLabel->setFont(Theme::fontUi(12, QFont::DemiBold));
    m_subjectTitleLabel->setStyleSheet(QStringLiteral("color: %1;").arg(Theme::textColor().name()));
    headRow->addWidget(m_subjectTitleLabel, 1);

    m_tierBadgeLabel = new QLabel(QStringLiteral("TIER F"), focusCard);
    m_tierBadgeLabel->setFont(Theme::fontMono(9, QFont::Bold));
    m_tierBadgeLabel->setStyleSheet(QStringLiteral(
        "color: %1; background: %2; border: 1px solid %3; border-radius: 4px; padding: 1px 6px;"
    ).arg(Theme::accentColor().name(),
          Theme::accentTint(30).name(QColor::HexArgb),
          Theme::accentColor().name()));
    headRow->addWidget(m_tierBadgeLabel);
    cardLayout->addLayout(headRow);

    // Large Timer
    m_timerLabel = new QLabel(QStringLiteral("45:00"), focusCard);
    m_timerLabel->setAlignment(Qt::AlignCenter);
    m_timerLabel->setFont(Theme::fontMono(36, QFont::Bold));
    m_timerLabel->setStyleSheet(QStringLiteral("color: %1; letter-spacing: 1px;").arg(Theme::textColor().name()));
    cardLayout->addWidget(m_timerLabel);

    // Progress Bar
    m_sessionProgressBar = new QProgressBar(focusCard);
    m_sessionProgressBar->setFixedHeight(4);
    m_sessionProgressBar->setTextVisible(false);
    m_sessionProgressBar->setRange(0, 100);
    m_sessionProgressBar->setValue(100);
    m_sessionProgressBar->setStyleSheet(QStringLiteral(
        "QProgressBar {"
        "  background: %1;"
        "  border: none;"
        "  border-radius: 2px;"
        "}"
        "QProgressBar::chunk {"
        "  background: %2;"
        "  border-radius: 2px;"
        "}"
    ).arg(Theme::surface2().name(QColor::HexArgb), Theme::accentColor().name()));
    cardLayout->addWidget(m_sessionProgressBar);

    // Presets & Control Buttons Row
    auto* ctrlRow = new QHBoxLayout();
    ctrlRow->setSpacing(6);

    m_btn25 = new QPushButton(QStringLiteral("25m"), focusCard);
    m_btn45 = new QPushButton(QStringLiteral("45m ⚡"), focusCard);
    m_btn50 = new QPushButton(QStringLiteral("50m"), focusCard);

    for (QPushButton* btn : {m_btn25, m_btn45, m_btn50}) {
        btn->setFont(Theme::fontMono(10, QFont::DemiBold));
        btn->setFixedHeight(28);
        btn->setCursor(Qt::PointingHandCursor);
        ctrlRow->addWidget(btn);
    }

    connect(m_btn25, &QPushButton::clicked, this, [this]() { setDurationMinutes(25); });
    connect(m_btn45, &QPushButton::clicked, this, [this]() { setDurationMinutes(45); });
    connect(m_btn50, &QPushButton::clicked, this, [this]() { setDurationMinutes(50); });

    ctrlRow->addSpacing(8);

    m_startPauseButton = new QPushButton(QStringLiteral("▶ Старт"), focusCard);
    m_startPauseButton->setFont(Theme::fontUi(11, QFont::Bold));
    m_startPauseButton->setFixedHeight(28);
    m_startPauseButton->setCursor(Qt::PointingHandCursor);
    m_startPauseButton->setStyleSheet(QStringLiteral(
        "QPushButton {"
        "  background: %1; color: #14142B; border: none; border-radius: 6px; padding: 0 12px;"
        "}"
        "QPushButton:hover { background: %2; }"
    ).arg(Theme::accentColor().name(), Theme::accentColor().lighter(115).name()));
    connect(m_startPauseButton, &QPushButton::clicked, this, &PomodoroWidget::startOrResumeSession);
    ctrlRow->addWidget(m_startPauseButton);

    m_stopButton = new QPushButton(QStringLiteral("⏹"), focusCard);
    m_stopButton->setFont(Theme::fontUi(11, QFont::Bold));
    m_stopButton->setFixedSize(28, 28);
    m_stopButton->setCursor(Qt::PointingHandCursor);
    m_stopButton->setToolTip(QStringLiteral("Стоп и сохранить сессию"));
    connect(m_stopButton, &QPushButton::clicked, this, [this]() { stopAndSaveSession(false); });
    ctrlRow->addWidget(m_stopButton);

    cardLayout->addLayout(ctrlRow);
    root->addWidget(focusCard);

    // ==========================================
    // 2. SCROLLABLE TILES CONTAINER (Grouped by Tier)
    // ==========================================
    auto* scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setMinimumHeight(180);
    scrollArea->setStyleSheet("background: transparent; border: none;");

    auto* tilesContent = new QWidget(scrollArea);
    auto* tilesLayout = new QVBoxLayout(tilesContent);
    tilesLayout->setContentsMargins(0, 4, 0, 4);
    tilesLayout->setSpacing(8);

    // Section Helper
    auto createSection = [tilesContent, tilesLayout](const QString& title, QWidget** containerOut, FlowLayout** layoutOut) {
        auto* titleLabel = new QLabel(title, tilesContent);
        titleLabel->setFont(Theme::fontMono(9, QFont::Bold));
        titleLabel->setStyleSheet(QStringLiteral("color: %1; letter-spacing: 0.5px;").arg(Theme::textDim().name()));
        tilesLayout->addWidget(titleLabel);

        *containerOut = new QWidget(tilesContent);
        *layoutOut = new FlowLayout(*containerOut, 0, 6, 6);
        tilesLayout->addWidget(*containerOut);
    };

    const LocalSeedConfig& seed = CategoryConfigLoader::seedConfig();
    createSection(seed.pomodoroTierALabel, &m_tierAContainer, &m_tierALayout);
    createSection(seed.pomodoroTierBLabel, &m_tierBContainer, &m_tierBLayout);
    createSection(seed.pomodoroTierCLabel, &m_tierCContainer, &m_tierCLayout);
    tilesLayout->addStretch();

    scrollArea->setWidget(tilesContent);
    root->addWidget(scrollArea, 1);

    // ==========================================
    // 3. FOOTER STATS & TOOLS
    // ==========================================
    auto* footerFrame = new QFrame(this);
    auto* footerLayout = new QVBoxLayout(footerFrame);
    footerLayout->setContentsMargins(2, 4, 2, 0);
    footerLayout->setSpacing(4);

    // Stats line
    m_todayStatsLabel = new QLabel(QStringLiteral("Сегодня: 0м · Tier A: 0% · Сессий: 0"), footerFrame);
    m_todayStatsLabel->setFont(Theme::fontMono(10, QFont::Normal));
    m_todayStatsLabel->setStyleSheet(QStringLiteral("color: %1;").arg(Theme::textDim().name()));
    footerLayout->addWidget(m_todayStatsLabel);

    // Last session line
    m_lastSessionLabel = new QLabel(QStringLiteral("Посл: --:--"), footerFrame);
    m_lastSessionLabel->setFont(Theme::fontMono(9, QFont::Normal));
    m_lastSessionLabel->setStyleSheet(QStringLiteral("color: %1;").arg(Theme::textDim().name()));
    footerLayout->addWidget(m_lastSessionLabel);

    // Tier A advice if 0%
    m_tierAAdviceLabel = new QLabel(QStringLiteral("💡 Совет: запланируй спринт по Корзине А для защиты диплома"), footerFrame);
    m_tierAAdviceLabel->setFont(Theme::fontUi(9, QFont::Normal));
    m_tierAAdviceLabel->setStyleSheet(QStringLiteral("color: %1;").arg(Theme::accentColor().name()));
    m_tierAAdviceLabel->setVisible(false);
    footerLayout->addWidget(m_tierAAdviceLabel);

    // Manual Adjustments & Tools Row
    auto* bottomRow = new QHBoxLayout();
    bottomRow->setSpacing(4);

    auto createPill = [this, bottomRow](const QString& text, int delta) {
        auto* btn = new QPushButton(text, this);
        btn->setFont(Theme::fontMono(9, QFont::Normal));
        btn->setFixedHeight(22);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setStyleSheet(QStringLiteral(
            "QPushButton { background: %1; border: 1px solid %2; border-radius: 4px; padding: 0 4px; color: %3; }"
            "QPushButton:hover { background: %4; }"
        ).arg(Theme::surface2().name(QColor::HexArgb),
              Theme::stroke().name(QColor::HexArgb),
              Theme::textDim().name(),
              Theme::surface3().name(QColor::HexArgb)));
        connect(btn, &QPushButton::clicked, this, [this, delta]() { adjustMinutes(delta); });
        bottomRow->addWidget(btn);
    };

    createPill(QStringLiteral("+15m"), 15);
    createPill(QStringLiteral("+30m"), 30);
    createPill(QStringLiteral("+1ч"), 60);
    createPill(QStringLiteral("−15m"), -15);

    bottomRow->addStretch();

    m_notesButton = new QPushButton(QStringLiteral("↗ Заметки"), footerFrame);
    m_notesButton->setFont(Theme::fontUi(9, QFont::DemiBold));
    m_notesButton->setFixedHeight(22);
    m_notesButton->setCursor(Qt::PointingHandCursor);
    m_notesButton->setToolTip(QStringLiteral("Открыть заметку предмета в Obsidian"));
    m_notesButton->setStyleSheet(QStringLiteral(
        "QPushButton { background: %1; border: 1px solid %2; border-radius: 5px; padding: 0 7px; color: %3; }"
        "QPushButton:hover { background: %4; }"
    ).arg(Theme::surface2().name(QColor::HexArgb),
          Theme::stroke().name(QColor::HexArgb),
          Theme::textColor().name(),
          Theme::surface3().name(QColor::HexArgb)));
    connect(m_notesButton, &QPushButton::clicked, this, &PomodoroWidget::openObsidianNotes);
    bottomRow->addWidget(m_notesButton);

    m_undoButton = new QPushButton(QStringLiteral("↩ Отмена"), footerFrame);
    m_undoButton->setFont(Theme::fontUi(9, QFont::DemiBold));
    m_undoButton->setFixedHeight(22);
    m_undoButton->setCursor(Qt::PointingHandCursor);
    m_undoButton->setToolTip(QStringLiteral("Откатить последнюю сохранённую сессию"));
    m_undoButton->setStyleSheet(QStringLiteral(
        "QPushButton { background: %1; border: 1px solid %2; border-radius: 5px; padding: 0 7px; color: %3; }"
        "QPushButton:hover { background: %4; }"
    ).arg(Theme::surface2().name(QColor::HexArgb),
          Theme::stroke().name(QColor::HexArgb),
          Theme::textColor().name(),
          Theme::surface3().name(QColor::HexArgb)));
    connect(m_undoButton, &QPushButton::clicked, this, &PomodoroWidget::undoLast);
    bottomRow->addWidget(m_undoButton);

    footerLayout->addLayout(bottomRow);
    root->addWidget(footerFrame);
}

void PomodoroWidget::buildSubjectTiles()
{
    qDeleteAll(m_tiles);
    m_tiles.clear();

    const auto categories = DataStore::instance().studyCategories(false);
    const QDateTime semStart = CategoryConfigLoader::semesterStartDate().startOfDay();
    int hotkey = 1;

    for (const StudyCategory& cat : categories) {
        const QString tier = cat.tier.toUpper();
        QWidget* parentContainer = m_tierCContainer;
        FlowLayout* targetLayout = m_tierCLayout;
        if (tier == "A") {
            parentContainer = m_tierAContainer;
            targetLayout = m_tierALayout;
        } else if (tier == "B") {
            parentContainer = m_tierBContainer;
            targetLayout = m_tierBLayout;
        }

        auto* tile = new SubjectTile(cat, parentContainer);
        const int studiedSecs = DataStore::instance().totalStudySecondsForCategory(cat.id, semStart);
        tile->setStudiedSeconds(studiedSecs);

        if (hotkey <= 9) {
            tile->setHotKeyIndex(hotkey++);
        }

        connect(tile, &SubjectTile::tileClicked, this, &PomodoroWidget::selectCategoryById);
        m_tiles.append(tile);
        targetLayout->addWidget(tile);
    }

    refreshSubjectTiles();
}

void PomodoroWidget::refreshSubjectTiles()
{
    const QDateTime semStart = CategoryConfigLoader::semesterStartDate().startOfDay();
    for (SubjectTile* tile : m_tiles) {
        tile->setSelected(tile->categoryId() == m_activeCategory.id);
        const int studiedSecs = DataStore::instance().totalStudySecondsForCategory(tile->categoryId(), semStart);
        tile->setStudiedSeconds(studiedSecs);
    }
}

bool PomodoroWidget::selectCategoryById(const QString& categoryId)
{
    auto cat = DataStore::instance().categoryById(categoryId);
    if (!cat.id.isEmpty()) {
        m_activeCategory = cat;
        updateActiveSubjectDisplay();
        refreshSubjectTiles();
        return true;
    }
    return false;
}

void PomodoroWidget::updateActiveSubjectDisplay()
{
    if (m_isBreak) {
        m_subjectTitleLabel->setText(QStringLiteral("☕ Перерыв / Восстановление"));
        m_tierBadgeLabel->setText(QStringLiteral("BREAK"));
        return;
    }

    const QString title = m_activeCategory.name.isEmpty() ? QStringLiteral("Free Study") : m_activeCategory.name;
    m_subjectTitleLabel->setText(title);

    const QString tier = m_activeCategory.tier.isEmpty() ? QStringLiteral("F") : m_activeCategory.tier.toUpper();
    m_tierBadgeLabel->setText(QStringLiteral("TIER %1").arg(tier));

    // Preset button styling highlight
    const int mins = m_targetDurationSeconds / 60;
    auto styleBtn = [](QPushButton* b, bool active) {
        if (active) {
            b->setStyleSheet(QStringLiteral(
                "QPushButton { background: %1; border: 1px solid %2; color: %3; border-radius: 4px; }"
            ).arg(Theme::accentTint(40).name(QColor::HexArgb), Theme::accentColor().name(), Theme::textColor().name()));
        } else {
            b->setStyleSheet(QStringLiteral(
                "QPushButton { background: %1; border: 1px solid %2; color: %3; border-radius: 4px; }"
                "QPushButton:hover { background: %4; }"
            ).arg(Theme::surface2().name(QColor::HexArgb), Theme::stroke().name(QColor::HexArgb), Theme::textDim().name(), Theme::surface3().name(QColor::HexArgb)));
        }
    };

    styleBtn(m_btn25, mins == 25);
    styleBtn(m_btn45, mins == 45);
    styleBtn(m_btn50, mins == 50);
}

void PomodoroWidget::setDurationMinutes(int minutes)
{
    if (m_state == SessionState::Running) {
        return;
    }

    m_targetDurationSeconds = minutes * 60;
    m_remainingSeconds = m_targetDurationSeconds;
    m_breakDurationSeconds = (minutes >= 45) ? 10 * 60 : 5 * 60;
    updateTimerDisplay();
    updateActiveSubjectDisplay();
}

void PomodoroWidget::startOrResumeSession()
{
    if (m_state == SessionState::Running || m_state == SessionState::BreakRunning) {
        pauseSession();
        return;
    }

    if (m_state == SessionState::Idle) {
        m_sessionStartedAt = QDateTime::currentDateTime();
        m_sessionDeadline = m_sessionStartedAt.addSecs(m_remainingSeconds);
    } else if (m_state == SessionState::Paused) {
        m_sessionDeadline = QDateTime::currentDateTime().addSecs(m_remainingSeconds);
    }

    m_state = m_isBreak ? SessionState::BreakRunning : SessionState::Running;
    m_lastTickTime = QDateTime::currentDateTime();
    m_tickTimer->start(1000);

    m_startPauseButton->setText(QStringLiteral("⏸ Пауза"));
    m_startPauseButton->setStyleSheet(QStringLiteral(
        "QPushButton {"
        "  background: #F2C47E; color: #14142B; border: none; border-radius: 6px; padding: 0 12px;"
        "}"
        "QPushButton:hover { background: #E3B068; }"
    ));

    saveRecoveryState();
}

void PomodoroWidget::pauseSession()
{
    if (m_state != SessionState::Running && m_state != SessionState::BreakRunning) {
        return;
    }

    m_state = SessionState::Paused;
    m_tickTimer->stop();

    m_startPauseButton->setText(QStringLiteral("▶ Продолжить"));
    m_startPauseButton->setStyleSheet(QStringLiteral(
        "QPushButton {"
        "  background: %1; color: #14142B; border: none; border-radius: 6px; padding: 0 12px;"
        "}"
    ).arg(Theme::accentColor().name()));

    saveRecoveryState();
}

void PomodoroWidget::stopAndSaveSession(bool completed, bool isBreak)
{
    m_tickTimer->stop();

    if (!isBreak) {
        const int elapsedSeconds = m_targetDurationSeconds - m_remainingSeconds;
        if (elapsedSeconds >= 60 || completed) {
            const int duration = completed ? m_targetDurationSeconds : elapsedSeconds;

            StudySession session;
            session.categoryId = m_activeCategory.id.isEmpty() ? QStringLiteral("free-study") : m_activeCategory.id;
            session.category = m_activeCategory.name.isEmpty() ? QStringLiteral("Free Study") : m_activeCategory.name;
            session.startedAt = m_sessionStartedAt.isValid() ? m_sessionStartedAt : QDateTime::currentDateTime().addSecs(-duration);
            session.finishedAt = QDateTime::currentDateTime();
            session.durationSeconds = duration;
            session.completed = completed;
            session.note = completed ? QStringLiteral("Pomodoro completed") : QStringLiteral("Pomodoro stopped");
            session.source = QStringLiteral("timer");
            session.loggedMd = false;

            DataStore::instance().addStudySession(session);

            // Write to Obsidian TimeLog.md
            QString vaultName = m_activeCategory.vaultName.isEmpty() ? m_activeCategory.shortName : m_activeCategory.vaultName;
            if (vaultName.isEmpty()) vaultName = session.category;
            TimeLogWriter::appendSession(session, vaultName);
        }
    }

    m_state = SessionState::Idle;
    m_isBreak = false;
    m_remainingSeconds = m_targetDurationSeconds;

    m_startPauseButton->setText(QStringLiteral("▶ Старт"));
    m_startPauseButton->setStyleSheet(QStringLiteral(
        "QPushButton {"
        "  background: %1; color: #14142B; border: none; border-radius: 6px; padding: 0 12px;"
        "}"
    ).arg(Theme::accentColor().name()));

    clearRecoveryState();
    refreshSubjectTiles();
    updateTodaySummary();
    updateTimerDisplay();
    updateActiveSubjectDisplay();
}

void PomodoroWidget::adjustMinutes(int deltaMinutes)
{
    if (m_activeCategory.id.isEmpty()) {
        return;
    }

    const QString note = deltaMinutes > 0 ? QStringLiteral("Manual +%1m").arg(deltaMinutes) : QStringLiteral("Manual %1m").arg(deltaMinutes);
    DataStore::instance().addManualStudyMinutes(m_activeCategory.id, m_activeCategory.name, deltaMinutes, note);

    if (deltaMinutes > 0) {
        StudySession s;
        s.categoryId = m_activeCategory.id;
        s.category = m_activeCategory.name;
        s.startedAt = QDateTime::currentDateTime();
        s.finishedAt = s.startedAt;
        s.durationSeconds = deltaMinutes * 60;
        s.completed = true;
        s.note = note;
        s.source = QStringLiteral("manual");
        s.loggedMd = false;

        QString vaultName = m_activeCategory.vaultName.isEmpty() ? m_activeCategory.shortName : m_activeCategory.vaultName;
        if (vaultName.isEmpty()) vaultName = s.category;
        TimeLogWriter::appendSession(s, vaultName);
    }

    refreshSubjectTiles();
    updateTodaySummary();
}

void PomodoroWidget::undoLast()
{
    if (DataStore::instance().undoLastSession()) {
        refreshSubjectTiles();
        updateTodaySummary();
    }
}

void PomodoroWidget::openObsidianNotes()
{
    QString queryName = m_activeCategory.vaultName.isEmpty() ? m_activeCategory.shortName : m_activeCategory.vaultName;
    if (queryName.isEmpty())
    {
        queryName = m_activeCategory.name;
    }

    const QString vault = CategoryConfigLoader::seedConfig().obsidianVaultName.trimmed();
    QUrl uri(QStringLiteral("obsidian://search"));
    QUrlQuery query;
    if (!vault.isEmpty())
    {
        query.addQueryItem(QStringLiteral("vault"), vault);
    }
    query.addQueryItem(QStringLiteral("query"), queryName);
    uri.setQuery(query);
    QDesktopServices::openUrl(uri);
}

void PomodoroWidget::updateTimerDisplay()
{
    const QString durText = formatDuration(m_remainingSeconds);
    m_timerLabel->setText(durText);
    const QString shortSubj = m_activeCategory.shortName.isEmpty() ? QStringLiteral("Focus") : m_activeCategory.shortName;
    setChromeTitle(shortSubj, durText);

    if (m_targetDurationSeconds > 0) {
        const int percent = qBound(0, (m_remainingSeconds * 100) / m_targetDurationSeconds, 100);
        m_sessionProgressBar->setValue(percent);
    }
}

void PomodoroWidget::updateTodaySummary()
{
    const QDate today = QDate::currentDate();
    const int todaySecs = DataStore::instance().totalStudySecondsForDate(today);
    const int tierAPercent = DataStore::instance().tierAStudyPercentForDate(today);
    const int sessionsCount = DataStore::instance().sessionsTodayCount();

    m_todayStatsLabel->setText(QStringLiteral("Сегодня: %1 · Tier A: %2% · Сессий: %3")
        .arg(formatHoursMinutes(todaySecs))
        .arg(tierAPercent)
        .arg(sessionsCount));

    m_tierAAdviceLabel->setVisible(tierAPercent == 0 && todaySecs > 0);

    const StudySession last = DataStore::instance().lastSession();
    if (last.id > 0 && last.finishedAt.isValid()) {
        const int mins = qMax(1, qRound(last.durationSeconds / 60.0));
        const StudyCategory cat = DataStore::instance().categoryById(last.categoryId);
        QString shortCat = !cat.shortName.isEmpty() ? cat.shortName : last.category;
        if (shortCat.length() > 20) {
            shortCat = shortCat.left(18) + QStringLiteral("…");
        }
        m_lastSessionLabel->setText(QStringLiteral("Посл: %1 → %2 (%3м) · %4")
            .arg(last.startedAt.toString("HH:mm"))
            .arg(last.finishedAt.toString("HH:mm"))
            .arg(mins)
            .arg(shortCat));
    } else {
        m_lastSessionLabel->setText(QStringLiteral("Посл: --:--"));
    }
}

void PomodoroWidget::buildContextMenu(QMenu* menu)
{
    const bool running = (m_state == SessionState::Running || m_state == SessionState::BreakRunning);
    QAction* startPauseAct = menu->addAction(
        running ? QString::fromUtf8("⏸ Пауза (Space)") : QString::fromUtf8("▶ Старт / Продолжить (Space)"));
    QAction* stopAct = menu->addAction(QString::fromUtf8("⏹ Стоп и сохранить сессию"));

    connect(startPauseAct, &QAction::triggered, this, &PomodoroWidget::startOrResumeSession);
    connect(stopAct, &QAction::triggered, this, [this]() { stopAndSaveSession(false); });

    QMenu* durMenu = menu->addMenu(QString::fromUtf8("Длительность спринта"));
    QAction* d25 = durMenu->addAction(QString::fromUtf8("25 минут"));
    QAction* d45 = durMenu->addAction(QString::fromUtf8("45 минут ⚡"));
    QAction* d50 = durMenu->addAction(QString::fromUtf8("50 минут"));
    connect(d25, &QAction::triggered, this, [this]() { setDurationMinutes(25); });
    connect(d45, &QAction::triggered, this, [this]() { setDurationMinutes(45); });
    connect(d50, &QAction::triggered, this, [this]() { setDurationMinutes(50); });

    const QString subjName = m_activeCategory.shortName.isEmpty() ? m_activeCategory.name : m_activeCategory.shortName;
    QMenu* adjMenu = menu->addMenu(QString::fromUtf8("Ручная корректировка (%1)").arg(subjName));
    QAction* p15 = adjMenu->addAction("+15 минут");
    QAction* p30 = adjMenu->addAction("+30 минут");
    QAction* p60 = adjMenu->addAction(QString::fromUtf8("+1 час"));
    QAction* m15 = adjMenu->addAction(QString::fromUtf8("−15 минут"));
    connect(p15, &QAction::triggered, this, [this]() { adjustMinutes(15); });
    connect(p30, &QAction::triggered, this, [this]() { adjustMinutes(30); });
    connect(p60, &QAction::triggered, this, [this]() { adjustMinutes(60); });
    connect(m15, &QAction::triggered, this, [this]() { adjustMinutes(-15); });

    menu->addSeparator();
    QAction* notesAct = menu->addAction(QString::fromUtf8("↗ Открыть заметки в Obsidian"));
    QAction* undoAct = menu->addAction(QString::fromUtf8("↩ Отменить последнюю сохранённую сессию"));
    connect(notesAct, &QAction::triggered, this, &PomodoroWidget::openObsidianNotes);
    connect(undoAct, &QAction::triggered, this, &PomodoroWidget::undoLast);
}

void PomodoroWidget::checkSleepGuard()
{
    const QDateTime now = QDateTime::currentDateTime();
    if (m_lastTickTime.isValid()) {
        const qint64 diffSecs = m_lastTickTime.secsTo(now);
        if (diffSecs > 5 && m_sessionDeadline.isValid()) {
            // Drift detected (system sleep/hibernation)
            const qint64 trueRemaining = now.secsTo(m_sessionDeadline);
            m_remainingSeconds = qMax(0, static_cast<int>(trueRemaining));
        }
    }
    m_lastTickTime = now;
}

void PomodoroWidget::saveRecoveryState()
{
    QSettings settings;
    settings.setValue("pomodoro/state", static_cast<int>(m_state));
    settings.setValue("pomodoro/categoryId", m_activeCategory.id);
    settings.setValue("pomodoro/targetDuration", m_targetDurationSeconds);
    settings.setValue("pomodoro/remainingSeconds", m_remainingSeconds);
    settings.setValue("pomodoro/startedAt", m_sessionStartedAt.toString(Qt::ISODate));
    settings.setValue("pomodoro/deadline", m_sessionDeadline.toString(Qt::ISODate));
    settings.setValue("pomodoro/isBreak", m_isBreak);
}

void PomodoroWidget::restoreRecoveryState()
{
    QSettings settings;
    if (!settings.contains("pomodoro/state")) return;

    const auto savedState = static_cast<SessionState>(settings.value("pomodoro/state").toInt());
    if (savedState == SessionState::Running || savedState == SessionState::Paused || savedState == SessionState::BreakRunning) {
        const QString savedCatId = settings.value("pomodoro/categoryId").toString();
        selectCategoryById(savedCatId);

        m_targetDurationSeconds = settings.value("pomodoro/targetDuration", 45 * 60).toInt();
        m_remainingSeconds = settings.value("pomodoro/remainingSeconds", 45 * 60).toInt();
        m_sessionStartedAt = QDateTime::fromString(settings.value("pomodoro/startedAt").toString(), Qt::ISODate);
        m_sessionDeadline = QDateTime::fromString(settings.value("pomodoro/deadline").toString(), Qt::ISODate);
        m_isBreak = settings.value("pomodoro/isBreak", false).toBool();

        // Restore as paused so user can decide to continue
        m_state = SessionState::Paused;
        m_startPauseButton->setText(QStringLiteral("▶ Возобновить"));
        updateTimerDisplay();
        updateActiveSubjectDisplay();
    }
}

void PomodoroWidget::clearRecoveryState()
{
    QSettings settings;
    settings.remove("pomodoro/state");
    settings.remove("pomodoro/categoryId");
    settings.remove("pomodoro/targetDuration");
    settings.remove("pomodoro/remainingSeconds");
    settings.remove("pomodoro/startedAt");
    settings.remove("pomodoro/deadline");
    settings.remove("pomodoro/isBreak");
}

void PomodoroWidget::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Space) {
        startOrResumeSession();
        event->accept();
        return;
    }

    if (event->key() >= Qt::Key_1 && event->key() <= Qt::Key_9) {
        const int index = event->key() - Qt::Key_1;
        if (index >= 0 && index < m_tiles.size()) {
            selectCategoryById(m_tiles.at(index)->categoryId());
            event->accept();
            return;
        }
    }

    WidgetBase::keyPressEvent(event);
}

QString PomodoroWidget::formatDuration(int totalSeconds) const
{
    const int minutes = qMax(0, totalSeconds) / 60;
    const int seconds = qMax(0, totalSeconds) % 60;
    return QStringLiteral("%1:%2")
        .arg(minutes, 2, 10, QChar('0'))
        .arg(seconds, 2, 10, QChar('0'));
}

QString PomodoroWidget::formatHoursMinutes(int totalSeconds) const
{
    const int minutes = qMax(0, totalSeconds) / 60;
    const int hours = minutes / 60;
    const int mins = minutes % 60;
    if (hours > 0) {
        return QStringLiteral("%1ч %2м").arg(hours).arg(mins);
    }
    return QStringLiteral("%1м").arg(mins);
}

REGISTER_WIDGET(
    "pomodoro",
    "Pomodoro focus timer",
    PomodoroWidget
)
