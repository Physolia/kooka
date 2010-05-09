/* This file is part of the KDE Project
   Copyright (C) 1999 Klaas Freitag <freitag@suse.de>

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
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#include "kscandevice.h"
#include "kscandevice.moc"

#include <qimage.h>
#include <qfileinfo.h>
#include <qapplication.h>
#include <qsocketnotifier.h>

#include <kdebug.h>
#include <klocale.h>
#include <kglobal.h>
#include <kconfig.h>
#include <kstandarddirs.h>
#include <kpassworddialog.h>

#include "scanglobal.h"
#include "scandevices.h"
#include "kgammatable.h"
#include "kscancontrols.h"
#include "kscanoption.h"
#include "kscanoptset.h"
#include "deviceselector.h"
#include "imgscaninfo.h"


#define MIN_PREVIEW_DPI		75
#define MAX_PROGRESS		100

#undef DEBUG_OPTIONS					// define to show before scan


// global data, see TODO in kscandevice.h
SANE_Handle KScanDevice::gScannerHandle = NULL;
KScanDevice::OptionDict *KScanDevice::option_dic = NULL;
KScanOptSet *KScanDevice::gammaTables = NULL;


/* ---------------------------------------------------------------------------
   Private class for KScanDevice
   ------------------------------------------------------------------------- */
class KScanDevice::KScanDevicePrivate

{
public:
    KScanDevicePrivate()
	: currScanResolutionX(0),
	  currScanResolutionY(0)
	{
	    
	}

    int currScanResolutionX, currScanResolutionY;
    
};


/* ---------------------------------------------------------------------------

   ------------------------------------------------------------------------- */
void KScanDevice::guiSetEnabled( const QByteArray& name, bool state )
{
    KScanOption *so = getExistingGuiElement( name );

    if( so )
    {
	QWidget *w = so->widget();

	if( w )
	    w->setEnabled( state );
    }
}


/* ---------------------------------------------------------------------------

   ------------------------------------------------------------------------- */
KScanOption *KScanDevice::getExistingGuiElement( const QByteArray& name )
{
    KScanOption *ret = NULL;
    QByteArray alias = aliasName(name);

    for (QList<KScanOption *>::const_iterator it = mGuiElements.constBegin();
         it!=mGuiElements.constEnd(); ++it)
    {
        KScanOption *opt = (*it);
        if (opt->getName()==alias)
        {
            ret = opt;
            break;
        }
    }

    return (ret);
}
/* ---------------------------------------------------------------------------

   ------------------------------------------------------------------------- */

// TODO: no need to pass desc and tooltip here, can get them from SANE
// via the KScanOption
KScanOption *KScanDevice::getGuiElement(const QByteArray &name,
                                        QWidget *parent,
                                        const QString &desc,
                                        const QString &tooltip )
{
   if (name.isEmpty()) return (NULL);
   if (!optionExists(name)) return (NULL);

   QByteArray alias = aliasName( name );

   kDebug() << "for name" << name << "desc" << desc << "alias" << alias;

   /* Check if already exists */
   KScanOption *so = getExistingGuiElement( name );

   if( so ) return( so );

   /* ...else create a new one */
   so = new KScanOption( alias );

   if( so->valid() && so->softwareSetable())
   {
      /** store new gui-elem in list of all gui-elements */
      mGuiElements.append( so );

      QWidget *w = so->createWidget( parent, desc, tooltip );
      if( w )
      {
	 connect( so,   SIGNAL( optionChanged( KScanOption* ) ),
		  this, SLOT(   slotOptChanged( KScanOption* )));
	 w->setEnabled( so->active() );
      }
      else
      {
	 kDebug() << "Error: No widget created for" << name;
      }
   }
   else
   {
      if( !so->valid())
	 kDebug() << "no option" << alias;
      else
      if( !so->softwareSetable())
	 kDebug() << "option" << alias << "is not Software Settable";

      delete so;
      so = 0;
   }

   return( so );
}


//  Constructor/destructor
//  ----------------------

KScanDevice::KScanDevice(QObject *parent)
   : QObject(parent)
{
    kDebug();

    d = new KScanDevicePrivate();

    /* Get SANE translations - bug 98150 */
    KGlobal::dirs()->addResourceDir( "locale", QString::fromLatin1("/usr/share/locale/") );
    KGlobal::locale()->insertCatalog(QString::fromLatin1("sane-backends"));

    ScanGlobal::self()->init();				// do sane_init() first of all

    option_dic = new OptionDict();

    gScannerHandle = NULL;
    mScannerInitialised = false;			// is device opened yet?
    mScanningState = KScanDevice::ScanIdle;

    mScanBuf = NULL;					// image data buffer while scanning
    mScanImage = NULL;					// temporary image to scan into
    mSocketNotifier = NULL;				// socket notifier for async scanning
    mSavedOptions = NULL;				// options to save during preview
    mBytesRead = 0;
    mBytesUsed = 0;
    mPixelX = 0;
    mPixelY = 0;

    mScannerName = "";

    gammaTables = new KScanOptSet( "GammaTables" );

    connect( this, SIGNAL( sigScanFinished( KScanDevice::Status )), SLOT( slotScanFinished( KScanDevice::Status )));
}


KScanDevice::~KScanDevice()
{
    kDebug();

    ScanGlobal::self()->setScanDevice(NULL);		// going away, don't call me

    delete mSavedOptions;
    delete d;
}


//  Opening/closing the scanner device
//  ----------------------------------

