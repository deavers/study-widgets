#pragma once

#include <QMap>
#include <QWidget>

class QAction;
class QCheckBox;
class QCloseEvent;
class QPushButton;
class QSystemTrayIcon;

class ControlPanel : public QWidget 
{
public:
    explicit ControlPanel(QWidget* parent = nullptr);
    ~ControlPanel() override;

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    void setupTrayIcon();
    void setupWidgetControls();

    void createAndShowWidget(const QString& id);
    void setWidgetEnabled(const QString& id, bool enabled);

    void showEnabledWidgets();
    void hideAllWidgets(bool showNotification = true);

    void setGameMode(bool enabled);
    void updateGameModeUi();

    void showControlPanel();
    void showTrayMessage(
        const QString& title,
        const QString& message
    );

    QMap<QString, QWidget*> m_activeWidgets;
    QMap<QString, QCheckBox*> m_widgetCheckBoxes;

    QSystemTrayIcon* m_trayIcon = nullptr;
    QAction* m_gameModeAction = nullptr;
    QPushButton* m_gameModeButton = nullptr;

    bool m_gameModeEnabled = false;
};