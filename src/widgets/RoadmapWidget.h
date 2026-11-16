#pragma once

#include "../WidgetBase.h"

#include <QList>
#include <QPair>
#include <QString>

class QLabel;
class QProgressBar;
class QVBoxLayout;

class RoadmapWidget : public WidgetBase
{
public:
    struct RoadmapTask
    {
        int lineIndex = -1;
        QString sectionTitle;
        QString text;
        bool completed = false;
        bool isCheckbox = true;
    };

    struct RoadmapSection
    {
        QString title;
        int totalTasks = 0;
        int completedTasks = 0;
    };

    explicit RoadmapWidget(QWidget* parent = nullptr);

protected:
    void buildContextMenu(QMenu* menu) override;

private:
    void populatePresetFiles();
    void chooseMarkdownFile();
    void loadAndParseMarkdown(const QString& filePath);
    void toggleTaskAtLine(int lineIndex, bool completed);

    QList<QPair<QString, QString>> m_presetFiles;

    QLabel* m_currentLevelLabel = nullptr;
    QLabel* m_statsSummaryLabel = nullptr;
    QLabel* m_fileBadgeLabel = nullptr;
    QLabel* m_nextRequirementLabel = nullptr;
    QProgressBar* m_levelBar = nullptr;

    QWidget* m_taskListContainer = nullptr;
    QVBoxLayout* m_taskListLayout = nullptr;

    QString m_currentFilePath;
};
