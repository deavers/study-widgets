#include "PrayerTimesWidget.h"

#include "../CategoryConfigLoader.h"
#include "../WidgetRegistry.h"

#include <QDate>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QSettings>
#include <QTime>
#include <QTimer>
#include <QVBoxLayout>

PrayerTimesWidget::PrayerTimesWidget(QWidget* parent)
    : WidgetBase("prayer_times", parent)
{
    const LocalSeedConfig& seed = CategoryConfigLoader::seedConfig();
    setBaseWidgetSize(340, 304);
    setChromeTitle(QString::fromUtf8("Намаз"), seed.prayerCityName);
    setRootMargins(12, 10, 12, 10);
    bodyLayout()->setSpacing(6);

    auto* topRow = new QHBoxLayout();
    topRow->setSpacing(6);

    m_nextPrayerLabel = new QLabel();
    m_nextPrayerLabel->setProperty("role", "dim");
    topRow->addWidget(m_nextPrayerLabel, 1);

    m_methodBadgeLabel = new QLabel();
    m_methodBadgeLabel->setProperty("role", "pill");
    m_methodBadgeLabel->setToolTip(QString::fromUtf8("ПКМ — сменить метод расчёта намазов"));
    topRow->addWidget(m_methodBadgeLabel);

    bodyLayout()->addLayout(topRow);

    m_countdownLabel = new QLabel("--:--:--");
    m_countdownLabel->setAlignment(Qt::AlignCenter);
    m_countdownLabel->setProperty("role", "bigAccent");
    m_countdownLabel->setStyleSheet("font-size: 32px; margin: -2px 0;");
    bodyLayout()->addWidget(m_countdownLabel);

    m_makroohLabel = new QLabel();
    m_makroohLabel->setAlignment(Qt::AlignCenter);
    m_makroohLabel->setProperty("role", "banner");
    m_makroohLabel->setStyleSheet("padding: 5px 8px; font-size: 11px;");
    bodyLayout()->addWidget(m_makroohLabel);

    auto* card = new QWidget();
    card->setProperty("role", "card");
    auto* grid = new QGridLayout(card);
    grid->setContentsMargins(14, 8, 14, 8);
    grid->setHorizontalSpacing(12);
    grid->setVerticalSpacing(4);

    const QStringList prayerNames = {
        QString::fromUtf8("Фаджр"),
        QString::fromUtf8("Восход"),
        QString::fromUtf8("Зухр"),
        QString::fromUtf8("Аср"),
        QString::fromUtf8("Магриб"),
        QString::fromUtf8("Иша")};

    for (int i = 0; i < prayerNames.size(); ++i) {
        auto* nameLbl = new QLabel(prayerNames[i]);
        auto* timeLbl = new QLabel("--:--");
        timeLbl->setProperty("role", "mono");
        timeLbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        grid->addWidget(nameLbl, i, 0);
        grid->addWidget(timeLbl, i, 1);
        m_nameLabels.append(nameLbl);
        m_timeLabels.append(timeLbl);
    }
    bodyLayout()->addWidget(card);

    m_sunnahSummaryLabel = new QLabel();
    m_sunnahSummaryLabel->setWordWrap(true);
    m_sunnahSummaryLabel->setAlignment(Qt::AlignCenter);
    m_sunnahSummaryLabel->setProperty("role", "monoDim");
    m_sunnahSummaryLabel->setStyleSheet("font-size: 10px;");
    bodyLayout()->addWidget(m_sunnahSummaryLabel);
    bodyLayout()->addStretch();

    QSettings settings;
    m_showSunnah = settings.value("prayer_times/showSunnah", true).toBool();
    m_sunnahSummaryLabel->setVisible(m_showSunnah);

    const int savedMethod = qBound(0, settings.value("prayer_times/methodIndex", 1).toInt(), 3);
    applySelectedMethod(savedMethod);

    m_timer = new QTimer(this);
    m_timer->setInterval(1000);
    connect(m_timer, &QTimer::timeout, this, &PrayerTimesWidget::refreshPrayerDisplay);
    m_timer->start();
}

void PrayerTimesWidget::buildContextMenu(QMenu* menu)
{
    const LocalSeedConfig& seed = CategoryConfigLoader::seedConfig();
    QMenu* methodMenu = menu->addMenu(
        QString::fromUtf8("Метод расчёта (%1 %2°N)")
            .arg(seed.prayerCityName)
            .arg(seed.prayerLatitude, 0, 'f', 2)
    );
    for (int i = 0; i < seed.prayerMethods.size(); ++i)
    {
        QAction* act = methodMenu->addAction(seed.prayerMethods[i].menuLabel);
        act->setCheckable(true);
        act->setChecked(m_methodIndex == i);
        connect(act, &QAction::triggered, this, [this, i]()
        {
            applySelectedMethod(i);
        });
    }

    QAction* sunnahAct = menu->addAction(QString::fromUtf8("Показывать сунна-времена (духа / тахаджуд)"));
    sunnahAct->setCheckable(true);
    sunnahAct->setChecked(m_showSunnah);
    connect(sunnahAct, &QAction::triggered, this, [this](bool checked)
    {
        m_showSunnah = checked;
        QSettings settings;
        settings.setValue("prayer_times/showSunnah", checked);
        m_sunnahSummaryLabel->setVisible(checked);
        setBaseWidgetSize(340, checked ? 304 : 272);
    });
}

QString PrayerTimesWidget::fmtTime(const QTime& t)
{
    return t.isValid() ? t.toString("HH:mm") : QStringLiteral("--:--");
}

