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

#include "scanparams.h"

#include "scanparams_p.h"

#include <qpushbutton.h>
#include <qimage.h>
#include <qtooltip.h>
#include <qprogressdialog.h>
#include <qcheckbox.h>
#include <qgroupbox.h>
#include <qbuttongroup.h>
#include <qradiobutton.h>
#include <qfileinfo.h>
#include <qgridlayout.h>
#include <qlabel.h>
#include <qscrollarea.h>

#include <kfiledialog.h>
#include <KLocalizedString>
#include <QDebug>
#include <kiconloader.h>
#include <kled.h>
#include <kmessagebox.h>
#include <kmimetype.h>
#include <khbox.h>
#include <ktabwidget.h>
#include <QIcon>

extern "C"
{
#include <sane/saneopts.h>
}

#include "scanglobal.h"
#include "scansourcedialog.h"
#include "massscandialog.h"
#include "gammadialog.h"
#include "kgammatable.h"
#include "kscancontrols.h"
#include "scansizeselector.h"
#include "kscanoption.h"
#include "kscanoptset.h"

//  Debugging options
#undef DEBUG_ADF

//  SANE testing options
#ifndef SANE_NAME_TEST_PICTURE
#define SANE_NAME_TEST_PICTURE      "test-picture"
#endif
#ifndef SANE_NAME_THREE_PASS
#define SANE_NAME_THREE_PASS        "three-pass"
#endif
#ifndef SANE_NAME_HAND_SCANNER
#define SANE_NAME_HAND_SCANNER      "hand-scanner"
#endif
#ifndef SANE_NAME_GRAYIFY
#define SANE_NAME_GRAYIFY       "grayify"
#endif

ScanParamsPage::ScanParamsPage(QWidget *parent, const char *name)
    : QWidget(parent)
{
    setObjectName(name);

    mLayout = new QGridLayout(this);
    mLayout->setSpacing(2 * KDialog::spacingHint());
    mLayout->setColumnStretch(2, 1);
    mLayout->setColumnMinimumWidth(1, KDialog::marginHint());

    mNextRow = 0;
    mPendingGroup = NULL;
}

ScanParamsPage::~ScanParamsPage()
{
}

void ScanParamsPage::checkPendingGroup()
{
    if (mPendingGroup != NULL) {            // separator to add first?
        QWidget *w = mPendingGroup;
        mPendingGroup = NULL;               // avoid recursion!
        addRow(w);
    }
}

void ScanParamsPage::addRow(QWidget *wid)
{
    if (wid == NULL) {
        return;    // must have one
    }

    checkPendingGroup();                // add separator if needed
    mLayout->addWidget(wid, mNextRow, 0, 1, -1);
    ++mNextRow;
}

void ScanParamsPage::addRow(QLabel *lab, QWidget *wid, QLabel *unit, Qt::Alignment align)
{
    if (wid == NULL) {
        return;    // must have one
    }
    wid->setMaximumWidth(MAX_CONTROL_WIDTH);

    checkPendingGroup();                // add separator if needed

    if (lab != NULL) {
        lab->setMaximumWidth(MAX_LABEL_WIDTH);
        lab->setMinimumWidth(MIN_LABEL_WIDTH);
        mLayout->addWidget(lab, mNextRow, 0, Qt::AlignLeft | align);
    }

    if (unit != NULL) {
        mLayout->addWidget(wid, mNextRow, 2, align);
        mLayout->addWidget(unit, mNextRow, 3, Qt::AlignLeft | align);
    } else {
        mLayout->addWidget(wid, mNextRow, 2, 1, 2, align);
    }

    ++mNextRow;
}

bool ScanParamsPage::lastRow()
{
    addGroup(NULL);                 // hide last if present

    mLayout->addWidget(new QLabel(QString::null, this), mNextRow, 0, 1, -1, Qt::AlignTop);
    mLayout->setRowStretch(mNextRow, 9);

    return (mNextRow > 0);
}

void ScanParamsPage::addGroup(QWidget *wid)
{
    if (mPendingGroup != NULL) {
        mPendingGroup->hide();    // dont't need this after all
    }

    mPendingGroup = wid;
}

ScanParams::ScanParams(QWidget *parent)
    : QWidget(parent)
{
    setObjectName("ScanParams");

    mSaneDevice = NULL;
    mVirtualFile = NULL;
    mGammaEditButt = NULL;
    mResolutionBind = NULL;
    mProgressDialog = NULL;
    mSourceSelect = NULL;
    mLed = NULL;

    mProblemMessage = NULL;
    mNoScannerMessage = NULL;

    // Preload the scan mode icons.  The ones from libksane, which will be
    // available if kdegraphics is installed, look better than our rather
    // ancient ones, so use those if possible.  Set canReturnNull to true
    // when looking up those, so the returned path will be empty if they
    // are not installed.

    KIconLoader::global()->addAppDir("libkscan");   // access to our icons

    QString ip = KIconLoader::global()->iconPath("color", KIconLoader::Small, true);
    if (ip.isEmpty()) {
        ip = KIconLoader::global()->iconPath("palette-color", KIconLoader::Small);
    }
    mIconColor = QIcon(ip);

    ip = KIconLoader::global()->iconPath("gray-scale", KIconLoader::Small, true);
    if (ip.isEmpty()) {
        ip = KIconLoader::global()->iconPath("palette-gray", KIconLoader::Small);
    }
    mIconGray = QIcon(ip);

    ip = KIconLoader::global()->iconPath("black-white", KIconLoader::Small, true);
    if (ip.isEmpty()) {
        ip = KIconLoader::global()->iconPath("palette-lineart", KIconLoader::Small);
    }
    mIconLineart = QIcon(ip);

    ip = KIconLoader::global()->iconPath("halftone", KIconLoader::Small, true);
    if (ip.isEmpty()) {
        ip = KIconLoader::global()->iconPath("palette-halftone", KIconLoader::Small);
    }
    mIconHalftone = QIcon(ip);

    /* intialise the default last save warnings */
    mStartupOptions = NULL;
}

