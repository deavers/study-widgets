#include "DataStore.h"

#include <QDebug>
#include <QDir>
#include <QSet>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>

namespace {

constexpr int CurrentSchemaVersion = 2;

const QString ConnectionName = "StudyWidgetsSqlConnection";

struct DefaultCategory {
    QString id;
    QString name;
    QString type;
    QString color;
    int sortOrder = 0;
};

QList<DefaultCategory> defaultCategories()
{
    return
    {
        {
            "free-study",
            "Free Study",
            "general",
            "#5B8DEF",
            0
        },
        {
            "ctf-practice",
            "CTF Practice",
            "project",
            "#E11D48",
            10
        },
        {
            "personal-project",
            "Personal Project",
            "project",
            "#D97706",
            20
        }
    };
}

QString toDatabaseDateTime(const QDateTime& value)
{
    return value.toString(Qt::ISODateWithMs);
}

QDateTime fromDatabaseDateTime(const QString& value)
{
    QDateTime result = QDateTime::fromString(
        value,
        Qt::ISODateWithMs
    );

    if (!result.isValid())
    {
        result = QDateTime::fromString(
            value,
            Qt::ISODate
        );
    }

    return result;
}

}

DataStore& DataStore::instance()
{
    static DataStore dataStore;
    return dataStore;
}

bool DataStore::open()
{
    if (m_isOpen)
    {
        return true;
    }

    m_lastError.clear();

    if (!openDatabase())
    {
        return false;
    }

    if (!ensureSchema())
    {
        return false;
    }

    m_isOpen = true;

    qInfo().noquote()
        << "StudyWidgets database:"
        << m_databasePath;

    return true;
}

bool DataStore::isOpen() const
{
    return m_isOpen;
}

QString DataStore::databasePath() const
{
    return m_databasePath;
}

QString DataStore::lastError() const
{
    return m_lastError;
}

bool DataStore::openDatabase()
{
    if (!QSqlDatabase::drivers().contains("QSQLITE"))
    {
        setError(
            "Qt SQLite driver is not available. "
            "The QSQLITE plugin could not be found."
        );

        return false;
    }

    const QString appDataDirectory =
        QStandardPaths::writableLocation(
            QStandardPaths::AppDataLocation
        );

    if (appDataDirectory.isEmpty())
    {
        setError(
            "Windows AppData directory could not be resolved."
        );

        return false;
    }

    QDir directory(appDataDirectory);

    if (!directory.mkpath("."))
    {
        setError(
            "Could not create the StudyWidgets application data directory."
        );

        return false;
    }

    m_databasePath = directory.filePath("studywidgets.sqlite");

    QSqlDatabase database;

    if (QSqlDatabase::contains(ConnectionName))
    {
        database = QSqlDatabase::database(ConnectionName);
    }
    else
    {
        database = QSqlDatabase::addDatabase(
            "QSQLITE",
            ConnectionName
        );
    }

    database.setDatabaseName(m_databasePath);

    if (!database.open())
    {
        setError(
            "Could not open SQLite database: " +
            database.lastError().text()
        );

        return false;
    }

    QSqlQuery pragmaQuery(database);

    if (!pragmaQuery.exec("PRAGMA foreign_keys = ON"))
    {
        setError(
            "Could not enable SQLite foreign keys: " +
            pragmaQuery.lastError().text()
        );

        return false;
    }

    return true;
}

bool DataStore::ensureSchema() {
    QSqlDatabase database =
        QSqlDatabase::database(ConnectionName);

    QSqlQuery query(database);

    const QString createVersionTable = R"(
        CREATE TABLE IF NOT EXISTS schema_version (
            version INTEGER PRIMARY KEY,
            applied_at TEXT NOT NULL
        )
    )";

    if (!query.exec(createVersionTable)) {
        setError(
            "Could not create schema_version table: " +
            query.lastError().text()
        );

        return false;
    }

    if (!query.exec(
        "SELECT version "
        "FROM schema_version "
        "ORDER BY version DESC "
        "LIMIT 1"
    )) {
        setError(
            "Could not read database schema version: " +
            query.lastError().text()
        );

        return false;
    }

    int databaseSchemaVersion = 0;

    if (!query.next()) {
        if (!createSchemaVersionOne()) {
            return false;
        }

        databaseSchemaVersion = 1;
    } else {
        databaseSchemaVersion =
            query.value(0).toInt();
    }

    if (databaseSchemaVersion > CurrentSchemaVersion) {
        setError(
            "Database schema is newer than this version of StudyWidgets."
        );

        return false;
    }

    while (databaseSchemaVersion < CurrentSchemaVersion) {
        if (databaseSchemaVersion == 1) {
            if (!migrateSchemaV1ToV2()) {
                return false;
            }

            databaseSchemaVersion = 2;
            continue;
        }

        setError(
            "No migration path is available for the current database schema."
        );

        return false;
    }

    return true;
}

