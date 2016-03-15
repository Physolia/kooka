/************************************************************************
 *									*
 *  This file is part of Kooka, a scanning/OCR application using	*
 *  Qt <http://www.qt.io> and KDE Frameworks <http://www.kde.org>.	*
 *									*
 *  Copyright (C) 2000-2016 Klaas Freitag <freitag@suse.de>		*
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

#include "deviceselector.h"

#include <qcheckbox.h>
#include <qlayout.h>
#include <qlabel.h>
#include <qlistwidget.h>
#include <qstandardpaths.h>
#include <qdebug.h>

#include <klocalizedstring.h>
#include <kguiitem.h>
#include <kiconloader.h>

#include "scanglobal.h"
#include "scandevices.h"
#include "scansettings.h"


DeviceSelector::DeviceSelector(QWidget *pnt,
                               const QList<QByteArray> &backends,
                               const KGuiItem &cancelGuiItem)
    : DialogBase(pnt)
{
    setObjectName("DeviceSelector");

    setButtons(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);
    setButtonText(QDialogButtonBox::Ok, i18n("Select"));
    setWindowTitle(i18n("Select Scan Device"));
    if (!cancelGuiItem.text().isEmpty()) {      // special GUI item provided
        setButtonGuiItem(QDialogButtonBox::Cancel, cancelGuiItem);
    }

    // Layout-Boxes
    QWidget *vb = new QWidget(this);
    vb->setMinimumSize(QSize(450, 180));
    setMainWidget(vb);

    QVBoxLayout *vlay = new QVBoxLayout(vb);

    QLabel *l = new QLabel(i18n("Available Scanners:"), vb);
    vlay->addWidget(l);

    mListBox = new QListWidget(vb);
    mListBox->setSelectionMode(QAbstractItemView::SingleSelection);
    mListBox->setUniformItemSizes(true);
    vlay->addWidget(mListBox, 1);
    l->setBuddy(mListBox);

    vlay->addSpacing(verticalSpacing());

    mSkipCheckbox = new QCheckBox(i18n("Always use this device at startup"), vb);
    vlay->addWidget(mSkipCheckbox);

    bool skipDialog = ScanSettings::startupSkipAsk();
    mSkipCheckbox->setChecked(skipDialog);


    setScanSources(backends);
}

DeviceSelector::~DeviceSelector()
{
}

QByteArray DeviceSelector::getDeviceFromConfig() const
{
    QByteArray result = ScanSettings::startupScanDevice().toLocal8Bit();
    //qDebug() << "Scanner from config" << result;

    bool skipDialog = ScanSettings::startupSkipAsk();
    if (skipDialog && !result.isEmpty() && mDeviceList.contains(result)) {
        //qDebug() << "Using scanner from config";
    } else {
        //qDebug() << "Not using scanner from config";
        result = "";
    }

    return (result);
}

bool DeviceSelector::getShouldSkip() const
{
    return (mSkipCheckbox->isChecked());
}

QByteArray DeviceSelector::getSelectedDevice() const
{
    const QList<QListWidgetItem *> selItems = mListBox->selectedItems();
    if (selItems.count() < 1) {
        return ("");
    }

    int selIndex = mListBox->row(selItems.first()); // of selected item
    int dcount = mDeviceList.count();
    //qDebug() << "selected index" << selIndex << "of" << dcount;
    if (selIndex < 0 || selIndex >= dcount) {
        return ("");    // can never happen?
    }

    QByteArray dev = mDeviceList.at(selIndex).toLocal8Bit();
    //qDebug() << "selected device" << dev;

    // Save both the scan device and the skip-start-dialog flag
    // in the global "scannerrc" file.
    ScanSettings::setStartupScanDevice(dev);
    ScanSettings::setStartupSkipAsk(getShouldSkip());
    ScanSettings::self()->save();
   
    return (dev);
}

void DeviceSelector::setScanSources(const QList<QByteArray> &backends)
{
    const KConfig *typeConf = NULL;

    QString typeFile = QStandardPaths::locate(QStandardPaths::GenericDataLocation, "libkookascan/scantypes.dat");
    //qDebug() << "Scanner type file" << typeFile;
    if (!typeFile.isEmpty()) {
        typeConf = new KConfig(typeFile, KConfig::SimpleConfig);
    }

    QByteArray defstr = ScanSettings::startupScanDevice().toLocal8Bit();
    QListWidgetItem *defItem = NULL;

    for (QList<QByteArray>::const_iterator it = backends.constBegin();
            it != backends.constEnd(); ++it) {
        QByteArray devName = (*it);
        const SANE_Device *dev = ScanDevices::self()->deviceInfo(devName);
        if (dev == NULL) {
            //qDebug() << "no device info for" << devName;
            continue;
        }

        mDeviceList.append(devName);

        QListWidgetItem *item = new QListWidgetItem();

        QWidget *hbox = new QWidget(this);
        QHBoxLayout *hlay = new QHBoxLayout(hbox);
        hlay->setMargin(0);

        hlay->setSpacing(horizontalSpacing());

        QString itemIcon = "scanner";
        if (typeConf != NULL) {         // type config file available
            QString devBase = QString(devName).section(':', 0, 0);
            QString ii = typeConf->group("Devices").readEntry(devBase, "");
            //qDebug() << "for device" << devBase << "icon" << ii;
            if (!ii.isEmpty()) {
                itemIcon = ii;
            } else {
                ii = typeConf->group("Types").readEntry(dev->type, "");
                //qDebug() << "for type" << dev->type << "icon" << ii;
                if (!ii.isEmpty()) {
                    itemIcon = ii;
                }
            }
        }

        QLabel *label = new QLabel(hbox);
        label->setPixmap(KIconLoader::global()->loadIcon(itemIcon,
                         KIconLoader::NoGroup,
                         KIconLoader::SizeMedium));
        hlay->addSpacing(horizontalSpacing());
        hlay->addWidget(label);

        label = new QLabel(QString::fromLatin1("<qt><b>%1 %2</b><br>%3").arg(dev->vendor)
                           .arg(dev->model)
                           .arg(devName.constData()), hbox);
        label->setTextInteractionFlags(Qt::NoTextInteraction);
        hlay->addSpacing(horizontalSpacing());
        hlay->addWidget(label);

        hlay->addStretch(1);

        mListBox->addItem(item);
        mListBox->setItemWidget(item, hbox);
        item->setData(Qt::SizeHintRole, QSize(1, 40));  // X-size is ignored

        // Select this item (when finished) either if it matches the default
        // device, or else is the first item.
        if (defItem == NULL || devName == defstr) {
            defItem = item;
        }
    }

    if (defItem != NULL) {
        defItem->setSelected(true);
    }
    delete typeConf;
}
