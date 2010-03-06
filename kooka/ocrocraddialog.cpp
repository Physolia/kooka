/***************************************************************************
                          kocrocrad.cpp  - ocrad dialog
                             -------------------
    begin                : Tue Jul 15 2003
    copyright            : (C) 2003 by Klaas Freitag
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

#include "ocrocraddialog.h"
#include "ocrocraddialog.moc"

#include <qlabel.h>
#include <qregexp.h>
#include <qcombobox.h>
#include <qlayout.h>
#include <qprogressbar.h>

#include <kglobal.h>
#include <kdebug.h>
#include <klocale.h>
#include <kanimatedbutton.h>
#include <kseparator.h>
#include <kmessagebox.h>
#include <kurlrequester.h>
#include <kprocess.h>
#include <kvbox.h>
#include <khbox.h>

#include "kookaimage.h"
#include "kookapref.h"

#include "ocrocradengine.h"


OcrOcradDialog::OcrOcradDialog(QWidget *parent)
    : OcrBaseDialog(parent),
      m_setupWidget(NULL),
      m_orfUrlRequester(NULL),
      m_layoutMode(0),
      m_ocrCmd(QString::null),
      m_version(0)
{
}


OcrOcradDialog::~OcrOcradDialog()
{
}


QString OcrOcradDialog::ocrEngineLogo() const
{
    return ("ocrad.png");
}


QString OcrOcradDialog::ocrEngineName() const
{
    return (OcrEngine::engineName(OcrEngine::EngineOcrad));
}


QString OcrOcradDialog::ocrEngineDesc() const
{
    return (OcrOcradEngine::engineDesc());
}


// TODO: support for other OCRAD options
// --charset, --filter, --invert, --format, --transform

OcrEngine::EngineError OcrOcradDialog::setupGui()
{
    OcrBaseDialog::setupGui();

    QWidget *w = addExtraSetupWidget();
    QGridLayout *gl = new QGridLayout(w);

    /* layout detection mode combo */
    KConfigGroup grp1 = KGlobal::config()->group(CFG_GROUP_OCRAD);
    int layoutDetect = grp1.readEntry(CFG_OCRAD_LAYOUT_DETECTION, 0);

    // TODO: buddy
    gl->addWidget(new QLabel(i18n("Layout analysis mode:"), w), 0, 0);

    m_layoutMode = new QComboBox(w);
    m_layoutMode->addItem(i18n("No Layout Detection"), 0);
    m_layoutMode->addItem(i18n("Column Detection"), 1);
    m_layoutMode->addItem(i18n("Full Layout Detection"), 2);
    m_layoutMode->setCurrentIndex(layoutDetect);
    gl->addWidget(m_layoutMode, 0, 1, Qt::AlignLeft);

    gl->setRowStretch(1, 1);				// for top alignment
    gl->setColumnStretch(1, 1);

    /* find the OCRAD binary */
    KConfigGroup grp2 = KGlobal::config()->group(CFG_GROUP_OCR_DIA);
    QString res = grp2.readPathEntry(CFG_OCRAD_BINARY, "");
    if (res.isEmpty())
    {
        res = KookaPref::tryFindOcrad();
        if (res.isEmpty())
        {
            /* Popup here telling that the config needs to be called */
            KMessageBox::sorry(this,i18n("The path to the OCRAD binary is not configured or is not valid.\n"
                                         "Please enter or check the path in the Kooka configuration."),
                               i18n("OCRAD Software Not Found"));
            enableButton(KDialog::User1, false);
        }
    }

    /* retrieve program version and display */
    if (res.isEmpty()) res = i18n("Not found");
    else m_ocrCmd = res;
    ocrShowInfo(res, version());			// show binary, get/show version

    progressBar()->setMaximum(0);			// animation only

    m_setupWidget = w;
    return (OcrEngine::ENG_OK);
}


void OcrOcradDialog::slotWriteConfig()
{
    kDebug();

    OcrBaseDialog::slotWriteConfig();

    KConfigGroup grp1 = KGlobal::config()->group(CFG_GROUP_OCR_DIA);
    grp1.writePathEntry(CFG_OCRAD_BINARY, getOCRCmd());

    KConfigGroup grp2 = KGlobal::config()->group(CFG_GROUP_OCRAD);
    grp2.writeEntry(CFG_OCRAD_LAYOUT_DETECTION,layoutDetectionMode());
}


void OcrOcradDialog::enableFields(bool enable)
{
    m_setupWidget->setEnabled(enable);
}


int OcrOcradDialog::layoutDetectionMode() const
{
    return (m_layoutMode->currentIndex());
}


/* Later: Allow interactive loading of orf files
 *  for now, return emty string
 */
QString OcrOcradDialog::orfUrl() const
{
    if (m_orfUrlRequester!=NULL) return (m_orfUrlRequester->url().url());
    else return (QString::null);
}


QString OcrOcradDialog::version()
{
    kDebug() << "of" << m_ocrCmd;
    if (m_ocrCmd.isEmpty()) return (QString::null);

    QString vers;

    KProcess proc;
    proc.setOutputChannelMode(KProcess::MergedChannels);
    proc << m_ocrCmd << "-V";

    int status = proc.execute(5000);
    if (status==0)
    {
        QByteArray output = proc.readAllStandardOutput();
        QRegExp rx("GNU Ocrad (version )?([\\d\\.]+)");
        if (rx.indexIn(output)>-1)
        {
            vers = rx.cap(2);
            m_version = vers.mid(2).toInt();
        }
        else vers = i18n("Unknown");
    }
    else
    {
        kDebug() << "failed with status" << status;
        vers = i18n("Error");
    }

    return (vers);
}
