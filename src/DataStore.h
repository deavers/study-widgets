#pragma once

#include <QDate>
#include <QDateTime>
#include <QList>
#include <QPointer>
#include <QSet>
#include <QString>
#include <QtGlobal>
#include <functional>

class QObject;

struct StudySession {
    qint64 id = -1;

    QString category;
    QString categoryId;

    QDateTime startedAt;
    QDateTime finishedAt;

    int durationSeconds = 0;
    bool completed = false;

    QString note;
    QString source = "timer"; // "timer" | "manual" | "import"
    bool loggedMd = false;
};

struct StudyCategory {
    QString id;
    QString name;
    QString shortName;
    QString type;
    QString color;
    QString tier = "F";       // "A", "B", "C", "P", "K", "F"
    QString vaultName;
    bool done = false;
    int courseLimit = 0;

    bool archived = false;
    int sortOrder = 0;
    int targetMinutes = 1500;
};

struct SpacedReviewItem {
    qint64 id = -1;
    QString categoryId;
    QString categoryName;
    QString topic;       // Pointer / Coefficient / ?-Ticket anchor
    QString kind;        // "prednaska", "cviceni", "exam"
    int stage = 0;       // 0=30m, 1=2h, 2=1d, 3=3d, 4=7d, 5=14d, 6=30d
    QDateTime nextReviewAt;
    QDateTime lastReviewedAt;
    bool completed = false;
    QString note;
};

struct GoalItem {
    qint64 id = -1;
    QString horizon;     // "day", "week", "month", "year"
    QString title;
    int currentValue = 0;
    int targetValue = 1;
    bool completed = false;
    QString periodKey;
    int sortOrder = 0;
};

struct CalendarEvent {
    qint64 id = -1;
    QDate date;
    QString timeText;    // e.g. "14:00" or ""
    QString title;
    QString tag = "general"; // "exam", "deadline", "study", "general"
    bool completed = false;
};

class DataStore {
public:
    static DataStore& instance();

    bool open();
    bool isOpen() const;

    QString databasePath() const;
    QString appDataDir() const;
    QString lastError() const;

    bool addStudySession(const StudySession& session);
    bool addManualStudyMinutes(
        const QString& categoryId,
        const QString& categoryName,
        int deltaMinutes,
        const QString& note = "Manual adjustment"
    );

    QList<StudySession> recentStudySessions(int limit = 50);
    StudySession lastSession();
    bool undoLastSession();
    int sessionsTodayCount();
    QList<StudySession> unloggedSessions();
    bool markSessionLogged(qint64 sessionId);

    int totalStudySecondsForDate(const QDate& date);
    int totalStudySecondsForCategory(const QString& categoryId, const QDateTime& since = QDateTime());
    int tierAStudyPercentForDate(const QDate& date);

    QList<StudyCategory> studyCategories(bool includeArchived = false);
    StudyCategory categoryById(const QString& categoryId);
    bool syncStudyCategories(const QList<StudyCategory>& categories);
    bool updateCategoryDone(const QString& categoryId, bool done);

    // Spaced Repetition (Practice / Lectures / Exam)
    QList<SpacedReviewItem> spacedReviews(bool includeCompleted = false);
    bool addSpacedReview(
        const QString& categoryId,
        const QString& topic,
        const QString& kind,
        const QString& note = QString()
    );
    bool advanceSpacedReview(qint64 id, bool resetTo30Min = false);
    bool removeSpacedReview(qint64 id);

    // Multi-Horizon Goals (Day / Week / Month / Year)
    QList<GoalItem> goalsByHorizon(const QString& horizon);
    bool addGoal(
        const QString& horizon,
        const QString& title,
        int targetValue = 1,
        int currentValue = 0
    );
    bool toggleGoal(qint64 id, bool completed);
    bool adjustGoalProgress(qint64 id, int delta);
    bool removeGoal(qint64 id);
    bool clearCompletedGoals(const QString& horizon);

    // Calendar Events & Deadlines
    QList<CalendarEvent> calendarEventsBetween(const QDate& startDate, const QDate& endDate);
    bool addCalendarEvent(
        const QDate& date,
        const QString& title,
        const QString& timeText = QString(),
        const QString& tag = "general"
    );
    bool toggleCalendarEvent(qint64 id, bool completed);
    bool removeCalendarEvent(qint64 id);
    bool clearCompletedCalendarEvents();
    QSet<QDate> activeStudyDatesBetween(const QDate& startDate, const QDate& endDate);

    // Export JSON snapshot for deave OS (FastAPI / React dashboard integration)
    bool exportDeaveBridgeSnapshot();

    // Semester Schedule, Specialty Curriculum & Bidirectional Auto-Sync (Calendar <-> Goals <-> Roadmaps)
    void ensureSemesterScheduleAndAutoTasks(bool forceReseed = false);
    void syncDailyTasksWithCalendarAndRoadmaps(const QDate& targetDate = QDate::currentDate());
    void registerChangeListener(QObject* context, std::function<void()> callback);
    void notifyListeners();

    static QDate semesterEndDate();
    static QString dailySubjectConsolidationTaskForDate(const QDate& date, QString* outTime = nullptr);
    static QString dailySpecialtyZeroToHeroTaskForDate(const QDate& date);
    static QStringList nextOpenTasksFromRoadmaps(int maxCount = 2);
    static bool markRoadmapTaskCompleted(const QString& taskText, bool completed);

    static qint64 intervalSecondsForStage(int stage);
    static QString stageLabel(int stage);

private:
    DataStore() = default;

    struct ListenerEntry {
        QPointer<QObject> context;
        std::function<void()> callback;
    };

    bool ensureSchema();
    bool createSchemaVersionOne();
    bool migrateSchemaV1ToV2();
    bool migrateSchemaV2ToV3();
    bool migrateSchemaV3ToV4();
    bool migrateSchemaV4ToV5();
    void seedDefaultGoalsAndReviewsIfEmpty();

    bool openDatabase();
    void setError(const QString& error);

    bool m_isOpen = false;
    bool m_syncing = false;
    QDate m_lastSyncedDate;
    QList<ListenerEntry> m_listeners;

    QString m_databasePath;
    QString m_appDataDir;
    QString m_lastError;
};
