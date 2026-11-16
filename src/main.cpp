#include <QApplication>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDate>
#include <QDebug>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalServer>
#include <QLocalSocket>
#include <QLockFile>
#include <QMessageBox>
#include <QScreen>
#include <QSettings>
#include <QStandardPaths>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTemporaryFile>
#include <QThread>

#include "CategoryConfigLoader.h"
#include "ControlPanel.h"
#include "DataStore.h"
#include "LegacyImporter.h"
#include "PrayerCalculator.h"
#include "TimeLogWriter.h"
#include "WidgetRegistry.h"
#include "api/DeaveApiClient.h"
#include "widgets/CalendarWidget.h"
#include "widgets/DailyWisdomWidget.h"
#include "widgets/NotesWidget.h"
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

    // --- Stage 3 Self-Test: DataStore v7 Schema, cached_notes, notes_draft & day_rules ---
    if (DataStore::instance().schemaVersion() != 7)
    {
        qCritical() << "FAIL: DataStore schema version is" << DataStore::instance().schemaVersion() << "expected 7.";
        return 1;
    }

    const QString v7BackupPath = QDir(DataStore::instance().appDataDir()).filePath(QStringLiteral("backups/studywidgets-before-v7.sqlite"));
    if (!QFile::exists(v7BackupPath))
    {
        qCritical() << "FAIL: V7 backup file missing:" << v7BackupPath;
        return 1;
    }
    qInfo() << "PASS: DataStore schema v7 and backup file verified:" << v7BackupPath;

    // Test cached_notes CRUD & SHA-256
    DataStore::instance().clearCachedNotes();
    const QString noteSrc = QStringLiteral("sample_source");
    const QString notePath = QStringLiteral("docs/sample_note.md");
    const QString noteTitle = QStringLiteral("Sample Note");
    const QString noteContent = QStringLiteral("# Sample Note Content\nTesting SQLite cache v6 storage and hash computation.");
    const QString expectedSha = QString::fromLatin1(
        QCryptographicHash::hash(noteContent.toUtf8(), QCryptographicHash::Sha256).toHex()
    );

    if (!DataStore::instance().saveCachedNote(noteSrc, notePath, noteTitle, noteContent))
    {
        qCritical() << "FAIL: saveCachedNote failed:" << DataStore::instance().lastError();
        return 1;
    }

    CachedNote readNote;
    if (!DataStore::instance().getCachedNote(noteSrc, notePath, &readNote))
    {
        qCritical() << "FAIL: getCachedNote failed for saved note.";
        return 1;
    }
    if (readNote.title != noteTitle || readNote.content != noteContent || readNote.contentSha256 != expectedSha)
    {
        qCritical() << "FAIL: getCachedNote payload mismatch.";
        return 1;
    }
    qInfo() << "PASS: cached_notes CRUD and SHA-256 checksum integrity verified.";

    // Test 512 KB size limit
    const QString oversizedContent(512 * 1024 + 1024, 'X');
    if (DataStore::instance().saveCachedNote(noteSrc, QStringLiteral("docs/oversized.md"), QStringLiteral("Oversized"), oversizedContent))
    {
        qCritical() << "FAIL: saveCachedNote accepted oversized note (>512 KB).";
        return 1;
    }
    if (DataStore::instance().cachedNotesCount() != 1)
    {
        qCritical() << "FAIL: cachedNotesCount changed after oversized note rejection.";
        return 1;
    }
    qInfo() << "PASS: 512 KB note size limit enforcement verified (oversized note rejected).";

    // Test LRU eviction
    DataStore::instance().setMaxCachedNotesCountForTesting(3);
    DataStore::instance().clearCachedNotes();
    for (int i = 1; i <= 3; ++i)
    {
        DataStore::instance().saveCachedNote(
            noteSrc,
            QStringLiteral("note_%1.md").arg(i),
            QStringLiteral("Note %1").arg(i),
            QStringLiteral("Content %1").arg(i)
        );
        QThread::msleep(15);
    }
    if (DataStore::instance().cachedNotesCount() != 3)
    {
        qCritical() << "FAIL: cachedNotesCount mismatch during LRU prep.";
        return 1;
    }
    // Insert 4th note: note_1 should be evicted
    DataStore::instance().saveCachedNote(
        noteSrc,
        QStringLiteral("note_4.md"),
        QStringLiteral("Note 4"),
        QStringLiteral("Content 4")
    );
    if (DataStore::instance().cachedNotesCount() != 3)
    {
        qCritical() << "FAIL: cachedNotesCount exceeded max limit 3 after insertion:" << DataStore::instance().cachedNotesCount();
        return 1;
    }
    CachedNote evCheck;
    if (DataStore::instance().getCachedNote(noteSrc, QStringLiteral("note_1.md"), &evCheck))
    {
        qCritical() << "FAIL: Oldest note_1 was not evicted by LRU.";
        return 1;
    }
    if (!DataStore::instance().getCachedNote(noteSrc, QStringLiteral("note_4.md"), &evCheck))
    {
        qCritical() << "FAIL: New note_4 missing from cache.";
        return 1;
    }
    // Reset limit to default
    DataStore::instance().setMaxCachedNotesCountForTesting(100);
    qInfo() << "PASS: LRU eviction ordered by cached_at verified.";

    // Test clearCachedNotes
    if (!DataStore::instance().clearCachedNotes() || DataStore::instance().cachedNotesCount() != 0)
    {
        qCritical() << "FAIL: clearCachedNotes did not empty cache.";
        return 1;
    }
    qInfo() << "PASS: clearCachedNotes() successfully emptied cache.";

    // Test notes_draft CRUD
    DataStore::instance().clearNotesDraft();
    const QString sampleDraft = QStringLiteral("Test quick capture draft");
    const QString sampleTarget = QStringLiteral("timelog");
    if (!DataStore::instance().saveNotesDraft(sampleDraft, sampleTarget))
    {
        qCritical() << "FAIL: saveNotesDraft failed:" << DataStore::instance().lastError();
        return 1;
    }
    QString readDraftText;
    QString readDraftTarget;
    if (!DataStore::instance().getNotesDraft(&readDraftText, &readDraftTarget))
    {
        qCritical() << "FAIL: getNotesDraft failed for saved draft.";
        return 1;
    }
    if (readDraftText != sampleDraft || readDraftTarget != sampleTarget)
    {
        qCritical() << "FAIL: getNotesDraft content mismatch:" << readDraftText << readDraftTarget;
        return 1;
    }
    if (!DataStore::instance().clearNotesDraft())
    {
        qCritical() << "FAIL: clearNotesDraft failed.";
        return 1;
    }
    if (DataStore::instance().getNotesDraft(&readDraftText, &readDraftTarget))
    {
        qCritical() << "FAIL: notes_draft still present after clearNotesDraft.";
        return 1;
    }
    qInfo() << "PASS: notes_draft CRUD verified.";

    // Test day_rules CRUD
    DataStore::instance().seedDefaultDayRulesIfEmpty();
    const QList<DayRuleItem> initialRules = DataStore::instance().dayRules();
    if (initialRules.isEmpty())
    {
        qCritical() << "FAIL: seedDefaultDayRulesIfEmpty did not create default rules.";
        return 1;
    }
    const QString customRuleTitle = QStringLiteral("Self-Test Custom Rule");
    if (!DataStore::instance().addDayRule(customRuleTitle, 99))
    {
        qCritical() << "FAIL: addDayRule failed:" << DataStore::instance().lastError();
        return 1;
    }
    const QList<DayRuleItem> afterAddRules = DataStore::instance().dayRules();
    qint64 testRuleId = -1;
    for (const DayRuleItem& item : afterAddRules)
    {
        if (item.title == customRuleTitle)
        {
            testRuleId = item.id;
            break;
        }
    }
    if (testRuleId < 0)
    {
        qCritical() << "FAIL: Added day rule not found in dayRules().";
        return 1;
    }
    if (!DataStore::instance().toggleDayRule(testRuleId, true))
    {
        qCritical() << "FAIL: toggleDayRule failed.";
        return 1;
    }
    const QList<DayRuleItem> afterToggleRules = DataStore::instance().dayRules();
    bool toggledOk = false;
    for (const DayRuleItem& item : afterToggleRules)
    {
        if (item.id == testRuleId && item.completed)
        {
            toggledOk = true;
            break;
        }
    }
    if (!toggledOk)
    {
        qCritical() << "FAIL: Day rule was not marked completed after toggleDayRule.";
        return 1;
    }
    if (!DataStore::instance().resetDayRules())
    {
        qCritical() << "FAIL: resetDayRules failed.";
        return 1;
    }
    for (const DayRuleItem& item : DataStore::instance().dayRules())
    {
        if (item.completed)
        {
            qCritical() << "FAIL: Day rule still completed after resetDayRules:" << item.title;
            return 1;
        }
    }
    qInfo() << "PASS: day_rules CRUD verified.";

    // Test DataStore close() and open() persistence across reboot
    const QString persistenceDraft = QStringLiteral("Persistent draft across restart");
    DataStore::instance().saveNotesDraft(persistenceDraft, QStringLiteral("inbox"));
    DataStore::instance().toggleDayRule(testRuleId, true);

    DataStore::instance().close();
    if (DataStore::instance().isOpen())
    {
        qCritical() << "FAIL: DataStore still reported open after close().";
        return 1;
    }
    if (!DataStore::instance().open())
    {
        qCritical() << "FAIL: DataStore open() failed after close():" << DataStore::instance().lastError();
        return 1;
    }
    QString reopenedDraft;
    QString reopenedTarget;
    if (!DataStore::instance().getNotesDraft(&reopenedDraft, &reopenedTarget) || reopenedDraft != persistenceDraft || reopenedTarget != QStringLiteral("inbox"))
    {
        qCritical() << "FAIL: notes_draft not preserved across DataStore close()/open():" << reopenedDraft;
        return 1;
    }
    bool reopenedToggled = false;
    for (const DayRuleItem& item : DataStore::instance().dayRules())
    {
        if (item.id == testRuleId && item.completed)
        {
            reopenedToggled = true;
            break;
        }
    }
    if (!reopenedToggled)
    {
        qCritical() << "FAIL: day_rules completion state not preserved across close()/open().";
        return 1;
    }
    DataStore::instance().removeDayRule(testRuleId);
    DataStore::instance().clearNotesDraft();
    qInfo() << "PASS: DataStore close() and open() persistence across reopen verified.";

    // --- Stage 1B Self-Test: DeaveApiClient isolated local mock server ---
    {
        QTcpServer mockServer;
        if (!mockServer.listen(QHostAddress(QStringLiteral("127.0.0.1")), 0))
        {
            qCritical() << "FAIL: Could not bind mock QTcpServer on 127.0.0.1:0:" << mockServer.errorString();
            return 1;
        }

        const quint16 mockPort = mockServer.serverPort();
        const QString mockBaseUrl = QStringLiteral("http://127.0.0.1:%1").arg(mockPort);

        QObject::connect(&mockServer, &QTcpServer::newConnection, &mockServer, [&mockServer]()
        {
            while (QTcpSocket* socket = mockServer.nextPendingConnection())
            {
                QObject::connect(socket, &QTcpSocket::readyRead, socket, [socket]()
                {
                    const QByteArray requestData = socket->readAll();
                    if (requestData.contains("GET /health"))
                    {
                        const QByteArray body = "{\"status\":\"ok\",\"api_version\":\"1\",\"env\":\"mock_env\"}";
                        const QByteArray response = "HTTP/1.1 200 OK\r\n"
                                                    "Content-Type: application/json\r\n"
                                                    "Content-Length: " + QByteArray::number(body.size()) + "\r\n"
                                                    "Connection: close\r\n\r\n" + body;
                        socket->write(response);
                        socket->flush();
                        socket->disconnectFromHost();
                    }
                    else if (requestData.contains("GET /api/v1/memory/read"))
                    {
                        const QByteArray body = "{\"source\":\"mock_src\",\"path\":\"mock.md\",\"title\":\"Mock Title\","
                                                "\"content\":\"# Mock Content\",\"content_sha256\":\"mock_sha\","
                                                "\"size\":14,\"mtime\":1700000000.0,\"node_id\":\"01JMOCK\",\"sensitivity\":\"normal\"}";
                        const QByteArray response = "HTTP/1.1 200 OK\r\n"
                                                    "Content-Type: application/json\r\n"
                                                    "Content-Length: " + QByteArray::number(body.size()) + "\r\n"
                                                    "Connection: close\r\n\r\n" + body;
                        socket->write(response);
                        socket->flush();
                        socket->disconnectFromHost();
                    }
                });
                QObject::connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
            }
        });

        DeaveApiClient apiClient(mockBaseUrl);
        apiClient.setTimeoutMs(400);

        // 1. Verify health check 200
        QEventLoop loop;
        DeaveHealthInfo healthResult;
        apiClient.checkHealth([&loop, &healthResult](const DeaveHealthInfo& info)
        {
            healthResult = info;
            loop.quit();
        });
        loop.exec();

        if (!healthResult.available || healthResult.apiVersion != QStringLiteral("1") || healthResult.env != QStringLiteral("mock_env"))
        {
            qCritical() << "FAIL: DeaveApiClient mock health check failed: available=" << healthResult.available
                        << "apiVersion=" << healthResult.apiVersion << "env=" << healthResult.env;
            return 1;
        }
        qInfo() << "PASS: DeaveApiClient mock health check 200 verified.";

        // 2. Verify readMemoryNote 200
        QEventLoop readLoop;
        bool readSuccess = false;
        DeaveNoteReadResult readResult;
        apiClient.readMemoryNote(QStringLiteral("mock_src"), QStringLiteral("mock.md"),
            [&readLoop, &readSuccess, &readResult](bool success, const DeaveNoteReadResult& res, const QString&)
        {
            readSuccess = success;
            readResult = res;
            readLoop.quit();
        });
        readLoop.exec();

        if (!readSuccess || readResult.content != QStringLiteral("# Mock Content") || readResult.contentSha256 != QStringLiteral("mock_sha"))
        {
            qCritical() << "FAIL: DeaveApiClient mock readMemoryNote failed.";
            return 1;
        }
        qInfo() << "PASS: DeaveApiClient mock readMemoryNote verified.";

        // 3. Verify server close & connection failure handling
        mockServer.close();
        QElapsedTimer failTimer;
        failTimer.start();
        QEventLoop failLoop;
        DeaveHealthInfo failHealth;
        apiClient.checkHealth([&failLoop, &failHealth](const DeaveHealthInfo& info)
        {
            failHealth = info;
            failLoop.quit();
        });
        failLoop.exec();

        if (failHealth.available)
        {
            qCritical() << "FAIL: DeaveApiClient reported available after mock server close.";
            return 1;
        }
        qInfo() << "PASS: DeaveApiClient connection failure handled gracefully in" << failTimer.elapsed() << "ms.";

        // 4. Verify timeout handling on hanging server (non-blocking)
        QTcpServer hangServer;
        if (!hangServer.listen(QHostAddress(QStringLiteral("127.0.0.1")), 0))
        {
            qCritical() << "FAIL: Could not bind hangServer on 127.0.0.1:0:" << hangServer.errorString();
            return 1;
        }
        QObject::connect(&hangServer, &QTcpServer::newConnection, &hangServer, [&hangServer]()
        {
            while (QTcpSocket* s = hangServer.nextPendingConnection())
            {
                QObject::connect(s, &QTcpSocket::disconnected, s, &QObject::deleteLater);
            }
        });

        DeaveApiClient hangClient(QStringLiteral("http://127.0.0.1:%1").arg(hangServer.serverPort()));
        hangClient.setTimeoutMs(300);

        QElapsedTimer hangTimer;
        hangTimer.start();
        QEventLoop hangLoop;
        DeaveHealthInfo hangHealth;
        hangClient.checkHealth([&hangLoop, &hangHealth](const DeaveHealthInfo& info)
        {
            hangHealth = info;
            hangLoop.quit();
        });
        hangLoop.exec();

        const qint64 hangElapsed = hangTimer.elapsed();
        hangServer.close();

        if (hangHealth.available)
        {
            qCritical() << "FAIL: DeaveApiClient reported available on hanging server.";
            return 1;
        }
        if (hangElapsed < 250 || hangElapsed > 2500)
        {
            qCritical() << "FAIL: DeaveApiClient timeout timing abnormal:" << hangElapsed << "ms";
            return 1;
        }
        qInfo() << "PASS: DeaveApiClient transfer timeout (300ms) aborted hanging connection in" << hangElapsed << "ms non-blockingly.";
    }

    // --- Stage 3 Self-Test: NotesWidget and POST /api/v1/capture mock tests ---
    {
        QTcpServer captureServer;
        if (!captureServer.listen(QHostAddress(QStringLiteral("127.0.0.1")), 0))
        {
            qCritical() << "FAIL: Could not bind captureServer on 127.0.0.1:0:" << captureServer.errorString();
            return 1;
        }

        const quint16 capturePort = captureServer.serverPort();
        const QString captureBaseUrl = QStringLiteral("http://127.0.0.1:%1").arg(capturePort);

        QTemporaryFile tempTokenFile;
        if (!tempTokenFile.open())
        {
            qCritical() << "FAIL: Could not create temporary token file for self-test.";
            return 1;
        }
        tempTokenFile.write("stale_token_initial\n");
        tempTokenFile.flush();
        tempTokenFile.close();

        const QString expectedValidToken = QStringLiteral("valid_token_rotated");
        bool mock503Enabled = false;
        QSet<QString> seenClientIds;
        int captureRequestsCount = 0;

        QObject::connect(&captureServer, &QTcpServer::newConnection, &captureServer, [&captureServer, &captureRequestsCount, &mock503Enabled, &expectedValidToken, &seenClientIds, &tempTokenFile]()
        {
            while (QTcpSocket* socket = captureServer.nextPendingConnection())
            {
                QObject::connect(socket, &QTcpSocket::readyRead, socket, [socket, &captureRequestsCount, &mock503Enabled, &expectedValidToken, &seenClientIds, &tempTokenFile]()
                {
                    const QByteArray requestData = socket->readAll();
                    if (requestData.contains("GET /health"))
                    {
                        const QByteArray body = "{\"status\":\"ok\",\"api_version\":\"1\",\"env\":\"mock_env\"}";
                        const QByteArray response = "HTTP/1.1 200 OK\r\n"
                                                    "Content-Type: application/json\r\n"
                                                    "Content-Length: " + QByteArray::number(body.size()) + "\r\n"
                                                    "Connection: close\r\n\r\n" + body;
                        socket->write(response);
                        socket->flush();
                        socket->disconnectFromHost();
                        return;
                    }

                    if (requestData.contains("POST /api/v1/capture"))
                    {
                        captureRequestsCount++;

                        if (mock503Enabled)
                        {
                            const QByteArray body = "{\"detail\":\"Capture service is disabled (CAPTURE_ENABLED=False).\"}";
                            const QByteArray response = "HTTP/1.1 503 Service Unavailable\r\n"
                                                        "Content-Type: application/json\r\n"
                                                        "Content-Length: " + QByteArray::number(body.size()) + "\r\n"
                                                        "Connection: close\r\n\r\n" + body;
                            socket->write(response);
                            socket->flush();
                            socket->disconnectFromHost();
                            return;
                        }

                        // Extract X-Deave-Token header
                        QString receivedToken;
                        const int headerIdx = requestData.indexOf("X-Deave-Token:");
                        if (headerIdx >= 0)
                        {
                            const int endLine = requestData.indexOf("\r\n", headerIdx);
                            if (endLine > headerIdx)
                            {
                                receivedToken = QString::fromLatin1(
                                    requestData.mid(headerIdx + 14, endLine - (headerIdx + 14))
                                ).trimmed();
                            }
                        }

                        if (receivedToken != expectedValidToken)
                        {
                            // Stale or missing token: write the rotated fresh token to temp file so client can reload it!
                            if (tempTokenFile.open())
                            {
                                tempTokenFile.resize(0);
                                tempTokenFile.write(expectedValidToken.toUtf8() + "\n");
                                tempTokenFile.flush();
                                tempTokenFile.close();
                            }

                            const QByteArray body = "{\"detail\":\"Missing or invalid X-Deave-Token header.\"}";
                            const QByteArray response = "HTTP/1.1 401 Unauthorized\r\n"
                                                        "Content-Type: application/json\r\n"
                                                        "Content-Length: " + QByteArray::number(body.size()) + "\r\n"
                                                        "Connection: close\r\n\r\n" + body;
                            socket->write(response);
                            socket->flush();
                            socket->disconnectFromHost();
                            return;
                        }

                        // Valid token: parse JSON body
                        const int bodyIdx = requestData.indexOf("\r\n\r\n");
                        QByteArray bodyBytes;
                        if (bodyIdx >= 0)
                        {
                            bodyBytes = requestData.mid(bodyIdx + 4);
                        }
                        const QJsonDocument doc = QJsonDocument::fromJson(bodyBytes);
                        const QJsonObject obj = doc.isObject() ? doc.object() : QJsonObject();
                        const QString clientId = obj.value(QStringLiteral("client_id")).toString();
                        const QString target = obj.value(QStringLiteral("target")).toString();

                        if (seenClientIds.contains(clientId))
                        {
                            // Duplicate request: return 200 OK with duplicated: true
                            const QByteArray body = "{\"status\":\"ok\",\"client_id\":\"" + clientId.toUtf8() + "\","
                                                    "\"target\":\"" + target.toUtf8() + "\",\"seq\":1,"
                                                    "\"duplicated\":true,\"message\":\"Duplicate capture recorded.\"}";
                            const QByteArray response = "HTTP/1.1 200 OK\r\n"
                                                        "Content-Type: application/json\r\n"
                                                        "Content-Length: " + QByteArray::number(body.size()) + "\r\n"
                                                        "Connection: close\r\n\r\n" + body;
                            socket->write(response);
                            socket->flush();
                            socket->disconnectFromHost();
                            return;
                        }

                        seenClientIds.insert(clientId);
                        const QByteArray body = "{\"status\":\"ok\",\"client_id\":\"" + clientId.toUtf8() + "\","
                                                "\"target\":\"" + target.toUtf8() + "\",\"seq\":1,"
                                                "\"duplicated\":false,\"message\":\"Captured to " + target.toUtf8() + ".\"}";
                        const QByteArray response = "HTTP/1.1 201 Created\r\n"
                                                    "Content-Type: application/json\r\n"
                                                    "Content-Length: " + QByteArray::number(body.size()) + "\r\n"
                                                    "Connection: close\r\n\r\n" + body;
                        socket->write(response);
                        socket->flush();
                        socket->disconnectFromHost();
                        return;
                    }
                });
                QObject::connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
            }
        });

        // 1. Direct DeaveApiClient test: fresh 201 Created and duplicate 200 OK
        DeaveApiClient captureClient(captureBaseUrl);
        captureClient.setTimeoutMs(500);

        const QString testClientId = QStringLiteral("client-test-id-001");
        QEventLoop freshLoop;
        DeaveCaptureResult freshResult;
        captureClient.captureNote(
            QStringLiteral("inbox"),
            QStringLiteral("Fresh note text"),
            testClientId,
            expectedValidToken,
            [&freshLoop, &freshResult](const DeaveCaptureResult& res)
            {
                freshResult = res;
                freshLoop.quit();
            }
        );
        freshLoop.exec();

        if (!freshResult.success || freshResult.httpStatusCode != 201 || freshResult.duplicated)
        {
            qCritical() << "FAIL: captureNote fresh request failed:" << freshResult.httpStatusCode << freshResult.errorMessage;
            return 1;
        }
        qInfo() << "PASS: DeaveApiClient POST /api/v1/capture fresh 201 Created verified.";

        // Duplicate request with exact same client_id
        QEventLoop dupLoop;
        DeaveCaptureResult dupResult;
        captureClient.captureNote(
            QStringLiteral("inbox"),
            QStringLiteral("Duplicate attempt"),
            testClientId,
            expectedValidToken,
            [&dupLoop, &dupResult](const DeaveCaptureResult& res)
            {
                dupResult = res;
                dupLoop.quit();
            }
        );
        dupLoop.exec();

        if (!dupResult.success || dupResult.httpStatusCode != 200 || !dupResult.duplicated)
        {
            qCritical() << "FAIL: captureNote duplicate request failed:" << dupResult.httpStatusCode;
            return 1;
        }
        qInfo() << "PASS: DeaveApiClient POST /api/v1/capture duplicate 200 OK verified.";

        // 2. NotesWidget test: 401 token reload retry with QTemporaryFile
        NotesWidget notesWidget;
        notesWidget.setConfirmBeforeSend(false);
        notesWidget.setApiBaseUrl(captureBaseUrl);
        notesWidget.setApiTokenPath(tempTokenFile.fileName());

        const QString initialNoteText = QStringLiteral("Note to test 401 reload and retry");
        notesWidget.setDraftText(initialNoteText);
        if (notesWidget.draftText() != initialNoteText)
        {
            qCritical() << "FAIL: notesWidget draft text not set properly.";
            return 1;
        }

        // Trigger capture:
        // Request 1 is sent with "stale_token_initial" -> mock server writes "valid_token_rotated" to file and returns 401.
        // notesWidget receives 401 -> reloads token file -> sends Request 2 with "valid_token_rotated" -> returns 201.
        // notesWidget receives 201 -> clears draft editor and clears SQLite draft!
        captureRequestsCount = 0;
        notesWidget.triggerCapture();

        // Wait for draft to be cleared
        for (int i = 0; i < 50; ++i)
        {
            QCoreApplication::processEvents();
            if (notesWidget.draftText().isEmpty())
            {
                break;
            }
            QThread::msleep(20);
        }

        if (!notesWidget.draftText().isEmpty())
        {
            qCritical() << "FAIL: notesWidget draft text was not cleared after successful 401 retry.";
            return 1;
        }

        QString checkDraft;
        if (DataStore::instance().getNotesDraft(&checkDraft))
        {
            qCritical() << "FAIL: DataStore notes_draft was not cleared after successful capture:" << checkDraft;
            return 1;
        }
        if (captureRequestsCount < 2)
        {
            qCritical() << "FAIL: Expected at least 2 capture requests (401 + retry), got:" << captureRequestsCount;
            return 1;
        }
        qInfo() << "PASS: NotesWidget 401 token reload retry and draft clearing on 201 verified.";

        // 3. NotesWidget test: Draft retention on 503 / server failure
        mock503Enabled = true;
        const QString retainedText = QStringLiteral("Important text that must NOT be lost on 503");
        notesWidget.setDraftText(retainedText);

        notesWidget.triggerCapture();
        for (int i = 0; i < 20; ++i)
        {
            QCoreApplication::processEvents();
            QThread::msleep(20);
        }

        if (notesWidget.draftText() != retainedText)
        {
            qCritical() << "FAIL: notesWidget draft text lost on 503 error:" << notesWidget.draftText();
            return 1;
        }
        QString retainedDbDraft;
        if (!DataStore::instance().getNotesDraft(&retainedDbDraft) || retainedDbDraft != retainedText)
        {
            qCritical() << "FAIL: DataStore notes_draft lost on 503 error:" << retainedDbDraft;
            return 1;
        }
        qInfo() << "PASS: NotesWidget draft retention on 503 error verified.";

        // Also test draft retention when server is closed
        captureServer.close();
        notesWidget.triggerCapture();
        for (int i = 0; i < 20; ++i)
        {
            QCoreApplication::processEvents();
            QThread::msleep(20);
        }

        if (notesWidget.draftText() != retainedText)
        {
            qCritical() << "FAIL: notesWidget draft text lost on server close:" << notesWidget.draftText();
            return 1;
        }
        DataStore::instance().clearNotesDraft();
        qInfo() << "PASS: NotesWidget draft retention on server close verified.";
    }

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
        if (auto* wb = dynamic_cast<WidgetBase*>(w))
        {
            wb->setSavePrefsEnabled(false);
        }
        const QPoint savedPos = w->pos();
        w->show();
        QCoreApplication::processEvents();

        const QSize initialSize = w->size();
        for (QScreen* scr : screens) {
            w->move(scr->availableGeometry().center() - QPoint(w->width() / 2, w->height() / 2));
            QCoreApplication::processEvents();
            if (w->size() != initialSize) {
                qCritical() << "FAIL: Widget" << info.id << "changed size on screen move:"
                            << initialSize << "->" << w->size();
                w->move(savedPos);
                QCoreApplication::processEvents();
                delete w;
                return 1;
            }
        }
        qInfo() << "PASS: Widget" << info.id << "size locked at" << w->size() << "across all screens.";

        if (info.id == "notes")
        {
            if (initialSize != QSize(380, 420))
            {
                qCritical() << "FAIL: Widget notes size is" << initialSize << "expected QSize(380, 420).";
                w->move(savedPos);
                QCoreApplication::processEvents();
                delete w;
                return 1;
            }
        }

        if (auto* wisdom = dynamic_cast<DailyWisdomWidget*>(w))
        {
            const int entries = wisdom->entryCount();
            for (int eIdx = 0; eIdx < entries; ++eIdx)
            {
                for (int sIdx = 0; sIdx < 3; ++sIdx)
                {
                    wisdom->setOperatorState(sIdx, false);
                    wisdom->showEntry(eIdx);
                    QCoreApplication::processEvents();

                    const QSize stateSize = wisdom->size();
                    for (QScreen* scr : screens)
                    {
                        wisdom->move(scr->availableGeometry().center() - QPoint(wisdom->width() / 2, wisdom->height() / 2));
                        QCoreApplication::processEvents();
                        if (wisdom->size() != stateSize)
                        {
                            qCritical() << "FAIL: Widget daily_wisdom changed size on screen move:"
                                        << "entry=" << eIdx << "state=" << sIdx
                                        << "screen=" << scr->name()
                                        << stateSize << "->" << wisdom->size();
                            wisdom->move(savedPos);
                            QCoreApplication::processEvents();
                            delete w;
                            return 1;
                        }
                    }
                }
            }
            qInfo() << "PASS: Widget" << info.id << "verified across"
                    << entries << "entries x 3 operator states x" << screens.size() << "screens.";
        }

        w->move(savedPos);
        QCoreApplication::processEvents();
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

