/* This file is part of the KDE Project
   Copyright (C) 2000 Klaas Freitag <freitag@suse.de>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   $Id$
*/

#ifndef PREVIEWER_H
#define PREVIEWER_H

#include <qwidget.h>
#include <qlayout.h>
#include <qimage.h>
#include <qrect.h>
#include <qcombobox.h>
#include <qbuttongroup.h>
#include <qradiobutton.h>
#include <qpoint.h>

#include <kruler.h>
#include <qmemarray.h>

/**
  *@author Klaas Freitag
  */
class ImageCanvas;
class QCheckBox;
class QSlider;
class KScanDevice;

class Previewer : public QWidget
{
    Q_OBJECT
public:
    Previewer(QWidget *parent=0, const char *name=0);
    ~Previewer();

    ImageCanvas *getImageCanvas( void ){ return( img_canvas ); }

    bool loadPreviewImage( const QString forScanner );

    /**
     * Static function that returns the image gallery base dir.
     */
    static QString galleryRoot();
    static QString previewFile( const QString& scanner );
    void   findSelection( );

public slots:
    void newImage( QImage* );
    void slFormatChange( int id );
    void slOrientChange(int);
    void slSetDisplayUnit( KRuler::MetricStyle unit );
    void setScanSize( int w, int h, KRuler::MetricStyle unit );
    void slCustomChange( void );
    void slNewDimen(QRect r);
    void slNewScanResolutions( int, int );
    void recalcFileSize( void );
    void slAutoSelThresh(int);
    void slAutoSelDustsize(int);
    void slSetScannerBgIsWhite(bool b);
    void slConnectScanner( KScanDevice *scan ) { m_scanner = scan; }
protected slots:
    void slScaleToWidth();
    void slScaleToHeight();
    void slAutoSelToggled(bool);

signals:
    void newRect( QRect );
    void noRect( void );
    void setScanWidth(const QString&);
    void setScanHeight(const QString&);
    void setSelectionSize( long );

private:
    void checkForScannerBg();

    QPoint calcPercent( int, int );

    QHBoxLayout *layout;
    ImageCanvas *img_canvas;
    QComboBox   *pre_format_combo;
    QMemArray<QCString> format_ids;
    QButtonGroup * bgroup;
    QRadioButton * rb1;
    QRadioButton * rb2;
    QImage       m_previewImage;

    QCheckBox    *m_cbAutoSel;

    bool imagePiece( QMemArray<long> src,
                     int& start,
                     int& end );

    int landscape_id, portrait_id;

    /** Autoselection: try to find a selection after a preview scan **/
    bool m_doAutoSelection;  /* switch auto-selection on and off */
    int  m_autoSelThresh;    /* threshold for auto selection     */
    int  m_dustsize;         /* dustsize for auto selection      */

    bool m_bgIsWhite;        /* indicates if a scan without paper results in black or white */
    QSlider *m_sliderThresh;
    QSlider  *m_sliderDust;


    double overallWidth, overallHeight;
    KRuler::MetricStyle sizeUnit;
    KRuler::MetricStyle displayUnit;
    bool isCustom;

    int  scanResX, scanResY;
    int  pix_per_byte;
    double selectionWidthMm;
    double selectionHeightMm;

    KScanDevice *m_scanner;
    class PreviewerPrivate;
    PreviewerPrivate *d;
};

#endif