ScanParams::~ScanParams()
{
    //qDebug();

    delete mStartupOptions;
    delete mProgressDialog;
}

bool ScanParams::connectDevice(KScanDevice *newScanDevice, bool galleryMode)
{
    QGridLayout *lay = new QGridLayout(this);
    lay->setMargin(0);
    lay->setColumnStretch(0, 9);

    if (newScanDevice == NULL) {            // no scanner device
        //qDebug() << "No scan device, gallery=" << galleryMode;
        mSaneDevice = NULL;
        createNoScannerMsg(galleryMode);
        return (true);
    }

    mSaneDevice = newScanDevice;
    mScanMode = ScanParams::NormalMode;
    adf = ADF_OFF;

    QLabel *lab = new QLabel(i18n("<qt><b>Scanner&nbsp;Settings</b>"), this);
    lay->addWidget(lab, 0, 0, Qt::AlignLeft);

    mLed = new KLed(this);
    mLed->setState(KLed::Off);
    mLed->setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed));
    lay->addWidget(mLed, 0, 1, Qt::AlignRight);

    lab = new QLabel(mSaneDevice->scannerDescription(), this);
    lay->addWidget(lab, 1, 0, 1, 2, Qt::AlignLeft);

    /* load the startup scanoptions */
    // TODO: check whether the saved scanner options apply to the current scanner?
    // They may be for a completely different one...
    // Or update KScanDevice and here to save/load the startup options
    // on a per-scanner basis.
    mStartupOptions = new KScanOptSet(DEFAULT_OPTIONSET);
    if (!mStartupOptions->loadConfig(mSaneDevice->scannerBackendName())) {
        //qDebug() << "Could not load startup options";
        delete mStartupOptions;
        mStartupOptions = NULL;
    }

    /* Now create Widgets for the important scan settings */
    QWidget *sv = createScannerParams();
    lay->addWidget(sv, 3, 0, 1, 2);
    lay->setRowStretch(3, 9);

    /* Reload all options to care for inactive options */
    mSaneDevice->reloadAllOptions();

    /* Create the Scan Buttons */
    QPushButton *pb = new QPushButton(QIcon::fromTheme("preview"), i18n("Pre&view"), this);
    pb->setToolTip(i18n("<qt>Start a preview scan and show the preview image"));
    pb->setMinimumWidth(100);
    connect(pb, &QPushButton::clicked, this, &ScanParams::slotAcquirePreview);
    lay->addWidget(pb, 5, 0, Qt::AlignLeft);

    pb = new QPushButton(QIcon::fromTheme("scan"), i18n("Star&t Scan"), this);
    pb->setToolTip(i18n("<qt>Start a scan and save the scanned image"));
    pb->setMinimumWidth(100);
    connect(pb, &QPushButton::clicked, this, &ScanParams::slotStartScan);
    lay->addWidget(pb, 5, 1, Qt::AlignRight);

    /* Initialise the progress dialog */
    mProgressDialog = new QProgressDialog(QString::null, i18n("Stop"), 0, 100, NULL);
    mProgressDialog->setModal(true);
    mProgressDialog->setAutoClose(true);
    mProgressDialog->setAutoReset(true);
    mProgressDialog->setMinimumDuration(100);
    mProgressDialog->setWindowTitle(i18n("Scanning"));
    setScanDestination(QString::null);          // reset destination display

    connect(mProgressDialog, &QProgressDialog::canceled, mSaneDevice, &KScanDevice::slotStopScanning);
    connect(mSaneDevice, &KScanDevice::sigScanProgress, this, &ScanParams::slotScanProgress);

    return (true);
}

KLed *ScanParams::operationLED() const
{
    return (mLed);
}

void ScanParams::initialise(KScanOption *so)
{
    if (so == NULL) {
        return;
    }
    if (mStartupOptions == NULL) {
        return;
    }
    if (!so->isReadable()) {
        return;
    }
    if (!so->isSoftwareSettable()) {
        return;
    }

    QByteArray name = so->getName();
    if (!name.isEmpty()) {
        QByteArray val = mStartupOptions->getValue(name);
        //qDebug() << "Initialising" << name << "with value" << val;
        so->set(val);
        so->apply();
    }
}

