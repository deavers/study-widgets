#include <QApplication>
#include <QCoreApplication>
#include <QDate>
#include <QDebug>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QGuiApplication>
#include <QMessageBox>
#include <QScreen>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTemporaryFile>

#include "CategoryConfigLoader.h"
#include "ControlPanel.h"
#include "DataStore.h"
#include "LegacyImporter.h"
#include "PrayerCalculator.h"
#include "TimeLogWriter.h"
#include "WidgetRegistry.h"
#include "widgets/CalendarWidget.h"
#include "widgets/PomodoroWidget.h"

namespace {

void selfTestMessageHandler(QtMsgType, const QMessageLogContext&, const QString& msg)
{
    fprintf(stderr, "%s\n", msg.toLocal8Bit().constData());
    fflush(stderr);
    QFile f("selftest.log");
    if (f.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream ts(&f);
        ts << msg << "\n";
        ts.flush();
        f.flush();
        f.close();
    }
}

int runSelfTest()
{
    QFile::remove("selftest.log");
    qInstallMessageHandler(selfTestMessageHandler);
    qInfo() << "=== StudyWidgets Self-Test Starting ===";

    if (!DataStore::instance().open()) {
        qCritical() << "FAIL: DataStore::open():" << DataStore::instance().lastError();
        return 1;
    }

    QString catErr;
    if (!CategoryConfigLoader::importLocalCategories(&catErr)) {
        qCritical() << "FAIL: importLocalCategories:" << catErr;
        return 1;
    }

    const QList<StudyCategory> cats = DataStore::instance().studyCategories(false);
    qInfo() << "PASS: Active categories loaded:" << cats.size();

    // Test TimeLogWriter legacy format match
    StudySession testSession;
    testSession.startedAt = QDateTime(QDate(2026, 6, 9), QTime(1, 28));
    testSession.finishedAt = QDateTime(QDate(2026, 6, 9), QTime(2, 51));
    testSession.durationSeconds = 45 * 60;
    testSession.category = "Subject1";
    const QString logLine = TimeLogWriter::formatSessionLine(testSession, "Subject1");
    const QString expectedLogLine = QStringLiteral("- 📅 **Дата:** 2026-06-09 | 🕒 **Время:** 02:51 | 📚 **Предмет:** [[Subject1]] | ⏱ **Длительность:** 45 мин\n");
    if (logLine != expectedLogLine) {
        qCritical() << "FAIL: TimeLogWriter line mismatch:\nActual:  " << logLine << "\nExpected:" << expectedLogLine;
        return 1;
    }
    qInfo() << "PASS: TimeLogWriter exact format matches legacy Rainmeter / Obsidian standard.";

    // Test Undo last session
    StudySession undoTest;
    undoTest.categoryId = "free-study";
    undoTest.category = "Free Study";
    undoTest.startedAt = QDateTime::currentDateTime();
    undoTest.finishedAt = undoTest.startedAt.addSecs(300);
    undoTest.durationSeconds = 300;
    undoTest.completed = true;
    DataStore::instance().addStudySession(undoTest);
    if (!DataStore::instance().undoLastSession()) {
        qCritical() << "FAIL: undoLastSession failed.";
        return 1;
    }
    qInfo() << "PASS: undoLastSession successfully deleted test session.";

    if (cats.isEmpty())
    {
        qCritical() << "FAIL: No active categories loaded.";
        return 1;
    }
    QString testCatId = cats.first().id;
    for (const StudyCategory& cat : cats)
    {
        if (cat.id != QStringLiteral("free-study"))
        {
            testCatId = cat.id;
            break;
        }
    }

    // Test PomodoroWidget
    auto* pomodoro = new PomodoroWidget();
    if (!pomodoro->selectCategoryById(testCatId))
    {
        qCritical() << "FAIL: PomodoroWidget selectCategoryById failed for" << testCatId;
        delete pomodoro;
        return 1;
    }
    delete pomodoro;
    qInfo() << "PASS: PomodoroWidget 1-click tile selection verified:" << testCatId;

    // Test Spaced Repetition CRUD (especially default empty note!)
    const int beforeRevCount = DataStore::instance().spacedReviews().size();
    if (!DataStore::instance().addSpacedReview(testCatId, "SelfTest: POSIX Mutex", "cviceni")) {
        qCritical() << "FAIL: addSpacedReview with default note:" << DataStore::instance().lastError();
        return 1;
    }
    const QList<SpacedReviewItem> afterAdd = DataStore::instance().spacedReviews();
    if (afterAdd.size() != beforeRevCount + 1) {
        qCritical() << "FAIL: spacedReviews count mismatch after add.";
        return 1;
    }

    qint64 testRevId = -1;
    for (const SpacedReviewItem& r : afterAdd) {
        if (r.topic == "SelfTest: POSIX Mutex") {
            testRevId = r.id;
            break;
        }
    }
    if (testRevId <= 0 ||
        !DataStore::instance().advanceSpacedReview(testRevId, false) ||
        !DataStore::instance().advanceSpacedReview(testRevId, true) ||
        !DataStore::instance().removeSpacedReview(testRevId)) {
        qCritical() << "FAIL: SpacedReview advance/remove failed.";
        return 1;
    }
    qInfo() << "PASS: SpacedRepetition CRUD (add/advance/reset/remove) with empty note succeeded.";

    // Test CalendarEvent CRUD (v5 schema) & Bidirectional Sync with Goals
    const QDate testDate = QDate::currentDate();
    if (!DataStore::instance().addCalendarEvent(testDate, "SelfTest: Sample Exam", "10:00", "exam"))
    {
        qCritical() << "FAIL: addCalendarEvent failed:" << DataStore::instance().lastError();
        return 1;
    }
    qint64 testEvId = -1;
    for (const CalendarEvent& ev : DataStore::instance().calendarEventsBetween(testDate, testDate))
    {
        if (ev.title == "SelfTest: Sample Exam")
        {
            testEvId = ev.id;
            break;
        }
    }
    qint64 mirroredGoalId = -1;
    for (const GoalItem& g : DataStore::instance().goalsByHorizon("day"))
    {
        if (g.title == "SelfTest: Sample Exam")
        {
            mirroredGoalId = g.id;
            break;
        }
    }
    if (testEvId <= 0 || mirroredGoalId <= 0 ||
        !DataStore::instance().toggleCalendarEvent(testEvId, true) ||
        !DataStore::instance().removeCalendarEvent(testEvId) ||
        !DataStore::instance().removeGoal(mirroredGoalId)) {
        qCritical() << "FAIL: CalendarEvent / Goal bidirectional sync or cleanup failed.";
        return 1;
    }
    qInfo() << "PASS: CalendarEvent CRUD & Bidirectional Sync with Goals verified.";

    // Verify semester schedule until semesterEndDate()
    const QDate semEnd = DataStore::semesterEndDate();
    const QList<CalendarEvent> endEvents = DataStore::instance().calendarEventsBetween(semEnd, semEnd);
    if (endEvents.size() < 2) {
        qCritical() << "FAIL: Semester end date" << semEnd.toString(Qt::ISODate)
                    << "missing consolidation/specialty events. Count=" << endEvents.size()
                    << "seededFlag=" << QSettings().value("study/semesterAutoSeededV7").toBool()
                    << "settingsFile=" << QSettings().fileName();
        return 1;
    }
    qInfo() << "PASS: Semester schedule populated through" << semEnd.toString(Qt::ISODate)
            << "with" << endEvents.size() << "events on final day.";

    // Test PrayerCalculator
    const LocalSeedConfig& seedCfg = CategoryConfigLoader::seedConfig();
    PrayerConfig pcfg;
    pcfg.latitude = seedCfg.prayerLatitude;
    pcfg.longitude = seedCfg.prayerLongitude;
    pcfg.elevation = seedCfg.prayerElevation;
    pcfg.fajrAngle = 8.0;
    pcfg.ishaAngle = 8.0;
    const PrayerSchedule sched8 = PrayerCalculator::calculate(QDate::currentDate(), pcfg);
    pcfg.fajrAngle = 12.0;
    pcfg.ishaAngle = 12.0;
    const PrayerSchedule sched12 = PrayerCalculator::calculate(QDate::currentDate(), pcfg);
    if (!sched8.fajr.isValid() || !sched12.fajr.isValid()) {
        qCritical() << "FAIL: PrayerCalculator returned invalid QTime.";
        return 1;
    }
    qInfo() << "PASS: Prayer Times calculated:"
            << "8deg Fajr=" << sched8.fajr.toString("HH:mm")
            << "Isha=" << sched8.isha.toString("HH:mm")
            << "| 12deg Fajr=" << sched12.fajr.toString("HH:mm")
            << "Isha=" << sched12.isha.toString("HH:mm");

    // Test instantiating, showing, and moving all widgets across available monitors
    const QList<QScreen*> screens = QGuiApplication::screens();
    qInfo() << "Detected screens:" << screens.size();
    for (int i = 0; i < screens.size(); ++i) {
        qInfo() << "  Screen" << i << screens[i]->name()
                << "geom=" << screens[i]->geometry()
                << "dpr=" << screens[i]->devicePixelRatio();
    }

    for (const WidgetInfo& info : widgetRegistry()) {
        QWidget* w = createWidgetById(info.id);
        if (!w) {
            qCritical() << "FAIL: Could not create widget:" << info.id;
            return 1;
        }
        w->show();
        QCoreApplication::processEvents();

        const QSize initialSize = w->size();
        for (QScreen* scr : screens) {
            w->move(scr->availableGeometry().center() - QPoint(w->width() / 2, w->height() / 2));
            QCoreApplication::processEvents();
            if (w->size() != initialSize && info.id != "welcome") {
                qCritical() << "FAIL: Widget" << info.id << "changed size on screen move:"
                            << initialSize << "->" << w->size();
                delete w;
                return 1;
            }
        }
        qInfo() << "PASS: Widget" << info.id << "size locked at" << w->size() << "across all screens.";
        w->hide();
        delete w;
    }

    qInfo() << "=== ALL SELF-TESTS PASSED SUCCESSFULLY ===";
    return 0;
}

} // namespace

