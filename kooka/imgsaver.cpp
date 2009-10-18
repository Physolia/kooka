/***************************************************************************
                          img_saver.cpp  -  description
                             -------------------
    begin                : Mon Dec 27 1999
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

#include "imgsaver.h"
//#include "imgsaver.moc"

#include <qdir.h>
#include <qregexp.h>

#include <kglobal.h>
#include <kconfig.h>
#include <kimageio.h>
#include <klocale.h>
#include <kmessagebox.h>
#include <kdebug.h>
#include <ktemporaryfile.h>
#include <kmimetype.h>

#include <kio/job.h>
#include <kio/netaccess.h>

#include "libkscan/previewer.h"

#include "kookaimage.h"
#include "formatdialog.h"


ImgSaver::ImgSaver(QWidget *parent, const KUrl &dir)
//    : QObject(parent)
{
    if (dir.isValid() && !dir.isEmpty() && dir.protocol()=="file")
    {							// can use specified place
        m_saveDirectory = dir.directory(KUrl::IgnoreTrailingSlash);
        kDebug() << "specified directory" << m_saveDirectory;
    }
    else						// cannot, so use default
    {
        m_saveDirectory = Previewer::galleryRoot();
        kDebug() << "default directory" << m_saveDirectory;
    }

    createDir(m_saveDirectory);				// ensure save location exists
    last_format = "";					// nothing saved yet
}


/* Needs a full qualified directory name */
void ImgSaver::createDir(const QString &dir)
{
    KUrl url(dir);
    if (!KIO::NetAccess::exists(url, false, 0))
    {
        kDebug() << "directory" << dir << "does not exist, try to create";
        // if( mkdir( QFile::encodeName( dir ), S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH ) != 0 )
        if (KIO::mkdir(url))
        {
            KMessageBox::sorry(NULL, i18n("<qt>The folder<br><filename>%1</filename><br>does not exist and could not be created", dir));
        }
    }
#if 0
    if (!fi.isWritable())
    {
        KMessageBox::sorry(NULL, i18n("<qt>The directory<br><filename>%1</filename><br> is not writeable, please check the permissions.", dir));
    }
#endif
}


/**
 *   This function asks the user for a filename or creates
 *   one by itself, depending on the settings
 **/
ImgSaveStat ImgSaver::saveImage(const QImage *image)
{
    if (image==NULL) return (ISS_ERR_PARAM);

    ImgSaver::ImageType imgType = ImgSaver::ImgNone;	// find out what kind of image it is
    if (image->depth()>8) imgType = ImgSaver::ImgHicolor;
    else
    {
        if (image->depth()==1 || image->numColors()==2) imgType = ImgSaver::ImgBW;
        else
        {
            if (image->allGray()) imgType = ImgSaver::ImgGray;
            else imgType = ImgSaver::ImgColor;
        }
    }

    QString saveFilename = createFilename();		// find next unused filename
    QString saveFormat = findFormat(imgType);		// find saved image format
    QString saveSubformat = findSubFormat(saveFormat);	// currently not used
							// get dialogue preferences
    const KConfigGroup grp = KGlobal::config()->group(OP_SAVER_GROUP);
    m_saveAskFilename = grp.readEntry(OP_SAVER_ASK_FILENAME, false);
    m_saveAskFormat = grp.readEntry(OP_SAVER_ASK_FORMAT, false);

    kDebug() << "before dialogue,"
             << "ask_filename=" << m_saveAskFilename
             << "ask_format=" << m_saveAskFormat
             << "filename=" << saveFilename
             << "format=" << saveFormat
             << "subformat=" << saveSubformat;

    while (saveFormat.isEmpty() || m_saveAskFormat || m_saveAskFilename)
    {							// is a dialogue neeeded?
        FormatDialog fd(NULL,imgType,m_saveAskFormat,saveFormat,m_saveAskFilename,saveFilename);
        if (!fd.exec()) return (ISS_SAVE_CANCELED);	// do the dialogue

        saveFilename = fd.getFilename();		// get filename as entered
        if (fd.useAssistant())				// redo with format options
        {
            m_saveAskFormat = true;
            continue;
        }

        saveFormat = fd.getFormat();			// get results from that
        saveSubformat = fd.getSubFormat();

        if (!saveFormat.isEmpty())			// have a valid format
        {
            if (fd.alwaysUseFormat()) storeFormatForType(imgType,saveFormat);
            break;					// save format for future
        }
    }

    kDebug() << "after dialogue,"
             << "filename=" << saveFilename
             << "format=" << saveFormat
             << "subformat=" << saveSubformat;

    QString fi = m_saveDirectory+"/"+saveFilename;	// full path to save
#ifdef USE_KIMAGEIO
    QString ext = KImageIO::suffix(saveFormat);		// extension it should have
#else
    QString ext = saveFormat.toLower();
#endif
    if (extension(fi)!=ext)				// already has correct extension?
    {
        fi +=  ".";					// no, add it on
        fi += ext;
    }
							// save image to that file
    return (save(image, fi, saveFormat, saveSubformat));
}



