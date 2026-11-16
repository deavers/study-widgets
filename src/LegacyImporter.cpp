#include "LegacyImporter.h"

#include "DataStore.h"

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMap>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>

namespace {

QString readTextWithEncodingCheck(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return QString();
    }

    const QByteArray bytes = file.readAll();
    file.close();

    if (bytes.size() >= 2 &&
        static_cast<uchar>(bytes[0]) == 0xFF &&
        static_cast<uchar>(bytes[1]) == 0xFE) {
        // UTF-16 LE with BOM
        return QString::fromUtf16(
            reinterpret_cast<const char16_t*>(bytes.constData() + 2),
            (bytes.size() - 2) / 2
        );
    }

    if (bytes.size() >= 4 && bytes[1] == '\0' && bytes[3] == '\0') {
        // UTF-16 LE without BOM
        return QString::fromUtf16(
            reinterpret_cast<const char16_t*>(bytes.constData()),
            bytes.size() / 2
        );
    }

    return QString::fromUtf8(bytes);
}

}

QString LegacyImporter::defaultSaveDataPath() {
    const QString docs = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    return QDir(docs).filePath("Rainmeter/Skins/deave_Pomodoro/SaveData.inc");
}

bool LegacyImporter::importIfFirstRun(const QString& customPath, QString* error) {
    QSettings settings;
    if (settings.value("legacy/imported_savedata", false).toBool()) {
        return true; // Already imported previously
    }

    const QString path = customPath.isEmpty() ? defaultSaveDataPath() : customPath;
    if (!QFileInfo::exists(path)) {
        // If file doesn't exist, nothing to import, not an error
        return true;
    }

    const bool success = importFromSaveData(path, error);
    if (success) {
        settings.setValue("legacy/imported_savedata", true);
    }
    return success;
}