void PrayerTimesWidget::applySelectedMethod(int index)
{
    const LocalSeedConfig& seed = CategoryConfigLoader::seedConfig();
    const int maxIdx = qMax(0, seed.prayerMethods.size() - 1);
    m_methodIndex = qBound(0, index, maxIdx);

    QSettings settings;
    settings.setValue("prayer_times/methodIndex", m_methodIndex);

    m_config = PrayerConfig();
    m_config.presetName = seed.prayerPresetName;
    m_config.latitude = seed.prayerLatitude;
    m_config.longitude = seed.prayerLongitude;
    m_config.elevation = seed.prayerElevation;

    if (!seed.prayerMethods.isEmpty())
    {
        const LocalPrayerMethod& pm = seed.prayerMethods[m_methodIndex];
        m_methodBadgeLabel->setText(pm.badgeLabel);
        m_config.fajrAngle = pm.fajrAngle;
        m_config.ishaAngle = pm.ishaAngle;
        m_config.tuneFajr = pm.tuneFajr;
        m_config.tuneSunrise = pm.tuneSunrise;
        m_config.tuneDhuhr = pm.tuneDhuhr;
        m_config.tuneAsr = pm.tuneAsr;
        m_config.tuneMaghrib = pm.tuneMaghrib;
        m_config.tuneIsha = pm.tuneIsha;
    }

    refreshPrayerDisplay();
}

void PrayerTimesWidget::refreshPrayerDisplay()
{
    const QDate today = QDate::currentDate();
    const QTime now = QTime::currentTime();

    const PrayerSchedule t = PrayerCalculator::calculate(today, m_config);
    const QList<QTime> timesList = {t.fajr, t.sunrise, t.dhuhr, t.asr, t.maghrib, t.isha};

    const QStringList shortNames = {
        QString::fromUtf8("Фаджр"), QString::fromUtf8("Восход"), QString::fromUtf8("Зухр"),
        QString::fromUtf8("Аср"), QString::fromUtf8("Магриб"), QString::fromUtf8("Иша")};

    int nextIndex = -1;
    for (int i = 0; i < timesList.size(); ++i) {
        if (timesList[i].isValid() && now < timesList[i]) {
            nextIndex = i;
            break;
        }
    }
    const int activeIndex = (nextIndex == -1) ? 5 : (nextIndex == 0 ? 5 : nextIndex - 1);

    for (int i = 0; i < 6; ++i) {
        m_timeLabels[i]->setText(fmtTime(timesList[i]));
        const QString role = (i == nextIndex) ? "accent" : (i == activeIndex ? "ok" : "");
        const QString nameStyle = role == "accent" ? "color: " + Theme::accentHex() + "; font-weight: 700;"
                                  : role == "ok"   ? "color: #8FD6A8; font-weight: 700;"
                                                   : "color: #C9D2E3;";
        m_nameLabels[i]->setStyleSheet(nameStyle);
        m_timeLabels[i]->setStyleSheet(nameStyle);
    }

    QTime targetTime;
    QString nextName;
    int secsLeft = 0;

    if (nextIndex >= 0) {
        targetTime = timesList[nextIndex];
        nextName = shortNames[nextIndex];
        secsLeft = now.secsTo(targetTime);
    } else {
        const PrayerSchedule tomorrow = PrayerCalculator::calculate(today.addDays(1), m_config);
        targetTime = tomorrow.fajr;
        nextName = QString::fromUtf8("Фаджр (завтра)");
        secsLeft = now.secsTo(QTime(23, 59, 59)) + 1 + QTime(0, 0, 0).secsTo(targetTime);
    }
    secsLeft = qMax(0, secsLeft);

    m_nextPrayerLabel->setText(
        QString::fromUtf8("До «%1» в %2").arg(nextName, fmtTime(targetTime)));
    const QString cdText = QString("%1:%2:%3")
                               .arg(secsLeft / 3600, 2, 10, QChar('0'))
                               .arg((secsLeft % 3600) / 60, 2, 10, QChar('0'))
                               .arg(secsLeft % 60, 2, 10, QChar('0'));
    m_countdownLabel->setText(cdText);
    setChromeTitle(nextName, cdText);

    QString makroohReason;
    if (now >= t.sunrise && now <= t.sunrise.addSecs(20 * 60)) {
        makroohReason = QString::fromUtf8("Макрух: после восхода, до %1").arg(fmtTime(t.sunrise.addSecs(20 * 60)));
    } else if (now >= t.dhuhr.addSecs(-10 * 60) && now < t.dhuhr) {
        makroohReason = QString::fromUtf8("Макрух: зенит, до %1").arg(fmtTime(t.dhuhr));
    } else if (now >= t.maghrib.addSecs(-15 * 60) && now < t.maghrib) {
        makroohReason = QString::fromUtf8("Макрух: перед закатом, до %1").arg(fmtTime(t.maghrib));
    }

    if (!makroohReason.isEmpty()) {
        m_makroohLabel->setText(makroohReason);
        Theme::setState(m_makroohLabel, "state", "danger");
    } else {
        m_makroohLabel->setText(QString::fromUtf8("Время для молитвы разрешено"));
        Theme::setState(m_makroohLabel, "state", "ok");
    }

    m_sunnahSummaryLabel->setText(
        QString::fromUtf8("духа %1 · 1/3 ночи %2\nполночь %3 · тахаджуд %4")
            .arg(fmtTime(t.duhaStart), fmtTime(t.firstThird), fmtTime(t.midnight), fmtTime(t.lastThird)));
}

REGISTER_WIDGET("prayer_times", "Времена намазов", PrayerTimesWidget)
