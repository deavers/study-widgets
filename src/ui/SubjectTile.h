#pragma once

#include "../DataStore.h"

#include <QWidget>

class SubjectTile : public QWidget
{
    Q_OBJECT

public:
    explicit SubjectTile(const StudyCategory& category, QWidget* parent = nullptr);

    QString categoryId() const { return m_category.id; }
    StudyCategory category() const { return m_category; }

    bool isSelected() const { return m_selected; }
    void setSelected(bool sel);

    void setStudiedSeconds(int seconds);
    void setHotKeyIndex(int idx); // 1..9 or 0

signals:
    void tileClicked(const QString& categoryId);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
    QSize sizeHint() const override;

private:
    StudyCategory m_category;
    int m_studiedSeconds = 0;
    int m_hotkeyIndex = 0;
    bool m_selected = false;
    bool m_hovered = false;
};