bool DataStore::createSchemaVersionOne()
{
    QSqlDatabase database =
        QSqlDatabase::database(ConnectionName);

    QSqlQuery query(database);

    const QString createSessionsTable = R"(
        CREATE TABLE IF NOT EXISTS study_sessions (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            category TEXT NOT NULL,
            started_at TEXT NOT NULL,
            finished_at TEXT NOT NULL,
            duration_seconds INTEGER NOT NULL CHECK(duration_seconds >= 0),
            completed INTEGER NOT NULL DEFAULT 0,
            note TEXT NOT NULL DEFAULT '',
            created_at TEXT NOT NULL
        )
    )";

    if (!query.exec(createSessionsTable))
    {
        setError(
            "Could not create study_sessions table: " +
            query.lastError().text()
        );

        return false;
    }

    if (!query.exec(
        "CREATE INDEX IF NOT EXISTS "
        "idx_study_sessions_started_at "
        "ON study_sessions(started_at)"
    ))
    {
        setError(
            "Could not create study_sessions index: " +
            query.lastError().text()
        );

        return false;
    }

    query.prepare(
        "INSERT INTO schema_version(version, applied_at) "
        "VALUES(?, ?)"
    );

    query.addBindValue(CurrentSchemaVersion);
    query.addBindValue(
        toDatabaseDateTime(QDateTime::currentDateTime())
    );

    if (!query.exec())
    {
        setError(
            "Could not save schema version: " +
            query.lastError().text()
        );

        return false;
    }

    return true;
}

bool DataStore::migrateSchemaV1ToV2() {
    QSqlDatabase database =
        QSqlDatabase::database(ConnectionName);

    if (!database.transaction()) {
        setError(
            "Could not start database migration transaction: " +
            database.lastError().text()
        );

        return false;
    }

    QSqlQuery query(database);

    const QString createCategoriesTable = R"(
        CREATE TABLE IF NOT EXISTS categories (
            id TEXT PRIMARY KEY,
            name TEXT NOT NULL COLLATE NOCASE UNIQUE,
            type TEXT NOT NULL CHECK(
                type IN ('subject', 'general', 'project')
            ),
            color TEXT NOT NULL,
            archived INTEGER NOT NULL DEFAULT 0 CHECK(
                archived IN (0, 1)
            ),
            sort_order INTEGER NOT NULL DEFAULT 0,
            created_at TEXT NOT NULL
        )
    )";

    if (!query.exec(createCategoriesTable)) {
        database.rollback();

        setError(
            "Could not create categories table: " +
            query.lastError().text()
        );

        return false;
    }

    if (!query.exec(
        "ALTER TABLE study_sessions "
        "ADD COLUMN category_id TEXT"
    )) {
        database.rollback();

        setError(
            "Could not add category_id to study_sessions: " +
            query.lastError().text()
        );

        return false;
    }

    if (!query.exec(
        "CREATE INDEX IF NOT EXISTS "
        "idx_study_sessions_category_id "
        "ON study_sessions(category_id)"
    )) {
        database.rollback();

        setError(
            "Could not create category_id index: " +
            query.lastError().text()
        );

        return false;
    }

    for (const DefaultCategory& category : defaultCategories()) {
        query.prepare(R"(
            INSERT OR IGNORE INTO categories (
                id,
                name,
                type,
                color,
                archived,
                sort_order,
                created_at
            )
            VALUES (?, ?, ?, ?, ?, ?, ?)
        )");

        query.addBindValue(category.id);
        query.addBindValue(category.name);
        query.addBindValue(category.type);
        query.addBindValue(category.color);
        query.addBindValue(0);
        query.addBindValue(category.sortOrder);
        query.addBindValue(
            toDatabaseDateTime(QDateTime::currentDateTime())
        );

        if (!query.exec()) {
            database.rollback();

            setError(
                "Could not seed default categories: " +
                query.lastError().text()
            );

            return false;
        }
    }

    for (const DefaultCategory& category : defaultCategories()) {
        query.prepare(R"(
            UPDATE study_sessions
            SET category_id = ?
            WHERE category_id IS NULL
              AND lower(category) = lower(?)
        )");

        query.addBindValue(category.id);
        query.addBindValue(category.name);

        if (!query.exec()) {
            database.rollback();

            setError(
                "Could not backfill existing category IDs: " +
                query.lastError().text()
            );

            return false;
        }
    }

    if (!query.exec(R"(
        UPDATE study_sessions
        SET category_id = 'free-study'
        WHERE category_id IS NULL
    )")) {
        database.rollback();

        setError(
            "Could not assign Free Study to legacy sessions: " +
            query.lastError().text()
        );

        return false;
    }

    query.prepare(
        "INSERT INTO schema_version(version, applied_at) "
        "VALUES(?, ?)"
    );

    query.addBindValue(2);
    query.addBindValue(
        toDatabaseDateTime(QDateTime::currentDateTime())
    );

    if (!query.exec()) {
        database.rollback();

        setError(
            "Could not save schema version 2: " +
            query.lastError().text()
        );

        return false;
    }

    if (!database.commit()) {
        setError(
            "Could not commit database migration: " +
            database.lastError().text()
        );

        return false;
    }

    qInfo().noquote()
        << "StudyWidgets database migrated to schema version 2.";

    return true;
}

