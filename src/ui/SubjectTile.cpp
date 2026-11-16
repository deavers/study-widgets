#include "SubjectTile.h"

#include "../Theme.h"

#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>

SubjectTile::SubjectTile(const StudyCategory& category, QWidget* parent)
    : QWidget(parent)
    , m_category(category)
{
    setCursor(Qt::PointingHandCursor);
    setAttribute(Qt::WA_Hover, true);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
}

void SubjectTile::setSelected(bool sel)
{
    if (m_selected != sel) {
        m_selected = sel;
        update();
    }
}

void SubjectTile::setStudiedSeconds(int seconds)
{
    if (m_studiedSeconds != seconds) {
        m_studiedSeconds = seconds;
        update();
    }
}

void SubjectTile::setHotKeyIndex(int idx)
{
    m_hotkeyIndex = idx;
    update();
}

QSize SubjectTile::sizeHint() const
{
    return QSize(96, 40);
}

void SubjectTile::enterEvent(QEnterEvent* event)
{
    m_hovered = true;
    update();
    QWidget::enterEvent(event);
}

void SubjectTile::leaveEvent(QEvent* event)
{
    m_hovered = false;
    update();
    QWidget::leaveEvent(event);
}

void SubjectTile::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        emit tileClicked(m_category.id);
    }
    QWidget::mousePressEvent(event);
}

void SubjectTile::paintEvent(QPaintEvent* /*event*/)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QRectF r = rect().adjusted(0.5, 0.5, -0.5, -0.5);
    const qreal radius = 8.0;

    QPainterPath path;
    path.addRoundedRect(r, radius, radius);

    // 1. Background & Border
    QColor bg = Theme::surface1();
    QColor border = Theme::stroke();

    if (m_selected) {
        bg = Theme::accentTint(40);
        border = Theme::accentColor();
    } else if (m_hovered) {
        bg = Theme::surface2();
        border = Theme::strokeStrong();
    }

    painter.fillPath(path, bg);

    QPen pen(border, m_selected ? 1.5 : 1.0);
    painter.setPen(pen);
    painter.drawPath(path);

    // 2. Category Color Accent Dot / Stripe
    QColor catColor(m_category.color.isEmpty() ? "#C4B5FD" : m_category.color);
    painter.setPen(Qt::NoPen);
    painter.setBrush(catColor);
    painter.drawRoundedRect(QRectF(r.left() + 6.0, r.top() + 7.0, 3.5, 3.5), 1.75, 1.75);

    // 3. Hotkey hint (1..9)
    if (m_hotkeyIndex >= 1 && m_hotkeyIndex <= 9) {
        painter.setFont(Theme::fontMono(8, QFont::Normal));
        painter.setPen(Theme::textFaint());
        painter.drawText(QRectF(r.right() - 14.0, r.top() + 3.0, 10.0, 10.0), Qt::AlignCenter, QString::number(m_hotkeyIndex));
    }

    // 4. Code / Title
    QString title = m_category.shortName.isEmpty() ? m_category.name : m_category.shortName;
    if (title.length() > 10) {
        title = title.left(9) + QStringLiteral("…");
    }

    painter.setFont(Theme::fontUi(10, QFont::DemiBold));
    painter.setPen(m_selected ? Theme::textColor() : (m_hovered ? Theme::textColor() : Theme::textDim()));
    painter.drawText(QRectF(r.left() + 14.0, r.top() + 3.0, r.width() - 28.0, 16.0), Qt::AlignLeft | Qt::AlignVCenter, title);

    // 5. Studied Time Subtitle
    const int totalMinutes = m_studiedSeconds / 60;
    const int hours = totalMinutes / 60;
    const int mins = totalMinutes % 60;

    QString timeText;
    if (m_category.tier == "K" && m_category.courseLimit > 0) {
        const int limitHours = m_category.courseLimit / 60;
        timeText = QStringLiteral("%1/%2ч").arg(hours).arg(limitHours);
        if (m_category.done) {
            timeText += QStringLiteral(" ✓");
        }
    } else {
        if (hours > 0) {
            timeText = QStringLiteral("%1ч%2").arg(hours).arg(mins, 2, 10, QChar('0'));
        } else {
            timeText = QStringLiteral("%1м").arg(mins);
        }
    }

    painter.setFont(Theme::fontMono(9, QFont::Normal));
    painter.setPen(m_selected ? Theme::accentColor() : Theme::textDim());
    painter.drawText(QRectF(r.left() + 6.0, r.top() + 19.0, r.width() - 12.0, 14.0), Qt::AlignLeft | Qt::AlignVCenter, timeText);

    // 6. Progress Bar Pill at bottom
    const int target = m_category.targetMinutes > 0 ? m_category.targetMinutes : 1500;
    const qreal progress = qBound(0.0, static_cast<qreal>(totalMinutes) / target, 1.0);
    const qreal barWidth = (r.width() - 12.0) * progress;

    if (barWidth > 1.0) {
        painter.setBrush(m_selected ? Theme::accentColor() : catColor);
        painter.setPen(Qt::NoPen);
        painter.drawRoundedRect(QRectF(r.left() + 6.0, r.bottom() - 3.5, barWidth, 2.0), 1.0, 1.0);
    }
}
