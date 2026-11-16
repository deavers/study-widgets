#include "CategoryConfigLoader.h"

#include "DataStore.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QRegularExpression>
#include <QSet>
#include <QSettings>
#include <QStandardPaths>

namespace
{

QDate s_semesterStartDate = QDate(2026, 9, 14);
QString s_timeLogFilePath;

QList<LocalPrayerMethod> defaultPrayerMethods()
{
    return {
        { QString::fromUtf8("8° коррекция"), QString::fromUtf8("8° Корр."), 8.0, 8.0, 0, 0, 2, 0, 2, 0 },
        { QStringLiteral("12° UOIF"), QStringLiteral("12° UOIF"), 12.0, 12.0, 0, 0, 2, 0, 2, 0 },
        { QStringLiteral("12° UOIF + Tune"), QStringLiteral("12°+Tune"), 12.0, 12.0, -9, -4, 6, 3, 0, 10 },
        { QStringLiteral("8° + Tune"), QStringLiteral("8°+Tune"), 8.0, 8.0, -9, -4, 6, 3, 0, 10 }
    };
}

LocalSeedConfig& cachedSeedConfig()
{
    static LocalSeedConfig instance;
    static bool initialized = false;
    if (!initialized)
    {
        instance.prayerMethods = defaultPrayerMethods();
        initialized = true;
    }
    return instance;
}

bool& seedConfigLoadedFlag()
{
    static bool loaded = false;
    return loaded;
}

QString readNonEmpty(const QJsonObject& obj, const QString& key, const QString& fallback)
{
    const QString v = obj.value(key).toString().trimmed();
    return v.isEmpty() ? fallback : v;
}

bool setError(
    QString* target,
    const QString& message
)
{
    if (target != nullptr)
    {
        *target = message;
    }

    return false;
}

bool isValidCategoryId(const QString& id)
{
    static const QRegularExpression pattern(
        "^[a-z0-9][a-z0-9_-]{0,63}$"
    );

    return pattern.match(id).hasMatch();
}

bool isValidColor(const QString& color)
{
    static const QRegularExpression pattern(
        "^#[0-9A-Fa-f]{6}$"
    );

    return pattern.match(color).hasMatch();
}

}

QString CategoryConfigLoader::localConfigPath()
{
    const QString appDataDirectory =
        QStandardPaths::writableLocation(
            QStandardPaths::AppDataLocation
        );

    return QDir(appDataDirectory).filePath(
        "config/categories.local.json"
    );
}

QString CategoryConfigLoader::seedConfigPath()
{
    const QString appDataDirectory =
        QStandardPaths::writableLocation(
            QStandardPaths::AppDataLocation
        );

    const QDir baseDir(appDataDirectory);
    const QString inConfigDir = baseDir.filePath(QStringLiteral("config/seed.local.json"));
    const QString inRootDir = baseDir.filePath(QStringLiteral("seed.local.json"));

    if (QFile::exists(inConfigDir))
    {
        return inConfigDir;
    }
    if (QFile::exists(inRootDir))
    {
        return inRootDir;
    }
    return inConfigDir;
}

const LocalSeedConfig& CategoryConfigLoader::seedConfig()
{
    if (!seedConfigLoadedFlag())
    {
        reloadSeedConfig(nullptr);
    }
    return cachedSeedConfig();
}

