#include "pdf_parser.h"
#include <QRegularExpression>
#include <cstddef>
#include <QDir>
#include <QUrl>

#define TOP_MARGIN (float)0.08
#define BOTTOM_MARGIN (float)0.08
#define MIN_IMAGE_SIZE (float)0.08

struct OffsetPiece {
	QString text;
	int start;
	int end;
};

PDFParser::PDFParser(QObject *parent)
	: QObject(parent)
	, m_context(nullptr)
	, m_document(nullptr)
	, m_pageCount(0)
{
	m_context = fz_new_context(nullptr, nullptr, FZ_STORE_UNLIMITED);

	if (m_context) {
		fz_disable_icc(m_context);
	}
}

PDFParser::~PDFParser()
{
	closePdf();
	if (m_context) {
		fz_drop_context(m_context);
		m_context = nullptr;
	}
}

const QString &PDFParser::getFilePath()
{
	return m_filePath;
}

void PDFParser::loadPdf(const QString &uriOrPath)
{
	closePdf();

	fz_try(m_context)
	{
		fz_register_document_handlers(m_context);
	}
	fz_catch(m_context)
	{
		QString error =
			QString("Failed to register handlers: %1").arg(fz_caught_message(m_context));
		emit pdfLoadFailed(error);
		return;
	}

	QUrl url(uriOrPath);
	const bool isContentUri = (url.scheme() == "content");

	if (isContentUri) {
		QFile file(uriOrPath);
		if (!file.open(QIODevice::ReadOnly)) {
			QString error = QString("Failed to open PDF: %1").arg(file.errorString());
			emit pdfLoadFailed(error);
			return;
		}
		m_pdfData = file.readAll();
		file.close();

		if (m_pdfData.isEmpty()) {
			emit pdfLoadFailed("Failed to open PDF: file appears to be empty");
			return;
		}

		fz_stream *stream = nullptr;
		fz_try(m_context)
		{
			stream = fz_open_memory(m_context,
									reinterpret_cast<const unsigned char *>(m_pdfData.constData()),
									static_cast<size_t>(m_pdfData.size()));
			m_document = fz_open_document_with_stream(m_context, "application/pdf", stream);
		}
		fz_always(m_context)
		{
			if (stream)
				fz_drop_stream(m_context, stream);
		}
		fz_catch(m_context)
		{
			QString error = QString("Failed to open PDF: %1").arg(fz_caught_message(m_context));
			m_pdfData.clear();
			emit pdfLoadFailed(error);
			return;
		}
	} else {
		QString localPath = url.isLocalFile() ? url.toLocalFile() : uriOrPath;

		fz_try(m_context)
		{
			m_document = fz_open_document(m_context, localPath.toUtf8().constData());
		}
		fz_catch(m_context)
		{
			QString error = QString("Failed to open PDF: %1").arg(fz_caught_message(m_context));
			emit pdfLoadFailed(error);
			return;
		}
	}

	fz_try(m_context)
	{
		m_pageCount = fz_count_pages(m_context, m_document);
	}
	fz_catch(m_context)
	{
		QString error =
			QString("Failed to count document pages: %1").arg(fz_caught_message(m_context));
		m_pdfData.clear();
		emit pdfLoadFailed(error);
		return;
	}

	m_filePath = uriOrPath;
	QVector<uint16_t> sentenceCounts;
	extractPdfStructure(sentenceCounts);
	emit pdfLoaded(m_pageCount, sentenceCounts);
}

void PDFParser::closePdf()
{
	if (m_document) {
		fz_drop_document(m_context, m_document);
		m_document = nullptr;
	}
	m_pdfData.clear();
	m_pageCount = 0;
	m_filePath.clear();
}

void PDFParser::extractPdfStructure(QVector<uint16_t> &sentenceCounts)
{
	sentenceCounts.reserve(m_pageCount);
	for (int pageNo = 0; pageNo < m_pageCount; pageNo++) {
		fz_page *page = nullptr;
		fz_stext_page *textPage = nullptr;
		uint16_t sentenceCount = 0;

		fz_try(m_context)
		{
			page = fz_load_page(m_context, m_document, pageNo);
			fz_rect pageBounds = fz_bound_page(m_context, page);
			float pageHeight = pageBounds.y1 - pageBounds.y0;
			float topMargin = pageBounds.y0 + pageHeight * TOP_MARGIN;
			float bottomMargin = pageBounds.y1 - pageHeight * BOTTOM_MARGIN;

			fz_stext_options opts = { 0 };
			textPage = fz_new_stext_page_from_page(m_context, page, &opts);

			countBlockContents(textPage->first_block, sentenceCount, topMargin, bottomMargin);
		}
		fz_always(m_context)
		{
			fz_drop_stext_page(m_context, textPage);
			fz_drop_page(m_context, page);
		}
		fz_catch(m_context)
		{
			// Failed page
		}
		sentenceCounts.append(sentenceCount);
	}
}

