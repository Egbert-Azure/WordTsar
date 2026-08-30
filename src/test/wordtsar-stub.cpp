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

#include "src/gui/wordtsar.h"

/////////////////////////////////////////////////////////////////////////////
///
/// @param  QString text [in] - Status message text
/// @param  bool progress [in] - Show progress indicator (default: false)
/// @param  int percent [in] - Progress percentage 0-100 (default: 0)
///
/// @return nothing
///
/// @brief
/// Stub implementation of cWordTsar::SetStatus() for test environment.
/// Does nothing - tests don't have a main window with status bar.
///
/// This stub is only linked into the test executable (WSTest) to satisfy
/// linker requirements when cEditorCtrl's progress methods are compiled.
/// The real implementation is in src/gui/wordtsar.cpp (not included in tests).
///
/////////////////////////////////////////////////////////////////////////////
void cWordTsar::SetStatus(QString text, bool progress, int percent)
{
    // Empty stub - no status bar in test environment
    (void)text;
    (void)progress;
    (void)percent;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Stub implementation of cWordTsar::ApplyDisplaySettings() for test
/// environment. Does nothing - tests don't have a main window with
/// chrome widgets.
///
/////////////////////////////////////////////////////////////////////////////
void cWordTsar::ApplyDisplaySettings(void)
{
    // Empty stub - no chrome widgets in test environment
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Stub implementation of cWordTsar::ApplyScreenColors() for test
/// environment. Does nothing - tests don't have screen chrome.
///
/////////////////////////////////////////////////////////////////////////////
void cWordTsar::ApplyScreenColors(void)
{
    // Empty stub - no screen chrome in test environment
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Stub implementation of cWordTsar::ToggleRevealCodes() for test
/// environment. Does nothing - tests don't have a main window with
/// splitter and reveal codes pane.
///
/////////////////////////////////////////////////////////////////////////////
void cWordTsar::ToggleRevealCodes(void)
{
    // Empty stub - no reveal codes pane in test environment
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Stub implementation of cWordTsar::UpdateCommandTagsLabel() for test
/// environment. Does nothing - tests don't have a View menu.
///
/////////////////////////////////////////////////////////////////////////////
void cWordTsar::UpdateCommandTagsLabel(eDisplayMode /*mode*/)
{
    // Empty stub - no View menu in test environment
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  mode [in] input mode to switch to
///
/// @return nothing
///
/// @brief
/// Stub implementation of cWordTsar::OnInputModeChanged() for test
/// environment. Does nothing - tests don't have menus to rebuild.
///
/////////////////////////////////////////////////////////////////////////////
void cWordTsar::OnInputModeChanged(eInputMode /*mode*/)
{
    // Empty stub - no menu provider swap in test environment
}
