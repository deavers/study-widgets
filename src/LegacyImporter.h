#pragma once

#include <QString>

class LegacyImporter
{
public:
    static QString defaultSaveDataPath();

    static bool importIfFirstRun(
        const QString& customPath = QString(),
        QString* error = nullptr
    );

    static bool importFromSaveData(
        const QString& filePath,
        QString* error = nullptr
    );
};
