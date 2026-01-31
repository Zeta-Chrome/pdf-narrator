#include "audio_manager.h"
#include <QAudioFormat>
#include <QMediaDevices>
#include <QAudioOutput>
#include <QDebug>

AudioManager::AudioManager(QObject *parent)
    : QObject(parent), m_speechVolume(1.0f), m_musicVolume(0.3f), m_isPaused(false)
{
    m_musicPlayer = std::make_unique<QMediaPlayer>();
    m_musicPlayer->setAudioOutput(new QAudioOutput(m_musicPlayer.get()));

    connect(m_musicPlayer.get(), &QMediaPlayer::playbackStateChanged, this,
            &AudioManager::onMusicStateChanged);
}

AudioManager::~AudioManager()
{
    stop();
}

void AudioManager::playSpeech(const QByteArray &audioData, float sampleRate)
{
    QMutexLocker locker(&m_mutex);

    if (m_audioSink && m_audioSink->state() != QAudio::StoppedState)
    {
        m_audioSink->stop();
    }

    QAudioFormat format;
    format.setSampleRate(static_cast<int>(sampleRate));
    format.setChannelCount(1);
    format.setSampleFormat(QAudioFormat::Float);

    QAudioDevice deviceInfo = QMediaDevices::defaultAudioOutput();
    if (!deviceInfo.isFormatSupported(format))
    {
        qWarning() << "Audio format not supported, trying to find nearest";
        format = deviceInfo.preferredFormat();
    }

    m_audioSink = std::make_unique<QAudioSink>(deviceInfo, format);
    m_audioSink->setVolume(m_speechVolume);

    connect(m_audioSink.get(), &QAudioSink::stateChanged, this, &AudioManager::onSpeechStateChanged,
            Qt::UniqueConnection);

    m_currentAudioData = audioData;
    m_audioBuffer = std::make_unique<QBuffer>(&m_currentAudioData);
    m_audioBuffer->open(QIODevice::ReadOnly);

    m_audioSink->start(m_audioBuffer.get());
    m_isPaused = false;
}

void AudioManager::playMusic(const QString &musicFilePath)
{
    QMutexLocker locker(&m_mutex);

    m_musicPlayer->setSource(QUrl::fromLocalFile(musicFilePath));
    m_musicPlayer->audioOutput()->setVolume(m_musicVolume);
    m_musicPlayer->play();
}

void AudioManager::stopMusic()
{
    QMutexLocker locker(&m_mutex);

    if (m_musicPlayer->playbackState() != QMediaPlayer::StoppedState)
    {
        m_musicPlayer->stop();
    }
}

void AudioManager::pause()
{
    QMutexLocker locker(&m_mutex);

    if (m_audioSink && m_audioSink->state() == QAudio::ActiveState)
    {
        m_audioSink->suspend();
        m_isPaused = true;
    }

    if (m_musicPlayer->playbackState() == QMediaPlayer::PlayingState)
    {
        m_musicPlayer->pause();
    }
}

void AudioManager::resume()
{
    QMutexLocker locker(&m_mutex);

    if (m_audioSink && m_audioSink->state() == QAudio::SuspendedState)
    {
        m_audioSink->resume();
        m_isPaused = false;
    }

    if (m_musicPlayer->playbackState() == QMediaPlayer::PausedState)
    {
        m_musicPlayer->play();
    }
}

void AudioManager::stop()
{
    QMutexLocker locker(&m_mutex);

    if (m_audioSink)
    {
        m_audioSink->stop();
        m_audioSink.reset();
    }

    if (m_audioBuffer)
    {
        m_audioBuffer->close();
        m_audioBuffer.reset();
    }

    if (m_musicPlayer->playbackState() != QMediaPlayer::StoppedState)
    {
        m_musicPlayer->stop();
    }

    m_isPaused = false;
    m_currentAudioData.clear();
}

void AudioManager::setMusicVolume(float volume)
{
    QMutexLocker locker(&m_mutex);

    m_musicVolume = qBound(0.0f, volume, 1.0f);
    if (m_musicPlayer && m_musicPlayer->audioOutput())
    {
        m_musicPlayer->audioOutput()->setVolume(m_musicVolume);
    }
}

bool AudioManager::isSpeechPlaying() const
{
    return m_audioSink && m_audioSink->state() == QAudio::ActiveState;
}

bool AudioManager::isMusicPlaying() const
{
    return m_musicPlayer->playbackState() == QMediaPlayer::PlayingState;
}

bool AudioManager::isPaused() const
{
    return m_isPaused;
}

void AudioManager::onSpeechStateChanged(QAudio::State state)
{
    if (state == QAudio::IdleState || state == QAudio::StoppedState)
    {
        emit speechFinished();
    }
    else if (state == QAudio::ActiveState)
    {
        // Speech is playing
    }
}

void AudioManager::onMusicStateChanged(QMediaPlayer::PlaybackState state)
{
    emit musicStateChanged(state == QMediaPlayer::PlayingState);
}
