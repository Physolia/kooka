/************************************************************************
 *									*
 *  This file is part of Kooka, a scanning/OCR application using	*
 *  Qt <http://www.qt.io> and KDE Frameworks <http://www.kde.org>.	*
 *									*
 *  Copyright (C) 1999-2016 Klaas Freitag <freitag@suse.de>		*
 *                          Jonathan Marten <jjm@keelhaul.me.uk>	*
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

#include "imgsaver.h"

#include <qdir.h>
#include <qregexp.h>
#include <qdebug.h>
#include <qmimedatabase.h>
#include <qtemporaryfile.h>

#include <klocalizedstring.h>
#include <kmessagebox.h>
#include <kconfigskeleton.h>

#include <kio/statjob.h>
#include <kio/mkdirjob.h>
#include <kio/filecopyjob.h>
#include <kio/udsentry.h>

#include "imageformat.h"
#include "kookaimage.h"
#include "kookapref.h"
#include "kookasettings.h"
#include "formatdialog.h"

#include "imagemetainfo.h"


static void createDir(const QUrl &url)
{
    qDebug() << url;
    KIO::StatJob *job = KIO::stat(url, KIO::StatJob::DestinationSide, 0 /* minimal details */);
    if (!job->exec())
    {
        KMessageBox::sorry(NULL, xi18nc("@info", "The directory <filename>%2</filename><nl/>could not be accessed.<nl/>%1",
                                        job->errorString(), url.url(QUrl::PreferLocalFile)));
        return;
    }

    qDebug() << job->statResult().numberValue(KIO::UDSEntry::UDS_FILE_TYPE);
    if (!job->statResult().isDir())
    {
        qDebug() << "directory" << url << "does not exist, try to create";
        KIO::MkdirJob *job = KIO::mkdir(url);
        if (!job->exec())
        {
            KMessageBox::sorry(NULL, xi18nc("@info", "The directory <filename>%2</filename><nl/>could not be created.<nl/>%1",
                                            job->errorString(), url.url(QUrl::PreferLocalFile)));
            return;
        }
    }
}

ImgSaver::ImgSaver(const QUrl &dir)
    : mSaveUrl(QUrl()),
      mSaveFormat("")
{
    if (dir.isValid() && !dir.isEmpty() && dir.isLocalFile()) {
        // can use specified place
        m_saveDirectory = dir;
        qDebug() << "specified directory" << m_saveDirectory;
    } else {                    // cannot, so use default
        m_saveDirectory = QUrl::fromLocalFile(KookaPref::galleryRoot());
        qDebug() << "default directory" << m_saveDirectory;
    }

    createDir(m_saveDirectory);				// ensure save location exists
}

QString extension(const QUrl &url)
{
    QMimeDatabase db;
    return (db.suffixForFileName(url.path()));
}

ImgSaver::ImageSaveStatus ImgSaver::getFilenameAndFormat(ImageMetaInfo::ImageType type)
{
    if (type == ImageMetaInfo::Unknown) return (ImgSaver::SaveStatusParam);

    QString saveFilename = createFilename();		// find next unused filename
    ImageFormat saveFormat = findFormat(type);		// find saved image format
    QString saveSubformat = findSubFormat(saveFormat);	// currently not used
							// get dialogue preferences
    m_saveAskFilename = KookaSettings::saverAskForFilename();
    m_saveAskFormat = KookaSettings::saverAskForFormat();

    qDebug() << "before dialogue,"
             << "type=" << type
             << "ask_filename=" << m_saveAskFilename
             << "ask_format=" << m_saveAskFormat
             << "filename=" << saveFilename
             << "format=" << saveFormat
             << "subformat=" << saveSubformat;

    while (saveFilename.isEmpty() || !saveFormat.isValid() || m_saveAskFormat || m_saveAskFilename)
    {							// is a dialogue neeeded?
        FormatDialog fd(NULL, type, m_saveAskFormat, saveFormat, m_saveAskFilename, saveFilename);
        if (!fd.exec()) {
            return (ImgSaver::SaveStatusCanceled);
        }
        // do the dialogue
        saveFilename = fd.getFilename();		// get filename as entered
        if (fd.useAssistant()) {			// redo with format options
            m_saveAskFormat = true;
            continue;
        }

        saveFormat = fd.getFormat();			// get results from that
        saveSubformat = fd.getSubFormat();

        if (saveFormat.isValid()) {			// have a valid format
            if (fd.alwaysUseFormat()) storeFormatForType(type, saveFormat);
            break;					// save format for future
        }
    }

    QUrl fi = m_saveDirectory.resolved(QUrl(saveFilename));
    QString ext = saveFormat.extension();
    if (extension(fi) != ext)				// already has correct extension?
    {
        fi.setPath(fi.path()+'.'+ext);			// no, add it on
    }

    mSaveUrl = fi;
    mSaveFormat = saveFormat;
    mSaveSubformat = saveSubformat;

    qDebug() << "after dialogue,"
             << "filename=" << saveFilename
             << "format=" << mSaveFormat
             << "subformat=" << mSaveSubformat
             << "url=" << mSaveUrl;
    return (ImgSaver::SaveStatusOk);
}

