#include "ControlPanel.h"

#include "WidgetBase.h"
#include "WidgetRegistry.h"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QCloseEvent>
#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QPushButton>
#include <QSettings>
#include <QStyle>
#include <QSystemTrayIcon>
#include <QVBoxLayout>
#include <QSignalBlocker>

ControlPanel::ControlPanel(QWidget* parent)
    : QWidget(parent) {
    setWindowTitle("StudyWidgets Control Panel");
    setMinimumWidth(400);
    resize(440, 390);

    setStyleSheet(R"(
        QWidget {
            background-color: #1c1e24;
            color: #e7eaf0;
            font-family: "Segoe UI";
        }

        QCheckBox {
            background-color: #262932;
            border: 1px solid #353945;
            border-radius: 8px;
            padding: 10px;
            spacing: 10px;
        }

        QCheckBox:hover {
            background-color: #2d303a;
            border-color: #4d72b8;
        }

        QPushButton {
            background-color: #313641;
            border: none;
            border-radius: 8px;
            color: #e7eaf0;
            padding: 9px 12px;
        }

        QPushButton:hover {
            background-color: #414855;
        }

        QPushButton:pressed {
            background-color: #252a33;
        }

        QPushButton:checked {
            background-color: #315f9c;
            border: 1px solid #5184c9;
        }

        QPushButton:checked:hover {
            background-color: #3b70b3;
        }
    )");

    setupTrayIcon();

    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(22, 20, 22, 20);
    rootLayout->setSpacing(12);

    auto* titleLabel = new QLabel("StudyWidgets");
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(18);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);

    auto* subtitleLabel = new QLabel(
        "Enable desktop widgets and manage focus modes."
    );
    subtitleLabel->setStyleSheet(
        "color: #9aa3b2;"
        "font-size: 12px;"
    );

    auto* sectionLabel = new QLabel("AVAILABLE WIDGETS");
    sectionLabel->setStyleSheet(
        "color: #7f899b;"
        "font-size: 10px;"
        "font-weight: bold;"
        "letter-spacing: 1px;"
    );

    rootLayout->addWidget(titleLabel);
    rootLayout->addWidget(subtitleLabel);
    rootLayout->addSpacing(8);
    rootLayout->addWidget(sectionLabel);

    setupWidgetControls();

    for (QCheckBox* checkBox : m_widgetCheckBoxes) 
    {
        rootLayout->addWidget(checkBox);
    }

    rootLayout->addStretch();

    auto* buttonsLayout = new QHBoxLayout();

    auto* hideAllButton = new QPushButton("Hide widgets");
    auto* showAllButton = new QPushButton("Show widgets");
    m_gameModeButton = new QPushButton("Game mode: OFF");
    m_gameModeButton->setCheckable(true);

    buttonsLayout->addWidget(hideAllButton);
    buttonsLayout->addWidget(showAllButton);
    buttonsLayout->addWidget(m_gameModeButton);

    rootLayout->addLayout(buttonsLayout);

    connect(
        hideAllButton,
        &QPushButton::clicked,
        this,
        [this]() {
            hideAllWidgets();
        }
    );

    connect(
        showAllButton,
        &QPushButton::clicked,
        this,
        [this]() {
            showEnabledWidgets();
        }
    );

    connect(
        m_gameModeButton,
        &QPushButton::toggled,
        this,
        &ControlPanel::setGameMode
    );
}

ControlPanel::~ControlPanel() 
{
    for (QWidget* widget : m_activeWidgets) 
    {
        delete widget;
    }
}

