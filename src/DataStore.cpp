#include "DataStore.h"

#include "CategoryConfigLoader.h"

#include <QCryptographicHash>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QSettings>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QTextStream>
#include <QTimer>

namespace {

constexpr int CurrentSchemaVersion = 7;

const QString ConnectionName = "StudyWidgetsSqlConnection";

struct DefaultCategory {
    QString id;
    QString name;
    QString shortName;
    QString type;
    QString color;
    QString tier = "F";
    QString vaultName;
    int sortOrder = 0;
    int targetMinutes = 1500;
};

QList<DefaultCategory> defaultCategories()
{
    return {
        {"free-study", "⚡ Free Study / Deep Research", "Free", "general", "#00F0FF", "F", "Free Study", 0, 600},
        {"subject-example-1", "🔴 [Tier A] Example Subject 1", "Subj 1", "subject", "#EF4444", "A", "Subject 1", 10, 1800},
        {"subject-example-2", "🟡 [Tier B] Example Subject 2", "Subj 2", "subject", "#3B82F6", "B", "Subject 2", 20, 1200},
        {"project-example", "🛠️ Sample Technical Project", "Project", "project", "#10B981", "P", "Project", 30, 1500}
    };
}

QString toDatabaseDateTime(const QDateTime& value) {
    return value.toString(Qt::ISODateWithMs);
}

QDateTime fromDatabaseDateTime(const QString& value) {
    QDateTime result = QDateTime::fromString(value, Qt::ISODateWithMs);
    if (!result.isValid()) {
        result = QDateTime::fromString(value, Qt::ISODate);
    }
    return result;
}

} // namespace

DataStore& DataStore::instance() {
    static DataStore dataStore;
    return dataStore;
}

qint64 DataStore::intervalSecondsForStage(int stage) {
    switch (stage) {
        case 0: return 30LL * 60;            // 30 минут (после занятия)
        case 1: return 2LL * 3600;           // 2 часа
        case 2: return 24LL * 3600;          // 1 день (после сна — консолидация)
        case 3: return 3LL * 24 * 3600;      // 3 дня
        case 4: return 7LL * 24 * 3600;      // 7 дней
        case 5: return 14LL * 24 * 3600;     // 14 дней
        default: return 30LL * 24 * 3600;    // 30 дней (долгосрочное закрепление)
    }
}

QString DataStore::stageLabel(int stage) {
    static const QStringList labels = {
        "30 мин", "2 часа", "1 день", "3 дня", "7 дней", "14 дней", "30 дней"
    };
    return labels.value(qBound(0, stage, labels.size() - 1));
}

bool DataStore::open() {
    if (m_isOpen) {
        return true;
    }

    m_lastError.clear();

    if (!openDatabase()) {
        return false;
    }

    if (!ensureSchema()) {
        return false;
    }

    m_isOpen = true;
    seedDefaultGoalsAndReviewsIfEmpty();
    ensureSemesterScheduleAndAutoTasks(false);
    syncDailyTasksWithCalendarAndRoadmaps(QDate::currentDate());
    exportDeaveBridgeSnapshot();

    qInfo().noquote() << "StudyWidgets database:" << m_databasePath;
    return true;
}

bool DataStore::isOpen() const {
    return m_isOpen;
}

void DataStore::close()
{
    if (QSqlDatabase::contains(ConnectionName))
    {
        QSqlDatabase database = QSqlDatabase::database(ConnectionName);
        if (database.isOpen())
        {
            database.close();
        }
    }
    m_isOpen = false;
    m_databasePath.clear();
    m_appDataDir.clear();
}

QString DataStore::databasePath() const {
    return m_databasePath;
}

QString DataStore::appDataDir() const {
    return m_appDataDir;
}

QString DataStore::lastError() const {
    return m_lastError;
}

bool DataStore::openDatabase() {
    if (!QSqlDatabase::drivers().contains("QSQLITE")) {
        setError("Qt SQLite driver is not available.");
        return false;
    }

    m_appDataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (m_appDataDir.isEmpty()) {
        setError("Windows AppData directory could not be resolved.");
        return false;
    }

    QDir directory(m_appDataDir);
    if (!directory.mkpath(".") || !directory.mkpath("backups") || !directory.mkpath("config")) {
        setError("Could not create the StudyWidgets application data directory.");
        return false;
    }

    m_databasePath = directory.filePath("studywidgets.sqlite");

    QSqlDatabase database;
    if (QSqlDatabase::contains(ConnectionName)) {
        database = QSqlDatabase::database(ConnectionName);
    } else {
        database = QSqlDatabase::addDatabase("QSQLITE", ConnectionName);
    }

    database.setDatabaseName(m_databasePath);

    if (!database.open()) {
        setError("Could not open SQLite database: " + database.lastError().text());
        return false;
    }

    QSqlQuery pragmaQuery(database);
    pragmaQuery.exec("PRAGMA foreign_keys = ON");
    pragmaQuery.exec("PRAGMA journal_mode = WAL");

    return true;
}

bool DataStore::ensureSchema() {
    QSqlDatabase database = QSqlDatabase::database(ConnectionName);
    QSqlQuery query(database);

    const QString createVersionTable = R"(
        CREATE TABLE IF NOT EXISTS schema_version (
            version INTEGER PRIMARY KEY,
            applied_at TEXT NOT NULL
        )
    )";

    if (!query.exec(createVersionTable)) {
        setError("Could not create schema_version table: " + query.lastError().text());
        return false;
    }

    if (!query.exec("SELECT version FROM schema_version ORDER BY version DESC LIMIT 1")) {
        setError("Could not read database schema version: " + query.lastError().text());
        return false;
    }

    int databaseSchemaVersion = 0;
    if (!query.next()) {
        if (!createSchemaVersionOne()) {
            return false;
        }
        databaseSchemaVersion = 1;
    } else {
        databaseSchemaVersion = query.value(0).toInt();
    }

    if (databaseSchemaVersion > CurrentSchemaVersion) {
        setError("Database schema is newer than this version of StudyWidgets.");
        return false;
    }

    while (databaseSchemaVersion < CurrentSchemaVersion) {
        if (databaseSchemaVersion == 1) {
            if (!migrateSchemaV1ToV2()) return false;
            databaseSchemaVersion = 2;
            continue;
        }
        if (databaseSchemaVersion == 2) {
            if (!migrateSchemaV2ToV3()) return false;
            databaseSchemaVersion = 3;
            continue;
        }
        if (databaseSchemaVersion == 3) {
            if (!migrateSchemaV3ToV4()) return false;
            databaseSchemaVersion = 4;
            continue;
        }
        if (databaseSchemaVersion == 4) {
            if (!migrateSchemaV4ToV5()) return false;
            databaseSchemaVersion = 5;
            continue;
        }
        if (databaseSchemaVersion == 5) {
            if (!migrateSchemaV5ToV6()) return false;
            databaseSchemaVersion = 6;
            continue;
        }
        if (databaseSchemaVersion == 6)
        {
            if (!migrateSchemaV6ToV7()) return false;
            databaseSchemaVersion = 7;
            continue;
        }
        setError("No migration path is available for the current database schema.");
        return false;
    }

    return true;
}

bool DataStore::createSchemaVersionOne() {
    QSqlDatabase database = QSqlDatabase::database(ConnectionName);
    QSqlQuery query(database);

    const QString createSessionsTable = R"(
        CREATE TABLE IF NOT EXISTS study_sessions (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            category TEXT NOT NULL,
            started_at TEXT NOT NULL,
            finished_at TEXT NOT NULL,
            duration_seconds INTEGER NOT NULL,
            completed INTEGER NOT NULL DEFAULT 0,
            note TEXT NOT NULL DEFAULT '',
            created_at TEXT NOT NULL
        )
    )";

    if (!query.exec(createSessionsTable)) {
        setError("Could not create study_sessions table: " + query.lastError().text());
        return false;
    }

    query.exec("CREATE INDEX IF NOT EXISTS idx_study_sessions_started_at ON study_sessions(started_at)");

    query.prepare("INSERT INTO schema_version(version, applied_at) VALUES(?, ?)");
    query.addBindValue(1);
    query.addBindValue(toDatabaseDateTime(QDateTime::currentDateTime()));
    return query.exec();
}

bool DataStore::migrateSchemaV1ToV2() {
    QSqlDatabase database = QSqlDatabase::database(ConnectionName);
    if (!database.transaction()) {
        setError("Could not start database migration transaction: " + database.lastError().text());
        return false;
    }

    QSqlQuery query(database);
    const QString createCategoriesTable = R"(
        CREATE TABLE IF NOT EXISTS categories (
            id TEXT PRIMARY KEY,
            name TEXT NOT NULL COLLATE NOCASE UNIQUE,
            type TEXT NOT NULL CHECK(type IN ('subject', 'general', 'project')),
            color TEXT NOT NULL,
            archived INTEGER NOT NULL DEFAULT 0 CHECK(archived IN (0, 1)),
            sort_order INTEGER NOT NULL DEFAULT 0,
            created_at TEXT NOT NULL
        )
    )";

    if (!query.exec(createCategoriesTable)) {
        database.rollback();
        setError("Could not create categories table: " + query.lastError().text());
        return false;
    }

    query.exec("ALTER TABLE study_sessions ADD COLUMN category_id TEXT");
    query.exec("CREATE INDEX IF NOT EXISTS idx_study_sessions_category_id ON study_sessions(category_id)");

    for (const DefaultCategory& category : defaultCategories()) {
        query.prepare(R"(
            INSERT OR IGNORE INTO categories (id, name, type, color, archived, sort_order, created_at)
            VALUES (?, ?, ?, ?, ?, ?, ?)
        )");
        query.addBindValue(category.id);
        query.addBindValue(category.name);
        query.addBindValue(category.type);
        query.addBindValue(category.color);
        query.addBindValue(0);
        query.addBindValue(category.sortOrder);
        query.addBindValue(toDatabaseDateTime(QDateTime::currentDateTime()));
        query.exec();
    }

    query.exec("UPDATE study_sessions SET category_id = 'free-study' WHERE category_id IS NULL");

    query.prepare("INSERT INTO schema_version(version, applied_at) VALUES(?, ?)");
    query.addBindValue(2);
    query.addBindValue(toDatabaseDateTime(QDateTime::currentDateTime()));
    if (!query.exec() || !database.commit()) {
        database.rollback();
        return false;
    }

    return true;
}

