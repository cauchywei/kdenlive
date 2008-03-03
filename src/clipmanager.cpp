/***************************************************************************
 *   Copyright (C) 2008 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA          *
 ***************************************************************************/
#include <KDebug>
#include <KFileDialog>

#include "addclipcommand.h"
#include "kdenlivesettings.h"
#include "clipmanager.h"
#include "docclipbase.h"

ClipManager::ClipManager(KdenliveDoc *doc): m_doc(doc) {
    m_clipIdCounter = 1;
}

ClipManager::~ClipManager() {
}

void ClipManager::setThumbsProgress(KUrl url, int progress) {
    m_doc->setThumbsProgress(url, progress);
}


void ClipManager::addClip(DocClipBase *clip) {
    m_clipList.append(clip);
}

void ClipManager::slotDeleteClip(uint clipId) {
    for (int i = 0; i < m_clipList.count(); i++) {
        if (m_clipList.at(i)->getId() == clipId) {
            //m_clipList.removeAt(i);
            AddClipCommand *command = new AddClipCommand(m_doc, m_clipList.at(i)->toXML(), clipId, false);
            m_doc->commandStack()->push(command);
            break;
        }
    }
}

void ClipManager::deleteClip(uint clipId) {
    for (int i = 0; i < m_clipList.count(); i++) {
        if (m_clipList.at(i)->getId() == clipId) {
            m_clipList.removeAt(i);
            break;
        }
    }
}

DocClipBase *ClipManager::getClipAt(int pos) {
    return m_clipList.at(pos);
}

DocClipBase *ClipManager::getClipById(int clipId) {
    kDebug() << "++++  CLIP MAN, LOOKING FOR CLIP ID: " << clipId;
    for (int i = 0; i < m_clipList.count(); i++) {
        if (m_clipList.at(i)->getId() == clipId) {
            kDebug() << "++++  CLIP MAN, FOUND FOR CLIP ID: " << clipId;
            return m_clipList.at(i);
        }
    }
    return NULL;
}

void ClipManager::slotAddClipFile(const KUrl url, const QString group) {
    kDebug() << "/////  CLIP MANAGER, ADDING CLIP: " << url;
    QDomDocument doc;
    QDomElement prod = doc.createElement("producer");
    prod.setAttribute("resource", url.path());
    uint id = m_clipIdCounter++;
    prod.setAttribute("id", QString::number(id));
    if (!group.isEmpty()) prod.setAttribute("group", group);
    KMimeType::Ptr type = KMimeType::findByUrl(url);
    if (type->name().startsWith("image/")) {
        prod.setAttribute("type", (int) IMAGE);
        prod.setAttribute("in", "0");
        prod.setAttribute("out", m_doc->getFramePos(KdenliveSettings::image_duration()));
    }
    AddClipCommand *command = new AddClipCommand(m_doc, prod, id, true);
    m_doc->commandStack()->push(command);
}

void ClipManager::slotAddColorClipFile(const QString name, const QString color, QString duration, const QString group) {
    QDomDocument doc;
    QDomElement prod = doc.createElement("producer");
    prod.setAttribute("mlt_service", "colour");
    prod.setAttribute("colour", color);
    prod.setAttribute("type", (int) COLOR);
    uint id = m_clipIdCounter++;
    prod.setAttribute("id", QString::number(id));
    prod.setAttribute("in", "0");
    prod.setAttribute("out", m_doc->getFramePos(duration));
    prod.setAttribute("name", name);
    AddClipCommand *command = new AddClipCommand(m_doc, prod, id, true);
    m_doc->commandStack()->push(command);
}