void ControlPanel::setupTrayIcon() 
{
    m_trayIcon = new QSystemTrayIcon(
        QApplication::style()->standardIcon(QStyle::SP_ComputerIcon),
        this
    );

    m_trayIcon->setToolTip("StudyWidgets");

    auto* trayMenu = new QMenu(this);

    QAction* openPanelAction = trayMenu->addAction("Open Control Panel");
    QAction* showWidgetsAction = trayMenu->addAction("Show widgets");
    QAction* hideWidgetsAction = trayMenu->addAction("Hide widgets");

    trayMenu->addSeparator();

    m_gameModeAction = trayMenu->addAction("Game mode");
    m_gameModeAction->setCheckable(true);

    trayMenu->addSeparator();

    QAction* exitAction = trayMenu->addAction("Exit StudyWidgets");

    m_trayIcon->setContextMenu(trayMenu);

    connect(
        openPanelAction,
        &QAction::triggered,
        this,
        &ControlPanel::showControlPanel
    );

    connect(
        showWidgetsAction,
        &QAction::triggered,
        this,
        &ControlPanel::showEnabledWidgets
    );

    connect(
        hideWidgetsAction,
        &QAction::triggered,
        this,
        [this]() {
            hideAllWidgets();
        }
    );

    connect(
        m_gameModeAction,
        &QAction::toggled,
        this,
        &ControlPanel::setGameMode
    );

    connect(
        exitAction,
        &QAction::triggered,
        []() {
            QApplication::quit();
        }
    );

    connect(
        m_trayIcon,
        &QSystemTrayIcon::activated,
        this,
        [this](QSystemTrayIcon::ActivationReason reason) 
        {
            if (reason == QSystemTrayIcon::Trigger) 
            {
                showControlPanel();
            }
        }
    );

    m_trayIcon->show();
}

void ControlPanel::setupWidgetControls() 
{
    QSettings settings;

    for (const WidgetInfo& widgetInfo : widgetRegistry()) 
    {
        auto* checkBox = new QCheckBox(widgetInfo.displayName);

        const bool enabled = settings.value(
            "widgets/" + widgetInfo.id + "/enabled",
            widgetInfo.id == "welcome"
        ).toBool();

        checkBox->setChecked(enabled);

        m_widgetCheckBoxes.insert(widgetInfo.id, checkBox);

        connect(
            checkBox,
            &QCheckBox::toggled,
            this,
            [this, id = widgetInfo.id](bool checked) 
            {
                setWidgetEnabled(id, checked);
            }
        );

        if (enabled) 
        {
            createAndShowWidget(widgetInfo.id);
        }
    }
}

void ControlPanel::createAndShowWidget(const QString& id) 
{
    if (m_activeWidgets.contains(id)) 
    {
        QWidget* widget = m_activeWidgets.value(id);

        if (!m_gameModeEnabled) 
        {
            widget->showNormal();
            widget->raise();
            widget->activateWindow();
        }

        return;
    }

    QWidget* widget = createWidgetById(id);

    if (widget == nullptr) 
    {
        showTrayMessage(
            "StudyWidgets",
            "Could not create widget: " + id
        );
        return;
    }

    if (auto* baseWidget = dynamic_cast<WidgetBase*>(widget)) 
    {
        baseWidget->setHideHandler(
            [this, id]() 
            {
                showTrayMessage(
                    "StudyWidgets",
                    id + " was hidden. Use the tray icon to restore it."
                );
            }
        );
    }

    m_activeWidgets.insert(id, widget);

    if (!m_gameModeEnabled) {
        widget->show();
    }
}

void ControlPanel::setWidgetEnabled(
    const QString& id,
    bool enabled
) 
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
            "All widgets are hidden. Use the tray icon to restore them."
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
            "Widgets are hidden. StudyWidgets is still running in the tray."
        );

        return;
    }

    showEnabledWidgets();

    showTrayMessage(
        "Game mode disabled",
        "Enabled widgets were restored."
    );
}

void ControlPanel::updateGameModeUi() 
{
    if (m_gameModeButton != nullptr) {
        const QSignalBlocker buttonBlocker(m_gameModeButton);

        m_gameModeButton->setChecked(m_gameModeEnabled);
        m_gameModeButton->setText(
            m_gameModeEnabled
                ? "Game mode: ON"
                : "Game mode: OFF"
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
        3500
    );
}

void ControlPanel::closeEvent(QCloseEvent* event) 
{
    hide();

    showTrayMessage(
        "StudyWidgets",
        "StudyWidgets is still running in the system tray."
    );

    event->ignore();
}