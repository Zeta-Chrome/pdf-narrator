#include "app_controller.h"
#include <QDebug>
#include <QFileInfo>
#include <QStandardPaths>
#include <QCoreApplication>
#include <QTimer>

AppController::AppController(QObject *parent)
    : QObject(parent), m_isPlaying(false), m_isPdfLoaded(false), m_totalPages(0)
{
    m_pdfParser = std::make_unique<PDFParser>();
    m_ttsManager = std::make_unique<TTSManager>();
    m_audioManager = std::make_unique<AudioManager>();

    connect(m_pdfParser.get(), &PDFParser::pdfLoaded, this, &AppController::onPdfLoaded);
    connect(m_pdfParser.get(), &PDFParser::pdfLoadFailed, this, &AppController::onPdfLoadFailed);
}

AppController::~AppController()
{
}

void AppController::openPDF(const QString &filePath)
{
    QFileInfo fileInfo(filePath);
    if (!fileInfo.exists() || !fileInfo.isFile())
    {
        emit errorOccurred("File does not exist: " + filePath);
        return;
    }

    m_pdfPath = filePath;
    m_pdfParser->loadPDF(filePath);
}

void AppController::openMusic(const QString &musicPath)
{
    QFileInfo fileInfo(musicPath);
    if (!fileInfo.exists() || !fileInfo.isFile())
    {
        emit errorOccurred("Music file does not exist: " + musicPath);
        return;
    }

    m_musicPath = musicPath;
    m_audioManager->playMusic(musicPath);
}

void AppController::onPdfLoaded(int totalPages)
{
    m_isPdfLoaded = true;
    m_totalPages = totalPages;
    emit isPdfLoadedChanged(); 
    emit totalPagesChanged();
    emit statusMessage(QString("PDF loaded from path: %1").arg(m_pdfPath));
}

void AppController::onPdfLoadFailed(const QString &error)
{
    m_isPdfLoaded = false;
    emit errorOccurred("Failed to load PDF: " + error);
}