ScanParamsPage *ScanParams::createTab(KTabWidget *tw, const QString &title, const char *name)
{
    QScrollArea *scroll = new QScrollArea(this);
    scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    scroll->setWidgetResizable(true);           // stretch to width
    scroll->setFrameStyle(QFrame::NoFrame);

    ScanParamsPage *frame = new ScanParamsPage(this, name);
    scroll->setWidget(frame);
    tw->addTab(scroll, title);

    return (frame);
}

QWidget *ScanParams::createScannerParams()
{
    KTabWidget *tw = new KTabWidget(this);
    tw->setTabsClosable(false);
    tw->setTabPosition(QTabWidget::North);

    ScanParamsPage *basicFrame = createTab(tw, i18n("&Basic"), "BasicFrame");
    ScanParamsPage *otherFrame = createTab(tw, i18n("Other"), "OtherFrame");
    ScanParamsPage *advancedFrame = createTab(tw, i18n("Advanced"), "AdvancedFrame");

    KScanOption *so;
    QLabel *l;
    QWidget *w;
    QLabel *u;
    ScanParamsPage *frame;

    // Initial "Basic" options
    frame = basicFrame;

    // Virtual/debug image file
    mVirtualFile = mSaneDevice->getGuiElement(SANE_NAME_FILE, frame);
    if (mVirtualFile != NULL) {
        initialise(mVirtualFile);
        connect(mVirtualFile, &KScanOption::guiChange, this, &ScanParams::slotOptionChanged);

        l = mVirtualFile->getLabel(frame, true);
        w = mVirtualFile->widget();
        frame->addRow(l, w);

        // Selection for either virtual scanner or SANE debug
        KVBox *vbg = new KVBox(frame);

        QRadioButton *rb1 = new QRadioButton(i18n("SANE Debug (from PNM image)"), vbg);
        rb1->setToolTip(i18n("<qt>Operate in the same way that a real scanner does (including scan area, image processing etc.), but reading from the specified image file. See <a href=\"man:sane-pnm(5)\">sane-pnm(5)</a> for more information."));
        QRadioButton *rb2 = new QRadioButton(i18n("Virtual Scanner (any image format)"), vbg);
        rb2->setToolTip(i18n("<qt>Do not perform any scanning or processing, but simply read the specified image file. This is for testing the image saving, etc."));

        if (mScanMode == ScanParams::NormalMode) {
            mScanMode = ScanParams::SaneDebugMode;
        }
        rb1->setChecked(mScanMode == ScanParams::SaneDebugMode);
        rb2->setChecked(mScanMode == ScanParams::VirtualScannerMode);

        // needed for new 'buttonClicked' signal:
        QButtonGroup *vbgGroup = new QButtonGroup(vbg);
        vbgGroup->addButton(rb1, 0);
        vbgGroup->addButton(rb2, 1);
        connect(vbgGroup, static_cast<void (QButtonGroup::*)(int)>(&QButtonGroup::buttonClicked), this, &ScanParams::slotVirtScanModeSelect);

        l = new QLabel(i18n("Reading mode:"), frame);
        frame->addRow(l, vbg, NULL, Qt::AlignTop);

        // Separator line after these.  Using a KScanGroup with a null text,
        // so that it looks the same as any real group separators following.
        frame->addGroup(new KScanGroup(frame, QString::null));
    }

    // Mode setting
    so = mSaneDevice->getGuiElement(SANE_NAME_SCAN_MODE, frame);
    if (so != NULL) {
        KScanCombo *cb = (KScanCombo *) so->widget();

        // Having loaded the 'sane-backends' message catalogue, these strings
        // are now translatable.  But KScanCombo::setIcon() works on the
        // untranslated strings.
        cb->setIcon(mIconLineart, I18N_NOOP("Lineart"));
        cb->setIcon(mIconLineart, I18N_NOOP("Binary"));
        cb->setIcon(mIconGray, I18N_NOOP("Gray"));
        cb->setIcon(mIconGray, I18N_NOOP("Grayscale"));
        cb->setIcon(mIconColor, I18N_NOOP("Color"));
        cb->setIcon(mIconHalftone, I18N_NOOP("Halftone"));

        initialise(so);
        connect(so, &KScanOption::guiChange, this, &ScanParams::slotOptionChanged);
        connect(so, &KScanOption::guiChange, this, &ScanParams::slotNewScanMode);

        l = so->getLabel(frame, true);
        frame->addRow(l, cb);
    }

    // Resolution setting.  Try "X-Resolution" setting first, this is the
    // option we want if the resolutions are split up.  If there is no such
    // option then try just "Resolution", this may not be the same as
    // "X-Resolution" even though this was the case in SANE<=1.0.19.
    so = mSaneDevice->getGuiElement(SANE_NAME_SCAN_X_RESOLUTION, frame);
    if (so == NULL) {
        so = mSaneDevice->getGuiElement(SANE_NAME_SCAN_RESOLUTION, frame);
    }
    if (so != NULL) {
        initialise(so);

        int x_res;
        so->get(&x_res);
        so->redrawWidget();

        connect(so, &KScanOption::guiChange, this, &ScanParams::slotOptionChanged);
        // Connection that passes the resolution to the previewer
        connect(so, &KScanOption::guiChange, this, &ScanParams::slotNewResolution);

        l = so->getLabel(frame, true);
        w = so->widget();
        u = so->getUnit(frame);
        frame->addRow(l, w, u);

        // Same X/Y resolution option (if present)
        mResolutionBind = mSaneDevice->getGuiElement(SANE_NAME_RESOLUTION_BIND, frame);
        if (mResolutionBind != NULL) {
            initialise(mResolutionBind);
            mResolutionBind->redrawWidget();

            connect(mResolutionBind, &KScanOption::guiChange, this, &ScanParams::slotOptionChanged);

            l = so->getLabel(frame, true);
            w = so->widget();
            frame->addRow(l, w);
        }

        // Now the "Y-Resolution" setting, if there is a separate one
        so = mSaneDevice->getGuiElement(SANE_NAME_SCAN_Y_RESOLUTION, frame);
        int y_res = x_res;

        if (so != NULL) {
            initialise(so);
            if (so->isActive()) {
                so->get(&y_res);
            }
            so->redrawWidget();

            // Connection that passes the resolution to the previewer
            connect(so, &KScanOption::guiChange, this, &ScanParams::slotNewResolution);

            l = so->getLabel(frame, true);
            w = so->widget();
            u = so->getUnit(frame);
            frame->addRow(l, w, u);
        }

        emit scanResolutionChanged(x_res, y_res);   // initialise the previewer
    } else {
        //qDebug() << "Serious: No resolution option available!";
    }

    // Scan size setting
    mAreaSelect = new ScanSizeSelector(frame, mSaneDevice->getMaxScanSize());
    connect(mAreaSelect, &ScanSizeSelector::sizeSelected, this, &ScanParams::slotScanSizeSelected);
    l = new QLabel("Scan &area:", frame);       // make sure it gets an accel
    l->setBuddy(mAreaSelect->focusProxy());
    frame->addRow(l, mAreaSelect, NULL, Qt::AlignTop);

    // Insert another beautification line
    frame->addGroup(new KScanGroup(frame, QString::null));

    // Source selection
    mSourceSelect = mSaneDevice->getGuiElement(SANE_NAME_SCAN_SOURCE, frame);
    if (mSourceSelect != NULL) {
        initialise(mSourceSelect);
        connect(mSourceSelect, &KScanOption::guiChange, this, &ScanParams::slotOptionChanged);

        l = mSourceSelect->getLabel(frame, true);
        w = mSourceSelect->widget();
        frame->addRow(l, w);

        //  TODO: enable the "Advanced" dialogue, because that
        //  contains other ADF options.  They are not implemented at the moment
        //  but they may be some day...
        //QPushButton *pb = new QPushButton( i18n("Source && ADF Options..."), frame);
        //connect(pb, SIGNAL(clicked()), SLOT(slotSourceSelect()));
        //lay->addWidget(pb,row,2,1,-1,Qt::AlignRight);
        //++row;
    }

    // SANE testing options, for the "test" device
    so = mSaneDevice->getGuiElement(SANE_NAME_TEST_PICTURE, frame);
    if (so != NULL) {
        initialise(so);
        connect(so, &KScanOption::guiChange, this, &ScanParams::slotOptionChanged);

        l = so->getLabel(frame);
        w = so->widget();
        frame->addRow(l, w);
    }

    // Now all of the other options which have not been accounted for yet.
    // Split them up into "Other" and "Advanced".
    const QList<QByteArray> opts = mSaneDevice->getAllOptions();
    for (QList<QByteArray>::const_iterator it = opts.constBegin();
            it != opts.constEnd(); ++it) {
        const QByteArray opt = (*it);

        if (opt == SANE_NAME_SCAN_TL_X ||       // ignore these (scan area)
                opt == SANE_NAME_SCAN_TL_Y ||
                opt == SANE_NAME_SCAN_BR_X ||
                opt == SANE_NAME_SCAN_BR_Y) {
            continue;
        }

        so = mSaneDevice->getExistingGuiElement(opt);   // see if already created
        if (so != NULL) {
            continue;    // if so ignore, don't duplicate
        }

        so = mSaneDevice->getGuiElement(opt, frame);
        if (so != NULL) {
            //qDebug() << "creating" << (so->isCommonOption() ? "OTHER" : "ADVANCED") << "option" << opt;
            initialise(so);
            connect(so, &KScanOption::guiChange, this, &ScanParams::slotOptionChanged);

            if (so->isCommonOption()) {
                frame = otherFrame;
            } else {
                frame = advancedFrame;
            }

            w = so->widget();
            if (so->isGroup()) {
                frame->addGroup(w);
            } else {
                l = so->getLabel(frame);
                u = so->getUnit(frame);
                frame->addRow(l, w, u);
            }

            // Some special things to do for particular options
            if (opt == SANE_NAME_BIT_DEPTH) {
                connect(so, &KScanOption::guiChange, this, &ScanParams::slotNewScanMode);
            } else if (opt == SANE_NAME_CUSTOM_GAMMA) {
                // Enabling/disabling the edit button is handled by
                // slotOptionChanged() calling setEditCustomGammaTableState()
                //connect(so, SIGNAL(guiChange(KScanOption*)), SLOT(slotOptionNotify(KScanOption*)));

                mGammaEditButt = new QPushButton(i18n("Edit Gamma Table..."), this);
                connect(mGammaEditButt, &QPushButton::clicked, this, &ScanParams::slotEditCustGamma);
                setEditCustomGammaTableState();

                frame->addRow(NULL, mGammaEditButt, NULL, Qt::AlignRight);
            }
        }
    }

    basicFrame->lastRow();              // final stretch row
    if (!otherFrame->lastRow()) {
        tw->setTabEnabled(1, false);
    }
    if (!advancedFrame->lastRow()) {
        tw->setTabEnabled(2, false);
    }

    initStartupArea();                  // set up and tell previewer
    slotNewScanMode();                  // tell previewer this too

    return (tw);                    // top-level (tab) widget
}