bool DataStore::migrateSchemaV2ToV3() {
    // Backup database before V3 migration
    const QString backupPath = QDir(m_appDataDir).filePath("backups/studywidgets-before-v3.sqlite");
    if (!QFile::exists(backupPath)) {
        QFile::copy(m_databasePath, backupPath);
    }

    QSqlDatabase database = QSqlDatabase::database(ConnectionName);
    if (!database.transaction()) {
        setError("Could not start V2->V3 migration transaction: " + database.lastError().text());
        return false;
    }

    QSqlQuery query(database);

    // Add target_minutes to categories if not present
    query.exec("ALTER TABLE categories ADD COLUMN target_minutes INTEGER NOT NULL DEFAULT 1500");

    // Create spaced_reviews table
    const QString createReviewsSql = R"(
        CREATE TABLE IF NOT EXISTS spaced_reviews (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            category_id TEXT NOT NULL,
            topic TEXT NOT NULL,
            kind TEXT NOT NULL DEFAULT 'cviceni',
            stage INTEGER NOT NULL DEFAULT 0,
            next_review_at TEXT NOT NULL,
            last_reviewed_at TEXT NOT NULL,
            completed INTEGER NOT NULL DEFAULT 0,
            note TEXT NOT NULL DEFAULT '',
            created_at TEXT NOT NULL
        )
    )";
    if (!query.exec(createReviewsSql)) {
        database.rollback();
        setError("Could not create spaced_reviews table: " + query.lastError().text());
        return false;
    }
    query.exec("CREATE INDEX IF NOT EXISTS idx_spaced_reviews_next ON spaced_reviews(completed, next_review_at)");

    // Create goals table
    const QString createGoalsSql = R"(
        CREATE TABLE IF NOT EXISTS goals (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            horizon TEXT NOT NULL CHECK(horizon IN ('day', 'week', 'month', 'year')),
            title TEXT NOT NULL,
            current_value INTEGER NOT NULL DEFAULT 0,
            target_value INTEGER NOT NULL DEFAULT 1,
            completed INTEGER NOT NULL DEFAULT 0,
            period_key TEXT NOT NULL DEFAULT '',
            sort_order INTEGER NOT NULL DEFAULT 0,
            created_at TEXT NOT NULL
        )
    )";
    if (!query.exec(createGoalsSql)) {
        database.rollback();
        setError("Could not create goals table: " + query.lastError().text());
        return false;
    }
    query.exec("CREATE INDEX IF NOT EXISTS idx_goals_horizon ON goals(horizon, completed)");

    query.prepare("INSERT OR REPLACE INTO schema_version(version, applied_at) VALUES(?, ?)");
    query.addBindValue(3);
    query.addBindValue(toDatabaseDateTime(QDateTime::currentDateTime()));
    if (!query.exec()) {
        database.rollback();
        setError("Could not record schema version 3: " + query.lastError().text());
        return false;
    }

    if (!database.commit()) {
        setError("Could not commit V3 migration: " + database.lastError().text());
        return false;
    }

    qInfo().noquote() << "StudyWidgets database migrated to schema version 3.";
    return true;
}

bool DataStore::migrateSchemaV3ToV4() {
    // Backup database before V4 migration
    QDir(m_appDataDir).mkpath("backups");
    const QString backupPath = QDir(m_appDataDir).filePath("backups/studywidgets-before-v4.sqlite");
    if (!QFile::exists(backupPath)) {
        QFile::copy(m_databasePath, backupPath);
    }

    QSqlDatabase database = QSqlDatabase::database(ConnectionName);
    if (!database.transaction()) {
        setError("Could not start V3->V4 migration transaction: " + database.lastError().text());
        return false;
    }

    QSqlQuery query(database);

    // Alter categories: short_name, tier, vault_name, done, course_limit
    query.exec("ALTER TABLE categories ADD COLUMN short_name TEXT NOT NULL DEFAULT ''");
    query.exec("ALTER TABLE categories ADD COLUMN tier TEXT NOT NULL DEFAULT 'F'");
    query.exec("ALTER TABLE categories ADD COLUMN vault_name TEXT NOT NULL DEFAULT ''");
    query.exec("ALTER TABLE categories ADD COLUMN done INTEGER NOT NULL DEFAULT 0");
    query.exec("ALTER TABLE categories ADD COLUMN course_limit INTEGER NOT NULL DEFAULT 0");

    // Populate initial default tier and short_name for known categories
    for (const DefaultCategory& cat : defaultCategories()) {
        query.prepare("UPDATE categories SET short_name = ?, tier = ?, vault_name = ? WHERE id = ?");
        query.addBindValue(cat.shortName);
        query.addBindValue(cat.tier);
        query.addBindValue(cat.vaultName);
        query.addBindValue(cat.id);
        query.exec();
    }

    // Alter study_sessions: source, logged_md
    query.exec("ALTER TABLE study_sessions ADD COLUMN source TEXT NOT NULL DEFAULT 'timer'");
    query.exec("ALTER TABLE study_sessions ADD COLUMN logged_md INTEGER NOT NULL DEFAULT 0");
    // Mark existing sessions as already logged to prevent re-dumping to TimeLog.md
    query.exec("UPDATE study_sessions SET logged_md = 1");

    query.prepare("INSERT OR REPLACE INTO schema_version(version, applied_at) VALUES(?, ?)");
    query.addBindValue(4);
    query.addBindValue(toDatabaseDateTime(QDateTime::currentDateTime()));
    if (!query.exec()) {
        database.rollback();
        setError("Could not record schema version 4: " + query.lastError().text());
        return false;
    }

    if (!database.commit()) {
        setError("Could not commit V4 migration: " + database.lastError().text());
        return false;
    }

    qInfo().noquote() << "StudyWidgets database migrated to schema version 4.";
    return true;
}

bool DataStore::migrateSchemaV4ToV5() {
    QDir(m_appDataDir).mkpath("backups");
    const QString backupPath = QDir(m_appDataDir).filePath("backups/studywidgets-before-v5.sqlite");
    if (!QFile::exists(backupPath)) {
        QFile::copy(m_databasePath, backupPath);
    }

    QSqlDatabase database = QSqlDatabase::database(ConnectionName);
    if (!database.transaction()) {
        setError("Could not start V4->V5 migration transaction: " + database.lastError().text());
        return false;
    }

    QSqlQuery query(database);
    const QString createEventsSql = R"(
        CREATE TABLE IF NOT EXISTS calendar_events (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            event_date TEXT NOT NULL,
            time_text TEXT NOT NULL DEFAULT '',
            title TEXT NOT NULL,
            tag TEXT NOT NULL DEFAULT 'general',
            completed INTEGER NOT NULL DEFAULT 0,
            created_at TEXT NOT NULL
        )
    )";
    if (!query.exec(createEventsSql)) {
        database.rollback();
        setError("Could not create calendar_events table: " + query.lastError().text());
        return false;
    }
    query.exec("CREATE INDEX IF NOT EXISTS idx_calendar_events_date ON calendar_events(event_date, completed)");

    query.prepare("INSERT OR REPLACE INTO schema_version(version, applied_at) VALUES(?, ?)");
    query.addBindValue(5);
    query.addBindValue(toDatabaseDateTime(QDateTime::currentDateTime()));
    if (!query.exec() || !database.commit()) {
        database.rollback();
        setError("Could not commit V5 migration: " + database.lastError().text());
        return false;
    }

    qInfo().noquote() << "StudyWidgets database migrated to schema version 5.";
    return true;
}

bool DataStore::migrateSchemaV5ToV6()
{
    QDir(m_appDataDir).mkpath("backups");
    const QString backupPath = QDir(m_appDataDir).filePath("backups/studywidgets-before-v6.sqlite");
    if (!QFile::exists(backupPath))
    {
        QFile::copy(m_databasePath, backupPath);
    }

    QSqlDatabase database = QSqlDatabase::database(ConnectionName);
    if (!database.transaction())
    {
        setError("Could not start V5->V6 migration transaction: " + database.lastError().text());
        return false;
    }

    QSqlQuery query(database);
    const QString createCachedNotesSql = R"(
        CREATE TABLE IF NOT EXISTS cached_notes (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            source TEXT NOT NULL,
            path TEXT NOT NULL,
            title TEXT NOT NULL DEFAULT '',
            content TEXT NOT NULL,
            content_sha256 TEXT NOT NULL,
            size INTEGER NOT NULL,
            cached_at TEXT NOT NULL,
            UNIQUE(source, path)
        )
    )";
    if (!query.exec(createCachedNotesSql))
    {
        database.rollback();
        setError("Could not create cached_notes table: " + query.lastError().text());
        return false;
    }

    query.exec("CREATE INDEX IF NOT EXISTS idx_cached_notes_source_path ON cached_notes(source, path)");
    query.exec("CREATE INDEX IF NOT EXISTS idx_cached_notes_cached_at ON cached_notes(cached_at)");

    query.prepare("INSERT OR REPLACE INTO schema_version(version, applied_at) VALUES(?, ?)");
    query.addBindValue(6);
    query.addBindValue(toDatabaseDateTime(QDateTime::currentDateTime()));
    if (!query.exec() || !database.commit())
    {
        database.rollback();
        setError("Could not commit V6 migration: " + database.lastError().text());
        return false;
    }

    qInfo().noquote() << "StudyWidgets database migrated to schema version 6.";
    return true;
}

bool DataStore::migrateSchemaV6ToV7()
{
    QDir(m_appDataDir).mkpath("backups");
    const QString backupPath = QDir(m_appDataDir).filePath("backups/studywidgets-before-v7.sqlite");
    if (!QFile::exists(backupPath))
    {
        QFile::copy(m_databasePath, backupPath);
    }

    QSqlDatabase database = QSqlDatabase::database(ConnectionName);
    if (!database.transaction())
    {
        setError("Could not start V6->V7 migration transaction: " + database.lastError().text());
        return false;
    }

    QSqlQuery query(database);
    const QString createNotesDraftSql = R"(
        CREATE TABLE IF NOT EXISTS notes_draft (
            id INTEGER PRIMARY KEY CHECK (id = 1),
            target TEXT NOT NULL DEFAULT 'inbox',
            text TEXT NOT NULL DEFAULT '',
            updated_at TEXT NOT NULL
        )
    )";
    if (!query.exec(createNotesDraftSql))
    {
        database.rollback();
        setError("Could not create notes_draft table: " + query.lastError().text());
        return false;
    }

    const QString createDayRulesSql = R"(
        CREATE TABLE IF NOT EXISTS day_rules (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            title TEXT NOT NULL,
            completed INTEGER NOT NULL DEFAULT 0,
            sort_order INTEGER NOT NULL DEFAULT 0,
            created_at TEXT NOT NULL
        )
    )";
    if (!query.exec(createDayRulesSql))
    {
        database.rollback();
        setError("Could not create day_rules table: " + query.lastError().text());
        return false;
    }

    query.exec("CREATE INDEX IF NOT EXISTS idx_day_rules_order ON day_rules(sort_order, id)");

    query.prepare("INSERT OR REPLACE INTO schema_version(version, applied_at) VALUES(?, ?)");
    query.addBindValue(7);
    query.addBindValue(toDatabaseDateTime(QDateTime::currentDateTime()));
    if (!query.exec() || !database.commit())
    {
        database.rollback();
        setError("Could not commit V7 migration: " + database.lastError().text());
        return false;
    }

    qInfo().noquote() << "StudyWidgets database migrated to schema version 7.";
    return true;
}

void DataStore::seedDefaultGoalsAndReviewsIfEmpty()
{
    QSqlDatabase database = QSqlDatabase::database(ConnectionName);
    QSqlQuery query(database);
    const LocalSeedConfig& seed = CategoryConfigLoader::seedConfig();

    if (query.exec("SELECT COUNT(*) FROM goals") && query.next() && query.value(0).toInt() == 0)
    {
        if (!seed.goals.isEmpty())
        {
            for (const LocalGoalSeed& g : seed.goals)
            {
                addGoal(g.horizon, g.title, g.targetValue, g.currentValue);
            }
        }
        else
        {
            addGoal("day", "2×45m Спринт по приоритетной задаче", 2, 0);
            addGoal("week", "Закрыть все практики и конспекты недели", 4, 0);
            addGoal("month", "Продвинуть основной технический проект", 8, 0);
            addGoal("year", "Выполнить годовой учебный план", 30, 0);
        }
    }

    if (query.exec("SELECT COUNT(*) FROM spaced_reviews") && query.next() && query.value(0).toInt() == 0)
    {
        if (!seed.spacedReviews.isEmpty())
        {
            for (const LocalSpacedReviewSeed& r : seed.spacedReviews)
            {
                addSpacedReview(r.categoryId, r.topic, r.kind);
            }
        }
        else
        {
            addSpacedReview("subject-example-1", "Пример повторения: ключевые понятия и практика", "cviceni");
            addSpacedReview("subject-example-2", "Пример повторения: разбор базовой темы", "prednaska");
        }
    }

    if (query.exec("SELECT COUNT(*) FROM calendar_events") && query.next() && query.value(0).toInt() == 0)
    {
        const QDate today = QDate::currentDate();
        if (!seed.calendarEvents.isEmpty())
        {
            for (const LocalCalendarEventSeed& ev : seed.calendarEvents)
            {
                addCalendarEvent(today.addDays(ev.dayOffset), ev.title, ev.timeText, ev.tag);
            }
        }
        else
        {
            addCalendarEvent(today, "Фокус-спринт по предмету 1", "16:30", "study");
            addCalendarEvent(today.addDays(1), "Подготовка к практическому занятию", "09:00", "deadline");
        }
    }
}

