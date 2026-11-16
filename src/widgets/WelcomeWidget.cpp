#include "WelcomeWidget.h"

#include "../WidgetRegistry.h"

#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

WelcomeWidget::WelcomeWidget(QWidget* parent)
    : WidgetBase("welcome", parent) {
    setBaseWidgetSize(380, 230);
    setChromeTitle(QStringLiteral("~/welcome $"), QStringLiteral("start"));

    auto* rootLayout = bodyLayout();
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(10);

    auto* subtitleLabel = new QLabel("Your local study companion");
    subtitleLabel->setStyleSheet("color: #A9AFC9; font-size: 12px;");

    auto* descriptionLabel = new QLabel(
        "This is the first widget foundation.\n"
        "Next: Pomodoro, habits, books and local progress data."
    );
    descriptionLabel->setWordWrap(true);
    descriptionLabel->setStyleSheet("color: #d6dae3; font-size: 13px; line-height: 1.4;");

    auto* statusLabel = new QLabel("● Core widget system is working");
    statusLabel->setStyleSheet("color: #8FD6A8; font-size: 12px;");

    rootLayout->addWidget(subtitleLabel);
    rootLayout->addSpacing(8);
    rootLayout->addWidget(descriptionLabel);
    rootLayout->addStretch();
    rootLayout->addWidget(statusLabel);
}

REGISTER_WIDGET(
    "welcome",
    "Welcome widget",
    WelcomeWidget
)
