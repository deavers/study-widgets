#include "DailyWisdomWidget.h"

#include "../CategoryConfigLoader.h"
#include "../WidgetRegistry.h"

#include <QClipboard>
#include <QDate>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QPushButton>
#include <QSettings>

DailyWisdomWidget::DailyWisdomWidget(QWidget* parent)
    : WidgetBase("daily_wisdom", parent)
{
    setBaseWidgetSize(410, 256);
    setChromeTitle("Аят дня", "Поток");
    setRootMargins(12, 10, 12, 10);
    bodyLayout()->setSpacing(6);

    loadWisdomEntries();

    auto* card = new QWidget();
    card->setProperty("role", "card");
    auto* cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(14, 11, 14, 10);
    cardLayout->setSpacing(8);

    m_arabicLabel = new QLabel();
    m_arabicLabel->setProperty("role", "arabic");
    m_arabicLabel->setWordWrap(true);
    m_arabicLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    cardLayout->addWidget(m_arabicLabel);

    m_translationLabel = new QLabel();
    m_translationLabel->setWordWrap(true);
    m_translationLabel->setStyleSheet("font-size: 12px;");
    cardLayout->addWidget(m_translationLabel);

    auto* sourceRow = new QHBoxLayout();
    sourceRow->setSpacing(6);

    m_sourceLabel = new QLabel();
    m_sourceLabel->setProperty("role", "ok");
    sourceRow->addWidget(m_sourceLabel, 1);

    auto* copyBtn = new QPushButton(Theme::glyph(Theme::Glyph::Copy));
    copyBtn->setProperty("variant", "ghost");
    copyBtn->setFixedSize(24, 22);
    copyBtn->setToolTip(QString::fromUtf8("Скопировать (или через ПКМ)"));
    copyBtn->setStyleSheet(QString("QPushButton { font-family: \"%1\"; font-size: 12px; padding: 0; }")
                               .arg(Theme::iconFamily()));

    auto* nextBtn = new QPushButton(Theme::glyph(Theme::Glyph::Refresh));
    nextBtn->setProperty("variant", "ghost");
    nextBtn->setFixedSize(24, 22);
    nextBtn->setToolTip(QString::fromUtf8("Другой аят или хадис (или через ПКМ)"));
    nextBtn->setStyleSheet(QString("QPushButton { font-family: \"%1\"; font-size: 12px; padding: 0; }")
                               .arg(Theme::iconFamily()));

    sourceRow->addWidget(copyBtn);
    sourceRow->addWidget(nextBtn);
    cardLayout->addLayout(sourceRow);

    bodyLayout()->addWidget(card);

    auto* stateRow = new QHBoxLayout();
    stateRow->setSpacing(6);
    const QStringList names = {QString::fromUtf8("Поток"), QString::fromUtf8("Туман"),
                               QString::fromUtf8("Аманат")};
    for (int i = 0; i < names.size(); ++i) {
        auto* btn = new QPushButton(names[i]);
        btn->setProperty("variant", "chip");
        btn->setCheckable(true);
        m_stateButtons.append(btn);
        stateRow->addWidget(btn, 1);
        connect(btn, &QPushButton::clicked, this, [this, i]() {
            setOperatorState(i);
        });
    }
    bodyLayout()->addLayout(stateRow);

    m_stateHintLabel = new QLabel();
    m_stateHintLabel->setWordWrap(true);
    m_stateHintLabel->setProperty("role", "note");
    bodyLayout()->addWidget(m_stateHintLabel);
    bodyLayout()->addStretch();

    connect(nextBtn, &QPushButton::clicked, this, &DailyWisdomWidget::nextEntry);
    connect(copyBtn, &QPushButton::clicked, this, &DailyWisdomWidget::copyCurrentEntry);

    showEntry(QDate::currentDate().dayOfYear() % m_entries.size());

    QSettings settings;
    setOperatorState(qBound(0, settings.value("daily_wisdom/operatorState", 0).toInt(), 2));
}

void DailyWisdomWidget::buildContextMenu(QMenu* menu)
{
    QAction* nextAct = menu->addAction(QString::fromUtf8("↻ Следующий аят или хадис"));
    QAction* copyAct = menu->addAction(QString::fromUtf8("📋 Скопировать текст в буфер"));
    connect(nextAct, &QAction::triggered, this, &DailyWisdomWidget::nextEntry);
    connect(copyAct, &QAction::triggered, this, &DailyWisdomWidget::copyCurrentEntry);

    menu->addSeparator();
    const QStringList states = {
        QString::fromUtf8("Состояние: Поток (глубокий фокус)"),
        QString::fromUtf8("Состояние: Туман (перегрузка / отдых)"),
        QString::fromUtf8("Состояние: Аманат (границы и баланс)")
    };
    for (int i = 0; i < states.size(); ++i) {
        QAction* act = menu->addAction(states[i]);
        act->setCheckable(true);
        act->setChecked(m_stateIndex == i);
        connect(act, &QAction::triggered, this, [this, i]() { setOperatorState(i); });
    }
}

void DailyWisdomWidget::copyCurrentEntry()
{
    if (m_entries.isEmpty()) {
        return;
    }
    const WisdomEntry& e = m_entries[m_currentIndex];
    QGuiApplication::clipboard()->setText(QString("%1\n%2\n— %3").arg(e.arabic, e.translation, e.source));
}