namespace {

bool isSingleInstanceBypassed(const QStringList& args)
{
    if (args.contains(QStringLiteral("--self-test"))
        || args.contains(QStringLiteral("--isolated"))
        || args.contains(QStringLiteral("--screenshot-all"))
        || args.contains(QStringLiteral("--clear-notes-cache"))
        || args.contains(QStringLiteral("--test-deave-api")))
    {
        return true;
    }

    for (const QString& arg : args)
    {
        if (arg == QStringLiteral("--screenshot-dir")
            || arg.startsWith(QStringLiteral("--screenshot-dir=")))
        {
            return true;
        }
    }
    return false;
}

bool notifyRunningInstance(const QString& serverName)
{
    QLocalSocket socket;
    socket.connectToServer(serverName);
    if (!socket.waitForConnected(400))
    {
        return false;
    }
    socket.write("SHOW\n");
    socket.flush();
    socket.waitForBytesWritten(400);
    socket.disconnectFromServer();
    return true;
}

} // namespace

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

    QTemporaryDir isolatedDir;
    QString isolatedAppData;
    struct IsolatedScopeGuard
    {
        QString& dirPath;
        ~IsolatedScopeGuard()
        {
            if (!dirPath.isEmpty())
            {
                DataStore::instance().close();
                QDir(dirPath).removeRecursively();
            }
        }
    } isolatedGuard{isolatedAppData};

    if (app.arguments().contains(QStringLiteral("--isolated")) && isolatedDir.isValid())
    {
        QApplication::setOrganizationName(QStringLiteral("StudyWidgetsIsolatedTest"));
        QStandardPaths::setTestModeEnabled(true);
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, isolatedDir.path());
        DataStore::instance().close();
        isolatedAppData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        QDir(isolatedAppData).removeRecursively();
    }

    if (app.arguments().contains("--self-test"))
    {
#ifdef Q_OS_WIN
        AttachConsole(ATTACH_PARENT_PROCESS);
        freopen("CONOUT$", "w", stdout);
        freopen("CONOUT$", "w", stderr);
#endif
        return runSelfTest();
    }

    if (app.arguments().contains(QStringLiteral("--clear-notes-cache")))
    {
#ifdef Q_OS_WIN
        AttachConsole(ATTACH_PARENT_PROCESS);
        freopen("CONOUT$", "w", stdout);
        freopen("CONOUT$", "w", stderr);
#endif
        if (!DataStore::instance().open())
        {
            fprintf(stderr, "FAIL: Could not open DataStore for --clear-notes-cache: %s\n",
                    DataStore::instance().lastError().toLocal8Bit().constData());
            fflush(stderr);
            qCritical() << "FAIL: Could not open DataStore for --clear-notes-cache:" << DataStore::instance().lastError();
            return 1;
        }

        const int beforeCount = DataStore::instance().cachedNotesCount();
        if (!DataStore::instance().clearCachedNotes())
        {
            fprintf(stderr, "FAIL: clearCachedNotes() failed: %s\n",
                    DataStore::instance().lastError().toLocal8Bit().constData());
            fflush(stderr);
            qCritical() << "FAIL: clearCachedNotes() failed:" << DataStore::instance().lastError();
            return 1;
        }

        fprintf(stdout, "PASS: Cleared %d cached notes from SQLite.\n", beforeCount);
        fflush(stdout);
        qInfo().noquote() << QStringLiteral("PASS: Cleared %1 cached notes from SQLite.").arg(beforeCount);
        return 0;
    }

    if (app.arguments().contains(QStringLiteral("--test-deave-api")))
    {
#ifdef Q_OS_WIN
        AttachConsole(ATTACH_PARENT_PROCESS);
        freopen("CONOUT$", "w", stdout);
        freopen("CONOUT$", "w", stderr);
#endif
        if (!DataStore::instance().open())
        {
            fprintf(stderr, "FAIL: Could not open DataStore for --test-deave-api: %s\n",
                    DataStore::instance().lastError().toLocal8Bit().constData());
            fflush(stderr);
            qCritical() << "FAIL: Could not open DataStore for --test-deave-api:" << DataStore::instance().lastError();
            return 1;
        }

        DeaveApiClient client;
        QEventLoop loop;
        DeaveHealthInfo health;
        client.checkHealth([&loop, &health](const DeaveHealthInfo& info)
        {
            health = info;
            loop.quit();
        });
        loop.exec();

        if (health.available)
        {
            fprintf(stdout, "[INFO] deave is ONLINE: api_version=%s, env=%s\n",
                    health.apiVersion.toLocal8Bit().constData(),
                    health.env.toLocal8Bit().constData());
            fflush(stdout);
            qInfo().noquote() << QStringLiteral("[INFO] deave is ONLINE: api_version=%1, env=%2").arg(health.apiVersion, health.env);
            const QString testSource = QStringLiteral("memory");
            const QString testPath = QStringLiteral("online_test_note.md");
            const QString testContent = QStringLiteral("# Online Test Note\nVerified deave online connectivity.");
            const bool saved = DataStore::instance().saveCachedNote(testSource, testPath, QStringLiteral("Online Test"), testContent);
            if (!saved)
            {
                fprintf(stderr, "FAIL: Could not save note to SQLite cache.\n");
                fflush(stderr);
                qCritical() << "FAIL: Could not save note to SQLite cache.";
                return 1;
            }
            CachedNote retrieved;
            if (!DataStore::instance().getCachedNote(testSource, testPath, &retrieved))
            {
                fprintf(stderr, "FAIL: Could not retrieve cached note.\n");
                fflush(stderr);
                qCritical() << "FAIL: Could not retrieve cached note.";
                return 1;
            }
            fprintf(stdout, "PASS: deave online health verified and note successfully cached.\n");
            fflush(stdout);
            qInfo() << "PASS: deave online health verified and note successfully cached.";
            return 0;
        }
        else
        {
            fprintf(stdout, "[INFO] deave is OFFLINE: deave недоступен\n");
            fflush(stdout);
            qInfo() << "[INFO] deave is OFFLINE: deave недоступен";
            const QString testSource = QStringLiteral("memory");
            const QString testPath = QStringLiteral("offline_fallback_note.md");
            const QString testContent = QStringLiteral("# Offline Fallback Note\nTesting cached note read during offline mode.");
            DataStore::instance().saveCachedNote(testSource, testPath, QStringLiteral("Offline Fallback"), testContent);
            CachedNote cached;
            if (!DataStore::instance().getCachedNote(testSource, testPath, &cached) || cached.content != testContent)
            {
                fprintf(stderr, "FAIL: Cached note fallback retrieval failed in offline mode.\n");
                fflush(stderr);
                qCritical() << "FAIL: Cached note fallback retrieval failed in offline mode.";
                return 1;
            }
            fprintf(stdout, "PASS: Cached note fallback verified successfully while deave is offline.\n");
            fflush(stdout);
            qInfo() << "PASS: Cached note fallback verified successfully while deave is offline.";
            return 0;
        }
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

    const QString singleInstanceServerName = QStringLiteral("deavers_StudyWidgets_SingleInstance_v1");
    const QString lockDir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QLockFile instanceLock(QDir(lockDir).filePath(QStringLiteral("StudyWidgets.lock")));
    QLocalServer singleInstanceServer;
    ControlPanel* controlPanelPtr = nullptr;
    bool pendingShowFromSecondInstance = false;

    if (!isSingleInstanceBypassed(app.arguments()))
    {
        QDir().mkpath(lockDir);
        instanceLock.setStaleLockTime(0);

        if (!instanceLock.tryLock(0))
        {
            if (notifyRunningInstance(singleInstanceServerName))
            {
                return 0;
            }

            QThread::msleep(150);
            if (notifyRunningInstance(singleInstanceServerName))
            {
                return 0;
            }

            if (instanceLock.error() == QLockFile::LockFailedError)
            {
                instanceLock.removeStaleLockFile();
                if (!instanceLock.tryLock(0))
                {
                    notifyRunningInstance(singleInstanceServerName);
                    return 0;
                }
            }
            else
            {
                notifyRunningInstance(singleInstanceServerName);
                return 0;
            }
        }

        QLocalServer::removeServer(singleInstanceServerName);
        if (singleInstanceServer.listen(singleInstanceServerName))
        {
            auto handleClient = [&controlPanelPtr, &pendingShowFromSecondInstance](QLocalSocket* client)
            {
                const QByteArray payload = client->readAll();
                if (!payload.contains("SHOW"))
                {
                    return;
                }
                if (controlPanelPtr)
                {
                    controlPanelPtr->activateFromSecondInstance();
                }
                else
                {
                    pendingShowFromSecondInstance = true;
                }
            };

            QObject::connect(&singleInstanceServer, &QLocalServer::newConnection, &app, [&singleInstanceServer, handleClient]()
            {
                while (QLocalSocket* client = singleInstanceServer.nextPendingConnection())
                {
                    QObject::connect(client, &QLocalSocket::readyRead, client, [client, handleClient]()
                    {
                        handleClient(client);
                    });
                    QObject::connect(client, &QLocalSocket::disconnected, client, &QObject::deleteLater);
                    if (client->bytesAvailable() > 0)
                    {
                        handleClient(client);
                    }
                }
            });
        }
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
    controlPanelPtr = &controlPanel;
    controlPanel.show();
    if (pendingShowFromSecondInstance)
    {
        controlPanel.activateFromSecondInstance();
        pendingShowFromSecondInstance = false;
    }

    const int rc = app.exec();
    controlPanelPtr = nullptr;
    singleInstanceServer.close();
    instanceLock.unlock();
    return rc;
}