void PDFParser::countBlockContents(fz_stext_block *block, uint16_t &sentenceCount, float topMargin,
								   float bottomMargin)
{
	QList<TextList> pageLines;
	TextList blockLines;

	for (fz_stext_block *b = block; b; b = b->next) {
		switch (b->type) {
		case FZ_STEXT_BLOCK_TEXT:
			if (b->bbox.y1 <= topMargin || b->bbox.y0 >= bottomMargin)
				continue;
			pageLines.append(getBlockTextLines(b));
			break;
		default:
			continue;
		}
	}

	TextList sentences = getPageSentences(pageLines);
	sentenceCount += sentences.strList.count();
}

TextList PDFParser::getBlockTextLines(fz_stext_block *block)
{
	TextList textLines;
	for (fz_stext_line *line = block->u.t.first_line; line; line = line->next) {
		QString lineText;
		for (fz_stext_char *ch = line->first_char; ch; ch = ch->next)
			lineText += QChar(ch->c);

		lineText = lineText.trimmed();
		if (lineText.isEmpty())
			continue;

		textLines.strList.append(lineText);
		textLines.yTopList.append(line->bbox.y0);
		textLines.yBottomList.append(line->bbox.y1);
	}

	// Add a period at the end of the block
	if (!textLines.strList.isEmpty() && !textLines.strList.last().endsWith('.'))
		textLines.strList.last().append('.');

	return textLines;
}

QList<OffsetPiece> splitWithOffsets(const QString &text, const QRegularExpression &re)
{
	QList<OffsetPiece> pieces;
	int lastEnd = 0;

	QRegularExpressionMatchIterator it = re.globalMatch(text);
	while (it.hasNext()) {
		QRegularExpressionMatch match = it.next();
		int matchStart = match.capturedStart();
		int matchEnd = match.capturedEnd();

		if (matchStart > lastEnd)
			pieces.append({ text.mid(lastEnd, matchStart - lastEnd), lastEnd, matchStart });

		lastEnd = matchEnd;
	}

	if (lastEnd < text.length())
		pieces.append({ text.mid(lastEnd), lastEnd, static_cast<int>(text.length()) });

	return pieces;
}