KScanDevice::Status KScanDevice::openDevice(const QByteArray &backend)
{
    KScanDevice::Status stat = KScanDevice::Ok;

    kDebug() << "backend" << backend;

    mSaneStatus = SANE_STATUS_UNSUPPORTED;
    if (backend.isEmpty()) return (KScanDevice::ParamError);

    // search for scanner
    if (ScanDevices::self()->deviceInfo(backend)==NULL) return (KScanDevice::NoDevice);

    mScannerName = backend;				// set now for authentication
    QApplication::setOverrideCursor(Qt::WaitCursor);	// potential lengthy operation
    ScanGlobal::self()->setScanDevice(this);		// for possible authentication
    mSaneStatus = sane_open(backend, &gScannerHandle);

    if (mSaneStatus==SANE_STATUS_ACCESS_DENIED)		// authentication failed?
    {
        clearSavedAuth();				// clear any saved password
        kDebug() << "retrying authentication";		// try again once more
        mSaneStatus = sane_open(backend, &gScannerHandle);
    }

    if (mSaneStatus==SANE_STATUS_GOOD)
    {
        stat = find_options();				// fill dictionary with options
        mScannerInitialised = true;			// note scanner opened OK
    }
    else
    {
        stat = KScanDevice::OpenDevice;
        mScannerName = "";
    }

    QApplication::restoreOverrideCursor();
    return (stat);
}




// TODO: does this need to be a slot?
void KScanDevice::slotCloseDevice()
{
   /* First of all, send a signal to close down the scanner dev. */
   emit( sigCloseDevice( ));

   kDebug() << "Saving scan settings";
   slotSaveScanConfigSet( DEFAULT_OPTIONSET, i18n("the default startup setup"));

   /* After return, delete all related stuff */
   mScannerName = "";
   if( gScannerHandle )
   {
      if( mScanningState != KScanDevice::ScanIdle )
      {
         kDebug() << "Scanner is still active, calling cancel!";
         sane_cancel( gScannerHandle );
      }
      sane_close( gScannerHandle );
      gScannerHandle = NULL;
   }

   qDeleteAll(mGuiElements);
   mGuiElements.clear();

   option_dic->clear();
   mScannerInitialised = false;
}


//  Scanner and image information
//  -----------------------------

QString KScanDevice::scannerDescription() const
{
    QString ret;

    if (!mScannerName.isNull() && mScannerInitialised)
    {
        ret = ScanDevices::self()->deviceDescription(mScannerName);
    }
    else
    {
        ret = i18n("No scanner selected");
    }

    kDebug() << "returning" << ret;
    return (ret);
}


QSize KScanDevice::getMaxScanSize( void ) const
{
   QSize s;
   double min, max, q;

   KScanOption so_w( SANE_NAME_SCAN_BR_X );
   so_w.getRange( &min, &max, &q );

   s.setWidth( (int) max );

   KScanOption so_h( SANE_NAME_SCAN_BR_Y );
   so_h.getRange( &min, &max, &q );

   s.setHeight( (int) max );

   return( s );
}


void KScanDevice::getCurrentFormat(int *format, int *depth)
{
    sane_get_parameters(gScannerHandle, &mSaneParameters);
    *format = mSaneParameters.format;
    *depth = mSaneParameters.depth;
}


//  Listing the available options
//  -----------------------------

KScanDevice::Status KScanDevice::find_options()
{
   KScanDevice::Status 	stat = KScanDevice::Ok;
   SANE_Int 	n;
   SANE_Int 	opt;

  SANE_Option_Descriptor *d;

  if( sane_control_option(gScannerHandle, 0,SANE_ACTION_GET_VALUE, &n, &opt)
      != SANE_STATUS_GOOD )
     stat = KScanDevice::ControlError;

  // printf("find_options(): Found %d options\n", n );

  // resize the Array which hold the descriptions
  if( stat == KScanDevice::Ok )
  {

     option_dic->clear();

     for(int i = 1; i<n; i++)
     {
	d = (SANE_Option_Descriptor*)
	   sane_get_option_descriptor( gScannerHandle, i);

	if( d!=NULL )
	{
	   // logOptions( d );
	   if (d->name!=NULL)
	   {
	      // Die Option anhand des Namen in den Dict

	      if( strlen( d->name ) > 0 )
	      {
		 kDebug() << "Inserting" << d->name << "as" << i;
		 /* create a new option in the set. */
		 option_dic->insert ( d->name, i );
		 mOptionList.append( (const char*) d->name );
#if 0
		 KScanOption *newOpt = new KScanOption( d->name );
		 const QString qq = newOpt->get();
		 qDebug( "INIT: <%s> = <%s>", d->name, qq );
		 allOptionSet->insert( d->name, newOpt );
#endif
	      }
	      else if( d->type == SANE_TYPE_GROUP )
	      {
	      	// qDebug( "######### Group found: %s ########", d->title );
	      }
	      else
		 kDebug() << "Unable to detect option";
	   }
	}
     }
  }
  return stat;
}


QList<QByteArray> KScanDevice::getAllOptions()
{
    return (mOptionList);
}


QList<QByteArray> KScanDevice::getCommonOptions()
{
    QList<QByteArray> opts;

    for (QList<QByteArray>::const_iterator it = mOptionList.constBegin();
         it!=mOptionList.constEnd(); ++it)
    {
        const QByteArray optname = (*it);
        KScanOption opt(optname);
        if (opt.commonOption()) opts.append(optname);
    }

    return (opts);
}


QList<QByteArray> KScanDevice::getAdvancedOptions()
{
    QList<QByteArray> opts;

    for (QList<QByteArray>::const_iterator it = mOptionList.constBegin();
         it!=mOptionList.constEnd(); ++it)
    {
        const QByteArray optname = (*it);
        KScanOption opt(optname);
        if (!opt.commonOption()) opts.append(optname);
    }

    return (opts);
}


//  Controlling options
//  -------------------

