#pragma once

#include "DataStore.h"

#include <QString>

class TimeLogWriter
{
public:
    static QString formatSessionLine(
        const StudySession& session,
        const QString& vaultName
    );

    static bool appendSession(
        const StudySession& session,
        const QString& vaultName,
        QString* error = nullptr
    );

    static int flushUnloggedSessions(
        QString* error = nullptr
    );
};
