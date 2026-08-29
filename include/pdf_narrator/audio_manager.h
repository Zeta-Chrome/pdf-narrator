#pragma once

#include <QObject>
#include <QAudioSink>
#include <QAudioSource>
#include <QAudioOutput>
#include <QMediaPlayer>
#include <QBuffer>
#include <QByteArray>
#include <QMutex>
#include <memory>

class AudioManager : public QObject {
	Q_OBJECT

public:
	explicit AudioManager(QObject *parent = nullptr);
	~AudioManager() override;

	void playSpeech(QByteArray audioData, int sampleRate, uint16_t pageNo, uint16_t sentenceIdx);
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

	bool isMusicEnabled() const
	{
		return m_isMusicEnabled;
	}

	bool isSpeechPlaying() const
	{
		return m_audioSink && m_audioSink->state() == QAudio::ActiveState;
	}

	bool isMusicPlaying() const
	{
		return m_musicPlayer && m_musicPlayer->playbackState() == QMediaPlayer::PlayingState;
	}

	bool isPaused() const
	{
		return m_isPaused;
	}

signals:
	void speechFinished(uint16_t pageNo, uint16_t sentenceIdx);
	void musicFinished();

private slots:
	void onSpeechStateChanged(QAudio::State state, uint16_t pageNo, uint16_t sentenceIdx);
	void onMusicStateChanged(QMediaPlayer::MediaStatus state);

private:
	std::unique_ptr<QAudioSink> m_audioSink;
	std::unique_ptr<QBuffer> m_audioBuffer;

	std::unique_ptr<QMediaPlayer> m_musicPlayer;
	std::unique_ptr<QAudioOutput> m_musicAudioOutput;

	float m_speechVolume = 1.0f;
	float m_musicVolume = 0.3f;
	bool m_isPaused = false;
	bool m_isMusicEnabled = true;

	QMutex m_mutex;
	QByteArray m_currentAudioData;
};