KScanDevice::Status KScanDevice::apply( KScanOption *opt, bool isGammaTable )
{
   KScanDevice::Status   stat = KScanDevice::Ok;
   if( !opt ) return( KScanDevice::ParamError );
   int sane_result = 0;

   int         val = option_dic->value(opt->getName());
   mSaneStatus = SANE_STATUS_GOOD;
   const QByteArray& oname = opt->getName();

   if ( oname == "preview" || oname == "mode" ) {
	  mSaneStatus = sane_control_option( gScannerHandle, val,
				       SANE_ACTION_SET_AUTO, 0,
				       &sane_result );
      /* No return here, please ! Carsten, does it still work than for you ? */
   }


   if( ! opt->initialised() || opt->getBuffer() == 0 )
   {
      kDebug() << "Attempt to set uninit/null buffer of" << oname << "-> skipping!";

      if( opt->autoSetable() )
      {
	 kDebug() << "Setting option" << oname << "automatic";
	 mSaneStatus = sane_control_option( gScannerHandle, val,
					  SANE_ACTION_SET_AUTO, 0,
					  &sane_result );
      }
      else
      {
	 mSaneStatus = SANE_STATUS_INVAL;
      }
      stat = KScanDevice::ParamError;
   }
   else
   {
      if( ! opt->active() )
      {
	 kDebug() << "Option" << oname << "is not active";
	 stat = KScanDevice::OptionNotActive;
      }
      else if( ! opt->softwareSetable() )
      {
	 kDebug() << "Option" << oname << "is not Software Settable";
	 stat = KScanDevice::OptionNotActive;
      }
      else
      {

	 mSaneStatus = sane_control_option( gScannerHandle, val,
					  SANE_ACTION_SET_VALUE,
					  opt->getBuffer(),
					  &sane_result );
      }
   }

   if( stat == KScanDevice::Ok )
   {
      if( mSaneStatus == SANE_STATUS_GOOD )
      {
	 kDebug() << "Applied" << oname << "successfully";

	 if( sane_result & SANE_INFO_RELOAD_OPTIONS )
	 {
	    kDebug() << "Setting status to reload options";
	    stat = KScanDevice::Reload;
#if 0
	    qDebug( "Emitting sigOptionChanged()" );
	    emit( sigOptionsChanged() );
#endif
	 }

#if 0
	 if( sane_result & SANE_INFO_RELOAD_PARAMS )
	    emit( sigScanParamsChanged() );
#endif
	 if( sane_result & SANE_INFO_INEXACT )
	 {
	    kDebug() << "Option" << oname << "was set inexact";
	 }

	 /* if it is a gamma table, the gamma values must be stored */
	 if( isGammaTable )
	 {
	    gammaTables->backupOption( *opt );
	    kDebug() << "GammaTable stored:" << opt->getName();
	 }
      }
      else
      {
	 kDebug() << "Bad SANE status" << lastSaneErrorMessage() << "for option" << oname;

      }
   }
   else
   {
      kDebug() << "Setting option" << oname << "failed";
   }

   if( stat == KScanDevice::Ok )
   {
      slotSetDirty( oname );
   }

   return( stat );
}

bool KScanDevice::optionExists( const QByteArray& name )
{
   if( name.isEmpty() ) return false;

   bool ret = false;

   QByteArray altname = aliasName( name );
   if( ! altname.isNull() )
   {
	   int i = option_dic->value(altname, -1);
	   ret = (i > -1);
   }

   return ret;
}


// TODO: does this need to be a slot?
void KScanDevice::slotSetDirty(const QByteArray &name)
{
    if (optionExists(name))
    {
        if (!mDirtyList.contains(name))
        {
            kDebug() << "Setting dirty" << name;
            mDirtyList.append(name);
        }
    }
}


/* This function tries to find name aliases which appear from backend to backend.
 *  Example: Custom-Gamma is for epson backends 'gamma-correction' - not a really
 *  cool thing :-|
 *  Maybe this helps us out ?
 */
QByteArray KScanDevice::aliasName( const QByteArray& name )
{
	if (option_dic->contains(name))
		return name;

	QByteArray ret = name;
    if( name == SANE_NAME_CUSTOM_GAMMA )
    {
		if (option_dic->contains("gamma-correction"))
			ret = "gamma-correction";
    }

    if( ret != name )
		kDebug() << "Found alias for" << name << "which is" << ret;

    return( ret );
}



/* Nothing to do yet. This Slot may get active to do same user Widget changes */
void KScanDevice::slotOptChanged(KScanOption *opt)
{
    kDebug() << "for option" << opt->getName();
    apply(opt);
}



/* This might result in a endless recursion ! */
void KScanDevice::slotReloadAllBut(KScanOption *not_opt)
{
    if (not_opt==NULL)
    {
        kDebug() << "called with invalid argument";
        return;
    }

    kDebug() << "Reload of all except" << not_opt->getName() << "forced";
    /* Make sure it's applied */
    apply(not_opt);

    for (QList<KScanOption *>::const_iterator it = mGuiElements.constBegin();
         it!=mGuiElements.constEnd(); ++it)
    {
        KScanOption *so = (*it);
        if (so!=not_opt)
        {
            kDebug() << "Reloading" << so->getName();
            so->slotReload();
            so->slotRedrawWidget(so);
        }
    }

    kDebug() << "Finished";
}



/* This might result in a endless recursion ! */
void KScanDevice::slotReloadAll()
{
    kDebug();

    for (QList<KScanOption *>::const_iterator it = mGuiElements.constBegin();
         it!=mGuiElements.constEnd(); ++it)
    {
        KScanOption *so = (*it);
        so->slotReload();
        so->slotRedrawWidget(so);
    }
}


//  Scanning control
//  ----------------

void KScanDevice::slotStopScanning()
{
    kDebug() << "Attempt to stop scanning";
    if (mScanningState==KScanDevice::ScanInProgress) emit sigScanFinished(KScanDevice::Cancelled);
    mScanningState = KScanDevice::ScanStopNow;
}


//  Preview image
//  -------------

const QString KScanDevice::previewFile()
{
    // TODO: this doesn't work if that directory doesn't exist,
    // and nothing ever creates that directory!
    // Do we want this feature to work?  If so, remove the 'false' argument below.
    QString dir = KGlobal::dirs()->saveLocation("data", "previews/", false);
    QString sname(scannerDescription());
    sname.replace( '/', "_");
    return (dir+sname);
}


QImage KScanDevice::loadPreviewImage()
{
   const QString prevFile = previewFile();

   kDebug() << "Loading preview from" << prevFile;
   return (QImage(prevFile));
}


bool KScanDevice::savePreviewImage(const QImage &image)
{
    if (image.isNull()) return (false);

   const QString prevFile = previewFile();

   kDebug() << "Saving preview to" << prevFile;
   return (image.save(prevFile, "BMP"));
}


