#pragma once

#include "../WidgetBase.h"

#include <QMap>
#include <QString>

class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QVBoxLayout;

class GoalsWidget : public WidgetBase
{
public:
    explicit GoalsWidget(QWidget* parent = nullptr);

protected:
    void buildContextMenu(QMenu* menu) override;

private:
    void switchHorizon(const QString& horizon);
    void addGoalFromInput();
    void rebuildGoalsList();

    QString m_currentHorizon = "day";
    QMap<QString, QPushButton*> m_tabButtons;

    QLabel* m_summaryLabel = nullptr;
    QLineEdit* m_titleInput = nullptr;
    QSpinBox* m_targetSpin = nullptr;

    QWidget* m_listContainer = nullptr;
    QVBoxLayout* m_listLayout = nullptr;
};
