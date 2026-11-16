#include "ScheduleWidget.h"

#include "../CategoryConfigLoader.h"
#include "../WidgetRegistry.h"

#include <QDate>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <QSettings>
#include <QStandardPaths>
#include <cmath>

namespace {

class ScheduleCanvasWidget : public QWidget
{
public:
    explicit ScheduleCanvasWidget(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        setMinimumSize(180, 90);
    }

    void setPixmap(const QPixmap& pixmap)
    {
        m_pixmap = pixmap;
        update();
    }

    void setBadgeInfo(const QString& text, bool isEven, bool visible)
    {
        m_badgeText = text;
        m_isEven = isEven;
        m_showBadge = visible;
        update();
    }

    void setDimOverlay(bool dim)
    {
        m_dim = dim;
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

        const QRect box = rect();

        if (m_pixmap.isNull()) {
            painter.setPen(QColor(154, 166, 186));
            painter.setFont(Theme::uiFont(12));
            painter.drawText(box.adjusted(10, 10, -10, -10), Qt::AlignCenter | Qt::TextWordWrap,
                             QString::fromUtf8("ПКМ → «Выбрать картинку расписания...»"));
        } else {
            QSize scaledSize = m_pixmap.size();
            scaledSize.scale(box.size(), Qt::KeepAspectRatio);

            const QRect target(box.left() + (box.width() - scaledSize.width()) / 2,
                               box.top() + (box.height() - scaledSize.height()) / 2,
                               scaledSize.width(), scaledSize.height());

            QPainterPath clip;
            clip.addRoundedRect(target, 10, 10);
            painter.save();
            painter.setClipPath(clip);
            painter.drawPixmap(target, m_pixmap);
            if (m_dim) {
                painter.fillRect(target, QColor(14, 16, 32, 48));
            }
            painter.restore();
        }

        if (m_showBadge && !m_badgeText.isEmpty()) {
            QFont f = Theme::monoFont(10, QFont::Bold);
            painter.setFont(f);
            const QFontMetrics fm(f);
            const int tw = fm.horizontalAdvance(m_badgeText) + 16;
            const int th = 20;
            const QRectF pillRect(box.right() - tw - 8, box.bottom() - th - 5, tw, th);

            painter.setPen(QPen(m_isEven ? Theme::accentColor() : QColor("#8FD6A8"), 1.2));
            painter.setBrush(QColor(14, 16, 32, 230));
            painter.drawRoundedRect(pillRect, 6.0, 6.0);

            painter.setPen(m_isEven ? Theme::accentColor() : QColor("#8FD6A8"));
            painter.drawText(pillRect, Qt::AlignCenter, m_badgeText);
        }
    }

private:
    QPixmap m_pixmap;
    QString m_badgeText;
    bool m_isEven = true;
    bool m_showBadge = true;
    bool m_dim = false;
};

} // namespace

ScheduleWidget::ScheduleWidget(QWidget* parent)
    : WidgetBase("schedule", parent)
{
    const LocalSeedConfig& seed = CategoryConfigLoader::seedConfig();
    setBaseWidgetSize(600, 300);
    setChromeTitle(seed.scheduleTitle, seed.scheduleSubtitle);

    setRootMargins(7, 7, 7, 7);
    bodyLayout()->setSpacing(0);

    m_canvasWidget = new ScheduleCanvasWidget(this);
    bodyLayout()->addWidget(m_canvasWidget, 1);

    QSettings settings;
    m_showBadge = settings.value("schedule/showBadge", true).toBool();
    m_dimImage = settings.value("schedule/dimImage", false).toBool();
    static_cast<ScheduleCanvasWidget*>(m_canvasWidget)->setDimOverlay(m_dimImage);

    updateWeekParityBadge();

    QString savedPath = settings.value("schedule/imagePath").toString();
    if (savedPath.isEmpty() || !QFileInfo::exists(savedPath))
    {
        const QString configured = seed.scheduleImagePath.trimmed();
        if (!configured.isEmpty() && QFileInfo::exists(configured))
        {
            savedPath = configured;
        }
        else
        {
            const QString appDataPng = QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation))
                                           .filePath(QStringLiteral("config/schedule.png"));
            if (QFileInfo::exists(appDataPng))
            {
                savedPath = appDataPng;
            }
        }
    }
    loadImage(savedPath);
}

