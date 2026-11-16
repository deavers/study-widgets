#include "NotesWidget.h"

#include "../DataStore.h"
#include "../Theme.h"
#include "../WidgetRegistry.h"

#include <QCheckBox>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QUuid>
#include <QVBoxLayout>

NotesWidget::NotesWidget(QWidget* parent)
    : WidgetBase(QStringLiteral("notes"), parent)
    , m_apiClient(new DeaveApiClient(QStringLiteral("http://127.0.0.1:8000"), this))
{
    setBaseWidgetSize(380, 420);
    setChromeTitle(QString::fromUtf8("Заметки"), QStringLiteral("Inbox"));
    setRootMargins(12, 10, 12, 10);

    DataStore::instance().seedDefaultDayRulesIfEmpty();

    setupUi();
    loadDraftFromStore();
    rebuildDayRulesList();
    checkDaemonHealth();
}

NotesWidget::~NotesWidget()
{
}

void NotesWidget::setApiBaseUrl(const QString& baseUrl)
{
    if (m_apiClient)
    {
        m_apiClient->setBaseUrl(baseUrl);
    }
}

void NotesWidget::setApiTokenPath(const QString& path)
{
    m_customTokenPath = path;
    m_cachedToken.clear();
}

void NotesWidget::setConfirmBeforeSend(bool confirm)
{
    m_confirmBeforeSend = confirm;
}

QString NotesWidget::draftText() const
{
    if (m_draftEditor)
    {
        return m_draftEditor->toPlainText();
    }
    return QString();
}

void NotesWidget::setDraftText(const QString& text)
{
    if (m_draftEditor)
    {
        m_draftEditor->setPlainText(text);
        persistDraftToStore();
    }
}

QString NotesWidget::selectedTarget() const
{
    return m_currentTarget;
}

void NotesWidget::setSelectedTarget(const QString& target)
{
    setTarget(target);
}

void NotesWidget::triggerCapture()
{
    executeCapture();
}

int NotesWidget::dayRulesCount() const
{
    return DataStore::instance().dayRules().size();
}

void NotesWidget::buildContextMenu(QMenu* menu)
{
    WidgetBase::buildContextMenu(menu);
}

void NotesWidget::setupUi()
{
    // Tabs header
    QHBoxLayout* tabsLayout = new QHBoxLayout();
    tabsLayout->setSpacing(6);

    m_notesTabBtn = new QPushButton(QString::fromUtf8("📝 Заметки"));
    m_notesTabBtn->setProperty("variant", "chip");
    m_notesTabBtn->setCheckable(true);
    m_notesTabBtn->setChecked(true);
    tabsLayout->addWidget(m_notesTabBtn, 1);

    m_rulesTabBtn = new QPushButton(QString::fromUtf8("✓ Правила дня"));
    m_rulesTabBtn->setProperty("variant", "chip");
    m_rulesTabBtn->setCheckable(true);
    m_rulesTabBtn->setChecked(false);
    tabsLayout->addWidget(m_rulesTabBtn, 1);

    bodyLayout()->addLayout(tabsLayout);

    connect(m_notesTabBtn, &QPushButton::clicked, this, [this]()
    {
        switchTab(0);
    });
    connect(m_rulesTabBtn, &QPushButton::clicked, this, [this]()
    {
        switchTab(1);
    });

    // Stacked widget for tabs
    m_stack = new QStackedWidget();

    QWidget* capturePage = new QWidget();
    setupCaptureTab(capturePage);
    m_stack->addWidget(capturePage);

    QWidget* rulesPage = new QWidget();
    setupRulesTab(rulesPage);
    m_stack->addWidget(rulesPage);

    bodyLayout()->addWidget(m_stack, 1);
}

