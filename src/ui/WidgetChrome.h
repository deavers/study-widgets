#pragma once

#include "../Theme.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QWidget>

#include <functional>

// macOS-style traffic lights: close (hide to tray), minimize (compact), zoom (size preset).
class TrafficLights : public QWidget
{
public:
    std::function<void()> onClose;
    std::function<void()> onCompact;
    std::function<void()> onZoom;

    explicit TrafficLights(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setFixedSize(56, 16);
        setMouseTracking(true);
        setCursor(Qt::PointingHandCursor);
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);

        static const QColor colors[3] = {QColor(0xFF, 0x5F, 0x57), QColor(0xFE, 0xBC, 0x2E), QColor(0x28, 0xC8, 0x40)};
        static const char* glyphs[3] = {"\xC3\x97", "\xE2\x88\x92", "+"};

        for (int i = 0; i < 3; ++i) {
            const QRectF dot(i * 20.0 + 1, 1, 14, 14);
            QColor c = colors[i];
            if (!m_hover) {
                c.setAlpha(m_active ? 235 : 150);
            }
            p.setPen(Qt::NoPen);
            p.setBrush(c);
            p.drawEllipse(dot);

            if (m_hover) {
                p.setPen(QColor(0, 0, 0, 160));
                QFont f = Theme::uiFont(11, QFont::Bold);
                p.setFont(f);
                p.drawText(dot, Qt::AlignCenter, QString::fromUtf8(glyphs[i]));
            }
        }
    }

    void enterEvent(QEnterEvent*) override
    {
        m_hover = true;
        update();
    }

    void leaveEvent(QEvent*) override
    {
        m_hover = false;
        update();
    }

    void mousePressEvent(QMouseEvent* event) override
    {
        event->accept(); // do not start a window drag from the dots
    }

    void mouseReleaseEvent(QMouseEvent* event) override
    {
        const int index = static_cast<int>(event->position().x()) / 20;
        if (event->button() == Qt::LeftButton) {
            if (index == 0 && onClose) {
                onClose();
            } else if (index == 1 && onCompact) {
                onCompact();
            } else if (index == 2 && onZoom) {
                onZoom();
            }
        }
        event->accept();
    }

private:
    bool m_hover = false;
    bool m_active = true;
};

// Window header: traffic lights, terminal-style prompt title and a menu button.
class WidgetChrome : public QWidget
{
public:
    TrafficLights* lights = nullptr;
    QPushButton* menuButton = nullptr;

    explicit WidgetChrome(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setFixedHeight(26);

        auto* layout = new QHBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(10);

        lights = new TrafficLights(this);

        m_prompt = new QLabel(this);
        m_prompt->setProperty("role", "prompt");

        menuButton = new QPushButton(Theme::glyph(Theme::Glyph::More), this);
        menuButton->setProperty("variant", "ghost");
        menuButton->setFlat(true);
        menuButton->setFixedSize(26, 22);
        menuButton->setCursor(Qt::PointingHandCursor);
        menuButton->setToolTip(QString::fromUtf8("Настройки виджета (ПКМ)"));
        menuButton->setStyleSheet(
            QString("QPushButton { font-family: \"%1\"; font-size: 14px; color: #A9AFC9; }")
                .arg(Theme::iconFamily()));

        layout->addWidget(lights);
        layout->addWidget(m_prompt, 1);
        layout->addWidget(menuButton);
        m_layout = layout;
    }

    // Puts a small widget (badge, button) between the title and the menu button.
    void addRightWidget(QWidget* widget)
    {
        m_layout->insertWidget(m_layout->count() - 1, widget);
    }

    void setPrompt(const QString& path, const QString& title)
    {
        m_prompt->setText(QString("%1 <span style=\"color:%2\">%3</span>")
                              .arg(path.toHtmlEscaped(), Theme::accentHex(), title.toHtmlEscaped()));
        m_prompt->setTextFormat(Qt::RichText);
    }

private:
    QLabel* m_prompt = nullptr;
    QHBoxLayout* m_layout = nullptr;
};
