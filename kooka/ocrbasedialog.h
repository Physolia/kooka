/***************************************************** -*- mode:c++; -*- ***
                  kocrbase.h  - base dialog for OCR
                             -------------------
    begin                : Sun Jun 11 2000
    copyright            : (C) 2000 by Klaas Freitag
    email                : freitag@suse.de
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *  This file may be distributed and/or modified under the terms of the    *
 *  GNU General Public License version 2 as published by the Free Software *
 *  Foundation and appearing in the file COPYING included in the           *
 *  packaging of this file.                                                *
 *
 *  As a special exception, permission is given to link this program       *
 *  with any version of the KADMOS ocr/icr engine of reRecognition GmbH,   *
 *  Kreuzlingen and distribute the resulting executable without            *
 *  including the source code for KADMOS in the source distribution.       *
 *
 *  As a special exception, permission is given to link this program       *
 *  with any edition of Qt, and distribute the resulting executable,       *
 *  without including the source code for Qt in the source distribution.   *
 *                                                                         *
 ***************************************************************************/

#ifndef OCRBASEDIALOG_H
#define OCRBASEDIALOG_H

#include <kpagedialog.h>

#include <kio/previewjob.h>

#include "ocrengine.h"

/**
  *@author Klaas Freitag
  */

class QLabel;
class QSize;
class QCheckBox;
class QGroupBox;

class KHBox;
class KVBox;
class KAnimatedButton;
class KSpellConfig;
class KPageWidgetItem;

class KookaImage;


class OcrBaseDialog: public KPageDialog
{
    Q_OBJECT

public:
    OcrBaseDialog(QWidget *parent,
                  KSpellConfig *spellConfig = NULL);
    virtual ~OcrBaseDialog();

    virtual OcrEngine::EngineError setupGui();

    /**
     * @return the name of the ocr engine
     */
    virtual QString ocrEngineName() const = 0;

    /**
     * @return the filename (without path) of the logo of the ocr engine.
     * the logo needs to be installed in $KDEDIR/share/apps/kooka/pics
     */
    virtual QString ocrEngineLogo() const = 0;

    /**
     * @return a description string of the ocr engine
     */
    virtual QString ocrEngineDesc() const = 0;

    virtual void introduceImage(const KookaImage *img);

    bool wantSpellCheck() const		{ return (m_userWantsSpellCheck); }
    KSpellConfig* spellConfig() const	{ return (m_spellConfig); }

public slots:
    void slotStopOCR();
    void slotStartOCR();

protected:
    /**
     * enable or disable dialog fields. This slot is called when the ocr process starts
     * with parameter state=false and called again if the gui should accept user input
     * again after ocr finished with parameter true.
     */
    virtual void enableFields(bool enable) = 0;

    KAnimatedButton *getAnimation(QWidget *parent);
// TODO: can be private?

    void ocrShowInfo(const QString &binary,const QString &version = QString::null);
    void ocrShowVersion(const QString &version);

    KPageWidgetItem *ocrPage() const		{ return (m_ocrPage); }

protected slots:
    virtual void slotWriteConfig();

private:
    /**
     * This creates a a tab OCR in the dialog and creates a small intro about the
     * ocr engine used.
     * It calls the virtual subs ocrEngineName, ocrEngineLogo and ocrEngineDesc which
     * must return the approbiate values for the engines.
     */
    void ocrIntro();

    /**
     * This creates a a tab Image Info in the dialog and creates a image description
     * about the current image to ocr.
     */
    void imgIntro();

    /**
     * This sets up the spellchecking configuration
     */
    void spellCheckIntro();

private slots:
    /**
     * hit if the user toggles the want-spellcheck checkbox
     */
    void slotWantSpellcheck(bool wantIt);

    void slotPreviewResult(KIO::Job *job);
    void slotGotPreview(const KFileItem *item,const QPixmap &newPix);
    void stopAnimation();
    void startAnimation();

private:
    KAnimatedButton  *m_animation;
    KPageWidgetItem        *m_ocrPage;
    KPageWidgetItem        *m_imgPage;
    KPageWidgetItem        *m_spellChkPage;
    KVBox        *m_metaBox;
    KHBox        *m_imgHBox;
    QLabel       *m_previewPix;

    KSpellConfig *m_spellConfig;
    bool          m_wantSpellCfg;         /* show the spellcheck options? */
    bool          m_userWantsSpellCheck;  /* user has enabled/disabled spellcheck */
    QSize         m_previewSize;

    QCheckBox     *m_cbWantCheck;
    QGroupBox     *m_gbSpellOpts;
    QLabel *m_lVersion;
};

#endif							// OCRBASEDIALOG_H