//  Displaying scan options
//  -----------------------
//
//  For debugging.  Originally showOptions() was called prepareScan() and had
//  the comment:
//
//    prepareScan tries to set as much as parameters as possible.
//
//    Function which applies all Options which need to be applied.
//    See SANE-Documentation Table 4.5, description for SANE_CAP_SOFT_DETECT.
//    The function sets the options which have SANE_CAP_AUTOMATIC set
//    to automatic adjust.
//
//  But this wasn't true - it only reports the current state of the options.

#ifdef DEBUG_OPTIONS

inline const char *optionNotifyString(int opt)
{
    return (opt!=0 ? "X  |" : "-  |");
}


void KScanDevice::showOptions()
{
    kDebug() << "######################################################################";
    kDebug() << "Scanner" << mScannerName;
    kDebug() << "----------------------------------+----+----+----+----+----+----+----+";
    kDebug() << " Option-Name                      |SSEL|HSEL|SDET|EMUL|AUTO|INAC|ADVA|";
    kDebug() << "----------------------------------+----+----+----+----+----+----+----+";

    for (OptionDict::const_iterator it = option_dic->constBegin();
         it!=option_dic->constEnd(); ++it)
    {
        int idx = it.value();
        const SANE_Option_Descriptor *desc = sane_get_option_descriptor(gScannerHandle, idx);
        if (desc!=NULL) continue;

        int cap = desc->cap;
        QString s = QString(it.key()).leftJustified(32);
        kDebug() << s << "|" <<
            optionNotifyString((cap & SANE_CAP_SOFT_SELECT)) << 
            optionNotifyString((cap & SANE_CAP_HARD_SELECT)) << 
            optionNotifyString((cap & SANE_CAP_SOFT_DETECT)) << 
            optionNotifyString((cap & SANE_CAP_EMULATED)) << 
            optionNotifyString((cap & SANE_CAP_AUTOMATIC)) << 
            optionNotifyString((cap & SANE_CAP_INACTIVE)) << 
            optionNotifyString((cap & SANE_CAP_ADVANCED));
    }
    kDebug() << "----------------------------------+----+----+----+----+----+----+----+";

    KScanOption pso(SANE_NAME_PREVIEW);
    kDebug() << "Preview-Switch is" << pso.get();
}

#endif							// DEBUG_OPTIONS


//  Creating a new image to receive the scan/preview
//  ------------------------------------------------

KScanDevice::Status KScanDevice::createNewImage(const SANE_Parameters *p)
{
    if (p==NULL) return (KScanDevice::ParamError);
    KScanDevice::Status stat = KScanDevice::Ok;

    delete mScanImage;
    mScanImage = NULL;

    if (p->depth==1)					//  Line art (bitmap)
    {
        mScanImage = new QImage(p->pixels_per_line,p->lines,QImage::Format_Mono);
        if (mScanImage!=NULL)
        {
            mScanImage->setColor(0,qRgb(0x00,0x00,0x00));
            mScanImage->setColor(1,qRgb(0xFF,0xFF,0xFF));
        }
    }
    else if (p->depth==8)				// 8 bit RGB
    {
        if (p->format==SANE_FRAME_GRAY)			// Grey scale
        {
            mScanImage = new QImage(p->pixels_per_line,p->lines,QImage::Format_Indexed8);
            if (mScanImage!=NULL)
            {
                for (int i = 0; i<256; i++) mScanImage->setColor(i,qRgb(i,i,i));
            }
        }
        else						// True colour
        {
            mScanImage = new QImage(p->pixels_per_line,p->lines,QImage::Format_RGB32);
        }
    }
    else						// Error, no others supported
    {
        kDebug() << "Only bit depths 1 or 8 supported!";
        stat = KScanDevice::ParamError;
    }

    if (stat==KScanDevice::Ok && mScanImage==NULL) stat = KScanDevice::NoMemory;
    return (stat);
}


//  Acquiring preview/scan image
//  ----------------------------

KScanDevice::Status KScanDevice::acquirePreview( bool forceGray, int dpi )
{
   double min, max, q;

   (void) forceGray;

    if (mSavedOptions!=NULL) mSavedOptions->clear();
    else mSavedOptions = new KScanOptSet("TempStore");

   /* set Preview = ON if exists */
   if( optionExists( SANE_NAME_PREVIEW ) )
   {
      KScanOption prev( aliasName(SANE_NAME_PREVIEW) );

      prev.set( true );
      apply( &prev );

      /* after having applied, save set to false to switch preview mode off after
	 scanning */
      prev.set( false );
      mSavedOptions->backupOption( prev );
   }

   /* Gray-Preview only  done by widget ? */
   if( optionExists( SANE_NAME_GRAY_PREVIEW ))
   {
     KScanOption *so = getExistingGuiElement( SANE_NAME_GRAY_PREVIEW );
     if( so )
     {
       if( so->get() == "true" )
       {
	 /* Gray preview on */
	 so->set( true );
	 kDebug() << "Setting GrayPreview ON";
       }
       else
       {
	 so->set(false );
	 kDebug() << "Setting GrayPreview OFF";
       }
     }
     apply( so );
   }


   if( optionExists( SANE_NAME_SCAN_MODE ) )
   {
      KScanOption mode( SANE_NAME_SCAN_MODE );
      const QString kk = mode.get();
      kDebug() << "Scan Mode is" << kk;
      mSavedOptions->backupOption( mode );
      /* apply if it has a widget, or apply always ? */
      if( mode.widget() ) apply( &mode );
   }

   /** Scan Resolution should always exist. **/
   KScanOption res ( SANE_NAME_SCAN_RESOLUTION );
   const QString p = res.get();

   kDebug() << "Scan Resolution (pre-preview) is" << p;
   mSavedOptions->backupOption( res );

   int set_dpi = dpi;

   if( dpi == 0 )
   {
       /* No resolution argument */
       if( ! res.getRange( &min, &max, &q ) )
       {
	   if( ! res.getRangeFromList ( &min, &max, &q ) )
	   {
	       kDebug() << "Could not retrieve resolution range!";
	       min = 75.0; // Hope that every scanner can 75
	   }
       }
       kDebug() << "Minimum Range" << min << "Maximum Range" << max;

      if( min > MIN_PREVIEW_DPI )
	 set_dpi = (int) min;
      else
	 set_dpi = MIN_PREVIEW_DPI;
   }

   /* Set scan resolution for preview. */
   if( !optionExists( SANE_NAME_SCAN_Y_RESOLUTION ) )
      d->currScanResolutionY = 0;
   else
   {
      KScanOption yres ( SANE_NAME_SCAN_Y_RESOLUTION );
      /* if active ? */
      mSavedOptions->backupOption( yres );
      yres.set( set_dpi );
      apply( &yres );
      yres.get( &d->currScanResolutionY );

      /* Resolution bind switch ? */
      if( optionExists( SANE_NAME_RESOLUTION_BIND ) )
      {
	 KScanOption bind_so( SANE_NAME_RESOLUTION_BIND );
	 /* Switch binding on if available */
	 mSavedOptions->backupOption( bind_so );
	 bind_so.set( true );
	 apply( &bind_so );
      }
   }

   res.set( set_dpi );
   apply( &res );

   /* Store the resulting preview for additional image information */
   res.get( &d->currScanResolutionX );

   if ( d->currScanResolutionY == 0 )
      d->currScanResolutionY = d->currScanResolutionX;

   /* Start scanning */
   KScanDevice::Status stat = acquireData( true );

   /* Restauration of the previous values must take place in the scanfinished slot,
    *  because scanning works asynchron now.
    */

   return( stat );
}