TextList PDFParser::getPageSentences(QList<TextList> &textBlockLines)
{
	TextList sentences;
	if (textBlockLines.isEmpty())
		return sentences;

	constexpr int MIN_WORDS = 15;
	constexpr int MAX_WORDS = 30;

	static const QRegularExpression cleanupRe(R"(\s+|[\x{FFFD}"\x{201C}\x{201D}]+)");
	static const QRegularExpression punctConflictRe(R"(([?!;])\s*[.]+|(?:[.]\s*)+([?!;]))");
	static const QRegularExpression multiPeriodRe(R"((?:\.\s*){2,})");
	static const QRegularExpression spaceBeforePunctRe(R"(\s+([.,!?;:]))");

	static const QRegularExpression sentenceSplitRe(
		R"((?<!\b[A-Z][a-z]{0,3}\.)(?<!\bi\.e\.)(?<!\be\.g\.)(?<!\betc\.)(?<!\b\d{1,3}\.)(?<=[.?!;\-\x{2013}\x{2014}])\s+|\x{2028})");
	static const QRegularExpression abbrevDotRe(R"(\b[A-Z][a-z]{0,3}\.|\bi\.e\.|\be\.g\.|\betc\.)");

	// Zero-allocation, character-preserving dot stripper
	auto stripAbbreviationDots = [](const QString &text) -> QString {
		QRegularExpressionMatchIterator it = abbrevDotRe.globalMatch(text);
		if (!it.hasNext())
			return text; // Fast-path: return unchanged copy/ref

		QString result;
		result.reserve(text.size());
		int lastEnd = 0;
		while (it.hasNext()) {
			const QRegularExpressionMatch m = it.next();
			result.append(QStringView(text).mid(lastEnd, m.capturedStart() - lastEnd));

			// Append matched token without '.'
			QStringView matched = m.capturedView(0);
			for (const QChar c : matched) {
				if (c != u'.')
					result.append(c);
			}
			lastEnd = m.capturedEnd();
		}
		result.append(QStringView(text).mid(lastEnd));
		return result;
	};

	// Zero-allocation word counter (O(N) character scan)
	auto countWords = [](QStringView s) -> size_t {
		size_t count = 0;
		bool inWord = false;
		for (const QChar c : s) {
			if (c.isSpace()) {
				inWord = false;
			} else if (!inWord) {
				inWord = true;
				++count;
			}
		}
		return count;
	};

	auto endsWithSentenceTerminator = [](QStringView s) -> bool {
		if (s.isEmpty())
			return false;
		const QChar c = s.back();
		return c == u'.' || c == u'!' || c == u'?' || c == u';';
	};

	struct RawSentence {
		QString text;
		float yTop;
		float yBottom;
		int blockIdx;
	};

	QList<RawSentence> rawList;

	for (int blockIdx = 0; blockIdx < textBlockLines.size(); ++blockIdx) {
		const TextList &block = textBlockLines[blockIdx];
		const qsizetype lineCount = block.strList.size();
		if (lineCount == 0)
			continue;

		struct LineSpan {
			int start;
			int end;
			float yTop;
			float yBottom;
		};

		QString joined;
		// Estimate average line length to reduce heap growth cycles
		joined.reserve(lineCount * 60);

		QList<LineSpan> lineSpans;
		lineSpans.reserve(lineCount);

		const auto &yTopList = block.yTopList;
		const auto &yBottomList = block.yBottomList;

		for (qsizetype i = 0; i < lineCount; ++i) {
			QString line = block.strList[i];

			// In-place regex cleanups
			line.replace(cleanupRe, QStringLiteral(" "));
			line.replace(punctConflictRe, QStringLiteral("\\1\\2"));
			line.replace(multiPeriodRe, QStringLiteral("."));
			line.replace(spaceBeforePunctRe, QStringLiteral("\\1"));

			line = line.trimmed();
			line = stripAbbreviationDots(line);
			if (line.isEmpty())
				continue;

			const bool isHyphenated = line.endsWith(u'-');
			if (isHyphenated)
				line.chop(1);

			const int start = joined.length();
			joined.append(line);
			const int end = joined.length();

			const float yTop = (i < yTopList.size()) ? yTopList[i] : 0.0f;
			const float yBottom = (i < yBottomList.size()) ? yBottomList[i] : 0.0f;
			lineSpans.append({ start, end, yTop, yBottom });

			const bool isLastLine = (i == lineCount - 1);
			if (!isLastLine && !isHyphenated)
				joined.append(u' ');
		}

		if (joined.isEmpty())
			continue;

		const QList<OffsetPiece> pieces = splitWithOffsets(joined, sentenceSplitRe);

		for (const OffsetPiece &piece : pieces) {
			QStringView textSpan = QStringView(piece.text).trimmed();
			if (textSpan.isEmpty())
				continue;

			float yTop = 0.0f;
			float yBottom = 0.0f;
			bool foundAny = false;

			for (const LineSpan &span : lineSpans) {
				if (span.start < piece.end && span.end > piece.start) {
					if (!foundAny) {
						yTop = span.yTop;
						yBottom = span.yBottom;
						foundAny = true;
					} else {
						yTop = std::min(yTop, span.yTop);
						yBottom = std::max(yBottom, span.yBottom);
					}
				}
			}

			rawList.append({ textSpan.toString(), yTop, yBottom, blockIdx });
		}
	}

	if (rawList.isEmpty())
		return sentences;

	// Pre-reserve sentence list buffers
	sentences.strList.reserve(rawList.size());
	sentences.yTopList.reserve(rawList.size());
	sentences.yBottomList.reserve(rawList.size());

	QString currentText;
	currentText.reserve(256);
	float currentYTop = 0.0f;
	float currentYBottom = 0.0f;
	size_t currentWordCount = 0;
	int currentBlockIdx = -1;
	bool hasCurrent = false;

	auto flushCurrentGroup = [&]() {
		if (!hasCurrent)
			return;

		QString finalText = currentText.trimmed();

		if (!finalText.isEmpty() && !endsWithSentenceTerminator(finalText))
			finalText.append(u'.');

		sentences.strList.append(std::move(finalText));
		sentences.yTopList.append(currentYTop);
		sentences.yBottomList.append(currentYBottom);

		hasCurrent = false;
		currentText.clear();
		currentWordCount = 0;
	};

	for (const RawSentence &raw : rawList) {
		const size_t words = countWords(raw.text);

		if (!hasCurrent) {
			currentText = raw.text;
			currentYTop = raw.yTop;
			currentYBottom = raw.yBottom;
			currentWordCount = words;
			currentBlockIdx = raw.blockIdx;
			hasCurrent = true;
			continue;
		}

		const bool enteringNewBlock = (raw.blockIdx != currentBlockIdx);
		const bool preferBreakHere = enteringNewBlock && currentWordCount >= MIN_WORDS;
		const bool wouldExceedMax = (currentWordCount >= MIN_WORDS) &&
									(currentWordCount + words > MAX_WORDS);

		if (preferBreakHere || wouldExceedMax) {
			flushCurrentGroup();
			currentText = raw.text;
			currentYTop = raw.yTop;
			currentYBottom = raw.yBottom;
			currentWordCount = words;
			currentBlockIdx = raw.blockIdx;
			hasCurrent = true;
		} else {
			currentText.append(u' ');
			currentText.append(raw.text);
			currentYTop = std::min(currentYTop, raw.yTop);
			currentYBottom = std::max(currentYBottom, raw.yBottom);
			currentWordCount += words;
			currentBlockIdx = raw.blockIdx;
		}
	}

	flushCurrentGroup();

	return sentences;
}

