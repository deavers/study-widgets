#pragma once

#include <QObject>
#include <QString>

class QSoundEffect;

class AlarmPlayer : public QObject
{
    Q_OBJECT

public:
    static AlarmPlayer& instance();

    void playAlarm();
    void setVolume(int volumePercent); // 0..100
    int volume() const;

    void setSoundPath(const QString& path);
    QString soundPath() const;

private:
    explicit AlarmPlayer(QObject* parent = nullptr);
    ~AlarmPlayer() override;

    QSoundEffect* m_sound = nullptr;
    int m_volume = 7;
    QString m_soundPath;
};