/** Starts scanning
 *  depending on if a filename is given or not, the function tries to open
 *  the file using the Qt-Image-IO or really scans the image.
 **/
KScanDevice::Status KScanDevice::acquireScan(const QString &filename)
{
    if( filename.isEmpty() )
    {
 	/* *real* scanning - apply all Options and go for it */
#ifdef DEBUG_OPTIONS
 	showOptions();
#endif
        for (QList<KScanOption *>::const_iterator it = mGuiElements.constBegin();
             it!=mGuiElements.constEnd(); ++it)
        {
            KScanOption *so = (*it);
 	    if (so->active())
 	    {
 	         kDebug() << "apply" << so->getName();
 	         apply( so );
 	    }
 	    else
 	    {
 	        kDebug() << "Option" << so->getName() << "is not active";
 	    }
 	}

	/** Scan Resolution should always exist. **/
	KScanOption res( SANE_NAME_SCAN_RESOLUTION );
	res.get( &d->currScanResolutionX );
        if ( !optionExists( SANE_NAME_SCAN_Y_RESOLUTION ) )
           d->currScanResolutionY = d->currScanResolutionX;
        else
        {
           KScanOption yres( SANE_NAME_SCAN_Y_RESOLUTION );
           yres.get( &d->currScanResolutionY );
        }

	return( acquireData( false ));
    }
    else
    {
   	/* a filename is in the parameter */
	QFileInfo file( filename );
	if( file.exists() )
	{
	     QImage i;
	     ImgScanInfo info;
	     if( i.load( filename ))
	     {
		info.setXResolution(i.dotsPerMeterX()); // TODO: *2.54/100
		info.setYResolution(i.dotsPerMeterY()); // TODO: *2.54/100
		info.setScannerName(filename);
		emit( sigNewImage( &i, &info ));
	     }
	}
        else
        {
            return KScanDevice::ParamError;
        }
    }

    return KScanDevice::Ok;
}


