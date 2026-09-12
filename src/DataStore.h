#pragma once

#include <QDateTime>
#include <QString>
#include <QList>
#include <QtGlobal>
#include <QDate>

struct StudySession
{
    qint64 id = -1;

    QString category;

    QDateTime startedAt;
    QDateTime finishedAt;

    int durationSeconds = 0;
    bool completed = false;

    QString note;
};

class DataStore
{
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

private:
    DataStore() = default;

    bool ensureSchema();
    bool createSchemaVersionOne();

    bool openDatabase();
    void setError(const QString& error);

    bool m_isOpen = false;

    QString m_databasePath;
    QString m_lastError;
};
