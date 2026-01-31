#pragma once

#include <QObject>
#include <QAudioSink>
#include <QAudioSource>
#include <QMediaPlayer>
#include <QBuffer>
#include <QByteArray>
#include <QMutex>

class AudioManager : public QObject
{
    Q_OBJECT

public:
    explicit AudioManager(QObject *parent = nullptr);
    ~AudioManager();

    void playSpeech(const QByteArray &audioData, float sampleRate);
    void playMusic(const QString &musicFilePath);
    void stopMusic();

    void pause();
    void resume();
    void stop();

    void setMusicVolume(float volume);

    bool isSpeechPlaying() const;
    bool isMusicPlaying() const;
    bool isPaused() const;

signals:
    void speechFinished();
    void speechError(const QString &error);
    void musicStateChanged(bool playing);
    void playbackPositionChanged(qint64 position);

private slots:
    void onSpeechStateChanged(QAudio::State state);
    void onMusicStateChanged(QMediaPlayer::PlaybackState state);

private:
    std::unique_ptr<QAudioSink> m_audioSink;
    std::unique_ptr<QBuffer> m_audioBuffer;
    std::unique_ptr<QMediaPlayer> m_musicPlayer;

    float m_speechVolume;
    float m_musicVolume;
    bool m_isPaused;

    QMutex m_mutex;
    QByteArray m_currentAudioData;
};
