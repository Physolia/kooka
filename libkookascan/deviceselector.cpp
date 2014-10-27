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
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#include "deviceselector.h"

#include <qcheckbox.h>
#include <qlayout.h>
#include <qlabel.h>
#include <qlistwidget.h>

#include <kconfig.h>
#include <QDebug>
#include <KLocalizedString>
#include <kvbox.h>
#include <kstandarddirs.h>
#include <kguiitem.h>
#include <KIconLoader>
#include <KGlobal>
#include "scanglobal.h"
#include "scandevices.h"

DeviceSelector::DeviceSelector(QWidget *parent,
                               const QList<QByteArray> &backends,
                               const KGuiItem &cancelGuiItem)
    : KDialog(parent)
{
    setObjectName("DeviceSelector");

    setModal(true);
    setButtons(KDialog::Ok | KDialog::Cancel);
    setButtonText(KDialog::Ok, i18n("Select"));
    setCaption(i18n("Select Scan Device"));
    showButtonSeparator(true);
    if (!cancelGuiItem.text().isEmpty()) {      // special GUI item provided
        setButtonGuiItem(KDialog::Cancel, cancelGuiItem);
    }

    // Layout-Boxes
    QWidget *vb = new QWidget(this);
    QVBoxLayout *vlay = new QVBoxLayout(vb);
    setMainWidget(vb);

    QLabel *l = new QLabel(i18n("Available Scanners:"), vb);
    vlay->addWidget(l);

    mListBox = new QListWidget(vb);
    mListBox->setSelectionMode(QAbstractItemView::SingleSelection);
    mListBox->setUniformItemSizes(true);
    vlay->addWidget(mListBox, 1);
    l->setBuddy(mListBox);

    vlay->addSpacing(KDialog::spacingHint());

    mSkipCheckbox = new QCheckBox(i18n("Always use this device at startup"), vb);
    vlay->addWidget(mSkipCheckbox);

    const KConfigGroup grp = ScanGlobal::self()->configGroup();
    bool skipDialog = grp.readEntry(STARTUP_SKIP_ASK, false);
    mSkipCheckbox->setChecked(skipDialog);

    setMinimumSize(QSize(450, 200));

    setScanSources(backends);
}

DeviceSelector::~DeviceSelector()
{
}

QByteArray DeviceSelector::getDeviceFromConfig() const
{
    const KConfigGroup grp = ScanGlobal::self()->configGroup();
    QByteArray result = grp.readEntry(STARTUP_SCANDEV).toLocal8Bit();
    //qDebug() << "Scanner from config" << result;

    bool skipDialog = grp.readEntry(STARTUP_SKIP_ASK, false);
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
    KConfigGroup grp = ScanGlobal::self()->configGroup();
    grp.writeEntry(STARTUP_SCANDEV, dev);
    grp.writeEntry(STARTUP_SKIP_ASK, getShouldSkip());
    grp.sync();

    return (dev);
}

void DeviceSelector::setScanSources(const QList<QByteArray> &backends)
{
    const KConfig *typeConf = NULL;
    QString typeFile = KGlobal::dirs()->findResource("data", "libkscan/scantypes.dat");
    //qDebug() << "Scanner type file" << typeFile;
    if (!typeFile.isEmpty()) {
        typeConf = new KConfig(typeFile, KConfig::SimpleConfig);
    }

    const KConfigGroup grp = ScanGlobal::self()->configGroup();
    QByteArray defstr = grp.readEntry(STARTUP_SCANDEV).toLocal8Bit();

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

        hlay->setSpacing(KDialog::spacingHint());

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
        hlay->addSpacing(KDialog::spacingHint());
        hlay->addWidget(label);

        label = new QLabel(QString::fromLatin1("<qt><b>%1 %2</b><br>%3").arg(dev->vendor)
                           .arg(dev->model)
                           .arg(devName.constData()), hbox);
        label->setTextInteractionFlags(Qt::NoTextInteraction);
        hlay->addSpacing(KDialog::spacingHint());
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