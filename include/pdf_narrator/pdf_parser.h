#pragma once
#include "cancellation_token.h"
#include <QObject>
#include <QString>
#include <QImage>
#include <QVector>
#include <qdebug.h>

extern "C" {
#include <mupdf/fitz.h>
}

struct TextList {
	QStringList strList;
	QList<float> yTopList;
	QList<float> yBottomList;
};

struct ImageList {
	QList<QImage> imageList;
	QList<float> yTopList;
	QList<float> yBottomList;
};

struct PlaybackSegment {
	int firstSentenceIdx = -1;
	int lastSentenceIdx = -1;
	int imageIdx = -1;

	bool hasSentences() const
	{
		return firstSentenceIdx != -1 && lastSentenceIdx != -1;
	}

	bool hasImage() const
	{
		return imageIdx != -1;
	}
};

class PDFParser : public QObject {
	Q_OBJECT

public:
	explicit PDFParser(std::shared_ptr<GenerationID> genId, QObject *parent = nullptr);
	~PDFParser();
	const QString &getFilePath();
	void loadPdf(const QString &filePath);
	void closePdf();
	void extractPageContents(int pageNumber, uint32_t genId);

signals:
	void pdfLoaded(int pageCount, const QVector<uint16_t> &sentenceCounts);
	void pdfLoadFailed(const QString &error);
	void pageExtracted(int pageNumber, const QStringList &sentences, const QList<QImage> &images,
					   const QList<PlaybackSegment> &segments, uint32_t genId);
	void pageExtractionFailed(int pageNumber, const QString &error, uint32_t genId);
	void pageExtractionCancelled(int pageNumber);

private:
	void extractPdfStructure(QVector<uint16_t> &sentenceCounts);
	void countBlockContents(fz_stext_block *block, uint16_t &sentenceCount, float topMargin,
							float bottomMargin);
	TextList getBlockTextLines(fz_stext_block *block);
	TextList getPageSentences(QList<TextList> &textBlockLines);
	void extractBlockContents(fz_stext_block *block, TextList &sentences, ImageList &images,
							  float topMargin, float bottomMargin, float minImageSize,
							  uint32_t genId);
	ImageList getBlockImage(fz_stext_block *block);
	void getPlaybackSegments(TextList &sentences, ImageList &images,
							 QList<PlaybackSegment> &segments);

#ifdef TESTING
	void saveImagesToTestDirectory(const ImageList &images, int pageNumber);
#endif

	fz_context *m_context;
	QByteArray m_pdfData;
	fz_document *m_document;
	QString m_filePath;
	int m_pageCount{ 0 };
	std::shared_ptr<GenerationID> m_genId;
};
