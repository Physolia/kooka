/***************************************************************************
                          kocrgocr.cpp  - GOCR ocr dialog
                             -------------------
    begin                : Fri Now 10 2000
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

#include "ocrgocrdialog.h"
#include "ocrgocrdialog.moc"

#include <qlabel.h>
#include <qtooltip.h>
#include <qregexp.h>
#include <qgridlayout.h>

#include <kvbox.h>
#include <kconfig.h>
#include <kglobal.h>
#include <kdebug.h>
#include <klocale.h>
#include <kseparator.h>
#include <kmessagebox.h>
#include <kprocess.h>

#include "kookaimage.h"
#include "kookapref.h"

#include "ocrgocrengine.h"

/* defines for konfig-reading */

#define CFG_GROUP_GOCR "gocr"
#define CFG_GOCR_DUSTSIZE "gocrDustSize"
#define CFG_GOCR_GRAYLEVEL "gocrGrayLevel"
#define CFG_GOCR_SPACEWIDTH "gocrSpaceWidth"


OcrGocrDialog::OcrGocrDialog(QWidget *parent, KSpellConfig *spellConfig)
    : OcrBaseDialog(parent, spellConfig),
      m_ocrCmd(QString::null),
      m_isBW(false)
{
}


OcrGocrDialog::~OcrGocrDialog()
{
}


QString OcrGocrDialog::ocrEngineLogo() const
{
    return ("gocr.png");
}

QString OcrGocrDialog::ocrEngineName() const
{
    return (OcrEngine::engineName(OcrEngine::EngineGocr));
}

QString OcrGocrDialog::ocrEngineDesc() const
{
    return (OcrGocrEngine::engineDesc());
}


OcrEngine::EngineError OcrGocrDialog::setupGui()
{
    OcrBaseDialog::setupGui();

    KVBox *page = static_cast<KVBox *>(ocrPage()->widget());
    new KSeparator(Qt::Horizontal, page);

    KConfigGroup grp1 = KGlobal::config()->group(CFG_GROUP_GOCR);

    /* Sliders for OCR-Options */
    QGridLayout *gl = new QGridLayout(page);
    gl->setSpacing(KDialog::spacingHint());

    QLabel *l = new QLabel(i18n("Gray level:"), page);
    gl->addWidget(l, 0, 0);
    sliderGrayLevel = new KScanSlider(page, QString::null, 0, 254, true, 160 );
    int numdefault = grp1.readEntry(CFG_GOCR_GRAYLEVEL, 160);
    sliderGrayLevel->slotSetSlider(numdefault);
    sliderGrayLevel->setToolTip(i18n("The threshold value below which gray pixels are\nconsidered to be black.\n\nThe default is 160."));
    l->setBuddy(sliderGrayLevel);
    gl->addWidget(sliderGrayLevel, 0, 1);

    l = new QLabel(i18n("Dust size:"), page);
    gl->addWidget(l, 1, 0);
    sliderDustSize = new KScanSlider(page, QString::null, 0, 60, true, 10 );
    numdefault = grp1.readEntry(CFG_GOCR_DUSTSIZE, 10);
    sliderDustSize->slotSetSlider(numdefault);
    sliderDustSize->setToolTip(i18n("Clusters smaller than this value\nwill be considered to be dust, and\nremoved from the image.\n\nThe default is 10."));
    l->setBuddy(sliderDustSize);
    gl->addWidget(sliderDustSize, 1, 1);

    l = new QLabel(i18n("Space width:"), page);
    gl->addWidget(l, 2, 0);
    sliderSpace = new KScanSlider(page, QString::null, 0, 60, true, 0 );
    numdefault = grp1.readEntry(CFG_GOCR_SPACEWIDTH, 0);
    sliderSpace->slotSetSlider(numdefault);
    sliderSpace->setToolTip(i18n("Spacing between characters.\n\nThe default is 0 which means autodetection."));
    l->setBuddy(sliderSpace);
    gl->addWidget(sliderSpace, 2, 1);

    gl->setRowStretch(3, 1);

    /* find the GOCR binary */
    KConfigGroup grp2 = KGlobal::config()->group(CFG_GROUP_OCR_DIA);
    QString res = grp2.readPathEntry(CFG_GOCR_BINARY, "");
    if (res.isEmpty())
    {
        res = KookaPref::tryFindGocr();
        if (res.isEmpty())
        {
            /* Popup here telling that the config needs to be called */
            KMessageBox::sorry(this, i18n("The path to the GOCR binary is not configured or is not valid.\n"
                                          "Please enter or check the path in the Kooka configuration."),
                               i18n("GOCR Software Not Found"));
            enableButton(KDialog::User1, false);
        }
    }

    /* retrieve program version and display */
    if (res.isEmpty()) res = i18n("Not found");
    else
    {
        m_ocrCmd = res;
        version(m_ocrCmd);				// start process to get version
    }
    ocrShowInfo(res);					// show binary, ready for version

    return (OcrEngine::ENG_OK);
}


void OcrGocrDialog::introduceImage(const KookaImage *img)
{
    if (img==NULL) return;
    OcrBaseDialog::introduceImage(img);

    m_isBW = true;
    if (img->numColors()>0 && img->numColors()<3)
    {
        kDebug() << "Have" << img->numColors() << "colors on depth" << img->depth();
        /* that means it is a black-and-white image. Thus we do not need the GrayLevel slider */
        m_isBW = false;
    }

    if (sliderGrayLevel!=NULL) sliderGrayLevel->setEnabled(m_isBW);
}


void OcrGocrDialog::slotWriteConfig( void )
{
    kDebug();

    OcrBaseDialog::slotWriteConfig();

    KConfigGroup grp1 = KGlobal::config()->group(CFG_GROUP_OCR_DIA);
    grp1.writePathEntry(CFG_GOCR_BINARY, getOCRCmd());

    KConfigGroup grp2 = KGlobal::config()->group(CFG_GROUP_GOCR);
    grp2.writeEntry(CFG_GOCR_GRAYLEVEL, getGraylevel());
    grp2.writeEntry(CFG_GOCR_DUSTSIZE, getDustsize());
    grp2.writeEntry(CFG_GOCR_SPACEWIDTH, getSpaceWidth());
}


void OcrGocrDialog::enableFields(bool enable)
{
    sliderGrayLevel->setEnabled(enable && m_isBW);
    sliderDustSize->setEnabled(enable);
    sliderSpace->setEnabled(enable);
}


void OcrGocrDialog::version(const QString &exe)
{
    kDebug() << "of" << exe;

    QString vers;

    KProcess proc;
    proc << exe << "-h";

    int status = proc.execute(5000);
    if (status==0)
    {
        QByteArray output = proc.readAllStandardError();
        QRegExp rx("-- gocr ([\\d\\.\\s]+)");
        if (rx.indexIn(output)>-1) vers = rx.cap(1);
        else vers = i18n("Unknown");
    }
    else
    {
        kDebug() << "failed with status" << status;
        vers = i18n("Error");
    }

    ocrShowVersion(vers);
}