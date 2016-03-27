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

#ifndef DIALOGBASE_H
#define DIALOGBASE_H

#include <qdialog.h>
#include <qdialogbuttonbox.h>
#include "libdialogutil_export.h"

class QShowEvent;
class KGuiItem;
class KConfigGroup;
class DialogStateSaver;


/**
 * @short A wrapper for QDialog incorportaing some convenience functions.
 *
 * This is a lightweight wrapper around QDialog, incorporating some useful
 * functions which used to be provided by KDialog in KDE4.  These are:
 *
 * - Managing the button box and providing access to its buttons
 * - Managing the top level layout
 * - Generating a button separator line if required
 * - Saving and restoring the dialog size
 *
 * @author Jonathan Marten
 **/

class LIBDIALOGUTIL_EXPORT DialogBase : public QDialog
{
    Q_OBJECT

public:
    /**
     * Destructor.
     *
     **/
    virtual ~DialogBase();

    /**
     * Retrieve the main widget.
     *
     * @return the main widget
     **/
    QWidget *mainWidget() const				{ return (mMainWidget); }

    /**
     * Set whether a button separator line is shown for
     * this instance of the dialog.
     *
     * @param on Whether the button separator is to be shown
     *
     * @note This option should be set before the dialog is shown
     * for the first time;  changing the setting afterwards has
     * no effect.
     *
     * @note If this is not explicitly set for a particular instance
     * of the dialog, the default is whatever has been set by
     * @c setButtonSeparatorShownDefault().
     **/
    void setButtonSeparatorShown(bool on)		{ mButtonSeparatorShown = on; }

    /**
     * Retrieve whether the button separator line is shown.
     *
     * @return @c true if the button separator is shown,
     * @c false if it is not.
     **/
    bool isButtonSeparatorShown() const			{ return (mButtonSeparatorShown); }

    /**
     * Set a state saver for the dialog.
     *
     * This may be a subclass of a DialogStateSaver, reimplemented in
     * order to save special dialog settings (e.g. the column states of
     * a list view).  If this is not set then a plain DialogStateSaver
     * will be created and used internally.
     *
     * @param saver the state saver
     *
     * @note The saver should be set before the dialog is shown for
     * the first time.
     * @see DialogStateSaver
     **/
    void setStateSaver(DialogStateSaver *saver);

    /**
     * Set the default option of whether the button separator line
     * is shown. The default is @c true. This is an application-wide
     * setting which affects all subsequently created dialogs.
     *
     * @param on Whether the button separator is to be shown
     **/
    static void setButtonSeparatorShownDefault(bool on);

    /**
     * Get a spacing hint suitable for use within the dialog layout.
     *
     * @return The spacing hint
     * @deprecated Kept for compatiblity with KDE4.
     * Use @c verticalSpacing() or @c horizontalSpacing() as appropriate.
     **/
    static Q_DECL_DEPRECATED int spacingHint();

    /**
     * Get a vertical spacing suitable for use within the dialog layout.
     *
     * @return The spacing hint
     **/
    static int verticalSpacing();

    /**
     * Get a horizontal spacing suitable for use within the dialog layout.
     *
     * @return The spacing hint
     **/
    static int horizontalSpacing();

protected:

    /**
     * Constructor.
     *
     * @param pnt Parent widget
     **/
    explicit DialogBase(QWidget *pnt = NULL);

    /**
     * Set the main widget to be displayed within the dialog.
     *
     * @param w The widget
     **/
    void setMainWidget(QWidget *w)			{ mMainWidget = w; }

    /**
     * Access the dialog's button box.
     *
     * @return the button box
     **/
    QDialogButtonBox *buttonBox() const			{ return (mButtonBox); }

    /**
     * Set the standard buttons to be displayed within the button box.
     *
     * @param buttons The buttons required
     *
     * @note This can be called at any time and the buttons will change
     * accordingly.  However, the buttons will be regenerated which means
     * that any special button text or icons, or any signal connections from
     * them, will be lost.
     **/
    void setButtons(QDialogButtonBox::StandardButtons buttons);

    /**
     * Set the enable state of a button.
     *
     * @param button The button to set
     * @param state The enable state for the button
     **/
    void setButtonEnabled(QDialogButtonBox::StandardButton button, bool state = true);

    /**
     * Set the text of a button.
     *
     * @param button The button to set
     * @param state The new text for the button
     *
     * @note This can be called at any time, and the button will change
     * accordingly.
     **/
    void setButtonText(QDialogButtonBox::StandardButton button, const QString &text);

    /**
     * Set the icon of a button.
     *
     * @param button The button to set
     * @param state The new icon for the button
     *
     * @note This can be called at any time, and the button will change
     * accordingly.
     **/
    void setButtonIcon(QDialogButtonBox::StandardButton button, const QIcon &icon);

    /**
     * Set up a button from a @c KGuiItem.
     *
     * @param button The button to set
     * @param guiItem The @c KGuiItem for the button
     *
     * @note This can be called at any time, and the button will change
     * accordingly.
     **/
    void setButtonGuiItem(QDialogButtonBox::StandardButton button, const KGuiItem &guiItem);

    /**
     * @reimp
     **/
    virtual void showEvent(QShowEvent *ev);

private:
    QDialogButtonBox *mButtonBox;
    QWidget *mMainWidget;
    bool mButtonSeparatorShown;
    DialogStateSaver *mStateSaver;
};

#endif							// DIALOGBASE_H