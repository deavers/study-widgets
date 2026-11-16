#include "CalendarWidget.h"

#include "../CategoryConfigLoader.h"
#include "../Theme.h"
#include "../WidgetRegistry.h"

#include <QCheckBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QSettings>
#include <QStackedWidget>
#include <QTimer>
#include <QVBoxLayout>

namespace {

class WeekStripCanvas : public QWidget
{
public:
    std::function<void(const QDate&)> onDateClicked;

    explicit WeekStripCanvas(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setFixedHeight(46);
        setCursor(Qt::PointingHandCursor);
    }

    void setData(const QDate& monday,
                 const QDate& selectedDate,
                 const QSet<QDate>& eventDates,
                 const QSet<QDate>& studyDates)
    {
        m_monday = monday;
        m_selectedDate = selectedDate;
        m_eventDates = eventDates;
        m_studyDates = studyDates;
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        if (!m_monday.isValid()) {
            return;
        }

        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);

        const QDate today = QDate::currentDate();
        const qreal gap = 4.0;
        const qreal cellW = (width() - gap * 6.0) / 7.0;
        const qreal cellH = height() - 2.0;

        static const char* dayNames[7] = {"Пн", "Вт", "Ср", "Чт", "Пт", "Сб", "Вс"};

        for (int i = 0; i < 7; ++i) {
            const QDate d = m_monday.addDays(i);
            const QRectF r(i * (cellW + gap), 1.0, cellW, cellH);

            const bool isToday = (d == today);
            const bool isSelected = (d == m_selectedDate);

            QColor bg = Theme::surface1();
            QColor border = Theme::stroke();
            if (isToday) {
                bg = Theme::accentTint(52);
                border = Theme::accentColor();
            } else if (isSelected) {
                bg = Theme::surface3();
                border = Theme::strokeStrong();
            }

            p.setPen(QPen(border, (isToday || isSelected) ? 1.4 : 1.0));
            p.setBrush(bg);
            p.drawRoundedRect(r, 7.0, 7.0);

            p.setFont(Theme::fontUi(9, QFont::DemiBold));
            p.setPen(i >= 5 ? Theme::statusWarn() : Theme::textDim());
            p.drawText(QRectF(r.left(), r.top() + 3.0, r.width(), 13.0),
                       Qt::AlignCenter, QString::fromUtf8(dayNames[i]));

            p.setFont(Theme::fontMono(12, isToday ? QFont::Bold : QFont::DemiBold));
            p.setPen(isToday ? Theme::accentColor() : Theme::textColor());
            p.drawText(QRectF(r.left(), r.top() + 15.0, r.width(), 18.0),
                       Qt::AlignCenter, QString::number(d.day()));

            // Indicator dots at bottom of cell
            const bool hasEv = m_eventDates.contains(d);
            const bool hasSt = m_studyDates.contains(d);
            if (hasEv && hasSt) {
                p.setPen(Qt::NoPen);
                p.setBrush(QColor("#F2C47E"));
                p.drawEllipse(QPointF(r.center().x() - 3.5, r.bottom() - 5.0), 2.2, 2.2);
                p.setBrush(QColor("#8FD6A8"));
                p.drawEllipse(QPointF(r.center().x() + 3.5, r.bottom() - 5.0), 2.2, 2.2);
            } else if (hasEv) {
                p.setPen(Qt::NoPen);
                p.setBrush(QColor("#F2C47E"));
                p.drawEllipse(QPointF(r.center().x(), r.bottom() - 5.0), 2.4, 2.4);
            } else if (hasSt) {
                p.setPen(Qt::NoPen);
                p.setBrush(QColor("#8FD6A8"));
                p.drawEllipse(QPointF(r.center().x(), r.bottom() - 5.0), 2.2, 2.2);
            }
        }
    }

    void mousePressEvent(QMouseEvent* event) override
    {
        if (event->button() == Qt::LeftButton && m_monday.isValid()) {
            const int idx = qBound(0, static_cast<int>(event->position().x() * 7 / qMax(1, width())), 6);
            if (onDateClicked) {
                onDateClicked(m_monday.addDays(idx));
            }
            event->accept();
            return;
        }
        QWidget::mousePressEvent(event);
    }

private:
    QDate m_monday;
    QDate m_selectedDate;
    QSet<QDate> m_eventDates;
    QSet<QDate> m_studyDates;
};

class MonthGridCanvas : public QWidget
{
public:
    std::function<void(const QDate&)> onDateClicked;

    explicit MonthGridCanvas(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setMinimumHeight(132);
        setCursor(Qt::PointingHandCursor);
    }