ImgSaver::ImageSaveStatus ImgSaver::setImageInfo(const ImageMetaInfo *info)
{
    if (info == NULL) return (ImgSaver::SaveStatusParam);
    return (getFilenameAndFormat(info->getImageType()));
}

ImgSaver::ImageSaveStatus ImgSaver::saveImage(const QImage *image)
{
    if (image == NULL) return (ImgSaver::SaveStatusParam);

    if (!mSaveFormat.isValid()) {			// see if have this already
        // if not, get from image now
        //qDebug() << "format not resolved yet";
        ImgSaver::ImageSaveStatus stat = getFilenameAndFormat(ImageMetaInfo::findImageType(image));
        if (stat != ImgSaver::SaveStatusOk) return (stat);
    }

    if (!mSaveUrl.isValid() || !mSaveFormat.isValid()) { // must have these now
        //qDebug() << "format not resolved!";
        return (ImgSaver::SaveStatusParam);
    }
    // save image to file
    return (saveImage(image, mSaveUrl, mSaveFormat, mSaveSubformat));
}

ImgSaver::ImageSaveStatus ImgSaver::saveImage(const QImage *image,
                                              const QUrl &url,
                                              const ImageFormat &format,
                                              const QString &subformat)
{
    if (image == NULL) return (ImgSaver::SaveStatusParam);

    qDebug() << "to" << url << "format" << format << "subformat" << subformat;

    mLastFormat = format.name();			// save for error message later
    mLastUrl = url;

    if (!url.isLocalFile())				// file must be local
    {
        qDebug() << "Can only save local files";
        // TODO: allow non-local files
        return (ImgSaver::SaveStatusProtocol);
    }

    QString filename = url.path();			// local file path
    QFileInfo fi(filename);				// information for that
    QString dirPath = fi.path();			// containing directory

    QDir dir(dirPath);
    if (!dir.exists())					// should always exist, except
    {							// for first preview save
        qDebug() << "Creating directory" << dirPath;
        if (!dir.mkdir(dirPath))
        {
            //qDebug() << "Could not create directory" << dirPath;
            return (ImgSaver::SaveStatusMkdir);
        }
    }

    if (fi.exists() && !fi.isWritable())
    {
        //qDebug() << "Cannot overwrite existing file" << filename;
        return (ImgSaver::SaveStatusPermission);
    }

    if (!format.canWrite())				// check format, is it writable?
    {
        //qDebug() << "Cannot write format" << format;
        return (ImgSaver::SaveStatusFormatNoWrite);
    }

    bool result = image->save(filename, format.name());
    return (result ? ImgSaver::SaveStatusOk : ImgSaver::SaveStatusFailed);
}

/**
 *  Find the next filename to use for the image to save.
 *  This is done by enumerating and checking against all existing files,
 *  regardless of format - because we have not resolved the format yet.
 **/
QString ImgSaver::createFilename()
{
    if (!m_saveDirectory.isLocalFile()) return (QString::null);
    // TODO: allow non-local files
    QDir files(m_saveDirectory.path(), "kscan_[0-9][0-9][0-9][0-9].*");
    QStringList l(files.entryList());
    l.replaceInStrings(QRegExp("\\..*$"), "");

    QString fname;
    for (int c = 1; c <= l.count() + 1; ++c) {  // that must be the upper bound
        fname = "kscan_" + QString::number(c).rightJustified(4, '0');
        if (!l.contains(fname)) {
            break;
        }
    }

    //qDebug() << "returning" << fname;
    return (fname);
}

/*
 * findFormat looks to see if there is a previously saved file format for
 * the image type in question.
 */
ImageFormat ImgSaver::findFormat(ImageMetaInfo::ImageType type)
{
    if (type == ImageMetaInfo::Thumbnail) {
        return (ImageFormat("BMP"));    // thumbnail always this format
    }
    if (type == ImageMetaInfo::Preview) {
        return (ImageFormat("BMP"));    // preview always this format
    }
    // real images from here on
    ImageFormat format = getFormatForType(type);
    //qDebug() << "format for type" << type << "=" << format;
    return (format);
}