void ScanParams::initStartupArea()
{
// TODO: restore area a user preference
#ifdef RESTORE_AREA
    if (mStartupOptions == NULL)            // no saved options available
#endif
    {
        applyRect(QRect());             // set maximum scan area
        return;
    }
    // set scan area from saved
    KScanOption *tl_x = mSaneDevice->getOption(SANE_NAME_SCAN_TL_X);
    KScanOption *tl_y = mSaneDevice->getOption(SANE_NAME_SCAN_TL_Y);
    KScanOption *br_x = mSaneDevice->getOption(SANE_NAME_SCAN_BR_X);
    KScanOption *br_y = mSaneDevice->getOption(SANE_NAME_SCAN_BR_Y);

    initialise(tl_x);
    initialise(tl_y);
    initialise(br_x);
    initialise(br_y);

    QRect rect;
    int val1, val2;
    tl_x->get(&val1); rect.setLeft(val1);       // pass area to previewer
    br_x->get(&val2); rect.setWidth(val2 - val1);
    tl_y->get(&val1); rect.setTop(val1);
    br_y->get(&val2); rect.setHeight(val2 - val1);
    emit newCustomScanSize(rect);

    mAreaSelect->selectSize(rect);          // set selector to match
}

void ScanParams::createNoScannerMsg(bool galleryMode)
{
    QWidget *lab;
    if (galleryMode) {
        lab = messageScannerNotSelected();
    } else {
        lab = messageScannerProblem();
    }

    QGridLayout *lay = dynamic_cast<QGridLayout *>(layout());
    if (lay != NULL) {
        lay->addWidget(lab, 0, 0, Qt::AlignTop);
    }
}