    void setData(int year,
                 int month,
                 const QDate& selectedDate,
                 const QSet<QDate>& eventDates,
                 const QSet<QDate>& studyDates)
    {
        m_year = year;
        m_month = month;
        m_selectedDate = selectedDate;
        m_eventDates = eventDates;
        m_studyDates = studyDates;

        const QDate firstOfMonth(year, month, 1);
        const int offset = firstOfMonth.dayOfWeek() - 1; // Monday = 0
        m_gridStart = firstOfMonth.addDays(-offset);
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        if (!m_gridStart.isValid()) {
            return;
        }

        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);

        const qreal headerH = 15.0;
        const qreal parityColW = 24.0;
        const qreal gridW = width() - parityColW;
        const qreal cellW = gridW / 7.0;
        const qreal cellH = (height() - headerH) / 6.0;

        static const char* dayNames[7] = {"Пн", "Вт", "Ср", "Чт", "Пт", "Сб", "Вс"};
        p.setFont(Theme::fontUi(9, QFont::DemiBold));
        p.setPen(Theme::textDim());
        p.drawText(QRectF(0, 0, parityColW - 2, headerH), Qt::AlignCenter, QStringLiteral("Нд"));

        for (int col = 0; col < 7; ++col) {
            p.setPen(col >= 5 ? Theme::statusWarn() : Theme::textDim());
            p.drawText(QRectF(parityColW + col * cellW, 0, cellW, headerH),
                       Qt::AlignCenter, QString::fromUtf8(dayNames[col]));
        }

        const QDate today = QDate::currentDate();

        for (int row = 0; row < 6; ++row) {
            const QDate rowMonday = m_gridStart.addDays(row * 7);
            const int isoWeek = rowMonday.weekNumber();
            const bool isEven = (isoWeek % 2 == 0);
            const qreal y = headerH + row * cellH;

            const LocalSeedConfig& seed = CategoryConfigLoader::seedConfig();
            // Week parity tag
            p.setFont(Theme::fontMono(8, QFont::DemiBold));
            p.setPen(isEven ? Theme::accentColor() : QColor("#8FD6A8"));
            p.drawText(
                QRectF(0, y, parityColW - 2, cellH),
                Qt::AlignCenter,
                QString("%1%2").arg(isoWeek).arg(isEven ? seed.weekEvenShort : seed.weekOddShort)
            );

            for (int col = 0; col < 7; ++col) {
                const QDate d = m_gridStart.addDays(row * 7 + col);
                const QRectF cellRect(parityColW + col * cellW + 1.5, y + 1.0, cellW - 3.0, cellH - 2.0);

                const bool inMonth = (d.month() == m_month);
                const bool isToday = (d == today);
                const bool isSelected = (d == m_selectedDate);

                if (isToday) {
                    p.setPen(QPen(Theme::accentColor(), 1.3));
                    p.setBrush(Theme::accentTint(58));
                    p.drawRoundedRect(cellRect, 5.0, 5.0);
                } else if (isSelected) {
                    p.setPen(QPen(Theme::strokeStrong(), 1.2));
                    p.setBrush(Theme::surface2());
                    p.drawRoundedRect(cellRect, 5.0, 5.0);
                }

                p.setFont(Theme::fontMono(10, isToday ? QFont::Bold : QFont::Normal));
                if (isToday) {
                    p.setPen(Theme::accentColor());
                } else if (!inMonth) {
                    p.setPen(QColor(169, 175, 201, 130));
                } else if (col >= 5) {
                    p.setPen(QColor(242, 196, 126, 230));
                } else {
                    p.setPen(Theme::textColor());
                }

                p.drawText(cellRect.adjusted(0, -1, 0, -3), Qt::AlignCenter, QString::number(d.day()));

                const bool hasEv = m_eventDates.contains(d);
                const bool hasSt = m_studyDates.contains(d);
                if (hasEv || hasSt) {
                    p.setPen(Qt::NoPen);
                    p.setBrush(hasEv ? QColor("#F2C47E") : QColor("#8FD6A8"));
                    p.drawEllipse(QPointF(cellRect.center().x(), cellRect.bottom() - 2.5), 1.9, 1.9);
                }
            }
        }
    }

    void mousePressEvent(QMouseEvent* event) override
    {
        if (event->button() == Qt::LeftButton && m_gridStart.isValid()) {
            const qreal headerH = 15.0;
            const qreal parityColW = 24.0;
            if (event->position().y() >= headerH && event->position().x() >= parityColW) {
                const int col = qBound(0, static_cast<int>((event->position().x() - parityColW) * 7.0 / qMax(1.0, width() - parityColW)), 6);
                const int row = qBound(0, static_cast<int>((event->position().y() - headerH) * 6.0 / qMax(1.0, height() - headerH)), 5);
                if (onDateClicked) {
                    onDateClicked(m_gridStart.addDays(row * 7 + col));
                }
                event->accept();
                return;
            }
        }
        QWidget::mousePressEvent(event);
    }

