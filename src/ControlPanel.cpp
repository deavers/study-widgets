#include "ControlPanel.h"

#include "CategoryConfigLoader.h"
#include "Theme.h"
#include "WidgetBase.h"
#include "WidgetRegistry.h"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QCloseEvent>
#include <QFont>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QPushButton>
#include <QScreen>
#include <QSettings>
#include <QSignalBlocker>
#include <QStyle>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QVBoxLayout>

ControlPanel::ControlPanel(QWidget* parent)
    : QWidget(parent)
{
    setWindowTitle("Deave StudyWidgets — Control Center");
    setMinimumWidth(460);
    resize(480, 610);

    setStyleSheet(R"(
        QWidget {
            background-color: #0E1020;
            color: #EEF0F8;
            font-family: "Segoe UI";
        }
        QCheckBox {
            background-color: #151A24;
            border: 1px solid #242C3D;
            border-radius: 8px;
            padding: 8px 11px;
            spacing: 10px;
            font-size: 12px;
            font-weight: 600;
            color: #EEF0F8;
        }
        QCheckBox:hover {
            background-color: #1B2230;
            border-color: #C4B5FD;
        }
        QCheckBox::indicator {
            width: 17px;
            height: 17px;
            border-radius: 5px;
            border: 1.5px solid #3B4866;
            background-color: #10141D;
        }
        QCheckBox::indicator:checked {
            background-color: #00C896;
            border-color: #8FD6A8;
        }
        QPushButton {
            background-color: #19202E;
            border: 1px solid #283246;
            border-radius: 8px;
            color: #E7EAF0;
            padding: 7px 10px;
            font-size: 11px;
            font-weight: 600;
        }
        QPushButton:hover {
            background-color: #232C3F;
            border-color: #C4B5FD;
        }
        QPushButton:pressed {
            background-color: #121722;
        }
        QPushButton:checked {
            background-color: #16364D;
            border: 1px solid #C4B5FD;
            color: #C4B5FD;
        }
    )");

    setupTrayIcon();

    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(20, 16, 20, 16);
    rootLayout->setSpacing(9);

    auto* titleLabel = new QLabel(QString::fromUtf8("⚡ Deave StudyWidgets"));
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(16);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);

    auto* subtitleLabel = new QLabel(
        QString::fromUtf8("ПКМ по виджету — настройки • Ctrl+Колёсико — масштаб • Правый нижний угол ◢ — растянуть.")
    );
    subtitleLabel->setWordWrap(true);
    subtitleLabel->setStyleSheet("color: #9AA5B8; font-size: 11px;");

    rootLayout->addWidget(titleLabel);
    rootLayout->addWidget(subtitleLabel);

    auto* presetLabel = new QLabel(QString::fromUtf8("РЕЖИМЫ РАБОЧЕГО СТОЛА (1-КЛИК):"));
    presetLabel->setStyleSheet(
        "color: #C4B5FD; font-size: 10px; font-weight: bold; letter-spacing: 1px;"
    );
    rootLayout->addWidget(presetLabel);

    auto* presetLayout = new QHBoxLayout();
    presetLayout->setSpacing(6);

    auto* studyProfileBtn = new QPushButton(CategoryConfigLoader::seedConfig().studyProfileLabel);
    auto* codeProfileBtn = new QPushButton(QString::fromUtf8("💻 Код / ИБ"));
    auto* reflectProfileBtn = new QPushButton(QString::fromUtf8("🌙 Рефлексия"));
    auto* mainDesktopBtn = new QPushButton(QString::fromUtf8("⚡ Основной (8)"));

    presetLayout->addWidget(studyProfileBtn);
    presetLayout->addWidget(codeProfileBtn);
    presetLayout->addWidget(reflectProfileBtn);
    presetLayout->addWidget(mainDesktopBtn);
    rootLayout->addLayout(presetLayout);

    auto* appearLabel = new QLabel(QString::fromUtf8("СТИЛЬ И СТЕКЛО (DEAVE GLASS):"));
    appearLabel->setStyleSheet(
        "color: #A78BFA; font-size: 10px; font-weight: bold; letter-spacing: 1px;"
    );
    rootLayout->addWidget(appearLabel);

    auto* appearLayout = new QHBoxLayout();
    appearLayout->setSpacing(6);
    auto* cyanBtn = new QPushButton(QString::fromUtf8("● Cyan"));
    auto* greenBtn = new QPushButton(QString::fromUtf8("● Green"));
    auto* violetBtn = new QPushButton(QString::fromUtf8("● Violet"));
    QPushButton* glassToggleBtn = new QPushButton(
        Theme::glassMode() == Theme::GlassMode::Glass
            ? QString::fromUtf8("Режим: Glass (Blur)")
            : QString::fromUtf8("Режим: Soft")
    );

    appearLayout->addWidget(cyanBtn);
    appearLayout->addWidget(greenBtn);
    appearLayout->addWidget(violetBtn);
    appearLayout->addWidget(glassToggleBtn);
    rootLayout->addLayout(appearLayout);

    connect(cyanBtn, &QPushButton::clicked, this, [this]() {
        Theme::setAccent(Theme::Accent::Cyan);
        for (QWidget* w : m_activeWidgets) w->update();
    });
    connect(greenBtn, &QPushButton::clicked, this, [this]() {
        Theme::setAccent(Theme::Accent::Green);
        for (QWidget* w : m_activeWidgets) w->update();
    });
    connect(violetBtn, &QPushButton::clicked, this, [this]() {
        Theme::setAccent(Theme::Accent::Violet);
        for (QWidget* w : m_activeWidgets) w->update();
    });
    connect(glassToggleBtn, &QPushButton::clicked, this, [this, glassToggleBtn]() {
        if (Theme::glassMode() == Theme::GlassMode::Glass) {
            Theme::setGlassMode(Theme::GlassMode::Soft);
            glassToggleBtn->setText(QString::fromUtf8("Режим: Soft"));
        } else {
            Theme::setGlassMode(Theme::GlassMode::Glass);
            glassToggleBtn->setText(QString::fromUtf8("Режим: Glass (Blur)"));
        }
        for (QWidget* w : m_activeWidgets) w->update();
    });

    auto* scaleLabel = new QLabel(QString::fromUtf8("МАСШТАБ ВСЕХ ВИДЖЕТОВ ПО РЕЖИМУ ЭКРАНА:"));
    scaleLabel->setStyleSheet(
        "color: #8FD6A8; font-size: 10px; font-weight: bold; letter-spacing: 1px;"
    );
    rootLayout->addWidget(scaleLabel);

    auto* scaleLayout = new QHBoxLayout();
    scaleLayout->setSpacing(6);
    auto* scale80Btn = new QPushButton(QString::fromUtf8("80% Мини"));
    auto* scale90Btn = new QPushButton(QString::fromUtf8("90% Компакт"));
    auto* scale100Btn = new QPushButton(QString::fromUtf8("100% Стандарт"));
    auto* scale115Btn = new QPushButton(QString::fromUtf8("115% Крупный"));

    scaleLayout->addWidget(scale80Btn);
    scaleLayout->addWidget(scale90Btn);
    scaleLayout->addWidget(scale100Btn);
    scaleLayout->addWidget(scale115Btn);
    rootLayout->addLayout(scaleLayout);

    auto* sectionLabel = new QLabel(QString::fromUtf8("АКТИВНЫЕ ВИДЖЕТЫ:"));
    sectionLabel->setStyleSheet(
        "color: #7F899B; font-size: 10px; font-weight: bold; letter-spacing: 1px;"
    );
    rootLayout->addWidget(sectionLabel);

    setupWidgetControls();

    QSettings layoutSettings;
    if (!layoutSettings.value("layout/autoTiledV6", false).toBool()) {
        layoutSettings.setValue("layout/autoTiledV6", true);
        QTimer::singleShot(150, this, &ControlPanel::tileActiveWidgets);
    }

    for (QCheckBox* checkBox : m_widgetCheckBoxes) {
        rootLayout->addWidget(checkBox);
    }

    rootLayout->addStretch();

    auto* tileButton = new QPushButton(
        QString::fromUtf8("📐 Выровнять раскладку на текущем экране")
    );
    tileButton->setStyleSheet(
        "background-color: #132A38; border: 1px solid #C4B5FD; color: #C4B5FD; font-weight: bold;"
    );
    rootLayout->addWidget(tileButton);

    auto* buttonsLayout = new QHBoxLayout();
    buttonsLayout->setSpacing(6);

    auto* hideAllButton = new QPushButton(QString::fromUtf8("Скрыть все"));
    auto* showAllButton = new QPushButton(QString::fromUtf8("Показать"));
    m_gameModeButton = new QPushButton("Game mode: OFF");
    m_gameModeButton->setCheckable(true);

    buttonsLayout->addWidget(hideAllButton);
    buttonsLayout->addWidget(showAllButton);
    buttonsLayout->addWidget(m_gameModeButton);

    rootLayout->addLayout(buttonsLayout);

    connect(studyProfileBtn, &QPushButton::clicked, this, [this]() {
        applyWorkspaceProfile({"pomodoro", "schedule", "calendar", "prayer_times", "goals"});
    });

    connect(codeProfileBtn, &QPushButton::clicked, this, [this]() {
        applyWorkspaceProfile({"pomodoro", "system_monitor", "roadmap", "calendar", "prayer_times", "goals"});
    });

    connect(reflectProfileBtn, &QPushButton::clicked, this, [this]() {
        applyWorkspaceProfile({"prayer_times", "daily_wisdom", "calendar", "goals", "roadmap"});
    });

    connect(mainDesktopBtn, &QPushButton::clicked, this, [this]() {
        applyWorkspaceProfile({
            "pomodoro", "schedule", "prayer_times", "calendar",
            "system_monitor", "daily_wisdom", "goals", "roadmap"
        });
    });

    connect(scale80Btn, &QPushButton::clicked, this, [this]() { applyGlobalScale(0.80); });
    connect(scale90Btn, &QPushButton::clicked, this, [this]() { applyGlobalScale(0.90); });
    connect(scale100Btn, &QPushButton::clicked, this, [this]() { applyGlobalScale(1.00); });
    connect(scale115Btn, &QPushButton::clicked, this, [this]() { applyGlobalScale(1.15); });

    connect(tileButton, &QPushButton::clicked, this, &ControlPanel::tileActiveWidgets);

    connect(hideAllButton, &QPushButton::clicked, this, [this]() {
        hideAllWidgets();
    });

    connect(showAllButton, &QPushButton::clicked, this, [this]() {
        showEnabledWidgets();
    });

    connect(
        m_gameModeButton,
        &QPushButton::toggled,
        this,
        &ControlPanel::setGameMode
    );
}