void NotesWidget::setupCaptureTab(QWidget* container)
{
    QVBoxLayout* layout = new QVBoxLayout(container);
    layout->setContentsMargins(0, 4, 0, 0);
    layout->setSpacing(6);

    // Target selector and daemon status
    QHBoxLayout* topRow = new QHBoxLayout();
    topRow->setSpacing(6);

    m_inboxBtn = new QPushButton(QStringLiteral("inbox"));
    m_inboxBtn->setProperty("variant", "chip");
    m_inboxBtn->setCheckable(true);
    m_inboxBtn->setChecked(true);
    topRow->addWidget(m_inboxBtn);

    m_timelogBtn = new QPushButton(QStringLiteral("timelog"));
    m_timelogBtn->setProperty("variant", "chip");
    m_timelogBtn->setCheckable(true);
    m_timelogBtn->setChecked(false);
    topRow->addWidget(m_timelogBtn);

    connect(m_inboxBtn, &QPushButton::clicked, this, [this]()
    {
        setTarget(QStringLiteral("inbox"));
    });
    connect(m_timelogBtn, &QPushButton::clicked, this, [this]()
    {
        setTarget(QStringLiteral("timelog"));
    });

    topRow->addStretch();

    m_daemonStatusLabel = new QLabel(QString::fromUtf8("● deave"));
    m_daemonStatusLabel->setProperty("role", "dim");
    topRow->addWidget(m_daemonStatusLabel);

    layout->addLayout(topRow);

    // Note reader row
    QHBoxLayout* readerRow = new QHBoxLayout();
    readerRow->setSpacing(4);

    m_readerSourceInput = new QLineEdit(QStringLiteral("vault"));
    m_readerSourceInput->setPlaceholderText(QStringLiteral("source"));
    m_readerSourceInput->setFixedWidth(60);
    readerRow->addWidget(m_readerSourceInput);

    m_readerPathInput = new QLineEdit();
    m_readerPathInput->setPlaceholderText(QString::fromUtf8("docs/note.md"));
    readerRow->addWidget(m_readerPathInput, 1);

    m_readBtn = new QPushButton(QString::fromUtf8("Открыть"));
    m_readBtn->setProperty("variant", "ghost");
    readerRow->addWidget(m_readBtn);

    layout->addLayout(readerRow);

    m_readerStatusLabel = new QLabel();
    m_readerStatusLabel->setProperty("role", "dim");
    m_readerStatusLabel->setWordWrap(true);
    layout->addWidget(m_readerStatusLabel);

    connect(m_readBtn, &QPushButton::clicked, this, [this]()
    {
        readNote(m_readerSourceInput->text(), m_readerPathInput->text());
    });
    connect(m_readerPathInput, &QLineEdit::returnPressed, this, [this]()
    {
        readNote(m_readerSourceInput->text(), m_readerPathInput->text());
    });

    // Draft editor
    m_draftEditor = new QPlainTextEdit();
    m_draftEditor->setPlaceholderText(QString::fromUtf8("Быстрая мысль, задача или заметка..."));
    m_draftEditor->setStyleSheet(QStringLiteral(
        "QPlainTextEdit { "
        "background: rgba(0, 0, 0, 70); "
        "border: 1px solid rgba(255, 255, 255, 26); "
        "border-radius: 8px; "
        "padding: 6px; "
        "color: #EEF0F8; "
        "font-size: 13px; "
        "}"
    ));
    layout->addWidget(m_draftEditor, 1);

    connect(m_draftEditor, &QPlainTextEdit::textChanged, this, &NotesWidget::persistDraftToStore);

    // Actions row
    QHBoxLayout* actionsRow = new QHBoxLayout();
    actionsRow->setSpacing(6);

    m_sendBtn = new QPushButton(QString::fromUtf8("Отправить в deave"));
    m_sendBtn->setProperty("variant", "primary");
    actionsRow->addWidget(m_sendBtn, 1);

    m_clearBtn = new QPushButton(QString::fromUtf8("Очистить"));
    m_clearBtn->setProperty("variant", "ghost");
    actionsRow->addWidget(m_clearBtn);

    layout->addLayout(actionsRow);

    connect(m_sendBtn, &QPushButton::clicked, this, &NotesWidget::executeCapture);
    connect(m_clearBtn, &QPushButton::clicked, this, [this]()
    {
        m_draftEditor->clear();
        DataStore::instance().clearNotesDraft();
        m_captureFeedbackLabel->setText(QString::fromUtf8("Черновик очищен."));
    });

    m_captureFeedbackLabel = new QLabel();
    m_captureFeedbackLabel->setProperty("role", "banner");
    m_captureFeedbackLabel->setWordWrap(true);
    layout->addWidget(m_captureFeedbackLabel);
}

