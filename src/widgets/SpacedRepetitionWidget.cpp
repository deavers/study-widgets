#include "SpacedRepetitionWidget.h"

#include "../DataStore.h"
#include "../Theme.h"
#include "../WidgetRegistry.h"

#include <QAbstractItemView>
#include <QComboBox>
#include <QDateTime>
#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QTimer>
#include <QVBoxLayout>

SpacedRepetitionWidget::SpacedRepetitionWidget(QWidget* parent)
    : WidgetBase("spaced_rep", parent)
{
    setBaseWidgetSize(420, 420);
    setChromeTitle(QStringLiteral("~/spaced_rep $"), QStringLiteral("reviews"));

    auto* rootLayout = bodyLayout();
    rootLayout->setSpacing(8);
    rootLayout->setContentsMargins(0, 0, 0, 0);

    auto* headerLayout = new QHBoxLayout();
    auto* titleLabel = new QLabel(QString::fromUtf8("Интервалы: Přednáška & Cvičení"));
    titleLabel->setFont(Theme::fontUi(12, QFont::DemiBold));
    titleLabel->setStyleSheet(QStringLiteral("color: %1;").arg(Theme::textColor().name()));

    m_summaryLabel = new QLabel();
    m_summaryLabel->setFont(Theme::fontMono(10, QFont::Bold));
    m_summaryLabel->setStyleSheet(QStringLiteral("color: %1;").arg(Theme::accentColor().name()));

    headerLayout->addWidget(titleLabel);
    headerLayout->addStretch();
    headerLayout->addWidget(m_summaryLabel);
    rootLayout->addLayout(headerLayout);

    auto* topInputRow = new QHBoxLayout();
    topInputRow->setSpacing(6);

    m_categoryCombo = new QComboBox();
    m_categoryCombo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    m_categoryCombo->setMinimumContentsLength(14);
    if (m_categoryCombo->view()) {
        m_categoryCombo->view()->setMinimumWidth(350);
    }

    const QList<StudyCategory> cats = DataStore::instance().studyCategories();
    for (const StudyCategory& c : cats) {
        m_categoryCombo->addItem(c.name, c.id);
    }

    m_typeCombo = new QComboBox();
    m_typeCombo->addItem("cviceni", "Cvičení");
    m_typeCombo->addItem("prednaska", "Přednáška");
    m_typeCombo->addItem("formula", QString::fromUtf8("Коэфф/Формула"));
    m_typeCombo->addItem("ticket", QString::fromUtf8("?-Тикет"));
    // Set display text properly
    m_typeCombo->clear();
    m_typeCombo->addItem("Cvičení", "cviceni");
    m_typeCombo->addItem("Přednáška", "prednaska");
    m_typeCombo->addItem(QString::fromUtf8("Коэфф/Формула"), "formula");
    m_typeCombo->addItem(QString::fromUtf8("?-Тикет"), "ticket");
    m_typeCombo->setFixedWidth(130);

    topInputRow->addWidget(m_categoryCombo, 1);
    topInputRow->addWidget(m_typeCombo);
    rootLayout->addLayout(topInputRow);

    auto* bottomInputRow = new QHBoxLayout();
    bottomInputRow->setSpacing(6);

    m_topicInput = new QLineEdit();
    m_topicInput->setPlaceholderText(
        QString::fromUtf8("Указатель / формула / ?-тикет → старт 30м...")
    );

    auto* addBtn = new QPushButton(QString::fromUtf8("+ 30м"));
    addBtn->setStyleSheet(
        "background-color: #2A2550; border: 1px solid #C4B5FD; color: #C4B5FD; font-weight: 700; padding: 6px 12px;"
    );

    bottomInputRow->addWidget(m_topicInput, 1);
    bottomInputRow->addWidget(addBtn);
    rootLayout->addLayout(bottomInputRow);

    auto* scrollArea = new QScrollArea();
    scrollArea->setWidgetResizable(true);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    m_listContainer = new QWidget();
    m_listContainer->setStyleSheet("background: transparent;");
    m_listLayout = new QVBoxLayout(m_listContainer);
    m_listLayout->setContentsMargins(0, 0, 4, 0);
    m_listLayout->setSpacing(7);
    m_listLayout->addStretch();

    scrollArea->setWidget(m_listContainer);
    rootLayout->addWidget(scrollArea, 1);

    connect(addBtn, &QPushButton::clicked, this, &SpacedRepetitionWidget::addReviewFromInput);
    connect(m_topicInput, &QLineEdit::returnPressed, this, &SpacedRepetitionWidget::addReviewFromInput);

    rebuildReviewList();

    m_timer = new QTimer(this);
    m_timer->setInterval(15000);
    connect(m_timer, &QTimer::timeout, this, &SpacedRepetitionWidget::rebuildReviewList);
    m_timer->start();
}

void SpacedRepetitionWidget::addReviewFromInput()
{
    const QString title = m_topicInput->text().trimmed();
    if (title.isEmpty()) {
        return;
    }

    const QString catId = m_categoryCombo->currentData().toString();
    const QString kind = m_typeCombo->currentData().toString();

    if (DataStore::instance().addSpacedReview(catId, title, kind, QStringLiteral(""))) {
        m_topicInput->clear();
        rebuildReviewList();
    }
}

