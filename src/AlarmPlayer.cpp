#include "AlarmPlayer.h"

#include <QFileInfo>
#include <QSettings>
#include <QSoundEffect>
#include <QUrl>

AlarmPlayer& AlarmPlayer::instance()
{
    static AlarmPlayer player;
    return player;
}

AlarmPlayer::AlarmPlayer(QObject* parent)
    : QObject(parent)
    , m_sound(new QSoundEffect(this))
{
    QSettings settings;
    m_volume = settings.value("alarm/volume", 7).toInt();
    m_soundPath = settings.value("alarm/sound", QStringLiteral("C:/Windows/Media/Alarm01.wav")).toString();

    if (!QFileInfo::exists(m_soundPath)) {
        m_soundPath = QStringLiteral("C:/Windows/Media/Windows Notify System Generic.wav");
    }

    m_sound->setSource(QUrl::fromLocalFile(m_soundPath));
    m_sound->setVolume(qBound(0.0f, m_volume / 100.0f, 1.0f));
}

AlarmPlayer::~AlarmPlayer() = default;

void AlarmPlayer::playAlarm()
{
    if (m_sound) {
        m_sound->stop();
        m_sound->play();
    }
}

void AlarmPlayer::setVolume(int volumePercent)
{
    m_volume = qBound(0, volumePercent, 100);
    if (m_sound) {
        m_sound->setVolume(m_volume / 100.0f);
    }
    QSettings settings;
    settings.setValue("alarm/volume", m_volume);
}

int AlarmPlayer::volume() const
{
    return m_volume;
}

void AlarmPlayer::setSoundPath(const QString& path)
{
    if (QFileInfo::exists(path)) {
        m_soundPath = path;
        if (m_sound) {
            m_sound->setSource(QUrl::fromLocalFile(path));
        }
        QSettings settings;
        settings.setValue("alarm/sound", path);
    }
}

QString AlarmPlayer::soundPath() const
{
    return m_soundPath;
}
