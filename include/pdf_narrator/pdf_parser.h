#pragma once
#include <QObject>
#include <QString>
#include <QImage>
#include <QVector>

#define TESTING
extern "C"
{
#include <mupdf/fitz.h>
}

struct PageIndex 
{
    uint16_t sentenceCount = 0;
    uint16_t imageCount = 0;
};

class PDFParser : public QObject
{
    Q_OBJECT

public:
    explicit PDFParser(QObject *parent = nullptr);
    ~PDFParser();
    void loadPdf(const QString &filePath);
    void closePdf();
    void extractPdfStructure();
    void extractPageContents(int pageNumber);

signals:
    void pdfLoaded(int pageCount);
    void pdfLoadFailed(const QString &error);
    void pdfStructureExtracted(QVector<PageIndex> structure);
    void pageExtracted(int pageNumber, QVector<QString> sentences, QVector<QImage> images);
    void pageExtractionFailed(int pageNumber, const QString &error);

private:
    QVector<QString> extractSentences(fz_stext_page *textPage);
    bool isCompleteSentence(const QString &text);

#ifdef TESTING
    void saveImagesToTestDirectory(const QVector<QImage> &images, int pageNumber);
#endif

    fz_context *m_context;
    fz_document *m_document;
    int m_pageCount;
    QString m_filePath;
    QString m_incompleteSentence;
};
