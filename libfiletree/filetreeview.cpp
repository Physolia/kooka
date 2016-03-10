/* This file is part of the KDEproject
   Copyright (C) 2000 David Faure <faure@kde.org>
                 2000 Carsten Pfeiffer <pfeiffer@kde.org>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License version 2 as published by the Free Software Foundation.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#include "filetreeview.h"

#include <stdlib.h>

#include <qevent.h>
#include <qdir.h>
#include <qapplication.h>
#include <qtimer.h>
#include <qmimedata.h>
#include <qdebug.h>

#include <kglobalsettings.h>
#include <kfileitem.h>
#include <kstandarddirs.h>

#include <kio/job.h>
#include <kio/global.h>

#include "filetreeviewitem.h"
#include "filetreebranch.h"


#undef DEBUG_LISTING


FileTreeView::FileTreeView(QWidget *parent)
    : QTreeWidget(parent)
{
    setObjectName("FileTreeView");
    //qDebug();

    setSelectionMode(QAbstractItemView::SingleSelection);
    setExpandsOnDoubleClick(false);         // we'll handle this ourselves
    setEditTriggers(QAbstractItemView::NoEditTriggers); // maybe changed later

    m_wantOpenFolderPixmaps = true;
    m_currentBeforeDropItem = NULL;
    m_dropItem = NULL;
    m_busyCount = 0;

    m_autoOpenTimer = new QTimer(this);
    m_autoOpenTimer->setInterval((QApplication::startDragTime() * 3) / 2);
    connect(m_autoOpenTimer, SIGNAL(timeout()), SLOT(slotAutoOpenFolder()));

    /* The executed-Slot only opens  a path, while the expanded-Slot populates it */
    connect(this, SIGNAL(itemActivated(QTreeWidgetItem*,int)),
            SLOT(slotExecuted(QTreeWidgetItem*)));
    connect(this, SIGNAL(itemExpanded(QTreeWidgetItem*)),
            SLOT(slotExpanded(QTreeWidgetItem*)));
    connect(this, SIGNAL(itemCollapsed(QTreeWidgetItem*)),
            SLOT(slotCollapsed(QTreeWidgetItem*)));
    connect(this, SIGNAL(itemDoubleClicked(QTreeWidgetItem*,int)),
            SLOT(slotDoubleClicked(QTreeWidgetItem*)));

    connect(model(), SIGNAL(dataChanged(QModelIndex,QModelIndex)),
            SLOT(slotDataChanged(QModelIndex,QModelIndex)));

    /* connections from the konqtree widget */
    connect(this, SIGNAL(itemSelectionChanged()),
            SLOT(slotSelectionChanged()));
    connect(this, SIGNAL(itemEntered(QTreeWidgetItem*,int)),
            SLOT(slotOnItem(QTreeWidgetItem*)));

    m_openFolderPixmap = QIcon::fromTheme("folder-open");
}

FileTreeView::~FileTreeView()
{
    // we must make sure that the FileTreeTreeViewItems are deleted _before_ the
    // branches are deleted. Otherwise, the KFileItems would be destroyed
    // and the FileTreeViewItems had dangling pointers to them.
    hide();
    clear();

    qDeleteAll(m_branches);
    m_branches.clear();
}

// This is used when dragging and dropping out of the view to somewhere else.
QMimeData *FileTreeView::mimeData(const QList<QTreeWidgetItem *> items) const
{
    QMimeData *mimeData = new QMimeData();
    QList<QUrl> urlList;

    for (QList<QTreeWidgetItem *>::const_iterator it = items.constBegin();
            it != items.constEnd(); ++it) {
        FileTreeViewItem *item = static_cast<FileTreeViewItem *>(*it);
#ifdef DEBUG_LISTING
        qDebug() << item->url();
#endif // DEBUG_LISTING
        urlList.append(item->url());
    }

    mimeData->setUrls(urlList);
    return (mimeData);
}

// Dragging and dropping into the view.
void FileTreeView::setDropItem(QTreeWidgetItem *item)
{
    if (item != NULL) {
        m_dropItem = item;
        // TODO: make auto-open an option, don't start timer if not enabled
        m_autoOpenTimer->start();
    } else {
        m_dropItem = NULL;
        m_autoOpenTimer->stop();
    }
}

void FileTreeView::dragEnterEvent(QDragEnterEvent *ev)
{
    if (!ev->mimeData()->hasUrls()) {       // not an URL drag
        ev->ignore();
        return;
    }

    ev->acceptProposedAction();

    QList<QTreeWidgetItem *> items = selectedItems();
    m_currentBeforeDropItem = (items.count() > 0 ? items.first() : NULL);
    setDropItem(itemAt(ev->pos()));
}

