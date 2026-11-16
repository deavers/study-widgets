#pragma once

#include "../WidgetBase.h"

#include <QPixmap>

class ScheduleWidget : public WidgetBase
{
public:
    explicit ScheduleWidget(QWidget* parent = nullptr);

protected:
    void buildContextMenu(QMenu* menu) override;

private:
    void loadImage(const QString& path);
    void fitWindowToImageAspect();
    void chooseImage();
    void updateWeekParityBadge();

    QWidget* m_canvasWidget = nullptr;

    QPixmap m_sourcePixmap;
    QString m_imagePath;
    bool m_showBadge = true;
    bool m_dimImage = false;
};
