#include "WelcomeWidget.h"

#include "../WidgetRegistry.h"

#include <QApplication>
#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

WelcomeWidget::WelcomeWidget(QWidget* parent)
    : WidgetBase("welcome", parent) {
    resize(380, 230);

    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(24, 20, 24, 20);
    rootLayout->setSpacing(10);

    auto* headerLayout = new QHBoxLayout();
    headerLayout->setSpacing(8);

    auto* titleLabel = new QLabel("StudyWidgets");
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(17);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);

    auto* minimizeButton = new QPushButton("−");
    minimizeButton->setFixedSize(30, 30);
    minimizeButton->setToolTip("Minimize window");
    minimizeButton->setCursor(Qt::PointingHandCursor);

    auto* closeButton = new QPushButton("×");
    closeButton->setFixedSize(30, 30);
    closeButton->setToolTip("Exit StudyWidgets");
    closeButton->setCursor(Qt::PointingHandCursor);

    minimizeButton->setStyleSheet(R"(
        QPushButton {
            background-color: transparent;
            border: none;
            border-radius: 15px;
            color: #aeb4c2;
            font-size: 22px;
            font-weight: bold;
        }

        QPushButton:hover {
            background-color: #3c3f49;
            color: #ffffff;
        }

        QPushButton:pressed {
            background-color: #4b4e5a;
        }
    )");

    closeButton->setStyleSheet(R"(
        QPushButton {
            background-color: transparent;
            border: none;
            border-radius: 15px;
            color: #aeb4c2;
            font-size: 22px;
            font-weight: bold;
        }

        QPushButton:hover {
            background-color: #b94b5c;
            color: #ffffff;
        }

        QPushButton:pressed {
            background-color: #923b49;
        }
    )");

    headerLayout->addWidget(titleLabel);
    headerLayout->addStretch();
    headerLayout->addWidget(minimizeButton);
    headerLayout->addWidget(closeButton);

    auto* subtitleLabel = new QLabel("Your local study companion");
    subtitleLabel->setStyleSheet(
        "color: #9aa3b2;"
        "font-size: 12px;"
    );

    auto* descriptionLabel = new QLabel(
        "This is the first widget foundation.\n"
        "Next: Pomodoro, habits, books and local progress data."
    );

    descriptionLabel->setWordWrap(true);
    descriptionLabel->setStyleSheet(
        "color: #d6dae3;"
        "font-size: 13px;"
        "line-height: 1.4;"
    );

    auto* statusLabel = new QLabel("● Core widget system is working");
    statusLabel->setStyleSheet(
        "color: #62d28f;"
        "font-size: 12px;"
    );

    rootLayout->addLayout(headerLayout);
    rootLayout->addWidget(subtitleLabel);
    rootLayout->addSpacing(8);
    rootLayout->addWidget(descriptionLabel);
    rootLayout->addStretch();
    rootLayout->addWidget(statusLabel);

    connect(
        minimizeButton,
        &QPushButton::clicked,
        this,
        &QWidget::showMinimized
    );

    connect(
        closeButton,
        &QPushButton::clicked,
        []() {
            QApplication::quit();
        }
    );
}

REGISTER_WIDGET(
    "welcome",
    "Welcome widget",
    WelcomeWidget
)