private:
    int m_year = 2026;
    int m_month = 11;
    QDate m_gridStart;
    QDate m_selectedDate;
    QSet<QDate> m_eventDates;
    QSet<QDate> m_studyDates;
};

} // namespace

CalendarWidget::CalendarWidget(QWidget* parent)
    : WidgetBase("calendar", parent)
    , m_selectedDate(QDate::currentDate())
{
    setBaseWidgetSize(340, 238);
    setChromeTitle(QStringLiteral("~/cal $"), QStringLiteral("plan"));
    setRootMargins(11, 10, 11, 10);
    bodyLayout()->setSpacing(5);

    // 1. Header & Mode Bar
    auto* topRow = new QHBoxLayout();
    topRow->setSpacing(4);

    m_headerLabel = new QLabel();
    m_headerLabel->setFont(Theme::fontMono(10, QFont::Bold));
    m_headerLabel->setStyleSheet(QStringLiteral("color: %1;").arg(Theme::textColor().name()));
    topRow->addWidget(m_headerLabel, 1);

    auto makeNavBtn = [this](const QString& txt, const QString& tip) {
        auto* b = new QPushButton(txt, this);
        b->setProperty("variant", "ghost");
        b->setFixedSize(20, 20);
        b->setCursor(Qt::PointingHandCursor);
        b->setToolTip(tip);
        b->setStyleSheet("QPushButton { padding: 0; font-size: 11px; border-radius: 5px; }");
        return b;
    };

    auto* prevBtn = makeNavBtn(QStringLiteral("‹"), QString::fromUtf8("Назад"));
    auto* todayBtn = makeNavBtn(QStringLiteral("•"), QString::fromUtf8("Сегодня"));
    auto* nextBtn = makeNavBtn(QStringLiteral("›"), QString::fromUtf8("Вперёд"));

    topRow->addWidget(prevBtn);
    topRow->addWidget(todayBtn);
    topRow->addWidget(nextBtn);
    topRow->addSpacing(2);

    auto makeModeBtn = [this](const QString& text, ViewMode mode) {
        auto* b = new QPushButton(text, this);
        b->setProperty("variant", "chip");
        b->setCheckable(true);
        b->setFixedHeight(20);
        b->setCursor(Qt::PointingHandCursor);
        b->setStyleSheet("QPushButton { padding: 1px 6px; font-size: 10px; border-radius: 6px; }");
        connect(b, &QPushButton::clicked, this, [this, mode]() { setViewMode(mode); });
        return b;
    };

    m_modeDaysBtn = makeModeBtn(QString::fromUtf8("Дни"), ViewMode::TodayTomorrow);
    m_modeWeekBtn = makeModeBtn(QString::fromUtf8("Нед"), ViewMode::Week);
    m_modeMonthBtn = makeModeBtn(QString::fromUtf8("Мес"), ViewMode::Month);

    topRow->addWidget(m_modeDaysBtn);
    topRow->addWidget(m_modeWeekBtn);
    topRow->addWidget(m_modeMonthBtn);

    bodyLayout()->addLayout(topRow);

    // 2. Stacked Views (Today/Tomorrow | Week | Month)
    m_stack = new QStackedWidget(this);

    // --- Page 0: Today / Tomorrow ---
    auto* pageDays = new QWidget();
    auto* pageDaysLayout = new QVBoxLayout(pageDays);
    pageDaysLayout->setContentsMargins(0, 0, 0, 0);
    pageDaysLayout->setSpacing(0);

    auto* daysScroll = new QScrollArea();
    daysScroll->setWidgetResizable(true);
    daysScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_daysContainer = new QWidget();
    m_daysContainer->setStyleSheet("background: transparent;");
    m_daysLayout = new QVBoxLayout(m_daysContainer);
    m_daysLayout->setContentsMargins(0, 0, 2, 0);
    m_daysLayout->setSpacing(5);
    daysScroll->setWidget(m_daysContainer);
    pageDaysLayout->addWidget(daysScroll, 1);
    m_stack->addWidget(pageDays);

    // --- Page 1: Week ---
    auto* pageWeek = new QWidget();
    auto* pageWeekLayout = new QVBoxLayout(pageWeek);
    pageWeekLayout->setContentsMargins(0, 0, 0, 0);
    pageWeekLayout->setSpacing(4);

    auto* weekStrip = new WeekStripCanvas(pageWeek);
    weekStrip->onDateClicked = [this](const QDate& d) { selectDate(d); };
    m_weekStripWidget = weekStrip;
    pageWeekLayout->addWidget(weekStrip);

    m_weekDayHeaderLabel = new QLabel();
    m_weekDayHeaderLabel->setProperty("role", "monoDim");
    m_weekDayHeaderLabel->setStyleSheet("font-size: 10px;");
    pageWeekLayout->addWidget(m_weekDayHeaderLabel);

    auto* weekScroll = new QScrollArea();
    weekScroll->setWidgetResizable(true);
    weekScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_weekEventsContainer = new QWidget();
    m_weekEventsContainer->setStyleSheet("background: transparent;");
    m_weekEventsLayout = new QVBoxLayout(m_weekEventsContainer);
    m_weekEventsLayout->setContentsMargins(0, 0, 2, 0);
    m_weekEventsLayout->setSpacing(4);
    weekScroll->setWidget(m_weekEventsContainer);
    pageWeekLayout->addWidget(weekScroll, 1);
    m_stack->addWidget(pageWeek);

    // --- Page 2: Month ---
    auto* pageMonth = new QWidget();
    auto* pageMonthLayout = new QVBoxLayout(pageMonth);
    pageMonthLayout->setContentsMargins(0, 0, 0, 0);
    pageMonthLayout->setSpacing(3);

    auto* monthGrid = new MonthGridCanvas(pageMonth);
    monthGrid->onDateClicked = [this](const QDate& d) { selectDate(d); };
    m_monthGridWidget = monthGrid;
    pageMonthLayout->addWidget(monthGrid, 1);

    m_monthSelectedSummary = new QLabel();
    m_monthSelectedSummary->setProperty("role", "monoDim");
    m_monthSelectedSummary->setStyleSheet("font-size: 10px;");
    pageMonthLayout->addWidget(m_monthSelectedSummary);
    m_stack->addWidget(pageMonth);

    bodyLayout()->addWidget(m_stack, 1);

    // 3. Bottom Quick-Add Bar
    auto* addRow = new QHBoxLayout();
    addRow->setSpacing(5);

    m_quickInput = new QLineEdit();
    m_quickInput->setFixedHeight(24);
    m_quickInput->setStyleSheet("QLineEdit { padding: 2px 7px; font-size: 11px; border-radius: 6px; }");

    auto* addBtn = new QPushButton(Theme::glyph(Theme::Glyph::Add));
    addBtn->setProperty("variant", "icon");
    addBtn->setFixedSize(24, 24);
    addBtn->setCursor(Qt::PointingHandCursor);
    addBtn->setToolTip(QString::fromUtf8("Добавить событие на выбранную дату"));

    addRow->addWidget(m_quickInput, 1);
    addRow->addWidget(addBtn);
    bodyLayout()->addLayout(addRow);

    connect(prevBtn, &QPushButton::clicked, this, [this]() { stepPeriod(-1); });
    connect(todayBtn, &QPushButton::clicked, this, &CalendarWidget::jumpToToday);
    connect(nextBtn, &QPushButton::clicked, this, [this]() { stepPeriod(1); });
    connect(addBtn, &QPushButton::clicked, this, &CalendarWidget::addEventFromInput);
    connect(m_quickInput, &QLineEdit::returnPressed, this, &CalendarWidget::addEventFromInput);

    QSettings settings;
    const int savedMode = qBound(0, settings.value("calendar/viewMode", 1).toInt(), 2);
    setViewMode(static_cast<ViewMode>(savedMode));

    DataStore::instance().registerChangeListener(this, [this]() {
        refreshAll();
    });

    m_refreshTimer = new QTimer(this);
    m_refreshTimer->setInterval(30000);
    connect(m_refreshTimer, &QTimer::timeout, this, &CalendarWidget::refreshAll);
    m_refreshTimer->start();
}

