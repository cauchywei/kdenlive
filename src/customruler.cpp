/***************************************************************************
 *   Copyright (C) 2007 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
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

#include "customruler.h"
#include "kdenlivesettings.h"

#include <KDebug>
#include <KIcon>
#include <KCursor>
#include <KGlobalSettings>

#include <QApplication>
#include <QMouseEvent>
#include <QStylePainter>

static const int FIX_WIDTH = 24; /* widget width in pixel */
static const int LINE_END = (FIX_WIDTH - 3);
static const int END_MARK_LENGTH = (FIX_WIDTH - 8);
static const int BIG_MARK_LENGTH = (END_MARK_LENGTH * 3 / 4);
static const int BIG_MARK_X2 = LINE_END;
static const int BIG_MARK_X1 = (BIG_MARK_X2 - BIG_MARK_LENGTH);
static const int MIDDLE_MARK_LENGTH = (END_MARK_LENGTH / 2);
static const int MIDDLE_MARK_X2 = LINE_END;
static const int MIDDLE_MARK_X1 = (MIDDLE_MARK_X2 - MIDDLE_MARK_LENGTH);
static const int LITTLE_MARK_LENGTH = (MIDDLE_MARK_LENGTH / 2);
static const int LITTLE_MARK_X2 = LINE_END;
static const int LITTLE_MARK_X1 = (LITTLE_MARK_X2 - LITTLE_MARK_LENGTH);

static int LABEL_SIZE;
static const int END_LABEL_X = 4;
static const int END_LABEL_Y = (END_LABEL_X + LABEL_SIZE - 2);

static int littleMarkDistance;
static int mediumMarkDistance;
static int bigMarkDistance;

#include "definitions.h"

const int CustomRuler::comboScale[] = { 1, 2, 5, 10, 25, 50, 125, 250, 500, 750, 1500, 3000, 6000, 12000};

CustomRuler::CustomRuler(Timecode tc, CustomTrackView *parent) :
        QWidget(parent),
        m_timecode(tc),
        m_view(parent),
        m_duration(0),
        m_offset(0),
        m_clickedGuide(-1),
        m_mouseMove(NO_MOVE)
{
    setFont(KGlobalSettings::toolBarFont());
    QFontMetricsF fontMetrics(font());
    LABEL_SIZE = fontMetrics.ascent() - 2;
    m_scale = 3;
    m_zoneColor = QColor(133, 255, 143);
    littleMarkDistance = FRAME_SIZE;
    mediumMarkDistance = FRAME_SIZE * m_timecode.fps();
    bigMarkDistance = FRAME_SIZE * m_timecode.fps() * 60;
    m_zoneStart = 0;
    m_zoneEnd = 100;
    m_contextMenu = new QMenu(this);
    QAction *addGuide = m_contextMenu->addAction(KIcon("document-new"), i18n("Add Guide"));
    connect(addGuide, SIGNAL(triggered()), m_view, SLOT(slotAddGuide()));
    m_editGuide = m_contextMenu->addAction(KIcon("document-properties"), i18n("Edit Guide"));
    connect(m_editGuide, SIGNAL(triggered()), this, SLOT(slotEditGuide()));
    m_deleteGuide = m_contextMenu->addAction(KIcon("edit-delete"), i18n("Delete Guide"));
    connect(m_deleteGuide , SIGNAL(triggered()), this, SLOT(slotDeleteGuide()));
    QAction *delAllGuides = m_contextMenu->addAction(KIcon("edit-delete"), i18n("Delete All Guides"));
    connect(delAllGuides, SIGNAL(triggered()), m_view, SLOT(slotDeleteAllGuides()));
    m_goMenu = m_contextMenu->addMenu(i18n("Go To"));
    connect(m_goMenu, SIGNAL(triggered(QAction *)), this, SLOT(slotGoToGuide(QAction *)));
    setMouseTracking(true);
    setMinimumHeight(20);
}

void CustomRuler::updateProjectFps(Timecode t)
{
    m_timecode = t;
    mediumMarkDistance = FRAME_SIZE * m_timecode.fps();
    bigMarkDistance = FRAME_SIZE * m_timecode.fps() * 60;
    update();
}

void CustomRuler::slotEditGuide()
{
    m_view->slotEditGuide(m_clickedGuide);
}

