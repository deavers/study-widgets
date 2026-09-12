#include "CategoryConfigLoader.h"

#include "DataStore.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QRegularExpression>
#include <QSet>
#include <QStandardPaths>

namespace {

bool setError(
    QString* target,
    const QString& message
) {
    if (target != nullptr) {
        *target = message;
    }

    return false;
}

bool isValidCategoryId(const QString& id) {
    static const QRegularExpression pattern(
        "^[a-z0-9][a-z0-9-]{0,63}$"
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

bool CategoryConfigLoader::importLocalCategories(
    QString* errorMessage
)
{
    const QString configPath = localConfigPath();

    if (!QFile::exists(configPath))
    {
        return true;
    }

    QFile file(configPath);

    if (!file.open(QIODevice::ReadOnly))
    {
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

    if (parseError.error != QJsonParseError::NoError)
    {
        return setError(
            errorMessage,
            "Invalid categories.local.json: " +
            parseError.errorString()
        );
    }

    if (!document.isObject())
    {
        return setError(
            errorMessage,
            "Category configuration root must be a JSON object."
        );
    }

    const QJsonObject root = document.object();

    if (root.value("$schema").toString() !=
        "studywidgets.categories.v1")
        {
        return setError(
            errorMessage,
            "Unsupported category configuration schema."
        );
    }

    const QJsonValue categoriesValue =
        root.value("categories");

    if (!categoriesValue.isArray())
    {
        return setError(
            errorMessage,
            "Category configuration must contain a categories array."
        );
    }

    const QJsonArray categoryArray =
        categoriesValue.toArray();

    QList<StudyCategory> categories;
    QSet<QString> knownIds;

    for (int index = 0;
         index < categoryArray.size();
         ++index)
        {
        if (!categoryArray.at(index).isObject())
        {
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

        category.type =
            object.value("type").toString().trimmed();

        category.color =
            object.value("color").toString().trimmed();

        category.sortOrder =
            object.value("sortOrder").toInt();

        category.archived =
            object.value("archived").toBool(false);

        if (!isValidCategoryId(category.id))
        {
            return setError(
                errorMessage,
                "Invalid category ID: " + category.id
            );
        }

        if (category.name.isEmpty())
        {
            return setError(
                errorMessage,
                "Category name cannot be empty."
            );
        }

        if (category.type != "subject" &&
            category.type != "general" &&
            category.type != "project")
            {
            return setError(
                errorMessage,
                "Invalid category type for: " + category.id
            );
        }

        if (!isValidColor(category.color))
        {
            return setError(
                errorMessage,
                "Invalid category color for: " + category.id
            );
        }

        if (knownIds.contains(category.id))
        {
            return setError(
                errorMessage,
                "Duplicate category ID: " + category.id
            );
        }

        knownIds.insert(category.id);
        categories.append(category);
    }

    if (!DataStore::instance().syncStudyCategories(categories))
    {
        return setError(
            errorMessage,
            DataStore::instance().lastError()
        );
    }

    return true;
}