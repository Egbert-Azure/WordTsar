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

#include "editorview.h"
#include "editorctrl.h"

#include "src/tui/wordstartui/src/scrollbar.h"
#include "src/tui/wordstartui/src/utf8helper.h"
#include "src/core/layout/layoutbase.h"
#include "src/core/layout/layoutstructs.h"
#include "src/core/document/document.h"
#include "src/input/inputhandler.h"
#include "src/core/include/config.h"

#include <string>
#include <vector>
#include <cmath>

using wordstartui::sCell;
using wordstartui::sColor;
using wordstartui::sInputEvent;
using wordstartui::sRect;
using wordstartui::sStyle;

namespace
{

// Control-code markers stored in the document buffer (see config.h).
const unsigned char MARKER = 127;   // MARKER_CHAR
const unsigned char REPLACE = 0;    // REPLACE_CHAR
const unsigned char SAVE = 1;       // SAVE_CHAR
const unsigned char HARDCR = 13;    // HARD_RETURN
const unsigned char EOFCH = 26;     // ^Z

}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  cWSEditorCtrl* editor [in] the editor control (not owned)
///
/// @return nothing
///
/// @brief
/// Construct the editor view over an editor control.
///
/////////////////////////////////////////////////////////////////////////////
cWSEditorView::cWSEditorView(cWSEditorCtrl* editor)
{
    mEditor = editor;
    mBounds.row = 0;
    mBounds.col = 0;
    mBounds.rows = 1;
    mBounds.cols = 1;
    mFirstLine = 0;
    mHorizScroll = 0;
    mShowScrollbar = true;
    mCaretRow = 0;
    mCaretCol = 0;
    mCaretVisible = false;
    mScrolledByBar = false;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  const sRect& bounds [in] editor area on screen
///
/// @return nothing
///
/// @brief
/// Set the editor's screen area and inform the engine of the viewport size.
///
/////////////////////////////////////////////////////////////////////////////
void cWSEditorView::SetBounds(const sRect& bounds)
{
    mBounds = bounds;
    mEditor->SetViewport(mFirstLine, mBounds.rows);
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  bool show [in] true to draw the scrollbar column
///
/// @return nothing
///
/// @brief
/// Show or hide the scrollbar column (Display > Show scroll bar). When hidden,
/// the reclaimed column is used for text.
///
/////////////////////////////////////////////////////////////////////////////
void cWSEditorView::SetShowScrollbar(bool show)
{
    mShowScrollbar = show;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return the first visible raw line
///
/// @brief
/// The raw line number shown at the top of the viewport.
///
/////////////////////////////////////////////////////////////////////////////
LINE_T cWSEditorView::GetFirstLine(void) const
{
    return mFirstLine;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return true when the last handled event scrolled via the scrollbar
///
/// @brief
/// Lets the host skip caret re-centering after a scrollbar drag.
///
/////////////////////////////////////////////////////////////////////////////
bool cWSEditorView::DidScrollByBar(void) const
{
    return mScrolledByBar;
}

int cWSEditorView::GetCaretRow(void) const
{
    return mCaretRow;
}

int cWSEditorView::GetCaretCol(void) const
{
    return mCaretCol;
}

bool cWSEditorView::CaretVisible(void) const
{
    return mCaretVisible;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  const sColor& fg / bg [in] cell colors
/// @param  uint32_t attrs [in] cell attributes
///
/// @return an sStyle bundling the arguments
///
/// @brief
/// Build a cell style from color + attribute pieces.
///
/////////////////////////////////////////////////////////////////////////////
sStyle cWSEditorView::StyleFor(const sColor& fg, const sColor& bg, uint32_t attrs) const
{
    sStyle style;

    style.fg = fg;
    style.bg = bg;
    style.attrs = attrs;

    return style;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  const std::string& fontDescriptor [in] "Font|Size|Bold|Italic|Underline|..."
/// @param  bool superscript [in] segment is superscripted
/// @param  bool subscript [in] segment is subscripted
/// @param  sColor& fg [in,out] foreground (overridden for unsupported attrs)
/// @param  sColor& bg [in,out] background (overridden for unsupported attrs)
/// @param  uint32_t& attrs [in,out] cell attribute bitmask
///
/// @return nothing
///
/// @brief
/// Turn a segment's bold/italic/underline (from the font descriptor) and
/// super/subscript flags into native cell attributes when the terminal supports
/// them, otherwise into substitute colors.
///
/////////////////////////////////////////////////////////////////////////////
void cWSEditorView::ApplySegmentFormatting(const std::string& fontDescriptor,
                                           bool superscript, bool subscript,
                                           sColor& fg, sColor& bg, uint32_t& attrs) const
{
    // Descriptor fields: Font|Size|Bold|Italic|Underline|Superscript|Subscript.
    bool bold = false;
    bool italic = false;
    bool underline = false;

    int field = 0;
    std::string value;

    for (size_t index = 0; index <= fontDescriptor.size(); ++index)
    {
        if ((index == fontDescriptor.size()) || (fontDescriptor[index] == '|'))
        {
            if (field == 2)
            {
                bold = (value == "1");
            }
            else if (field == 3)
            {
                italic = (value == "1");
            }
            else if (field == 4)
            {
                underline = (value == "1");
            }

            field++;
            value.clear();
        }
        else
        {
            value.push_back(fontDescriptor[index]);
        }
    }

    if (bold == true)
    {
        if (mEditor->TermSupportsBold() == true)
        {
            attrs = attrs | wordstartui::CELL_ATTR_BOLD;
        }
        else
        {
            fg = mEditor->mBoldColour;
            bg = mEditor->mBoldBgColour;
        }
    }

    if (italic == true)
    {
        if (mEditor->TermSupportsItalic() == true)
        {
            attrs = attrs | wordstartui::CELL_ATTR_ITALIC;
        }
        else
        {
            fg = mEditor->mItalicColour;
            bg = mEditor->mItalicBgColour;
        }
    }

    if (underline == true)
    {
        if (mEditor->TermSupportsUnderline() == true)
        {
            attrs = attrs | wordstartui::CELL_ATTR_UNDERLINE;
        }
        else
        {
            fg = mEditor->mUnderlineColour;
            bg = mEditor->mUnderlineBgColour;
        }
    }

    // Terminals cannot raise/lower glyphs, so super/subscript always use color.
    if (superscript == true)
    {
        fg = mEditor->mSuperscriptColour;
        bg = mEditor->mSuperscriptBgColour;
    }

    if (subscript == true)
    {
        fg = mEditor->mSubscriptColour;
        bg = mEditor->mSubscriptBgColour;
    }
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  LINE_T rawLine [in] raw line number
///
/// @return the number of screen rows the line occupies (line spacing)
///
/// @brief
/// A single-spaced text line takes one row; a line whose paragraph carries a
/// .ls multiplier takes round(linespace) rows (at least 1). Dot-command and
/// comment lines always take one row.
///
/////////////////////////////////////////////////////////////////////////////
int cWSEditorView::LineScreenRows(LINE_T rawLine) const
{
    cLayoutBase* layout = mEditor->GetLayout();
    if (layout == nullptr)
    {
        return 1;
    }

    const sLineLayout* line = layout->GetLineByRawLineNumber(rawLine);
    if ((line == nullptr) || (line->segments.empty() == true))
    {
        return 1;
    }

    const sParagraphLayout* para = layout->GetParagraphLayout(line->segments.front().paragraph);
    if (para == nullptr)
    {
        return 1;
    }

    if ((para->isCommand == true) || (para->isComment == true))
    {
        return 1;
    }

    double ls = para->endState.linespace;
    if (ls < 1.0)
    {
        return 1;
    }

    int rows = static_cast<int>(std::round(ls));
    if (rows < 1)
    {
        rows = 1;
    }

    return rows;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  const sLineLayout* line [in] line to measure
/// @param  COORD_T twipsPerCol [in] monospace column width in twips
///
/// @return the starting screen column (relative to the editor left edge)
///
/// @brief
/// The line's leading indent plus any centre/right alignment offset, in
/// monospace columns. Shared by Draw() and the click hit-test so the two agree
/// on where a line's text begins.
///
/////////////////////////////////////////////////////////////////////////////
int cWSEditorView::LineStartCol(const sLineLayout* line, COORD_T twipsPerCol) const
{
    if ((line == nullptr) || (line->boxIndex < 0))
    {
        return 0;
    }

    cLayoutBase* layout = mEditor->GetLayout();
    if (layout == nullptr)
    {
        return 0;
    }

    const sBoxes* box = layout->GetBoxByIndex(line->boxIndex);
    if (box == nullptr)
    {
        return 0;
    }

    int indentCol = 0;
    COORD_T indent = line->pagex - box->left;
    if (indent > 0)
    {
        indentCol = static_cast<int>(indent / twipsPerCol);
    }

    int alignOffset = 0;
    if ((line->center == true) || (line->right == true))
    {
        cDocument* doc = mEditor->GetDocument();
        int contentCols = 0;
        POSITION_T docPos = line->documentPosition;
        for (const sSegmentLayout& s : line->segments)
        {
            if (doc == nullptr)
            {
                contentCols += static_cast<int>(s.GetGraphemeCount());
                continue;
            }
            std::vector<std::string> graphemes;
            s.GetGraphemes(doc, graphemes);
            for (size_t gi = 0; gi < graphemes.size(); ++gi)
            {
                contentCols += GraphemeColumns(docPos, graphemes[gi], line->pagenumber);
                docPos++;
            }
        }
        int boxCols = static_cast<int>((box->right - box->left) / twipsPerCol);
        int available = boxCols - indentCol - contentCols;
        if (available > 0)
        {
            if (line->center == true)
            {
                alignOffset = available / 2;
            }
            else if (line->right == true)
            {
                alignOffset = available;
            }
        }
    }

    return indentCol + alignOffset;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  const sLineLayout* line [in] line to measure
/// @param  COORD_T twipsPerCol [in] monospace column width in twips
///
/// @return the screen column just past the line's last grapheme
///
/// @brief
/// Walks the line's segments the way Draw() does (indent start, tab-stop
/// expansion, one column per grapheme) but without clipping to the text width.
/// Used to flag lines whose content spills past the visible text area.
///
/////////////////////////////////////////////////////////////////////////////
int cWSEditorView::LineEndCol(const sLineLayout* line, COORD_T twipsPerCol) const
{
    if (line == nullptr)
    {
        return 0;
    }

    cLayoutBase* layout = mEditor->GetLayout();
    cDocument* doc = mEditor->GetDocument();
    if ((layout == nullptr) || (doc == nullptr))
    {
        return LineStartCol(line, twipsPerCol);
    }

    int col = LineStartCol(line, twipsPerCol);
    POSITION_T docPos = line->documentPosition;

    for (const sSegmentLayout& seg : line->segments)
    {
        if (seg.isTab == true)
        {
            COORD_T currentTwips = static_cast<COORD_T>(col) * twipsPerCol;
            sTabStop nextStop = layout->GetNextTabStop(currentTwips);
            int nextCol = static_cast<int>(nextStop.position / twipsPerCol);
            if (nextCol <= col)
            {
                nextCol = col + 1;
            }
            col = nextCol;
            docPos++;
            continue;
        }

        std::vector<std::string> graphemes;
        seg.GetGraphemes(doc, graphemes);
        for (size_t gi = 0; gi < graphemes.size(); ++gi)
        {
            col += GraphemeColumns(docPos, graphemes[gi], line->pagenumber);
            docPos++;
        }
    }

    return col;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  POSITION_T docPos [in] document position of the grapheme
/// @param  const std::string& grapheme [in] the raw grapheme
/// @param  PAGE_T pagenumber [in] page number of the containing line
///
/// @return number of display columns the grapheme occupies
///
/// @brief
/// Most graphemes are one column. A marker (font tag, visible control code)
/// expands via GetDisplayCharacter to a multi-character string -- e.g. a font
/// tag renders as "<name size>" -- and occupies that many columns. Used by
/// Draw() and the column helpers so rendering, click mapping, caret scrolling,
/// and overflow detection all agree on a line's width.
///
/////////////////////////////////////////////////////////////////////////////
int cWSEditorView::GraphemeColumns(POSITION_T docPos, const std::string& grapheme, PAGE_T pagenumber) const
{
    unsigned char c = grapheme.empty() ? 0 : static_cast<unsigned char>(grapheme[0]);
    bool isMarker = (c == MARKER) || (c == REPLACE) || (c == SAVE);
    if (isMarker == false)
    {
        return 1;
    }

    cLayoutBase* layout = mEditor->GetLayout();
    if (layout == nullptr)
    {
        return 1;
    }

    std::string disp = layout->GetDisplayCharacter(docPos, grapheme, pagenumber);
    if (disp.empty() == true)
    {
        return 1;
    }

    int width = static_cast<int>(wordstartui::cUtf8Helper::SplitGraphemes(disp).size());
    if (width < 1)
    {
        width = 1;
    }
    return width;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return the editor's text area width in columns (excluding the flag /
///         scrollbar columns)
///
/// @brief
/// Match Draw()'s text-area width so horizontal scrolling and clipping agree.
///
/////////////////////////////////////////////////////////////////////////////
int cWSEditorView::TextWidthCols(void) const
{
    cLayoutBase* layout = mEditor->GetLayout();

    bool showFlag = (mEditor->mAlwaysFlag == true) ||
                    ((layout != nullptr) && (layout->GetShowControl() == SHOW_ALL));

    int rightReserved = (mShowScrollbar == true) ? 1 : 0;   // scrollbar column
    int textCols = mBounds.cols - rightReserved;
    if (showFlag == true)
    {
        textCols -= 1;   // reserve the flag column too
    }
    if (textCols < 1)
    {
        textCols = 1;
    }

    return textCols;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  const sLineLayout* line [in] the caret's line
/// @param  POSITION_T position [in] a document position on that line
///
/// @return the logical (unscrolled) monospace column of that position
///
/// @brief
/// Walk the line the way Draw() does (indent start, tab-stop expansion, one
/// column per grapheme) up to the given position. Used to keep the caret within
/// the horizontal scroll window.
///
/////////////////////////////////////////////////////////////////////////////
int cWSEditorView::LogicalColumnAt(const sLineLayout* line, POSITION_T position) const
{
    if (line == nullptr)
    {
        return 0;
    }

    cLayoutBase* layout = mEditor->GetLayout();
    cDocument* doc = mEditor->GetDocument();
    if ((layout == nullptr) || (doc == nullptr))
    {
        return 0;
    }

    COORD_T twipsPerCol = mEditor->GetColumnWidth();
    if (twipsPerCol <= 0)
    {
        twipsPerCol = 120;
    }

    int col = LineStartCol(line, twipsPerCol);
    POSITION_T docPos = line->documentPosition;

    for (const sSegmentLayout& seg : line->segments)
    {
        if (seg.isTab == true)
        {
            if (docPos >= position)
            {
                return col;
            }
            COORD_T currentTwips = static_cast<COORD_T>(col) * twipsPerCol;
            sTabStop nextStop = layout->GetNextTabStop(currentTwips);
            int nextCol = static_cast<int>(nextStop.position / twipsPerCol);
            if (nextCol <= col)
            {
                nextCol = col + 1;
            }
            col = nextCol;
            docPos++;
            continue;
        }

        std::vector<std::string> graphemes;
        seg.GetGraphemes(doc, graphemes);
        for (size_t gi = 0; gi < graphemes.size(); ++gi)
        {
            if (docPos >= position)
            {
                return col;
            }
            col += GraphemeColumns(docPos, graphemes[gi], line->pagenumber);
            docPos++;
        }
    }

    return col;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  int screenRow [in] absolute screen row of a click
///
/// @return the raw line number whose text belongs to that screen row
///
/// @brief
/// Reverse of Draw()'s accumulating row model: walks raw lines from mFirstLine,
/// charging a page-break separator row (on page change), the text row, then the
/// line-spacing blank rows. A click on a separator row maps to the line below
/// it; a click on a blank row maps to the text line above it; a click below all
/// content maps to the last rendered line.
///
/////////////////////////////////////////////////////////////////////////////
LINE_T cWSEditorView::RawLineAtScreenRow(int screenRow) const
{
    cLayoutBase* layout = mEditor->GetLayout();
    if (layout == nullptr)
    {
        return mFirstLine;
    }

    LINE_T total = layout->GetNumberOfLines();
    if (total <= 0)
    {
        return 0;
    }

    PAGE_T lastPage = 0;
    if (mFirstLine > 0)
    {
        const sLineLayout* prevLine = layout->GetLineByRawLineNumber(mFirstLine - 1);
        if (prevLine != nullptr)
        {
            lastPage = prevLine->pagenumber;
        }
    }

    int row = mBounds.row;
    int bottomRow = mBounds.row + mBounds.rows;
    LINE_T ln = mFirstLine;
    LINE_T lastLine = mFirstLine;

    while ((row < bottomRow) && (ln < total))
    {
        const sLineLayout* line = layout->GetLineByRawLineNumber(ln);
        if (line == nullptr)
        {
            ln++;
            continue;
        }

        // Separator row (charged before the text row) belongs to this line.
        if ((line->pagenumber != lastPage) && (lastPage != 0))
        {
            if (screenRow == row)
            {
                return ln;
            }
            row++;
            if (row >= bottomRow)
            {
                break;
            }
        }
        lastPage = line->pagenumber;

        // Text row.
        if (screenRow == row)
        {
            return ln;
        }
        row++;

        // Line-spacing blank rows belong to this same line.
        int extraRows = LineScreenRows(ln) - 1;
        for (int extra = 0; (extra < extraRows) && (row < bottomRow); ++extra)
        {
            if (screenRow == row)
            {
                return ln;
            }
            row++;
        }

        lastLine = ln;
        ln++;
    }

    return lastLine;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  const sLineLayout* line [in] line the click landed on
/// @param  int targetScreenCol [in] absolute screen column of the click
///
/// @return the document position of the character under the click
///
/// @brief
/// Reverse of Draw()'s column walk: starts at the line's indent column and
/// walks segments/graphemes (expanding tabs to their stop) until the running
/// column reaches the clicked column. Clamps to the line start on the left and
/// to the end-of-line position past the last grapheme.
///
/////////////////////////////////////////////////////////////////////////////
POSITION_T cWSEditorView::PositionInLine(const sLineLayout* line, int targetScreenCol) const
{
    if (line == nullptr)
    {
        return 0;
    }

    cLayoutBase* layout = mEditor->GetLayout();
    cDocument* doc = mEditor->GetDocument();
    if ((layout == nullptr) || (doc == nullptr))
    {
        return line->documentPosition;
    }

    COORD_T twipsPerCol = mEditor->GetColumnWidth();
    if (twipsPerCol <= 0)
    {
        twipsPerCol = 120;
    }

    int targetCol = (targetScreenCol - mBounds.col) + mHorizScroll;
    int col = LineStartCol(line, twipsPerCol);
    POSITION_T docPos = line->documentPosition;

    if (targetCol <= col)
    {
        return docPos;
    }

    for (const sSegmentLayout& seg : line->segments)
    {
        if (seg.isTab == true)
        {
            COORD_T currentTwips = static_cast<COORD_T>(col) * twipsPerCol;
            sTabStop nextStop = layout->GetNextTabStop(currentTwips);
            int nextCol = static_cast<int>(nextStop.position / twipsPerCol);
            if (nextCol <= col)
            {
                nextCol = col + 1;
            }

            if (targetCol < nextCol)
            {
                return docPos;
            }

            col = nextCol;
            docPos++;
            continue;
        }

        std::vector<std::string> graphemes;
        seg.GetGraphemes(doc, graphemes);

        for (size_t gi = 0; gi < graphemes.size(); ++gi)
        {
            if (targetCol <= col)
            {
                return docPos;
            }
            col += GraphemeColumns(docPos, graphemes[gi], line->pagenumber);
            docPos++;
        }
    }

    return docPos;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Scroll the viewport so the caret's line is visible.
///
/////////////////////////////////////////////////////////////////////////////
void cWSEditorView::EnsureCaretVisible(void)
{
    cLayoutBase* layout = mEditor->GetLayout();
    cDocument* doc = mEditor->GetDocument();

    if ((layout == nullptr) || (doc == nullptr))
    {
        return;
    }

    mEditor->CalculateCaretPosition();

    POSITION_T caretPos = doc->GetPosition();
    LINE_T caretLine = layout->GetLineFromPosition(caretPos);
    LINE_T total = layout->GetNumberOfLines();
    int rows = mBounds.rows;

    if (caretLine < mFirstLine)
    {
        mFirstLine = caretLine;
    }
    else
    {
        // Walk from the current top toward the caret line, charging each line
        // its line-spacing rows plus a row for any page-break separator, the
        // same way Draw() lays them out. The caret stays visible while its
        // text row lands inside the viewport height.
        int rowsAccumulated = 0;
        PAGE_T lastPage = 0;
        if (mFirstLine > 0)
        {
            const sLineLayout* prevLine = layout->GetLineByRawLineNumber(mFirstLine - 1);
            if (prevLine != nullptr)
            {
                lastPage = prevLine->pagenumber;
            }
        }

        bool caretVisible = false;
        for (LINE_T scan = mFirstLine; (scan <= caretLine) && (scan < total); ++scan)
        {
            const sLineLayout* scanLine = layout->GetLineByRawLineNumber(scan);
            if (scanLine == nullptr)
            {
                break;
            }

            int sep = 0;
            if ((scanLine->pagenumber != lastPage) && (lastPage != 0))
            {
                sep = 1;
            }

            if (scan == caretLine)
            {
                if ((rowsAccumulated + sep) < rows)
                {
                    caretVisible = true;
                }
                break;
            }

            rowsAccumulated += sep + LineScreenRows(scan);
            lastPage = scanLine->pagenumber;
        }

        if (caretVisible == false)
        {
            // Caret below the viewport: walk backward from the caret line,
            // charging each preceding line (and any separator between it and
            // its successor) until the viewport height is exhausted.
            int sumRows = 1;
            LINE_T newTop = caretLine;
            const sLineLayout* caretLineLayout = layout->GetLineByRawLineNumber(caretLine);
            PAGE_T nextPage = (caretLineLayout != nullptr) ? caretLineLayout->pagenumber : 0;

            while (newTop > 0)
            {
                LINE_T prev = newTop - 1;
                const sLineLayout* prevLine = layout->GetLineByRawLineNumber(prev);
                if (prevLine == nullptr)
                {
                    break;
                }

                int prevRows = LineScreenRows(prev);
                if (prevLine->pagenumber != nextPage)
                {
                    prevRows += 1;
                }

                if ((sumRows + prevRows) > rows)
                {
                    break;
                }

                sumRows += prevRows;
                nextPage = prevLine->pagenumber;
                newTop = prev;
            }

            mFirstLine = newTop;
        }
    }

    if (mFirstLine < 0)
    {
        mFirstLine = 0;
    }

    // Horizontal scroll: keep the caret's logical column inside the text area,
    // with a small margin.
    const sLineLayout* caretLineLayout = layout->GetLineByRawLineNumber(caretLine);
    if (caretLineLayout != nullptr)
    {
        int caretCol = LogicalColumnAt(caretLineLayout, caretPos);
        int width = TextWidthCols();
        int margin = width / 4;
        if (margin > 4)
        {
            margin = 4;
        }

        if (caretCol < (mHorizScroll + margin))
        {
            mHorizScroll = caretCol - margin;
            if (mHorizScroll < 0)
            {
                mHorizScroll = 0;
            }
        }
        else if (caretCol >= (mHorizScroll + width - margin))
        {
            mHorizScroll = caretCol - width + margin + 1;
        }
    }

    mEditor->SetViewport(mFirstLine, mBounds.rows);
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  cScreen& screen [in,out] target screen
///
/// @return nothing
///
/// @brief
/// Render the visible document lines as RGB cells and record the caret cell.
///
/////////////////////////////////////////////////////////////////////////////
void cWSEditorView::Draw(wordstartui::cScreen& screen)
{
    cLayoutBase* layout = mEditor->GetLayout();
    cDocument* doc = mEditor->GetDocument();

    mCaretVisible = false;

    if ((layout == nullptr) || (doc == nullptr))
    {
        return;
    }

    sStyle normal = StyleFor(mEditor->mTextColour, mEditor->mBGroundColour, wordstartui::CELL_ATTR_NONE);
    sStyle flagStyle = StyleFor(mEditor->mFlagFg, mEditor->mFlagBg, wordstartui::CELL_ATTR_NONE);

    COORD_T twipsPerCol = mEditor->GetColumnWidth();
    if (twipsPerCol <= 0)
    {
        twipsPerCol = 120;
    }

    POSITION_T caretPos = doc->GetPosition();
    LINE_T total = layout->GetNumberOfLines();
    eShowControl showCtl = layout->GetShowControl();
    bool showFlag = (mEditor->mAlwaysFlag == true) || (showCtl == SHOW_ALL);

    int cols = mBounds.cols;
    int rightReserved = (mShowScrollbar == true) ? 1 : 0;   // scrollbar column
    int scrollCol = mBounds.col + cols - 1;                 // used only when the scrollbar shows
    int flagCol = mBounds.col + cols - 1 - rightReserved;   // just left of the scrollbar (or last col)
    int textCols = cols - rightReserved;
    if (showFlag == true)
    {
        textCols -= 1;
    }
    if (textCols < 1)
    {
        textCols = 1;
    }

    // Clear the whole editor area to the editor background first.
    sRect area = mBounds;
    screen.FillRect(area, " ", normal);

    // Accumulating row model: each raw line takes one text row plus any
    // line-spacing blank rows, and a page-break separator row is inserted
    // whenever the page number changes. lastPage seeds from the line above
    // the viewport so a break at the very top of the viewport is still drawn.
    PAGE_T lastPage = 0;
    if (mFirstLine > 0)
    {
        const sLineLayout* prevLine = layout->GetLineByRawLineNumber(mFirstLine - 1);
        if (prevLine != nullptr)
        {
            lastPage = prevLine->pagenumber;
        }
    }

    int screenRow = mBounds.row;
    int bottomRow = mBounds.row + mBounds.rows;
    LINE_T ln = mFirstLine;

    while ((screenRow < bottomRow) && (ln < total))
    {
        const sLineLayout* line = layout->GetLineByRawLineNumber(ln);
        if (line == nullptr)
        {
            ln++;
            continue;
        }

        // ----- page-break separator when the page number changes -----
        if ((line->pagenumber != lastPage) && (lastPage != 0))
        {
            for (int c = 0; c < textCols; ++c)
            {
                screen.PutCell(screenRow, mBounds.col + c, "\xe2\x94\x80", normal);
            }
            screen.PutCell(screenRow, flagCol, "P", flagStyle);
            screenRow++;
            if (screenRow >= bottomRow)
            {
                break;
            }
        }
        lastPage = line->pagenumber;

        // ----- dot-command / comment line colour + flag indicator -----
        const sParagraphLayout* para = nullptr;
        if (line->segments.empty() == false)
        {
            para = layout->GetParagraphLayout(line->segments.front().paragraph);
        }

        bool isDotLine = (line->isPrintable == false);
        sColor dotFg = mEditor->mDotColour;   // reused below; corrected per status
        sColor dotBg = mEditor->mDotColour;
        dotFg = mEditor->mDotFgColour;
        dotBg = mEditor->mDotColour;

        std::string flagChar = " ";

        if (isDotLine == true)
        {
            if ((para != nullptr) && (para->isComment == true))
            {
                dotFg = mEditor->mCommentFgColour;
                dotBg = mEditor->mCommentColour;
            }
            else if ((para != nullptr) && (para->dotStatus == DOT_ERROR))
            {
                dotFg = mEditor->mErrorFgColour;
                dotBg = mEditor->mErrorColour;
            }
            else if ((para != nullptr) && (para->dotStatus == DOT_UNKNOWN))
            {
                dotFg = mEditor->mUnknownFgColour;
                dotBg = mEditor->mUnknownColour;
            }
            else if ((para != nullptr) && (para->dotStatus == DOT_NOTIMPLEMENTED))
            {
                dotFg = mEditor->mNotImplementedFgColour;
                dotBg = mEditor->mNotImplementedColour;
            }

            // A dot/comment line paints the whole width in its colour.
            sRect lineRect;
            lineRect.row = screenRow;
            lineRect.col = mBounds.col;
            lineRect.rows = 1;
            lineRect.cols = textCols;
            screen.FillRect(lineRect, " ", StyleFor(dotFg, dotBg, wordstartui::CELL_ATTR_NONE));

            if ((para != nullptr) && ((para->isComment == true) || (para->dotStatus == DOT_GOOD)))
            {
                flagChar = ".";
            }
            else
            {
                flagChar = "?";
            }
        }
        else
        {
            // Regular paragraph: flag the last line with the CR symbol.
            const sLineLayout* nextl = layout->GetLineByRawLineNumber(ln + 1);
            PARAGRAPH_T thisPara = line->segments.empty() ? static_cast<PARAGRAPH_T>(-1)
                                                          : line->segments.front().paragraph;
            PARAGRAPH_T nextPara = (nextl != nullptr && nextl->segments.empty() == false)
                                       ? nextl->segments.front().paragraph
                                       : static_cast<PARAGRAPH_T>(-1);
            if ((nextl == nullptr) || (thisPara != nextPara))
            {
                flagChar = "\xe2\x86\xb5";   // CR symbol
            }
        }

        // ----- leading indent + centre/right offset -----
        int startCol = LineStartCol(line, twipsPerCol);

        int hs = mHorizScroll;
        int col = startCol;
        POSITION_T docPos = line->documentPosition;

        for (const sSegmentLayout& seg : line->segments)
        {
            sColor segFg = mEditor->mTextColour;
            sColor segBg = mEditor->mBGroundColour;

            if (seg.isBlock == true)
            {
                segFg = mEditor->mBlockFgColour;
                segBg = mEditor->mBlockColour;
            }
            else if (isDotLine == true)
            {
                segFg = dotFg;
                segBg = dotBg;
            }
            else if (seg.textcolor.IsDefault() == false)
            {
                // Explicit per-segment text color from the document (.color / RTF).
                segFg = wordstartui::MakeRgb(static_cast<uint8_t>(seg.textcolor.red),
                                             static_cast<uint8_t>(seg.textcolor.green),
                                             static_cast<uint8_t>(seg.textcolor.blue));
            }

            if (seg.isTab == true)
            {
                int caretSc = col - hs;
                if ((docPos == caretPos) && (caretSc >= 0) && (caretSc < textCols))
                {
                    mCaretRow = screenRow;
                    mCaretCol = mBounds.col + caretSc;
                    mCaretVisible = true;
                }

                int currentCol = col;
                COORD_T currentTwips = static_cast<COORD_T>(currentCol) * twipsPerCol;
                sTabStop nextStop = layout->GetNextTabStop(currentTwips);
                int nextCol = static_cast<int>(nextStop.position / twipsPerCol);
                if (nextCol <= currentCol)
                {
                    nextCol = currentCol + 1;
                }

                // Tab-type marker shown at the tab's start in Show-All mode.
                std::string tabMarker = ">";
                switch (seg.tabType)
                {
                    case TAB_CENTER:  tabMarker = "!"; break;
                    case TAB_RIGHT:
                    case TAB_RIGHT1:  tabMarker = "["; break;
                    case TAB_DECIMAL: tabMarker = "#"; break;
                    case TAB_TAB:
                    default:          tabMarker = ">"; break;
                }

                for (int t = currentCol; t < nextCol; ++t)
                {
                    int st = t - hs;
                    if (st < 0)
                    {
                        continue;
                    }
                    if (st >= textCols)
                    {
                        break;
                    }

                    if ((t == currentCol) && (showCtl == SHOW_ALL))
                    {
                        screen.PutCell(screenRow, mBounds.col + st, tabMarker,
                                       StyleFor(mEditor->mHighlightFgColour, mEditor->mHighlightColour, wordstartui::CELL_ATTR_NONE));
                    }
                    else
                    {
                        screen.PutCell(screenRow, mBounds.col + st, " ", StyleFor(segFg, segBg, wordstartui::CELL_ATTR_NONE));
                    }
                }

                col = nextCol;
                docPos++;
                continue;
            }

            std::vector<std::string> graphemes;
            seg.GetGraphemes(doc, graphemes);

            uint32_t attrs = wordstartui::CELL_ATTR_NONE;
            ApplySegmentFormatting(seg.font, seg.isSuperscript, seg.isSubscript, segFg, segBg, attrs);

            for (size_t gi = 0; gi < graphemes.size(); ++gi)
            {
                int startSc = col - hs;
                if (startSc >= textCols)
                {
                    break;
                }

                const std::string& g = graphemes[gi];
                unsigned char c = g.empty() ? 0 : static_cast<unsigned char>(g[0]);

                if ((docPos == caretPos) && (startSc >= 0))
                {
                    mCaretRow = screenRow;
                    mCaretCol = mBounds.col + startSc;
                    mCaretVisible = true;
                }

                bool isCC = false;
                for (size_t ccIdx : seg.controlCodeIndices)
                {
                    if (ccIdx == gi)
                    {
                        isCC = true;
                        break;
                    }
                }

                bool isMarker = (c == MARKER) || (c == REPLACE) || (c == SAVE);
                bool isTerminator = (c == HARDCR) || (c == EOFCH);

                bool isSearch = (mEditor->mSearchBlockSet == true) &&
                                (docPos >= mEditor->mStartSearchBlock) &&
                                (docPos < mEditor->mEndSearchBlock);

                // Search-hit colours sit above the base/dot colours but below a
                // marked block (block wins) and the control-code highlight.
                sColor gFg = segFg;
                sColor gBg = segBg;
                if ((isSearch == true) && (seg.isBlock == false))
                {
                    gFg = mEditor->mSearchFgColour;
                    gBg = mEditor->mSearchColour;
                }

                // Resolve the display columns this grapheme paints. A marker can
                // expand to several columns (e.g. a font tag "<name size>"); a
                // terminator paints one blank; anything else is the grapheme.
                std::vector<std::string> cells;
                if (isMarker == true)
                {
                    std::string disp = layout->GetDisplayCharacter(docPos, g, line->pagenumber);
                    if (disp.empty() == true)
                    {
                        disp = " ";
                    }
                    cells = wordstartui::cUtf8Helper::SplitGraphemes(disp);
                    if (cells.empty() == true)
                    {
                        cells.push_back(" ");
                    }

                    if (isCC == true)
                    {
                        gFg = mEditor->mHighlightFgColour;
                        gBg = mEditor->mHighlightColour;
                    }
                }
                else if (isTerminator == true)
                {
                    cells.push_back(" ");
                }
                else
                {
                    cells.push_back(g);
                }

                for (size_t ci = 0; ci < cells.size(); ++ci)
                {
                    int sc = col - hs;
                    if (sc >= textCols)
                    {
                        break;
                    }
                    if (sc >= 0)
                    {
                        screen.PutCell(screenRow, mBounds.col + sc, cells[ci],
                                       StyleFor(gFg, gBg, attrs));
                    }
                    col++;
                }

                docPos++;
            }
        }

        // Caret at or past the last grapheme on this line.
        if ((docPos == caretPos) && (mCaretVisible == false))
        {
            int caretSc = col - hs;
            if ((caretSc >= 0) && (caretSc < textCols))
            {
                mCaretRow = screenRow;
                mCaretCol = mBounds.col + caretSc;
                mCaretVisible = true;
            }
        }

        // ----- flag / indicator column -----
        if (showFlag == true)
        {
            // A line whose content spills past the visible right edge gets the
            // WordStar "+" overflow flag, overriding the CR / dot indicator.
            if (LineEndCol(line, twipsPerCol) > (mHorizScroll + textCols))
            {
                flagChar = "+";
            }

            screen.PutCell(screenRow, flagCol, flagChar, flagStyle);
        }

        screenRow++;

        // ----- line-spacing blank rows (.ls multiplier) -----
        int extraRows = LineScreenRows(ln) - 1;
        for (int extra = 0; (extra < extraRows) && (screenRow < bottomRow); ++extra)
        {
            if (showFlag == true)
            {
                screen.PutCell(screenRow, flagCol, " ", flagStyle);
            }
            screenRow++;
        }

        ln++;
    }

    // Scrollbar column with a proportional, draggable thumb (hidden when the
    // "Show scroll bar" display option is off).
    if ((mShowScrollbar == true) && (mBounds.cols >= 1))
    {
        int totalLines = 0;

        if (layout != nullptr)
        {
            totalLines = static_cast<int>(layout->GetNumberOfLines());
        }

        wordstartui::sStyle trackStyle = StyleFor(mEditor->mScrollbarFg, mEditor->mScrollbarBg, wordstartui::CELL_ATTR_NONE);
        wordstartui::sStyle thumbStyle = StyleFor(mEditor->mScrollbarFg, mEditor->mScrollbarBg, wordstartui::CELL_ATTR_BOLD);

        wordstartui::cScrollBar scrollbar;
        wordstartui::sRect track;
        track.row = mBounds.row;
        track.col = scrollCol;
        track.rows = mBounds.rows;
        track.cols = 1;
        scrollbar.SetArrows(true);
        scrollbar.SetMetrics(track, totalLines, mBounds.rows, static_cast<int>(mFirstLine));
        scrollbar.Draw(screen, trackStyle, thumbStyle);
    }
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  const sInputEvent& event [in] wordstartui input event
///
/// @return true if the event was consumed
///
/// @brief
/// Translate a wordstartui input event into the shared WordStar input handler
/// (control chars first, then named special keys, then printable text).
///
/////////////////////////////////////////////////////////////////////////////
bool cWSEditorView::HandleEvent(const sInputEvent& event)
{
    mScrolledByBar = false;

    // Mouse wheel and scrollbar (arrows / thumb) scroll the document.
    if (event.type == wordstartui::INPUT_TYPE_MOUSE)
    {
        cLayoutBase* sblayout = mEditor->GetLayout();

        if (sblayout == nullptr)
        {
            return false;
        }

        int total = static_cast<int>(sblayout->GetNumberOfLines());
        int maxTop = total - mBounds.rows;

        if (maxTop < 0)
        {
            maxTop = 0;
        }

        LINE_T target = mFirstLine;
        bool doScroll = false;

        // Wheel scrolls three lines per notch.
        if (event.mouseAction == wordstartui::MOUSE_ACTION_WHEEL)
        {
            target = mFirstLine + static_cast<LINE_T>(event.mouseWheel * 3);
            doScroll = true;
        }
        else
        {
            int scrollCol = mBounds.col + mBounds.cols - 1;
            bool leftButton = (event.mouseButton == wordstartui::MOUSE_BUTTON_LEFT);
            bool pressOrDrag = (event.mouseAction == wordstartui::MOUSE_ACTION_PRESS) ||
                               (event.mouseAction == wordstartui::MOUSE_ACTION_DRAG);
            bool onScrollbar = (mShowScrollbar == true) && (event.mouseCol == scrollCol) &&
                               (event.mouseRow >= mBounds.row) &&
                               (event.mouseRow < (mBounds.row + mBounds.rows));

            if ((pressOrDrag == true) && (leftButton == true) && (onScrollbar == true))
            {
                wordstartui::cScrollBar scrollbar;
                wordstartui::sRect track;
                track.row = mBounds.row;
                track.col = scrollCol;
                track.rows = mBounds.rows;
                track.cols = 1;
                scrollbar.SetArrows(true);
                scrollbar.SetMetrics(track, total, mBounds.rows, static_cast<int>(mFirstLine));

                int pageStep = mBounds.rows - 1;   // one viewport, less a line of overlap
                if (pageStep < 1)
                {
                    pageStep = 1;
                }

                if (event.mouseAction == wordstartui::MOUSE_ACTION_PRESS)
                {
                    if (scrollbar.IsUpArrow(event.mouseRow) == true)
                    {
                        target = mFirstLine - 1;
                    }
                    else if (scrollbar.IsDownArrow(event.mouseRow) == true)
                    {
                        target = mFirstLine + 1;
                    }
                    else if (scrollbar.IsAboveThumb(event.mouseRow) == true)
                    {
                        target = mFirstLine - pageStep;   // page up
                    }
                    else if (scrollbar.IsBelowThumb(event.mouseRow) == true)
                    {
                        target = mFirstLine + pageStep;   // page down
                    }
                    else
                    {
                        target = mFirstLine;   // press on the thumb: drag moves it
                    }
                }
                else
                {
                    // Drag maps the thumb proportionally to the mouse row.
                    target = static_cast<LINE_T>(scrollbar.TopForRow(event.mouseRow));
                }

                doScroll = true;
            }
        }

        // Left-click in the text area positions the caret (matches the GUI).
        if ((doScroll == false) &&
            (event.mouseAction == wordstartui::MOUSE_ACTION_PRESS) &&
            (event.mouseButton == wordstartui::MOUSE_BUTTON_LEFT))
        {
            bool inTextArea = (event.mouseRow >= mBounds.row) &&
                              (event.mouseRow < (mBounds.row + mBounds.rows)) &&
                              (event.mouseCol >= mBounds.col) &&
                              (event.mouseCol < (mBounds.col + mBounds.cols - 1));

            if (inTextArea == true)
            {
                cDocument* doc = mEditor->GetDocument();
                if (doc != nullptr)
                {
                    LINE_T ln = RawLineAtScreenRow(event.mouseRow);
                    const sLineLayout* line = sblayout->GetLineByRawLineNumber(ln);
                    if (line != nullptr)
                    {
                        POSITION_T pos = PositionInLine(line, event.mouseCol);
                        doc->SetPosition(pos);
                        mEditor->mLastKeyUpOrDown = false;   // reset sticky column
                        mScrolledByBar = false;
                        mEditor->Repaint();
                    }
                }
                return true;
            }
        }

        if (doScroll == false)
        {
            return false;
        }

        if (target < 0)
        {
            target = 0;
        }

        if (target > static_cast<LINE_T>(maxTop))
        {
            target = static_cast<LINE_T>(maxTop);
        }

        mFirstLine = target;
        mEditor->SetViewport(mFirstLine, mBounds.rows);
        mEditor->Repaint();
        mScrolledByBar = true;
        return true;
    }

    IInputHandler* input = mEditor->GetInput();
    if (input == nullptr)
    {
        return false;
    }

    bool handled = false;

    // 1. Control characters (^A..^Z) route to the engine first, matching the
    //    WordStar diamond/chord model. 0x0D (Enter) is delivered as a special
    //    key below.
    if ((event.type == wordstartui::INPUT_TYPE_CONTROL) && (event.controlCode >= 1) && (event.controlCode <= 26) && (event.controlCode != 13))
    {
        handled = input->HandleKey(static_cast<char>(event.controlCode), event.shift, false);
        if (handled == true)
        {
            return true;
        }
    }

    // 2. Named special keys.
    if (event.type == wordstartui::INPUT_TYPE_SPECIAL)
    {
        eSpecialKey key = SPECIAL_ESCAPE;
        bool mapped = true;

        switch (event.special)
        {
            case wordstartui::SPECIAL_KEY_ENTER:       key = SPECIAL_ENTER;      break;
            case wordstartui::SPECIAL_KEY_TAB:         key = SPECIAL_TAB;        break;
            case wordstartui::SPECIAL_KEY_BACKSPACE:   key = SPECIAL_BACKSPACE;  break;
            case wordstartui::SPECIAL_KEY_DELETE:      key = SPECIAL_DELETE;     break;
            case wordstartui::SPECIAL_KEY_HOME:        key = SPECIAL_HOME;       break;
            case wordstartui::SPECIAL_KEY_END:         key = SPECIAL_END;        break;
            case wordstartui::SPECIAL_KEY_PAGE_UP:     key = SPECIAL_PAGE_UP;    break;
            case wordstartui::SPECIAL_KEY_PAGE_DOWN:   key = SPECIAL_PAGE_DOWN;  break;
            case wordstartui::SPECIAL_KEY_ARROW_UP:    key = SPECIAL_UP;         break;
            case wordstartui::SPECIAL_KEY_ARROW_DOWN:  key = SPECIAL_DOWN;       break;
            case wordstartui::SPECIAL_KEY_ARROW_LEFT:  key = SPECIAL_LEFT;       break;
            case wordstartui::SPECIAL_KEY_ARROW_RIGHT: key = SPECIAL_RIGHT;      break;
            case wordstartui::SPECIAL_KEY_ESCAPE:      key = SPECIAL_ESCAPE;     break;
            default:                                   mapped = false;          break;
        }

        if (mapped == true)
        {
            handled = input->HandleSpecialKey(key, event.shift, event.ctrl, event.alt);
        }

        if (handled == true)
        {
            return true;
        }
    }

    // 3. Function keys.
    if (event.type == wordstartui::INPUT_TYPE_FUNCTION)
    {
        if ((event.functionKey >= 1) && (event.functionKey <= 12))
        {
            eSpecialKey key = static_cast<eSpecialKey>(SPECIAL_F1 + (event.functionKey - 1));
            handled = input->HandleSpecialKey(key, event.shift, event.ctrl, event.alt);
            if (handled == true)
            {
                return true;
            }
        }
    }

    // 4. Printable text.
    if ((event.type == wordstartui::INPUT_TYPE_TEXT) && (event.textUtf8.empty() == false))
    {
        unsigned char first = static_cast<unsigned char>(event.textUtf8[0]);

        if (first >= 0x20)
        {
            if (event.textUtf8.size() == 1)
            {
                handled = input->HandleKey(event.textUtf8[0], event.shift, event.alt);
                if (handled == false)
                {
                    mEditor->InsertText(event.textUtf8);
                    handled = true;
                }
            }
            else
            {
                mEditor->InsertText(event.textUtf8);
                handled = true;
            }
        }
    }

    return handled;
}
