//////////////////////////////////////////////////////////////////////////////
//
// WordTsar - Wordstar clone for modern systems http://wordtsar.ca
// Copyright (C) 2018 Gerald Brandt
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as
// published by the Free Software Foundation, either version 3 of the
// License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//////////////////////////////////////////////////////////////////////////////

/**
 * @class cOverlayWidget
 *
 * @brief Transparent overlay widget that stays on top of its parent.
 *
 * Implements the cOverlayWidget class, a QWidget that installs itself as
 * an event filter on its parent so it always resizes to match the parent
 * and stays raised above sibling widgets.
 *
 * @section overlay_behavior Overlay Behavior
 * - Installs an event filter on the parent widget to intercept resize events
 * - Automatically resizes to match the parent's geometry when the parent resizes
 * - Calls raise() to ensure it stays on top of all sibling widgets
 * - Configured with Qt::WA_NoSystemBackground and Qt::WA_TransparentForMouseEvents
 *   so it doesn't intercept mouse clicks or paint a background
 *
 * @section overlay_usage Usage Context
 * Suitable for drawing non-interactive visual overlays (e.g., page boundary
 * guides, margin indicators, selection highlights) on top of the editor
 * surface without interfering with mouse-based text editing.
 *
 * @author Gerald Brandt
 * @copyright GNU Affero General Public License v3.0
 *
 * @see cOverlayWidget Overlay widget class
 * @see cEditorCtrl Editor widget that may host overlays
 */

#include "overlaywidget.h"

void cOverlayWidget::newParent(void)
{
    if (!parent) return;
    parent->installEventFilter(this);
    raise();
}


cOverlayWidget::cOverlayWidget(QWidget * inparent) : QWidget{inparent}
{
    parent = inparent ;
    setAttribute(Qt::WA_NoSystemBackground);
    setAttribute(Qt::WA_TransparentForMouseEvents);
    newParent();
}

//! Catches resize and child events from the parent widget
bool cOverlayWidget::eventFilter(QObject * obj, QEvent * ev)
{
    if (obj == parent)
    {
        if (ev->type() == QEvent::Resize)
        {
            resize(static_cast<QResizeEvent*>(ev)->size());
        }
        else if (ev->type() == QEvent::ChildAdded)
        {
            raise();
        }
    }
    return QWidget::eventFilter(obj, ev);
}

//! Tracks parent widget changes
bool cOverlayWidget::event(QEvent* ev)
{
    if (ev->type() == QEvent::ParentAboutToChange)
    {
        if (parent)
        {
            parent->removeEventFilter(this);
        }
    }
    else if (ev->type() == QEvent::ParentChange)
    {
        newParent();
    }
    return QWidget::event(ev);
}