void DailyWisdomWidget::loadWisdomEntries()
{
    m_entries = {
        {QString::fromUtf8("وَقُل رَّبِّ زِدْنِي عِلْمًا"),
         QString::fromUtf8("«И говори: “Господи! Приумножь мои знания!”»"),
         QString::fromUtf8("Коран, Та-Ха (20:114)")},
        {QString::fromUtf8("فَإِنَّ مَعَ الْعُسْرِ يُسْرًا ۝ إِنَّ مَعَ الْعُسْرِ يُسْرًا"),
         QString::fromUtf8("«Воистину, за каждой тягостью наступает облегчение. Воистину, за каждой тягостью наступает облегчение.»"),
         QString::fromUtf8("Коран, Аш-Шарх (94:5–6)")},
        {QString::fromUtf8("إِنَّ اللَّهَ يُحِبُّ إِذَا عَمِلَ أَحَدُكُمْ عَمَلًا أَنْ يُتْقِنَهُ"),
         QString::fromUtf8("«Аллах любит, когда кто-либо из вас делает дело, чтобы он выполнял его в совершенстве (иткан).»"),
         QString::fromUtf8("Хадис — аль-Байхаки")},
        {QString::fromUtf8("أَلَا بِذِكْرِ اللَّهِ تَطْمَئِنُّ الْقُلُوبُ"),
         QString::fromUtf8("«Воистину, поминанием Аллаха утешаются сердца.»"),
         QString::fromUtf8("Коран, Ар-Рад (13:28)")},
        {QString::fromUtf8("مَنْ سَلَكَ طَرِيقًا يَلْتَمِسُ فِيهِ عِلْمًا سَهَّلَ اللَّهُ لَهُ بِهِ طَرِيقًا إِلَى الْجَنَّةِ"),
         QString::fromUtf8("«Тому, кто вступит на путь поиска знаний, Аллах облегчит путь в Рай.»"),
         QString::fromUtf8("Хадис — Сахих Муслим (2699)")},
        {QString::fromUtf8("وَأَن لَّيْسَ لِلْإِنسَانِ إِلَّا مَا سَعَىٰ ۝ وَأَنَّ سَعْيَهُ سَوْفَ يُرَىٰ"),
         QString::fromUtf8("«Человеку уготовано только то, к чему он стремился. И его устремление будет увидено.»"),
         QString::fromUtf8("Коран, Ан-Наджм (53:39–40)")},
        {QString::fromUtf8("نِعْمَتَانِ مَغْبُونٌ فِيهِمَا كَثِيرٌ مِنَ النَّاسِ: الصِّحَّةُ وَالْفَرَاغُ"),
         QString::fromUtf8("«Две милости, которые не ценят многие люди: здоровье и свободное время.»"),
         QString::fromUtf8("Хадис — Сахих аль-Бухари (6412)")},
        {QString::fromUtf8("إِنَّمَا الْأَعْمَالُ بِالنِّيَّاتِ وَإِنَّمَا لِكُلِّ امْرِئٍ مَا نَوَى"),
         QString::fromUtf8("«Дела оцениваются только по намерениям, и каждому достанется лишь то, что он намеревался обрести.»"),
         QString::fromUtf8("Хадис — Сахих аль-Бухари (1)")},
        {QString::fromUtf8("لَا يُكَلِّفُ اللَّهُ نَفْسًا إِلَّا وُسْعَهَا"),
         QString::fromUtf8("«Аллах не возлагает на душу сверх её возможностей.»"),
         QString::fromUtf8("Коран, Аль-Бакара (2:286)")},
        {QString::fromUtf8("أَحَبُّ الْأَعْمَالِ إِلَى اللَّهِ أَدْوَمُهَا وَإِنْ قَلَّ"),
         QString::fromUtf8("«Самые любимые дела перед Аллахом — те, которые совершаются постоянно, даже если их мало.»"),
         QString::fromUtf8("Хадис — Сахих аль-Бухари (6464)")}};
}

void DailyWisdomWidget::showEntry(int index)
{
    if (m_entries.isEmpty()) {
        return;
    }
    m_currentIndex = (index + m_entries.size()) % m_entries.size();
    const WisdomEntry& e = m_entries[m_currentIndex];
    m_arabicLabel->setText(e.arabic);
    m_translationLabel->setText(e.translation);
    m_sourceLabel->setText(e.source);
    setChromeTitle(e.source, QString());
}

void DailyWisdomWidget::nextEntry()
{
    showEntry(m_currentIndex + 1);
}

void DailyWisdomWidget::setOperatorState(int stateIndex)
{
    m_stateIndex = qBound(0, stateIndex, 2);
    QSettings settings;
    settings.setValue("daily_wisdom/operatorState", m_stateIndex);

    for (int i = 0; i < m_stateButtons.size(); ++i) {
        m_stateButtons[i]->setChecked(i == m_stateIndex);
    }

    const LocalSeedConfig& seed = CategoryConfigLoader::seedConfig();
    if (m_stateIndex == 0)
    {
        m_stateHintLabel->setText(seed.wisdomFlowHint);
    }
    else if (m_stateIndex == 1)
    {
        m_stateHintLabel->setText(QString::fromUtf8(
            "Туман: сенсорная перегрузка или недосып. Убери шум, сделай 2 физиологических вдоха, разомни шею, омовение."
        ));
    }
    else
    {
        m_stateHintLabel->setText(seed.wisdomAmanahHint);
    }
}

REGISTER_WIDGET("daily_wisdom", "Аят и хадис дня", DailyWisdomWidget)
