/**************************************************************************
            kooka.h  -  Main program class
                             -------------------
    begin                : Sun Jan 16 2000
    copyright            : (C) 2000 by Klaas Freitag
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

#ifndef KOOKA_H
#define KOOKA_H

#include <kxmlguiwindow.h>

#include "kookaview.h"

#define KOOKA_STATE_GROUP "State"
#define PREFERENCE_DIA_TAB "PreferencesTab"

class KConfigGroup;
class KPrinter;
class KToggleAction;
class QAction;
class KActionMenu;

/**
 * This class serves as the main window for Kooka.  It handles the
 * menus, toolbars, and status bars.
 *
 * @short Main window class
 * @author Klaas Freitag <freitag@suse.de>
 * @version 0.1
 */
class Kooka : public KXmlGuiWindow
{
    Q_OBJECT

public:
    /**
     * Default Constructor
     */
    Kooka(const QByteArray &deviceToUse);

    /**
     * Default Destructor
     */
    ~Kooka();

    /**
     * Startup, loads (at the moment) only the last displayed image
     **/
    void startup();

protected:
    virtual void closeEvent(QCloseEvent *ev);

    /**
     * Overridden virtuals for Qt drag 'n drop (XDND)
     */
    virtual void dragEnterEvent(QDragEnterEvent *ev);
    // virtual void dropEvent(QDropEvent *event);

    /**
     * This function is called when it is time for the app to save its
     * properties for session management purposes.
     */
    void saveProperties(KConfigGroup &grp);

    /**
     * This function is called when this app is restored.  The KConfig
     * object points to the session management config file that was saved
     * with @ref saveProperties
     */
    void readProperties(const KConfigGroup &grp);

    virtual void applyMainWindowSettings(const KConfigGroup &grp) Q_DECL_OVERRIDE;

protected slots:
    void slotUpdateScannerActions(bool haveConnection);
    void slotUpdateRectangleActions(bool haveSelection);
    void slotUpdateViewActions(KookaView::StateFlags state);
    void slotUpdateOcrResultActions(bool haveText);
    void slotOpenWithMenu();
    void slotUpdateReadOnlyActions(bool ro);
    void slotUpdateAutoSelectActions(bool isAvailable, bool isOn);

private slots:
    void filePrint();

    void optionsPreferences();
    void optionsOcrPreferences();

private:
    void setupAccel();
    void setupActions();

private:
    KookaView *m_view;

    KPrinter   *m_printer;
    KToggleAction *m_scanParamsAction;
    KToggleAction *m_previewerAction;

    QAction *m_saveOCRTextAction;
    int m_prefDialogIndex;

    QAction *scanAction;
    QAction *previewAction;
    QAction *photocopyAction;
    QAction *paramsAction;
    KToggleAction *autoselAction;
    QAction *ocrAction;
    QAction *ocrSelectAction;
    QAction *ocrSpellAction;

    QAction *newFromSelectionAction;
    QAction *scaleToWidthAction;
    QAction *scaleToHeightAction;
    QAction *scaleToOriginalAction;
    QAction *scaleToZoomAction;
    KToggleAction *keepZoomAction;
    QAction *mirrorVerticallyAction;
    QAction *mirrorHorizontallyAction;
    QAction *rotateCwAction;
    QAction *rotateAcwAction;
    QAction *rotate180Action;

    QAction *createFolderAction;
    QAction *saveImageAction;
    QAction *printImageAction;
    QAction *importImageAction;
    QAction *deleteImageAction;
    QAction *renameImageAction;
    QAction *unloadImageAction;
    QAction *propsImageAction;

    QAction *selectDeviceAction;
    QAction *addDeviceAction;

    KActionMenu *openWithMenu;

    bool m_imageChangeAllowed;
};

#endif                          // KOOKA_H