bool DataStore::addStudySession(const StudySession& session) {
    if (!m_isOpen && !open()) {
        return false;
    }

    const QString category = session.category.trimmed();
    const QString categoryId = session.categoryId.trimmed();

    if (categoryId.isEmpty()) {
        setError("Study session category ID cannot be empty.");
        return false;
    }

    if (!session.startedAt.isValid() || !session.finishedAt.isValid()) {
        setError("Study session start and finish times must be valid.");
        return false;
    }

    QSqlDatabase database = QSqlDatabase::database(ConnectionName);
    QSqlQuery query(database);

    query.prepare(R"(
        INSERT INTO study_sessions (
            category,
            category_id,
            started_at,
            finished_at,
            duration_seconds,
            completed,
            note,
            source,
            logged_md,
            created_at
        )
        VALUES (?, ?, ?, ?, ?, ?, COALESCE(?, ''), ?, ?, ?)
    )");

    const QString safeSessionNote = session.note.trimmed();
    query.addBindValue(category.isEmpty() ? QStringLiteral("Free Study") : category);
    query.addBindValue(categoryId);
    query.addBindValue(toDatabaseDateTime(session.startedAt));
    query.addBindValue(toDatabaseDateTime(session.finishedAt));
    query.addBindValue(session.durationSeconds);
    query.addBindValue(session.completed ? 1 : 0);
    query.addBindValue(safeSessionNote.isEmpty() ? QStringLiteral("") : safeSessionNote);
    query.addBindValue(session.source.isEmpty() ? QStringLiteral("timer") : session.source);
    query.addBindValue(session.loggedMd ? 1 : 0);
    query.addBindValue(toDatabaseDateTime(QDateTime::currentDateTime()));

    if (!query.exec()) {
        setError("Could not save study session: " + query.lastError().text());
        return false;
    }

    exportDeaveBridgeSnapshot();
    return true;
}

bool DataStore::addManualStudyMinutes(
    const QString& categoryId,
    const QString& categoryName,
    int deltaMinutes,
    const QString& note
) {
    if (deltaMinutes == 0) {
        return true;
    }

    const int deltaSeconds = deltaMinutes * 60;
    const int currentCatSeconds = totalStudySecondsForCategory(categoryId);
    int actualDeltaSeconds = deltaSeconds;
    if (currentCatSeconds + actualDeltaSeconds < 0) {
        actualDeltaSeconds = -currentCatSeconds;
    }
    if (actualDeltaSeconds == 0) {
        return true;
    }

    StudySession session;
    session.categoryId = categoryId;
    session.category = categoryName;
    session.startedAt = QDateTime::currentDateTime();
    session.finishedAt = session.startedAt;
    session.durationSeconds = actualDeltaSeconds;
    session.completed = true;
    session.note = note;
    session.source = QStringLiteral("manual");
    session.loggedMd = false;

    return addStudySession(session);
}

QList<StudySession> DataStore::recentStudySessions(int limit) {
    QList<StudySession> sessions;
    if (!m_isOpen && !open()) return sessions;
    if (limit < 1) return sessions;

    QSqlDatabase database = QSqlDatabase::database(ConnectionName);
    QSqlQuery query(database);

    query.prepare(R"(
        SELECT id, category, category_id, started_at, finished_at, duration_seconds, completed, note, source, logged_md
        FROM study_sessions
        ORDER BY started_at DESC
        LIMIT ?
    )");
    query.addBindValue(limit);

    if (!query.exec()) {
        setError("Could not load study sessions: " + query.lastError().text());
        return sessions;
    }

    while (query.next()) {
        StudySession session;
        session.id = query.value(0).toLongLong();
        session.category = query.value(1).toString();
        session.categoryId = query.value(2).toString();
        session.startedAt = fromDatabaseDateTime(query.value(3).toString());
        session.finishedAt = fromDatabaseDateTime(query.value(4).toString());
        session.durationSeconds = query.value(5).toInt();
        session.completed = query.value(6).toBool();
        session.note = query.value(7).toString();
        session.source = query.value(8).toString();
        session.loggedMd = query.value(9).toBool();
        sessions.append(session);
    }

    return sessions;
}

StudySession DataStore::lastSession() {
    auto list = recentStudySessions(1);
    if (!list.isEmpty()) {
        return list.first();
    }
    return StudySession();
}

bool DataStore::undoLastSession() {
    if (!m_isOpen && !open()) return false;
    QSqlDatabase database = QSqlDatabase::database(ConnectionName);
    QSqlQuery query(database);
    if (!query.exec("SELECT id FROM study_sessions ORDER BY id DESC LIMIT 1") || !query.next()) {
        return false;
    }
    const qint64 lastId = query.value(0).toLongLong();
    query.prepare("DELETE FROM study_sessions WHERE id = ?");
    query.addBindValue(lastId);
    bool ok = query.exec();
    if (ok) {
        exportDeaveBridgeSnapshot();
    }
    return ok;
}

int DataStore::sessionsTodayCount() {
    if (!m_isOpen && !open()) return 0;
    const QDateTime dayStart = QDate::currentDate().startOfDay();
    const QDateTime nextDayStart = QDate::currentDate().addDays(1).startOfDay();
    QSqlDatabase database = QSqlDatabase::database(ConnectionName);
    QSqlQuery query(database);
    query.prepare(R"(
        SELECT COUNT(*)
        FROM study_sessions
        WHERE started_at >= ? AND started_at < ? AND completed = 1 AND duration_seconds > 0
    )");
    query.addBindValue(toDatabaseDateTime(dayStart));
    query.addBindValue(toDatabaseDateTime(nextDayStart));
    if (query.exec() && query.next()) {
        return query.value(0).toInt();
    }
    return 0;
}

QList<StudySession> DataStore::unloggedSessions() {
    QList<StudySession> sessions;
    if (!m_isOpen && !open()) return sessions;
    QSqlDatabase database = QSqlDatabase::database(ConnectionName);
    QSqlQuery query(database);
    if (!query.exec(R"(
        SELECT id, category, category_id, started_at, finished_at, duration_seconds, completed, note, source, logged_md
        FROM study_sessions
        WHERE logged_md = 0 AND duration_seconds > 0
        ORDER BY started_at ASC
    )")) {
        return sessions;
    }
    while (query.next()) {
        StudySession s;
        s.id = query.value(0).toLongLong();
        s.category = query.value(1).toString();
        s.categoryId = query.value(2).toString();
        s.startedAt = fromDatabaseDateTime(query.value(3).toString());
        s.finishedAt = fromDatabaseDateTime(query.value(4).toString());
        s.durationSeconds = query.value(5).toInt();
        s.completed = query.value(6).toBool();
        s.note = query.value(7).toString();
        s.source = query.value(8).toString();
        s.loggedMd = query.value(9).toBool();
        sessions.append(s);
    }
    return sessions;
}

bool DataStore::markSessionLogged(qint64 sessionId) {
    if (!m_isOpen && !open()) return false;
    QSqlDatabase database = QSqlDatabase::database(ConnectionName);
    QSqlQuery query(database);
    query.prepare("UPDATE study_sessions SET logged_md = 1 WHERE id = ?");
    query.addBindValue(sessionId);
    return query.exec();
}

bool DataStore::syncStudyCategories(const QList<StudyCategory>& categories) {
    if (!m_isOpen && !open()) return false;
    if (categories.isEmpty()) {
        setError("Category configuration must contain at least one category.");
        return false;
    }

    QSet<QString> knownIds;
    for (const StudyCategory& category : categories) {
        const QString id = category.id.trimmed();
        const QString name = category.name.trimmed();
        if (id.isEmpty() || name.isEmpty()) {
            setError("Category ID and name cannot be empty.");
            return false;
        }
        if (knownIds.contains(id)) {
            setError("Category IDs must be unique.");
            return false;
        }
        knownIds.insert(id);
    }

    if (!knownIds.contains("free-study")) {
        setError("Category configuration must contain free-study.");
        return false;
    }

    QSqlDatabase database = QSqlDatabase::database(ConnectionName);
    if (!database.transaction()) {
        setError("Could not start category synchronization transaction: " + database.lastError().text());
        return false;
    }

    QSqlQuery query(database);
    query.prepare(R"(
        INSERT INTO categories (
            id, name, short_name, type, color, tier, vault_name, done, course_limit, archived, sort_order, target_minutes, created_at
        )
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        ON CONFLICT(id) DO UPDATE SET
            name = excluded.name,
            short_name = excluded.short_name,
            type = excluded.type,
            color = excluded.color,
            tier = excluded.tier,
            vault_name = excluded.vault_name,
            done = excluded.done,
            course_limit = excluded.course_limit,
            archived = excluded.archived,
            sort_order = excluded.sort_order,
            target_minutes = excluded.target_minutes
    )");

    for (const StudyCategory& category : categories) {
        query.bindValue(0, category.id.trimmed());
        query.bindValue(1, category.name.trimmed());
        query.bindValue(2, category.shortName.trimmed());
        query.bindValue(3, category.type);
        query.bindValue(4, category.color);
        query.bindValue(5, category.tier.trimmed().isEmpty() ? QStringLiteral("F") : category.tier.trimmed());
        query.bindValue(6, category.vaultName.trimmed());
        query.bindValue(7, category.done ? 1 : 0);
        query.bindValue(8, category.courseLimit);
        query.bindValue(9, category.archived ? 1 : 0);
        query.bindValue(10, category.sortOrder);
        query.bindValue(11, category.targetMinutes > 0 ? category.targetMinutes : 1500);
        query.bindValue(12, toDatabaseDateTime(QDateTime::currentDateTime()));

        if (!query.exec()) {
            database.rollback();
            setError("Could not save category configuration: " + query.lastError().text());
            return false;
        }
    }

    QStringList placeholders;
    for (int index = 0; index < categories.size(); ++index) {
        placeholders.append("?");
    }

    QSqlQuery archiveQuery(database);
    archiveQuery.prepare(
        "UPDATE categories SET archived = 1 WHERE id NOT IN (" + placeholders.join(", ") + ")"
    );
    for (const StudyCategory& category : categories) {
        archiveQuery.addBindValue(category.id.trimmed());
    }

    if (!archiveQuery.exec() || !database.commit()) {
        database.rollback();
        setError("Could not commit category synchronization.");
        return false;
    }

    exportDeaveBridgeSnapshot();
    return true;
}

QList<StudyCategory> DataStore::studyCategories(bool includeArchived) {
    QList<StudyCategory> categories;
    if (!m_isOpen && !open()) return categories;

    QSqlDatabase database = QSqlDatabase::database(ConnectionName);
    QSqlQuery query(database);

    QString sql = R"(
        SELECT id, name, short_name, type, color, tier, vault_name, done, course_limit, archived, sort_order, COALESCE(target_minutes, 1500)
        FROM categories
    )";
    if (!includeArchived) {
        sql += " WHERE archived = 0";
    }
    sql += " ORDER BY sort_order ASC, name ASC";

    if (!query.exec(sql)) {
        setError("Could not load study categories: " + query.lastError().text());
        return categories;
    }

    while (query.next()) {
        StudyCategory category;
        category.id = query.value(0).toString();
        category.name = query.value(1).toString();
        category.shortName = query.value(2).toString();
        category.type = query.value(3).toString();
        category.color = query.value(4).toString();
        category.tier = query.value(5).toString();
        category.vaultName = query.value(6).toString();
        category.done = query.value(7).toBool();
        category.courseLimit = query.value(8).toInt();
        category.archived = query.value(9).toBool();
        category.sortOrder = query.value(10).toInt();
        category.targetMinutes = query.value(11).toInt();
        categories.append(category);
    }

    return categories;
}

