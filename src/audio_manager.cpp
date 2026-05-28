#include "audio_manager.h"
#include <QAudioFormat>
#include <QMediaDevices>
#include <QAudioOutput>
#include <QDebug>
#include <qaudio.h>

AudioManager::AudioManager(QObject *parent)
    : QObject(parent)
{
    m_musicPlayer = std::make_unique<QMediaPlayer>();
    QAudioOutput *audio = new QAudioOutput(this);
    m_musicPlayer->setAudioOutput(audio);

    connect(m_musicPlayer.get(), &QMediaPlayer::mediaStatusChanged, this,
            &AudioManager::onMusicStateChanged);
}

AudioManager::~AudioManager()
{
    stopMusic();
    stopSpeech();
}

void AudioManager::playSpeech(QByteArray audioData, float sampleRate, uint16_t pageNo, uint16_t sentenceIdx)
{
    QMutexLocker locker(&m_mutex);

    if (m_audioSink && m_audioSink->state() != QAudio::StoppedState && m_audioSink &&
        m_audioSink->state() != QAudio::IdleState)
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

    connect(m_audioSink.get(), &QAudioSink::stateChanged, this,
            [this, pageNo, sentenceIdx](QAudio::State state)
            {
                onSpeechStateChanged(state, pageNo, sentenceIdx);
            });

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

    if (!m_isMusicEnabled)
    {
        return;
    }
    m_musicPlayer->play();
}

void AudioManager::toggleMusic()
{
    m_isMusicEnabled = !m_isMusicEnabled;
    if (m_isMusicEnabled)
    {
        m_musicPlayer->play();
    }
    else
    {
        m_musicPlayer->pause();
    }
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

    if (!m_isMusicEnabled)
    {
        return;
    }

    if (m_musicPlayer->playbackState() == QMediaPlayer::PlayingState)
    {
        m_musicPlayer->pause();
    }
    else
    {
        m_musicPlayer->setPosition(0);
        m_musicPlayer->play();
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

    if (!m_isMusicEnabled)
    {
        return;
    }

    if (m_musicPlayer->playbackState() == QMediaPlayer::PausedState)
    {
        m_musicPlayer->play();
    }
}

void AudioManager::stopSpeech()
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

void AudioManager::onSpeechStateChanged(QAudio::State state, uint16_t pageNo, uint16_t sentenceIdx)
{
    if (state == QAudio::IdleState)
    {
        emit speechFinished(pageNo, sentenceIdx);
    }
    else if (state == QAudio::ActiveState)
    {
        // Speech is playing
    }
}

void AudioManager::onMusicStateChanged(QMediaPlayer::MediaStatus state)
{
    if (state == QMediaPlayer::EndOfMedia)
    {
        emit musicFinished();
    }
}
