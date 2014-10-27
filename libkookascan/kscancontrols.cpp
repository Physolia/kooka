/* This file is part of the KDE Project
   Copyright (C) 2000 Klaas Freitag <freitag@suse.de>
   Copyright (C) 2010 Jonathan Marten <jjm@keelhaul.me.uk>

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

#include "kscancontrols.h"

#include <QGroupBox>
#include <qlayout.h>
#include <qtoolbutton.h>
#include <qspinbox.h>
#include <qcombobox.h>
#include <qcheckbox.h>
#include <qlabel.h>
#include <qslider.h>
#include <qlineedit.h>

#include <KLocalizedString>
#include <QDebug>
#include <kurlrequester.h>
#include <kimageio.h>
#include <QIcon>

//  KScanControl - base class
//  -------------------------

KScanControl::KScanControl(QWidget *parent, const QString &text)
    : QWidget(parent)
{
    mLayout = new QHBoxLayout(this);
    mLayout->setMargin(0);

    mText = text;
    if (mText.isEmpty()) {
        mText = i18n("(Unknown)");
    }
}

KScanControl::~KScanControl()               {}

QString KScanControl::text() const
{
    return (QString::null);
}
void KScanControl::setText(const QString &text)     {}

int KScanControl::value() const
{
    return (0);
}
void KScanControl::setValue(int val)            {}

QString KScanControl::label() const
{
    return (mText + ":");
}

//  KScanSlider - slider, spin box and optional reset button
//  --------------------------------------------------------

KScanSlider::KScanSlider(QWidget *parent, const QString &text,
                         double min, double max,
                         bool haveStdButt, int stdValue)
    : KScanControl(parent, text)
{
    mValue = mStdValue = stdValue;
    mStdButt = NULL;

    mSlider = new QSlider(Qt::Horizontal, this);    // slider
    mSlider->setRange(((int) min), ((int) max));
    mSlider->setTickPosition(QSlider::TicksBelow);
    mSlider->setTickInterval(qMax(((int)((max - min) / 10)), 1));
    mSlider->setSingleStep(qMax(((int)((max - min) / 20)), 1));
    mSlider->setPageStep(qMax(((int)((max - min) / 10)), 1));
    mSlider->setMinimumWidth(140);
    mSlider->setValue(mValue);              // initial value
    mLayout->addWidget(mSlider, 1);

    mSpinbox = new QSpinBox(this);          // spin box
    mSpinbox->setRange((int) min, (int) max);
    mSpinbox->setSingleStep(1);
    mSpinbox->setValue(mValue);             // initial value
    mLayout->addWidget(mSpinbox);

    if (haveStdButt) {
        mStdButt = new QToolButton(this);       // reset button
        mStdButt->setIcon(QIcon::fromTheme("edit-undo"));
        mStdButt->setToolTip(i18n("Reset this setting to its standard value, %1", stdValue));
        mLayout->addWidget(mStdButt);
    }

    connect(mSlider, SIGNAL(valueChanged(int)), SLOT(slotSliderSpinboxChange(int)));
    connect(mSpinbox, SIGNAL(valueChanged(int)), SLOT(slotSliderSpinboxChange(int)));
    if (mStdButt != NULL) {
        connect(mStdButt, SIGNAL(clicked()), SLOT(slotRevertValue()));
    }

    setFocusProxy(mSlider);
    setFocusPolicy(Qt::StrongFocus);
}

void KScanSlider::setValue(int val)
{
    if (val == mValue) {
        return;    // avoid recursive signals
    }
    mValue = val;

    int spin = mSpinbox->value();
    if (spin != val) {
        mSpinbox->blockSignals(true);
        mSpinbox->setValue(val);            // track in spin box
        mSpinbox->blockSignals(false);
    }

    int slid = mSlider->value();
    if (slid != val) {
        mSlider->blockSignals(true);
        mSlider->setValue(val);             // track in slider
        mSlider->blockSignals(false);
    }
}

int KScanSlider::value() const
{
    return (mValue);
}

void KScanSlider::slotSliderSpinboxChange(int val)
{
    setValue(val);
    emit settingChanged(val);
}

void KScanSlider::slotRevertValue()
{
    // only connected if button exists
    slotSliderSpinboxChange(mStdValue);
}

//  KScanStringEntry - free text entry field
//  ----------------------------------------

KScanStringEntry::KScanStringEntry(QWidget *parent, const QString &text)
    : KScanControl(parent, text)
{
    mEntry = new QLineEdit(this);
    mLayout->addWidget(mEntry);

    connect(mEntry, SIGNAL(textChanged(QString)), SIGNAL(settingChanged(QString)));
    connect(mEntry, SIGNAL(returnPressed()), SIGNAL(returnPressed()));

    setFocusProxy(mEntry);
    setFocusPolicy(Qt::StrongFocus);
}

QString KScanStringEntry::text() const
{
    return (mEntry->text());
}

void KScanStringEntry::setText(const QString &text)
{
    if (text == mEntry->text()) {
        return;    // avoid recursive signals
    }
    mEntry->setText(text);
}

//  KScanNumberEntry - number entry field
//  -------------------------------------

KScanNumberEntry::KScanNumberEntry(QWidget *parent, const QString &text)
    : KScanControl(parent, text)
{
    mEntry = new QLineEdit(this);
    mEntry->setValidator(new QIntValidator);
    mLayout->addWidget(mEntry);

    connect(mEntry, SIGNAL(textChanged(QString)), SLOT(slotTextChanged(QString)));
    connect(mEntry, SIGNAL(returnPressed()), SIGNAL(returnPressed()));

    setFocusProxy(mEntry);
    setFocusPolicy(Qt::StrongFocus);
}

int KScanNumberEntry::value() const
{
    return (mEntry->text().toInt());
}

void KScanNumberEntry::setValue(int i)
{
    mEntry->setText(QString::number(i));
}

void KScanNumberEntry::slotTextChanged(const QString &s)
{
    emit settingChanged(s.toInt());
}

//  KScanCheckbox - on/off option
//  -----------------------------

KScanCheckbox::KScanCheckbox(QWidget *parent, const QString &text)
    : KScanControl(parent, text)
{
    mCheckbox = new QCheckBox(text, this);
    mLayout->addWidget(mCheckbox);

    connect(mCheckbox, SIGNAL(stateChanged(int)), SIGNAL(settingChanged(int)));

    setFocusProxy(mCheckbox);
    setFocusPolicy(Qt::StrongFocus);
}

int KScanCheckbox::value() const
{
    return ((int) mCheckbox->isChecked());
}

void KScanCheckbox::setValue(int i)
{
    mCheckbox->setChecked((bool) i);
}

QString KScanCheckbox::label() const
{
    return (QString::null);
}

//  KScanCombo - combo box with list of options
//  -------------------------------------------
//
//  This control (and currently only this control) is special, because the
//  item text is set to the translated value of the option values.  But these
//  values need to reported back unchanged, so the untranslated form is stored
//  in the itemData and the translated form is seen by the user as itemText.
//  Any access needs to only set or report the itemData, never the itemText,
//  which is why the activated(QString) signal cannot be used directly.

KScanCombo::KScanCombo(QWidget *parent, const QString &text,
                       const QList<QByteArray> &list)
    : KScanControl(parent, text)
{
    init();

    for (QList<QByteArray>::const_iterator it = list.constBegin();
            it != list.constEnd(); ++it) {
        const QByteArray item = (*it);
        mCombo->addItem(i18n(item), item);
    }
}

void KScanCombo::init()
{
    mCombo = new QComboBox(this);
    mLayout->addWidget(mCombo);

    connect(mCombo, SIGNAL(activated(int)), SLOT(slotActivated(int)));

    setFocusProxy(mCombo);
    setFocusPolicy(Qt::StrongFocus);
}

void KScanCombo::setText(const QString &text)
{
    int i = mCombo->findData(text);         // find item with that text
    if (i == -1) {
        return;    // ignore if not present
    }

    if (i == mCombo->currentIndex()) {
        return;    // avoid recursive signals
    }
    mCombo->setCurrentIndex(i);
}

void KScanCombo::setIcon(const QIcon &icon, const char *ent)
{
    int i = mCombo->findData(ent);          // find item with that text
    if (i != -1) {
        mCombo->setItemIcon(i, icon);
    }
}

QString KScanCombo::text() const
{
    return (textAt(mCombo->currentIndex()));
}

void KScanCombo::setValue(int i)
{
    mCombo->setCurrentIndex(i);
}

QString KScanCombo::textAt(int i) const
{
    return (i == -1 ? QString::null : mCombo->itemData(i).toString());
}

int KScanCombo::count() const
{
    return (mCombo->count());
}

void KScanCombo::slotActivated(int i)
{
    emit settingChanged(i);
    emit settingChanged(textAt(i));
}

//  KScanFileRequester - standard URL requester
//  -------------------------------------------

KScanFileRequester::KScanFileRequester(QWidget *parent, const QString &text)
    : KScanControl(parent, text)
{
    mEntry = new KUrlRequester(this);
    mLayout->addWidget(mEntry);

    QString fileSelector = "*.pnm *.PNM *.pbm *.PBM *.pgm *.PGM *.ppm *.PPM|PNM Image Files (*.pnm,*.pbm,*.pgm,*.ppm)\n";
    fileSelector += KImageIO::pattern() + "\n";
    fileSelector += i18n("*|All Files");
    mEntry->setFilter(fileSelector);

    connect(mEntry, SIGNAL(textChanged(QString)), SIGNAL(settingChanged(QString)));
    connect(mEntry, SIGNAL(returnPressed()), SIGNAL(returnPressed()));

    setFocusProxy(mEntry);
    setFocusPolicy(Qt::StrongFocus);
}

QString KScanFileRequester::text() const
{
    return (mEntry->url().url());
}

void KScanFileRequester::setText(const QString &text)
{
    if (text == mEntry->url().url()) {
        return;    // avoid recursive signals
    }
    mEntry->setUrl(QUrl::fromLocalFile(text));
}

//  KScanGroup - group separator
//  ----------------------------

KScanGroup::KScanGroup(QWidget *parent, const QString &text)
    : KScanControl(parent, text)
{
    mGroup = new QGroupBox(text, this);
    mGroup->setFlat(true);
    mLayout->addWidget(mGroup);
}

QString KScanGroup::label() const
{
    return (QString::null);
}

//  KScanPushButton - action button
//  -------------------------------

KScanPushButton::KScanPushButton(QWidget *parent, const QString &text)
    : KScanControl(parent, text)
{
    mButton = new QPushButton(text, this);
    mLayout->addWidget(mButton);

    connect(mButton, SIGNAL(clicked()), SIGNAL(returnPressed()));
}

QString KScanPushButton::label() const
{
    return (QString::null);
}