KScanDevice::Status KScanDevice::acquireData(bool isPreview)
{
    KScanDevice::Status stat = KScanDevice::Ok;
    int frames = 0;

    mScanningPreview = isPreview;
    mScanningState = KScanDevice::ScanStarting;
    mBytesRead = 0;

    emit sigScanStart();
    QApplication::setOverrideCursor(Qt::WaitCursor);	// potential lengthy operation

    ScanGlobal::self()->setScanDevice(this);		// for possible authentication

    while (true)					// loop while frames available
    {
        ++frames;					// count up how many

        mSaneStatus = sane_start(gScannerHandle);
        if (mSaneStatus==SANE_STATUS_ACCESS_DENIED)	// authentication failed?
        {
            kDebug() << "retrying authentication";
            clearSavedAuth();				// clear any saved password
            mSaneStatus = sane_start(gScannerHandle);	// try again once more
        }

        if (mSaneStatus==SANE_STATUS_GOOD)
        {
            mSaneStatus = sane_get_parameters(gScannerHandle, &mSaneParameters);
            if (mSaneStatus==SANE_STATUS_GOOD)
            {
                kDebug() << "Scan parameters... frame" << frames;
                kDebug() << "  format:          " << mSaneParameters.format;
                kDebug() << "  last_frame:      " << mSaneParameters.last_frame;
                kDebug() << "  lines:           " << mSaneParameters.lines;
                kDebug() << "  depth:           " << mSaneParameters.depth;
                kDebug() << "  pixels_per_line: " << mSaneParameters.pixels_per_line;
                kDebug() << "  bytes_per_line:  " << mSaneParameters.bytes_per_line;

                // TODO: implement "Hand Scanner" support
                if (mSaneParameters.lines<1)
                {
                    kDebug() << "Hand Scanner not supported";
                    stat = KScanDevice::NotSupported;
                }
                else if (mSaneParameters.pixels_per_line==0)
                {
                    kDebug() << "Nothing to acquire!";
                    stat = KScanDevice::EmptyPic;
                }
            }
            else
            {
                stat = KScanDevice::OpenDevice;
                kDebug() << "sane_get_parameters() error" << lastSaneErrorMessage();
            }
        }
        else
        {
            stat = KScanDevice::OpenDevice;
            kDebug() << "sane_start() error" << lastSaneErrorMessage();
        }
        QApplication::restoreOverrideCursor();

        if (mScanningState==KScanDevice::ScanStarting)	// first time through loop
        {
            // Create image to receive scan, based on SANE parameters
            if (stat==KScanDevice::Ok) stat = createNewImage(&mSaneParameters);

            // Create/reinitialise buffer for scanning one line
            if (stat==KScanDevice::Ok)
            {
                if (mScanBuf!=NULL) delete [] mScanBuf;
                mScanBuf = new SANE_Byte[mSaneParameters.bytes_per_line+4];
                if (mScanBuf==NULL) stat = KScanDevice::NoMemory;
            }						// can this ever happen?

            int fd = 0;
            // Don't assume that sane_get_select_fd() will succeed even if
            // sane_set_io_mode() successfully sets I/O mode to noblocking -
            // bug 159300
            if (sane_set_io_mode(gScannerHandle, SANE_TRUE)==SANE_STATUS_GOOD &&
                sane_get_select_fd(gScannerHandle, &fd)==SANE_STATUS_GOOD)
            {
                kDebug() << "using read socket notifier";
                mSocketNotifier = new QSocketNotifier(fd, QSocketNotifier::Read, this);
                connect(mSocketNotifier, SIGNAL(activated(int)), SLOT(doProcessABlock()));
            }
            // TODO: report which SANE function above failed, so we can
            // identify noncooperating scanners
        }

        if (stat!=KScanDevice::Ok)			// some problem getting started
        {
            // Scanning could not start - give up now
            kDebug() << "Scanning failed to start, status" << stat;
            emit sigScanFinished(stat);
            return (stat);
        }

        if (mScanningState==KScanDevice::ScanStarting)	// first time through loop
        {
            QApplication::setOverrideCursor(Qt::BusyCursor);
            emit sigAcquireStart();
        }

        emit sigScanProgress(0);			// signal the progress dialog
        qApp->processEvents();				// update the progress window

        mPixelX = 0;
        mPixelY = 0;
        mBytesUsed = 0;

        // Set internal status to indicate scanning in progress.
        // This status might be changed, e.g. by pressing Stop on a
        // GUI dialog displayed during scanning.
        mScanningState = KScanDevice::ScanInProgress;

        // As originally coded, if using the socket notifier
        // sane_get_parameters() was only called once at the beginning of
        // the scan - just after sane_start() above.  If not using the socket
        // notifier on the other hand, sane_get_parameters() was called after
        // each doProcessABlock() in the loop below.
        //
        // According to the SANE documentation text, sane_get_parameters()
        // needs to be called once after sane_start() to get the exact
        // parameters, but not necessarily in the reading loop that just
        // needs to call sane_read() repeatedly).  The diagram above, though,
        // seems to imply that sane_get_parameters() should be done in the
        // reading loop.
        //
        // Doing the sane_get_parameters() just once seems to work with all
        // of the scanners that I have available to test, both using the
        // socket notifier and not.  So that is what we now do, in this
        // much simpler loop.

        while (true)					// loop for one scanned frame
        {
            if (mSocketNotifier!=NULL)			// using the socket notifier
            {
                qApp->processEvents();			// let it fire away
            }
            else					// not using socket notifier
            {
                doProcessABlock();			// may block while reading
            }
							// exit loop when frame done
            if (mScanningState==KScanDevice::ScanIdle ||
                mScanningState==KScanDevice::ScanNextFrame) break;
        }
							// exit loop when scan done
        if (mScanningState==KScanDevice::ScanIdle) break;
    }

    kDebug() << "Scan read" << mBytesRead << "bytes in" << frames << "frames";

    emit sigScanFinished(stat);				// scan is now finished
    return (stat);
}


/* This function calls at least sane_read and converts the read data from the scanner
 * to the qimage.
 * The function needs:
 * QImage img valid
 * the data-buffer  set to a appropriate size
 **/

// TODO: probably needs to be extended for 16-bit scanner support