StudyCategory DataStore::categoryById(const QString& categoryId) {
    StudyCategory category;
    if (!m_isOpen && !open()) return category;

    QSqlDatabase database = QSqlDatabase::database(ConnectionName);
    QSqlQuery query(database);
    query.prepare(R"(
        SELECT id, name, short_name, type, color, tier, vault_name, done, course_limit, archived, sort_order, COALESCE(target_minutes, 1500)
        FROM categories
        WHERE id = ?
        LIMIT 1
    )");
    query.addBindValue(categoryId.trimmed());

    if (query.exec() && query.next()) {
        category.id = query.value(0).toString();
        category.name = query.value(1).toString();
        category.shortName = query.value(2).toString();
        category.type = query.value(3).toString();
        category.color = query.value(4).toString();
        category.tier = query.value(5).toString();
        category.vaultName = query.value(6).toString();
        category.done = query.value(7).toBool();
        category.courseLimit = query.value(8).toInt();
        category.archived = query.value(9).toBool();
        category.sortOrder = query.value(10).toInt();
        category.targetMinutes = query.value(11).toInt();
    }
    return category;
}

bool DataStore::updateCategoryDone(const QString& categoryId, bool done) {
    if (!m_isOpen && !open()) return false;
    QSqlDatabase database = QSqlDatabase::database(ConnectionName);
    QSqlQuery query(database);
    query.prepare("UPDATE categories SET done = ? WHERE id = ?");
    query.addBindValue(done ? 1 : 0);
    query.addBindValue(categoryId.trimmed());
    bool ok = query.exec();
    if (ok) {
        exportDeaveBridgeSnapshot();
    }
    return ok;
}

int DataStore::totalStudySecondsForDate(const QDate& date) {
    if (!m_isOpen && !open()) return 0;
    if (!date.isValid()) return 0;

    const QDateTime dayStart = date.startOfDay();
    const QDateTime nextDayStart = date.addDays(1).startOfDay();

    QSqlDatabase database = QSqlDatabase::database(ConnectionName);
    QSqlQuery query(database);
    query.prepare(R"(
        SELECT COALESCE(SUM(duration_seconds), 0)
        FROM study_sessions
        WHERE started_at >= ? AND started_at < ?
    )");
    query.addBindValue(toDatabaseDateTime(dayStart));
    query.addBindValue(toDatabaseDateTime(nextDayStart));

    if (!query.exec() || !query.next()) return 0;
    return qMax(0, query.value(0).toInt());
}

int DataStore::totalStudySecondsForCategory(const QString& categoryId, const QDateTime& since) {
    if (!m_isOpen && !open()) return 0;

    QSqlDatabase database = QSqlDatabase::database(ConnectionName);
    QSqlQuery query(database);
    if (since.isValid()) {
        query.prepare(R"(
            SELECT COALESCE(SUM(duration_seconds), 0)
            FROM study_sessions
            WHERE category_id = ? AND started_at >= ?
        )");
        query.addBindValue(categoryId.trimmed());
        query.addBindValue(toDatabaseDateTime(since));
    } else {
        query.prepare(R"(
            SELECT COALESCE(SUM(duration_seconds), 0)
            FROM study_sessions
            WHERE category_id = ?
        )");
        query.addBindValue(categoryId.trimmed());
    }

    if (!query.exec() || !query.next()) return 0;
    return qMax(0, query.value(0).toInt());
}

int DataStore::tierAStudyPercentForDate(const QDate& date) {
    const int totalSecs = totalStudySecondsForDate(date);
    if (totalSecs <= 0) return 0;

    const QDateTime dayStart = date.startOfDay();
    const QDateTime nextDayStart = date.addDays(1).startOfDay();

    QSqlDatabase database = QSqlDatabase::database(ConnectionName);
    QSqlQuery query(database);
    query.prepare(R"(
        SELECT COALESCE(SUM(s.duration_seconds), 0)
        FROM study_sessions s
        LEFT JOIN categories c ON c.id = s.category_id
        WHERE s.started_at >= ? AND s.started_at < ?
          AND (UPPER(c.tier) = 'A')
    )");
    query.addBindValue(toDatabaseDateTime(dayStart));
    query.addBindValue(toDatabaseDateTime(nextDayStart));

    if (!query.exec() || !query.next()) return 0;
    const int tierASecs = qMax(0, query.value(0).toInt());
    return qBound(0, static_cast<int>((tierASecs * 100LL) / totalSecs), 100);
}

QList<SpacedReviewItem> DataStore::spacedReviews(bool includeCompleted) {
    QList<SpacedReviewItem> items;
    if (!m_isOpen && !open()) return items;

    QSqlDatabase database = QSqlDatabase::database(ConnectionName);
    QSqlQuery query(database);

    QString sql = R"(
        SELECT r.id, r.category_id, COALESCE(c.name, r.category_id), r.topic, r.kind,
               r.stage, r.next_review_at, r.last_reviewed_at, r.completed, r.note
        FROM spaced_reviews r
        LEFT JOIN categories c ON c.id = r.category_id
    )";
    if (!includeCompleted) {
        sql += " WHERE r.completed = 0";
    }
    sql += " ORDER BY r.next_review_at ASC";

    if (!query.exec(sql)) {
        setError("Could not load spaced reviews: " + query.lastError().text());
        return items;
    }

    while (query.next()) {
        SpacedReviewItem item;
        item.id = query.value(0).toLongLong();
        item.categoryId = query.value(1).toString();
        item.categoryName = query.value(2).toString();
        item.topic = query.value(3).toString();
        item.kind = query.value(4).toString();
        item.stage = query.value(5).toInt();
        item.nextReviewAt = fromDatabaseDateTime(query.value(6).toString());
        item.lastReviewedAt = fromDatabaseDateTime(query.value(7).toString());
        item.completed = query.value(8).toBool();
        item.note = query.value(9).toString();
        items.append(item);
    }

    return items;
}

bool DataStore::addSpacedReview(
    const QString& categoryId,
    const QString& topic,
    const QString& kind,
    const QString& note
) {
    if (!m_isOpen && !open()) return false;
    const QString trimmedTopic = topic.trimmed();
    if (trimmedTopic.isEmpty()) return false;

    const QDateTime now = QDateTime::currentDateTime();
    const QDateTime nextReview = now.addSecs(intervalSecondsForStage(0)); // 30 minutes first consolidation!

    QSqlDatabase database = QSqlDatabase::database(ConnectionName);
    QSqlQuery query(database);
    query.prepare(R"(
        INSERT INTO spaced_reviews (
            category_id, topic, kind, stage, next_review_at, last_reviewed_at, completed, note, created_at
        )
        VALUES (?, ?, ?, 0, ?, ?, 0, COALESCE(?, ''), ?)
    )");
    const QString safeNote = note.trimmed();
    query.addBindValue(categoryId.isEmpty() ? QStringLiteral("free-study") : categoryId);
    query.addBindValue(trimmedTopic);
    query.addBindValue(kind.isEmpty() ? QStringLiteral("cviceni") : kind);
    query.addBindValue(toDatabaseDateTime(nextReview));
    query.addBindValue(toDatabaseDateTime(now));
    query.addBindValue(safeNote.isEmpty() ? QStringLiteral("") : safeNote);
    query.addBindValue(toDatabaseDateTime(now));

    if (!query.exec()) {
        setError("Could not add spaced review: " + query.lastError().text());
        return false;
    }

    exportDeaveBridgeSnapshot();
    return true;
}

bool DataStore::advanceSpacedReview(qint64 id, bool resetTo30Min) {
    if (!m_isOpen && !open()) return false;

    QSqlDatabase database = QSqlDatabase::database(ConnectionName);
    QSqlQuery getQ(database);
    getQ.prepare("SELECT stage FROM spaced_reviews WHERE id = ?");
    getQ.addBindValue(id);
    if (!getQ.exec() || !getQ.next()) return false;

    const int currentStage = getQ.value(0).toInt();
    const int nextStage = resetTo30Min ? 0 : (currentStage + 1);
    const bool completed = (!resetTo30Min && nextStage > 6);
    const QDateTime now = QDateTime::currentDateTime();
    const QDateTime nextReview = now.addSecs(intervalSecondsForStage(qMin(nextStage, 6)));

    QSqlQuery updQ(database);
    updQ.prepare(R"(
        UPDATE spaced_reviews
        SET stage = ?, next_review_at = ?, last_reviewed_at = ?, completed = ?
        WHERE id = ?
    )");
    updQ.addBindValue(qMin(nextStage, 6));
    updQ.addBindValue(toDatabaseDateTime(nextReview));
    updQ.addBindValue(toDatabaseDateTime(now));
    updQ.addBindValue(completed ? 1 : 0);
    updQ.addBindValue(id);

    if (!updQ.exec()) return false;
    exportDeaveBridgeSnapshot();
    return true;
}

bool DataStore::removeSpacedReview(qint64 id) {
    if (!m_isOpen && !open()) return false;
    QSqlDatabase database = QSqlDatabase::database(ConnectionName);
    QSqlQuery query(database);
    query.prepare("DELETE FROM spaced_reviews WHERE id = ?");
    query.addBindValue(id);
    const bool ok = query.exec();
    exportDeaveBridgeSnapshot();
    return ok;
}

QList<GoalItem> DataStore::goalsByHorizon(const QString& horizon) {
    QList<GoalItem> list;
    if (!m_isOpen && !open()) return list;

    if (horizon == QStringLiteral("day") && !m_syncing && m_lastSyncedDate != QDate::currentDate()) {
        syncDailyTasksWithCalendarAndRoadmaps(QDate::currentDate());
    }

    QSqlDatabase database = QSqlDatabase::database(ConnectionName);
    QSqlQuery query(database);
    query.prepare(R"(
        SELECT id, horizon, title, current_value, target_value, completed, period_key, sort_order
        FROM goals
        WHERE horizon = ?
        ORDER BY completed ASC, sort_order ASC, id ASC
    )");
    query.addBindValue(horizon);

    if (!query.exec()) return list;
    while (query.next()) {
        GoalItem item;
        item.id = query.value(0).toLongLong();
        item.horizon = query.value(1).toString();
        item.title = query.value(2).toString();
        item.currentValue = query.value(3).toInt();
        item.targetValue = qMax(1, query.value(4).toInt());
        item.completed = query.value(5).toBool();
        item.periodKey = query.value(6).toString();
        item.sortOrder = query.value(7).toInt();
        list.append(item);
    }
    return list;
}

bool DataStore::addGoal(
    const QString& horizon,
    const QString& title,
    int targetValue,
    int currentValue
) {
    if (!m_isOpen && !open()) return false;
    const QString cleanTitle = title.trimmed();
    if (cleanTitle.isEmpty()) return false;

    const int tgt = qMax(1, targetValue);
    const int cur = qBound(0, currentValue, tgt);
    const QString todayIso = QDate::currentDate().toString(Qt::ISODate);

    QSqlDatabase database = QSqlDatabase::database(ConnectionName);
    QSqlQuery query(database);
    query.prepare(R"(
        INSERT INTO goals (horizon, title, current_value, target_value, completed, period_key, sort_order, created_at)
        VALUES (?, ?, ?, ?, ?, ?, 0, ?)
    )");
    query.addBindValue(horizon);
    query.addBindValue(cleanTitle);
    query.addBindValue(cur);
    query.addBindValue(tgt);
    query.addBindValue(cur >= tgt ? 1 : 0);
    query.addBindValue(todayIso);
    query.addBindValue(toDatabaseDateTime(QDateTime::currentDateTime()));

    if (!query.exec()) return false;

    // Auto-mirror daily tasks into today's CalendarWidget
    if (!m_syncing && horizon == QStringLiteral("day")) {
        m_syncing = true;
        QSqlQuery checkEv(database);
        checkEv.prepare("SELECT id FROM calendar_events WHERE event_date = ? AND title = ? LIMIT 1");
        checkEv.addBindValue(todayIso);
        checkEv.addBindValue(cleanTitle);
        if (checkEv.exec() && !checkEv.next()) {
            QSqlQuery insEv(database);
            insEv.prepare(R"(
                INSERT INTO calendar_events (event_date, time_text, title, tag, completed, created_at)
                VALUES (?, '', ?, 'general', ?, ?)
            )");
            insEv.addBindValue(todayIso);
            insEv.addBindValue(cleanTitle);
            insEv.addBindValue(cur >= tgt ? 1 : 0);
            insEv.addBindValue(toDatabaseDateTime(QDateTime::currentDateTime()));
            insEv.exec();
        }
        m_syncing = false;
    }

    exportDeaveBridgeSnapshot();
    if (!m_syncing) {
        notifyListeners();
    }
    return true;
}

