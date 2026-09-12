#pragma once

#include <QDate>
#include <QDateTime>
#include <QList>
#include <QString>
#include <QtGlobal>

struct StudySession {
    qint64 id = -1;

    QString category;
    QString categoryId;

    QDateTime startedAt;
    QDateTime finishedAt;

    int durationSeconds = 0;
    bool completed = false;

    QString note;
};

struct StudyCategory {
    QString id;
    QString name;
    QString type;
    QString color;

    bool archived = false;
    int sortOrder = 0;
};

class DataStore {
public:
    static DataStore& instance();

    bool open();
    bool isOpen() const;

    QString databasePath() const;
    QString lastError() const;

    bool addStudySession(const StudySession& session);

    QList<StudySession> recentStudySessions(
        int limit = 50
    );

    int totalStudySecondsForDate(
        const QDate& date
    );

    QList<StudyCategory> studyCategories(
        bool includeArchived = false
    );

    bool syncStudyCategories(
        const QList<StudyCategory>& categories
    );

private:
    DataStore() = default;

    bool ensureSchema();
    bool createSchemaVersionOne();
    bool migrateSchemaV1ToV2();

    bool openDatabase();
    void setError(const QString& error);

    bool m_isOpen = false;

    QString m_databasePath;
    QString m_lastError;
};