bool DataStore::addStudySession(
    const StudySession& session
)
{
    if (!m_isOpen && !open())
    {
        return false;
    }

    const QString category = session.category.trimmed();

    const QString categoryId =
        session.categoryId.trimmed();

    if (categoryId.isEmpty()) {
        setError(
            "Study session category ID cannot be empty."
        );

        return false;
    }

    if (!session.startedAt.isValid() ||
        !session.finishedAt.isValid())
        {
        setError(
            "Study session start and finish times must be valid."
        );

        return false;
    }

    if (session.finishedAt < session.startedAt)
    {
        setError(
            "Study session finish time cannot be earlier than start time."
        );

        return false;
    }

    if (session.durationSeconds < 0)
    {
        setError(
            "Study session duration cannot be negative."
        );

        return false;
    }

    QSqlDatabase database =
        QSqlDatabase::database(ConnectionName);

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
            created_at
        )
        VALUES (?, ?, ?, ?, ?, ?, ?, ?)
    )");

    query.addBindValue(category);
    query.addBindValue(categoryId);
    query.addBindValue(toDatabaseDateTime(session.startedAt));
    query.addBindValue(toDatabaseDateTime(session.finishedAt));
    query.addBindValue(session.durationSeconds);
    query.addBindValue(session.completed ? 1 : 0);

    QString note = session.note.trimmed();

    if (note.isNull())
    {
        note = QStringLiteral("");
    }

    query.addBindValue(note);

    query.addBindValue(
        toDatabaseDateTime(QDateTime::currentDateTime())
    );

    if (!query.exec())
    {
        setError(
            "Could not save study session: " +
            query.lastError().text()
        );

        return false;
    }

    return true;
}

QList<StudySession> DataStore::recentStudySessions(
    int limit
)
{
    QList<StudySession> sessions;

    if (!m_isOpen && !open())
    {
        return sessions;
    }

    if (limit < 1)
    {
        return sessions;
    }

    QSqlDatabase database =
        QSqlDatabase::database(ConnectionName);

    QSqlQuery query(database);

    query.prepare(R"(
        SELECT
            id,
            category,
            category_id,
            started_at,
            finished_at,
            duration_seconds,
            completed,
            note
        FROM study_sessions
        ORDER BY started_at DESC
        LIMIT ?
    )");

    query.addBindValue(limit);

    if (!query.exec())
    {
        setError(
            "Could not load study sessions: " +
            query.lastError().text()
        );

        return sessions;
    }

    while (query.next())
    {
        StudySession session;

        session.id = query.value(0).toLongLong();
        session.category = query.value(1).toString();
        session.categoryId = query.value(2).toString();

        session.startedAt = fromDatabaseDateTime(
            query.value(3).toString()
        );
        session.finishedAt = fromDatabaseDateTime(
            query.value(4).toString()
        );
        session.durationSeconds = query.value(5).toInt();
        session.completed = query.value(6).toBool();
        session.note = query.value(7).toString();

        sessions.append(session);
    }

    return sessions;
}

