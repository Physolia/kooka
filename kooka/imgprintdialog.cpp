/***************************************************************************
                imgprintdialog.h  - Kooka's Image Printing
                             -------------------
    begin                : May 2003
    copyright            : (C) 1999 by Klaas Freitag
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
#include "imgprintdialog.h"

#include <klocale.h>
#include <knuminput.h>
#include <kdialog.h>

#include <qstring.h>
#include <qmap.h>
#include <qlayout.h>
#include <qvbuttongroup.h>
#include <qcheckbox.h>
#include <qradiobutton.h>
#include "kookaimage.h"
#include <qvgroupbox.h>
#include <qpaintdevicemetrics.h>
#include <qlabel.h>
#include <kdebug.h>

#define ID_SCREEN 0
#define ID_ORIG   1
#define ID_CUSTOM 2


ImgPrintDialog::ImgPrintDialog( KookaImage *img, QWidget *parent, const char* name )
    : KPrintDialogPage( parent, name ),
      m_image(img),
      m_ignoreSignal(false)
{
    setTitle(i18n("Image Printing"));
    QVBoxLayout *layout = new QVBoxLayout( this );
    // layout->setMargin( KDialog::marginHint() );
    // layout->setSpacing( KDialog::spacingHint() );

    m_scaleRadios = new QVButtonGroup( i18n("Image Print Size"), this );
    m_scaleRadios->setRadioButtonExclusive(true);
    connect( m_scaleRadios, SIGNAL(clicked(int)), SLOT(slScaleChanged(int)));

    m_rbScreen = new QRadioButton( i18n("Scale to same Size as on Screen"),
                                       m_scaleRadios );

    m_scaleRadios->insert( m_rbScreen, ID_SCREEN );

    m_rbOrigSize = new QRadioButton( i18n("Original Size (calc. from scan resolution)"),
                                     m_scaleRadios );
    m_scaleRadios->insert( m_rbOrigSize, ID_ORIG );


    m_rbScale    = new QRadioButton( i18n("Scale image to custom dimension"), m_scaleRadios );
    m_scaleRadios->insert( m_rbScale, ID_CUSTOM );

    layout->addWidget( m_scaleRadios );


    QHBoxLayout *hbox = new QHBoxLayout( this );
    layout->addLayout( hbox );

    QVGroupBox *group = new QVGroupBox( i18n("Image Print Size"), this );
    hbox->addWidget( group );

    m_sizeW = new KIntNumInput( group );
    m_sizeW->setLabel( i18n("Image Width:"), AlignVCenter );
    m_sizeW->setSuffix( i18n(" mm"));
    connect( m_sizeW, SIGNAL(valueChanged(int)), SLOT(slCustomWidthChanged(int)));
    m_sizeH = new KIntNumInput( m_sizeW, AlignVCenter, group  );
    m_sizeH->setLabel( i18n("Image Height:"), AlignVCenter);
    m_sizeH->setSuffix( i18n(" mm"));
    connect( m_sizeH, SIGNAL(valueChanged(int)), SLOT(slCustomHeightChanged(int)));

    m_ratio = new QCheckBox( i18n("Maintain aspect ration"), group, "cbAspectRatio" );
    m_ratio->setChecked(true);

    QVGroupBox *group1 = new QVGroupBox( i18n("Resolutions"), this );
    hbox->addWidget( group1 );

    /* Postscript generation resolution  */
    m_psDraft = new QCheckBox( i18n("Generate low resolution Postscript (fast draft print)"),
				      group1, "cbPostScriptRes" );
    m_psDraft->setChecked( false );


    /* Scan resolution of the image */
    m_dpi = new KIntNumInput( group1 );
    m_dpi->setLabel( i18n("Scan resolution (dpi) " ), AlignVCenter );
    m_dpi->setValue( 300 );
    m_dpi->setSuffix( i18n(" dpi"));

    /* Label for displaying the screen Resolution */
    m_screenRes = new QLabel( group1 );

    QWidget *spaceEater = new QWidget( this );
    spaceEater->setSizePolicy( QSizePolicy( QSizePolicy::Ignored, QSizePolicy::Ignored ));
    layout->addWidget( spaceEater );

    /* Set start values */
    m_rbScreen->setChecked(true);
    slScaleChanged( ID_SCREEN );
}

void ImgPrintDialog::setImage( KookaImage *img )
{
    if( ! img ) return;

    // TODO: get scan resolution out of the image

}

