#include "RoadmapWidget.h"

#include "../CategoryConfigLoader.h"
#include "../DataStore.h"
#include "../Theme.h"
#include "../WidgetRegistry.h"

#include <QCheckBox>
#include <QCoreApplication>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QProgressBar>
#include <QPushButton>
#include <QRegularExpression>
#include <QSaveFile>
#include <QScrollArea>
#include <QSettings>
#include <QStandardPaths>
#include <QTextStream>
#include <QTimer>
#include <QVBoxLayout>

RoadmapWidget::RoadmapWidget(QWidget* parent)
    : WidgetBase("roadmap", parent)
{
    setBaseWidgetSize(390, 390);
    setChromeTitle(QStringLiteral("Roadmap"), QStringLiteral("tracker"));
    setRootMargins(12, 10, 12, 10);

    auto* rootLayout = bodyLayout();
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(6);

    m_currentLevelLabel = new QLabel();
    m_currentLevelLabel->setWordWrap(true);
    m_currentLevelLabel->setFont(Theme::fontUi(11, QFont::DemiBold));
    m_currentLevelLabel->setStyleSheet(QStringLiteral(
        "background-color: %1; border: 1px solid %2; border-radius: 8px; "
        "color: %3; padding: 6px 9px;"
    ).arg(Theme::surface1().name(QColor::HexArgb),
          Theme::accentColor().name(),
          Theme::accentColor().name()));
    rootLayout->addWidget(m_currentLevelLabel);

    auto* statsRow = new QHBoxLayout();
    statsRow->setSpacing(6);

    m_statsSummaryLabel = new QLabel();
    m_statsSummaryLabel->setProperty("role", "monoDim");
    m_statsSummaryLabel->setStyleSheet("font-size: 10px;");
    statsRow->addWidget(m_statsSummaryLabel, 1);

    m_fileBadgeLabel = new QLabel();
    m_fileBadgeLabel->setProperty("role", "pill");
    m_fileBadgeLabel->setToolTip(QString::fromUtf8("ПКМ — сменить или обновить .md файл Roadmap"));
    statsRow->addWidget(m_fileBadgeLabel);

    rootLayout->addLayout(statsRow);

    m_levelBar = new QProgressBar();
    m_levelBar->setTextVisible(false);
    rootLayout->addWidget(m_levelBar);

    m_nextRequirementLabel = new QLabel();
    m_nextRequirementLabel->setFont(Theme::fontMono(9, QFont::DemiBold));
    m_nextRequirementLabel->setStyleSheet(QStringLiteral("color: %1;").arg(Theme::statusOk().name()));
    rootLayout->addWidget(m_nextRequirementLabel);

    auto* scrollArea = new QScrollArea();
    scrollArea->setWidgetResizable(true);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    m_taskListContainer = new QWidget();
    m_taskListContainer->setStyleSheet("background: transparent;");
    m_taskListLayout = new QVBoxLayout(m_taskListContainer);
    m_taskListLayout->setContentsMargins(0, 0, 4, 0);
    m_taskListLayout->setSpacing(5);
    m_taskListLayout->addStretch();

    scrollArea->setWidget(m_taskListContainer);
    rootLayout->addWidget(scrollArea, 1);

    populatePresetFiles();

    DataStore::instance().registerChangeListener(this, [this]() {
        if (!m_currentFilePath.isEmpty()) {
            loadAndParseMarkdown(m_currentFilePath);
        }
    });

    if (!m_currentFilePath.isEmpty()) {
        loadAndParseMarkdown(m_currentFilePath);
    }
}

void RoadmapWidget::buildContextMenu(QMenu* menu)
{
    QAction* openAct = menu->addAction(QString::fromUtf8("📁 Выбрать другой .md файл Roadmap..."));
    QAction* syncAct = menu->addAction(QString::fromUtf8("↻ Перечитать текущий .md файл с диска"));

    connect(openAct, &QAction::triggered, this, &RoadmapWidget::chooseMarkdownFile);
    connect(syncAct, &QAction::triggered, this, [this]() {
        loadAndParseMarkdown(m_currentFilePath);
    });

    if (!m_presetFiles.isEmpty()) {
        QMenu* presetMenu = menu->addMenu(QString::fromUtf8("Быстрый выбор Roadmap (.md)"));
        for (const auto& pair : m_presetFiles) {
            QAction* act = presetMenu->addAction(pair.first);
            act->setCheckable(true);
            act->setChecked(pair.second == m_currentFilePath);
            const QString path = pair.second;
            connect(act, &QAction::triggered, this, [this, path]() {
                QSettings settings;
                settings.setValue("roadmap/currentFile", path);
                loadAndParseMarkdown(path);
            });
        }
    }
}