void NotesWidget::setupRulesTab(QWidget* container)
{
    QVBoxLayout* layout = new QVBoxLayout(container);
    layout->setContentsMargins(0, 4, 0, 0);
    layout->setSpacing(6);

    // New rule input row
    QHBoxLayout* inputRow = new QHBoxLayout();
    inputRow->setSpacing(6);

    m_newRuleInput = new QLineEdit();
    m_newRuleInput->setPlaceholderText(QString::fromUtf8("Новое правило дня..."));
    inputRow->addWidget(m_newRuleInput, 1);

    m_addRuleBtn = new QPushButton(QStringLiteral("+"));
    m_addRuleBtn->setProperty("variant", "primary");
    m_addRuleBtn->setFixedSize(30, 30);
    inputRow->addWidget(m_addRuleBtn);

    layout->addLayout(inputRow);

    connect(m_addRuleBtn, &QPushButton::clicked, this, &NotesWidget::addDayRuleFromInput);
    connect(m_newRuleInput, &QLineEdit::returnPressed, this, &NotesWidget::addDayRuleFromInput);

    // Scrollable rules checklist
    QScrollArea* scroll = new QScrollArea();
    scroll->setWidgetResizable(true);
    scroll->setStyleSheet(QStringLiteral("QScrollArea { background: transparent; border: none; }"));

    m_rulesListContainer = new QWidget();
    m_rulesListContainer->setStyleSheet(QStringLiteral("background: transparent;"));
    m_rulesListLayout = new QVBoxLayout(m_rulesListContainer);
    m_rulesListLayout->setContentsMargins(0, 0, 0, 0);
    m_rulesListLayout->setSpacing(4);

    scroll->setWidget(m_rulesListContainer);
    layout->addWidget(scroll, 1);

    // Footer row
    QHBoxLayout* footerRow = new QHBoxLayout();
    footerRow->setSpacing(6);

    QPushButton* resetBtn = new QPushButton(QString::fromUtf8("Сбросить галочки"));
    resetBtn->setProperty("variant", "ghost");
    footerRow->addWidget(resetBtn);

    connect(resetBtn, &QPushButton::clicked, this, &NotesWidget::resetAllDayRules);

    footerRow->addStretch();

    m_rulesSummaryLabel = new QLabel();
    m_rulesSummaryLabel->setProperty("role", "pill");
    footerRow->addWidget(m_rulesSummaryLabel);

    layout->addLayout(footerRow);
}

void NotesWidget::switchTab(int index)
{
    m_notesTabBtn->setChecked(index == 0);
    m_rulesTabBtn->setChecked(index == 1);
    m_stack->setCurrentIndex(index);
}

void NotesWidget::setTarget(const QString& target)
{
    m_currentTarget = target;
    m_inboxBtn->setChecked(target == QStringLiteral("inbox"));
    m_timelogBtn->setChecked(target == QStringLiteral("timelog"));
    setChromeTitle(QString::fromUtf8("Заметки"), target);
    persistDraftToStore();
}

void NotesWidget::loadDraftFromStore()
{
    QString text;
    QString target;
    if (DataStore::instance().getNotesDraft(&text, &target))
    {
        if (!target.isEmpty())
        {
            setTarget(target);
        }
        if (m_draftEditor)
        {
            m_draftEditor->setPlainText(text);
        }
    }
}

void NotesWidget::persistDraftToStore()
{
    if (m_draftEditor)
    {
        DataStore::instance().saveNotesDraft(m_draftEditor->toPlainText(), m_currentTarget);
    }
}

QString NotesWidget::resolveTokenPath() const
{
    if (!m_customTokenPath.isEmpty())
    {
        return m_customTokenPath;
    }

    QSettings settings;
    const QString cfgPath = settings.value(QStringLiteral("deave/apiTokenPath")).toString().trimmed();
    if (!cfgPath.isEmpty() && QFile::exists(cfgPath))
    {
        return cfgPath;
    }

    const QString envTokenPath = qEnvironmentVariable("DEAVE_API_TOKEN_PATH").trimmed();
    if (!envTokenPath.isEmpty() && QFile::exists(envTokenPath))
    {
        return envTokenPath;
    }

    const QString envDataDir = qEnvironmentVariable("DEAVE_DATA_DIR").trimmed();
    if (!envDataDir.isEmpty())
    {
        const QString candidate = QDir(envDataDir).filePath(QStringLiteral("api-token"));
        if (QFile::exists(candidate))
        {
            return candidate;
        }
    }

    const QString appData = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    const QString standardPath = QDir(appData).filePath(QStringLiteral("deave/memory/api-token"));
    if (QFile::exists(standardPath))
    {
        return standardPath;
    }

    return QString();
}