void FileTreeView::dragMoveEvent(QDragMoveEvent *ev)
{
    if (!ev->mimeData()->hasUrls()) {       // not an URL drag
        ev->ignore();
        return;
    }

    QTreeWidgetItem *item = itemAt(ev->pos());
    if (item == NULL || item->isDisabled()) {   // over a valid item?
        // no, ignore drops on it
        setDropItem(NULL);              // clear drop item
        return;
    }

    FileTreeViewItem *ftvi = static_cast<FileTreeViewItem *>(item);
    //if (!ftvi->isDir()) item = item->parent();    // if file, highlight parent dir

    setCurrentItem(item);               // temporarily select it
    if (item != m_dropItem) {
        setDropItem(item);    // changed, update drop item
    }

    ev->accept();
}

void FileTreeView::dragLeaveEvent(QDragLeaveEvent *ev)
{
    if (m_currentBeforeDropItem != NULL) {      // there was a current item
        // before the drag started
        setCurrentItem(m_currentBeforeDropItem);    // restore its selection
        scrollToItem(m_currentBeforeDropItem);
    } else if (m_dropItem != NULL) {        // item selected by drag
        m_dropItem->setSelected(false);         // clear that selection
    }

    m_currentBeforeDropItem = NULL;
    setDropItem(NULL);
}

void FileTreeView::dropEvent(QDropEvent *ev)
{
    if (!ev->mimeData()->hasUrls()) {       // not an URL drag
        ev->ignore();
        return;
    }

    if (m_dropItem == NULL) {
        return;    // invalid drop target
    }

    FileTreeViewItem *item = static_cast<FileTreeViewItem *>(m_dropItem);
#ifdef DEBUG_LISTING
    qDebug() << "onto" << item->url();
#endif // DEBUG_LISTING
    setDropItem(NULL);					// stop timer now
							// also clears m_dropItem!
    emit dropped(ev, item);
    ev->accept();
}

void FileTreeView::slotCollapsed(QTreeWidgetItem *tvi)
{
    FileTreeViewItem *item = static_cast<FileTreeViewItem *>(tvi);
    if (item != NULL && item->isDir()) {
        item->setIcon(0, itemIcon(item));
    }
}

void FileTreeView::slotExpanded(QTreeWidgetItem *tvi)
{
    FileTreeViewItem *item = static_cast<FileTreeViewItem *>(tvi);
    if (item == NULL) {
        return;
    }

#ifdef DEBUG_LISTING
    qDebug() << item->text(0);
#endif // DEBUG_LISTING

    FileTreeBranch *branch = item->branch();

    // Check if the branch needs to be populated now
    if (item->isDir() && branch != NULL && item->childCount() == 0) {
#ifdef DEBUG_LISTING
        qDebug() << "need to populate" << item->url();
#endif // DEBUG_LISTING
        if (!branch->populate(item->url(), item)) {
            //qDebug() << "Branch populate failed!";
        }
    }

    // set pixmap for open folders
    if (item->isDir() && item->isExpanded()) {
        item->setIcon(0, itemIcon(item));
    }
}

// Called when an item is single- or double-clicked, according to the
// configured selection model.
//
// If the item is a branch root, we don't want to expand/collapse it on
// a single click, but just to select it.  An explicit double click will
// do the expand/collapse.

void FileTreeView::slotExecuted(QTreeWidgetItem *item)
{
    if (item == NULL) {
        return;
    }

    FileTreeViewItem *ftvi = static_cast<FileTreeViewItem *>(item);
    if (ftvi != NULL && ftvi->isDir() && !ftvi->isRoot()) {
        item->setExpanded(!item->isExpanded());
    }
}

void FileTreeView::slotDoubleClicked(QTreeWidgetItem *item)
{
    if (item == NULL) {
        return;
    }

    FileTreeViewItem *ftvi = static_cast<FileTreeViewItem *>(item);
    if (ftvi != NULL && ftvi->isRoot()) {
        item->setExpanded(!item->isExpanded());
    }
}

void FileTreeView::slotAutoOpenFolder()
{
    m_autoOpenTimer->stop();

#ifdef DEBUG_LISTING
    qDebug() << "children" << m_dropItem->childCount() << "expanded" << m_dropItem->isExpanded();
#endif // DEBUG_LISTING
    if (m_dropItem->childCount() == 0) {
        return;						// nothing to expand
    }
    if (m_dropItem->isExpanded()) {
        return;						// already expanded
    }

    m_dropItem->setExpanded(true);			// expand the item
}