/**
 *   This function uses a filename provided by the caller.
 **/
ImgSaveStat ImgSaver::saveImage(const QImage *image,
                                const KUrl &url,
                                const QString &imgFormat)
{
    QString format = imgFormat;
    if (format.isEmpty()) format = "BMP";

    return (save(image, url, format));
}


/**
   private save() does the work to save the image.
   the filename must be complete and local.
**/
ImgSaveStat ImgSaver::save(const QImage *image,
                           const KUrl &url,
                           const QString &format,
                           const QString &subformat)
{
    if (format.isEmpty() || image==NULL) return (ISS_ERR_PARAM);

    kDebug() << "to" << url.prettyUrl() << "format" << format << "subformat" << subformat;

    last_format = format.toLatin1();			// save for error message later
    last_url = url;

    if (!url.isLocalFile())				// file must be local
    {
	kDebug() << "Can only save local files";
	return (ISS_ERR_PROTOCOL);
    }

    QString filename = url.path();			// local file path
    QFileInfo fi(filename);				// information for that
    QString dirPath = fi.path();			// containing directory

    QDir dir(dirPath);
    if (!dir.exists())					// should always exist, except
    {							// for first preview save
        kDebug() << "Creating directory" << dirPath;
        if (!dir.mkdir(dirPath))
        {
	    kDebug() << "Could not create directory" << dirPath;
            return (ISS_ERR_MKDIR);
        }
    }

    if (fi.exists() && !fi.isWritable())
    {
        kDebug() << "Cannot overwrite existing file" << filename;
        return (ISS_ERR_PERM);
    }

#ifdef USE_KIMAGEIO
    if (!KImageIO::canWrite(format))			// check the format, is it writable?
    {
        kDebug() << "Cannot write format" << format;
        return (ISS_ERR_FORMAT_NO_WRITE);
    }
#endif

    bool result = image->save(filename, last_format);
    return (result ? ISS_OK : ISS_ERR_UNKNOWN);
}


/**
 *  Find the next filename to use for the image to save.
 *  This is done by enumerating and checking against all existing files,
 *  regardless of format - because we have not resolved the format yet.
 **/
QString ImgSaver::createFilename()
{
    QDir files(m_saveDirectory,"kscan_[0-9][0-9][0-9][0-9].*");
    QStringList l(files.entryList());
    l.replaceInStrings(QRegExp("\\..*$"),"");

    QString fname;
    for (int c = 1; c<=l.count()+1; ++c)		// that must be the upper bound
    {
	fname = "kscan_"+QString::number(c).rightJustified(4, '0');
	if (!l.contains(fname)) break;
    }

    kDebug() << "returning" << fname;
    return (fname);
}


/*
 * findFormat looks to see if there is a previously saved file format for
 * the image type in question.
 */

QString ImgSaver::findFormat(ImgSaver::ImageType type)
{
    if (type==ImgSaver::ImgThumbnail) return ("BMP");	// thumbnail always this format
    if (type==ImgSaver::ImgPreview) return ("BMP");	// preview always this format
							// real images from here on
    QString format = getFormatForType(type);
    kDebug() << "format for type" << type << "=" << format;
    return (format);
}