QWidget *ScanParams::messageScannerNotSelected()
{
    if (mNoScannerMessage == NULL) {
        mNoScannerMessage = new QLabel(
            i18n("<qt>"
                 "<b>No scanner selected</b>"
                 "<p>"
                 "Select a scanner device to perform scanning."));

        mNoScannerMessage->setWordWrap(true);
    }

    return (mNoScannerMessage);
}

QWidget *ScanParams::messageScannerProblem()
{
    if (mProblemMessage == NULL) {
        mProblemMessage = new QLabel(
            i18n("<qt>"
                 "<b>Problem: No scanner found, or unable to access it</b>"
                 "<p>"
                 "There was a problem using the SANE (Scanner Access Now Easy) library to access "
                 "the scanner device.  There may be a problem with your SANE installation, or it "
                 "may not be configured to support your scanner."
                 "<p>"
                 "Check that SANE is correctly installed and configured on your system, and "
                 "also that the scanner device name and settings are correct."
                 "<p>"
                 "See the SANE project home page "
                 "(<a href=\"http://www.sane-project.org\">www.sane-project.org</a>) "
                 "for more information on SANE installation and setup."));

        mProblemMessage->setWordWrap(true);
        mProblemMessage->setOpenExternalLinks(true);
    }

    return (mProblemMessage);
}

void ScanParams::slotSourceSelect()
{
    AdfBehaviour adf = ADF_OFF;

    if (mSourceSelect == NULL) {
        return;    // no source selection GUI
    }
    if (!mSourceSelect->isValid()) {
        return;    // no option on scanner
    }

    const QByteArray &currSource = mSourceSelect->get();
    //qDebug() << "Current source is" << currSource;

    QList<QByteArray> sources = mSourceSelect->getList();
#ifdef DEBUG_ADF
    if (!sources.contains("Automatic Document Feeder")) {
        sources.append("Automatic Document Feeder");
    }
#endif

    // TODO: the 'sources' list has exactly the same options as the
    // scan source combo (apart from the debugging hack above), so
    // what's the point of repeating it in this dialogue?
    ScanSourceDialog d(this, sources, adf);
    d.slotSetSource(currSource);

    if (d.exec() != QDialog::Accepted) {
        return;
    }

    QString sel_source = d.getText();
    adf = d.getAdfBehave();
    //qDebug() << "new source" << sel_source << "ADF" << adf;

    /* set the selected Document source, the behavior is stored in a membervar */
    mSourceSelect->set(sel_source.toLatin1());      // TODO: FIX in ScanSourceDialog, then here
    mSourceSelect->apply();
    mSourceSelect->reload();
    mSourceSelect->redrawWidget();
}

/* Slot which is called if the user switches in the gui between
 *  the SANE-Debug-Mode and the qt image reading
 */