QString ImgSaver::picTypeAsString(ImageMetaInfo::ImageType type)
{
    QString res;

    switch (type) {
    case ImageMetaInfo::LowColour:
        res = i18n("indexed color image (up to 8 bit depth)");
        break;

    case ImageMetaInfo::Greyscale:
        res = i18n("gray scale image (up to 8 bit depth)");
        break;

    case ImageMetaInfo::BlackWhite:
        res = i18n("lineart image (black and white, 1 bit depth)");
        break;

    case ImageMetaInfo::HighColour:
        res = i18n("high/true color image (more than 8 bit depth)");
        break;

    default:
        res = i18n("unknown image type %1", type);
        break;
    }

    return (res);
}

/*
 *  This method returns true if the image format given in format is remembered
 *  for that image type.
 */
bool ImgSaver::isRememberedFormat(ImageMetaInfo::ImageType type, const ImageFormat &format)
{
    return (getFormatForType(type) == format);
}

static KConfigSkeleton::ItemString *configItemForType(ImageMetaInfo::ImageType type)
{
    switch (type)
    {
case ImageMetaInfo::LowColour:	return (KookaSettings::self()->formatLowColourItem());
case ImageMetaInfo::Greyscale:	return (KookaSettings::self()->formatGreyscaleItem());
case ImageMetaInfo::BlackWhite:	return (KookaSettings::self()->formatBlackWhiteItem());
case ImageMetaInfo::HighColour:	return (KookaSettings::self()->formatHighColourItem());
default:			return (KookaSettings::self()->formatUnknownItem());
    }
}

ImageFormat ImgSaver::getFormatForType(ImageMetaInfo::ImageType type)
{
    const KConfigSkeleton::ItemString *ski = configItemForType(type);
    Q_ASSERT(ski!=NULL);
    return (ImageFormat(ski->value().toLocal8Bit()));
}

void ImgSaver::storeFormatForType(ImageMetaInfo::ImageType type, const ImageFormat &format)
{
    //  We don't save OP_FILE_ASK_FORMAT here, this is the global setting
    //  "Always use the Save Assistant" from the Kooka configuration which
    //  is a preference option affecting all image types.  To get automatic
    //  saving in the preferred format, turn off that option in "Configure
    //  Kooka - Image Saver" and select "Always use this format for this type
    //  of file" when saving an image.  As long as an image of that type has
    //  scanned and saved, then the Save Assistant will not subsequently
    //  appear for that image type.
    //
    //  This means that turning on the "Always use the Save Assistant" option
    //  will do exactly what it says.

    KConfigSkeleton::ItemString *ski = configItemForType(type);
    Q_ASSERT(ski!=NULL);
    ski->setValue(format.name());
    KookaSettings::self()->save();
}

QString ImgSaver::findSubFormat(const ImageFormat &format)
{
    //qDebug() << "for" << format;
    return (QString::null);             // no subformats currently used
}

QString ImgSaver::errorString(ImgSaver::ImageSaveStatus status) const
{
    QString re;
    switch (status) {
    case ImgSaver::SaveStatusOk:
        re = i18n("Save OK");                           break;
    case ImgSaver::SaveStatusPermission:
        re = i18n("Permission denied");                     break;
    case ImgSaver::SaveStatusBadFilename:           // never used
        re = i18n("Bad file name");                     break;
    case ImgSaver::SaveStatusNoSpace:           // never used
        re = i18n("No space left on device");                   break;
    case ImgSaver::SaveStatusFormatNoWrite:
        re = i18n("Cannot write image format '%1'", mLastFormat.constData());   break;
    case ImgSaver::SaveStatusProtocol:
        re = i18n("Cannot write using protocol '%1'", mLastUrl.scheme()); break;
    case ImgSaver::SaveStatusCanceled:
        re = i18n("User cancelled saving");                 break;
    case ImgSaver::SaveStatusMkdir:
        re = i18n("Cannot create directory");                   break;
    case ImgSaver::SaveStatusFailed:
        re = i18n("Save failed");                       break;
    case ImgSaver::SaveStatusParam:
        re = i18n("Bad parameter");                     break;
    default:
        re = i18n("Unknown status %1", status);                 break;
    }
    return (re);
}