ControlPanel::~ControlPanel()
{
    for (QWidget* widget : m_activeWidgets) {
        delete widget;
    }
}

void ControlPanel::setupTrayIcon()
{
    m_trayIcon = new QSystemTrayIcon(
        QApplication::style()->standardIcon(QStyle::SP_ComputerIcon),
        this
    );

    m_trayIcon->setToolTip("Deave StudyWidgets");

    auto* trayMenu = new QMenu(this);

    QAction* openPanelAction = trayMenu->addAction(QString::fromUtf8("Открыть Панель управления"));
    QAction* tileAction = trayMenu->addAction(QString::fromUtf8("📐 Выровнять раскладку виджетов"));
    QAction* showWidgetsAction = trayMenu->addAction(QString::fromUtf8("Показать виджеты"));
    QAction* hideWidgetsAction = trayMenu->addAction(QString::fromUtf8("Скрыть виджеты"));

    trayMenu->addSeparator();

    m_gameModeAction = trayMenu->addAction("Game mode");
    m_gameModeAction->setCheckable(true);

    trayMenu->addSeparator();

    QAction* exitAction = trayMenu->addAction(QString::fromUtf8("Выход"));

    m_trayIcon->setContextMenu(trayMenu);

    connect(openPanelAction, &QAction::triggered, this, &ControlPanel::showControlPanel);
    connect(tileAction, &QAction::triggered, this, &ControlPanel::tileActiveWidgets);
    connect(showWidgetsAction, &QAction::triggered, this, &ControlPanel::showEnabledWidgets);
    connect(hideWidgetsAction, &QAction::triggered, this, [this]() { hideAllWidgets(); });
    connect(m_gameModeAction, &QAction::toggled, this, &ControlPanel::setGameMode);
    connect(exitAction, &QAction::triggered, this, &ControlPanel::exitApplication);

    connect(
        m_trayIcon,
        &QSystemTrayIcon::activated,
        this,
        [this](QSystemTrayIcon::ActivationReason reason) {
            if (reason == QSystemTrayIcon::Trigger) {
                showControlPanel();
            }
        }
    );

    m_trayIcon->show();
}