void ScanParams::slotVirtScanModeSelect(int but)
{
    ScanParams::ScanMode mode;
    if (but == 0) {
        mScanMode = ScanParams::SaneDebugMode;    // SANE Debug
    } else {
        mScanMode = ScanParams::VirtualScannerMode;    // Virtual Scanner
    }
    const bool enable = (mScanMode == ScanParams::SaneDebugMode);

    mSaneDevice->guiSetEnabled(SANE_NAME_HAND_SCANNER, enable);
    mSaneDevice->guiSetEnabled(SANE_NAME_THREE_PASS, enable);
    mSaneDevice->guiSetEnabled(SANE_NAME_GRAYIFY, enable);
    mSaneDevice->guiSetEnabled(SANE_NAME_CONTRAST, enable);
    mSaneDevice->guiSetEnabled(SANE_NAME_BRIGHTNESS, enable);
    mSaneDevice->guiSetEnabled(SANE_NAME_SCAN_RESOLUTION, enable);
    mSaneDevice->guiSetEnabled(SANE_NAME_SCAN_X_RESOLUTION, enable);
    mSaneDevice->guiSetEnabled(SANE_NAME_SCAN_Y_RESOLUTION, enable);

    mAreaSelect->setEnabled(enable);
}

KScanDevice::Status ScanParams::prepareScan(QString *vfp)
{
    //qDebug() << "scan mode=" << mScanMode;

    setScanDestination(QString::null);          // reset progress display

    // Check compatibility of scan settings
    int format;
    int depth;
    mSaneDevice->getCurrentFormat(&format, &depth);
    if (depth == 1 && format != SANE_FRAME_GRAY) {  // 1-bit scan depth in colour?
        KMessageBox::sorry(this, i18n("1-bit depth scan cannot be done in color"));
        return (KScanDevice::ParamError);
    } else if (depth == 16) {
        KMessageBox::sorry(this, i18n("16-bit depth scans are not supported"));
        return (KScanDevice::ParamError);
    }

    QString virtfile;
    if (mScanMode == ScanParams::SaneDebugMode || mScanMode == ScanParams::VirtualScannerMode) {
        if (mVirtualFile != NULL) {
            virtfile = mVirtualFile->get();
        }
        if (virtfile.isEmpty()) {
            KMessageBox::sorry(this, i18n("A file must be entered for testing or virtual scanning"));
            return (KScanDevice::ParamError);
        }

        QFileInfo fi(virtfile);
        if (!fi.exists()) {
            KMessageBox::sorry(this, i18n("<qt>The testing or virtual file<br><filename>%1</filename><br>was not found or is not readable", virtfile));
            return (KScanDevice::ParamError);
        }

        if (mScanMode == ScanParams::SaneDebugMode) {
            KMimeType::Ptr mimetype = KMimeType::findByPath(virtfile);
            if (!(mimetype->is("image/x-portable-bitmap") ||
                    mimetype->is("image/x-portable-greymap") ||
                    mimetype->is("image/x-portable-pixmap"))) {
                KMessageBox::sorry(this, i18n("<qt>SANE Debug can only read PNM files.<br>"
                                              "This file is type <b>%1</b>.", mimetype->name()));
                return (KScanDevice::ParamError);
            }
        }
    }

    if (vfp != NULL) {
        *vfp = virtfile;
    }
    return (KScanDevice::Ok);
}

void ScanParams::setScanDestination(const QString &dest)
{
    ////qDebug() << "scan destination is" << dest;
    if (dest.isEmpty()) {
        mProgressDialog->setLabelText(i18n("Scan in progress"));
    } else {
        mProgressDialog->setLabelText(i18n("Scan in progress<br><br><filename>%1</filename>", dest));
    }
}

void ScanParams::slotScanProgress(int value)
{
    mProgressDialog->setValue(value);
    if (value == 0 && !mProgressDialog->isVisible()) {
        mProgressDialog->show();
    }
}

/* Slot called to start acquiring a preview */
void ScanParams::slotAcquirePreview()
{

    // TODO: should be able to preview in Virtual Scanner mode, it just means
    // that the preview image will be the same size as the final image (which
    // doesn't matter).

    if (mScanMode == ScanParams::VirtualScannerMode) {
        KMessageBox::sorry(this, i18n("Cannot preview in Virtual Scanner mode"));
        return;
    }

    QString virtfile;
    KScanDevice::Status stat = prepareScan(&virtfile);
    if (stat != KScanDevice::Ok) {
        return;
    }

    //qDebug() << "scan mode=" << mScanMode << "virtfile" << virtfile;

    KScanOption *greyPreview = mSaneDevice->getExistingGuiElement(SANE_NAME_GRAY_PREVIEW);
    int gp = 0;
    if (greyPreview != NULL) {
        greyPreview->get(&gp);
    }

    setMaximalScanSize();               // always preview at maximal size
    mAreaSelect->selectCustomSize(QRect());     // reset selector to reflect that

    stat = mSaneDevice->acquirePreview(gp);
    if (stat != KScanDevice::Ok) {
        //qDebug() << "Error, preview status " << stat;
    }
}