void CustomRuler::slotDeleteGuide()
{
    m_view->slotDeleteGuide(m_clickedGuide);
}

void CustomRuler::slotGoToGuide(QAction *act)
{
    m_view->setCursorPos(act->data().toInt(), true);
    m_view->initCursorPos(act->data().toInt());
}

void CustomRuler::setZone(QPoint p)
{
    m_zoneStart = p.x();
    m_zoneEnd = p.y();
    update();
}

void CustomRuler::mouseReleaseEvent(QMouseEvent * /*event*/)
{
    m_mouseMove = NO_MOVE;
}

// virtual
void CustomRuler::mousePressEvent(QMouseEvent * event)
{
    int pos = (int)((event->x() + offset()));
    if (event->button() == Qt::RightButton) {
        m_clickedGuide = m_view->hasGuide((int)(pos / m_factor), (int)(5 / m_factor + 1));
        m_editGuide->setEnabled(m_clickedGuide > 0);
        m_deleteGuide->setEnabled(m_clickedGuide > 0);
        m_view->buildGuidesMenu(m_goMenu);
        m_contextMenu->exec(event->globalPos());
        return;
    }
    setFocus(Qt::MouseFocusReason);
    m_view->activateMonitor();
    m_moveCursor = RULER_CURSOR;
    if (event->y() > 10) {
        if (qAbs(pos - m_zoneStart * m_factor) < 4) m_moveCursor = RULER_START;
        else if (qAbs(pos - (m_zoneStart + (m_zoneEnd - m_zoneStart) / 2) * m_factor) < 4) m_moveCursor = RULER_MIDDLE;
        else if (qAbs(pos - m_zoneEnd * m_factor) < 4) m_moveCursor = RULER_END;
    }
    if (m_moveCursor == RULER_CURSOR) {
        m_view->setCursorPos((int) pos / m_factor);
        m_clickPoint = event->pos();
        m_startRate = m_rate;
    }
}

// virtual
void CustomRuler::mouseMoveEvent(QMouseEvent * event)
{
    if (event->buttons() == Qt::LeftButton) {
        int pos = (int)((event->x() + offset()) / m_factor);
        int zoneStart = m_zoneStart;
        int zoneEnd = m_zoneEnd;
        if (pos < 0) pos = 0;
        if (m_moveCursor == RULER_CURSOR) {
            QPoint diff = event->pos() - m_clickPoint;
            if (m_mouseMove == NO_MOVE) {
                if (!KdenliveSettings::verticalzoom() || qAbs(diff.x()) >= QApplication::startDragDistance()) {
                    m_mouseMove = HORIZONTAL_MOVE;
                } else if (qAbs(diff.y()) >= QApplication::startDragDistance()) {
                    m_mouseMove = VERTICAL_MOVE;
                } else return;
            }
            if (m_mouseMove == HORIZONTAL_MOVE) {
                m_view->setCursorPos(pos);
                m_view->slotCheckPositionScrolling();
            } else {
                int verticalDiff = m_startRate - (diff.y()) / 7;
                if (verticalDiff != m_rate) emit adjustZoom(verticalDiff);
            }
            return;
        } else if (m_moveCursor == RULER_START) m_zoneStart = pos;
        else if (m_moveCursor == RULER_END) m_zoneEnd = pos;
        else if (m_moveCursor == RULER_MIDDLE) {
            int move = pos - (m_zoneStart + (m_zoneEnd - m_zoneStart) / 2);
            if (move + m_zoneStart < 0) move = - m_zoneStart;
            m_zoneStart += move;
            m_zoneEnd += move;
        }
        emit zoneMoved(m_zoneStart, m_zoneEnd);
        m_view->setDocumentModified();

        int min = qMin(m_zoneStart, zoneStart);
        int max = qMax(m_zoneEnd, zoneEnd);
        update(min * m_factor - m_offset - 2, 0, (max - min) * m_factor + 4, height());

    } else {
        int pos = (int)((event->x() + offset()));
        if (event->y() <= 10) setCursor(Qt::ArrowCursor);
        else if (qAbs(pos - m_zoneStart * m_factor) < 4) {
            setCursor(KCursor("left_side", Qt::SizeHorCursor));
            if (KdenliveSettings::frametimecode()) setToolTip(i18n("Zone start: %1", m_zoneStart));
            else setToolTip(i18n("Zone start: %1", m_timecode.getTimecodeFromFrames(m_zoneStart)));
        } else if (qAbs(pos - m_zoneEnd * m_factor) < 4) {
            setCursor(KCursor("right_side", Qt::SizeHorCursor));
            if (KdenliveSettings::frametimecode()) setToolTip(i18n("Zone end: %1", m_zoneEnd));
            else setToolTip(i18n("Zone end: %1", m_timecode.getTimecodeFromFrames(m_zoneEnd)));
        } else if (qAbs(pos - (m_zoneStart + (m_zoneEnd - m_zoneStart) / 2) * m_factor) < 4) {
            setCursor(Qt::SizeHorCursor);
            if (KdenliveSettings::frametimecode()) setToolTip(i18n("Zone duration: %1", m_zoneEnd - m_zoneStart));
            else setToolTip(i18n("Zone duration: %1", m_timecode.getTimecodeFromFrames(m_zoneEnd - m_zoneStart)));
        } else {
            setCursor(Qt::ArrowCursor);
            if (KdenliveSettings::frametimecode()) setToolTip(i18n("Position: %1", (int)(pos / m_factor)));
            else setToolTip(i18n("Position: %1", m_timecode.getTimecodeFromFrames(pos / m_factor)));
        }
    }
}


