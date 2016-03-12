/************************************************************************
 *									*
 *  This file is part of Kooka, a scanning/OCR application using	*
 *  Qt <http://www.qt.io> and KDE Frameworks <http://www.kde.org>.	*
 *									*
 *  Copyright (C) 2000-2016 Klaas Freitag <freitag@suse.de>		*
 *                          Jonathan Marten <jjm@keelhaul.me.uk>	*
 *									*
 *  Kooka is free software; you can redistribute it and/or modify it	*
 *  under the terms of the GNU Library General Public License as	*
 *  published by the Free Software Foundation and appearing in the	*
 *  file COPYING included in the packaging of this file;  either	*
 *  version 2 of the License, or (at your option) any later version.	*
 *									*
 *  As a special exception, permission is given to link this program	*
 *  with any version of the KADMOS OCR/ICR engine (a product of		*
 *  reRecognition GmbH, Kreuzlingen), and distribute the resulting	*
 *  executable without including the source code for KADMOS in the	*
 *  source distribution.						*
 *									*
 *  This program is distributed in the hope that it will be useful,	*
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of	*
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the	*
 *  GNU General Public License for more details.			*
 *									*
 *  You should have received a copy of the GNU General Public		*
 *  License along with this program;  see the file COPYING.  If		*
 *  not, see <http://www.gnu.org/licenses/>.				*
 *									*
 ************************************************************************/

#ifndef OCRENGINE_H
#define OCRENGINE_H

#include <qobject.h>
#include <qtextformat.h>

#include "kookaimage.h"

/**
  *@author Klaas Freitag
  */

class QRect;
class QStringList;
class QImage;
class QTextDocument;

class KProcess;

class KookaImage;
class ImageCanvas;
class OcrBaseDialog;

// Using a QTextDocument for the OCR results, with the source image rectangle
// and other OCR information held as text properties.

class OcrWordData : public QTextCharFormat
{
public:
    enum DataType {
        Rectangle = QTextFormat::UserProperty,      // QRect
        Alternatives,                   // QStringList
        KNode                       // int
    };

    OcrWordData() : QTextCharFormat()   {}
};

class OcrEngine : public QObject
{
    Q_OBJECT

public:
    enum EngineError {                  // OCR Engine setup errors
        ENG_ERROR,
        ENG_OK,
        ENG_DATA_MISSING,
        ENG_BAD_SETUP
    };

    enum EngineType {               // OCR Engine configured type
        EngineNone,
        EngineGocr,
        EngineOcrad,
        EngineKadmos
    };

    OcrEngine(QWidget *parent);
    virtual ~OcrEngine();

    bool startOCRVisible(QWidget *parent = NULL);

    /**
     * Sets an image Canvas that displays the result image of OCR. If this
     * is set to NULL (or never set) no result image is displayed.
     * The OCR fabric passes a new image to the canvas which is a copy of
     * the image to OCR.
     */
    void setImageCanvas(ImageCanvas *canvas);
    void setImage(const KookaImage img);
    void setTextDocument(QTextDocument *doc);

    static const QString engineName(OcrEngine::EngineType eng);
    static OcrEngine *createEngine(QWidget *parent, bool *gotoPrefs = NULL);
    bool engineValid() const;

public slots:
    void slotHighlightWord(const QRect &r);
    void slotScrollToWord(const QRect &r);

signals:
    void newOCRResultText();

    /**
     * progress of the ocr process. The first integer is the main progress,
     * the second the sub progress. If there is only on progress, it is the
     * first parameter, the second is always -1 than.
     * Both have a range from 0..100.
     * Note that this signal may not be emitted if the engine does not support
     * progress.
     */
    void ocrProgress(int progress, int subprogress);

    /**
     * Select a word in the editor corresponding to the position within
     * the result image.
     */
    void selectWord(const QPoint &p);

    /**
     * indicates that the text editor holding the text that came through
     * newOCRResultText should be set to readonly or not. Can be connected
     * to QTextEdit::setReadOnly directly.
     */
    void readOnlyEditor(bool isReadOnly);

    void setSpellCheckConfig(const QString &configFile);
    void startSpellCheck(bool interactive, bool background);

protected:
    void finishedOCRVisible(bool success);
    virtual OcrBaseDialog *createOCRDialog(QWidget *parent) = 0;

    virtual QStringList tempFiles(bool retain) = 0;

    virtual OcrEngine::EngineType engineType() const = 0;

    QTextDocument *startResultDocument();
    void finishResultDocument();

    void startLine();
    void addWord(const QString &word, const OcrWordData &data);
    void finishLine();

protected:
    KProcess *m_ocrProcess;
    QString m_ocrResultFile;
    QString m_ocrResultText;
    QWidget *m_parent;
    // one block contains all lines of the page
protected slots:
    void slotClose();
    void slotStopOCR();

    /**
     * Handle mouse double clicks on the image viewer showing the
     * OCR result image.
     */
    void slotImagePosition(const QPoint &p);

private:
    virtual void startProcess(OcrBaseDialog *dia, const KookaImage *img) = 0;

    void stopOCRProcess(bool tellUser);
    void removeTempFiles();

private slots:
    void startOCRProcess();

private:
    OcrBaseDialog *m_ocrDialog;
    bool m_ocrRunning;

    KookaImage m_introducedImage;
    QImage *m_resultImage;
    ImageCanvas *m_imgCanvas;

    int m_currHighlight;
    bool m_trackingActive;

    QTextDocument *m_document;
    QTextCursor *m_cursor;
    int m_wordCount;
};

#endif                          // OCRENGINE_H