void CalendarWidget::setViewMode(ViewMode mode)
{
    m_mode = mode;
    QSettings settings;
    settings.setValue("calendar/viewMode", static_cast<int>(mode));

    m_modeDaysBtn->setChecked(mode == ViewMode::TodayTomorrow);
    m_modeWeekBtn->setChecked(mode == ViewMode::Week);
    m_modeMonthBtn->setChecked(mode == ViewMode::Month);

    m_stack->setCurrentIndex(static_cast<int>(mode));
    refreshAll();
}

void CalendarWidget::stepPeriod(int delta)
{
    if (m_mode == ViewMode::TodayTomorrow) {
        m_selectedDate = m_selectedDate.addDays(delta);
    } else if (m_mode == ViewMode::Week) {
        m_selectedDate = m_selectedDate.addDays(delta * 7);
    } else {
        m_selectedDate = m_selectedDate.addMonths(delta);
    }
    refreshAll();
}

void CalendarWidget::jumpToToday()
{
    m_selectedDate = QDate::currentDate();
    refreshAll();
}

void CalendarWidget::selectDate(const QDate& date)
{
    if (!date.isValid()) {
        return;
    }
    m_selectedDate = date;
    refreshAll();
}

void CalendarWidget::addEventFromInput()
{
    const QString raw = m_quickInput->text().trimmed();
    if (raw.isEmpty()) {
        return;
    }

    QString timeStr;
    QString title = raw;
    static const QRegularExpression timePrefixRe(R"(^(\d{1,2}:\d{2})\s+(.+)$)");
    const QRegularExpressionMatch m = timePrefixRe.match(raw);
    if (m.hasMatch()) {
        timeStr = m.captured(1);
        title = m.captured(2).trimmed();
    }

    if (DataStore::instance().addCalendarEvent(m_selectedDate, title, timeStr)) {
        m_quickInput->clear();
        refreshAll();
    }
}

