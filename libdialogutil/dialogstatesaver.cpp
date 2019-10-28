/************************************************************************
 *									*
 *  This file is part of Kooka, a scanning/OCR application using	*
 *  Qt <http://www.qt.io> and KDE Frameworks <http://www.kde.org>.	*
 *									*
 *  Copyright (C) 2016 Jonathan Marten <jjm@keelhaul.me.uk>		*
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

#include "dialogstatesaver.h"

#include <qdialog.h>
#include <qwindow.h>
#include <qscreen.h>
#include <qdebug.h>

#include <kconfiggroup.h>
#include <ksharedconfig.h>


static bool sSaveSettings = true;


DialogStateSaver::DialogStateSaver(QDialog *pnt)
{
    Q_ASSERT(pnt!=nullptr);
    mParent = pnt;
}


static KConfigGroup configGroupFor(QWidget *window)
{
    QString objName = window->objectName();
    if (objName.isEmpty())
    {
        objName = window->metaObject()->className();
        qWarning() << "object name not set, using class name" << objName;
    }
    else qDebug() << "for" << objName << "which is a" << window->metaObject()->className();

    return (KSharedConfig::openConfig(QString(), KConfig::NoCascade)->group(objName));
}


void DialogStateSaver::restoreConfig()
{
    if (!sSaveSettings) return;				// settings not to be restored

    const KConfigGroup grp = configGroupFor(mParent);
    this->restoreConfig(mParent, grp);
}


void DialogStateSaver::restoreConfig(QDialog *dialog, const KConfigGroup &grp)
{
    restoreWindowState(dialog, grp);
}


void DialogStateSaver::restoreWindowState(QWidget *window)
{
    const KConfigGroup grp = configGroupFor(window);
    restoreWindowState(window, grp);
}


void DialogStateSaver::restoreWindowState(QWidget *window, const KConfigGroup &grp)
{
    // originally from KDE4 KDialog::restoreDialogSize()
    const QRect desk = window->parentWidget()->window()->windowHandle()->screen()->geometry();
    const QSize sizeDefault = window->sizeHint();

    qDebug() << "from" << grp.name() << "in" << grp.config()->name();
    const int width = grp.readEntry(QString::fromLatin1("Width %1").arg(desk.width()), sizeDefault.width());
    const int height = grp.readEntry(QString::fromLatin1("Height %1").arg(desk.height()), sizeDefault.height());
    window->resize(width, height);
}


void DialogStateSaver::saveConfig() const
{
    if (!sSaveSettings) return;				// settings not to be saved

    KConfigGroup grp = configGroupFor(mParent);
    this->saveConfig(mParent, grp);
    grp.sync();
}


void DialogStateSaver::saveConfig(QDialog *dialog, KConfigGroup &grp) const
{
    saveWindowState(dialog, grp);
}


void DialogStateSaver::saveWindowState(QWidget *window)
{
    KConfigGroup grp = configGroupFor(window);
    saveWindowState(window, grp);
}


void DialogStateSaver::saveWindowState(QWidget *window, KConfigGroup &grp)
{
    // originally from KDE4 KDialog::saveDialogSize()
    const QRect desk = window->parentWidget()->window()->windowHandle()->screen()->geometry();
    const QSize sizeToSave = window->size();

    qDebug() << "to" << grp.name() << "in" << grp.config()->name();
    grp.writeEntry(QString::fromLatin1("Width %1").arg(desk.width()), sizeToSave.width());
    grp.writeEntry( QString::fromLatin1("Height %1").arg(desk.height()), sizeToSave.height());
    grp.sync();
}


void DialogStateSaver::setSaveSettingsDefault(bool on)
{
    sSaveSettings = on;
}
