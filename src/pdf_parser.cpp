#include "pdf_parser.h"
#include <QString>
#include <QByteArray>
#include <QRegularExpression>
#include <QDir>

// ===== Custom device for image extraction =====
typedef struct
{
    fz_device super;
    uint16_t count;
} image_count_device;

typedef struct
{
    fz_device super;
    fz_context *ctx;
    QVector<QImage> *images;
} image_extract_device;

static void image_count_fill_image(fz_context *ctx, fz_device *dev_,
    fz_image *image, fz_matrix ctm, float alpha, fz_color_params cp)
{
    ((image_count_device *)dev_)->count++;
}

static fz_device *fz_new_image_count_device(fz_context *ctx)
{
    image_count_device *dev = fz_new_derived_device(ctx, image_count_device);
    dev->super.fill_image = image_count_fill_image;
    dev->count = 0;
    return (fz_device *)dev;
}

static void image_extract_fill_image(fz_context *ctx, fz_device *dev_, fz_image *image, fz_matrix ctm,
                                     float alpha, fz_color_params color_params)
{
    image_extract_device *dev = (image_extract_device *)dev_;
    fz_pixmap *pix = NULL;
    fz_pixmap *rgb_pix = NULL;

    fz_try(ctx)
    {
        pix = fz_get_pixmap_from_image(ctx, image, NULL, NULL, NULL, NULL);
        if (!pix)
            fz_throw(ctx, FZ_ERROR_GENERIC, "no pixmap");

        fz_drop_colorspace(ctx, pix->colorspace);
        pix->colorspace = NULL;

        int n = fz_pixmap_components(ctx, pix);

        if (n == 1 || n == 2)
        {
            int w = fz_pixmap_width(ctx, pix);
            int h = fz_pixmap_height(ctx, pix);
            int has_alpha = (n == 2) ? 1 : 0;

            rgb_pix = fz_new_pixmap(ctx, fz_device_rgb(ctx), w, h, NULL, has_alpha);
            unsigned char *src = fz_pixmap_samples(ctx, pix);
            unsigned char *dst = fz_pixmap_samples(ctx, rgb_pix);

            for (int i = 0; i < w * h; i++)
            {
                unsigned char gray = src[i * n];
                dst[i * 3] = gray;      // R
                dst[i * 3 + 1] = gray;  // G
                dst[i * 3 + 2] = gray;  // B
            }

            fz_drop_pixmap(ctx, pix);
            pix = rgb_pix;
            rgb_pix = NULL;
        }
        else if (n != 3 && n != 4)
        {
            fz_color_params no_icc = fz_default_color_params;

            rgb_pix = fz_convert_pixmap(ctx, pix, fz_device_rgb(ctx), NULL, NULL, no_icc, 0);
            fz_drop_pixmap(ctx, pix);
            pix = rgb_pix;
            rgb_pix = NULL;
        }

        int width = fz_pixmap_width(ctx, pix);
        int height = fz_pixmap_height(ctx, pix);
        int stride = fz_pixmap_stride(ctx, pix);
        unsigned char *samples = fz_pixmap_samples(ctx, pix);
        n = fz_pixmap_components(ctx, pix);

        if (width > 0 && height > 0 && samples && (n == 3 || n == 4))
        {
            QImage::Format format = (n == 4) ? QImage::Format_RGBA8888 : QImage::Format_RGB888;
            QImage qimg(samples, width, height, stride, format);
            QImage copied = qimg.copy();

            if (!copied.isNull())
            {
                dev->images->append(copied);
            }
        }
    }
    fz_always(ctx)
    {
        fz_drop_pixmap(ctx, rgb_pix);
        fz_drop_pixmap(ctx, pix);
    }
    fz_catch(ctx)
    {
        // Silently skip
    }
}

// Don't extract masks - they're just duplicates
static void image_extract_fill_image_mask(fz_context *ctx, fz_device *dev_, fz_image *image,
                                          fz_matrix ctm, fz_colorspace *colorspace, const float *color,
                                          float alpha, fz_color_params color_params)
{
    // Skip masks
}

static void image_extract_clip_image_mask(fz_context *ctx, fz_device *dev_, fz_image *image,
                                          fz_matrix ctm, fz_rect scissor)
{
    // Skip clipping masks
}

static void image_extract_drop_device(fz_context *ctx, fz_device *dev_)
{
    // Cleanup if needed
}

static fz_device *fz_new_image_extract_device(fz_context *ctx, QVector<QImage> *images)
{
    image_extract_device *dev = fz_new_derived_device(ctx, image_extract_device);

    dev->super.drop_device = image_extract_drop_device;
    dev->super.fill_image = image_extract_fill_image;
    dev->super.fill_image_mask = image_extract_fill_image_mask;
    dev->super.clip_image_mask = image_extract_clip_image_mask;

    dev->ctx = ctx;
    dev->images = images;

    return (fz_device *)dev;
}

PDFParser::PDFParser(QObject *parent)
    : QObject(parent), m_context(nullptr), m_document(nullptr), m_pageCount(0)
{
    m_context = fz_new_context(nullptr, nullptr, FZ_STORE_UNLIMITED);

    if (m_context)
    {
        fz_disable_icc(m_context);
    }
}