void ScheduleWidget::buildContextMenu(QMenu* menu)
{
    QAction* chooseAct = menu->addAction(QString::fromUtf8("🖼 Выбрать картинку расписания..."));
    connect(chooseAct, &QAction::triggered, this, &ScheduleWidget::chooseImage);

    const LocalSeedConfig& seed = CategoryConfigLoader::seedConfig();
    QAction* badgeAct = menu->addAction(
        QString::fromUtf8("Показывать плашку недели (%1 / %2)").arg(seed.weekEvenLabel, seed.weekOddLabel)
    );
    badgeAct->setCheckable(true);
    badgeAct->setChecked(m_showBadge);
    connect(badgeAct, &QAction::triggered, this, [this](bool checked) {
        m_showBadge = checked;
        QSettings settings;
        settings.setValue("schedule/showBadge", checked);
        updateWeekParityBadge();
    });

    QAction* dimAct = menu->addAction(QString::fromUtf8("Мягкое приглушение белого фона (ночной режим)"));
    dimAct->setCheckable(true);
    dimAct->setChecked(m_dimImage);
    connect(dimAct, &QAction::triggered, this, [this](bool checked) {
        m_dimImage = checked;
        QSettings settings;
        settings.setValue("schedule/dimImage", checked);
        static_cast<ScheduleCanvasWidget*>(m_canvasWidget)->setDimOverlay(checked);
    });
}

void ScheduleWidget::updateWeekParityBadge()
{
    const QDate today = QDate::currentDate();
    int yearNumber = 0;
    const int isoWeek = today.weekNumber(&yearNumber);
    const bool isEven = (isoWeek % 2 == 0);

    const LocalSeedConfig& seed = CategoryConfigLoader::seedConfig();
    const int semWeek = qBound(1, isoWeek >= 38 ? (isoWeek - 37) : (isoWeek - 6), 14);
    const QString text = QString("%1 · %2/13 · %3")
                             .arg(isEven ? seed.weekEvenLabel : seed.weekOddLabel)
                             .arg(semWeek)
                             .arg(today.toString("dd.MM"));

    static_cast<ScheduleCanvasWidget*>(m_canvasWidget)->setBadgeInfo(text, isEven, m_showBadge);
    setChromeTitle(seed.scheduleHeaderShort, text);
}

void ScheduleWidget::fitWindowToImageAspect()
{
    if (m_sourcePixmap.isNull() || m_sourcePixmap.width() <= 0) {
        return;
    }

    QSettings settings;
    settings.remove("widgets/schedule/customSize");

    const int imageWidth = 590;
    const double ratio = static_cast<double>(m_sourcePixmap.height()) / m_sourcePixmap.width();
    const int imageHeight = static_cast<int>(std::round(imageWidth * ratio));

    // Tight 7px margins on all sides, no header bar
    setBaseWidgetSize(imageWidth + 14, imageHeight + 14);
}

void ScheduleWidget::loadImage(const QString& path)
{
    m_imagePath = path;
    if (!path.isEmpty() && QFileInfo::exists(path)) {
        m_sourcePixmap = QPixmap(path);
        QSettings settings;
        settings.setValue("schedule/imagePath", path);
    } else {
        m_sourcePixmap = QPixmap();
    }

    static_cast<ScheduleCanvasWidget*>(m_canvasWidget)->setPixmap(m_sourcePixmap);
    fitWindowToImageAspect();
}

void ScheduleWidget::chooseImage()
{
    const LocalSeedConfig& seed = CategoryConfigLoader::seedConfig();
    QString startDir = m_imagePath;
    if (startDir.isEmpty() || !QFileInfo::exists(startDir))
    {
        startDir = !seed.scheduleBrowseDir.isEmpty() && QFileInfo::exists(seed.scheduleBrowseDir)
            ? seed.scheduleBrowseDir
            : QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    }

    const QString file = QFileDialog::getOpenFileName(
        this,
        QString::fromUtf8("Картинка расписания"),
        startDir,
        "Images (*.png *.jpg *.jpeg *.bmp *.webp)"
    );
    if (!file.isEmpty())
    {
        loadImage(file);
    }
}

REGISTER_WIDGET("schedule", "Расписание и неделя", ScheduleWidget)
