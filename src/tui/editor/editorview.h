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

#ifndef WORDTSAR_WSTUI_EDITORVIEW_H
#define WORDTSAR_WSTUI_EDITORVIEW_H

#include "src/tui/wordstartui/src/screen.h"
#include "src/tui/wordstartui/src/tuidefs.h"

#include "src/core/include/config.h"   // LINE_T, POSITION_T

class cWSEditorCtrl;
struct sLineLayout;

/////////////////////////////////////////////////////////////////////////////
///
/// @class cWSEditorView
///
/// @brief
/// Renders the shared layout engine's visible lines into a wordstartui cScreen
/// (24-bit RGB cells) and forwards keystrokes to the editor's shared input
/// handler. The shared layout engine produces the lines; this is
/// the wordstartui rendering of it.
///
/////////////////////////////////////////////////////////////////////////////
class cWSEditorView
{
    // =================================================================
    // METHODS
    // =================================================================

public:
    cWSEditorView(cWSEditorCtrl* editor);

    void SetBounds(const wordstartui::sRect& bounds);
    void SetShowScrollbar(bool show);
    void Draw(wordstartui::cScreen& screen);
    bool HandleEvent(const wordstartui::sInputEvent& event);
    void EnsureCaretVisible(void);

    int GetCaretRow(void) const;
    int GetCaretCol(void) const;
    bool CaretVisible(void) const;
    LINE_T GetFirstLine(void) const;

    // True when the last handled event scrolled via the scrollbar (so the host
    // should not re-center the viewport on the caret).
    bool DidScrollByBar(void) const;

private:
    wordstartui::sStyle StyleFor(const wordstartui::sColor& fg,
                                 const wordstartui::sColor& bg,
                                 uint32_t attrs) const;
    void ApplySegmentFormatting(const std::string& fontDescriptor,
                                bool superscript, bool subscript,
                                wordstartui::sColor& fg, wordstartui::sColor& bg,
                                uint32_t& attrs) const;
    int GraphemeColumns(POSITION_T docPos, const std::string& grapheme, PAGE_T pagenumber) const;
    int LineScreenRows(LINE_T rawLine) const;
    int LineStartCol(const sLineLayout* line, COORD_T twipsPerCol) const;
    int LineEndCol(const sLineLayout* line, COORD_T twipsPerCol) const;
    LINE_T RawLineAtScreenRow(int screenRow) const;
    POSITION_T PositionInLine(const sLineLayout* line, int targetScreenCol) const;
    int LogicalColumnAt(const sLineLayout* line, POSITION_T position) const;
    int TextWidthCols(void) const;

    // =================================================================
    // MEMBER VARIABLES
    // =================================================================

private:
    cWSEditorCtrl* mEditor;   // not owned
    wordstartui::sRect mBounds;
    LINE_T mFirstLine;
    int mHorizScroll;         // leftmost visible logical column (horizontal scroll)
    bool mShowScrollbar;      // draw the scrollbar column (Display > Show scroll bar)
    int mCaretRow;
    int mCaretCol;
    bool mCaretVisible;
    bool mScrolledByBar;
};

#endif // WORDTSAR_WSTUI_EDITORVIEW_H
