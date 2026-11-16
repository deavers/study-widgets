#pragma once

#include "../DataStore.h"
#include "../WidgetBase.h"

#include <QDate>
#include <QList>
#include <QSet>

class QLabel;
class QLineEdit;
class QPushButton;
class QScrollArea;
class QStackedWidget;
class QTimer;
class QVBoxLayout;

class CalendarWidget : public WidgetBase
{
public:
    enum class ViewMode {
        TodayTomorrow = 0,
        Week = 1,
        Month = 2
    };

    explicit CalendarWidget(QWidget* parent = nullptr);

    void setViewMode(ViewMode mode);
    ViewMode viewMode() const { return m_mode; }
    void selectDate(const QDate& date);
    QSize effectiveMinimumSizeForWidth(int targetWidth = -1) const override;

protected:
    void buildContextMenu(QMenu* menu) override;
    void onThemeChanged() override;

private:
    void stepPeriod(int delta);
    void jumpToToday();
    void addEventFromInput();
    void promptAddEventDialog();
    void refreshAll();
    void refreshHeader();
    void refreshTodayTomorrowView();
    void refreshWeekView();
    void refreshMonthView();

    static QString vsbWeekBadge(const QDate& date);
    static QString shortDayNameRu(int dayOfWeek);
    static QString monthNameRu(int month);

    ViewMode m_mode = ViewMode::Month;
    QDate m_selectedDate;

    QLabel* m_headerLabel = nullptr;
    QPushButton* m_modeDaysBtn = nullptr;
    QPushButton* m_modeWeekBtn = nullptr;
    QPushButton* m_modeMonthBtn = nullptr;

    QStackedWidget* m_stack = nullptr;

    // Page 0: Today / Tomorrow
    QScrollArea* m_daysScroll = nullptr;
    QWidget* m_daysContainer = nullptr;
    QVBoxLayout* m_daysLayout = nullptr;

    // Page 1: Week
    QWidget* m_weekStripWidget = nullptr;
    QLabel* m_weekDayHeaderLabel = nullptr;
    QScrollArea* m_weekScroll = nullptr;
    QWidget* m_weekEventsContainer = nullptr;
    QVBoxLayout* m_weekEventsLayout = nullptr;

    // Page 2: Month
    QWidget* m_monthGridWidget = nullptr;
    QLabel* m_monthSelectedSummary = nullptr;

    // Shared quick-add bar
    QLineEdit* m_quickInput = nullptr;
    QTimer* m_refreshTimer = nullptr;
};
