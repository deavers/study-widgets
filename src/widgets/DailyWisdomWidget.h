#pragma once

#include "../WidgetBase.h"

#include <QList>
#include <QString>

class QLabel;
class QPushButton;

class DailyWisdomWidget : public WidgetBase
{
public:
    struct WisdomEntry
    {
        QString arabic;
        QString translation;
        QString source;
    };

    explicit DailyWisdomWidget(QWidget* parent = nullptr);

    int entryCount() const { return m_entries.size(); }
    void showEntry(int index);
    void setOperatorState(int stateIndex, bool persist = true);

protected:
    void buildContextMenu(QMenu* menu) override;

private:
    void loadWisdomEntries();
    void nextEntry();
    void copyCurrentEntry();

    QList<WisdomEntry> m_entries;
    int m_currentIndex = 0;
    int m_stateIndex = 0;
    QList<QPushButton*> m_stateButtons;

    QLabel* m_arabicLabel = nullptr;
    QLabel* m_translationLabel = nullptr;
    QLabel* m_sourceLabel = nullptr;
    QLabel* m_stateHintLabel = nullptr;
};