QString ImgSaver::tempSaveImage(const KookaImage *img, const ImageFormat &format, int colors)
{
    if (img == NULL) return (QString::null);

    const QString tempTemplate = QDir::tempPath()+'/'+"imgsaverXXXXXX."+format.extension();
    QTemporaryFile tmpFile(tempTemplate);
    tmpFile.setAutoRemove(false);

    if (!tmpFile.open())
    {
        qDebug() << "Error opening temp file" << tmpFile.fileName();
        tmpFile.setAutoRemove(true);
        return (QString::null);
    }
    QString name = tmpFile.fileName();
    tmpFile.close();

    const KookaImage *tmpImg = NULL;
    if (colors != -1 && img->depth() != colors) { // Need to convert image
        QImage::Format newfmt;
        switch (colors) {
        case 1:     newfmt = QImage::Format_Mono;
            break;

        case 8:     newfmt = QImage::Format_Indexed8;
            break;

        case 24:    newfmt = QImage::Format_RGB888;
            break;

        case 32:    newfmt = QImage::Format_RGB32;
            break;

        default:    //qDebug() << "Error: Bad colour depth requested" << colors;
            tmpFile.setAutoRemove(true);
            return (QString::null);
        }

        tmpImg = new KookaImage(img->convertToFormat(newfmt));
        img = tmpImg;
    }

    qDebug() << "Saving to" << name << "in format" << format;
    if (!img->save(name, format.name())) {
        qDebug() << "Error saving to" << name;
        tmpFile.setAutoRemove(true);
        name = QString::null;
    }

    if (tmpImg != NULL) delete tmpImg;
    return (name);
}

bool copyRenameImage(bool isCopying, const QUrl &fromUrl, const QUrl &toUrl, bool askExt, QWidget *overWidget)
{
    QString errorString = QString::null;

    /* Check if the provided filename has a extension */
    QString extFrom = extension(fromUrl);
    QString extTo = extension(toUrl);

    QUrl targetUrl(toUrl);
    if (extTo.isEmpty() && !extFrom.isEmpty())
    {							// ask if the extension should be added
        int result = KMessageBox::Yes;
        QString fName = toUrl.fileName();
        if (!fName.endsWith(".")) fName += ".";
        fName += extFrom;

        if (askExt) result = KMessageBox::questionYesNo(overWidget,
                                 xi18nc("@info", "The file name you supplied has no file extension.<nl/>"
                                        "Should the original one be added?<nl/><nl/>"
                                        "This would result in the new file name <filename>%1</filename>", fName),
                                 i18n("Extension Missing"),
                                 KGuiItem(i18n("Add Extension")),
                                 KGuiItem(i18n("Do Not Add")),
                                 "AutoAddExtensions");
        if (result == KMessageBox::Yes)
        {
            targetUrl.setPath(targetUrl.adjusted(QUrl::RemoveFilename).path()+fName);
        }
    }
    else if (!extFrom.isEmpty() && extFrom != extTo)
    {
        QMimeDatabase db;
        const QMimeType fromType = db.mimeTypeForUrl(fromUrl);
        const QMimeType toType = db.mimeTypeForUrl(toUrl);
        if (toType.name() != fromType.name())
        {
            errorString = "Changing the image format is not currently supported";
        }
    }

    if (errorString.isEmpty())				// no problem so far
    {
        qDebug() << (isCopying ? "Copy" : "Rename") << "->" << targetUrl;

        KIO::StatJob *job = KIO::stat(targetUrl, KIO::StatJob::DestinationSide, 0);
        if (job->exec())				// stat with minimal details
        {						// to see if destination exists
                errorString = i18n("Target already exists");
        }
        else
        {
            KJob *job;
            if (isCopying) job = KIO::file_copy(fromUrl, targetUrl);
            else job = KIO::file_move(fromUrl, targetUrl);
							// copy/rename the file
            if (!job->exec()) errorString = job->errorString();
        }
    }

    if (!errorString.isEmpty())				// file operation error
    {
        QString msg = (isCopying ? i18n("Unable to copy the file") :
                                   i18n("Unable to rename the file"));
        QString title = (isCopying ? i18n("Error copying file") :
                                     i18n("Error renaming file"));
        KMessageBox::sorry(overWidget, xi18nc("@info", "%1 <filename>%3</filename><nl/>%2",
                                              msg, errorString,
                                              fromUrl.url(QUrl::PreferLocalFile)), title);
        return (false);
    }

    return (true);                  // file operation succeeded
}

bool ImgSaver::renameImage(const QUrl &fromUrl, const QUrl &toUrl, bool askExt, QWidget *overWidget)
{
    return (copyRenameImage(false, fromUrl, toUrl, askExt, overWidget));
}

bool ImgSaver::copyImage(const QUrl &fromUrl, const QUrl &toUrl, QWidget *overWidget)
{
    return (copyRenameImage(true, fromUrl, toUrl, true, overWidget));
}
