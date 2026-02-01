#pragma once

#include <QObject>
#include <QString>
#include <QImage>
#include <QVariant>
#include <QMutex>

#include <pdf_parser.h>
#include <tts_manager.h>
#include <audio_manager.h>

class AppController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool isPlaying READ isPlaying NOTIFY isPlayingChanged)
    Q_PROPERTY(bool isPdfLoaded READ isPdfLoaded NOTIFY isPdfLoadedChanged)
    Q_PROPERTY(int totalPages READ totalPages NOTIFY totalPagesChanged)

public:
    explicit AppController(QObject *parent = nullptr);
    ~AppController();

    // Property getters
    bool isPlaying() const
    {
        return m_isPlaying;
    }
    bool isPdfLoaded() const
    {
        return m_isPdfLoaded;
    }
    int totalPages() const 
    {
        return m_totalPages;
    }

    // Q_INVOKABLE methods for QML
    Q_INVOKABLE void openPDF(const QString &filePath);
    Q_INVOKABLE void openMusic(const QString &musicPath);

signals:
    void errorOccurred(const QString &error);
    void statusMessage(const QString &message);
    void isPdfLoadedChanged();
    void totalPagesChanged();
    void isPlayingChanged();

private slots:
    void onPdfLoaded(int totalPages);
    void onPdfLoadFailed(const QString &error);

private:
    void loadPage(int pageNumber);

    std::unique_ptr<PDFParser> m_pdfParser;
    std::unique_ptr<TTSManager> m_ttsManager;
    std::unique_ptr<AudioManager> m_audioManager;

    bool m_isPlaying;
    bool m_isPdfLoaded;
    QString m_pdfPath;
    int m_totalPages;
    QString m_musicPath;
};
