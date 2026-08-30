#ifndef _CSPELLCHECK_H_
#define _CSPELLCHECK_H_

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

#include <QDialog>
#include <QKeyEvent>

#include "ui_spellcheck.h"

#include "src/gui/editor/editorctrl.h"
#include "src/core/spellcheck/spellcheck.h"

const int SPELLIGNORE = 100 ;
const int SPELLADD = 200 ;
const int SPELLENTER = 300 ;
const int SPELLMORE = 400 ;


/////////////////////////////////////////////////////////////////////////////
///
/// @enum eSpellMode
///
/// @brief
/// Spell check operation modes.
/// Determines whether to check the entire document, a single word,
/// or enter a word manually via the spell check dialog.
///
/////////////////////////////////////////////////////////////////////////////
enum eSpellMode
{
    SPELLCHECKDOCUMENT,
    SPELLCHECKWORD,
    SPELLENTERWORD
} ;


class cSpellCheck: public QDialog
{
    Q_OBJECT

    // =================================================================
    // METHODS
    // =================================================================

public:
    cSpellCheck(cEditorCtrl *parent, eSpellMode mode) ;
    ~cSpellCheck(void) ;

    void CheckDocument(void) ;
    void CheckWord(void) ;
    void CheckEnteredWord(void) ;

public slots:
    void ignorebuttonclicked(void) ;
    void addbuttonclicked(void) ;
    void enterbuttonclicked(void) ;
    void morebuttonclicked(void) ;
    void cancelbuttonclicked(void) ;
    void suggestiondoubleclicked(QListWidgetItem *item) ;

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void showEvent(QShowEvent *e) ;

private:
    bool PrepareText(PARAGRAPH_T para, std::string &text, std::vector<POSITION_T> &wordstarts, std::vector<POSITION_T> &modwordstarts, POSITION_T &start) ;
    void AddWord(std::string word) ;
    void EnterWord(std::string &word) ;

    int trimandcheck(std::string &s) ;
    void ShowSingleWordDialog(const std::string &word, POSITION_T docWordStart, POSITION_T docWordEnd) ;

    // =================================================================
    // MEMBER VARIABLES
    // =================================================================

private:
    QByteArray mGeometry ;                  // remmeber where the dialog was

    cEditorCtrl *mEditor ;
    eSpellMode mMode ;
    bool mSpellCheckDotCommands ;       // spell check text content of dot commands
    Ui::SpellCheck spellcheck ;

    cSpellChecker spelling ;

} ;




#endif