void CalendarWidget::promptAddEventDialog()
{
    bool ok = false;
    const QString text = QInputDialog::getText(
        this,
        QString::fromUtf8("Новое событие — %1").arg(m_selectedDate.toString("dd.MM.yyyy")),
        QString::fromUtf8("Введите время и название (например: %1):")
            .arg(CategoryConfigLoader::seedConfig().calendarDialogExample),
        QLineEdit::Normal,
        QString(),
        &ok
    );
    if (ok && !text.trimmed().isEmpty()) {
        m_quickInput->setText(text.trimmed());
        addEventFromInput();
    }
}

void CalendarWidget::buildContextMenu(QMenu* menu)
{
    QAction* mDays = menu->addAction(QString::fromUtf8("Режим: Сегодня и Завтра"));
    mDays->setCheckable(true);
    mDays->setChecked(m_mode == ViewMode::TodayTomorrow);

    QAction* mWeek = menu->addAction(QString::fromUtf8("Режим: Неделя (7 дней)"));
    mWeek->setCheckable(true);
    mWeek->setChecked(m_mode == ViewMode::Week);

    QAction* mMonth = menu->addAction(QString::fromUtf8("Режим: Месяц (сетка)"));
    mMonth->setCheckable(true);
    mMonth->setChecked(m_mode == ViewMode::Month);

    menu->addSeparator();
    QAction* goToday = menu->addAction(QString::fromUtf8("Перейти к сегодняшнему дню"));
    QAction* addEv = menu->addAction(
        QString::fromUtf8("+ Добавить событие на %1...").arg(m_selectedDate.toString("dd.MM")));
    QAction* resyncSem = menu->addAction(
        QString::fromUtf8("↻ Обновить закрепление и авто-задачи до %1")
            .arg(DataStore::semesterEndDate().toString("dd.MM.yyyy"))
    );
    QAction* clearDone = menu->addAction(QString::fromUtf8("Очистить выполненные события"));

    connect(mDays, &QAction::triggered, this, [this]() { setViewMode(ViewMode::TodayTomorrow); });
    connect(mWeek, &QAction::triggered, this, [this]() { setViewMode(ViewMode::Week); });
    connect(mMonth, &QAction::triggered, this, [this]() { setViewMode(ViewMode::Month); });
    connect(goToday, &QAction::triggered, this, &CalendarWidget::jumpToToday);
    connect(addEv, &QAction::triggered, this, &CalendarWidget::promptAddEventDialog);
    connect(resyncSem, &QAction::triggered, this, [this]() {
        DataStore::instance().ensureSemesterScheduleAndAutoTasks(true);
        DataStore::instance().syncDailyTasksWithCalendarAndRoadmaps(QDate::currentDate());
        DataStore::instance().notifyListeners();
        refreshAll();
    });
    connect(clearDone, &QAction::triggered, this, [this]() {
        DataStore::instance().clearCompletedCalendarEvents();
        refreshAll();
    });
}

void CalendarWidget::onThemeChanged()
{
    refreshAll();
}