QString NotesWidget::readTokenFromDisk(const QString& path) const
{
    if (path.isEmpty() || !QFile::exists(path))
    {
        return QString();
    }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        return QString();
    }
    return QString::fromUtf8(file.readAll()).trimmed();
}

void NotesWidget::updateDaemonStatus(bool online)
{
    if (m_daemonStatusLabel)
    {
        if (online)
        {
            m_daemonStatusLabel->setText(QString::fromUtf8("● онлайн"));
            m_daemonStatusLabel->setStyleSheet(QStringLiteral("color: #4ade80;"));
        }
        else
        {
            m_daemonStatusLabel->setText(QString::fromUtf8("● deave недоступен"));
            m_daemonStatusLabel->setStyleSheet(QStringLiteral("color: #f87171;"));
        }
    }
}

void NotesWidget::checkDaemonHealth()
{
    if (!m_apiClient)
    {
        return;
    }
    m_apiClient->checkHealth([this](const DeaveHealthInfo& info)
    {
        updateDaemonStatus(info.available);
    });
}

void NotesWidget::executeCapture()
{
    if (m_isSending)
    {
        return;
    }

    const QString text = m_draftEditor->toPlainText().trimmed();
    if (text.isEmpty())
    {
        m_captureFeedbackLabel->setText(QString::fromUtf8("Введите текст заметки."));
        return;
    }

    if (m_confirmBeforeSend)
    {
        const QMessageBox::StandardButton reply = QMessageBox::question(
            this,
            QString::fromUtf8("Подтверждение отправки"),
            QString::fromUtf8("Отправить заметку в %1?").arg(m_currentTarget),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::Yes
        );
        if (reply != QMessageBox::Yes)
        {
            return;
        }
    }

    m_isSending = true;
    m_captureFeedbackLabel->setText(QString::fromUtf8("Отправка в deave..."));

    const QString clientId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QString tokenPath = resolveTokenPath();
    if (m_cachedToken.isEmpty() && !tokenPath.isEmpty())
    {
        m_cachedToken = readTokenFromDisk(tokenPath);
    }

    auto handleResult = [this, clientId, text, tokenPath](const DeaveCaptureResult& res)
    {
        m_isSending = false;
        if (res.success)
        {
            m_draftEditor->clear();
            DataStore::instance().clearNotesDraft();
            m_captureFeedbackLabel->setText(QString::fromUtf8("✓ Успешно захвачено в %1.").arg(res.target));
            setChromeTitle(QString::fromUtf8("Заметки"), QString::fromUtf8("Захвачено"));
            updateDaemonStatus(true);
        }
        else if (res.httpStatusCode == 401)
        {
            // Token mismatch or expired: re-read api-token file from disk and retry once with same client_id
            const QString freshToken = readTokenFromDisk(tokenPath);
            if (!freshToken.isEmpty())
            {
                m_cachedToken = freshToken;
                m_isSending = true;
                m_apiClient->captureNote(m_currentTarget, text, clientId, m_cachedToken, [this](const DeaveCaptureResult& retryRes)
                {
                    m_isSending = false;
                    if (retryRes.success)
                    {
                        m_draftEditor->clear();
                        DataStore::instance().clearNotesDraft();
                        m_captureFeedbackLabel->setText(QString::fromUtf8("✓ Захвачено после обновления токена."));
                        setChromeTitle(QString::fromUtf8("Заметки"), QString::fromUtf8("Захвачено"));
                        updateDaemonStatus(true);
                    }
                    else
                    {
                        // Retry failed: text is NEVER cleared
                        m_captureFeedbackLabel->setText(QString::fromUtf8("Ошибка авторизации: неверный токен. Черновик сохранён."));
                    }
                });
                return;
            }

            // Token file not found: text is NEVER cleared
            m_captureFeedbackLabel->setText(QString::fromUtf8("Ошибка авторизации (401). api-token не найден. Черновик сохранён."));
        }
        else if (res.httpStatusCode == 503)
        {
            // Capture service disabled: text is NEVER cleared
            m_captureFeedbackLabel->setText(QString::fromUtf8("deave недоступен: захват отключён (503). Черновик сохранён."));
            updateDaemonStatus(false);
        }
        else
        {
            // Network offline / timeout / other error: text is NEVER cleared
            updateDaemonStatus(false);
            const QString err = res.errorMessage.isEmpty() ? QString::fromUtf8("таймаут сети") : res.errorMessage;
            m_captureFeedbackLabel->setText(QString::fromUtf8("deave недоступен (%1). Черновик сохранён.").arg(err));
        }
    };

    m_apiClient->captureNote(m_currentTarget, text, clientId, m_cachedToken, handleResult);
}