bool DataStore::syncStudyCategories(
    const QList<StudyCategory>& categories
) {
    if (!m_isOpen && !open()) {
        return false;
    }

    if (categories.isEmpty()) {
        setError(
            "Category configuration must contain at least one category."
        );

        return false;
    }

    QSet<QString> knownIds;

    for (const StudyCategory& category : categories) {
        const QString id = category.id.trimmed();
        const QString name = category.name.trimmed();

        if (id.isEmpty()) {
            setError(
                "Category ID cannot be empty."
            );

            return false;
        }

        if (name.isEmpty()) {
            setError(
                "Category name cannot be empty."
            );

            return false;
        }

        if (category.type != "subject" &&
            category.type != "general" &&
            category.type != "project") {
            setError(
                "Category type must be subject, general or project."
            );

            return false;
        }

        if (knownIds.contains(id)) {
            setError(
                "Category IDs must be unique."
            );

            return false;
        }

        knownIds.insert(id);
    }

    if (!knownIds.contains("free-study")) {
        setError(
            "Category configuration must contain free-study."
        );

        return false;
    }

    QSqlDatabase database =
        QSqlDatabase::database(ConnectionName);

    if (!database.transaction()) {
        setError(
            "Could not start category synchronization transaction: " +
            database.lastError().text()
        );

        return false;
    }

    QSqlQuery query(database);

    query.prepare(R"(
        INSERT INTO categories (
            id,
            name,
            type,
            color,
            archived,
            sort_order,
            created_at
        )
        VALUES (?, ?, ?, ?, ?, ?, ?)
        ON CONFLICT(id) DO UPDATE SET
            name = excluded.name,
            type = excluded.type,
            color = excluded.color,
            archived = excluded.archived,
            sort_order = excluded.sort_order
    )");

    for (const StudyCategory& category : categories) {
        query.bindValue(0, category.id.trimmed());
        query.bindValue(1, category.name.trimmed());
        query.bindValue(2, category.type);
        query.bindValue(3, category.color);
        query.bindValue(4, category.archived ? 1 : 0);
        query.bindValue(5, category.sortOrder);
        query.bindValue(
            6,
            toDatabaseDateTime(QDateTime::currentDateTime())
        );

        if (!query.exec()) {
            database.rollback();

            setError(
                "Could not save category configuration: " +
                query.lastError().text()
            );

            return false;
        }
    }

    QStringList placeholders;

    for (int index = 0; index < categories.size(); ++index) {
        placeholders.append("?");
    }

    QSqlQuery archiveQuery(database);

    archiveQuery.prepare(
        "UPDATE categories "
        "SET archived = 1 "
        "WHERE id NOT IN (" +
        placeholders.join(", ") +
        ")"
    );

    for (const StudyCategory& category : categories) {
        archiveQuery.addBindValue(category.id.trimmed());
    }

    if (!archiveQuery.exec()) {
        database.rollback();

        setError(
            "Could not archive categories missing from local configuration: " +
            archiveQuery.lastError().text()
        );

        return false;
    }

    if (!database.commit()) {
        setError(
            "Could not commit category synchronization: " +
            database.lastError().text()
        );

        return false;
    }

    return true;
}

QList<StudyCategory> DataStore::studyCategories(
    bool includeArchived
) {
    QList<StudyCategory> categories;

    if (!m_isOpen && !open()) {
        return categories;
    }

    QSqlDatabase database =
        QSqlDatabase::database(ConnectionName);

    QSqlQuery query(database);

    QString sql = R"(
        SELECT
            id,
            name,
            type,
            color,
            archived,
            sort_order
        FROM categories
    )";

    if (!includeArchived) {
        sql += " WHERE archived = 0";
    }

    sql += " ORDER BY sort_order ASC, name ASC";

    if (!query.exec(sql)) {
        setError(
            "Could not load study categories: " +
            query.lastError().text()
        );

        return categories;
    }

    while (query.next()) {
        StudyCategory category;

        category.id = query.value(0).toString();
        category.name = query.value(1).toString();
        category.type = query.value(2).toString();
        category.color = query.value(3).toString();
        category.archived = query.value(4).toBool();
        category.sortOrder = query.value(5).toInt();

        categories.append(category);
    }

    return categories;
}

int DataStore::totalStudySecondsForDate(
    const QDate& date
)
{
    if (!m_isOpen && !open())
    {
        return 0;
    }

    if (!date.isValid())
    {
        setError(
            "Cannot calculate total study time for an invalid date."
        );

        return 0;
    }

    const QDateTime dayStart = date.startOfDay();
    const QDateTime nextDayStart = date.addDays(1).startOfDay();

    QSqlDatabase database =
        QSqlDatabase::database(ConnectionName);

    QSqlQuery query(database);

    query.prepare(R"(
        SELECT COALESCE(SUM(duration_seconds), 0)
        FROM study_sessions
        WHERE started_at >= ?
          AND started_at < ?
    )");

    query.addBindValue(toDatabaseDateTime(dayStart));
    query.addBindValue(toDatabaseDateTime(nextDayStart));

    if (!query.exec())
    {
        setError(
            "Could not calculate daily study duration: " +
            query.lastError().text()
        );

        return 0;
    }

    if (!query.next())
    {
        return 0;
    }

    return query.value(0).toInt();
}

void DataStore::setError(const QString& error)
{
    m_lastError = error;

    qWarning().noquote()
        << "StudyWidgets DataStore error:"
        << error;
}