void PDFParser::extractPageContents(int pageNumber, uint8_t genId)
{
	fz_page *page = nullptr;
	fz_stext_page *textPage = nullptr;
	TextList sentences;
	ImageList images;

	fz_try(m_context)
	{
		page = fz_load_page(m_context, m_document, pageNumber);
		fz_rect pageBounds = fz_bound_page(m_context, page);
		float pageHeight = pageBounds.y1 - pageBounds.y0;
		float topMargin = pageBounds.y0 + pageHeight * TOP_MARGIN;
		float bottomMargin = pageBounds.y1 - pageHeight * BOTTOM_MARGIN;
		float minImageSize = pageHeight * MIN_IMAGE_SIZE;

		fz_stext_options opts = { 0 };
		opts.flags |= FZ_STEXT_PRESERVE_IMAGES;
		textPage = fz_new_stext_page_from_page(m_context, page, &opts);

		extractBlockContents(textPage->first_block, sentences, images, topMargin, bottomMargin,
							 minImageSize);
	}
	fz_always(m_context)
	{
		fz_drop_stext_page(m_context, textPage);
		fz_drop_page(m_context, page);
	}
	fz_catch(m_context)
	{
		const char *errMsg = fz_caught_message(m_context);
		QString errorString = QString::fromUtf8(errMsg);

		emit pageExtractionFailed(pageNumber, errorString, genId);
	}

#ifdef TESTING
	saveImagesToTestDirectory(images, pageNumber);
#endif

	// Get image ranges
	QList<PlaybackSegment> segments;
	getPlaybackSegments(sentences, images, segments);

	emit pageExtracted(pageNumber, sentences.strList, images.imageList, segments, genId);
}

void PDFParser::extractBlockContents(fz_stext_block *block, TextList &sentences, ImageList &images,
									 float topMargin, float bottomMargin, float minImageSize)
{
	QList<TextList> pageLines;
	ImageList blockImage;

	for (fz_stext_block *b = block; b; b = b->next) {
		switch (b->type) {
		case FZ_STEXT_BLOCK_TEXT:
			if (b->bbox.y1 <= topMargin || b->bbox.y0 >= bottomMargin)
				continue;
			pageLines.append(getBlockTextLines(b));
			break;
		case FZ_STEXT_BLOCK_IMAGE: {
			float imgWidth = b->bbox.x1 - b->bbox.x0;
			float imgHeight = b->bbox.y1 - b->bbox.y0;

			if (imgWidth <= minImageSize || imgHeight <= minImageSize)
				continue;

			blockImage = getBlockImage(b);
			images.imageList += blockImage.imageList;
			images.yTopList += blockImage.yTopList;
			images.yBottomList += blockImage.yBottomList;
			break;
		}
		default:
			continue;
		}
	}

	sentences = getPageSentences(pageLines);
}