bool CategoryConfigLoader::reloadSeedConfig(QString* errorMessage)
{
    LocalSeedConfig cfg;
    cfg.prayerMethods = defaultPrayerMethods();

    const QString path = seedConfigPath();
    seedConfigLoadedFlag() = true;

    if (!QFile::exists(path))
    {
        cachedSeedConfig() = cfg;
        return true;
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
    {
        cachedSeedConfig() = cfg;
        return setError(errorMessage, "Could not open seed.local.json.");
    }

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject())
    {
        cachedSeedConfig() = cfg;
        return setError(errorMessage, "Invalid seed.local.json: " + parseError.errorString());
    }

    const QJsonObject root = doc.object();
    const QString schema = root.value("$schema").toString();
    if (!schema.isEmpty() && schema != QStringLiteral("studywidgets.seed.v1"))
    {
        cachedSeedConfig() = cfg;
        return setError(errorMessage, "Unsupported seed configuration schema.");
    }

    cfg.loaded = true;

    const QJsonObject pathsObj = root.value("paths").toObject();
    cfg.timeLogPath = pathsObj.value("timeLogPath").toString().trimmed();
    cfg.quickInboxPath = pathsObj.value("quickInboxPath").toString().trimmed();
    cfg.obsidianVaultName = pathsObj.value("obsidianVaultName").toString().trimmed();
    cfg.scheduleImagePath = pathsObj.value("scheduleImagePath").toString().trimmed();
    cfg.scheduleBrowseDir = pathsObj.value("scheduleBrowseDir").toString().trimmed();
    cfg.roadmapBrowseDir = pathsObj.value("roadmapBrowseDir").toString().trimmed();
    cfg.defaultRoadmapPath = pathsObj.value("defaultRoadmapPath").toString().trimmed();

    const QJsonArray roadmapArr = pathsObj.value("roadmaps").toArray();
    for (const QJsonValue& val : roadmapArr)
    {
        if (!val.isObject())
        {
            continue;
        }
        const QJsonObject o = val.toObject();
        LocalRoadmapEntry entry;
        entry.label = o.value("label").toString().trimmed();
        entry.shortLabel = o.value("shortLabel").toString().trimmed();
        entry.path = o.value("path").toString().trimmed();
        entry.syncDaily = o.value("syncDaily").toBool(true);
        if (!entry.path.isEmpty())
        {
            if (entry.label.isEmpty())
            {
                entry.label = QFileInfo(entry.path).fileName();
            }
            cfg.roadmaps.append(entry);
        }
    }

    const QJsonObject uiObj = root.value("ui").toObject();
    cfg.studyProfileLabel = readNonEmpty(uiObj, "studyProfileLabel", cfg.studyProfileLabel);
    cfg.pomodoroTierALabel = readNonEmpty(uiObj, "pomodoroTierALabel", cfg.pomodoroTierALabel);
    cfg.pomodoroTierBLabel = readNonEmpty(uiObj, "pomodoroTierBLabel", cfg.pomodoroTierBLabel);
    cfg.pomodoroTierCLabel = readNonEmpty(uiObj, "pomodoroTierCLabel", cfg.pomodoroTierCLabel);
    cfg.scheduleTitle = readNonEmpty(uiObj, "scheduleTitle", cfg.scheduleTitle);
    cfg.scheduleSubtitle = readNonEmpty(uiObj, "scheduleSubtitle", cfg.scheduleSubtitle);
    cfg.scheduleHeaderShort = readNonEmpty(uiObj, "scheduleHeaderShort", cfg.scheduleHeaderShort);
    cfg.weekEvenLabel = readNonEmpty(uiObj, "weekEvenLabel", cfg.weekEvenLabel);
    cfg.weekOddLabel = readNonEmpty(uiObj, "weekOddLabel", cfg.weekOddLabel);
    cfg.weekEvenShort = readNonEmpty(uiObj, "weekEvenShort", cfg.weekEvenShort);
    cfg.weekOddShort = readNonEmpty(uiObj, "weekOddShort", cfg.weekOddShort);
    cfg.calendarDialogExample = readNonEmpty(uiObj, "calendarDialogExample", cfg.calendarDialogExample);
    cfg.calendarQuickHint = readNonEmpty(uiObj, "calendarQuickHint", cfg.calendarQuickHint);
    cfg.goalsAutoSyncLabel = readNonEmpty(uiObj, "goalsAutoSyncLabel", cfg.goalsAutoSyncLabel);
    cfg.studyEventTag = readNonEmpty(uiObj, "studyEventTag", cfg.studyEventTag);
    cfg.roadmapTaskPrefix = readNonEmpty(uiObj, "roadmapTaskPrefix", cfg.roadmapTaskPrefix);
    cfg.wisdomFlowHint = readNonEmpty(uiObj, "wisdomFlowHint", cfg.wisdomFlowHint);
    cfg.wisdomAmanahHint = readNonEmpty(uiObj, "wisdomAmanahHint", cfg.wisdomAmanahHint);

    const QJsonObject prayerObj = root.value("prayer").toObject();
    cfg.prayerCityName = readNonEmpty(prayerObj, "cityName", cfg.prayerCityName);
    cfg.prayerPresetName = readNonEmpty(prayerObj, "presetName", cfg.prayerPresetName);
    if (prayerObj.contains("latitude"))
    {
        cfg.prayerLatitude = prayerObj.value("latitude").toDouble(cfg.prayerLatitude);
    }
    if (prayerObj.contains("longitude"))
    {
        cfg.prayerLongitude = prayerObj.value("longitude").toDouble(cfg.prayerLongitude);
    }
    if (prayerObj.contains("elevation"))
    {
        cfg.prayerElevation = prayerObj.value("elevation").toDouble(cfg.prayerElevation);
    }
    const QJsonArray methodArr = prayerObj.value("methods").toArray();
    if (!methodArr.isEmpty())
    {
        QList<LocalPrayerMethod> parsedMethods;
        for (const QJsonValue& val : methodArr)
        {
            if (!val.isObject())
            {
                continue;
            }
            const QJsonObject mo = val.toObject();
            LocalPrayerMethod pm;
            pm.menuLabel = mo.value("menuLabel").toString().trimmed();
            pm.badgeLabel = mo.value("badgeLabel").toString().trimmed();
            pm.fajrAngle = mo.value("fajrAngle").toDouble(12.0);
            pm.ishaAngle = mo.value("ishaAngle").toDouble(12.0);
            pm.tuneFajr = mo.value("tuneFajr").toInt(0);
            pm.tuneSunrise = mo.value("tuneSunrise").toInt(0);
            pm.tuneDhuhr = mo.value("tuneDhuhr").toInt(0);
            pm.tuneAsr = mo.value("tuneAsr").toInt(0);
            pm.tuneMaghrib = mo.value("tuneMaghrib").toInt(0);
            pm.tuneIsha = mo.value("tuneIsha").toInt(0);
            if (!pm.menuLabel.isEmpty())
            {
                if (pm.badgeLabel.isEmpty())
                {
                    pm.badgeLabel = pm.menuLabel;
                }
                parsedMethods.append(pm);
            }
        }
        if (!parsedMethods.isEmpty())
        {
            cfg.prayerMethods = parsedMethods;
        }
    }

    const QJsonArray goalsArr = root.value("goals").toArray();
    for (const QJsonValue& val : goalsArr)
    {
        if (!val.isObject())
        {
            continue;
        }
        const QJsonObject go = val.toObject();
        LocalGoalSeed g;
        g.horizon = go.value("horizon").toString().trimmed();
        g.title = go.value("title").toString().trimmed();
        g.targetValue = go.value("target").toInt(1);
        g.currentValue = go.value("current").toInt(0);
        if (!g.horizon.isEmpty() && !g.title.isEmpty())
        {
            cfg.goals.append(g);
        }
    }

    const QJsonArray revArr = root.value("spacedReviews").toArray();
    for (const QJsonValue& val : revArr)
    {
        if (!val.isObject())
        {
            continue;
        }
        const QJsonObject ro = val.toObject();
        LocalSpacedReviewSeed r;
        r.categoryId = ro.value("categoryId").toString().trimmed();
        r.topic = ro.value("topic").toString().trimmed();
        r.kind = ro.value("kind").toString(QStringLiteral("cviceni")).trimmed();
        if (!r.categoryId.isEmpty() && !r.topic.isEmpty())
        {
            cfg.spacedReviews.append(r);
        }
    }

    const QJsonArray evArr = root.value("calendarEvents").toArray();
    for (const QJsonValue& val : evArr)
    {
        if (!val.isObject())
        {
            continue;
        }
        const QJsonObject eo = val.toObject();
        LocalCalendarEventSeed e;
        e.dayOffset = eo.value("dayOffset").toInt(0);
        e.title = eo.value("title").toString().trimmed();
        e.timeText = eo.value("time").toString().trimmed();
        e.tag = eo.value("tag").toString(QStringLiteral("general")).trimmed();
        if (!e.title.isEmpty())
        {
            cfg.calendarEvents.append(e);
        }
    }

    const QJsonObject semObj = root.value("semester").toObject();
    if (semObj.contains("startDate"))
    {
        const QDate d = QDate::fromString(semObj.value("startDate").toString(), Qt::ISODate);
        if (d.isValid())
        {
            cfg.semesterStartDate = d;
        }
    }
    if (semObj.contains("teachingEndDate"))
    {
        const QDate d = QDate::fromString(semObj.value("teachingEndDate").toString(), Qt::ISODate);
        if (d.isValid())
        {
            cfg.semesterTeachingEndDate = d;
        }
    }
    if (semObj.contains("endDate"))
    {
        const QDate d = QDate::fromString(semObj.value("endDate").toString(), Qt::ISODate);
        if (d.isValid())
        {
            cfg.semesterEndDate = d;
        }
    }

    auto parseDayRules = [](const QJsonObject& dayMapObj, QMap<int, LocalSemesterDayRule>& target)
    {
        for (int dow = 1; dow <= 7; ++dow)
        {
            const QString key = QString::number(dow);
            if (!dayMapObj.contains(key) || !dayMapObj.value(key).isObject())
            {
                continue;
            }
            const QJsonObject ruleObj = dayMapObj.value(key).toObject();
            LocalSemesterDayRule rule;
            rule.timeText = ruleObj.value("time").toString(QStringLiteral("18:00")).trimmed();
            rule.text = ruleObj.value("text").toString().trimmed();
            rule.evenText = ruleObj.value("even").toString().trimmed();
            rule.oddText = ruleObj.value("odd").toString().trimmed();
            target.insert(dow, rule);
        }
    };
    parseDayRules(semObj.value("teachingDays").toObject(), cfg.teachingDays);
    parseDayRules(semObj.value("examDays").toObject(), cfg.examDays);

    const QJsonArray hgArr = semObj.value("horizonGoals").toArray();
    for (const QJsonValue& val : hgArr)
    {
        if (!val.isObject())
        {
            continue;
        }
        const QJsonObject ho = val.toObject();
        LocalHorizonGoalSeed hg;
        hg.horizon = ho.value("horizon").toString().trimmed();
        hg.title = ho.value("title").toString().trimmed();
        hg.targetValue = ho.value("target").toInt(1);
        hg.currentValue = ho.value("current").toInt(0);
        hg.sortOrder = ho.value("sortOrder").toInt(0);
        if (!hg.horizon.isEmpty() && !hg.title.isEmpty())
        {
            cfg.semesterHorizonGoals.append(hg);
        }
    }

    const QJsonObject curObj = root.value("dailyCurriculum").toObject();
    if (curObj.contains("anchorDate"))
    {
        const QDate d = QDate::fromString(curObj.value("anchorDate").toString(), Qt::ISODate);
        if (d.isValid())
        {
            cfg.curriculumAnchorDate = d;
        }
    }
    cfg.curriculumTime = readNonEmpty(curObj, "defaultTime", cfg.curriculumTime);
    const QJsonArray curArr = curObj.value("items").toArray();
    for (const QJsonValue& val : curArr)
    {
        const QString item = val.toString().trimmed();
        if (!item.isEmpty())
        {
            cfg.curriculumItems.append(item);
        }
    }

    cachedSeedConfig() = cfg;
    return true;
}

