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

    void playSpeech(QByteArray audioData, float sampleRate);
    void playMusic(const QString &musicFilePath);
    void toggleMusic();
    void stopMusic();
    void stopSpeech();

    void pause();
    void resume();

    void setMusicVolume(float volume);
    
    float musicVolume() const 
    {
        return m_musicVolume;
    }

    int isMusicEnabled() const 
    {
        return m_isMusicEnabled;
    }

    bool isSpeechPlaying() const
    {
        return m_audioSink && m_audioSink->state() == QAudio::ActiveState;
    }

    bool isMusicPlaying() const
    {
        return m_musicPlayer->playbackState() == QMediaPlayer::PlayingState;
    }

    bool isPaused() const
    {
        return m_isPaused;
    }

signals:
    void speechFinished();
    void speechFailed(const QString &error);
    void musicFinished();

private slots:
    void onSpeechStateChanged(QAudio::State state);
    void onMusicStateChanged(QMediaPlayer::MediaStatus state);

private:
    std::unique_ptr<QAudioSink> m_audioSink;
    std::unique_ptr<QBuffer> m_audioBuffer;
    std::unique_ptr<QMediaPlayer> m_musicPlayer;

    float m_speechVolume;
    float m_musicVolume;
    bool m_isPaused;
    bool m_isMusicEnabled;

    QMutex m_mutex;
    QByteArray m_currentAudioData;
};