void ControlPanel::setupWidgetControls()
{
    QSettings settings;

    // Disable spaced_rep by default as requested by the user
    if (!settings.contains("widgets/spaced_rep_hidden_v4")) {
        settings.setValue("widgets/spaced_rep_hidden_v4", true);
        settings.setValue("widgets/spaced_rep/enabled", false);
        settings.setValue("widgets/welcome/enabled", false);
    }

    for (const WidgetInfo& widgetInfo : widgetRegistry()) {
        auto* checkBox = new QCheckBox(widgetInfo.displayName);

        const bool defaultEnabled = (widgetInfo.id != "welcome" && widgetInfo.id != "spaced_rep");
        const bool enabled = settings.value(
            "widgets/" + widgetInfo.id + "/enabled",
            defaultEnabled
        ).toBool();

        checkBox->setChecked(enabled);
        m_widgetCheckBoxes.insert(widgetInfo.id, checkBox);

        connect(
            checkBox,
            &QCheckBox::toggled,
            this,
            [this, id = widgetInfo.id](bool checked) {
                setWidgetEnabled(id, checked);
            }
        );

        if (enabled) {
            createAndShowWidget(widgetInfo.id);
        }
    }
}

void ControlPanel::applyWorkspaceProfile(const QStringList& enabledWidgetIds)
{
    for (auto it = m_widgetCheckBoxes.cbegin(); it != m_widgetCheckBoxes.cend(); ++it) {
        const bool shouldEnable = enabledWidgetIds.contains(it.key());
        it.value()->setChecked(shouldEnable);
    }
}