void KScanDevice::doProcessABlock()
{
    int val,i;
    QRgb col, newCol;

    SANE_Byte *rptr = NULL;
    SANE_Int bytes_read = 0;
    int chan = 0;
    mSaneStatus = SANE_STATUS_GOOD;
    uchar eight_pix = 0;

    if (mScanningState==KScanDevice::ScanIdle) return;	// scan finished, no more to do
							// block notifications while working
    if (mSocketNotifier!=NULL) mSocketNotifier->setEnabled(false);

    while (true)
    {
        mSaneStatus = sane_read(gScannerHandle,
                              (mScanBuf+mBytesUsed),
                              mSaneParameters.bytes_per_line,
                              &bytes_read);
        if (mSaneStatus!=SANE_STATUS_GOOD)
        {
            if (mSaneStatus!=SANE_STATUS_EOF)		// this is OK, just stop
            {						// any other error
                kDebug() << "sane_read() error" << lastSaneErrorMessage()
                         << "bytes read" << bytes_read;
            }
            break;
        }

        if (bytes_read<1) break;			// no data, finish loop

	mBytesRead += bytes_read;
	// qDebug( "Bytes read: %d, bytes written: %d", bytes_read, mBytesUsed );

        int red = 0;
        int green = 0;
        int blue = 0;

	rptr = mScanBuf;				// start of scan data
	switch (mSaneParameters.format)
	{
case SANE_FRAME_RGB:
            if (mSaneParameters.lines<1) break;
            bytes_read += mBytesUsed;			// die übergebliebenen Bytes dazu
            mBytesUsed = bytes_read % 3;

            for (val = 0; val<((bytes_read-mBytesUsed)/3); val++)
            {
                red   = *rptr++;
                green = *rptr++;
                blue  = *rptr++;

                if (mPixelX>=mSaneParameters.pixels_per_line)
                {					// reached end of a row
                    mPixelX = 0;
                    mPixelY++;
                }
                if (mPixelY<mScanImage->height())	// within image height
                {
		    mScanImage->setPixel(mPixelX, mPixelY, qRgb(red, green, blue));
                }
                mPixelX++;
            }

            for (val = 0; val<mBytesUsed; val++)	// Copy the remaining bytes down
            {						// to the beginning of the block
                *(mScanBuf+val) = *rptr++;
            }
            break;

case SANE_FRAME_GRAY:
            for (val = 0; val<bytes_read ; val++)
            {
                if (mPixelY>=mSaneParameters.lines) break;
                if (mSaneParameters.depth==8)		// Greyscale
                {
		    if (mPixelX>=mSaneParameters.pixels_per_line)
                    {					// reached end of a row
                        mPixelX = 0;
                        mPixelY++;
                    }
		    mScanImage->setPixel(mPixelX, mPixelY, *rptr++);
		    mPixelX++;
                }
                else					// Lineart (bitmap)
                {					// needs to be converted to byte
		    eight_pix = *rptr++;
		    for (i = 0; i<8; i++)
		    {
                        if (mPixelY<mSaneParameters.lines)
                        {
                            chan = (eight_pix & 0x80)>0 ? 0 : 1;
                            eight_pix = eight_pix << 1;
                            mScanImage->setPixel(mPixelX, mPixelY, chan);
                            mPixelX++;
                            if( mPixelX>=mSaneParameters.pixels_per_line)
                            {
                                mPixelX = 0;
                                mPixelY++;
                                break;
                            }
                        }
		    }
                }
            }
            break;

case SANE_FRAME_RED:
case SANE_FRAME_GREEN:
case SANE_FRAME_BLUE:
            for (val = 0; val<bytes_read ; val++)
            {
                if (mPixelX>=mSaneParameters.pixels_per_line)
                {					// reached end of a row
                    mPixelX = 0;
		    mPixelY++;
                }

                if (mPixelY<mSaneParameters.lines)
                {
		    col = mScanImage->pixel(mPixelX, mPixelY);

		    red   = qRed(col);
		    green = qGreen(col);
		    blue  = qBlue(col);
		    chan  = *rptr++;

		    switch (mSaneParameters.format)
		    {
case SANE_FRAME_RED:    newCol = qRgba(chan, green, blue, 0xFF);
                        break;

case SANE_FRAME_GREEN:  newCol = qRgba(red, chan, blue, 0xFF);
                        break;

case SANE_FRAME_BLUE:   newCol = qRgba(red, green, chan, 0xFF);
                        break;

default:                newCol = qRgba(0xFF, 0xFF, 0xFF, 0xFF);
                        break;
		    }
		    mScanImage->setPixel(mPixelX, mPixelY, newCol);
		    mPixelX++;
                }
            }
            break;

default:    kDebug() << "Undefined SANE format" << mSaneParameters.format;
            break;
	}						// switch of scan format

	if ((mSaneParameters.lines>0) && ((mSaneParameters.lines*mPixelY)>0))
	{
            int progress =  (int)(((double)MAX_PROGRESS)/mSaneParameters.lines*mPixelY);
            if (progress<MAX_PROGRESS) emit sigScanProgress(progress);
	}

        // cannot get here, bytes_read and EOF tested above
	//if( bytes_read == 0 || mSaneStatus == SANE_STATUS_EOF )
	//{
	//   kDebug() << "mSaneStatus not OK:" << sane_stat;
	//   break;
	//}

        if (mScanningState==KScanDevice::ScanStopNow)
        {
            /* mScanningState is set to ScanStopNow due to hitting slStopScanning   */
            /* Mostly that one is fired by the STOP-Button in the progress dialog. */

            /* This is also hit after the normal finish of the scan. Most probably,
             * the QSocketnotifier fires for a few times after the scan has been
             * cancelled.  Does it matter ? To see it, just uncomment the qDebug msg.
             */
            kDebug() << "Stopping the scan progress";
            mScanningState = KScanDevice::ScanIdle;
//            emit sigScanFinished(KScanDevice::Ok);
            break;
        }
    }							// end of main loop

    // Here when scanning is finished or has had an error
    if (mSaneStatus==SANE_STATUS_EOF)			// end of scan pass
    {
        if (mSaneParameters.last_frame)			// end of scanning run
        {
            /** Everythings okay, the picture is ready **/
            kDebug() << "Last frame reached, scan successful";
            mScanningState = KScanDevice::ScanIdle;
//            emit sigScanFinished(KScanDevice::Ok);
        }
        else
        {
            // TODO: nothing ever looks for ScanNextFrame!
            /** EOF und nicht letzter Frame -> Parameter neu belegen und neu starten **/
            mScanningState = KScanDevice::ScanNextFrame;
            kDebug() << "EOF, but another frame to scan";
        }
    }

    if (mSaneStatus==SANE_STATUS_CANCELLED)
    {
        mScanningState = KScanDevice::ScanStopNow;
        kDebug() << "Scan was cancelled";
    }

    if (mSocketNotifier!=NULL) mSocketNotifier->setEnabled(true);
}


void KScanDevice::slotScanFinished(KScanDevice::Status status)
{
    if (mSocketNotifier!=NULL)				// clean up if in use
    {
	delete mSocketNotifier;
	mSocketNotifier = NULL;
    }

    emit sigScanProgress(MAX_PROGRESS);
    QApplication::restoreOverrideCursor();

    kDebug() << "status" <<  status;

    if (mScanBuf!=NULL)
    {
	delete[] mScanBuf;
	mScanBuf = NULL;
    }

    if (status==KScanDevice::Ok && mScanImage!=NULL)
    {
	ImgScanInfo info;
	info.setXResolution(d->currScanResolutionX);
	info.setYResolution(d->currScanResolutionY);
	info.setScannerName(mScannerName);

	// put the resolution also into the image itself
	mScanImage->setDotsPerMeterX(static_cast<int>(d->currScanResolutionX / 0.0254 + 0.5));
	mScanImage->setDotsPerMeterY(static_cast<int>(d->currScanResolutionY / 0.0254 + 0.5));

	if (mScanningPreview)
	{
	    savePreviewImage(*mScanImage);
	    emit sigNewPreview(mScanImage, &info);

	    loadOptionSet(mSavedOptions);		// restore original scan settings
	}
	else
	{
	    emit sigNewImage(mScanImage, &info);
	}
    }

    sane_cancel(gScannerHandle);

    /* This follows after sending the signal */
    if (mScanImage!=NULL)
    {
	delete mScanImage;
	mScanImage = NULL;
    }

    mScanningState = KScanDevice::ScanIdle;
}


//  Configuration
//  -------------