bool LegacyImporter::importFromSaveData(const QString& filePath, QString* error) {
    const QString text = readTextWithEncodingCheck(filePath);
    if (text.isEmpty()) {
        if (error) *error = "Could not read legacy SaveData.inc or file is empty: " + filePath;
        return false;
    }

    QMap<QString, QString> vars;
    const QStringList lines = text.split(QRegularExpression(QStringLiteral("[\r\n]+")), Qt::SkipEmptyParts);

    for (const QString& line : lines) {
        const QString trimmed = line.trimmed();
        if (trimmed.isEmpty() || trimmed.startsWith(';') || trimmed.startsWith('[')) {
            continue;
        }

        const int equalsIndex = trimmed.indexOf('=');
        if (equalsIndex > 0) {
            const QString key = trimmed.left(equalsIndex).trimmed();
            const QString val = trimmed.mid(equalsIndex + 1).trimmed();
            vars.insert(key, val);
        }
    }

    QSettings appSettings;
    if (vars.contains("AlarmVolume")) {
        appSettings.setValue("alarm/volume", vars.value("AlarmVolume").toInt());
    }
    if (vars.contains("AlarmSound")) {
        appSettings.setValue("alarm/sound", vars.value("AlarmSound"));
    }

    QList<StudyCategory> existingCategories = DataStore::instance().studyCategories(true);
    QMap<QString, StudyCategory> categoryMap;
    for (const auto& cat : existingCategories) {
        categoryMap.insert(cat.id, cat);
        categoryMap.insert(cat.name, cat);
        if (!cat.shortName.isEmpty()) {
            categoryMap.insert(cat.shortName, cat);
        }
    }

    // 1. Past semester university subjects (Sub1 .. Sub8)
    for (int i = 1; i <= 8; ++i) {
        const QString nameKey = QString("Sub%1Name").arg(i);
        const QString timeKey = QString("Sub%1Time").arg(i);
        if (!vars.contains(nameKey)) continue;

        const QString subName = vars.value(nameKey);
        const int minutes = vars.value(timeKey, "0").toInt();
        const QString catId = QString("legacy-%1").arg(subName.toLower().remove(QRegularExpression("[^a-z0-9]")));

        if (!categoryMap.contains(catId) && !categoryMap.contains(subName)) {
            StudyCategory cat;
            cat.id = catId;
            cat.name = QString("❄️ [Archived] %1").arg(subName);
            cat.shortName = subName;
            cat.vaultName = subName;
            cat.type = "subject";
            cat.color = "#64748B";
            cat.tier = "F";
            cat.archived = true;
            cat.sortOrder = 300 + i;
            cat.targetMinutes = qMax(600, minutes);

            existingCategories.append(cat);
            categoryMap.insert(cat.id, cat);
            categoryMap.insert(cat.name, cat);
            categoryMap.insert(cat.shortName, cat);
        }

        // Add imported study session if minutes > 0
        if (minutes > 0) {
            StudySession session;
            session.categoryId = catId;
            session.category = subName;
            session.startedAt = QDateTime(QDate(2026, 6, 9), QTime(0, 0));
            session.finishedAt = session.startedAt.addSecs(minutes * 60);
            session.durationSeconds = minutes * 60;
            session.completed = true;
            session.note = QStringLiteral("Imported from Rainmeter SaveData.inc");
            session.source = QStringLiteral("import");
            session.loggedMd = true; // Was already logged in past Obsidian TimeLog!
            DataStore::instance().addStudySession(session);
        }
    }

    // 2. Courses (Sub9 .. Sub13)
    for (int i = 9; i <= 15; ++i) {
        const QString nameKey = QString("Sub%1Name").arg(i);
        const QString timeKey = QString("Sub%1Time").arg(i);
        const QString limitKey = QString("Sub%1Limit").arg(i);
        const QString doneKey = QString("Sub%1Done").arg(i);
        if (!vars.contains(nameKey)) continue;

        const QString courseName = vars.value(nameKey);
        const int minutes = vars.value(timeKey, "0").toInt();
        const int limit = vars.value(limitKey, "1200").toInt();
        const bool isDone = vars.value(doneKey, "0").toInt() == 1;

        // Generate clean id: course-vk-alg, course-yandex-py, etc.
        QString cleanId = courseName.toLower();
        cleanId.replace(" ", "-");
        cleanId.replace("я", "ya").replace("п", "p").replace("и", "i");
        cleanId.remove(QRegularExpression("[^a-z0-9-]"));
        if (cleanId.isEmpty()) {
            cleanId = QString("course-%1").arg(i);
        } else {
            cleanId = QString("course-%1").arg(cleanId);
        }

        if (!categoryMap.contains(cleanId) && !categoryMap.contains(courseName)) {
            StudyCategory cat;
            cat.id = cleanId;
            cat.name = QString("🎓 %1").arg(courseName);
            cat.shortName = courseName.length() > 12 ? courseName.left(10) + "…" : courseName;
            cat.vaultName = courseName;
            cat.type = "subject";
            cat.color = "#F59E0B";
            cat.tier = "K"; // Courses tier!
            cat.archived = false;
            cat.sortOrder = 150 + i;
            cat.courseLimit = limit;
            cat.targetMinutes = limit;
            cat.done = isDone;

            existingCategories.append(cat);
            categoryMap.insert(cat.id, cat);
            categoryMap.insert(cat.name, cat);
            categoryMap.insert(cat.shortName, cat);
        }

        if (minutes > 0) {
            StudySession session;
            session.categoryId = cleanId;
            session.category = courseName;
            session.startedAt = QDateTime(QDate(2026, 6, 9), QTime(0, 0));
            session.finishedAt = session.startedAt.addSecs(minutes * 60);
            session.durationSeconds = minutes * 60;
            session.completed = true;
            session.note = QStringLiteral("Imported from Rainmeter SaveData.inc");
            session.source = QStringLiteral("import");
            session.loggedMd = true; // Was already logged
            DataStore::instance().addStudySession(session);
        }
    }

    DataStore::instance().syncStudyCategories(existingCategories);
    return true;
}