/* Slot called to start scanning */
void ScanParams::slotStartScan()
{
    QString virtfile;
    KScanDevice::Status stat = prepareScan(&virtfile);
    if (stat != KScanDevice::Ok) {
        return;
    }

    //qDebug() << "scan mode=" << mScanMode << "virtfile" << virtfile;

    if (mScanMode != ScanParams::VirtualScannerMode) { // acquire via SANE
        if (adf == ADF_OFF) {
            //qDebug() << "Start to acquire image";
            stat = mSaneDevice->acquireScan();
        } else {
            //qDebug() << "ADF Scan not yet implemented :-/";
            // stat = performADFScan();
        }
    } else {                    // acquire via Qt-IMGIO
        //qDebug() << "Acquiring from virtual file";
        stat = mSaneDevice->acquireScan(virtfile);
    }

    if (stat != KScanDevice::Ok) {
        //qDebug() << "Error, scan status " << stat;
    }
}

bool ScanParams::getGammaTableFrom(const QByteArray &opt, KGammaTable *gt)
{
    KScanOption *so = mSaneDevice->getOption(opt, false);
    if (so == NULL) {
        return (false);
    }

    if (!so->get(gt)) {
        return (false);
    }
    //qDebug() << "got from" << so->getName() << "=" << gt->toString();
    return (true);
}

bool ScanParams::setGammaTableTo(const QByteArray &opt, const KGammaTable *gt)
{
    KScanOption *so = mSaneDevice->getOption(opt, false);
    if (so == NULL) {
        return (false);
    }

    //qDebug() << "set" << so->getName() << "=" << gt->toString();
    so->set(gt);
    return (so->apply());
}

void ScanParams::slotEditCustGamma()
{
    KGammaTable gt;                 // start with default values

    // Get the current gamma table from either the combined gamma table
    // option, or any one of the colour channel gamma tables.
    if (!getGammaTableFrom(SANE_NAME_GAMMA_VECTOR, &gt)) {
        if (!getGammaTableFrom(SANE_NAME_GAMMA_VECTOR_R, &gt)) {
            if (!getGammaTableFrom(SANE_NAME_GAMMA_VECTOR_G, &gt)) {
                if (!getGammaTableFrom(SANE_NAME_GAMMA_VECTOR_B, &gt)) {
                    // Should not happen... but if it does, no problem
                    // the dialogue will just use the default values
                    // for an empty gamma table.
                    //qDebug() << "no existing/active gamma option!";
                }
            }
        }
    }
    //qDebug() << "initial gamma table" << gt.toString();

// TODO; Maybe need to have a separate GUI widget for each gamma table, a
// little preview of the gamma curve (a GammaWidget) with an edit button.
// Will avoid the special case for the SANE_NAME_CUSTOM_GAMMA button followed
// by the edit button, and will allow separate editing of the R/G/B gamma
// tables if the scanner has them.

    GammaDialog gdiag(&gt, this);
    connect(&gdiag, &GammaDialog::gammaToApply, this, &ScanParams::slotApplyGamma);
    gdiag.exec();
}

void ScanParams::slotApplyGamma(const KGammaTable *gt)
{
    if (gt == NULL) {
        return;
    }

    bool reload = false;

    KScanOption *so = mSaneDevice->getOption(SANE_NAME_CUSTOM_GAMMA);
    if (so != NULL) {               // do we have a gamma switch?
        int cg = 0;
        if (so->get(&cg) && !cg) {          // yes, see if already on
            // if not, set it on now
            //qDebug() << "Setting gamma switch on";
            so->set(true);
            reload = so->apply();
        }
    }

    //qDebug() << "Applying gamma table" << gt->toString();
    reload |= setGammaTableTo(SANE_NAME_GAMMA_VECTOR, gt);
    reload |= setGammaTableTo(SANE_NAME_GAMMA_VECTOR_R, gt);
    reload |= setGammaTableTo(SANE_NAME_GAMMA_VECTOR_G, gt);
    reload |= setGammaTableTo(SANE_NAME_GAMMA_VECTOR_B, gt);

    if (reload) {
        mSaneDevice->reloadAllOptions();    // reload is needed
    }
}

// The user has changed an option.  Apply that;  as a result of doing so,
// it may be necessary to reload every other scanner option apart from
// this one.

void ScanParams::slotOptionChanged(KScanOption *so)
{
    if (so == NULL || mSaneDevice == NULL) {
        return;
    }
    mSaneDevice->applyOption(so);

    // Update the gamma edit button state, if the option exists
    setEditCustomGammaTableState();
}

// Enable editing of the gamma tables if any one of the gamma tables
// exists and is currently active.

void ScanParams::setEditCustomGammaTableState()
{
    if (mSaneDevice == NULL) {
        return;
    }
    if (mGammaEditButt == NULL) {
        return;
    }

    bool butState = false;

    KScanOption *so = mSaneDevice->getOption(SANE_NAME_CUSTOM_GAMMA, false);
    if (so != NULL) {
        butState = so->isActive();
    }

    if (!butState) {
        KScanOption *so = mSaneDevice->getOption(SANE_NAME_GAMMA_VECTOR, false);
        if (so != NULL) {
            butState = so->isActive();
        }
    }

    if (!butState) {
        KScanOption *so = mSaneDevice->getOption(SANE_NAME_GAMMA_VECTOR_R, false);
        if (so != NULL) {
            butState = so->isActive();
        }
    }

    if (!butState) {
        KScanOption *so = mSaneDevice->getOption(SANE_NAME_GAMMA_VECTOR_G, false);
        if (so != NULL) {
            butState = so->isActive();
        }
    }

    if (!butState) {
        KScanOption *so = mSaneDevice->getOption(SANE_NAME_GAMMA_VECTOR_B, false);
        if (so != NULL) {
            butState = so->isActive();
        }
    }

    //qDebug() << "State of edit custom gamma button=" << butState;
    mGammaEditButt->setEnabled(butState);
}

