#pragma once

#include <QDate>
#include <QList>
#include <QMap>
#include <QString>
#include <QStringList>

struct LocalRoadmapEntry
{
    QString label;
    QString shortLabel;
    QString path;
    bool syncDaily = true;
};

struct LocalPrayerMethod
{
    QString menuLabel;
    QString badgeLabel;
    double fajrAngle = 12.0;
    double ishaAngle = 12.0;
    int tuneFajr = 0;
    int tuneSunrise = 0;
    int tuneDhuhr = 2;
    int tuneAsr = 0;
    int tuneMaghrib = 2;
    int tuneIsha = 0;
};

struct LocalGoalSeed
{
    QString horizon;
    QString title;
    int targetValue = 1;
    int currentValue = 0;
};

struct LocalSpacedReviewSeed
{
    QString categoryId;
    QString topic;
    QString kind;
};

struct LocalCalendarEventSeed
{
    int dayOffset = 0;
    QString title;
    QString timeText;
    QString tag = QStringLiteral("general");
};

struct LocalSemesterDayRule
{
    QString timeText;
    QString text;
    QString evenText;
    QString oddText;
};

struct LocalHorizonGoalSeed
{
    QString horizon;
    QString title;
    int targetValue = 1;
    int currentValue = 0;
    int sortOrder = 0;
};

struct LocalSeedConfig
{
    bool loaded = false;

    QString timeLogPath;
    QString quickInboxPath;
    QString obsidianVaultName;
    QString scheduleImagePath;
    QString scheduleBrowseDir;
    QString roadmapBrowseDir;
    QString defaultRoadmapPath;
    QList<LocalRoadmapEntry> roadmaps;

    QString studyProfileLabel = QString::fromUtf8("🎓 Учёба");
    QString pomodoroTierALabel = QString::fromUtf8("TIER A — ВЫСОКИЙ ПРИОРИТЕТ");
    QString pomodoroTierBLabel = QString::fromUtf8("TIER B — СРЕДНИЙ ПРИОРИТЕТ");
    QString pomodoroTierCLabel = QString::fromUtf8("TIER C · ПРОЕКТЫ · КУРСЫ");
    QString scheduleTitle = QString::fromUtf8("Расписание");
    QString scheduleSubtitle = QString::fromUtf8("Учёба");
    QString scheduleHeaderShort = QString::fromUtf8("План");
    QString weekEvenLabel = QStringLiteral("EVEN");
    QString weekOddLabel = QStringLiteral("ODD");
    QString weekEvenShort = QStringLiteral("E");
    QString weekOddShort = QStringLiteral("O");
    QString calendarDialogExample = QString::fromUtf8("14:00 Лекция / Практика");
    QString calendarQuickHint = QString::fromUtf8("14:00 Лекция");
    QString goalsAutoSyncLabel = QString::fromUtf8("↻ Синхронизировать авто-задачи и Roadmap");
    QString studyEventTag = QStringLiteral("study");
    QString roadmapTaskPrefix = QString::fromUtf8("🗺️ [Roadmap]");
    QString wisdomFlowHint = QString::fromUtf8(
        "Поток: радар чист. Лучшее окно для глубокой работы: ключевые проекты и приоритетные задачи."
    );
    QString wisdomAmanahHint = QString::fromUtf8(
        "Аманат и границы: эмпатия — дар, но твои тело и время тоже аманат. Помогай системно, не сжигая сон и приоритетные задачи."
    );

    QString prayerCityName = QString::fromUtf8("По умолчанию");
    QString prayerPresetName = QStringLiteral("Standard 12° (UOIF)");
    double prayerLatitude = 50.08;
    double prayerLongitude = 14.43;
    double prayerElevation = 200.0;
    QList<LocalPrayerMethod> prayerMethods;

    QList<LocalGoalSeed> goals;
    QList<LocalSpacedReviewSeed> spacedReviews;
    QList<LocalCalendarEventSeed> calendarEvents;

    QDate semesterStartDate;
    QDate semesterTeachingEndDate;
    QDate semesterEndDate;
    QMap<int, LocalSemesterDayRule> teachingDays;
    QMap<int, LocalSemesterDayRule> examDays;
    QList<LocalHorizonGoalSeed> semesterHorizonGoals;

    QDate curriculumAnchorDate = QDate(2026, 11, 9);
    QString curriculumTime = QStringLiteral("19:30");
    QStringList curriculumItems;
};

class CategoryConfigLoader
{
public:
    static QString localConfigPath();
    static QString seedConfigPath();

    static bool importLocalCategories(
        QString* errorMessage = nullptr
    );

    static const LocalSeedConfig& seedConfig();
    static bool reloadSeedConfig(QString* errorMessage = nullptr);

    static QDate semesterStartDate();
    static QString timeLogFilePath();
    static void setSemesterStartDate(const QDate& date);
    static void setTimeLogFilePath(const QString& path);
};