void ImgPrintDialog::setOptions(const QMap<QString,QString>& opts)
{
    // m_autofit->setChecked(opts["app-img-autofit"] == "1");
    QString scale = opts[OPT_SCALING];

    kdDebug(28000) << "In setOption" << endl;

    if( scale == "scan" )
        m_rbOrigSize->setChecked(true);
    else if( scale == "custom" )
        m_rbScale->setChecked(true);
    else
        m_rbScreen->setChecked(true);

    int help  = opts[OPT_SCAN_RES].toInt();
    m_dpi->setValue( help );

    help = opts[OPT_WIDTH].toInt();
    m_sizeW->setValue( help );

    help = opts[OPT_HEIGHT].toInt();
    m_sizeH->setValue( help );

    help = opts[OPT_SCREEN_RES].toInt();
    m_screenRes->setText(i18n( "Screen Resolution: %1 dpi").arg(help));

    help = opts[OPT_PSGEN_DRAFT].toInt();
    m_psDraft->setChecked( help == 1 );

    help = opts[OPT_RATIO].toInt();
    m_ratio->setChecked( help == 1 );

}


void ImgPrintDialog::getOptions(QMap<QString,QString>& opts, bool )
{
    // TODO: Check for meaning of include_def !
    // kdDebug(28000) << "In getOption with include_def: "  << include_def << endl;

    QString scale = "screen";
    if( m_rbOrigSize->isChecked() )
        scale = "scan";
    else if( m_rbScale->isChecked() )
        scale = "custom";

    opts[OPT_SCALING] = scale;

    opts[OPT_SCAN_RES]    = QString::number( m_dpi->value()         );
    opts[OPT_WIDTH]       = QString::number( m_sizeW->value()       );
    opts[OPT_HEIGHT]      = QString::number( m_sizeH->value()       );
    opts[OPT_PSGEN_DRAFT] = QString::number( m_psDraft->isChecked() );
    opts[OPT_RATIO]       = QString::number( m_ratio->isChecked()   );

    {
	QPaintDeviceMetrics metric( this );
	opts[OPT_SCREEN_RES] = QString::number( metric.logicalDpiX());
    }
}

bool ImgPrintDialog::isValid(QString& msg)
{
    /* check if scan reso is higher than 0 in case its needed */
    int id = m_scaleRadios->id( m_scaleRadios->selected());
    if( id == ID_ORIG && m_dpi->value() == 0 )
    {
        msg = i18n("Please specify a scan resolution larger than 0");
        return false;
    }
    else if( id == ID_CUSTOM && (m_sizeW->value() == 0 || m_sizeH->value() == 0 )  )
    {
        msg = i18n("For custom printing, a valid size should be specified.\n"
                   "At least one dimension is zero.");
    }

    return true;
}

void ImgPrintDialog::slScaleChanged( int id )
{
    if( id == ID_SCREEN )
    {
	/* disalbe size, scan res. */
	m_dpi->setEnabled(false);

        m_ratio->setEnabled(false);
        m_sizeW->setEnabled(false);
        m_sizeH->setEnabled(false);
    }
    else if( id == ID_ORIG )
    {
	/* disable size */
	m_dpi->setEnabled(true);
        m_ratio->setEnabled(false);
        m_sizeW->setEnabled(false);
        m_sizeH->setEnabled(false);
    }
    else if( id == ID_CUSTOM )
    {
	m_dpi->setEnabled(false);
        m_ratio->setEnabled(true);
        m_sizeW->setEnabled(true);
        m_sizeH->setEnabled(true);
    }
}

void ImgPrintDialog::slCustomWidthChanged( int val )
{
    if( m_ignoreSignal )
    {
        m_ignoreSignal = false;
        return;
    }

    /* go out here if scaling is not custom */
    if( m_scaleRadios->id( m_scaleRadios->selected()) != ID_CUSTOM ) return;

    /* go out here if maintain aspect ration is off */
    if( ! m_ratio->isChecked() ) return;

    m_ignoreSignal = true;
    kdDebug(28000) << "Setting value to horizontal size" << endl;
    m_sizeH->setValue( int( double(val) *
                            double(m_image->height())/double(m_image->width()) ) );

}

void ImgPrintDialog::slCustomHeightChanged( int val )
{
    if( m_ignoreSignal )
    {
        m_ignoreSignal = false;
        return;
    }

    /* go out here if scaling is not custom */
    if( m_scaleRadios->id( m_scaleRadios->selected()) != ID_CUSTOM ) return;

    /* go out here if maintain aspect ration is off */
    if( ! m_ratio->isChecked() ) return;

    m_ignoreSignal = true;
    kdDebug(28000) << "Setting value to vertical size" << endl;
    m_sizeW->setValue( int( double(val) *
                            double(m_image->width())/double(m_image->height()) ) );

}

#include "imgprintdialog.moc"