PDFParser::~PDFParser()
{
    closePdf();
    if (m_context)
    {
        fz_drop_context(m_context);
        m_context = nullptr;
    }
}

void PDFParser::loadPdf(const QString &filePath)
{
    closePdf();

    fz_try(m_context)
    {
        fz_register_document_handlers(m_context);
    }
    fz_catch(m_context)
    {
        QString error = QString("Failed to register handlers: %1").arg(fz_caught_message(m_context));
        emit pdfLoadFailed(error);
        return;
    }

    fz_try(m_context)
    {
        m_document = fz_open_document(m_context, filePath.toUtf8().constData());
    }
    fz_catch(m_context)
    {
        QString error = QString("Failed to open PDF: %1").arg(fz_caught_message(m_context));
        emit pdfLoadFailed(error);
        return;
    }

    fz_try(m_context)
    {
        m_pageCount = fz_count_pages(m_context, m_document);
    }
    fz_catch(m_context)
    {
        QString error = QString("Failed to count document pages: %1").arg(fz_caught_message(m_context));
        emit pdfLoadFailed(error);
        return;
    }

    m_filePath = filePath;
    m_incompleteSentence.clear();  // Reset incomplete sentence when loading new PDF
    emit pdfLoaded(m_pageCount);
}

void PDFParser::closePdf()
{
    if (m_document)
    {
        fz_drop_document(m_context, m_document);
        m_document = nullptr;
    }
    m_pageCount = 0;
    m_filePath.clear();
    m_incompleteSentence.clear();  // Clear incomplete sentence when closing
}

bool PDFParser::isCompleteSentence(const QString &text)
{
    QString trimmed = text.trimmed();
    if (trimmed.isEmpty())
        return true;

    // Check if the sentence ends with proper punctuation
    QChar lastChar = trimmed[trimmed.length() - 1];
    return (lastChar == '.' || lastChar == '!' || lastChar == '?');
}

#ifdef TESTING
void PDFParser::saveImagesToTestDirectory(const QVector<QImage> &images, int pageNumber)
{
    QString testDir = "/home/zeta/Documents/test_images";

    // Create directory if it doesn't exist
    QDir dir;
    if (!dir.exists(testDir))
    {
        dir.mkpath(testDir);
    }

    for (int i = 0; i < images.size(); i++)
    {
        QString filename = QString("%1/page%2_img%3.png").arg(testDir).arg(pageNumber).arg(i);

        if (images[i].save(filename))
        {
            printf("Saved test image: %s\n", filename.toUtf8().constData());
        }
        else
        {
            fprintf(stderr, "Failed to save test image: %s\n", filename.toUtf8().constData());
        }
    }
}
#endif

QVector<QString> PDFParser::extractSentences(fz_stext_page *textPage)
{
    static const QRegularExpression sentenceEnd(R"((?<=[.!?])\s+)");
    static const QRegularExpression abbreviation(R"(\b([A-Z][a-z]{0,3}|\d+)\.$)");

    auto wordCount = [](const QString &s) {
        return s.split(' ', Qt::SkipEmptyParts).size();
    };

    auto flushBuffer = [&](QVector<QString> &result, QString &buffer) {
        QString s = buffer.simplified();
        if (!s.isEmpty())
            result.append(s);
        buffer.clear();
    };

    QVector<QString> result;
    QString buffer;

    for (fz_stext_block *block = textPage->first_block; block; block = block->next)
    {
        if (block->type != FZ_STEXT_BLOCK_TEXT)
            continue;

        // Build lines preserving boundaries with a sentinel
        QString blockText;
        for (fz_stext_line *line = block->u.t.first_line; line; line = line->next)
        {
            QString lineText;
            for (fz_stext_char *ch = line->first_char; ch; ch = ch->next)
                lineText += QChar(ch->c);
            lineText = lineText.trimmed();
            if (lineText.isEmpty()) continue;

            if (lineText.endsWith('-'))
                blockText += lineText.chopped(1);      // hyphen — no separator
            else
                blockText += lineText + "\n";          // preserve line boundary
        }

        QStringList lines = blockText.split('\n', Qt::SkipEmptyParts);
        QStringList parts;

        for (const QString &line : lines)
        {
            QStringList sentenceParts = line.simplified().split(sentenceEnd);
            for (int i = 0; i < sentenceParts.size(); i++)
            {
                if (i == sentenceParts.size() - 1)
                    parts.append(sentenceParts[i].trimmed() + "\n");
                else
                    parts.append(sentenceParts[i].trimmed());
            }
        }

        for (const QString &part : parts)
        {
            bool isLineEnd = part.endsWith('\n');
            QString text = part.trimmed();
            if (text.isEmpty()) continue;

            buffer += (buffer.isEmpty() ? "" : " ") + text;

            bool isAbbreviation = abbreviation.match(buffer.trimmed()).hasMatch();
            bool longEnough     = wordCount(buffer) > 7;
            bool tooLong        = wordCount(buffer) >= 50;

            if (tooLong)
            {
                flushBuffer(result, buffer);
            }
            else if (longEnough && !isAbbreviation && !isLineEnd)
            {
                flushBuffer(result, buffer);
            }
        }
    }

    if (!buffer.simplified().isEmpty())
    {
        if (wordCount(buffer) <= 7 && !result.isEmpty())
            result.last() += " " + buffer.simplified();
        else
            result.append(buffer.simplified());
    }

    return result;
}

