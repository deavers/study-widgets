#pragma once

#include "../WidgetBase.h"

class QComboBox;
class QLabel;
class QLineEdit;
class QTimer;
class QVBoxLayout;

class SpacedRepetitionWidget : public WidgetBase
{
public:
    explicit SpacedRepetitionWidget(QWidget* parent = nullptr);

private:
    void addReviewFromInput();
    void rebuildReviewList();

    QComboBox* m_categoryCombo = nullptr;
    QComboBox* m_typeCombo = nullptr;
    QLineEdit* m_topicInput = nullptr;
    QLabel* m_summaryLabel = nullptr;
    QWidget* m_listContainer = nullptr;
    QVBoxLayout* m_listLayout = nullptr;
    QTimer* m_timer = nullptr;
};