bool DataStore::toggleGoal(qint64 id, bool completed) {
    if (!m_isOpen && !open()) return false;
    QSqlDatabase database = QSqlDatabase::database(ConnectionName);

    QString horizon;
    QString goalTitle;
    {
        QSqlQuery infoQ(database);
        infoQ.prepare("SELECT horizon, title FROM goals WHERE id = ?");
        infoQ.addBindValue(id);
        if (infoQ.exec() && infoQ.next()) {
            horizon = infoQ.value(0).toString();
            goalTitle = infoQ.value(1).toString();
        }
    }

    QSqlQuery query(database);
    query.prepare(R"(
        UPDATE goals
        SET completed = ?,
            current_value = CASE WHEN ? = 1 THEN target_value ELSE 0 END
        WHERE id = ?
    )");
    query.addBindValue(completed ? 1 : 0);
    query.addBindValue(completed ? 1 : 0);
    query.addBindValue(id);
    const bool ok = query.exec();

    if (ok && !m_syncing && !goalTitle.isEmpty()) {
        m_syncing = true;
        if (horizon == QStringLiteral("day")) {
            QSqlQuery syncCal(database);
            syncCal.prepare("UPDATE calendar_events SET completed = ? WHERE event_date = ? AND title = ?");
            syncCal.addBindValue(completed ? 1 : 0);
            syncCal.addBindValue(QDate::currentDate().toString(Qt::ISODate));
            syncCal.addBindValue(goalTitle);
            syncCal.exec();
        }
        const QString rdPrefix = CategoryConfigLoader::seedConfig().roadmapTaskPrefix;
        if ((!rdPrefix.isEmpty() && goalTitle.startsWith(rdPrefix))
            || goalTitle.startsWith(QString::fromUtf8("🗺️ [Roadmap")))
        {
            markRoadmapTaskCompleted(goalTitle, completed);
        }
        m_syncing = false;
        notifyListeners();
    }

    exportDeaveBridgeSnapshot();
    return ok;
}

bool DataStore::adjustGoalProgress(qint64 id, int delta) {
    if (!m_isOpen && !open()) return false;
    QSqlDatabase database = QSqlDatabase::database(ConnectionName);
    QSqlQuery getQ(database);
    getQ.prepare("SELECT current_value, target_value, horizon, title FROM goals WHERE id = ?");
    getQ.addBindValue(id);
    if (!getQ.exec() || !getQ.next()) return false;

    const int cur = getQ.value(0).toInt();
    const int tgt = qMax(1, getQ.value(1).toInt());
    const QString horizon = getQ.value(2).toString();
    const QString goalTitle = getQ.value(3).toString();
    const int nextVal = qBound(0, cur + delta, tgt);
    const bool done = (nextVal >= tgt);

    QSqlQuery updQ(database);
    updQ.prepare("UPDATE goals SET current_value = ?, completed = ? WHERE id = ?");
    updQ.addBindValue(nextVal);
    updQ.addBindValue(done ? 1 : 0);
    updQ.addBindValue(id);
    const bool ok = updQ.exec();

    if (ok && !m_syncing && horizon == QStringLiteral("day") && !goalTitle.isEmpty()) {
        m_syncing = true;
        QSqlQuery syncCal(database);
        syncCal.prepare("UPDATE calendar_events SET completed = ? WHERE event_date = ? AND title = ?");
        syncCal.addBindValue(done ? 1 : 0);
        syncCal.addBindValue(QDate::currentDate().toString(Qt::ISODate));
        syncCal.addBindValue(goalTitle);
        syncCal.exec();
        m_syncing = false;
        notifyListeners();
    }

    exportDeaveBridgeSnapshot();
    return ok;
}

bool DataStore::removeGoal(qint64 id) {
    if (!m_isOpen && !open()) return false;
    QSqlDatabase database = QSqlDatabase::database(ConnectionName);
    QSqlQuery query(database);
    query.prepare("DELETE FROM goals WHERE id = ?");
    query.addBindValue(id);
    const bool ok = query.exec();
    exportDeaveBridgeSnapshot();
    if (ok && !m_syncing) {
        notifyListeners();
    }
    return ok;
}

bool DataStore::clearCompletedGoals(const QString& horizon) {
    if (!m_isOpen && !open()) return false;
    QSqlDatabase database = QSqlDatabase::database(ConnectionName);
    QSqlQuery query(database);
    query.prepare("DELETE FROM goals WHERE horizon = ? AND completed = 1");
    query.addBindValue(horizon);
    const bool ok = query.exec();
    exportDeaveBridgeSnapshot();
    if (ok && !m_syncing) {
        notifyListeners();
    }
    return ok;
}

QList<CalendarEvent> DataStore::calendarEventsBetween(const QDate& startDate, const QDate& endDate) {
    QList<CalendarEvent> list;
    if (!m_isOpen && !open()) return list;
    if (!startDate.isValid() || !endDate.isValid()) return list;

    const QDate today = QDate::currentDate();
    if (!m_syncing && m_lastSyncedDate != today && startDate <= today && today <= endDate) {
        syncDailyTasksWithCalendarAndRoadmaps(today);
    }

    QSqlDatabase database = QSqlDatabase::database(ConnectionName);
    QSqlQuery query(database);
    query.prepare(R"(
        SELECT id, event_date, time_text, title, tag, completed
        FROM calendar_events
        WHERE event_date >= ? AND event_date <= ?
        ORDER BY event_date ASC, completed ASC,
                 CASE WHEN time_text = '' THEN '99:99' ELSE time_text END ASC, id ASC
    )");
    query.addBindValue(startDate.toString(Qt::ISODate));
    query.addBindValue(endDate.toString(Qt::ISODate));

    if (!query.exec()) return list;
    while (query.next()) {
        CalendarEvent ev;
        ev.id = query.value(0).toLongLong();
        ev.date = QDate::fromString(query.value(1).toString(), Qt::ISODate);
        ev.timeText = query.value(2).toString();
        ev.title = query.value(3).toString();
        ev.tag = query.value(4).toString();
        ev.completed = query.value(5).toBool();
        list.append(ev);
    }
    return list;
}

bool DataStore::addCalendarEvent(
    const QDate& date,
    const QString& title,
    const QString& timeText,
    const QString& tag
) {
    if (!m_isOpen && !open()) return false;
    if (!date.isValid()) return false;
    const QString cleanTitle = title.trimmed();
    if (cleanTitle.isEmpty()) return false;

    QSqlDatabase database = QSqlDatabase::database(ConnectionName);
    QSqlQuery query(database);
    query.prepare(R"(
        INSERT INTO calendar_events (event_date, time_text, title, tag, completed, created_at)
        VALUES (?, ?, ?, ?, 0, ?)
    )");
    query.addBindValue(date.toString(Qt::ISODate));
    query.addBindValue(timeText.trimmed());
    query.addBindValue(cleanTitle);
    query.addBindValue(tag.trimmed().isEmpty() ? QStringLiteral("general") : tag.trimmed());
    query.addBindValue(toDatabaseDateTime(QDateTime::currentDateTime()));

    if (!query.exec()) return false;

    // Auto-mirror today's calendar events into GoalsWidget ("day")
    if (!m_syncing && date == QDate::currentDate()) {
        m_syncing = true;
        QSqlQuery checkGoal(database);
        checkGoal.prepare("SELECT id FROM goals WHERE horizon = 'day' AND title = ? LIMIT 1");
        checkGoal.addBindValue(cleanTitle);
        if (checkGoal.exec() && !checkGoal.next()) {
            QSqlQuery insGoal(database);
            insGoal.prepare(R"(
                INSERT INTO goals (horizon, title, current_value, target_value, completed, period_key, sort_order, created_at)
                VALUES ('day', ?, 0, 1, 0, ?, 15, ?)
            )");
            insGoal.addBindValue(cleanTitle);
            insGoal.addBindValue(date.toString(Qt::ISODate));
            insGoal.addBindValue(toDatabaseDateTime(QDateTime::currentDateTime()));
            insGoal.exec();
        }
        m_syncing = false;
    }

    exportDeaveBridgeSnapshot();
    if (!m_syncing) {
        notifyListeners();
    }
    return true;
}

bool DataStore::toggleCalendarEvent(qint64 id, bool completed) {
    if (!m_isOpen && !open()) return false;
    QSqlDatabase database = QSqlDatabase::database(ConnectionName);

    QDate evDate;
    QString evTitle;
    {
        QSqlQuery infoQ(database);
        infoQ.prepare("SELECT event_date, title FROM calendar_events WHERE id = ?");
        infoQ.addBindValue(id);
        if (infoQ.exec() && infoQ.next()) {
            evDate = QDate::fromString(infoQ.value(0).toString(), Qt::ISODate);
            evTitle = infoQ.value(1).toString();
        }
    }

    QSqlQuery query(database);
    query.prepare("UPDATE calendar_events SET completed = ? WHERE id = ?");
    query.addBindValue(completed ? 1 : 0);
    query.addBindValue(id);
    const bool ok = query.exec();

    if (ok && !m_syncing && !evTitle.isEmpty()) {
        m_syncing = true;
        if (evDate == QDate::currentDate()) {
            QSqlQuery syncGoal(database);
            syncGoal.prepare(R"(
                UPDATE goals
                SET completed = ?,
                    current_value = CASE WHEN ? = 1 THEN target_value ELSE 0 END
                WHERE horizon = 'day' AND title = ?
            )");
            syncGoal.addBindValue(completed ? 1 : 0);
            syncGoal.addBindValue(completed ? 1 : 0);
            syncGoal.addBindValue(evTitle);
            syncGoal.exec();
        }
        const QString rdPrefix = CategoryConfigLoader::seedConfig().roadmapTaskPrefix;
        if ((!rdPrefix.isEmpty() && evTitle.startsWith(rdPrefix))
            || evTitle.startsWith(QString::fromUtf8("🗺️ [Roadmap")))
        {
            markRoadmapTaskCompleted(evTitle, completed);
        }
        m_syncing = false;
        notifyListeners();
    }

    exportDeaveBridgeSnapshot();
    return ok;
}

bool DataStore::removeCalendarEvent(qint64 id) {
    if (!m_isOpen && !open()) return false;
    QSqlDatabase database = QSqlDatabase::database(ConnectionName);
    QSqlQuery query(database);
    query.prepare("DELETE FROM calendar_events WHERE id = ?");
    query.addBindValue(id);
    const bool ok = query.exec();
    exportDeaveBridgeSnapshot();
    if (ok && !m_syncing) {
        notifyListeners();
    }
    return ok;
}

bool DataStore::clearCompletedCalendarEvents() {
    if (!m_isOpen && !open()) return false;
    QSqlDatabase database = QSqlDatabase::database(ConnectionName);
    QSqlQuery query(database);
    const bool ok = query.exec("DELETE FROM calendar_events WHERE completed = 1");
    exportDeaveBridgeSnapshot();
    if (ok && !m_syncing) {
        notifyListeners();
    }
    return ok;
}