void NotesWidget::readNote(const QString& source, const QString& path)
{
    const QString src = source.trimmed().isEmpty() ? QStringLiteral("vault") : source.trimmed();
    const QString relPath = path.trimmed();
    if (relPath.isEmpty())
    {
        m_readerStatusLabel->setText(QString::fromUtf8("Укажите путь к заметке."));
        return;
    }

    m_readerStatusLabel->setText(QString::fromUtf8("Чтение заметки..."));
    m_apiClient->readMemoryNote(src, relPath, [this, src, relPath](bool success, const DeaveNoteReadResult& note, const QString& errorMsg)
    {
        if (success)
        {
            DataStore::instance().saveCachedNote(src, relPath, note.title, note.content, note.contentSha256, note.size);
            m_draftEditor->setPlainText(note.content);
            m_readerStatusLabel->setText(QString::fromUtf8("✓ Заметка загружена (онлайн)."));
            updateDaemonStatus(true);
        }
        else
        {
            updateDaemonStatus(false);
            CachedNote cached;
            if (DataStore::instance().getCachedNote(src, relPath, &cached))
            {
                m_draftEditor->setPlainText(cached.content);
                m_readerStatusLabel->setText(QString::fromUtf8("● deave недоступен: загружена из кэша."));
            }
            else
            {
                m_readerStatusLabel->setText(QString::fromUtf8("● deave недоступен (%1): заметка не найдена в кэше.").arg(errorMsg));
            }
        }
    });
}

void NotesWidget::rebuildDayRulesList()
{
    if (!m_rulesListLayout)
    {
        return;
    }

    QLayoutItem* child;
    while ((child = m_rulesListLayout->takeAt(0)) != nullptr)
    {
        if (child->widget())
        {
            child->widget()->deleteLater();
        }
        delete child;
    }

    const QList<DayRuleItem> rules = DataStore::instance().dayRules();
    int completedCount = 0;

    for (const DayRuleItem& rule : rules)
    {
        if (rule.completed)
        {
            completedCount++;
        }

        QWidget* card = new QWidget();
        card->setProperty("role", "card");
        QHBoxLayout* cardLayout = new QHBoxLayout(card);
        cardLayout->setContentsMargins(8, 6, 8, 6);
        cardLayout->setSpacing(8);

        QCheckBox* check = new QCheckBox();
        check->setChecked(rule.completed);
        cardLayout->addWidget(check);

        QLabel* label = new QLabel(rule.title);
        label->setWordWrap(true);
        if (rule.completed)
        {
            label->setProperty("role", "dim");
        }
        cardLayout->addWidget(label, 1);

        QPushButton* delBtn = new QPushButton(QString::fromUtf8("✕"));
        delBtn->setProperty("variant", "ghost");
        delBtn->setFixedSize(24, 24);
        cardLayout->addWidget(delBtn);

        const qint64 ruleId = rule.id;
        connect(check, &QCheckBox::toggled, this, [this, ruleId](bool checked)
        {
            DataStore::instance().toggleDayRule(ruleId, checked);
            rebuildDayRulesList();
        });

        connect(delBtn, &QPushButton::clicked, this, [this, ruleId]()
        {
            DataStore::instance().removeDayRule(ruleId);
            rebuildDayRulesList();
        });

        m_rulesListLayout->addWidget(card);
    }

    m_rulesListLayout->addStretch();
    m_rulesSummaryLabel->setText(QString::fromUtf8("Выполнено: %1/%2").arg(completedCount).arg(rules.size()));
}

void NotesWidget::addDayRuleFromInput()
{
    if (!m_newRuleInput)
    {
        return;
    }
    const QString text = m_newRuleInput->text().trimmed();
    if (!text.isEmpty())
    {
        DataStore::instance().addDayRule(text, dayRulesCount());
        m_newRuleInput->clear();
        rebuildDayRulesList();
    }
}

void NotesWidget::resetAllDayRules()
{
    DataStore::instance().resetDayRules();
    rebuildDayRulesList();
}

REGISTER_WIDGET("notes", "Быстрые заметки и правила дня", NotesWidget)