void ControlPanel::applyGlobalScale(double scaleFactor)
{
    for (QWidget* w : m_activeWidgets) {
        if (auto* base = dynamic_cast<WidgetBase*>(w)) {
            base->setScaleFactor(scaleFactor);
        }
    }
}

void ControlPanel::tileActiveWidgets()
{
    QScreen* targetScreen = this->screen();
    if (!targetScreen) {
        targetScreen = QGuiApplication::primaryScreen();
    }
    if (!targetScreen) {
        return;
    }

    const QRect area = targetScreen->availableGeometry();
    const int margin = 10;
    const int gap = 10;

    auto placeWidget = [&](const QString& id, int x, int y) {
        if (!m_activeWidgets.contains(id)) {
            return;
        }
        QWidget* w = m_activeWidgets.value(id);
        if (!w || !w->isVisible()) {
            return;
        }
        w->move(x, y);
        QSettings settings;
        settings.setValue("widgets/" + id + "/position", w->pos());
    };

    // Harmonious perimeter + middle-column layout fitting all 8 widgets on screen:
    // Top-Left: Schedule
    placeWidget("schedule", area.left() + margin, area.top() + margin);

    // Right column: Pomodoro (top-right) and Daily Wisdom (bottom-right)
    int rightColLeftEdge = area.right() - margin;
    if (m_activeWidgets.contains("pomodoro") && m_activeWidgets["pomodoro"]->isVisible()) {
        QWidget* pom = m_activeWidgets["pomodoro"];
        placeWidget("pomodoro", area.right() - margin - pom->width(), area.top() + margin);
        rightColLeftEdge = qMin(rightColLeftEdge, area.right() - margin - pom->width());
    }
    if (m_activeWidgets.contains("daily_wisdom") && m_activeWidgets["daily_wisdom"]->isVisible()) {
        QWidget* dw = m_activeWidgets["daily_wisdom"];
        placeWidget("daily_wisdom", area.right() - margin - dw->width(), area.bottom() - margin - dw->height());
        rightColLeftEdge = qMin(rightColLeftEdge, area.right() - margin - dw->width());
    }

    // Middle-Right column: Prayer Times (top) -> Calendar (middle) -> System Monitor (bottom)
    const int midColRight = rightColLeftEdge - gap;
    int midColTopY = area.top() + margin;
    if (m_activeWidgets.contains("prayer_times") && m_activeWidgets["prayer_times"]->isVisible()) {
        QWidget* pr = m_activeWidgets["prayer_times"];
        placeWidget("prayer_times", midColRight - pr->width(), midColTopY);
        midColTopY += pr->height() + (gap - 2);
    }

    int midColBottomY = area.bottom() - margin;
    if (m_activeWidgets.contains("system_monitor") && m_activeWidgets["system_monitor"]->isVisible()) {
        QWidget* sm = m_activeWidgets["system_monitor"];
        midColBottomY -= sm->height();
        placeWidget("system_monitor", midColRight - sm->width(), midColBottomY);
        midColBottomY -= (gap - 2);
    }

    if (m_activeWidgets.contains("calendar") && m_activeWidgets["calendar"]->isVisible()) {
        QWidget* cal = m_activeWidgets["calendar"];
        int calY = midColTopY;
        if (midColBottomY > midColTopY + cal->height()) {
            calY = midColTopY + (midColBottomY - midColTopY - cal->height()) / 2;
        }
        placeWidget("calendar", midColRight - cal->width(), calY);
    }

    // Bottom-Left & Bottom-Middle-Left: Goals -> Roadmap
    int bottomLeftX = area.left() + margin;
    if (m_activeWidgets.contains("goals") && m_activeWidgets["goals"]->isVisible()) {
        QWidget* g = m_activeWidgets["goals"];
        placeWidget("goals", bottomLeftX, area.bottom() - margin - g->height());
        bottomLeftX += g->width() + gap;
    }
    if (m_activeWidgets.contains("roadmap") && m_activeWidgets["roadmap"]->isVisible()) {
        QWidget* r = m_activeWidgets["roadmap"];
        placeWidget("roadmap", bottomLeftX, area.bottom() - margin - r->height());
    }
}