// TODO: does this need to be a slot?
void KScanDevice::slotSaveScanConfigSet(const QString &setName, const QString &descr)
{
    if (setName.isEmpty()) return;			// do not save unnamed set
    if (mScannerName.isNull()) return;			// do not save for no scanner

    kDebug() << "Saving configuration" << setName;
    KScanOptSet optSet(DEFAULT_OPTIONSET);
    getCurrentOptions(&optSet);
    optSet.saveConfig(mScannerName, setName, descr);
}


void KScanDevice::loadOptionSet( KScanOptSet *optSet )
{
   if (optSet==NULL) return;

   kDebug() << "Loading option set" << optSet->optSetName() << "with" << optSet->count() << "options";

   KScanOptSet::ConstIterator it = optSet->begin();
   while( it != optSet->end() )
   {
	  KScanOption *so = it.value();
      if( ! so->initialised() )
	 kDebug() << "Option" << so->getName() << "is not initialised";

      if( ! so->active() )
	 kDebug() << "Option" << so->getName() << "is not active";

      if( so && so->active() && so->initialised())
      {
	 kDebug() << "Option" << so->getName() << "set to" << so->get();
	 apply( so );
      }
      ++it;
   }

}


void KScanDevice::getCurrentOptions(KScanOptSet *optSet)
{
    if (optSet==NULL) return;

    for (QList<KScanOption *>::const_iterator it = mGuiElements.constBegin();
         it!=mGuiElements.constEnd(); ++it)
    {
        KScanOption *so = (*it);
        if (so==NULL) continue;

        kDebug() << "Storing" << so->getName();
        if (so->active())
        {
            apply(so);
            optSet->backupOption(*so);
        }

        /* drop the thing from the dirty-list */
        mDirtyList.removeOne(so->getName());
    }

    for (QList<QByteArray>::const_iterator it = mDirtyList.constBegin();
         it!=mDirtyList.constEnd(); ++it)
    {
        KScanOption so(*it);
        optSet->backupOption(so);
    }
}


QString KScanDevice::getConfig(const QString &key, const QString &def) const
{
    const KConfigGroup grp = ScanGlobal::self()->configGroup(mScannerName);
    return (grp.readEntry(key, def));
}


// TODO: does this need to be a slot?
void KScanDevice::storeConfig(const QString &key, const QString &val)
{
    if (mScannerName.isNull())
    {
        kDebug() << "Skipping config write, no scanner name!";
        return;
    }

    kDebug() << "Storing config" << key << "in group" << mScannerName;

    KConfigGroup grp = ScanGlobal::self()->configGroup(mScannerName);
    grp.writeEntry(key, val);
    grp.sync();
}



//  SANE Authentication
//  -------------------
//
//  According to the SANE documentation, this may be requested for any use of
//  sane_open(), sane_control_option() or sane_start() on a scanner device
//  that requires authentication.
//
//  The only uses of sane_open() and sane_start() are here in this file, and
//  they set the current scanner using setScanDevice() before performing the
//  SANE operation.
//
//  This does not happen for all uses of sane_control_option(), either here or
//  in KScanOption, so there is a slight possibility that if authentication is
//  needed for those (and has not been previously requested by sane_open() or
//  sane_start()) then it will use the wrong scanner device or will not prompt
//  at all.  However, Kooka only supports one scanner open at a time, and does
//  sane_open() before any use of sane_control_option().  So hopefully this
//  will not be a problem.

bool KScanDevice::authenticate(QByteArray *retuser, QByteArray *retpass)
{
    kDebug() << "for" << mScannerName;

    // TODO: use KWallet for username/password?
    KConfigGroup grp = ScanGlobal::self()->configGroup(mScannerName);
    QByteArray user = QByteArray::fromBase64(grp.readEntry("user", QString()).toLocal8Bit());
    QByteArray pass = QByteArray::fromBase64(grp.readEntry("pass", QString()).toLocal8Bit());

    if (!user.isEmpty() && !pass.isEmpty())
    {
        kDebug() << "have saved username/password";
    }
    else
    {
        kDebug() << "asking for username/password";

        KPasswordDialog dlg(NULL, KPasswordDialog::ShowKeepPassword|KPasswordDialog::ShowUsernameLine);
        dlg.setPrompt(i18n("<qt>The scanner<br><b>%1</b><br>requires authentication.", mScannerName.constData()));
        dlg.setCaption(i18n("Scanner Authentication"));

        if (!user.isEmpty()) dlg.setUsername(user);
        if (!pass.isEmpty()) dlg.setPassword(pass);

        if (!dlg.exec()) return (false);

        user = dlg.username().toLocal8Bit();
        pass = dlg.password().toLocal8Bit();
        if (dlg.keepPassword())
        {
            grp.writeEntry("user", user.toBase64());
            grp.writeEntry("pass", pass.toBase64());
        }
    }

    *retuser = user;
    *retpass = pass;
    return (true);
}


void KScanDevice::clearSavedAuth()
{
    KConfigGroup grp = ScanGlobal::self()->configGroup(mScannerName);

    grp.deleteEntry("user");
    grp.deleteEntry("pass");
    grp.sync();
}


//  Error reporting
//  ---------------

QString KScanDevice::lastSaneErrorMessage() const
{
    return (sane_strstatus(mSaneStatus));
}


QString KScanDevice::statusMessage(KScanDevice::Status stat)
{
    switch (stat)
    {
case KScanDevice::Ok:			return (i18n("OK"));		// shouldn't be reported
case KScanDevice::NoDevice:		return (i18n("No device"));	// never during scanning
case KScanDevice::ParamError:		return (i18n("Bad parameter"));
case KScanDevice::OpenDevice:		return (i18n("Cannot open device"));
case KScanDevice::ControlError:		return (i18n("sane_control_option() failed"));
case KScanDevice::EmptyPic:		return (i18n("Empty picture"));
case KScanDevice::NoMemory:		return (i18n("Out of memory"));
case KScanDevice::Reload:		return (i18n("Needs reload"));	// never during scanning
case KScanDevice::Cancelled:		return (i18n("Cancelled"));	// shouldn't be reported
case KScanDevice::OptionNotActive:	return (i18n("Not active"));	// never during scanning
case KScanDevice::NotSupported:		return (i18n("Not supported"));
default:				return (i18n("Unknown status %1", stat));
    }
}
