#include "DataStore.h"

#include <QDebug>
#include <QDir>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>

namespace {

constexpr int CurrentSchemaVersion = 1;

const QString ConnectionName = "StudyWidgetsSqlConnection";

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

bool DataStore::ensureSchema()
{
    QSqlDatabase database =
        QSqlDatabase::database(ConnectionName);

    QSqlQuery query(database);

    const QString createVersionTable = R"(
        CREATE TABLE IF NOT EXISTS schema_version (
            version INTEGER PRIMARY KEY,
            applied_at TEXT NOT NULL
        )
    )";

    if (!query.exec(createVersionTable))
    {
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
    ))
    {
        setError(
            "Could not read database schema version: " +
            query.lastError().text()
        );

        return false;
    }

    if (!query.next())
    {
        return createSchemaVersionOne();
    }

    const int databaseSchemaVersion =
        query.value(0).toInt();

    if (databaseSchemaVersion > CurrentSchemaVersion)
    {
        setError(
            "Database schema is newer than this version of StudyWidgets."
        );

        return false;
    }

    if (databaseSchemaVersion < CurrentSchemaVersion)
    {
        setError(
            "Database migration is required but has not been implemented yet."
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

bool DataStore::addStudySession(
    const StudySession& session
)
{
    if (!m_isOpen && !open())
    {
        return false;
    }

    const QString category = session.category.trimmed();

    if (category.isEmpty())
    {
        setError(
            "Study session category cannot be empty."
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
            started_at,
            finished_at,
            duration_seconds,
            completed,
            note,
            created_at
        )
        VALUES (?, ?, ?, ?, ?, ?, ?)
    )");

    query.addBindValue(category);
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
        session.startedAt = fromDatabaseDateTime(
            query.value(2).toString()
        );
        session.finishedAt = fromDatabaseDateTime(
            query.value(3).toString()
        );
        session.durationSeconds = query.value(4).toInt();
        session.completed = query.value(5).toBool();
        session.note = query.value(6).toString();

        sessions.append(session);
    }

    return sessions;
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
