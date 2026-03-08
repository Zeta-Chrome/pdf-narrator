#pragma once
#include <QObject>
#include <QString>
#include <QImage>
#include <QVector>

extern "C"
{
#include <mupdf/fitz.h>
}

class PDFParser : public QObject
{
    Q_OBJECT

public:
    explicit PDFParser(QObject *parent = nullptr);
    ~PDFParser();
    void loadPDF(const QString &filePath);
    void closePDF();
    void extractPageContents(int pageNumber);

signals:
    void pdfLoaded(int pageCount);
    void pdfLoadFailed(const QString &error);
    void pageExtracted(int pageNumber, QVector<QString> sentences, QVector<QImage> images);
    void pageExtractionFailed(int pageNumber, const QString &error);

private:
    QVector<QString> extractLines(fz_stext_page *textPage);
    QVector<QString> segmentLinesToSentences(QVector<QString>& lines);
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