void ControlPanel::createAndShowWidget(const QString& id)
{
    if (m_activeWidgets.contains(id)) {
        QWidget* widget = m_activeWidgets.value(id);
        if (!m_gameModeEnabled) {
            widget->showNormal();
        }
        return;
    }

    QWidget* widget = createWidgetById(id);
    if (widget == nullptr) {
        showTrayMessage("StudyWidgets", "Could not create widget: " + id);
        return;
    }

    if (auto* baseWidget = dynamic_cast<WidgetBase*>(widget)) {
        baseWidget->setHideHandler([this, id]() {
            showTrayMessage(
                "StudyWidgets",
                id + QString::fromUtf8(" скрыт. Используйте иконку в трее для возврата.")
            );
        });

        baseWidget->setExitHandler([this]() {
            exitApplication();
        });
    }

    m_activeWidgets.insert(id, widget);

    if (!m_gameModeEnabled) {
        widget->show();
    }
}

void ControlPanel::setWidgetEnabled(const QString& id, bool enabled)
{
    QSettings settings;
    settings.setValue("widgets/" + id + "/enabled", enabled);

    if (enabled) {
        createAndShowWidget(id);
        return;
    }

    if (m_activeWidgets.contains(id)) {
        m_activeWidgets.value(id)->hide();
    }
}

void ControlPanel::showEnabledWidgets()
{
    if (m_gameModeEnabled) {
        m_gameModeEnabled = false;
        updateGameModeUi();
    }

    for (auto it = m_widgetCheckBoxes.cbegin();
         it != m_widgetCheckBoxes.cend();
         ++it) {
        if (it.value()->isChecked()) {
            createAndShowWidget(it.key());
        }
    }
}

void ControlPanel::hideAllWidgets(bool showNotification)
{
    for (QWidget* widget : m_activeWidgets) {
        widget->hide();
    }

    if (showNotification) {
        showTrayMessage(
            "StudyWidgets",
            QString::fromUtf8("Все виджеты скрыты. Нажмите на иконку в трее, чтобы вернуть их.")
        );
    }
}

void ControlPanel::setGameMode(bool enabled)
{
    if (m_gameModeEnabled == enabled) {
        updateGameModeUi();
        return;
    }

    m_gameModeEnabled = enabled;
    updateGameModeUi();

    if (m_gameModeEnabled) {
        hideAllWidgets(false);
        showTrayMessage(
            "Game mode enabled",
            QString::fromUtf8("Виджеты скрыты на время игры.")
        );
        return;
    }

    showEnabledWidgets();
    showTrayMessage(
        "Game mode disabled",
        QString::fromUtf8("Виджеты восстановлены на рабочий стол.")
    );
}

void ControlPanel::updateGameModeUi()
{
    if (m_gameModeButton != nullptr) {
        const QSignalBlocker buttonBlocker(m_gameModeButton);
        m_gameModeButton->setChecked(m_gameModeEnabled);
        m_gameModeButton->setText(
            m_gameModeEnabled ? "Game mode: ON" : "Game mode: OFF"
        );
    }

    if (m_gameModeAction != nullptr) {
        const QSignalBlocker actionBlocker(m_gameModeAction);
        m_gameModeAction->setChecked(m_gameModeEnabled);
    }
}

void ControlPanel::showControlPanel()
{
    showNormal();
    raise();
    activateWindow();
}

void ControlPanel::showTrayMessage(
    const QString& title,
    const QString& message
)
{
    if (m_trayIcon == nullptr) {
        return;
    }

    m_trayIcon->showMessage(
        title,
        message,
        QSystemTrayIcon::Information,
        3000
    );
}

void ControlPanel::exitApplication()
{
    if (m_isExiting) {
        return;
    }

    m_isExiting = true;

    if (m_trayIcon != nullptr) {
        m_trayIcon->hide();
    }

    for (QWidget* widget : m_activeWidgets) {
        if (widget == nullptr) {
            continue;
        }
        widget->hide();
        delete widget;
    }

    m_activeWidgets.clear();
    hide();
    QApplication::exit(0);
}

void ControlPanel::closeEvent(QCloseEvent* event)
{
    if (m_isExiting) {
        event->accept();
        return;
    }

    hide();
    showTrayMessage(
        "Deave StudyWidgets",
        QString::fromUtf8("Виджеты продолжают работать на рабочем столе (иконка в трее).")
    );
    event->ignore();
}
