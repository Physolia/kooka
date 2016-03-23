/***************************************************************************
                kookaprint.h  -  Printing from the gallery
                             -------------------
    begin                : Tue May 13 2003
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

#ifndef KOOKAPRINT_H
#define KOOKAPRINT_H

#include <qprinter.h>
// #include <qmap.h>
// #include <qstring.h>
// #include <qpoint.h>

#ifndef KDE4
#include <kprinter.h>
#include <kdeprint/kprintdialogpage.h>
#endif

class QPainter;
// class QSize;
// 
// class KPrinter;
// class KLineEdit;

class KookaImage;


class KookaPrint : public QPrinter
{
//     Q_OBJECT

public:
    explicit KookaPrint();

    void setOptions(const QMap<QString, QString> *opts);


    /**
     * The top left edge of the required print position
     */
// TODO: no need for virtual
// TODO: no need to be public either
    virtual QPoint printPosTopLeft(const QSize &) const;
    virtual QPoint printPosTopRight(const QSize &) const;
    virtual QPoint printPosBottomLeft(const QSize &) const;
    virtual QPoint printPosBottomRight(const QSize &) const;

    virtual int extraMarginPix() const;

    /**
     * The maximum pixel size of the image (or imagepart) on
     * the current page
     */
    virtual QSize maxPageSize(int extraShrinkPercent = 0) const;

// TODO: do these need to be slots?
public slots:
    bool printImage(const KookaImage *img, int intextraMarginPercent = 10);
    void printFittingToPage(const KookaImage *img);

protected:
    typedef enum { SW, NW, NO, SO } MarkerDirection;

    virtual void drawMarkerAroundPoint(const QPoint &);
    virtual void drawCutSign(const QPoint &, int, MarkerDirection);
    virtual void drawCornerMarker(const QSize &, int, int, int, int);

private:
    QPainter *m_painter;
    const QMap<QString, QString> *m_options;

    int m_extraMarginPercent;
};

#endif                          // KOOKAPRINT_H
