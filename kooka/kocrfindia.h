/***************************************************************************
                          kocrfindia.h  -  Dialogclass after scanning
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


#ifndef KOCRFINDIA_H
#define KOCRFINDIA_H

#include <qlabel.h>
#include <qstring.h>
#include <qstrlist.h>
#include <qsemimodal.h>
#include <qprogressbar.h>
#include <kdialogbase.h>
#include <qradiobutton.h>
#include <qgroupbox.h>
#include <qimage.h>
#include <qstring.h>
#include <qcstring.h>

#include <keditcl.h>
#include <kscanslider.h>
/**
  *@author Klaas Freitag
  */




class KOCRFinalDialog: public KDialogBase
{
   Q_OBJECT
public: 
   KOCRFinalDialog( QWidget *, QString );
   ~KOCRFinalDialog();

public slots:
void fillText( QString );
   
protected:

private slots:
   void writeConfig( void );
   
private:
   KEdit *textEdit;
   QImage *resultImg;
};

#endif
