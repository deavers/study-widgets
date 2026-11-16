#include "TimeLogWriter.h"

#include "CategoryConfigLoader.h"
#include "DataStore.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QtMath>

QString TimeLogWriter::formatSessionLine(
    const StudySession& session,
    const QString& vaultName
) {
    const QString dateStr = session.finishedAt.isValid()
        ? session.finishedAt.toString("yyyy-MM-dd")
        : session.startedAt.toString("yyyy-MM-dd");

    const QString timeStr = session.finishedAt.isValid()
        ? session.finishedAt.toString("HH:mm")
        : session.startedAt.toString("HH:mm");

    const QString cleanName = vaultName.trimmed().isEmpty()
        ? (session.category.trimmed().isEmpty() ? QStringLiteral("Free Study") : session.category.trimmed())
        : vaultName.trimmed();

    const int minutes = qMax(1, qRound(session.durationSeconds / 60.0));
    const QString manualTag = (session.source == "manual") ? QStringLiteral(" ✎") : QStringLiteral("");

    // Exact legacy format:
    // - 📅 **Дата:** 2026-06-09 | 🕒 **Время:** 02:51 | 📚 **Предмет:** [[Subject]] | ⏱ **Длительность:** 45 мин
    return QStringLiteral("- 📅 **Дата:** %1 | 🕒 **Время:** %2 | 📚 **Предмет:** [[%3]] | ⏱ **Длительность:** %4 мин%5\n")
        .arg(dateStr, timeStr, cleanName, QString::number(minutes), manualTag);
}

bool TimeLogWriter::appendSession(
    const StudySession& session,
    const QString& vaultName,
    QString* error
) {
    if (session.durationSeconds <= 0) {
        return true;
    }

    const QString filePath = CategoryConfigLoader::timeLogFilePath();
    if (filePath.isEmpty()) {
        if (error) *error = "TimeLog file path is not configured.";
        return false;
    }

    QFileInfo fileInfo(filePath);
    QDir dir = fileInfo.dir();
    if (!dir.exists() && !dir.mkpath(".")) {
        if (error) *error = "Could not create directory for TimeLog: " + dir.absolutePath();
        return false;
    }

    QFile file(filePath);
    bool fileHadContent = false;
    bool needsLeadingNewline = false;

    if (file.exists()) {
        fileHadContent = file.size() > 0;
        if (fileHadContent && file.open(QIODevice::ReadOnly)) {
            file.seek(file.size() - 1);
            char lastChar = 0;
            file.read(&lastChar, 1);
            if (lastChar != '\n') {
                needsLeadingNewline = true;
            }
            file.close();
        }
    }

    if (!file.open(QIODevice::Append | QIODevice::Text)) {
        if (error) *error = "Could not open TimeLog.md for appending: " + file.errorString();
        return false;
    }

    QTextStream out(&file);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    out.setCodec("UTF-8");
#else
    out.setEncoding(QStringConverter::Utf8);
#endif

    if (needsLeadingNewline) {
        out << "\n";
    }

    const QString line = formatSessionLine(session, vaultName);
    out << line;
    out.flush();
    file.close();

    if (session.id > 0) {
        DataStore::instance().markSessionLogged(session.id);
    }

    return true;
}

int TimeLogWriter::flushUnloggedSessions(QString* error) {
    const auto unlogged = DataStore::instance().unloggedSessions();
    if (unlogged.isEmpty()) {
        return 0;
    }

    int count = 0;
    for (const StudySession& s : unlogged) {
        StudyCategory cat = DataStore::instance().categoryById(s.categoryId);
        QString vaultName = cat.vaultName.isEmpty() ? cat.shortName : cat.vaultName;
        if (vaultName.isEmpty()) {
            vaultName = s.category;
        }

        if (appendSession(s, vaultName, error)) {
            count++;
        } else {
            break; // Stop if destination unreachable
        }
    }

    return count;
}
