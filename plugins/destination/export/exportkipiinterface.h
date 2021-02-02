/************************************************************************
 *									*
 *  This file is part of Kooka, a scanning/OCR application using	*
 *  Qt <http://www.qt.io> and KDE Frameworks <http://www.kde.org>.	*
 *									*
 *  Copyright (C) 2021      Jonathan Marten <jjm@keelhaul.me.uk>	*
 *  Based on the KIPI interface for Spectacle, which is:		*
 *  Copyright (C) 2015 Boudhayan Gupta <me@BaloneyGeek.com>		*
 *  Copyright (C) 2010 Pau Garcia i Quiles <pgquiles@elpauer.org>	*
 *  Essentially a rip-off of code for Kamoso by:			*
 *  Copyright (C) 2008-2009 by Aleix Pol <aleixpol@kde.org>		*
 *  Copyright (C) 2008-2009 by Alex Fiestas <alex@eyeos.org>		*
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

#ifndef EXPORTKIPIINTERFACE_H
#define EXPORTKIPIINTERFACE_H

#include <qlist.h>

#include <kipi/interface.h>
#include <kipi/uploadwidget.h>
#include <kipi/imagecollection.h>
#include <kipi/imagecollectionselector.h>
#include <kipi/imageinfo.h>

class ExportCollectionShared;


class ExportKipiInterface : public KIPI::Interface
{
    Q_OBJECT

public:
    explicit ExportKipiInterface(QObject *pnt = nullptr);
    ~ExportKipiInterface() override = default;

    KIPI::MetadataProcessor *createMetadataProcessor() const override;

    KIPI::ImageCollection currentAlbum() override;
    KIPI::ImageCollection currentSelection() override;
    QList<KIPI::ImageCollection> allAlbums() override;

    KIPI::ImageCollectionSelector *imageCollectionSelector(QWidget *pnt) override;
    KIPI::UploadWidget *uploadWidget(QWidget *pnt) override;

    int features() const override;
    KIPI::ImageInfo info(const QUrl &) override;
    void thumbnails(const QList<QUrl> &urls, int size) override;

    void setUrl(const QUrl &url);

private:
    QUrl mUrl;
};

#endif							// EXPORTKIPIINTERFACE_H
