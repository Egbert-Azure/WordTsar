#ifndef WORDSTARINPUT_H
#define WORDSTARINPUT_H

//////////////////////////////////////////////////////////////////////////////
//
// WordTsar - Wordstar clone for modern systems http://wordtsar.ca
// Copyright (C) 2018 Gerald Brandt
// Copyright (C) 2026 Egbert H. Schroeer
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

// Forward declaration - avoid circular dependency
class cEditorBase;

/// @ingroup Keyboard
/// @{

enum eSpecialChars
{
    CTRL_A = 1,
    CTRL_B,
    CTRL_C,
    CTRL_D,
    CTRL_E,
    CTRL_F,
    CTRL_G,
    CTRL_H,
    CTRL_I,
    CTRL_J,
    CTRL_K,
    CTRL_L,
    CTRL_M,
    CTRL_N,
    CTRL_O,
    CTRL_P,
    CTRL_Q,
    CTRL_R,
    CTRL_S,
    CTRL_T,
    CTRL_U,
    CTRL_V,
    CTRL_W,
    CTRL_X,
    CTRL_Y,
    CTRL_Z,
    ESCAPE = 27,
} ;


/////////////////////////////////////////////////////////////////////////////
///
/// @enum eWordStarCommand
///
/// @brief
/// What a key does in WordStar mode, from the command tables in
/// wordtsarinput.cpp. The key handlers report WSCMD_NOT_IMPLEMENTED and
/// WSCMD_INVALID keys from the same tables.
///
/////////////////////////////////////////////////////////////////////////////
enum eWordStarCommand
{
    WSCMD_RUNS,                 ///< runs without asking for input
    WSCMD_PREFIX,               ///< chord prefix, waits for the next key
    WSCMD_FIND,                 ///< Find prompt (^QF)
    WSCMD_REPLACE,              ///< Find and Replace prompt (^QA)
    WSCMD_FILE,                 ///< file name prompt a macro answers (^KR)
    WSCMD_SUBKEY,               ///< waits for one more key (^KZ)
    WSCMD_DIALOG,               ///< opens a dialog a macro cannot answer
    WSCMD_NOT_IMPLEMENTED,      ///< "not implemented" message
    WSCMD_INVALID               ///< no command: "invalid command" or not handled
};

const char WSCMD_CONTROL = 0;   ///< WordStarCommand prefix: a control key (1-31)
const char WSCMD_ALT = 1;       ///< WordStarCommand prefix: an Alt key

// Command of key ch after prefix: WSCMD_CONTROL, WSCMD_ALT or a chord
// prefix 'k', 'q', 'o', 'p', 'm'. Chord keys are lower case, ^X as 'x'.
eWordStarCommand WordStarCommand(char prefix, char ch);

// Command of a named key (no chord pending).
eWordStarCommand WordStarSpecialCommand(eSpecialKey key);



class cWordStarInput : public IInputHandler
{
    // =================================================================
    // METHODS
    // =================================================================
public:
    cWordStarInput(cEditorBase *editor) ;
    ~cWordStarInput(void) override ;

    bool CheckControlMode(void) override ;
    bool HandleKey(char ch, bool shift = false, bool alt = false) override ;
    bool HandleSpecialKey(eSpecialKey key, bool shift = false,
                          bool ctrl = false, bool alt = false) override ;
    eHelpDisplay GetHelpStatus(void) override ;

//private:
    void OnControlMChar(char key) ;
    void OnControlOChar(char key) ;
    void OnControlPChar(char key) ;
    bool OnControlKChar(char key) ;
    void OnControlQChar(char key) ;
    void OnControlKZChar(char key) ;
    void OnHelpTargetChar(char key) ;
    void OnHelpChordTargetChar(char key) ;
    bool ChordAvailable(char prefix, char ch) ;   // reports a not implemented or invalid chord key

    // =================================================================
    // MEMBER VARIABLES
    // =================================================================
private:
    cEditorBase *mEditor ;

    bool mControlMMode ;
    bool mControlKMode ;                    ///> modes for key sequences
    bool mControlQMode ;
    bool mControlPMode ;
    bool mControlOMode ;
    bool mControlKZMode ;                   ///> after ^K,Z, waiting for A (ascending) or D (descending)
    bool mWaitingForHelpTarget ;             ///> true after F1, before the command key it describes
    bool mWaitingForHelpChordTarget ;        ///> true after F1,<chord-prefix>, before the sub-command key
    char mHelpChordPrefix ;                  ///> which chord (k/q/o/p/m) F1's chord-prefix key selected

    eHelpDisplay mOldHelpStatus ;

} ;
    
    
/// @}

#endif  // WORDSTARINPUT_H