void CalendarWidget::refreshAll()
{
    if (!m_selectedDate.isValid()) {
        m_selectedDate = QDate::currentDate();
    }
    refreshHeader();
    m_quickInput->setPlaceholderText(
        QString::fromUtf8("+ Событие на %1 (напр. %2)...")
            .arg(m_selectedDate.toString("dd.MM"), CategoryConfigLoader::seedConfig().calendarQuickHint)
    );

    if (m_mode == ViewMode::TodayTomorrow) {
        refreshTodayTomorrowView();
    } else if (m_mode == ViewMode::Week) {
        refreshWeekView();
    } else {
        refreshMonthView();
    }
}

void CalendarWidget::refreshHeader()
{
    const QString badge = vsbWeekBadge(m_selectedDate);
    if (m_mode == ViewMode::TodayTomorrow) {
        m_headerLabel->setText(QString("%1 %2 · %3")
                                   .arg(shortDayNameRu(m_selectedDate.dayOfWeek()),
                                        m_selectedDate.toString("dd.MM"),
                                        badge));
    } else if (m_mode == ViewMode::Week) {
        const QDate mon = m_selectedDate.addDays(-(m_selectedDate.dayOfWeek() - 1));
        const QDate sun = mon.addDays(6);
        m_headerLabel->setText(QString("%1–%2 · %3")
                                   .arg(mon.toString("dd.MM"), sun.toString("dd.MM"), badge));
    } else {
        m_headerLabel->setText(QString("%1 %2 · %3")
                                   .arg(monthNameRu(m_selectedDate.month()))
                                   .arg(m_selectedDate.year())
                                   .arg(badge));
    }
    setChromeTitle(QStringLiteral("Календарь"), m_headerLabel->text());
}

void CalendarWidget::refreshTodayTomorrowView()
{
    while (QLayoutItem* item = m_daysLayout->takeAt(0)) {
        if (QWidget* w = item->widget()) {
            w->deleteLater();
        }
        delete item;
    }

    const QDate d0 = m_selectedDate;
    const QDate d1 = m_selectedDate.addDays(1);
    const QDate today = QDate::currentDate();

    auto buildDayCard = [this, today](const QDate& d, const QString& prefix) {
        auto* card = new QWidget();
        card->setProperty("role", "card");
        card->setProperty("state", d == today ? "accent" : "");
        auto* lay = new QVBoxLayout(card);
        lay->setContentsMargins(9, 6, 9, 6);
        lay->setSpacing(4);

        const int studyMins = DataStore::instance().totalStudySecondsForDate(d) / 60;
        auto* head = new QHBoxLayout();
        auto* title = new QLabel(QString("%1 · %2 %3")
                                     .arg(prefix, shortDayNameRu(d.dayOfWeek()), d.toString("dd.MM")));
        title->setFont(Theme::fontMono(10, QFont::Bold));
        title->setStyleSheet(QStringLiteral("color: %1;")
                                 .arg(d == today ? Theme::accentColor().name() : Theme::textColor().name()));
        head->addWidget(title, 1);

        if (studyMins > 0) {
            auto* stBadge = new QLabel(QString::fromUtf8("⚡ %1м").arg(studyMins));
            stBadge->setFont(Theme::fontMono(9, QFont::DemiBold));
            stBadge->setStyleSheet("color: #8FD6A8;");
            head->addWidget(stBadge);
        }
        lay->addLayout(head);

        const QList<CalendarEvent> events = DataStore::instance().calendarEventsBetween(d, d);
        for (const CalendarEvent& ev : events) {
            auto* row = new QHBoxLayout();
            row->setSpacing(6);
            auto* cb = new QCheckBox();
            cb->setChecked(ev.completed);
            const QString labelTxt = ev.timeText.isEmpty()
                ? ev.title
                : QString("[%1] %2").arg(ev.timeText, ev.title);
            auto* lbl = new QLabel(labelTxt);
            lbl->setWordWrap(true);
            lbl->setStyleSheet(ev.completed
                                   ? "color: #7B819F; text-decoration: line-through; font-size: 11px;"
                                   : "color: #EEF0F8; font-size: 11px;");

            auto* del = new QPushButton(QStringLiteral("×"));
            del->setProperty("variant", "ghost");
            del->setFixedSize(18, 18);
            del->setStyleSheet("QPushButton { padding: 0; color: #7B819F; font-size: 12px; }"
                               "QPushButton:hover { color: #F29AA8; }");

            row->addWidget(cb);
            row->addWidget(lbl, 1);
            row->addWidget(del);
            lay->addLayout(row);

            const qint64 id = ev.id;
            connect(cb, &QCheckBox::toggled, this, [this, id](bool checked) {
                DataStore::instance().toggleCalendarEvent(id, checked);
                QTimer::singleShot(0, this, &CalendarWidget::refreshAll);
            });
            connect(del, &QPushButton::clicked, this, [this, id]() {
                DataStore::instance().removeCalendarEvent(id);
                QTimer::singleShot(0, this, &CalendarWidget::refreshAll);
            });
        }

        if (events.isEmpty()) {
            if (d == today) {
                const QList<GoalItem> dayGoals = DataStore::instance().goalsByHorizon("day");
                int openGoals = 0;
                QString firstOpen;
                for (const GoalItem& g : dayGoals) {
                    if (!g.completed) {
                        ++openGoals;
                        if (firstOpen.isEmpty()) firstOpen = g.title;
                    }
                }
                auto* hint = new QLabel(openGoals > 0
                                            ? QString::fromUtf8("Цель дня (%1): %2").arg(openGoals).arg(firstOpen)
                                            : QString::fromUtf8("Событий нет · день свободен"));
                hint->setWordWrap(true);
                hint->setStyleSheet("color: #A9AFC9; font-size: 10px;");
                lay->addWidget(hint);
            } else {
                auto* empty = new QLabel(QString::fromUtf8("Нет запланированных событий"));
                empty->setStyleSheet("color: #7B819F; font-size: 10px;");
                lay->addWidget(empty);
            }
        }

        return card;
    };

    const QString label0 = (d0 == today) ? QString::fromUtf8("СЕГОДНЯ") : QString::fromUtf8("ДЕНЬ 1");
    const QString label1 = (d1 == today.addDays(1)) ? QString::fromUtf8("ЗАВТРА") : QString::fromUtf8("ДЕНЬ 2");

    m_daysLayout->addWidget(buildDayCard(d0, label0));
    m_daysLayout->addWidget(buildDayCard(d1, label1));
    m_daysLayout->addStretch();
}