// This assumes that the SANE unit for the scan area is millimetres.
// All scanners out there appear to do this.
void ScanParams::applyRect(const QRect &rect)
{
    //qDebug() << "rect=" << rect;

    KScanOption *tl_x = mSaneDevice->getOption(SANE_NAME_SCAN_TL_X);
    KScanOption *tl_y = mSaneDevice->getOption(SANE_NAME_SCAN_TL_Y);
    KScanOption *br_x = mSaneDevice->getOption(SANE_NAME_SCAN_BR_X);
    KScanOption *br_y = mSaneDevice->getOption(SANE_NAME_SCAN_BR_Y);

    double min1, max1;
    double min2, max2;

    if (!rect.isValid()) {              // set full scan area
        tl_x->getRange(&min1, &max1); tl_x->set(min1);
        br_x->getRange(&min1, &max1); br_x->set(max1);
        tl_y->getRange(&min2, &max2); tl_y->set(min2);
        br_y->getRange(&min2, &max2); br_y->set(max2);

        //qDebug() << "setting full area" << min1 << min2 << "-" << max1 << max2;
    } else {
        double tlx = rect.left();
        double tly = rect.top();
        double brx = rect.right();
        double bry = rect.bottom();

        tl_x->getRange(&min1, &max1);
        if (tlx < min1) {
            brx += (min1 - tlx);
            tlx = min1;
        }
        tl_x->set(tlx); br_x->set(brx);

        tl_y->getRange(&min2, &max2);
        if (tly < min2) {
            bry += (min2 - tly);
            tly = min2;
        }
        tl_y->set(tly); br_y->set(bry);

        //qDebug() << "setting area" << tlx << tly << "-" << brx << bry;
    }

    tl_x->apply();
    tl_y->apply();
    br_x->apply();
    br_y->apply();
}

//  The previewer is telling us that the user has drawn or auto-selected a
//  new preview area (specified in millimetres).
void ScanParams::slotNewPreviewRect(const QRect &rect)
{
    //qDebug() << "rect=" << rect;

    applyRect(rect);
    mAreaSelect->selectSize(rect);
}

//  A new preset scan size or orientation chosen by the user
void ScanParams::slotScanSizeSelected(const QRect &rect)
{
    //qDebug() << "rect=" << rect << "full=" << !rect.isValid();

    applyRect(rect);
    emit newCustomScanSize(rect);
}

/*
 * sets the scan area to the default, which is the whole area.
 */
void ScanParams::setMaximalScanSize()
{
    //qDebug() << "Setting to default";
    slotScanSizeSelected(QRect());
}

void ScanParams::slotNewResolution(KScanOption *opt)
{
    KScanOption *opt_x = mSaneDevice->getExistingGuiElement(SANE_NAME_SCAN_X_RESOLUTION);
    if (opt_x == NULL) {
        opt_x = mSaneDevice->getExistingGuiElement(SANE_NAME_SCAN_RESOLUTION);
    }
    KScanOption *opt_y = mSaneDevice->getExistingGuiElement(SANE_NAME_SCAN_Y_RESOLUTION);

    int x_res = 0;                                      // get the X resolution
    if (opt_x != NULL && opt_x->isValid()) {
        opt_x->get(&x_res);
    }

    int y_res = 0;                                      // get the Y resolution
    if (opt_y != NULL && opt_y->isValid()) {
        opt_y->get(&y_res);
    }

    //qDebug() << "X/Y resolution" << x_res << y_res;

    if (y_res == 0) {
        y_res = x_res;    // use X res if Y unavailable
    }
    if (x_res == 0) {
        x_res = y_res;    // unlikely, but orthogonal
    }

    if (x_res == 0 && y_res == 0) {
        //qDebug() << "resolution not available!";
    } else {
        emit scanResolutionChanged(x_res, y_res);
    }
}

void ScanParams::slotNewScanMode()
{
    int format = SANE_FRAME_RGB;
    int depth = 8;
    mSaneDevice->getCurrentFormat(&format, &depth);

    int strips = (format == SANE_FRAME_GRAY ? 1 : 3);

    //qDebug() << "format" << format << "depth" << depth << "-> strips " << strips;

    if (strips == 1 && depth == 1) {        // bitmap scan
        emit scanModeChanged(0);            // 8 pixels per byte
    } else {
        // bytes per pixel
        emit scanModeChanged(strips * (depth == 16 ? 2 : 1));
    }
}

KScanDevice::Status ScanParams::performADFScan(void)
{
    KScanDevice::Status stat = KScanDevice::Ok;
    bool          scan_on = true;

    MassScanDialog *msd = new MassScanDialog(this);
    msd->show();

    /* The scan source should be set to ADF by the SourceSelect-Dialog */

    while (scan_on) {
        scan_on = false;
    }
    return (stat);
}