// virtual
void CustomRuler::wheelEvent(QWheelEvent * e)
{
    int delta = 1;
    if (e->modifiers() == Qt::ControlModifier) delta = m_timecode.fps();
    if (e->delta() < 0) delta = 0 - delta;
    m_view->moveCursorPos(delta);
}

int CustomRuler::inPoint() const
{
    return m_zoneStart;
}

int CustomRuler::outPoint() const
{
    return m_zoneEnd;
}

void CustomRuler::slotMoveRuler(int newPos)
{
    m_offset = newPos;
    update();
}

int CustomRuler::offset() const
{
    return m_offset;
}

void CustomRuler::slotCursorMoved(int oldpos, int newpos)
{
    if (qAbs(oldpos - newpos) * m_factor > m_textSpacing) {
        update(oldpos * m_factor - offset() - 6, 0, 17, height());
        update(newpos * m_factor - offset() - 6, 0, 17, height());
    } else update(qMin(oldpos, newpos) * m_factor - offset() - 6, 0, qAbs(oldpos - newpos) * m_factor + 17, height());
}

void CustomRuler::setPixelPerMark(int rate)
{
    int scale = comboScale[rate];
    m_rate = rate;
    m_factor = 1.0 / (double) scale * FRAME_SIZE;
    m_scale = 1.0 / (double) scale;
    double fend = m_scale * littleMarkDistance;
    if (rate > 8) {
        mediumMarkDistance = (double) FRAME_SIZE * m_timecode.fps() * 60;
        bigMarkDistance = (double) FRAME_SIZE * m_timecode.fps() * 300;
    } else if (rate > 6) {
        mediumMarkDistance = (double) FRAME_SIZE * m_timecode.fps() * 10;
        bigMarkDistance = (double) FRAME_SIZE * m_timecode.fps() * 30;
    } else if (rate > 3) {
        mediumMarkDistance = (double) FRAME_SIZE * m_timecode.fps();
        bigMarkDistance = (double) FRAME_SIZE * m_timecode.fps() * 5;
    } else {
        mediumMarkDistance = (double) FRAME_SIZE * m_timecode.fps();
        bigMarkDistance = (double) FRAME_SIZE * m_timecode.fps() * 60;
    }
    switch (rate) {
    case 0:
        m_textSpacing = fend;
        break;
    case 1:
        m_textSpacing = fend * 5;
        break;
    case 2:
    case 3:
    case 4:
        m_textSpacing = fend * m_timecode.fps();
        break;
    case 5:
        m_textSpacing = fend * m_timecode.fps() * 5;
        break;
    case 6:
        m_textSpacing = fend * m_timecode.fps() * 10;
        break;
    case 7:
        m_textSpacing = fend * m_timecode.fps() * 30;
        break;
    case 8:
    case 9:
        m_textSpacing = fend * m_timecode.fps() * 40;
        break;
    case 10:
        m_textSpacing = fend * m_timecode.fps() * 80;
        break;
    case 11:
    case 12:
        m_textSpacing = fend * m_timecode.fps() * 400;
        break;
    case 13:
        m_textSpacing = fend * m_timecode.fps() * 800;
        break;
    }
    update();
}