void CalendarWidget::refreshWeekView()
{
    const QDate monday = m_selectedDate.addDays(-(m_selectedDate.dayOfWeek() - 1));
    const QDate sunday = monday.addDays(6);

    const QList<CalendarEvent> weekEvents = DataStore::instance().calendarEventsBetween(monday, sunday);
    QSet<QDate> eventDates;
    for (const CalendarEvent& ev : weekEvents) {
        if (!ev.completed) {
            eventDates.insert(ev.date);
        }
    }
    const QSet<QDate> studyDates = DataStore::instance().activeStudyDatesBetween(monday, sunday);

    static_cast<WeekStripCanvas*>(m_weekStripWidget)->setData(monday, m_selectedDate, eventDates, studyDates);

    const int studiedMins = DataStore::instance().totalStudySecondsForDate(m_selectedDate) / 60;
    m_weekDayHeaderLabel->setText(
        QString::fromUtf8("%1, %2 · Фокус: %3м")
            .arg(shortDayNameRu(m_selectedDate.dayOfWeek()),
                 m_selectedDate.toString("dd.MM.yyyy"))
            .arg(studiedMins));

    while (QLayoutItem* item = m_weekEventsLayout->takeAt(0)) {
        if (QWidget* w = item->widget()) {
            w->deleteLater();
        }
        delete item;
    }

    const QList<CalendarEvent> dayEvents = DataStore::instance().calendarEventsBetween(m_selectedDate, m_selectedDate);
    for (const CalendarEvent& ev : dayEvents) {
        auto* rowWidget = new QWidget();
        rowWidget->setProperty("role", "card");
        auto* row = new QHBoxLayout(rowWidget);
        row->setContentsMargins(8, 4, 6, 4);
        row->setSpacing(6);

        auto* cb = new QCheckBox();
        cb->setChecked(ev.completed);

        const QString txt = ev.timeText.isEmpty() ? ev.title : QString("[%1] %2").arg(ev.timeText, ev.title);
        auto* lbl = new QLabel(txt);
        lbl->setWordWrap(true);
        lbl->setStyleSheet(ev.completed
                               ? "color: #7B819F; text-decoration: line-through; font-size: 11px;"
                               : "color: #EEF0F8; font-size: 11px;");

        auto* del = new QPushButton(QStringLiteral("×"));
        del->setProperty("variant", "ghost");
        del->setFixedSize(18, 18);
        del->setStyleSheet("QPushButton { padding: 0; color: #7B819F; }"
                           "QPushButton:hover { color: #F29AA8; }");

        row->addWidget(cb);
        row->addWidget(lbl, 1);
        row->addWidget(del);
        m_weekEventsLayout->addWidget(rowWidget);

        const qint64 id = ev.id;
        connect(cb, &QCheckBox::toggled, this, [this, id](bool checked) {
            DataStore::instance().toggleCalendarEvent(id, checked);
            QTimer::singleShot(0, this, &CalendarWidget::refreshAll);
        });
        connect(del, &QPushButton::clicked, this, [this, id]() {
            DataStore::instance().removeCalendarEvent(id);
            QTimer::singleShot(0, this, &CalendarWidget::refreshAll);
        });
    }

    if (dayEvents.isEmpty()) {
        auto* empty = new QLabel(QString::fromUtf8("На %1 событий нет. Добавьте внизу ↓")
                                     .arg(m_selectedDate.toString("dd.MM")));
        empty->setStyleSheet("color: #7B819F; font-size: 10px; padding: 4px 2px;");
        m_weekEventsLayout->addWidget(empty);
    }

    m_weekEventsLayout->addStretch();
}