void SpacedRepetitionWidget::rebuildReviewList()
{
    while (QLayoutItem* item = m_listLayout->takeAt(0)) {
        if (QWidget* w = item->widget()) {
            w->deleteLater();
        }
        delete item;
    }

    const QList<SpacedReviewItem> items = DataStore::instance().spacedReviews();
    const QDateTime now = QDateTime::currentDateTime();

    static const QStringList stageNames = {
        QString::fromUtf8("30м"),
        QString::fromUtf8("2ч"),
        QString::fromUtf8("1д"),
        QString::fromUtf8("3д"),
        QString::fromUtf8("7д"),
        QString::fromUtf8("14д"),
        QString::fromUtf8("30д")
    };

    int dueCount = 0;

    for (const SpacedReviewItem& item : items) {
        const qint64 secsLeft = now.secsTo(item.nextReviewAt);
        const bool isDue = (secsLeft <= 0);
        if (isDue) {
            ++dueCount;
        }

        auto* card = new QWidget();
        card->setStyleSheet(
            isDue
                ? "background-color: #3A2230; border: 1px solid #F29AA8; border-radius: 8px;"
                : "background-color: #1C1F36; border: 1px solid #2F3558; border-radius: 8px;"
        );

        auto* cardLayout = new QVBoxLayout(card);
        cardLayout->setContentsMargins(10, 8, 10, 8);
        cardLayout->setSpacing(5);

        auto* titleLbl = new QLabel(
            QString("[%1] %2").arg(item.kind.toUpper(), item.topic)
        );
        titleLbl->setWordWrap(true);
        titleLbl->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        titleLbl->setStyleSheet(
            "border: none; font-weight: 600; font-size: 12px; color: #FFFFFF;"
        );
        cardLayout->addWidget(titleLbl);

        auto* bottomRow = new QHBoxLayout();
        bottomRow->setSpacing(6);

        const QString stageTag = stageNames.value(
            qBound(0, item.stage, stageNames.size() - 1)
        );

        QString dueText;
        if (isDue) {
            const qint64 overdueMins = (-secsLeft) / 60;
            dueText = QString::fromUtf8("🔥 Повторить! (+%1м • этап %2)")
                          .arg(overdueMins)
                          .arg(stageTag);
        } else {
            const qint64 mins = (secsLeft + 59) / 60;
            if (mins < 60) {
                dueText = QString::fromUtf8("⏳ Через %1 мин (этап %2)")
                              .arg(mins)
                              .arg(stageTag);
            } else if (mins < 1440) {
                dueText = QString::fromUtf8("⏳ Через %1ч %2м (этап %3)")
                              .arg(mins / 60)
                              .arg(mins % 60)
                              .arg(stageTag);
            } else {
                dueText = QString::fromUtf8("⏳ Через %1 дн. (этап %2)")
                              .arg(mins / 1440)
                              .arg(stageTag);
            }
        }

        auto* statusLbl = new QLabel(dueText);
        statusLbl->setStyleSheet(
            isDue
                ? "border: none; color: #F7B9C4; font-size: 11px; font-weight: 700;"
                : "border: none; color: #C4B5FD; font-size: 11px; font-weight: 600;"
        );

        auto* repeat30Btn = new QPushButton(QString::fromUtf8("↻ 30м"));
        repeat30Btn->setToolTip(QString::fromUtf8("Перезапустить 30-минутный таймер закрепления"));

        auto* nextStageBtn = new QPushButton(QString::fromUtf8("✓ Этап+"));
        nextStageBtn->setStyleSheet(
            "background-color: #1F3A33; border: 1px solid #8FD6A8; color: #BFEBD0; font-weight: 700;"
        );

        auto* delBtn = new QPushButton(QString::fromUtf8("×"));
        delBtn->setFixedWidth(28);
        delBtn->setStyleSheet("color: #A8B4C8; font-weight: 700;");

        bottomRow->addWidget(statusLbl, 1);
        bottomRow->addWidget(repeat30Btn);
        bottomRow->addWidget(nextStageBtn);
        bottomRow->addWidget(delBtn);
        cardLayout->addLayout(bottomRow);

        const qint64 id = item.id;
        connect(repeat30Btn, &QPushButton::clicked, this, [this, id]() {
            DataStore::instance().advanceSpacedReview(id, true);
            QTimer::singleShot(0, this, &SpacedRepetitionWidget::rebuildReviewList);
        });
        connect(nextStageBtn, &QPushButton::clicked, this, [this, id]() {
            DataStore::instance().advanceSpacedReview(id, false);
            QTimer::singleShot(0, this, &SpacedRepetitionWidget::rebuildReviewList);
        });
        connect(delBtn, &QPushButton::clicked, this, [this, id]() {
            DataStore::instance().removeSpacedReview(id);
            QTimer::singleShot(0, this, &SpacedRepetitionWidget::rebuildReviewList);
        });

        m_listLayout->addWidget(card);
    }

    m_listLayout->addStretch();

    m_summaryLabel->setText(
        dueCount > 0
            ? QString::fromUtf8("🔥 К повтору: %1 / %2").arg(dueCount).arg(items.size())
            : QString::fromUtf8("Активно: %1").arg(items.size())
    );
}

REGISTER_WIDGET(
    "spaced_rep",
    "Повторение 30м (Přednáška & Cvičení)",
    SpacedRepetitionWidget
)
