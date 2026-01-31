#pragma once
#include <QObject>
#include <QString>
#include <QImage>
#include <QVector>

extern "C"
{
#include <mupdf/fitz.h>
}

struct PageContents
{
    QVector<QString> sentences;
    QVector<QImage> images;
};

class PDFParser : public QObject
{
    Q_OBJECT
public:
    explicit PDFParser(QObject *parent = nullptr);
    ~PDFParser();
    bool loadPDF(const QString &filePath);
    void closePDF();
    PageContents extractPageContents(int pageNumber);
    int pageCount() const;
    bool isLoaded() const
    {
        return m_context != nullptr && m_document != nullptr;
    }
signals:
    void pdfLoaded(int pageCount);
    void pdfLoadFailed(const QString &error);
    void pageRendered(int pageNumber, const QImage &image);
    void textExtracted(int pageNumber, const QVector<QString> &sentences);
private:
    QVector<QString> segmentIntoSentences(const QString &text);
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