void CustomRuler::setDuration(int d)
{
    int oldduration = m_duration;
    m_duration = d;
    update(qMin(oldduration, m_duration) * m_factor - 1 - offset(), 0, qAbs(oldduration - m_duration) * m_factor + 2, height());
}

// virtual
void CustomRuler::paintEvent(QPaintEvent *e)
{
    QStylePainter p(this);
    p.setClipRect(e->rect());
    const int projectEnd = (int)(m_duration * m_factor);
    p.fillRect(0, 0, projectEnd - m_offset, height(), palette().alternateBase().color());

    const int zoneStart = (int)(m_zoneStart * m_factor);
    const int zoneEnd = (int)(m_zoneEnd * m_factor);
    const QRect zoneRect();

    p.fillRect(zoneStart - m_offset, height() / 2, zoneEnd - zoneStart, height() / 2, m_zoneColor);

    const int value  = m_view->cursorPos() * m_factor - m_offset;
    int minval = (e->rect().left() + m_offset) / FRAME_SIZE - 1;
    const int maxval = (e->rect().right() + m_offset) / FRAME_SIZE + 1;
    if (minval < 0)
        minval = 0;

    double f, fend;
    const int offsetmax = maxval * FRAME_SIZE;

    p.setPen(palette().text().color());

    // draw time labels
    int offsetmin = (e->rect().left() + m_offset) / m_textSpacing;
    offsetmin = offsetmin * m_textSpacing;
    for (f = offsetmin; f < offsetmax; f += m_textSpacing) {
        QString lab;
        if (KdenliveSettings::frametimecode())
            lab = QString::number((int)(f / m_factor + 0.5));
        else
            lab = m_timecode.getTimecodeFromFrames((int)(f / m_factor + 0.5));
        p.drawText(f - m_offset + 2, LABEL_SIZE, lab);
    }

    offsetmin = (e->rect().left() + m_offset) / littleMarkDistance;
    offsetmin = offsetmin * littleMarkDistance;
    // draw the little marks
    fend = m_scale * littleMarkDistance;
    if (fend > 5) {
        for (f = offsetmin - m_offset; f < offsetmax - m_offset; f += fend)
            p.drawLine((int)f, LITTLE_MARK_X1, (int)f, LITTLE_MARK_X2);
    }

    offsetmin = (e->rect().left() + m_offset) / mediumMarkDistance;
    offsetmin = offsetmin * mediumMarkDistance;
    // draw medium marks
    fend = m_scale * mediumMarkDistance;
    if (fend > 5) {
        for (f = offsetmin - m_offset - fend; f < offsetmax - m_offset + fend; f += fend)
            p.drawLine((int)f, MIDDLE_MARK_X1, (int)f, MIDDLE_MARK_X2);
    }

    offsetmin = (e->rect().left() + m_offset) / bigMarkDistance;
    offsetmin = offsetmin * bigMarkDistance;
    // draw big marks
    fend = m_scale * bigMarkDistance;
    if (fend > 5) {
        for (f = offsetmin - m_offset; f < offsetmax - m_offset; f += fend)
            p.drawLine((int)f, BIG_MARK_X1, (int)f, BIG_MARK_X2);
    }

    // draw zone cursors
    int off = offset();
    if (zoneStart > 0) {
        QPolygon pa(4);
        pa.setPoints(4, zoneStart - off + 3, 9, zoneStart - off, 9, zoneStart - off, 18, zoneStart - off + 3, 18);
        p.drawPolyline(pa);
    }

    if (zoneEnd > 0) {
        QColor center(Qt::white);
        center.setAlpha(150);
        QRect rec(zoneStart - off + (zoneEnd - zoneStart) / 2 - 4, 9, 8, 9);
        p.fillRect(rec, center);
        p.drawRect(rec);

        QPolygon pa(4);
        pa.setPoints(4, zoneEnd - off - 3, 9, zoneEnd - off, 9, zoneEnd - off, 18, zoneEnd - off - 3, 18);
        p.drawPolyline(pa);
    }

    // draw pointer
    QPolygon pa(3);
    pa.setPoints(3, value - 6, 8, value + 6, 8, value, 16);
    p.setBrush(palette().highlight());
    p.drawPolygon(pa);
}

#include "customruler.moc"
