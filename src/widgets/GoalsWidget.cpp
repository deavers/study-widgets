#include "GoalsWidget.h"

#include "../CategoryConfigLoader.h"
#include "../DataStore.h"
#include "../WidgetRegistry.h"

#include <QCheckBox>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QTextStream>
#include <QTimer>

namespace {

QString quickInboxPath()
{
    const QString configured = CategoryConfigLoader::seedConfig().quickInboxPath.trimmed();
    if (!configured.isEmpty())
    {
        const QFileInfo info(configured);
        if (info.isDir())
        {
            return QDir(configured).filePath(QStringLiteral("Quick_Inbox.md"));
        }
        if (info.dir().exists())
        {
            return configured;
        }
    }
    return QDir(DataStore::instance().appDataDir()).filePath(QStringLiteral("Quick_Inbox.md"));
}

} // namespace

GoalsWidget::GoalsWidget(QWidget* parent)
    : WidgetBase("goals", parent)
{
    setBaseWidgetSize(390, 390);
    setChromeTitle("Цели", "0/0");
    setRootMargins(12, 10, 12, 10);

    auto* tabsRow = new QHBoxLayout();
    tabsRow->setSpacing(5);

    const QList<QPair<QString, QString>> horizons = {
        {"day", QString::fromUtf8("День")},
        {"week", QString::fromUtf8("Неделя")},
        {"month", QString::fromUtf8("Месяц")},
        {"year", QString::fromUtf8("Год")}};

    for (const auto& pair : horizons) {
        auto* btn = new QPushButton(pair.second);
        btn->setProperty("variant", "chip");
        btn->setCheckable(true);
        m_tabButtons.insert(pair.first, btn);
        tabsRow->addWidget(btn, 1);

        const QString key = pair.first;
        connect(btn, &QPushButton::clicked, this, [this, key]() { switchHorizon(key); });
    }

    m_summaryLabel = new QLabel();
    m_summaryLabel->setProperty("role", "pill");
    tabsRow->addWidget(m_summaryLabel);

    bodyLayout()->addLayout(tabsRow);

    auto* inputRow = new QHBoxLayout();
    inputRow->setSpacing(6);

    m_titleInput = new QLineEdit();
    m_titleInput->setPlaceholderText(QString::fromUtf8("Новая цель или быстрая мысль"));

    m_targetSpin = new QSpinBox();
    m_targetSpin->setRange(1, 5000);
    m_targetSpin->setValue(1);
    m_targetSpin->setPrefix(QString::fromUtf8("×"));
    m_targetSpin->setToolTip(QString::fromUtf8("Сколько раз нужно выполнить"));
    m_targetSpin->setFixedWidth(66);
    m_targetSpin->setButtonSymbols(QAbstractSpinBox::NoButtons);

    auto* addBtn = new QPushButton(Theme::glyph(Theme::Glyph::Add));
    addBtn->setProperty("variant", "icon");
    addBtn->setFixedSize(30, 30);
    addBtn->setToolTip(QString::fromUtf8("Добавить как цель"));

    auto* inboxBtn = new QPushButton(Theme::glyph(Theme::Glyph::Note));
    inboxBtn->setProperty("variant", "icon");
    inboxBtn->setFixedSize(30, 30);
    inboxBtn->setToolTip(QString::fromUtf8("Отправить мысль в Quick Inbox (Obsidian)"));

    inputRow->addWidget(m_titleInput, 1);
    inputRow->addWidget(m_targetSpin);
    inputRow->addWidget(addBtn);
    inputRow->addWidget(inboxBtn);
    bodyLayout()->addLayout(inputRow);

    auto* scrollArea = new QScrollArea();
    scrollArea->setWidgetResizable(true);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    m_listContainer = new QWidget();
    m_listContainer->setStyleSheet("background: transparent;");
    m_listLayout = new QVBoxLayout(m_listContainer);
    m_listLayout->setContentsMargins(0, 0, 4, 0);
    m_listLayout->setSpacing(7);
    m_listLayout->addStretch();

    scrollArea->setWidget(m_listContainer);
    bodyLayout()->addWidget(scrollArea, 1);

    connect(addBtn, &QPushButton::clicked, this, &GoalsWidget::addGoalFromInput);
    connect(m_titleInput, &QLineEdit::returnPressed, this, &GoalsWidget::addGoalFromInput);

    connect(inboxBtn, &QPushButton::clicked, this, [this]() {
        const QString note = m_titleInput->text().trimmed();
        if (note.isEmpty()) {
            return;
        }
        const QString line =
            QString("- [%1] %2\n").arg(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm"), note);

        QFile f(quickInboxPath());
        if (f.open(QIODevice::Append | QIODevice::Text))
        {
            QTextStream out(&f);
            out.setEncoding(QStringConverter::Utf8);
            out << line;
            f.close();
            m_titleInput->clear();
            m_summaryLabel->setText(QString::fromUtf8("в Inbox"));
        }
        else
        {
            m_summaryLabel->setText(QString::fromUtf8("ошибка записи"));
        }
    });

    DataStore::instance().registerChangeListener(this, [this]() {
        rebuildGoalsList();
    });

    switchHorizon("day");
}

void GoalsWidget::buildContextMenu(QMenu* menu)
{
    const QList<QPair<QString, QString>> horizons = {
        {"day", QString::fromUtf8("Горизонт: День")},
        {"week", QString::fromUtf8("Горизонт: Неделя")},
        {"month", QString::fromUtf8("Горизонт: Месяц")},
        {"year", QString::fromUtf8("Горизонт: Год")}};

    for (const auto& pair : horizons) {
        QAction* act = menu->addAction(pair.second);
        act->setCheckable(true);
        act->setChecked(m_currentHorizon == pair.first);
        const QString key = pair.first;
        connect(act, &QAction::triggered, this, [this, key]() { switchHorizon(key); });
    }

    menu->addSeparator();
    QAction* syncAutoAct = menu->addAction(
        CategoryConfigLoader::seedConfig().goalsAutoSyncLabel
    );
    connect(syncAutoAct, &QAction::triggered, this, [this]() {
        DataStore::instance().ensureSemesterScheduleAndAutoTasks(true);
        DataStore::instance().syncDailyTasksWithCalendarAndRoadmaps(QDate::currentDate());
        DataStore::instance().notifyListeners();
        rebuildGoalsList();
    });

    QAction* clearAct = menu->addAction(QString::fromUtf8("Очистить выполненные цели этого горизонта"));
    connect(clearAct, &QAction::triggered, this, [this]() {
        DataStore::instance().clearCompletedGoals(m_currentHorizon);
        rebuildGoalsList();
    });
}

void GoalsWidget::switchHorizon(const QString& horizon)
{
    m_currentHorizon = horizon;
    for (auto it = m_tabButtons.cbegin(); it != m_tabButtons.cend(); ++it) {
        it.value()->setChecked(it.key() == horizon);
    }
    rebuildGoalsList();
}

void GoalsWidget::addGoalFromInput()
{
    const QString title = m_titleInput->text().trimmed();
    if (title.isEmpty()) {
        return;
    }
    if (DataStore::instance().addGoal(m_currentHorizon, title, m_targetSpin->value())) {
        m_titleInput->clear();
        m_targetSpin->setValue(1);
        rebuildGoalsList();
    }
}

void GoalsWidget::rebuildGoalsList()
{
    while (QLayoutItem* item = m_listLayout->takeAt(0)) {
        if (QWidget* w = item->widget()) {
            w->deleteLater();
        }
        delete item;
    }

    const QList<GoalItem> goals = DataStore::instance().goalsByHorizon(m_currentHorizon);
    int completedCount = 0;

    for (const GoalItem& g : goals) {
        if (g.completed) {
            ++completedCount;
        }

        auto* card = new QWidget();
        card->setProperty("role", "card");
        card->setProperty("state", g.completed ? "done" : "");

        auto* cardLayout = new QVBoxLayout(card);
        cardLayout->setContentsMargins(11, 9, 9, 9);
        cardLayout->setSpacing(7);

        auto* topRow = new QHBoxLayout();
        topRow->setSpacing(8);

        auto* check = new QCheckBox();
        check->setChecked(g.completed);

        auto* titleLbl = new QLabel(g.title);
        titleLbl->setWordWrap(true);
        titleLbl->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        if (g.completed) {
            titleLbl->setProperty("role", "dim");
            titleLbl->setStyleSheet("text-decoration: line-through;");
        }

        auto* delBtn = new QPushButton(Theme::glyph(Theme::Glyph::Cancel));
        delBtn->setProperty("variant", "icon");
        delBtn->setStyleSheet("QPushButton { background: transparent; border: none; color: #7B819F; }"
                              "QPushButton:hover { color: #F29AA8; }");
        delBtn->setFixedSize(22, 22);

        topRow->addWidget(check, 0, Qt::AlignTop);
        topRow->addWidget(titleLbl, 1);
        topRow->addWidget(delBtn, 0, Qt::AlignTop);
        cardLayout->addLayout(topRow);

        const qint64 id = g.id;
        connect(check, &QCheckBox::toggled, this, [this, id](bool checked) {
            DataStore::instance().toggleGoal(id, checked);
            QTimer::singleShot(0, this, &GoalsWidget::rebuildGoalsList);
        });
        connect(delBtn, &QPushButton::clicked, this, [this, id]() {
            DataStore::instance().removeGoal(id);
            QTimer::singleShot(0, this, &GoalsWidget::rebuildGoalsList);
        });

        if (g.targetValue > 1) {
            auto* progRow = new QHBoxLayout();
            progRow->setSpacing(7);

            auto* bar = new QProgressBar();
            bar->setRange(0, g.targetValue);
            bar->setValue(qBound(0, g.currentValue, g.targetValue));
            bar->setTextVisible(false);

            auto* count = new QLabel(QString("%1/%2").arg(g.currentValue).arg(g.targetValue));
            count->setProperty("role", "monoDim");
            count->setMinimumWidth(44);
            count->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

            auto* minusBtn = new QPushButton(Theme::glyph(Theme::Glyph::Remove));
            minusBtn->setProperty("variant", "icon");
            minusBtn->setFixedSize(24, 22);
            auto* plusBtn = new QPushButton(Theme::glyph(Theme::Glyph::Add));
            plusBtn->setProperty("variant", "icon");
            plusBtn->setFixedSize(24, 22);

            progRow->addWidget(bar, 1);
            progRow->addWidget(count);
            progRow->addWidget(minusBtn);
            progRow->addWidget(plusBtn);
            cardLayout->addLayout(progRow);

            connect(minusBtn, &QPushButton::clicked, this, [this, id]() {
                DataStore::instance().adjustGoalProgress(id, -1);
                QTimer::singleShot(0, this, &GoalsWidget::rebuildGoalsList);
            });
            connect(plusBtn, &QPushButton::clicked, this, [this, id]() {
                DataStore::instance().adjustGoalProgress(id, 1);
                QTimer::singleShot(0, this, &GoalsWidget::rebuildGoalsList);
            });
        }

        m_listLayout->addWidget(card);
    }

    m_listLayout->addStretch();
    m_summaryLabel->setText(QString("%1/%2").arg(completedCount).arg(goals.size()));
    setChromeTitle(QString::fromUtf8("Цели"), m_summaryLabel->text());
}

REGISTER_WIDGET("goals", "Цели и Quick Inbox", GoalsWidget)