void FileTreeView::slotSelectionChanged()
{
    if (m_dropItem != NULL) {           // don't do this during the dragmove
    }
}

void FileTreeView::slotDataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight)
{
    if (topLeft.column() != 0) {
        return;    // not the file name
    }
    if (topLeft.row() != bottomRight.row()) {
        return;    // not a single row
    }
    if (topLeft.column() != bottomRight.column()) {
        return;    // not a single column
    }

    QTreeWidgetItem *twi = itemFromIndex(topLeft);
    if (twi == NULL) {
        return;
    }
    FileTreeViewItem *item = static_cast<FileTreeViewItem *>(twi);

    QString oldName = item->url().fileName();
    QString newName = item->text(0);
    if (oldName == newName) {
        return;    // no change
    }
    if (newName.isEmpty()) {
        return;    // no new name
    }

    emit fileRenamed(item, newName);
    item->branch()->itemRenamed(item);          // update branch's item map
}

FileTreeBranch *FileTreeView::addBranch(const QUrl &path, const QString &name,
                                        bool showHidden)
{
    const QIcon &folderPix = QIcon::fromTheme("inode-directory");
    return (addBranch(path, name, folderPix, showHidden));
}

FileTreeBranch *FileTreeView::addBranch(const QUrl &path, const QString &name,
                                        const QIcon &pix, bool showHidden)
{
    //qDebug() << path;

    /* Open a new branch */
    FileTreeBranch *newBranch = new FileTreeBranch(this, path, name, pix,
            showHidden);
    return (addBranch(newBranch));
}

FileTreeBranch *FileTreeView::addBranch(FileTreeBranch *newBranch)
{
    connect(newBranch, SIGNAL(populateStarted(FileTreeViewItem*)),
            SLOT(slotStartAnimation(FileTreeViewItem*)));
    connect(newBranch, SIGNAL(populateFinished(FileTreeViewItem*)),
            SLOT(slotStopAnimation(FileTreeViewItem*)));

    connect(newBranch, SIGNAL(newTreeViewItems(FileTreeBranch*,FileTreeViewItemList)),
            SLOT(slotNewTreeViewItems(FileTreeBranch*,FileTreeViewItemList)));

    m_branches.append(newBranch);
    return (newBranch);
}

FileTreeBranch *FileTreeView::branch(const QString &searchName) const
{
    for (FileTreeBranchList::const_iterator it = m_branches.constBegin();
            it != m_branches.constEnd(); ++it) {
        FileTreeBranch *branch = (*it);
        QString bname = branch->name();
#ifdef DEBUG_LISTING
        qDebug() << "branch" << bname;
#endif // DEBUG_LISTING
        if (bname == searchName) {
#ifdef DEBUG_LISTING
            qDebug() << "Found requested branch";
#endif // DEBUG_LISTING
            return (branch);
        }
    }

    return (NULL);
}

const FileTreeBranchList &FileTreeView::branches() const
{
    return (m_branches);
}

bool FileTreeView::removeBranch(FileTreeBranch *branch)
{
    if (m_branches.contains(branch)) {
        delete branch->root();
        m_branches.removeOne(branch);
        return (true);
    } else {
        return (false);
    }
}

void FileTreeView::setDirOnlyMode(FileTreeBranch *branch, bool bom)
{
    if (branch != NULL) {
        branch->setDirOnlyMode(bom);
    }
}

void FileTreeView::slotNewTreeViewItems(FileTreeBranch *branch, const FileTreeViewItemList &items)
{
    if (branch == NULL) {
        return;
    }
#ifdef DEBUG_LISTING
    qDebug();
#endif // DEBUG_LISTING

    /* Sometimes it happens that new items should become selected, i.e. if the user
     * creates a new dir, he probably wants it to be selected. This can not be done
     * right after creating the directory or file, because it takes some time until
     * the item appears here in the treeview. Thus, the creation code sets the member
     * m_neUrlToSelect to the required url. If this url appears here, the item becomes
     * selected and the member nextUrlToSelect will be cleared.
     */
    if (!m_nextUrlToSelect.isEmpty()) {
        for (FileTreeViewItemList::const_iterator it = items.constBegin();
                it != items.constEnd(); ++it) {
            QUrl url = (*it)->url();

            if (m_nextUrlToSelect.adjusted(QUrl::StripTrailingSlash|QUrl::NormalizePathSegments) ==
                url.adjusted(QUrl::StripTrailingSlash|QUrl::NormalizePathSegments)) {
                setCurrentItem(static_cast<QTreeWidgetItem *>(*it));
                m_nextUrlToSelect = QUrl();
                break;
            }
        }
    }
}

