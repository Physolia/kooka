/***************************************************************************
                          massscandialog.cpp  -  description                              
                             -------------------                                         
    begin                : Sun Jun 11 2000                                           
    copyright            : (C) 2000 by Klaas Freitag                         
    email                : freitag@suse.de
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   * 
 *                                                                         *
 ***************************************************************************/

#include <qlayout.h>
#include <qlabel.h>
#include <qgroupbox.h>
#include <qframe.h>

#include <qpushbutton.h>

#include <klocale.h>
#include <kdebug.h>

#include "massscandialog.h"

MassScanDialog::MassScanDialog( QWidget *parent )
   :QSemiModal( parent, "MASS_SCAN", true )
{
   setCaption( i18n( "ADF Scanning" ));
   kdDebug(29000) << "Starting MassScanDialog!" << endl;
   // Layout-Boxes
   QVBoxLayout *bigdad = new QVBoxLayout( this, 5 );
   // QHBoxLayout *hl1= new QHBoxLayout( );      // Caption
   QHBoxLayout *l_but  = new QHBoxLayout( 10 );  // Buttons
 	
 	/* Caption */
 	QLabel *l1 = new QLabel( QString( i18n( "<B>Mass Scanning</B>" )), this);
   bigdad->addWidget( l1, 1);
 	
 	/* Scan parameter information */
 	QGroupBox *f1 = new QGroupBox( i18n("Scan parameter:"), this );
 	f1->setFrameStyle( QFrame::Box | QFrame::Sunken );
 	f1->setMargin(5);
 	f1->setLineWidth( 1 );
   QVBoxLayout *l_main = new QVBoxLayout( f1, f1->frameWidth()+3, 3 );
 	bigdad->addWidget( f1, 6 );
 	
   scanopts = i18n("Scanning <B>%s</B> with <B>%d</B> dpi");
 	l_scanopts = new QLabel( scanopts, f1 );
 	l_main->addWidget( l_scanopts );

   tofolder = i18n("Storing new images in folder <B>%s</B>");
 	l_tofolder = new QLabel( tofolder, f1 );
 	l_main->addWidget( l_tofolder );
 	
 	/* Scan Progress information */
 	QGroupBox *f2 = new QGroupBox( i18n("Scan progress:"), this );
 	f2->setFrameStyle( QFrame::Box | QFrame::Sunken );
 	f2->setMargin(15);
 	f2->setLineWidth( 1 );
   QVBoxLayout *l_pro = new QVBoxLayout( f2, f2->frameWidth()+3, 3 );
 	bigdad->addWidget( f2, 6 );

 	QHBoxLayout *l_scanp = new QHBoxLayout( );
 	l_pro->addLayout( l_scanp, 5 );
   progress = i18n("Scanning page %1");
   l_progress = new QLabel( progress, f2 );
   l_scanp->addWidget( l_progress, 3 );
 	l_scanp->addStretch( 1 );
   QPushButton *pb_cancel_scan = new QPushButton( i18n("Cancel Scan"), f2);
   l_scanp->addWidget( pb_cancel_scan,3 );

   progressbar = new QProgressBar( 1000, f2 );
   l_pro->addWidget( progressbar, 3 );

 	/* Buttons to start scanning and close the Window */
  	bigdad->addLayout( l_but );

   QPushButton *b_start = new QPushButton( i18n("Start Scan"), this, "ButtOK" );
   connect( b_start, SIGNAL(clicked()), this, SLOT( slStartScan()) );

   QPushButton *b_cancel = new QPushButton( i18n("Stop"), this, "ButtCancel" );
   connect( b_cancel, SIGNAL(clicked()), this, SLOT(slStopScan()) );

   QPushButton *b_finish = new QPushButton( i18n("Close"), this, "ButtFinish" );
   connect( b_finish, SIGNAL(clicked()), this, SLOT(slFinished()) );

   l_but->addWidget( b_start );
   l_but->addWidget( b_cancel );
   l_but->addWidget( b_finish );

   bigdad->activate();
   show();
}

MassScanDialog::~MassScanDialog()
{

}

void MassScanDialog::slStartScan( void )
{

}

void MassScanDialog::slStopScan( void )
{

}

void MassScanDialog::slFinished( void )
{
    delete this;
}

#include "massscandialog.moc"
