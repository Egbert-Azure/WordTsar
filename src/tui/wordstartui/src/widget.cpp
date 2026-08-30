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

#include "widget.h"

namespace wordstartui
{

/////////////////////////////////////////////////////////////////////////////
///
/// @class cWidget
///
/// @brief
/// Base class for all simple WordStar TUI widgets.
///
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Construct a widget with empty bounds.
///
/////////////////////////////////////////////////////////////////////////////
cWidget::cWidget(void)
{
    mBounds.row = 0;
    mBounds.col = 0;
    mBounds.rows = 0;
    mBounds.cols = 0;
    mHasFocus = false;
    mEnabled = true;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Destroy a widget.
///
/////////////////////////////////////////////////////////////////////////////
cWidget::~cWidget(void)
{
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return true when the widget can receive focus
///
/// @brief
/// Report whether this widget is focusable.
///
/////////////////////////////////////////////////////////////////////////////
bool cWidget::CanFocus(void) const
{
    return false;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  const sRect& bounds [in] widget bounds
///
/// @return nothing
///
/// @brief
/// Set widget bounds.
///
/////////////////////////////////////////////////////////////////////////////
void cWidget::SetBounds(const sRect& bounds)
{
    mBounds = bounds;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return widget bounds
///
/// @brief
/// Get widget bounds.
///
/////////////////////////////////////////////////////////////////////////////
sRect cWidget::GetBounds(void) const
{
    return mBounds;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  bool focus [in] focus state
///
/// @return nothing
///
/// @brief
/// Set widget focus state.
///
/////////////////////////////////////////////////////////////////////////////
void cWidget::SetFocus(bool focus)
{
    mHasFocus = focus;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return true when the widget has focus
///
/// @brief
/// Return the widget focus state.
///
/////////////////////////////////////////////////////////////////////////////
bool cWidget::HasFocus(void) const
{
    return mHasFocus;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  bool enabled [in] enabled state
///
/// @return nothing
///
/// @brief
/// Enable or disable the widget.
///
/////////////////////////////////////////////////////////////////////////////
void cWidget::SetEnabled(bool enabled)
{
    mEnabled = enabled;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return true when the widget is enabled
///
/// @brief
/// Return the enabled state.
///
/////////////////////////////////////////////////////////////////////////////
bool cWidget::IsEnabled(void) const
{
    return mEnabled;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  int row [in] screen row
/// @param  int col [in] screen column
///
/// @return true when the position is inside the widget's event area
///
/// @brief
/// Default mouse hit test: the widget bounds. Overlay widgets override this.
///
/////////////////////////////////////////////////////////////////////////////
bool cWidget::ContainsEventPoint(int row, int col) const
{
    return ContainsPoint(row, col);
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return true when the widget is showing an overlay past its bounds
///
/// @brief
/// Default: widgets do not draw overlays. Overlay widgets override this.
///
/////////////////////////////////////////////////////////////////////////////
bool cWidget::HasOpenOverlay(void) const
{
    return false;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  int row [in] screen row
/// @param  int col [in] screen column
///
/// @return true when the position is inside the widget bounds
///
/// @brief
/// Test whether a screen position is inside this widget.
///
/////////////////////////////////////////////////////////////////////////////
bool cWidget::ContainsPoint(int row, int col) const
{
    if (row < mBounds.row)
    {
        return false;
    }

    if (row >= (mBounds.row + mBounds.rows))
    {
        return false;
    }

    if (col < mBounds.col)
    {
        return false;
    }

    if (col >= (mBounds.col + mBounds.cols))
    {
        return false;
    }

    return true;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  const sInputEvent& event [in] input event
///
/// @return true when the event is a mouse event inside this widget
///
/// @brief
/// Test whether a mouse event occurred inside this widget.
///
/////////////////////////////////////////////////////////////////////////////
bool cWidget::IsMouseInside(const sInputEvent& event) const
{
    if (event.type != INPUT_TYPE_MOUSE)
    {
        return false;
    }

    return ContainsPoint(event.mouseRow, event.mouseCol);
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  const sInputEvent& event [in] input event
///
/// @return true when the event is a left mouse press inside this widget
///
/// @brief
/// Test for the common widget activation mouse gesture.
///
/////////////////////////////////////////////////////////////////////////////
bool cWidget::IsLeftMousePressInside(const sInputEvent& event) const
{
    if (IsMouseInside(event) == false)
    {
        return false;
    }

    if (event.mouseAction != MOUSE_ACTION_PRESS)
    {
        return false;
    }

    if (event.mouseButton != MOUSE_BUTTON_LEFT)
    {
        return false;
    }

    return true;
}

}
