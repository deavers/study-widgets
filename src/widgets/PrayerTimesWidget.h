#pragma once

#include "../PrayerCalculator.h"
#include "../WidgetBase.h"

#include <QList>

class QLabel;
class QTimer;

class PrayerTimesWidget : public WidgetBase
{
public:
    explicit PrayerTimesWidget(QWidget* parent = nullptr);

protected:
    void buildContextMenu(QMenu* menu) override;

private:
    void applySelectedMethod(int index);
    void refreshPrayerDisplay();
    static QString fmtTime(const QTime& t);

    int m_methodIndex = 1;
    bool m_showSunnah = true;

    QLabel* m_nextPrayerLabel = nullptr;
    QLabel* m_methodBadgeLabel = nullptr;
    QLabel* m_countdownLabel = nullptr;
    QLabel* m_makroohLabel = nullptr;

    QList<QLabel*> m_nameLabels;
    QList<QLabel*> m_timeLabels;

    QLabel* m_sunnahSummaryLabel = nullptr;

    QTimer* m_timer = nullptr;
    PrayerConfig m_config;
};