void CalendarWidget::refreshMonthView()
{
    const QDate firstOfMonth(m_selectedDate.year(), m_selectedDate.month(), 1);
    const QDate gridStart = firstOfMonth.addDays(-(firstOfMonth.dayOfWeek() - 1));
    const QDate gridEnd = gridStart.addDays(41);

    const QList<CalendarEvent> monthEvents = DataStore::instance().calendarEventsBetween(gridStart, gridEnd);
    QSet<QDate> eventDates;
    for (const CalendarEvent& ev : monthEvents) {
        if (!ev.completed) {
            eventDates.insert(ev.date);
        }
    }
    const QSet<QDate> studyDates = DataStore::instance().activeStudyDatesBetween(gridStart, gridEnd);

    static_cast<MonthGridCanvas*>(m_monthGridWidget)->setData(
        m_selectedDate.year(), m_selectedDate.month(), m_selectedDate, eventDates, studyDates);

    const QList<CalendarEvent> selectedEvents =
        DataStore::instance().calendarEventsBetween(m_selectedDate, m_selectedDate);
    if (!selectedEvents.isEmpty()) {
        const CalendarEvent& first = selectedEvents.first();
        const QString timePrefix = first.timeText.isEmpty() ? QString() : (first.timeText + " ");
        const QString extra = selectedEvents.size() > 1
            ? QString(" (+%1)").arg(selectedEvents.size() - 1)
            : QString();
        m_monthSelectedSummary->setText(
            QString("%1 %2: %3%4%5")
                .arg(shortDayNameRu(m_selectedDate.dayOfWeek()),
                     m_selectedDate.toString("dd.MM"),
                     timePrefix,
                     first.title,
                     extra));
        m_monthSelectedSummary->setStyleSheet(QStringLiteral("color: %1; font-size: 10px;")
                                                  .arg(Theme::accentColor().name()));
    } else {
        const int studiedMins = DataStore::instance().totalStudySecondsForDate(m_selectedDate) / 60;
        m_monthSelectedSummary->setText(
            studiedMins > 0
                ? QString::fromUtf8("%1 %2: событий нет · спринты %3м")
                      .arg(shortDayNameRu(m_selectedDate.dayOfWeek()), m_selectedDate.toString("dd.MM"))
                      .arg(studiedMins)
                : QString::fromUtf8("%1 %2: событий нет (клик по дню для выбора)")
                      .arg(shortDayNameRu(m_selectedDate.dayOfWeek()), m_selectedDate.toString("dd.MM")));
        m_monthSelectedSummary->setStyleSheet("color: #A9AFC9; font-size: 10px;");
    }
}

QString CalendarWidget::vsbWeekBadge(const QDate& date)
{
    int yearNumber = 0;
    const int isoWeek = date.weekNumber(&yearNumber);
    const bool isEven = (isoWeek % 2 == 0);
    const int semWeek = qBound(1, isoWeek >= 38 ? (isoWeek - 37) : (isoWeek - 6), 14);
    return QString("%1 %2/13").arg(isEven ? "SUDÝ" : "LICHÝ").arg(semWeek);
}

QString CalendarWidget::shortDayNameRu(int dayOfWeek)
{
    static const char* names[7] = {"Пн", "Вт", "Ср", "Чт", "Пт", "Сб", "Вс"};
    return QString::fromUtf8(names[qBound(1, dayOfWeek, 7) - 1]);
}

QString CalendarWidget::monthNameRu(int month)
{
    static const char* months[12] = {
        "Янв", "Фев", "Мар", "Апр", "Май", "Июн",
        "Июл", "Авг", "Сен", "Окт", "Ноя", "Дек"};
    return QString::fromUtf8(months[qBound(1, month, 12) - 1]);
}

REGISTER_WIDGET("calendar", "Мини-календарь (Дни / Неделя / Месяц)", CalendarWidget)
