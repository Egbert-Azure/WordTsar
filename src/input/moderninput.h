#ifndef MODERNINPUT_H
#define MODERNINPUT_H

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

#include "src/core/include/config.h"
#include "src/input/inputhandler.h"

// Forward declaration
class cEditorBase;

/// @ingroup Input
/// @{


/////////////////////////////////////////////////////////////////////////////
///
/// @class cModernInput
///
/// @brief
/// Modern/CUA keyboard input handler implementation.
/// Implements standard CUA-style keyboard shortcuts (Ctrl+C/V/X/Z, etc.)
/// with WordStar-compatible Alt prefix chords (Alt+K/Q/O/P) for advanced
/// operations not covered by the standard CUA binding set.
///
/// Direct Ctrl+letter bindings handle common operations:
///   Ctrl+Z=Undo, Ctrl+Y=Redo, Ctrl+X=Cut, Ctrl+C=Copy, Ctrl+V=Paste,
///   Ctrl+S=Save, Ctrl+O=Open, Ctrl+F=Find, Ctrl+H=Replace, etc.
///
/// Alt prefix chords provide access to all WordStar operations:
///   Alt+K (block/marker), Alt+Q (navigation), Alt+O (onscreen),
///   Alt+P (style/print). These work on both GUI (Qt::AltModifier) and
///   TUI (ESC prefix sequences).
///
/////////////////////////////////////////////////////////////////////////////
class cModernInput : public IInputHandler
{
    // =================================================================
    // METHODS
    // =================================================================
public:
    cModernInput(cEditorBase *editor) ;
    ~cModernInput(void) override ;

    bool CheckControlMode(void) override ;
    bool HandleKey(char ch, bool shift = false, bool alt = false) override ;
    bool HandleSpecialKey(eSpecialKey key, bool shift = false,
                          bool ctrl = false, bool alt = false) override ;
    eHelpDisplay GetHelpStatus(void) override ;

private:
    void OnAltKChar(char key) ;
    void OnAltQChar(char key) ;
    void OnAltOChar(char key) ;
    void OnAltPChar(char key) ;

    // =================================================================
    // MEMBER VARIABLES
    // =================================================================
private:
    cEditorBase *mEditor ;

    bool mAltKMode ;                        ///< waiting for follow-up after Alt+K
    bool mAltQMode ;                        ///< waiting for follow-up after Alt+Q
    bool mAltOMode ;                        ///< waiting for follow-up after Alt+O
    bool mAltPMode ;                        ///< waiting for follow-up after Alt+P

} ;

/// @}

#endif  // MODERNINPUT_H
