#ifndef MACROSCRIPT_H
#define MACROSCRIPT_H

//////////////////////////////////////////////////////////////////////////////
//
// WordTsar - Wordstar clone for modern systems http://wordtsar.ca
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

#include <string>
#include <utility>
#include <vector>

#include "src/input/keystroke.h"

/// @ingroup Input
/// @{


enum eMacroStatement
{
    MACRO_KEYS,                 ///< Key("..."): keys
    MACRO_PAUSE_INPUT,          ///< PauseForInput: keys holds the end key, text the message
    MACRO_PAUSE_KEY,            ///< PauseForKey()
    MACRO_GOTO,                 ///< Goto label: target
    MACRO_CALL,                 ///< Call sub: target
    MACRO_RETURN,               ///< Return
    MACRO_END_SUB,              ///< End Sub, End Main
    MACRO_STOP,                 ///< Stop
    MACRO_PLAY_MACRO,           ///< PlayMacro 'name': text
    MACRO_CHAIN_MACRO,          ///< ChainMacro 'name': text
    MACRO_SETTING,              ///< setting, value
    MACRO_IF_EXCEPTION,         ///< IfException: target is End IfException, clauses
    MACRO_CLAUSE,               ///< ACK:, QRY:, NCM:, DEF:: text, target is End IfException
    MACRO_END_IF_EXCEPTION      ///< End IfException
};


enum eMacroSetting
{
    MACRO_SETTING_AUTO_RESTORE,
    MACRO_SETTING_INSERT,
    MACRO_SETTING_CMD_TAGS,
    MACRO_SETTING_HIDE_DOTS,
    MACRO_SETTING_HELP_LEVEL,   ///< WordStar 7 level 0-4
    MACRO_SETTING_IGNORED       ///< ColMode, ColReplace, WSQuiet
};


enum eMacroException
{
    MACRO_EXCEPTION_ACK,        ///< a message to acknowledge
    MACRO_EXCEPTION_QRY,        ///< a question
    MACRO_EXCEPTION_NCM         ///< nothing typed at PauseForInput
};


struct sMacroStatement
{
    eMacroStatement kind = MACRO_KEYS;
    size_t line = 0;                                        ///< source line, from 1
    std::vector<sKeyStroke> keys;
    std::string text;
    eMacroSetting setting = MACRO_SETTING_IGNORED;
    int value = 0;
    size_t target = 0;
    std::vector<std::pair<std::string, size_t>> clauses;   ///< MACRO_IF_EXCEPTION: "ACK", "QRY", "NCM", "DEF" and their first statement
};


/////////////////////////////////////////////////////////////////////////////
///
/// @class cMacroDialog
///
/// @brief
/// The Find or Find and Replace dialog answered by played keys. Fields Find,
/// (Replace,) Options. Enter goes to the next field and in Options closes
/// the dialog; Tab goes to the next field, Shift+Tab back; F10 and ^K close
/// it; Esc cancels. ^Y clears the field, ^N types a hard return, ^P and a
/// key types that control code, Backspace deletes. Alt+T goes to Options,
/// another Alt+letter turns that option letter on or off.
///
/////////////////////////////////////////////////////////////////////////////
class cMacroDialog
{
public:
    enum eResult { DIALOG_OPEN, DIALOG_OK, DIALOG_CANCEL };

    explicit cMacroDialog(bool replace);
    eResult Feed(const sKeyStroke& key);

    const std::string& Text(void) const;
    const std::string& Replacement(void) const;
    const std::string& Options(void) const;

private:
    std::string& Field(void);

    bool mReplace;
    size_t mField;                  // 0 Find, 1 Replace, last Options
    bool mControlP;                 // ^P typed, waiting for the control key
    std::string mText;
    std::string mReplacement;
    std::string mOptions;
};


/////////////////////////////////////////////////////////////////////////////
///
/// @class cMacroScript
///
/// @brief
/// A WordStar 7 macro (.wsm) parsed and checked as a whole: the statements
/// of all subroutines, labels and IfException clauses resolved. Parse
/// refuses the first statement WordTsar does not support; CheckKeys follows
/// the keys through the WordStar command tables and refuses the first key
/// that reaches a command WordTsar does not have. Keys typed into the Find
/// and Find and Replace dialogs and the key that answers an exception are
/// not commands.
///
/////////////////////////////////////////////////////////////////////////////
class cMacroScript
{
public:
    bool Parse(const std::vector<std::string>& lines);       // lines before the first 0x1A
    bool CheckKeys(void);                                    // WordStar macros; Modern ones are not checked
    const std::string& Error(void) const;                    // "Not supported in WordTsar: <command>, line <n>"

    const std::vector<sMacroStatement>& Statements(void) const;
    size_t Main(void) const;                                 // first statement of Sub MAIN
    bool IsModern(void) const;                               // REM Modern after Sub MAIN

    static std::string MacroName(const std::string& reference);   // PlayMacro name without folder and .wsm

    // Keys at the start of a clause that answer the exception: one for a
    // message (ACK, DEF); for a question (QRY) those up to and including the
    // first Enter, F10, ^K or Esc, or one if there is none.
    static size_t AnswerLength(const std::vector<sKeyStroke>& keys, eMacroException kind);

private:
    bool Refuse(const std::string& command, size_t line);

    std::vector<sMacroStatement> mStatements;
    size_t mMain = 0;
    bool mModern = false;
    std::string mError;
};

/// @}

#endif // MACROSCRIPT_H