void RoadmapWidget::populatePresetFiles()
{
    m_presetFiles.clear();

    const LocalSeedConfig& seed = CategoryConfigLoader::seedConfig();
    for (const LocalRoadmapEntry& entry : seed.roadmaps)
    {
        if (!entry.path.isEmpty() && QFileInfo::exists(entry.path))
        {
            m_presetFiles.append({entry.label, entry.path});
        }
    }

    QSettings settings;
    const QString defaultPath = seed.defaultRoadmapPath.trimmed();
    if (!defaultPath.isEmpty()
        && !settings.value("roadmap/upgradedToZeroToHeroV1", false).toBool()
        && QFileInfo::exists(defaultPath))
    {
        settings.setValue("roadmap/currentFile", defaultPath);
        settings.setValue("roadmap/upgradedToZeroToHeroV1", true);
    }

    const QString savedPath = settings.value("roadmap/currentFile").toString();
    if (!savedPath.isEmpty() && QFileInfo::exists(savedPath))
    {
        m_currentFilePath = savedPath;
        bool existsInList = false;
        for (const auto& p : m_presetFiles)
        {
            if (p.second == savedPath)
            {
                existsInList = true;
                break;
            }
        }
        if (!existsInList)
        {
            m_presetFiles.append({QFileInfo(savedPath).fileName(), savedPath});
        }
    }
    else if (!m_presetFiles.isEmpty())
    {
        m_currentFilePath = m_presetFiles.first().second;
    }
}

void RoadmapWidget::chooseMarkdownFile()
{
    const LocalSeedConfig& seed = CategoryConfigLoader::seedConfig();
    QString startDir = seed.roadmapBrowseDir.trimmed();
    if (startDir.isEmpty() || !QFileInfo::exists(startDir))
    {
        startDir = !m_currentFilePath.isEmpty()
            ? QFileInfo(m_currentFilePath).absolutePath()
            : QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    }

    const QString path = QFileDialog::getOpenFileName(
        this,
        QString::fromUtf8("Выберите Markdown файл Roadmap (.md)"),
        startDir,
        "Markdown Files (*.md *.markdown *.txt)"
    );
    if (path.isEmpty()) {
        return;
    }

    QSettings settings;
    settings.setValue("roadmap/currentFile", path);

    bool found = false;
    for (const auto& pair : m_presetFiles) {
        if (pair.second == path) {
            found = true;
            break;
        }
    }
    if (!found) {
        m_presetFiles.append({QFileInfo(path).fileName(), path});
    }

    loadAndParseMarkdown(path);
}