#ifdef Q_OS_WIN
#include <windows.h>
#endif

int main(int argc, char* argv[])
{
    // Use unified System DPI Awareness on Windows so frameless desktop widgets
    // can be dragged freely between mixed-DPI monitors (e.g. 100% MSI + 125% Laptop)
    // without WM_DPICHANGED recursive 1.25x window expansion or coordinate jumps.
    qputenv("QT_QPA_PLATFORM", "windows:dpiawareness=1");

    QApplication app(argc, argv);

    QApplication::setApplicationName("StudyWidgets");
    QApplication::setOrganizationName("deavers");

    app.setQuitOnLastWindowClosed(false);

    if (app.arguments().contains("--self-test"))
    {
#ifdef Q_OS_WIN
        AttachConsole(ATTACH_PARENT_PROCESS);
        freopen("CONOUT$", "w", stdout);
        freopen("CONOUT$", "w", stderr);
#endif
        QTemporaryDir isolatedDir;
        QString isolatedAppData;
        if (app.arguments().contains(QStringLiteral("--isolated")) && isolatedDir.isValid())
        {
            QApplication::setOrganizationName(QStringLiteral("StudyWidgetsIsolatedTest"));
            QStandardPaths::setTestModeEnabled(true);
            QSettings::setDefaultFormat(QSettings::IniFormat);
            QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, isolatedDir.path());
            isolatedAppData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
            QDir(isolatedAppData).removeRecursively();
        }
        const int rc = runSelfTest();
        if (!isolatedAppData.isEmpty())
        {
            QDir(isolatedAppData).removeRecursively();
        }
        return rc;
    }

    if (app.arguments().contains("--screenshot-all")) {
        DataStore::instance().open();
        CategoryConfigLoader::importLocalCategories();

        QString outDir = QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation))
                             .filePath(QStringLiteral("screenshots"));
        for (const QString& arg : app.arguments())
        {
            if (arg.startsWith(QStringLiteral("--screenshot-dir=")))
            {
                const QString customDir = arg.mid(QStringLiteral("--screenshot-dir=").length()).trimmed();
                if (!customDir.isEmpty())
                {
                    outDir = customDir;
                }
            }
        }
        QDir().mkpath(outDir);

        for (const WidgetInfo& info : widgetRegistry()) {
            if (info.id == "welcome") continue;
            QWidget* w = createWidgetById(info.id);
            if (!w) continue;
            w->show();
            QCoreApplication::processEvents();
            if (info.id == "system_monitor") {
                QElapsedTimer waitTimer;
                waitTimer.start();
                while (waitTimer.elapsed() < 320) {
                    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
                }
            }
            QPixmap pix = w->grab();
            pix.save(outDir + "/" + info.id + ".png");

            if (info.id == "calendar") {
                if (auto* cal = dynamic_cast<CalendarWidget*>(w)) {
                    cal->setViewMode(CalendarWidget::ViewMode::TodayTomorrow);
                    QCoreApplication::processEvents();
                    cal->grab().save(outDir + "/calendar_days.png");

                    cal->setViewMode(CalendarWidget::ViewMode::Week);
                    QCoreApplication::processEvents();
                    cal->grab().save(outDir + "/calendar_week.png");

                    cal->setViewMode(CalendarWidget::ViewMode::Month);
                    QCoreApplication::processEvents();
                    cal->grab().save(outDir + "/calendar_month.png");
                }
            }

            w->hide();
            delete w;
        }
        return 0;
    }

    if (!DataStore::instance().open()) {
        QMessageBox::critical(
            nullptr,
            "StudyWidgets database error",
            "The local SQLite database could not be opened.\n\n" +
            DataStore::instance().lastError()
        );

        return 1;
    }

    QString categoryConfigError;

    if (!CategoryConfigLoader::importLocalCategories(&categoryConfigError)) {
        QMessageBox::warning(
            nullptr,
            "StudyWidgets category configuration",
            "Could not import local categories.\n\n" +
            categoryConfigError
        );
    }

    // Import legacy Rainmeter SaveData.inc if first run
    LegacyImporter::importIfFirstRun();

    // Flush any pending sessions that were not written to TimeLog.md
    TimeLogWriter::flushUnloggedSessions();

    ControlPanel controlPanel;
    controlPanel.show();

    return app.exec();
}