void PDFParser::extractPdfStructure()
{
    QVector<PageIndex> structure;
    structure.reserve(m_pageCount);

    for (int pageNo = 0; pageNo < m_pageCount; pageNo++)
    {
        fz_page *page = nullptr;
        fz_stext_page *textPage = nullptr;
        fz_display_list *list = nullptr;
        fz_device *dev = nullptr;
        PageIndex index;

        fz_try(m_context)
        {
            page = fz_load_page(m_context, m_document, pageNo);

            // Sentence count
            fz_stext_options opts = {0};
            textPage = fz_new_stext_page_from_page(m_context, page, &opts);
            index.sentenceCount = static_cast<uint16_t>(extractSentences(textPage).size());

            // Image count — build display list then run counter device
            fz_rect bounds = fz_bound_page(m_context, page);
            list = fz_new_display_list(m_context, bounds);
            dev = fz_new_list_device(m_context, list);
            fz_run_page(m_context, page, dev, fz_identity, nullptr);
            fz_close_device(m_context, dev);
            fz_drop_device(m_context, dev);
            dev = nullptr;

            image_count_device *countDev = 
                (image_count_device *)fz_new_image_count_device(m_context);
            fz_run_display_list(m_context, list, (fz_device *)countDev,
                                fz_identity, bounds, nullptr);
            fz_close_device(m_context, (fz_device *)countDev);
            index.imageCount = countDev->count;
            fz_drop_device(m_context, (fz_device *)countDev);
        }
        fz_always(m_context)
        {
            fz_drop_display_list(m_context, list);
            fz_drop_device(m_context, dev);
            fz_drop_stext_page(m_context, textPage);
            fz_drop_page(m_context, page);
        }
        fz_catch(m_context)
        {
            // Failed page — zero counts, pipeline will skip it
        }

        structure.append(index);
    }

    emit pdfStructureExtracted(structure);
}

void PDFParser::extractPageContents(int pageNumber)
{
    QVector<QString> sentences;
    QVector<QImage> images;
    fz_page *page = NULL;
    fz_stext_page *textPage = NULL;
    fz_buffer *buf = NULL;
    fz_display_list *list = NULL;
    fz_device *dev = NULL;

    fz_try(m_context)
    {
        page = fz_load_page(m_context, m_document, pageNumber);

        // Text Extraction
        fz_stext_options opts = {0};
        textPage = fz_new_stext_page_from_page(m_context, page, &opts);

        // Use structured extraction instead of raw buffer
        QVector<QString> sentences = extractSentences(textPage);

        // Prepend incomplete sentence from previous page if exists
        if (!m_incompleteSentence.isEmpty())
        {
            if (sentences.length() > 0)
            {
                sentences[0] = m_incompleteSentence + " " + sentences[0];
            }
            else
            {
                sentences.append(m_incompleteSentence);
            }
            m_incompleteSentence.clear();
        }

        // Check if the last sentence is complete
        if (!sentences.isEmpty())
        {
            QString lastSentence = sentences.last();
            if (!isCompleteSentence(lastSentence))
            {
                // Save incomplete sentence for next page
                m_incompleteSentence = lastSentence;
                sentences.removeLast();
            }
        }

        // Image Extraction
        fz_rect bounds = fz_bound_page(m_context, page);
        list = fz_new_display_list(m_context, bounds);
        dev = fz_new_list_device(m_context, list);
        fz_run_page(m_context, page, dev, fz_identity, NULL);
        fz_close_device(m_context, dev);
        fz_drop_device(m_context, dev);
        dev = NULL;

        fz_device *imgDev = fz_new_image_extract_device(m_context, &images);
        fz_run_display_list(m_context, list, imgDev, fz_identity, bounds, NULL);
        fz_close_device(m_context, imgDev);
        fz_drop_device(m_context, imgDev);

        emit pageExtracted(pageNumber, sentences, images);

#ifdef TESTING
        // Save images to test directory only when TESTING is defined
        saveImagesToTestDirectory(contents.images, pageNumber);
#endif
    }
    fz_always(m_context)
    {
        fz_drop_display_list(m_context, list);
        fz_drop_device(m_context, dev);
        fz_drop_buffer(m_context, buf);
        fz_drop_stext_page(m_context, textPage);
        fz_drop_page(m_context, page);
    }
    fz_catch(m_context)
    {
        QString error = QString("Failed to parse page no: %1 with error: %2")
                            .arg(pageNumber)
                            .arg(fz_caught_message(m_context));
        emit pageExtractionFailed(pageNumber, error);
        return;
    }
}