void RoadmapWidget::loadAndParseMarkdown(const QString& filePath)
{
    m_currentFilePath = filePath;

    QString shortFile = QFileInfo(filePath).fileName();
    if (shortFile.length() > 22) {
        shortFile = shortFile.left(19) + QStringLiteral("…");
    }
    m_fileBadgeLabel->setText(shortFile.isEmpty() ? QStringLiteral(".md") : shortFile);

    while (QLayoutItem* item = m_taskListLayout->takeAt(0)) {
        if (QWidget* w = item->widget()) {
            w->deleteLater();
        }
        delete item;
    }

    QFile file(filePath);
    if (!file.exists() || !file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        m_currentLevelLabel->setText(
            QString::fromUtf8("Файл не найден. Нажмите ПКМ → «Выбрать другой .md файл Roadmap».")
        );
        return;
    }

    QTextStream in(&file);
    in.setEncoding(QStringConverter::Utf8);
    QStringList lines;
    while (!in.atEnd()) {
        lines.append(in.readLine());
    }
    file.close();

    static const QRegularExpression headingRe(R"(^(#{1,3})\s+(.+)$)");
    static const QRegularExpression checkboxRe(R"(^\s*[-*]\s+\[([ xX])\]\s+(.+)$)");
    static const QRegularExpression bulletRe(R"(^\s*[-*]\s+\*\*([^*]+)\*\*(.*)$)");

    QList<RoadmapSection> sections;
    QList<RoadmapTask> tasks;
    QString currentSection = QString::fromUtf8("Базовый уровень");

    bool hasAnyCheckboxes = false;
    for (const QString& line : lines) {
        if (checkboxRe.match(line).hasMatch()) {
            hasAnyCheckboxes = true;
            break;
        }
    }

    for (int i = 0; i < lines.size(); ++i) {
        const QString line = lines[i];
        const QRegularExpressionMatch hm = headingRe.match(line);
        if (hm.hasMatch()) {
            currentSection = hm.captured(2).trimmed();
            RoadmapSection sec;
            sec.title = currentSection;
            sections.append(sec);
            continue;
        }

        const QRegularExpressionMatch cm = checkboxRe.match(line);
        if (cm.hasMatch()) {
            RoadmapTask t;
            t.lineIndex = i;
            t.sectionTitle = currentSection;
            t.completed = (cm.captured(1).toLower() == "x");
            t.text = cm.captured(2).trimmed();
            t.isCheckbox = true;
            tasks.append(t);

            if (sections.isEmpty()) {
                RoadmapSection sec;
                sec.title = currentSection;
                sections.append(sec);
            }
            sections.last().totalTasks++;
            if (t.completed) {
                sections.last().completedTasks++;
            }
            continue;
        }

        if (!hasAnyCheckboxes) {
            const QRegularExpressionMatch bm = bulletRe.match(line);
            if (bm.hasMatch()) {
                RoadmapTask t;
                t.lineIndex = i;
                t.sectionTitle = currentSection;
                t.completed = false;
                t.text = (bm.captured(1) + bm.captured(2)).trimmed();
                t.isCheckbox = false;
                tasks.append(t);

                if (sections.isEmpty()) {
                    RoadmapSection sec;
                    sec.title = currentSection;
                    sections.append(sec);
                }
                sections.last().totalTasks++;
            }
        }
    }

    int totalCount = 0;
    int totalDone = 0;
    for (const RoadmapTask& t : tasks) {
        ++totalCount;
        if (t.completed) {
            ++totalDone;
        }
    }

    RoadmapSection activeSec;
    activeSec.title = QString::fromUtf8("Все этапы завершены!");
    int activeIndex = 0;
    bool foundActive = false;

    for (int i = 0; i < sections.size(); ++i) {
        if (sections[i].totalTasks > 0 &&
            sections[i].completedTasks < sections[i].totalTasks) {
            activeSec = sections[i];
            activeIndex = i + 1;
            foundActive = true;
            break;
        }
    }

    if (!foundActive && !sections.isEmpty()) {
        activeSec = sections.last();
        activeIndex = sections.size();
    }

    const int remainingInLevel = qMax(0, activeSec.totalTasks - activeSec.completedTasks);

    m_currentLevelLabel->setText(
        QString::fromUtf8("⚡ Уровень %1/%2: %3")
            .arg(activeIndex)
            .arg(qMax(1, static_cast<int>(sections.size())))
            .arg(activeSec.title)
    );

    const int lvlPct = activeSec.totalTasks > 0 ? (activeSec.completedTasks * 100) / activeSec.totalTasks : 0;
    const int totPct = totalCount > 0 ? (totalDone * 100) / totalCount : 0;

    m_statsSummaryLabel->setText(
        QString::fromUtf8("Уровень: %1/%2 (%3%) · Всего: %4/%5 (%6%)")
            .arg(activeSec.completedTasks)
            .arg(activeSec.totalTasks)
            .arg(lvlPct)
            .arg(totalDone)
            .arg(totalCount)
            .arg(totPct)
    );

    m_levelBar->setRange(0, qMax(1, activeSec.totalTasks));
    m_levelBar->setValue(activeSec.completedTasks);

    m_nextRequirementLabel->setText(
        remainingInLevel > 0
            ? QString::fromUtf8("Осталось до следующего уровня (%1):").arg(remainingInLevel)
            : QString::fromUtf8("Уровень полностью закрыт! 🎉")
    );

    setChromeTitle(QStringLiteral("Roadmap"), QString("%1/%2 (%3%)").arg(totalDone).arg(totalCount).arg(totPct));

    int displayed = 0;
    for (const RoadmapTask& t : tasks) {
        if (t.completed) {
            continue;
        }
        if (displayed >= 18) {
            break;
        }
        ++displayed;

        auto* rowWidget = new QWidget();
        rowWidget->setStyleSheet(
            "background-color: #1C1F36; border: 1px solid #2F3558; border-radius: 6px;"
        );
        auto* rowLayout = new QHBoxLayout(rowWidget);
        rowLayout->setContentsMargins(8, 6, 8, 6);
        rowLayout->setSpacing(7);

        if (t.isCheckbox) {
            auto* cb = new QCheckBox();
            cb->setChecked(t.completed);
            cb->setFixedWidth(20);

            QString cleanText = t.text;
            cleanText.remove(QRegularExpression(QStringLiteral(R"(^\[v\d+[^\]]*\]\s*)"), QRegularExpression::CaseInsensitiveOption));
            cleanText.remove(QRegularExpression(QStringLiteral(R"(^\[Phase\s*\d+[^\]]*\]\s*)"), QRegularExpression::CaseInsensitiveOption));
            auto* lbl = new QLabel(cleanText);
            lbl->setWordWrap(true);
            lbl->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
            lbl->setStyleSheet("border: none; color: #EEF0F8; font-size: 12px; font-weight: 500;");

            const int lineIdx = t.lineIndex;
            connect(cb, &QCheckBox::toggled, this, [this, lineIdx](bool checked) {
                toggleTaskAtLine(lineIdx, checked);
            });
            rowLayout->addWidget(cb, 0, Qt::AlignTop);
            rowLayout->addWidget(lbl, 1);
        } else {
            QString cleanText = t.text;
            cleanText.remove(QRegularExpression(QStringLiteral(R"(^\[v\d+[^\]]*\]\s*)"), QRegularExpression::CaseInsensitiveOption));
            auto* lbl = new QLabel(QString("• %1").arg(cleanText));
            lbl->setWordWrap(true);
            lbl->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
            lbl->setStyleSheet("border: none; color: #DCE4F2; font-size: 12px;");
            rowLayout->addWidget(lbl, 1);
        }

        m_taskListLayout->addWidget(rowWidget);
    }

    m_taskListLayout->addStretch();
}