ImageList PDFParser::getBlockImage(fz_stext_block *block)
{
	fz_pixmap *pix = NULL;
	fz_pixmap *rgb_pix = NULL;
	fz_image *image = block->u.i.image;
	ImageList result;
	result.yTopList.append(block->bbox.y0);
	result.yBottomList.append(block->bbox.y1);

	fz_try(m_context)
	{
		pix = fz_get_pixmap_from_image(m_context, image, NULL, NULL, NULL, NULL);
		if (!pix)
			fz_throw(m_context, FZ_ERROR_GENERIC, "no pixmap");

		fz_drop_colorspace(m_context, pix->colorspace);
		pix->colorspace = NULL;

		int n = fz_pixmap_components(m_context, pix);

		if (n == 1 || n == 2) {
			int w = fz_pixmap_width(m_context, pix);
			int h = fz_pixmap_height(m_context, pix);
			int has_alpha = (n == 2) ? 1 : 0;

			rgb_pix = fz_new_pixmap(m_context, fz_device_rgb(m_context), w, h, NULL, has_alpha);
			unsigned char *src = fz_pixmap_samples(m_context, pix);
			unsigned char *dst = fz_pixmap_samples(m_context, rgb_pix);

			for (size_t i = 0; i < w * h; i++) {
				unsigned char gray = src[i * n];
				dst[i * 3] = gray; // R
				dst[i * 3 + 1] = gray; // G
				dst[i * 3 + 2] = gray; // B
			}

			fz_drop_pixmap(m_context, pix);
			pix = rgb_pix;
			rgb_pix = NULL;
		} else if (n != 3 && n != 4) {
			fz_color_params no_icc = fz_default_color_params;

			rgb_pix =
				fz_convert_pixmap(m_context, pix, fz_device_rgb(m_context), NULL, NULL, no_icc, 0);
			fz_drop_pixmap(m_context, pix);
			pix = rgb_pix;
			rgb_pix = NULL;
		}

		int width = fz_pixmap_width(m_context, pix);
		int height = fz_pixmap_height(m_context, pix);
		int stride = fz_pixmap_stride(m_context, pix);
		unsigned char *samples = fz_pixmap_samples(m_context, pix);
		n = fz_pixmap_components(m_context, pix);

		if (width > 0 && height > 0 && samples && (n == 3 || n == 4)) {
			QImage::Format format = (n == 4) ? QImage::Format_RGBA8888 : QImage::Format_RGB888;
			QImage qimg(samples, width, height, stride, format);
			result.imageList.append(qimg.copy());
		}
	}
	fz_always(m_context)
	{
		fz_drop_pixmap(m_context, rgb_pix);
		fz_drop_pixmap(m_context, pix);
	}
	fz_catch(m_context)
	{
		// Skip
	}

	return result;
}

void PDFParser::getPlaybackSegments(TextList &sentences, ImageList &images,
									QList<PlaybackSegment> &segments)
{
	segments.clear();

	int sentenceIdx = 0;
	int imageIdx = 0;
	int sentencesLen = (int)sentences.strList.size();
	int imagesLen = (int)images.imageList.size();

	while (sentenceIdx < sentencesLen && imageIdx < imagesLen) {
		PlaybackSegment segment = { .firstSentenceIdx = -1, .lastSentenceIdx = -1, .imageIdx = -1 };
		if (images.yBottomList[imageIdx] <= sentences.yTopList[sentenceIdx]) {
			segment.imageIdx = imageIdx;
			imageIdx++;
		} else if (sentences.yBottomList[sentenceIdx] <= images.yTopList[imageIdx]) {
			segment.firstSentenceIdx = sentenceIdx;
			while (sentenceIdx < sentencesLen &&
				   sentences.yBottomList[sentenceIdx] <= images.yTopList[imageIdx]) {
				segment.lastSentenceIdx = sentenceIdx;
				sentenceIdx++;
			}
		} else {
			segment.firstSentenceIdx = sentenceIdx;
			segment.imageIdx = imageIdx;
			while (sentenceIdx < sentencesLen &&
				   images.yBottomList[imageIdx] > sentences.yTopList[sentenceIdx] &&
				   sentences.yBottomList[sentenceIdx] > images.yTopList[imageIdx]) {
				segment.lastSentenceIdx = sentenceIdx;
				sentenceIdx++;
			}
		}
		segments.append(segment);
	}

	// Add the remaining ones
	if (sentenceIdx < sentencesLen) {
		segments.append(PlaybackSegment{ .firstSentenceIdx = sentenceIdx,
										 .lastSentenceIdx = sentencesLen - 1 });
	} else {
		for (int i = imageIdx; i < imagesLen; i++) {
			segments.append(PlaybackSegment{ .imageIdx = i });
		}
	}
}

#ifdef TESTING
void PDFParser::saveImagesToTestDirectory(const ImageList &images, int pageNumber)
{
	QString testDir = "/home/zeta/Documents/test_images";

	// Create directory if it doesn't exist
	QDir dir;
	if (!dir.exists(testDir)) {
		dir.mkpath(testDir);
	}

	for (int i = 0; i < images.imageList.size(); i++) {
		QString filename = QString("%1/page%2_img%3.png").arg(testDir).arg(pageNumber).arg(i);

		if (images.imageList[i].save(filename))
			printf("Saved test image: %s\n", filename.toUtf8().constData());
		else
			fprintf(stderr, "Failed to save test image: %s\n", filename.toUtf8().constData());
	}
}
#endif
