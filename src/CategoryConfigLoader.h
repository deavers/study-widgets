#pragma once

#include <QString>

class CategoryConfigLoader
{
public:
    static QString localConfigPath();

    static bool importLocalCategories(
        QString* errorMessage = nullptr
    );
};