QIcon FileTreeView::itemIcon(FileTreeViewItem *item) const
{
    QIcon pix;

    if (item != NULL) {
        /* Check whether it is a branch root */
        FileTreeBranch *branch = item->branch();
        if (item == branch->root()) {
            pix = branch->pixmap();
            if (m_wantOpenFolderPixmaps && branch->root()->isExpanded()) {
                pix = branch->openPixmap();
            }
        } else {
            // TODO: different modes, user Pixmaps ?
            pix = QIcon::fromTheme(item->fileItem()->iconName());
            /* Only if it is a dir and the user wants open dir pixmap and it is open,
             * change the fileitem's pixmap to the open folder pixmap. */
            if (item->isDir() && m_wantOpenFolderPixmaps) {
                if (item->isExpanded()) {
                    pix = m_openFolderPixmap;
                }
            }
        }
    }

    return (pix);
}

void FileTreeView::slotStartAnimation(FileTreeViewItem *item)
{
    if (item == NULL) {
        return;
    }
#ifdef DEBUG_LISTING
    qDebug() << "for" << item->text(0);
#endif // DEBUG_LISTING

    ++m_busyCount;
    setCursor(Qt::BusyCursor);
}

void FileTreeView::slotStopAnimation(FileTreeViewItem *item)
{
    if (item == NULL) {
        return;
    }
#ifdef DEBUG_LISTING
    qDebug() << "for" << item->text(0);
#endif // DEBUG_LISTING
    if (m_busyCount <= 0) {
        return;
    }

    --m_busyCount;
    if (m_busyCount == 0) {
        unsetCursor();
    }
}

FileTreeViewItem *FileTreeView::selectedFileTreeViewItem() const
{
    QList<QTreeWidgetItem *> items = selectedItems();
    return (items.count() > 0 ? static_cast<FileTreeViewItem *>(items.first()) : NULL);
}

const KFileItem *FileTreeView::selectedFileItem() const
{
    FileTreeViewItem *item = selectedFileTreeViewItem();
    return (item == NULL ? NULL : item->fileItem());
}

QUrl FileTreeView::selectedUrl() const
{
    FileTreeViewItem *item = selectedFileTreeViewItem();
    return (item != NULL ? item->url() : QUrl());
}

FileTreeViewItem *FileTreeView::highlightedFileTreeViewItem() const
{
    return (static_cast<FileTreeViewItem *>(currentItem()));
}

const KFileItem *FileTreeView::highlightedFileItem() const
{
    FileTreeViewItem *item = highlightedFileTreeViewItem();
    return (item == NULL ? NULL : item->fileItem());
}

QUrl FileTreeView::highlightedUrl() const
{
    FileTreeViewItem *item = highlightedFileTreeViewItem();
    return (item != NULL ? item->url() : QUrl());
}

void FileTreeView::slotOnItem(QTreeWidgetItem *item)
{
    FileTreeViewItem *i = static_cast<FileTreeViewItem *>(item);
    if (i != NULL) emit onItem(i->url().url(QUrl::PreferLocalFile));
}

FileTreeViewItem *FileTreeView::findItemInBranch(const QString &branchName, const QString &relUrl) const
{
    FileTreeBranch *br = branch(branchName);
    return (findItemInBranch(br, relUrl));
}

FileTreeViewItem *FileTreeView::findItemInBranch(FileTreeBranch *branch, const QString &relUrl) const
{
    FileTreeViewItem *ret = NULL;
    if (branch != NULL) {
        if (relUrl.isEmpty() || relUrl == "/") {
            ret = branch->root();
        } else {
            QString partUrl(relUrl);
            if (partUrl.endsWith('/')) {
                partUrl.chop(1);
            }

            QUrl url = branch->rootUrl().resolved(QUrl::fromUserInput(relUrl));
#ifdef DEBUG_LISTING
            qDebug() << "searching for" << url;
#endif // DEBUG_LISTING
            ret = branch->findItemByUrl(url);
        }
    }

    return (ret);
}

bool FileTreeView::showFolderOpenPixmap() const
{
    return (m_wantOpenFolderPixmaps);
}

void FileTreeView::setShowFolderOpenPixmap(bool showIt)
{
    m_wantOpenFolderPixmaps = showIt;
}

void FileTreeView::slotSetNextUrlToSelect(const QUrl &url)
{
    m_nextUrlToSelect = url;
}