QSet<QDate> DataStore::activeStudyDatesBetween(const QDate& startDate, const QDate& endDate) {
    QSet<QDate> dates;
    if (!m_isOpen && !open()) return dates;
    if (!startDate.isValid() || !endDate.isValid()) return dates;

    QSqlDatabase database = QSqlDatabase::database(ConnectionName);
    QSqlQuery query(database);
    query.prepare(R"(
        SELECT substr(started_at, 1, 10)
        FROM study_sessions
        WHERE started_at >= ? AND started_at < ? AND duration_seconds >= 60
        GROUP BY substr(started_at, 1, 10)
    )");
    query.addBindValue(toDatabaseDateTime(startDate.startOfDay()));
    query.addBindValue(toDatabaseDateTime(endDate.addDays(1).startOfDay()));

    if (query.exec()) {
        while (query.next()) {
            const QDate d = QDate::fromString(query.value(0).toString(), Qt::ISODate);
            if (d.isValid()) {
                dates.insert(d);
            }
        }
    }
    return dates;
}

bool DataStore::exportDeaveBridgeSnapshot() {
    if (!m_isOpen || m_appDataDir.isEmpty()) return false;

    QJsonObject root;
    root["schema"] = "deave.widgets.bridge.v1";
    root["updatedAt"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    root["todayStudySeconds"] = totalStudySecondsForDate(QDate::currentDate());
    root["tierAPercentToday"] = tierAStudyPercentForDate(QDate::currentDate());

    QJsonArray catArr;
    for (const StudyCategory& cat : studyCategories(false)) {
        QJsonObject cObj;
        cObj["id"] = cat.id;
        cObj["name"] = cat.name;
        cObj["shortName"] = cat.shortName;
        cObj["tier"] = cat.tier;
        cObj["vaultName"] = cat.vaultName;
        cObj["color"] = cat.color;
        cObj["done"] = cat.done;
        cObj["courseLimit"] = cat.courseLimit;
        cObj["studiedSeconds"] = totalStudySecondsForCategory(cat.id);
        cObj["targetMinutes"] = cat.targetMinutes;
        catArr.append(cObj);
    }
    root["categories"] = catArr;

    QJsonObject goalsObj;
    for (const QString& h : {"day", "week", "month", "year"}) {
        QJsonArray gArr;
        for (const GoalItem& g : goalsByHorizon(h)) {
            QJsonObject gItem;
            gItem["id"] = g.id;
            gItem["title"] = g.title;
            gItem["current"] = g.currentValue;
            gItem["target"] = g.targetValue;
            gItem["completed"] = g.completed;
            gArr.append(gItem);
        }
        goalsObj[h] = gArr;
    }
    root["goals"] = goalsObj;

    QJsonArray revArr;
    for (const SpacedReviewItem& r : spacedReviews(false)) {
        QJsonObject rObj;
        rObj["id"] = r.id;
        rObj["category"] = r.categoryName;
        rObj["topic"] = r.topic;
        rObj["kind"] = r.kind;
        rObj["stage"] = r.stage;
        rObj["stageLabel"] = stageLabel(r.stage);
        rObj["nextReviewAt"] = r.nextReviewAt.toString(Qt::ISODate);
        revArr.append(rObj);
    }
    root["spacedReviews"] = revArr;

    const QString bridgePath = QDir(m_appDataDir).filePath("deave_bridge_state.json");
    QSaveFile file(bridgePath);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    return file.commit();
}

QDate DataStore::semesterEndDate()
{
    const LocalSeedConfig& seed = CategoryConfigLoader::seedConfig();
    if (seed.semesterEndDate.isValid())
    {
        return seed.semesterEndDate;
    }
    return CategoryConfigLoader::semesterStartDate().addDays(140);
}

QString DataStore::dailySubjectConsolidationTaskForDate(const QDate& date, QString* outTime)
{
    const QDate d = date.isValid() ? date : QDate::currentDate();
    const bool isEven = (d.weekNumber() % 2 == 0);
    const int dow = d.dayOfWeek(); // 1 = Monday .. 7 = Sunday

    const LocalSeedConfig& seed = CategoryConfigLoader::seedConfig();
    const QDate semStart = CategoryConfigLoader::semesterStartDate();
    const QDate teachingEnd = seed.semesterTeachingEndDate.isValid()
        ? seed.semesterTeachingEndDate
        : semStart.addDays(97);

    QString timeStr = QStringLiteral("18:00");
    QString task;

    const auto& ruleMap = (d <= teachingEnd) ? seed.teachingDays : seed.examDays;
    if (ruleMap.contains(dow))
    {
        const LocalSemesterDayRule& rule = ruleMap.value(dow);
        if (!rule.timeText.isEmpty())
        {
            timeStr = rule.timeText;
        }
        if (isEven && !rule.evenText.isEmpty())
        {
            task = rule.evenText;
        }
        else if (!isEven && !rule.oddText.isEmpty())
        {
            task = rule.oddText;
        }
        else
        {
            task = rule.text;
        }
    }

    if (task.isEmpty())
    {
        if (d <= teachingEnd)
        {
            timeStr = (dow >= 6) ? QStringLiteral("12:00") : QStringLiteral("18:00");
            const QString parityLabel = isEven ? seed.weekEvenLabel : seed.weekOddLabel;
            task = QString::fromUtf8("📌 Повторение материала (День %1 • %2)")
                       .arg(dow)
                       .arg(parityLabel);
        }
        else
        {
            timeStr = QStringLiteral("10:00");
            task = QString::fromUtf8("📌 Повторение материала (Сессия • День %1)").arg(dow);
        }
    }

    if (outTime)
    {
        *outTime = timeStr;
    }
    return task;
}

QString DataStore::dailySpecialtyZeroToHeroTaskForDate(const QDate& date)
{
    const LocalSeedConfig& seed = CategoryConfigLoader::seedConfig();
    const QStringList& configured = seed.curriculumItems;

    static const QStringList defaultCurriculum = {
        QString::fromUtf8("🛠️ [Практика • День 1] Разобрать ключевую концепцию и написать небольшой тест-пример"),
        QString::fromUtf8("🛠️ [Практика • День 2] Повторить сложные места из конспекта и закрепить задачами"),
        QString::fromUtf8("🛠️ [Практика • День 3] Реализовать небольшой модуль и проверить граничные случаи"),
        QString::fromUtf8("🛠️ [Практика • День 4] Разобрать профильную статью или документацию по теме недели"),
        QString::fromUtf8("🛠️ [Практика • День 5] Выполнить лабораторную задачу без подсказок с таймером 45 мин"),
        QString::fromUtf8("🛠️ [Практика • День 6] Рефакторинг и документирование прогресса за неделю"),
        QString::fromUtf8("🛠️ [Практика • День 7] Итоговый обзор недели и планирование следующих шагов")
    };

    const QStringList& curriculum = configured.isEmpty() ? defaultCurriculum : configured;
    const QDate anchorDate = seed.curriculumAnchorDate.isValid() ? seed.curriculumAnchorDate : QDate(2026, 11, 9);
    const QDate d = date.isValid() ? date : QDate::currentDate();
    qint64 offset = anchorDate.daysTo(d);
    if (offset < 0)
    {
        offset = (offset % curriculum.size() + curriculum.size()) % curriculum.size();
    }
    else
    {
        offset = offset % curriculum.size();
    }
    return curriculum.at(static_cast<int>(offset));
}

QStringList DataStore::nextOpenTasksFromRoadmaps(int maxCount)
{
    QStringList results;
    if (maxCount <= 0)
    {
        return results;
    }

    const LocalSeedConfig& seed = CategoryConfigLoader::seedConfig();
    if (seed.roadmaps.isEmpty())
    {
        return results;
    }

    const QString prefix = seed.roadmapTaskPrefix.isEmpty()
        ? QString::fromUtf8("🗺️ [Roadmap]")
        : seed.roadmapTaskPrefix;

    static const QRegularExpression headingRe(R"(^#{1,4}\s+(.+)$)");
    static const QRegularExpression checkboxRe(R"(^\s*[-*]\s+\[\s\]\s+(.+)$)");

    for (const LocalRoadmapEntry& entry : seed.roadmaps)
    {
        if (!entry.syncDaily || entry.path.isEmpty())
        {
            continue;
        }
        if (results.size() >= maxCount)
        {
            break;
        }
        QFile file(entry.path);
        if (!file.exists() || !file.open(QIODevice::ReadOnly | QIODevice::Text))
        {
            continue;
        }
        QTextStream in(&file);
        in.setEncoding(QStringConverter::Utf8);
        QString currentHeading;
        while (!in.atEnd())
        {
            const QString line = in.readLine();
            const QRegularExpressionMatch hm = headingRe.match(line);
            if (hm.hasMatch())
            {
                currentHeading = hm.captured(1).trimmed();
                continue;
            }
            if (currentHeading.contains(QString::fromUtf8("Правила прохождения"), Qt::CaseInsensitive) ||
                currentHeading.contains(QString::fromUtf8("Обязательный чек-лист"), Qt::CaseInsensitive))
            {
                continue;
            }
            const QRegularExpressionMatch m = checkboxRe.match(line);
            if (m.hasMatch())
            {
                QString taskText = m.captured(1).trimmed();
                taskText.remove(QStringLiteral("**"));
                const QString formatted = QStringLiteral("%1 %2").arg(prefix, taskText);
                if (!results.contains(formatted))
                {
                    results.append(formatted);
                }
                break; // Take the top open task from each active roadmap
            }
        }
        file.close();
    }

    return results;
}

bool DataStore::markRoadmapTaskCompleted(const QString& taskText, bool completed)
{
    QString cleanTarget = taskText.trimmed();
    const LocalSeedConfig& seed = CategoryConfigLoader::seedConfig();
    const QString prefix = seed.roadmapTaskPrefix;
    if (!prefix.isEmpty() && cleanTarget.startsWith(prefix))
    {
        cleanTarget = cleanTarget.mid(prefix.length()).trimmed();
    }
    else if (cleanTarget.startsWith(QString::fromUtf8("🗺️ [")))
    {
        const int closeBracket = cleanTarget.indexOf(']');
        if (closeBracket > 0)
        {
            cleanTarget = cleanTarget.mid(closeBracket + 1).trimmed();
        }
    }
    if (cleanTarget.isEmpty())
    {
        return false;
    }

    static const QRegularExpression checkboxRe(R"(^(\s*[-*]\s+\[)[ xX](\]\s+)(.+)$)");

    for (const LocalRoadmapEntry& entry : seed.roadmaps)
    {
        if (!entry.syncDaily || entry.path.isEmpty())
        {
            continue;
        }
        QFile file(entry.path);
        if (!file.exists() || !file.open(QIODevice::ReadOnly | QIODevice::Text))
        {
            continue;
        }
        QTextStream in(&file);
        in.setEncoding(QStringConverter::Utf8);
        QStringList lines;
        while (!in.atEnd())
        {
            lines.append(in.readLine());
        }
        file.close();

        bool modified = false;
        for (int i = 0; i < lines.size(); ++i)
        {
            const QRegularExpressionMatch m = checkboxRe.match(lines[i]);
            if (!m.hasMatch())
            {
                continue;
            }
            QString candidate = m.captured(3).trimmed();
            candidate.remove(QStringLiteral("**"));
            if (candidate == cleanTarget || candidate.contains(cleanTarget) || cleanTarget.contains(candidate))
            {
                lines[i] = m.captured(1) + (completed ? QStringLiteral("x") : QStringLiteral(" ")) + m.captured(2) + m.captured(3);
                modified = true;
                break;
            }
        }

        if (modified)
        {
            QSaveFile saveFile(entry.path);
            if (saveFile.open(QIODevice::WriteOnly | QIODevice::Text))
            {
                QTextStream out(&saveFile);
                out.setEncoding(QStringConverter::Utf8);
                for (const QString& l : lines)
                {
                    out << l << "\n";
                }
                if (saveFile.commit())
                {
                    return true;
                }
            }
        }
    }

    return false;
}

void DataStore::ensureSemesterScheduleAndAutoTasks(bool forceReseed)
{
    if (!m_isOpen)
    {
        return;
    }

    QSettings settings;
    const bool alreadySeeded = settings.value("study/semesterAutoSeededV7", false).toBool();
    if (alreadySeeded && !forceReseed)
    {
        return;
    }

    const bool prevSync = m_syncing;
    m_syncing = true;

    const LocalSeedConfig& seed = CategoryConfigLoader::seedConfig();
    const QString eventTag = seed.studyEventTag.isEmpty() ? QStringLiteral("study") : seed.studyEventTag;
    const QString curTime = seed.curriculumTime.isEmpty() ? QStringLiteral("19:30") : seed.curriculumTime;

    QSqlDatabase database = QSqlDatabase::database(ConnectionName);
    database.transaction();

    // Remove outdated auto-generated calendar consolidation entries if force-reseeding
    if (forceReseed)
    {
        QSqlQuery cleanQ(database);
        cleanQ.exec(R"(
            DELETE FROM calendar_events
            WHERE completed = 0
              AND (title LIKE '📌 Закрепить%'
                   OR title LIKE '📌 Повторение%'
                   OR title LIKE '🛠️ [С нуля%'
                   OR title LIKE '🛠️ [Практика%')
        )");
    }

    // Load existing (event_date, title) pairs to avoid duplicates
    QSet<QString> existingEvents;
    {
        QSqlQuery existQ(database);
        if (existQ.exec("SELECT event_date, title FROM calendar_events"))
        {
            while (existQ.next())
            {
                existingEvents.insert(existQ.value(0).toString() + QStringLiteral("||") + existQ.value(1).toString());
            }
        }
    }

    const QDate startDate = CategoryConfigLoader::semesterStartDate();
    const QDate endDate = semesterEndDate();
    const QString nowStr = toDatabaseDateTime(QDateTime::currentDateTime());

    QSqlQuery insEvent(database);
    insEvent.prepare(R"(
        INSERT INTO calendar_events (event_date, time_text, title, tag, completed, created_at)
        VALUES (?, ?, ?, ?, 0, ?)
    )");

    for (QDate d = startDate; d <= endDate; d = d.addDays(1))
    {
        const QString dIso = d.toString(Qt::ISODate);

        // 1. Daily subject consolidation task based on schedule & week parity
        QString subjTime;
        const QString subjTitle = dailySubjectConsolidationTaskForDate(d, &subjTime);
        const QString subjKey = dIso + QStringLiteral("||") + subjTitle;
        if (!existingEvents.contains(subjKey))
        {
            insEvent.addBindValue(dIso);
            insEvent.addBindValue(subjTime);
            insEvent.addBindValue(subjTitle);
            insEvent.addBindValue(eventTag);
            insEvent.addBindValue(nowStr);
            insEvent.exec();
            existingEvents.insert(subjKey);
        }

        // 2. Daily practice task from configured curriculum
        const QString specTitle = dailySpecialtyZeroToHeroTaskForDate(d);
        const QString specKey = dIso + QStringLiteral("||") + specTitle;
        if (!existingEvents.contains(specKey))
        {
            insEvent.addBindValue(dIso);
            insEvent.addBindValue(curTime);
            insEvent.addBindValue(specTitle);
            insEvent.addBindValue(QStringLiteral("general"));
            insEvent.addBindValue(nowStr);
            insEvent.exec();
            existingEvents.insert(specKey);
        }
    }

    // Ensure rich Week / Month / Year goals exist in GoalsWidget
    auto ensureHorizonGoal = [&](const QString& horizon, const QString& title, int target, int current, int sortOrder)
    {
        QSqlQuery checkG(database);
        checkG.prepare("SELECT id FROM goals WHERE horizon = ? AND title = ? LIMIT 1");
        checkG.addBindValue(horizon);
        checkG.addBindValue(title);
        if (checkG.exec() && !checkG.next())
        {
            QSqlQuery insG(database);
            insG.prepare(R"(
                INSERT INTO goals (horizon, title, current_value, target_value, completed, period_key, sort_order, created_at)
                VALUES (?, ?, ?, ?, 0, ?, ?, ?)
            )");
            insG.addBindValue(horizon);
            insG.addBindValue(title);
            insG.addBindValue(current);
            insG.addBindValue(target);
            insG.addBindValue(QDate::currentDate().toString(Qt::ISODate));
            insG.addBindValue(sortOrder);
            insG.addBindValue(nowStr);
            insG.exec();
        }
    };

    if (!seed.semesterHorizonGoals.isEmpty())
    {
        for (const LocalHorizonGoalSeed& hg : seed.semesterHorizonGoals)
        {
            ensureHorizonGoal(hg.horizon, hg.title, hg.targetValue, hg.currentValue, hg.sortOrder);
        }
    }
    else
    {
        ensureHorizonGoal(
            QStringLiteral("week"),
            QString::fromUtf8("📌 Закрепить все ключевые темы недели"),
            5, 0, 1
        );
        ensureHorizonGoal(
            QStringLiteral("month"),
            QString::fromUtf8("🛠️ Завершить практический модуль месяца"),
            8, 0, 1
        );
        ensureHorizonGoal(
            QStringLiteral("year"),
            QString::fromUtf8("🎯 Закрыть годовую программу обучения"),
            24, 0, 1
        );
    }

    database.commit();
    settings.setValue("study/semesterAutoSeededV7", true);
    m_syncing = prevSync;
}

void DataStore::syncDailyTasksWithCalendarAndRoadmaps(const QDate& targetDate)
{
    if (!m_isOpen || m_syncing)
    {
        return;
    }

    m_syncing = true;
    const QDate today = targetDate.isValid() ? targetDate : QDate::currentDate();
    m_lastSyncedDate = today;
    const QString todayIso = today.toString(Qt::ISODate);
    const QString nowStr = toDatabaseDateTime(QDateTime::currentDateTime());

    const LocalSeedConfig& seed = CategoryConfigLoader::seedConfig();
    const QString eventTag = seed.studyEventTag.isEmpty() ? QStringLiteral("study") : seed.studyEventTag;
    const QString curTime = seed.curriculumTime.isEmpty() ? QStringLiteral("19:30") : seed.curriculumTime;

    QSqlDatabase database = QSqlDatabase::database(ConnectionName);
    database.transaction();

    // 1. Compute today's automatic tasks
    QString subjTime;
    const QString todaySubjTask = dailySubjectConsolidationTaskForDate(today, &subjTime);
    const QString todaySpecTask = dailySpecialtyZeroToHeroTaskForDate(today);
    const QStringList roadmapTasks = nextOpenTasksFromRoadmaps(2);

    // 2. Remove stale uncompleted auto-generated "day" goals from previous days or completed roadmaps
    {
        QSqlQuery delStale(database);
        delStale.prepare(R"(
            DELETE FROM goals
            WHERE horizon = 'day'
              AND completed = 0
              AND (title LIKE '📌 Закрепить%'
                   OR title LIKE '📌 Повторение%'
                   OR title LIKE '🛠️ [С нуля%'
                   OR title LIKE '🛠️ [Практика%'
                   OR title LIKE '🗺️ [Roadmap%')
              AND period_key != ?
        )");
        delStale.addBindValue(todayIso);
        delStale.exec();

        // Also remove uncompleted roadmap goals & calendar events whose text is no longer the active open roadmap task
        QSqlQuery checkRd(database);
        if (checkRd.exec("SELECT id, title FROM goals WHERE horizon = 'day' AND completed = 0 AND title LIKE '🗺️ [Roadmap%'"))
        {
            while (checkRd.next())
            {
                const qint64 gid = checkRd.value(0).toLongLong();
                const QString gtitle = checkRd.value(1).toString();
                if (!roadmapTasks.contains(gtitle))
                {
                    QSqlQuery rm(database);
                    rm.prepare("DELETE FROM goals WHERE id = ?");
                    rm.addBindValue(gid);
                    rm.exec();
                }
            }
        }
        QSqlQuery checkCalRd(database);
        checkCalRd.prepare("SELECT id, title FROM calendar_events WHERE event_date = ? AND completed = 0 AND title LIKE '🗺️ [Roadmap%'");
        checkCalRd.addBindValue(todayIso);
        if (checkCalRd.exec())
        {
            while (checkCalRd.next())
            {
                const qint64 cid = checkCalRd.value(0).toLongLong();
                const QString ctitle = checkCalRd.value(1).toString();
                if (!roadmapTasks.contains(ctitle))
                {
                    QSqlQuery rmCal(database);
                    rmCal.prepare("DELETE FROM calendar_events WHERE id = ?");
                    rmCal.addBindValue(cid);
                    rmCal.exec();
                }
            }
        }
    }

    // 3. Ensure today's Calendar Events and Daily Goals both contain all auto-tasks and share completion state
    auto ensureSyncedTaskForToday = [&](const QString& title, const QString& timeStr, const QString& tag, int sortOrder)
    {
        if (title.isEmpty())
        {
            return;
        }

        // Check calendar_events on todayIso
        qint64 calId = -1;
        bool calDone = false;
        {
            QSqlQuery qCal(database);
            qCal.prepare("SELECT id, completed FROM calendar_events WHERE event_date = ? AND title = ? LIMIT 1");
            qCal.addBindValue(todayIso);
            qCal.addBindValue(title);
            if (qCal.exec() && qCal.next())
            {
                calId = qCal.value(0).toLongLong();
                calDone = qCal.value(1).toBool();
            }
        }

        // Check goals (horizon = 'day')
        qint64 goalId = -1;
        bool goalDone = false;
        {
            QSqlQuery qGoal(database);
            qGoal.prepare("SELECT id, completed FROM goals WHERE horizon = 'day' AND title = ? LIMIT 1");
            qGoal.addBindValue(title);
            if (qGoal.exec() && qGoal.next())
            {
                goalId = qGoal.value(0).toLongLong();
                goalDone = qGoal.value(1).toBool();
            }
        }

        const bool unifiedDone = (calDone || goalDone);

        if (calId < 0)
        {
            QSqlQuery insCal(database);
            insCal.prepare(R"(
                INSERT INTO calendar_events (event_date, time_text, title, tag, completed, created_at)
                VALUES (?, ?, ?, ?, ?, ?)
            )");
            insCal.addBindValue(todayIso);
            insCal.addBindValue(timeStr);
            insCal.addBindValue(title);
            insCal.addBindValue(tag);
            insCal.addBindValue(unifiedDone ? 1 : 0);
            insCal.addBindValue(nowStr);
            insCal.exec();
        }
        else if (calDone != unifiedDone)
        {
            QSqlQuery updCal(database);
            updCal.prepare("UPDATE calendar_events SET completed = ? WHERE id = ?");
            updCal.addBindValue(unifiedDone ? 1 : 0);
            updCal.addBindValue(calId);
            updCal.exec();
        }

        if (goalId < 0)
        {
            QSqlQuery insGoal(database);
            insGoal.prepare(R"(
                INSERT INTO goals (horizon, title, current_value, target_value, completed, period_key, sort_order, created_at)
                VALUES ('day', ?, ?, 1, ?, ?, ?, ?)
            )");
            insGoal.addBindValue(title);
            insGoal.addBindValue(unifiedDone ? 1 : 0);
            insGoal.addBindValue(unifiedDone ? 1 : 0);
            insGoal.addBindValue(todayIso);
            insGoal.addBindValue(sortOrder);
            insGoal.addBindValue(nowStr);
            insGoal.exec();
        }
        else if (goalDone != unifiedDone)
        {
            QSqlQuery updGoal(database);
            updGoal.prepare("UPDATE goals SET completed = ?, current_value = ?, period_key = ? WHERE id = ?");
            updGoal.addBindValue(unifiedDone ? 1 : 0);
            updGoal.addBindValue(unifiedDone ? 1 : 0);
            updGoal.addBindValue(todayIso);
            updGoal.addBindValue(goalId);
            updGoal.exec();
        }
    };

    ensureSyncedTaskForToday(todaySubjTask, subjTime, eventTag, -30);
    ensureSyncedTaskForToday(todaySpecTask, curTime, QStringLiteral("general"), -20);
    for (int i = 0; i < roadmapTasks.size(); ++i)
    {
        ensureSyncedTaskForToday(
            roadmapTasks.at(i),
            i == 0 ? QStringLiteral("20:30") : QStringLiteral("21:15"),
            QStringLiteral("general"),
            -10 + i
        );
    }

    database.commit();
    m_syncing = false;
}

void DataStore::registerChangeListener(QObject* context, std::function<void()> callback) {
    if (!context || !callback) {
        return;
    }
    m_listeners.append({QPointer<QObject>(context), std::move(callback)});
}

void DataStore::notifyListeners() {
    for (int i = m_listeners.size() - 1; i >= 0; --i) {
        if (m_listeners[i].context.isNull()) {
            m_listeners.removeAt(i);
            continue;
        }
        QObject* ctx = m_listeners[i].context.data();
        auto cb = m_listeners[i].callback;
        QTimer::singleShot(0, ctx, cb);
    }
}

void DataStore::setError(const QString& error) {
    m_lastError = error;
    qWarning().noquote() << "StudyWidgets DataStore error:" << error;
}

bool DataStore::saveCachedNote(
    const QString& source,
    const QString& path,
    const QString& title,
    const QString& content,
    const QString& contentSha256,
    qint64 size)
{
    static constexpr qint64 MaxCachedNoteSizeBytes = 512 * 1024; // 512 KB

    const QByteArray utf8Bytes = content.toUtf8();
    const qint64 actualSize = (size >= 0) ? size : static_cast<qint64>(utf8Bytes.size());

    // Strict 512 KB limit enforcement
    if (actualSize > MaxCachedNoteSizeBytes || utf8Bytes.size() > MaxCachedNoteSizeBytes)
    {
        qWarning().noquote() << QStringLiteral("Cannot cache note exceeding 512 KB: size=%1 bytes").arg(actualSize);
        return false;
    }

    QString sha256 = contentSha256;
    if (sha256.isEmpty())
    {
        sha256 = QString::fromLatin1(QCryptographicHash::hash(utf8Bytes, QCryptographicHash::Sha256).toHex());
    }

    QSqlDatabase database = QSqlDatabase::database(ConnectionName);
    QSqlQuery query(database);

    // Upsert into cached_notes
    query.prepare(
        "INSERT INTO cached_notes(source, path, title, content, content_sha256, size, cached_at) "
        "VALUES(?, ?, ?, ?, ?, ?, ?) "
        "ON CONFLICT(source, path) DO UPDATE SET "
        "  title = excluded.title, "
        "  content = excluded.content, "
        "  content_sha256 = excluded.content_sha256, "
        "  size = excluded.size, "
        "  cached_at = excluded.cached_at"
    );
    query.addBindValue(source);
    query.addBindValue(path);
    query.addBindValue(title);
    query.addBindValue(content);
    query.addBindValue(sha256);
    query.addBindValue(actualSize);
    query.addBindValue(toDatabaseDateTime(QDateTime::currentDateTime()));

    if (!query.exec())
    {
        setError("Could not save cached note: " + query.lastError().text());
        return false;
    }

    // LRU eviction ordered by cached_at
    const int maxCount = m_maxCachedNotesCount > 0 ? m_maxCachedNotesCount : 100;
    QSqlQuery countQuery(database);
    if (countQuery.exec("SELECT COUNT(*) FROM cached_notes") && countQuery.next())
    {
        const int count = countQuery.value(0).toInt();
        if (count > maxCount)
        {
            const int excess = count - maxCount;
            QSqlQuery evictQuery(database);
            evictQuery.prepare(
                "DELETE FROM cached_notes WHERE id IN ("
                "  SELECT id FROM cached_notes ORDER BY cached_at ASC, id ASC LIMIT ?"
                ")"
            );
            evictQuery.addBindValue(excess);
            evictQuery.exec();
        }
    }

    return true;
}

bool DataStore::getCachedNote(const QString& source, const QString& path, CachedNote* outNote)
{
    QSqlDatabase database = QSqlDatabase::database(ConnectionName);
    QSqlQuery query(database);
    query.prepare(
        "SELECT id, source, path, title, content, content_sha256, size, cached_at "
        "FROM cached_notes WHERE source = ? AND path = ?"
    );
    query.addBindValue(source);
    query.addBindValue(path);

    if (!query.exec() || !query.next())
    {
        return false;
    }

    if (outNote)
    {
        outNote->id = query.value(0).toLongLong();
        outNote->source = query.value(1).toString();
        outNote->path = query.value(2).toString();
        outNote->title = query.value(3).toString();
        outNote->content = query.value(4).toString();
        outNote->contentSha256 = query.value(5).toString();
        outNote->size = query.value(6).toLongLong();
        outNote->cachedAt = fromDatabaseDateTime(query.value(7).toString());
    }

    // Refresh LRU recency
    const qint64 noteId = query.value(0).toLongLong();
    QSqlQuery touchQuery(database);
    touchQuery.prepare("UPDATE cached_notes SET cached_at = ? WHERE id = ?");
    touchQuery.addBindValue(toDatabaseDateTime(QDateTime::currentDateTime()));
    touchQuery.addBindValue(noteId);
    touchQuery.exec();

    return true;
}

bool DataStore::clearCachedNotes()
{
    QSqlDatabase database = QSqlDatabase::database(ConnectionName);
    QSqlQuery query(database);
    if (!query.exec("DELETE FROM cached_notes"))
    {
        setError("Could not clear cached_notes: " + query.lastError().text());
        return false;
    }
    return true;
}

int DataStore::cachedNotesCount()
{
    QSqlDatabase database = QSqlDatabase::database(ConnectionName);
    QSqlQuery query(database);
    if (query.exec("SELECT COUNT(*) FROM cached_notes") && query.next())
    {
        return query.value(0).toInt();
    }
    return 0;
}

int DataStore::schemaVersion() const
{
    QSqlDatabase database = QSqlDatabase::database(ConnectionName);
    QSqlQuery query(database);
    if (query.exec("SELECT version FROM schema_version ORDER BY version DESC LIMIT 1") && query.next())
    {
        return query.value(0).toInt();
    }
    return 0;
}

void DataStore::setMaxCachedNotesCountForTesting(int count)
{
    m_maxCachedNotesCount = count;
}

int DataStore::maxCachedNotesCount() const
{
    return m_maxCachedNotesCount;
}

bool DataStore::saveNotesDraft(const QString& text, const QString& target)
{
    QSqlDatabase database = QSqlDatabase::database(ConnectionName);
    QSqlQuery query(database);
    query.prepare(R"(
        INSERT INTO notes_draft (id, target, text, updated_at)
        VALUES (1, :target, :text, :updated_at)
        ON CONFLICT(id) DO UPDATE SET
            target = excluded.target,
            text = excluded.text,
            updated_at = excluded.updated_at
    )");
    query.bindValue(QStringLiteral(":target"), target.trimmed().isEmpty() ? QStringLiteral("inbox") : target.trimmed());
    query.bindValue(QStringLiteral(":text"), text);
    query.bindValue(QStringLiteral(":updated_at"), toDatabaseDateTime(QDateTime::currentDateTime()));
    if (!query.exec())
    {
        setError("Could not save notes draft: " + query.lastError().text());
        return false;
    }
    return true;
}

bool DataStore::getNotesDraft(QString* outText, QString* outTarget)
{
    QSqlDatabase database = QSqlDatabase::database(ConnectionName);
    QSqlQuery query(database);
    if (!query.exec("SELECT target, text FROM notes_draft WHERE id = 1") || !query.next())
    {
        return false;
    }
    if (outTarget)
    {
        *outTarget = query.value(0).toString();
    }
    if (outText)
    {
        *outText = query.value(1).toString();
    }
    return true;
}

bool DataStore::clearNotesDraft()
{
    QSqlDatabase database = QSqlDatabase::database(ConnectionName);
    QSqlQuery query(database);
    if (!query.exec("DELETE FROM notes_draft WHERE id = 1"))
    {
        setError("Could not clear notes draft: " + query.lastError().text());
        return false;
    }
    return true;
}

QList<DayRuleItem> DataStore::dayRules()
{
    QList<DayRuleItem> list;
    QSqlDatabase database = QSqlDatabase::database(ConnectionName);
    QSqlQuery query(database);
    if (query.exec("SELECT id, title, completed, sort_order, created_at FROM day_rules ORDER BY sort_order ASC, id ASC"))
    {
        while (query.next())
        {
            DayRuleItem item;
            item.id = query.value(0).toLongLong();
            item.title = query.value(1).toString();
            item.completed = (query.value(2).toInt() != 0);
            item.sortOrder = query.value(3).toInt();
            item.createdAt = fromDatabaseDateTime(query.value(4).toString());
            list.append(item);
        }
    }
    return list;
}

bool DataStore::addDayRule(const QString& title, int sortOrder)
{
    const QString clean = title.trimmed();
    if (clean.isEmpty())
    {
        return false;
    }
    QSqlDatabase database = QSqlDatabase::database(ConnectionName);
    QSqlQuery query(database);
    query.prepare("INSERT INTO day_rules (title, completed, sort_order, created_at) VALUES (?, 0, ?, ?)");
    query.addBindValue(clean);
    query.addBindValue(sortOrder);
    query.addBindValue(toDatabaseDateTime(QDateTime::currentDateTime()));
    if (!query.exec())
    {
        setError("Could not add day rule: " + query.lastError().text());
        return false;
    }
    notifyListeners();
    return true;
}

bool DataStore::toggleDayRule(qint64 id, bool completed)
{
    QSqlDatabase database = QSqlDatabase::database(ConnectionName);
    QSqlQuery query(database);
    query.prepare("UPDATE day_rules SET completed = ? WHERE id = ?");
    query.addBindValue(completed ? 1 : 0);
    query.addBindValue(id);
    if (!query.exec())
    {
        setError("Could not toggle day rule: " + query.lastError().text());
        return false;
    }
    notifyListeners();
    return true;
}

bool DataStore::removeDayRule(qint64 id)
{
    QSqlDatabase database = QSqlDatabase::database(ConnectionName);
    QSqlQuery query(database);
    query.prepare("DELETE FROM day_rules WHERE id = ?");
    query.addBindValue(id);
    if (!query.exec())
    {
        setError("Could not remove day rule: " + query.lastError().text());
        return false;
    }
    notifyListeners();
    return true;
}

bool DataStore::resetDayRules()
{
    QSqlDatabase database = QSqlDatabase::database(ConnectionName);
    QSqlQuery query(database);
    if (!query.exec("UPDATE day_rules SET completed = 0"))
    {
        setError("Could not reset day rules: " + query.lastError().text());
        return false;
    }
    notifyListeners();
    return true;
}

void DataStore::seedDefaultDayRulesIfEmpty()
{
    QSqlDatabase database = QSqlDatabase::database(ConnectionName);
    QSqlQuery query(database);
    if (query.exec("SELECT COUNT(*) FROM day_rules") && query.next() && query.value(0).toInt() == 0)
    {
        const QStringList defaults = {
            QString::fromUtf8("Фокус-блок: ключевая задача дня без переключений"),
            QString::fromUtf8("Консолидация конспектов и разбор сложных мест"),
            QString::fromUtf8("Сверка чек-листов и целей спринта"),
            QString::fromUtf8("Водный баланс и физическая разминка")
        };
        for (int i = 0; i < defaults.size(); ++i)
        {
            addDayRule(defaults[i], i);
        }
    }
}