QDate CategoryConfigLoader::semesterStartDate()
{
    QSettings settings;
    const QString val = settings.value("study/semesterStart").toString();
    if (!val.isEmpty())
    {
        const QDate d = QDate::fromString(val, Qt::ISODate);
        if (d.isValid())
        {
            return d;
        }
    }
    const LocalSeedConfig& seed = seedConfig();
    if (seed.semesterStartDate.isValid())
    {
        return seed.semesterStartDate;
    }
    return s_semesterStartDate;
}

void CategoryConfigLoader::setSemesterStartDate(const QDate& date)
{
    if (date.isValid())
    {
        s_semesterStartDate = date;
        QSettings settings;
        settings.setValue("study/semesterStart", date.toString(Qt::ISODate));
    }
}

QString CategoryConfigLoader::timeLogFilePath()
{
    QSettings settings;
    const QString saved = settings.value("study/timeLogPath").toString().trimmed();
    if (!saved.isEmpty())
    {
        return saved;
    }

    if (!s_timeLogFilePath.isEmpty())
    {
        return s_timeLogFilePath;
    }

    const LocalSeedConfig& seed = seedConfig();
    if (!seed.timeLogPath.isEmpty())
    {
        return seed.timeLogPath;
    }

    const QString appDataDirectory =
        QStandardPaths::writableLocation(
            QStandardPaths::AppDataLocation
        );
    return QDir(appDataDirectory).filePath("TimeLog.md");
}

