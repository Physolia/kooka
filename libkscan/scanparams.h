/***************************************************************************
                          scanparams.h  -  description
                             -------------------
    begin                : Fri Dec 17 1999
    copyright            : (C) 1999 by Klaas Freitag
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


#ifndef SCANPARAMS_H
#define SCANPARAMS_H

#include "kscandevice.h"
#include "scansourcedialog.h"

#include <qlayout.h>
#include <qvbox.h>
#include <qhbox.h>

#include <qframe.h>
#include <qcheckbox.h>
#include <qpushbutton.h>
#include <qbuttongroup.h>
#include <qradiobutton.h>
#include <qdir.h>
#include <qprogressdialog.h>
#include <qpixmap.h>

#include "kscandevice.h"

/**
  *@author Klaas Freitag
  */

class GammaDialog;

typedef enum { ID_SANE_DEBUG, ID_QT_IMGIO, ID_SCAN } ScanMode;

class ScanParams : public QVBox
{
   Q_OBJECT
public:
   ScanParams( QWidget *parent, const char *name = 0);
   ~ScanParams();
#if 0
   QSize sizeHint( );
#endif
   bool connectDevice( KScanDevice* );
public slots:
/**
 * In this slot, a custom scan window can be set, e.g. through a preview
 * image with a area selector. The QRect-param needs to contain values
 * between 0 and 1000, which are interpreted as tenth of percent of the
 * whole image dimensions.
 **/
void slCustomScanSize( QRect );
	
   /**
    * sets the scan area to the default, which is the whole area.
    */	
   void slMaximalScanSize( void );	
	
   /**
    * starts acquireing a preview image.
    * This ends up in a preview-signal of the scan-device object
    */
   void slAcquirePreview( void );
   void slStartScan( void );
	
   /**
    * connect this slot to KScanOptions Signal optionChanged to be informed
    * on a options change.
    */
   void slOptionNotify( KScanOption *kso );

protected slots:		
/**
 * connected to the button which opens the source selection dialog
 */
void slSourceSelect( void );
   /**
    * allows to select a file or directory for the virtuell scanner
    */
   void slFileSelect	( void );

   /**
    *  Slot to call if the virtual scanner mode is changed
    */
   void slVirtScanModeSelect( int id );
	
   /**
    *  Slot for result on an edit-Custom Gamma Table request.
    *  Starts a dialog.
    */
   void slEditCustGamma( void );
	
   /**
    *  Slot called if a Gui-Option changed due to user action, eg. the
    *  user selects another entry in a List.
    *  Action to do is to apply the new value and check, if it affects others.
    */
   void slReloadAllGui( KScanOption* );

   /**
    *  Slot called when the Edit Custom Gamma-Dialog has a new gamma table
    *  to apply. This is an internal slot.
    */
   void slApplyGamma( GammaDialog& );
   
private:
	
	
   void          scannerParams( void ); // QVBoxLayout *top );
   void          virtualScannerParams( void );
   KScanStat     performADFScan( void );	
	
   KScanDevice      *sane_device;
   KScanOption   *virt_filename;
   QCheckBox     *cb_gray_preview;
   QPushButton   *pb_edit_gtable;
   QPushButton   *pb_source_sel;	
   ADF_BEHAVE	  adf;
   QButtonGroup  *bg_virt_scan_mode;
   ScanMode  	  scan_mode;
   QDir          last_virt_scan_path;
	
   KScanOption   *xy_resolution_bind;

   QProgressDialog *progressDialog;

   QPixmap       pixLineArt, pixGray, pixColor, pixMiniFloppy;
};

#endif