QString ImgSaver::picTypeAsString(ImgSaver::ImageType type)
{
    QString res;

    switch (type)
    {
case ImgSaver::ImgColor:
        res = i18n("indexed color image (up to 8 bit depth)");
	break;

case ImgSaver::ImgGray:
	res = i18n("gray scale image (up to 8 bit depth)");
	break;

case ImgSaver::ImgBW:
	res = i18n("lineart image (black and white, 1 bit depth)");
	break;

case ImgSaver::ImgHicolor:
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
bool ImgSaver::isRememberedFormat(ImgSaver::ImageType type, const QString &format)
{
    return (getFormatForType(type)==format);
}




const char *configKeyFor(ImgSaver::ImageType type)
{
    switch (type)
    {
case ImgSaver::ImgColor:    return (OP_FORMAT_COLOR);
case ImgSaver::ImgGray:     return (OP_FORMAT_GRAY);
case ImgSaver::ImgBW:       return (OP_FORMAT_BW);
case ImgSaver::ImgHicolor:  return (OP_FORMAT_HICOLOR);

default:                    kDebug() << "unknown type" << type;
                            return (OP_FORMAT_UNKNOWN);
    }
}



QString ImgSaver::getFormatForType(ImgSaver::ImageType type)
{
    const KConfigGroup grp = KGlobal::config()->group(OP_SAVER_GROUP);
    return (grp.readEntry(configKeyFor(type), ""));
}


void ImgSaver::storeFormatForType(ImgSaver::ImageType type,const QString &format)
{
    KConfigGroup grp = KGlobal::config()->group(OP_SAVER_GROUP);

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
    //
    //  konf->writeEntry( OP_FILE_ASK_FORMAT, ask );
    //  m_saveAskFormat = ask;

    grp.writeEntry(configKeyFor(type), format);
    grp.sync();
}



QString ImgSaver::findSubFormat(const QString &format)
{
    kDebug() << "for" << format;
    return (QString::null);				// no subformats currently used
}



QString ImgSaver::errorString(ImgSaveStat stat)
{
    QString re;
    switch (stat)
    {
case ISS_OK:			re = i18n("Save OK");			break;
case ISS_ERR_PERM:		re = i18n("Permission denied");		break;
case ISS_ERR_FILENAME:		re = i18n("Bad file name");		break;  // never used
case ISS_ERR_NO_SPACE:		re = i18n("No space left on device");	break;	// never used
case ISS_ERR_FORMAT_NO_WRITE:	re = i18n("Cannot write image format '%1'",last_format.data());
									break;
case ISS_ERR_PROTOCOL:		re = i18n("Cannot write using protocol '%1'",last_url.protocol());
									break;
case ISS_SAVE_CANCELED:		re = i18n("User cancelled saving");	break;
case ISS_ERR_MKDIR:		re = i18n("Cannot create directory");	break;
case ISS_ERR_UNKNOWN:		re = i18n("Save failed");		break;
case ISS_ERR_PARAM:		re = i18n("Bad parameter");		break;
default:			re = i18n("Unknown status %1",stat);
				break;
    }
    return (re);
}


QString ImgSaver::extension(const KUrl &url)
{
    return (KMimeType::extractKnownExtension(url.pathOrUrl()));
//    QString extension = url.fileName();
//    int dotPos = extension.findRev('.');		// find last separator
//    if( dotPos>0) extension = extension.mid(dotPos+1);	// extract from filename
//    else extension = QString::null;			// no extension
//    return (extension);
}


QString ImgSaver::tempSaveImage(const KookaImage *img, const QString &format, int colors)
{
    if (img==NULL) return (QString::null);

    KTemporaryFile tmpFile;
    tmpFile.setSuffix("."+format.toLower());
    tmpFile.setAutoRemove(false);

    if (!tmpFile.open())
    {
        kDebug() << "Error opening temp file" << tmpFile.fileName();
        tmpFile.setAutoRemove(true);
        return (QString::null);
    }

    QString name = tmpFile.fileName();
    tmpFile.close();

    KookaImage tmpImg;
    if (colors!=-1 && img->numColors()!=colors)
    {
	// Need to convert image
        QImage::Format newfmt;
        switch (colors)
        {
case 1:     newfmt = QImage::Format_Mono;
            break;

case 8:     newfmt = QImage::Format_Indexed8;
            break;


case 24:    newfmt = QImage::Format_RGB888;
            break;


case 32:    newfmt = QImage::Format_RGB32;
            break;

default:    kDebug() << "Error: Bad color depth requested" << colors;
            tmpFile.setAutoRemove(true);
            return (QString::null);
        }

        tmpImg = img->convertToFormat(newfmt);
        img = &tmpImg;
    }

    kDebug() << "Saving to" << name << "in format" << format.toUpper();
    if (!img->save(name, format.toLatin1()))
    {
        kDebug() << "Error saving to" << name;
        tmpFile.setAutoRemove(true);
        return (QString::null);
    }

    return (name);
}




// TODO: are copy/rename similar enough to merge?

bool ImgSaver::renameImage(const KUrl &fromUrl, const KUrl &toUrl, bool askExt, QWidget *overWidget)
{
    /* Check if the provided filename has a extension */
    QString extFrom = extension(fromUrl);
    QString extTo = extension(toUrl);
    KUrl targetUrl(toUrl);

    if (extTo.isEmpty() && !extFrom.isEmpty())
    {
        /* Ask if the extension should be added */
        int result = KMessageBox::Yes;
        QString fName = toUrl.fileName();
        if (!fName.endsWith( "." )) fName += ".";
        fName += extFrom;

        if (askExt)
        {
            result = KMessageBox::questionYesNo(overWidget,
                                                i18n("<qt><p>The file name you supplied has no file extension."
                                                     "<br>Should the original one be added?\n"
                                                     "<p>That would result in the new file name <filename>%1</filename>", fName),
                                                i18n("Extension Missing"),
                                                KGuiItem(i18n("Add Extension")),
                                                KGuiItem(i18n("Do Not Add")),
                                                "AutoAddExtensions");
      }

      if (result == KMessageBox::Yes)
      {
          targetUrl.setFileName( fName );
          kDebug() << "Rename file to" << targetUrl.prettyUrl();
      }
    }
    else if(!extFrom.isEmpty() && extFrom!=extTo)
    {
        KMimeType::Ptr fromType = KMimeType::findByUrl(fromUrl);
        KMimeType::Ptr toType = KMimeType::findByUrl(toUrl);
        if (!toType->is(fromType->name()))
        {
            KMessageBox::error(overWidget,
                               i18n("Format changes of images are currently not supported."),
			       i18n("Wrong Extension"));
            return (false);
        }
    }

    if (KIO::NetAccess::exists(targetUrl, false, overWidget))
    {
        kDebug() << "Target already exists" << targetUrl;
        return (false);
    }

    return (KIO::NetAccess::move(fromUrl, targetUrl, overWidget));
}





bool ImgSaver::copyImage(const KUrl &fromUrl, const KUrl &toUrl, QWidget *overWidget)
{
    /* Check if the provided filename has a extension */
    QString extFrom = extension(fromUrl);
    QString extTo = extension(toUrl);
    KUrl targetUrl(toUrl);

    if (extTo.isEmpty() && !extFrom.isEmpty())
    {
        /* Ask if the extension should be added */
        QString fName = toUrl.fileName();
        if(!fName.endsWith(".")) fName += ".";
        fName += extFrom;

        int result = KMessageBox::questionYesNo(overWidget,
                                                i18n("<qt><p>The file name you supplied has no file extension."
                                                     "<br>Should the original one be added?\n"
                                                     "<p>That would result in the new file name <filename>%1</filename>", fName),
                                                i18n("Extension Missing"),
                                                KGuiItem(i18n("Add Extension")),
                                                KGuiItem(i18n("Do Not Add")),
                                                "AutoAddExtensions");
        if (result==KMessageBox::Yes)
        {
            targetUrl.setFileName(fName);
            kDebug() << "Copy to" << targetUrl.prettyUrl();
        }
    }
    else if (!extFrom.isEmpty() && extFrom!=extTo)
    {
        KMimeType::Ptr fromType = KMimeType::findByUrl(fromUrl);
        KMimeType::Ptr toType = KMimeType::findByUrl(toUrl);
        if (!toType->is(fromType->name()))
        {
            KMessageBox::error(overWidget,
                               i18n("Format changes of images are currently not supported."),
			       i18n("Wrong Extension"));
	   return (false);
       }
   }

    // TODO: need an 'exists' overwrite check as above?
    return (KIO::NetAccess::copy(fromUrl, targetUrl, overWidget));
}