void RoadmapWidget::toggleTaskAtLine(int lineIndex, bool completed)
{
    QFile file(m_currentFilePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return;
    }

    QTextStream in(&file);
    in.setEncoding(QStringConverter::Utf8);
    QStringList lines;
    while (!in.atEnd()) {
        lines.append(in.readLine());
    }
    file.close();

    if (lineIndex < 0 || lineIndex >= lines.size()) {
        return;
    }

    static const QRegularExpression checkboxRe(R"(^(\s*[-*]\s+\[)[ xX](\]\s+.+)$)");
    const QRegularExpressionMatch m = checkboxRe.match(lines[lineIndex]);
    if (!m.hasMatch()) {
        return;
    }

    lines[lineIndex] = m.captured(1) + (completed ? "x" : " ") + m.captured(2);

    QSaveFile saveFile(m_currentFilePath);
    if (saveFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&saveFile);
        out.setEncoding(QStringConverter::Utf8);
        for (int i = 0; i < lines.size(); ++i) {
            out << lines[i] << "\n";
        }
        saveFile.commit();
    }

    DataStore::instance().syncDailyTasksWithCalendarAndRoadmaps(QDate::currentDate());
    DataStore::instance().notifyListeners();

    const QString path = m_currentFilePath;
    QTimer::singleShot(0, this, [this, path]() {
        loadAndParseMarkdown(path);
    });
}

REGISTER_WIDGET(
    "roadmap",
    "Roadmap & Прогресс уровня (.md)",
    RoadmapWidget
)