void CategoryConfigLoader::setTimeLogFilePath(const QString& path)
{
    if (!path.isEmpty())
    {
        s_timeLogFilePath = path;
        QSettings settings;
        settings.setValue("study/timeLogPath", path);
    }
}

bool CategoryConfigLoader::importLocalCategories(
    QString* errorMessage
) {
    const QString configPath = localConfigPath();

    if (!QFile::exists(configPath)) {
        return true;
    }

    QFile file(configPath);

    if (!file.open(QIODevice::ReadOnly)) {
        return setError(
            errorMessage,
            "Could not open local category configuration."
        );
    }

    QJsonParseError parseError;

    const QJsonDocument document =
        QJsonDocument::fromJson(
            file.readAll(),
            &parseError
        );

    if (parseError.error != QJsonParseError::NoError) {
        return setError(
            errorMessage,
            "Invalid categories.local.json: " +
            parseError.errorString()
        );
    }

    if (!document.isObject()) {
        return setError(
            errorMessage,
            "Category configuration root must be a JSON object."
        );
    }

    const QJsonObject root = document.object();

    if (root.value("$schema").toString() !=
        "studywidgets.categories.v1") {
        return setError(
            errorMessage,
            "Unsupported category configuration schema."
        );
    }

    // Root options
    if (root.contains("semesterStart")) {
        const QDate sDate = QDate::fromString(root.value("semesterStart").toString(), "yyyy-MM-dd");
        if (sDate.isValid()) {
            setSemesterStartDate(sDate);
        }
    }

    if (root.contains("timeLogPath")) {
        const QString tPath = root.value("timeLogPath").toString().trimmed();
        if (!tPath.isEmpty()) {
            setTimeLogFilePath(tPath);
        }
    }

    const QJsonValue categoriesValue =
        root.value("categories");

    if (!categoriesValue.isArray()) {
        return setError(
            errorMessage,
            "Category configuration must contain a categories array."
        );
    }

    const QJsonArray categoryArray =
        categoriesValue.toArray();

    QList<StudyCategory> categories;
    QSet<QString> knownIds;

    for (int index = 0; index < categoryArray.size(); ++index) {
        if (!categoryArray.at(index).isObject()) {
            return setError(
                errorMessage,
                "Every category must be a JSON object."
            );
        }

        const QJsonObject object =
            categoryArray.at(index).toObject();

        StudyCategory category;

        category.id =
            object.value("id").toString().trimmed();

        category.name =
            object.value("name").toString().trimmed();

        category.shortName =
            object.value("short").toString().trimmed();

        category.type =
            object.value("type").toString().trimmed();

        category.color =
            object.value("color").toString().trimmed();

        category.tier =
            object.value("tier").toString().trimmed().toUpper();
        if (category.tier.isEmpty()) {
            category.tier = QStringLiteral("F");
        }

        category.vaultName =
            object.value("vaultName").toString().trimmed();
        if (category.vaultName.isEmpty()) {
            category.vaultName = category.shortName.isEmpty() ? category.name : category.shortName;
        }

        category.done =
            object.value("done").toBool(false);

        category.courseLimit =
            object.value("courseLimit").toInt(0);

        category.sortOrder =
            object.value("sortOrder").toInt();

        category.targetMinutes =
            object.value("targetMinutes").toInt(1500);

        category.archived =
            object.value("archived").toBool(false);

        if (!isValidCategoryId(category.id)) {
            return setError(
                errorMessage,
                "Invalid category ID: " + category.id
            );
        }

        if (category.name.isEmpty()) {
            return setError(
                errorMessage,
                "Category name cannot be empty."
            );
        }

        if (category.type != "subject" &&
            category.type != "general" &&
            category.type != "project") {
            return setError(
                errorMessage,
                "Invalid category type for: " + category.id
            );
        }

        if (!isValidColor(category.color)) {
            return setError(
                errorMessage,
                "Invalid category color for: " + category.id
            );
        }

        if (knownIds.contains(category.id)) {
            return setError(
                errorMessage,
                "Duplicate category ID: " + category.id
            );
        }

        knownIds.insert(category.id);
        categories.append(category);
    }

    if (!DataStore::instance().syncStudyCategories(categories)) {
        return setError(
            errorMessage,
            DataStore::instance().lastError()
        );
    }

    return true;
}
