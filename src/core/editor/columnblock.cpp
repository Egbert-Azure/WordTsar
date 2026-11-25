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

/////////////////////////////////////////////////////////////////////////////
///
/// Column blocks (^KN) and column replace (^KI).
///
/// With column mode on, the block is the rectangle from the line of the
/// ^KB marker to the line of the ^KK marker, between the layout x of the
/// two markers (twips). Every layout line in between is a row, except dot
/// command and comment lines, which no column command changes.
///
/// A line with tabs is split into cells at its tabs. A row takes the whole
/// cells whose first character lies in [left, right), plus one separator
/// tab: the one before the first cell taken, or the one after the last
/// when the first cell of the line is taken. A line without tabs takes the
/// characters whose left edge lies in [left, right).
///
/// Inserting at the cursor's x on the cursor line and the lines below: in a
/// line with tabs, an x inside a cell moves to the end of that cell, and
/// cells from a line with tabs bring one separator tab on the side that
/// has text; with column replace on, cells replace cells one for one and
/// the tabs stay. A line without tabs is padded with spaces up to x; with
/// column replace on, the copied text overwrites as many characters.
///
/// Copy, move and delete plan every change before the first one, apply
/// them from the highest position down in one undo group, and set ^KB and
/// ^KK on the result.
///
/////////////////////////////////////////////////////////////////////////////

#include "editorbase.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>

bool cEditorBase::mColumnMode = false ;
bool cEditorBase::mColumnReplace = false ;


struct cEditorBase::sColumnLine
{
    PARAGRAPH_T paragraph = 0 ;
    POSITION_T start = 0 ;                  // first position of the line
    POSITION_T end = 0 ;                    // end of the line's text
    std::vector<COORD_T> x ;                // layout x of each position start..end-1
    COORD_T pagex = 0 ;                     // x of the line start
    COORD_T endX = 0 ;                      // x after the line's last character
    std::vector<POSITION_T> tabs ;          // positions of the line's tabs
    std::string font ;                      // font at the line end

    COORD_T XAt(POSITION_T position) const
    {
        return (position >= start && position < end) ? x[static_cast<size_t>(position - start)] : endX ;
    }
} ;


struct cEditorBase::sColumnBounds
{
    COORD_T left = 0 ;
    COORD_T right = 0 ;
    PARAGRAPH_T firstPara = 0 ;
    size_t firstLine = 0 ;
    PARAGRAPH_T lastPara = 0 ;
    size_t lastLine = 0 ;
} ;


namespace {

// layout x values closer than this are the same column
const COORD_T COLUMN_EPSILON = 1.0 ;

const char BAD_SHAPE_MESSAGE[] = "End of block is not below and to right of start of block." ;

struct sColumnEdit
{
    POSITION_T pos = 0 ;
    POSITION_T del = 0 ;
    std::vector<sUndoCharInfo> ins ;
    size_t seq = 0 ;
    POSITION_T finalPos = 0 ;               // position of ins after every edit
} ;

struct sColumnSource
{
    bool tabbed = false ;
    std::vector<sUndoCharInfo> content ;                    // the cells, with the tabs between them
    std::vector<std::vector<sUndoCharInfo>> cells ;         // the cells, without tabs
    sUndoCharInfo separator ;
} ;

sUndoCharInfo PlainChar(CHAR_T ch)
{
    sUndoCharInfo info ;
    info.codepoint = ch ;
    return info ;
}

std::vector<sUndoCharInfo> Spaces(POSITION_T count)
{
    return std::vector<sUndoCharInfo>(static_cast<size_t>(std::max(count, POSITION_T(0))), PlainChar(' ')) ;
}

void Append(std::vector<sUndoCharInfo>& to, const std::vector<sUndoCharInfo>& chars)
{
    to.insert(to.end(), chars.begin(), chars.end()) ;
}

// characters that take space on the line
POSITION_T VisibleCount(const std::vector<sUndoCharInfo>& chars)
{
    return static_cast<POSITION_T>(std::count_if(chars.begin(), chars.end(), [](const sUndoCharInfo& info)
    {
        return info.codepoint >= STYLE_END_OF_STYLES ;
    })) ;
}

// The order edits are applied in: highest position first; at one position
// deletions first, then the later edit first, so inserts at one position
// end up in the order they were planned.
std::vector<size_t> ApplyOrder(const std::vector<sColumnEdit>& edits)
{
    std::vector<size_t> order(edits.size()) ;
    std::iota(order.begin(), order.end(), size_t(0)) ;
    std::sort(order.begin(), order.end(), [&edits](size_t a, size_t b)
    {
        const sColumnEdit& x = edits[a] ;
        const sColumnEdit& y = edits[b] ;
        if (x.pos != y.pos)
        {
            return x.pos > y.pos ;
        }
        if ((x.del > 0) != (y.del > 0))
        {
            return x.del > 0 ;
        }
        return x.seq > y.seq ;
    }) ;
    return order ;
}

// false if two edits change the same text
bool EditsOverlap(const std::vector<sColumnEdit>& edits)
{
    std::vector<const sColumnEdit*> sorted ;
    for (const sColumnEdit& edit : edits)
    {
        sorted.push_back(&edit) ;
    }
    std::sort(sorted.begin(), sorted.end(), [](const sColumnEdit* a, const sColumnEdit* b)
    {
        return (a->pos != b->pos) ? (a->pos < b->pos) : (a->del < b->del) ;
    }) ;
    for (size_t i = 1 ; i < sorted.size() ; i++)
    {
        if (sorted[i - 1]->pos + sorted[i - 1]->del > sorted[i]->pos)
        {
            return true ;
        }
    }
    return false ;
}

// applies the edits and sets finalPos of each
void ApplyEdits(cDocument* document, std::vector<sColumnEdit>& edits)
{
    std::vector<size_t> order = ApplyOrder(edits) ;

    POSITION_T shift = 0 ;
    for (size_t i = order.size() ; i > 0 ; i--)
    {
        sColumnEdit& edit = edits[order[i - 1]] ;
        edit.finalPos = edit.pos + shift ;
        shift += static_cast<POSITION_T>(edit.ins.size()) - edit.del ;
    }

    for (size_t index : order)
    {
        const sColumnEdit& edit = edits[index] ;
        if (edit.del > 0)
        {
            document->Delete(edit.pos, edit.del) ;
        }
        if (edit.ins.empty() == false)
        {
            document->ReinsertCharacters(edit.pos, edit.ins) ;
        }
    }
}

// where an unchanged position, or one inside a replaced range, is after the edits
POSITION_T MapPosition(const std::vector<sColumnEdit>& edits, POSITION_T position)
{
    POSITION_T shift = 0 ;
    for (const sColumnEdit& edit : edits)
    {
        if (edit.del > 0 && position >= edit.pos && position <= edit.pos + edit.del)
        {
            return edit.finalPos + std::min(position - edit.pos, static_cast<POSITION_T>(edit.ins.size())) ;
        }
        if (edit.pos + edit.del <= position)
        {
            shift += static_cast<POSITION_T>(edit.ins.size()) - edit.del ;
        }
    }
    return position + shift ;
}

// cell starts and ends of a line with tabs
void LineCells(POSITION_T start, POSITION_T end, const std::vector<POSITION_T>& tabs,
               std::vector<POSITION_T>& cellStarts, std::vector<POSITION_T>& cellEnds)
{
    cellStarts.assign(1, start) ;
    cellEnds.clear() ;
    for (POSITION_T tab : tabs)
    {
        cellEnds.push_back(tab) ;
        cellStarts.push_back(tab + 1) ;
    }
    cellEnds.push_back(end) ;
}

} // namespace


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Column mode (^KN) and column replace (^KI) are one setting for every
/// editor of the session.
///
/////////////////////////////////////////////////////////////////////////////
void cEditorBase::SetColumnMode(bool on)
{
    mColumnMode = on ;
}


bool cEditorBase::GetColumnMode(void)
{
    return mColumnMode ;
}


void cEditorBase::SetColumnReplace(bool on)
{
    mColumnReplace = on ;
}


bool cEditorBase::GetColumnReplace(void)
{
    return mColumnReplace ;
}


void cEditorBase::ToggleColumnMode(void)
{
    SetColumnMode(!mColumnMode) ;
    Repaint() ;
}


void cEditorBase::ToggleColumnReplace(void)
{
    SetColumnReplace(!mColumnReplace) ;
    Repaint() ;
}


bool cEditorBase::IsColumnBlock(void) const
{
    return mColumnMode && mDocument != nullptr && mDocument->mBlockSet ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  para      [in]  paragraph
/// @param  lineIndex [in]  layout line of the paragraph
/// @param  line      [out] its positions, layout x and tabs
///
/// @return false if the line is not laid out
///
/// @brief
/// Positions the layout leaves out (hidden control codes) take the x of
/// the next shown character.
///
/////////////////////////////////////////////////////////////////////////////
bool cEditorBase::BuildColumnLine(PARAGRAPH_T para, size_t lineIndex, sColumnLine& line)
{
    const sParagraphLayout* paraLayout = mLayout->GetParagraphLayout(para) ;
    if (paraLayout == nullptr || lineIndex >= paraLayout->lines.size() ||
        para >= mDocument->GetNumberofParagraphs())
    {
        return false ;
    }

    POSITION_T paraStart = 0, paraEnd = 0 ;
    mDocument->GetParagraphStartandEnd(para, paraStart, paraEnd) ;

    const sLineLayout& layoutLine = paraLayout->lines[lineIndex] ;
    line.paragraph = para ;
    line.start = paraStart + layoutLine.linestart ;
    line.end = (lineIndex + 1 < paraLayout->lines.size()) ? paraStart + paraLayout->lines[lineIndex + 1].linestart : paraEnd ;
    line.end = std::max(line.end, line.start) ;
    line.pagex = layoutLine.pagex ;
    line.endX = layoutLine.pagex + layoutLine.LayoutEndX() ;
    line.font.clear() ;
    line.tabs.clear() ;

    size_t count = static_cast<size_t>(line.end - line.start) ;
    line.x.assign(count, 0) ;
    std::vector<bool> shown(count, false) ;

    for (const sSegmentLayout& segment : layoutLine.segments)
    {
        size_t graphemes = std::min(segment.GetGraphemeCount(), segment.position.size()) ;
        for (size_t i = 0 ; i < graphemes ; i++)
        {
            POSITION_T position = paraStart + segment.startPosition + static_cast<POSITION_T>(i) ;
            if (position >= line.start && position < line.end)
            {
                size_t index = static_cast<size_t>(position - line.start) ;
                line.x[index] = layoutLine.pagex + segment.position[i] ;
                shown[index] = true ;
            }
        }
        line.font = segment.font ;
    }

    COORD_T next = line.endX ;
    for (size_t i = count ; i > 0 ; i--)
    {
        if (shown[i - 1] == false)
        {
            line.x[i - 1] = next ;
        }
        next = line.x[i - 1] ;
    }

    for (POSITION_T position = line.start ; position < line.end ; position++)
    {
        if (mDocument->GetControlChar(position) == STYLE_TAB)
        {
            line.tabs.push_back(position) ;
        }
    }

    return true ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return false if the position's paragraph has no laid out line
///
/////////////////////////////////////////////////////////////////////////////
bool cEditorBase::ColumnLineAt(POSITION_T position, sColumnLine& line, size_t& lineIndex)
{
    PARAGRAPH_T para = mDocument->GetParagraphFromPosition(position) ;
    const sParagraphLayout* paraLayout = mLayout->GetParagraphLayout(para) ;
    if (paraLayout == nullptr || paraLayout->lines.empty())
    {
        return false ;
    }

    POSITION_T paraStart = 0, paraEnd = 0 ;
    mDocument->GetParagraphStartandEnd(para, paraStart, paraEnd) ;

    lineIndex = 0 ;
    for (size_t i = 0 ; i < paraLayout->lines.size() ; i++)
    {
        if (paraStart + paraLayout->lines[i].linestart <= position)
        {
            lineIndex = i ;
        }
    }
    return BuildColumnLine(para, lineIndex, line) ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return false without a marked block whose marker lines are laid out
///
/// @brief
/// Left and right edge (layout x of ^KB and ^KK) and the first and last
/// line of the rectangle.
///
/////////////////////////////////////////////////////////////////////////////
bool cEditorBase::GetColumnBounds(sColumnBounds& bounds)
{
    if (mDocument == nullptr || mLayout == nullptr || mDocument->mBlockSet == false)
    {
        return false ;
    }

    sColumnLine begin, end ;
    if (ColumnLineAt(mDocument->mStartBlock, begin, bounds.firstLine) == false ||
        ColumnLineAt(mDocument->mEndBlock, end, bounds.lastLine) == false)
    {
        return false ;
    }

    bounds.left = begin.XAt(mDocument->mStartBlock) ;
    bounds.right = end.XAt(mDocument->mEndBlock) ;
    bounds.firstPara = begin.paragraph ;
    bounds.lastPara = end.paragraph ;
    return true ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  lastPara [in] last paragraph a column command reads
///
/// @brief
/// Lays out, through lastPara, the paragraphs the layout has not reached
/// or that changed since the last layout. Once per command:
/// mColumnLaidOut is reset when a command starts and ends.
///
/////////////////////////////////////////////////////////////////////////////
void cEditorBase::LayoutForColumns(PARAGRAPH_T lastPara)
{
    if (mLayout == nullptr || mDocument == nullptr)
    {
        return ;
    }

    lastPara = std::min(lastPara, mDocument->GetNumberofParagraphs() - 1) ;
    if (lastPara <= mColumnLaidOut)
    {
        return ;
    }

    PARAGRAPH_T first = mColumnLaidOut + 1 ;
    if (mColumnLaidOut < 0)
    {
        first = lastPara + 1 ;
        if (mDocumentDirty)
        {
            first = std::min(first, mDirtyFromParagraph) ;
        }
        for (PARAGRAPH_T para = 0 ; para < first ; para++)
        {
            if (mLayout->IsParagraphLaidOut(para) == false)
            {
                first = para ;
                break ;
            }
        }
        first = std::max(first, PARAGRAPH_T(0)) ;
    }

    for (PARAGRAPH_T para = first ; para <= lastPara ; para++)
    {
        mLayout->LayoutParagraph(para) ;
    }
    mColumnLaidOut = lastPara ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  line  [in]  a line of the rectangle
/// @param  left  [in]  left edge, layout x
/// @param  right [in]  right edge, layout x
/// @param  row   [out] the line's part in the rectangle
///
/////////////////////////////////////////////////////////////////////////////
void cEditorBase::ColumnRowOfLine(const sColumnLine& line, COORD_T left, COORD_T right, sColumnRow& row)
{
    row.paragraph = line.paragraph ;
    row.lineStart = line.start ;
    row.lineEnd = line.end ;
    row.tabbed = (line.tabs.empty() == false) ;

    auto inside = [left, right](COORD_T x)
    {
        return x >= left - COLUMN_EPSILON && x < right - COLUMN_EPSILON ;
    } ;

    if (row.tabbed)
    {
        std::vector<POSITION_T> cellStarts, cellEnds ;
        LineCells(line.start, line.end, line.tabs, cellStarts, cellEnds) ;

        size_t first = cellStarts.size(), last = 0 ;
        for (size_t cell = 0 ; cell < cellStarts.size() ; cell++)
        {
            if (inside(line.XAt(cellStarts[cell])))
            {
                first = std::min(first, cell) ;
                last = cell ;
            }
        }

        if (first == cellStarts.size())
        {
            row.from = row.to = row.contentFrom = row.contentTo = line.end ;
            return ;
        }

        row.contentFrom = row.from = cellStarts[first] ;
        row.contentTo = row.to = cellEnds[last] ;
        if (first > 0)
        {
            row.from = cellStarts[first] - 1 ;
        }
        else if (last + 1 < cellStarts.size())
        {
            row.to = cellEnds[last] + 1 ;
        }
        return ;
    }

    POSITION_T first = NOT_SET, last = NOT_SET, after = line.end ;
    for (POSITION_T position = line.start ; position < line.end ; position++)
    {
        COORD_T x = line.XAt(position) ;
        if (after == line.end && x >= left - COLUMN_EPSILON)
        {
            after = position ;
        }
        if (inside(x))
        {
            if (first == NOT_SET)
            {
                first = position ;
            }
            last = position ;
        }
    }

    if (first == NOT_SET)
    {
        row.from = row.to = row.contentFrom = row.contentTo = after ;
        return ;
    }
    row.contentFrom = row.from = first ;
    row.contentTo = row.to = last + 1 ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  rows        [out] one row per layout line of the rectangle, dot
///                           command and comment lines left out
/// @param  layoutFirst [in]  lay out the block's paragraphs first
/// @param  badShape    [out] set true if ^KK is not right of ^KB
///
/// @return false if there is no column block
///
/////////////////////////////////////////////////////////////////////////////
bool cEditorBase::CollectColumnRows(std::vector<sColumnRow>& rows, bool layoutFirst, bool* badShape)
{
    rows.clear() ;
    if (badShape != nullptr)
    {
        *badShape = false ;
    }
    if (IsColumnBlock() == false || mLayout == nullptr)
    {
        return false ;
    }

    if (layoutFirst)
    {
        LayoutForColumns(mDocument->GetParagraphFromPosition(mDocument->mEndBlock)) ;
    }

    sColumnBounds bounds ;
    if (GetColumnBounds(bounds) == false)
    {
        return false ;
    }
    if (bounds.right <= bounds.left + COLUMN_EPSILON)
    {
        if (badShape != nullptr)
        {
            *badShape = true ;
        }
        return false ;
    }

    for (PARAGRAPH_T para = bounds.firstPara ; para <= bounds.lastPara ; para++)
    {
        const sParagraphLayout* paraLayout = mLayout->GetParagraphLayout(para) ;
        if (paraLayout == nullptr || paraLayout->isCommand || paraLayout->isComment)
        {
            continue ;
        }
        for (size_t index = 0 ; index < paraLayout->lines.size() ; index++)
        {
            if ((para == bounds.firstPara && index < bounds.firstLine) ||
                (para == bounds.lastPara && index > bounds.lastLine))
            {
                continue ;
            }
            sColumnLine line ;
            if (BuildColumnLine(para, index, line))
            {
                sColumnRow row ;
                ColumnRowOfLine(line, bounds.left, bounds.right, row) ;
                rows.push_back(row) ;
            }
        }
    }
    return true ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return the rows of the marked column block from the current layout;
///         empty without a column block
///
/////////////////////////////////////////////////////////////////////////////
std::vector<sColumnRow> cEditorBase::GetColumnRows(void)
{
    std::vector<sColumnRow> rows ;
    CollectColumnRows(rows, false) ;
    return rows ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  line [in]  a laid out line
/// @param  from [out] first position of the line in the column block
/// @param  to   [out] end of that part
///
/// @return false if no part of the line is in the column block
///
/// @brief
/// For the painters: the cells or characters of the line inside the
/// rectangle, separator tabs left out.
///
/////////////////////////////////////////////////////////////////////////////
bool cEditorBase::GetColumnLineRange(const sLineLayout& line, POSITION_T& from, POSITION_T& to)
{
    if (IsColumnBlock() == false || mLayout == nullptr)
    {
        return false ;
    }

    sColumnBounds bounds ;
    if (GetColumnBounds(bounds) == false || bounds.right <= bounds.left + COLUMN_EPSILON)
    {
        return false ;
    }

    PARAGRAPH_T para = mDocument->GetParagraphFromPosition(line.documentPosition) ;
    if (para < bounds.firstPara || para > bounds.lastPara)
    {
        return false ;
    }
    const sParagraphLayout* paraLayout = mLayout->GetParagraphLayout(para) ;
    if (paraLayout == nullptr || paraLayout->isCommand || paraLayout->isComment)
    {
        return false ;
    }

    for (size_t index = 0 ; index < paraLayout->lines.size() ; index++)
    {
        if (paraLayout->lines[index].linestart != line.linestart)
        {
            continue ;
        }
        if ((para == bounds.firstPara && index < bounds.firstLine) ||
            (para == bounds.lastPara && index > bounds.lastLine))
        {
            return false ;
        }
        sColumnLine columnLine ;
        if (BuildColumnLine(para, index, columnLine) == false)
        {
            return false ;
        }
        sColumnRow row ;
        ColumnRowOfLine(columnLine, bounds.left, bounds.right, row) ;
        from = row.contentFrom ;
        to = row.contentTo ;
        return from < to ;
    }
    return false ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  move [in] true for ^KV, false for ^KC
///
/// @brief
/// Copies or moves the column block to the cursor's x on the cursor line
/// and the lines below it; lines are added at the end of the document if
/// there are too few. A move whose target overlaps the block does nothing.
///
/////////////////////////////////////////////////////////////////////////////
void cEditorBase::ColumnCopyOrMove(bool move)
{
    CloseTypingGroup() ;
    mColumnLaidOut = -1 ;

    std::vector<sColumnRow> rows ;
    bool badShape = false ;
    if (CollectColumnRows(rows, true, &badShape) == false || rows.empty())
    {
        mColumnLaidOut = -1 ;
        if (badShape)
        {
            ShowError("Column Block", BAD_SHAPE_MESSAGE) ;
        }
        return ;
    }

    POSITION_T cursor = mDocument->GetPosition() ;
    LayoutForColumns(mDocument->GetParagraphFromPosition(cursor)) ;
    sColumnLine cursorLine ;
    size_t cursorIndex = 0 ;
    if (ColumnLineAt(cursor, cursorLine, cursorIndex) == false)
    {
        mColumnLaidOut = -1 ;
        return ;
    }
    COORD_T cursorX = cursorLine.XAt(cursor) ;

    // what each row brings
    std::vector<sColumnSource> sources ;
    for (const sColumnRow& row : rows)
    {
        sColumnSource source ;
        source.tabbed = row.tabbed ;
        source.content = mDocument->CaptureCharacters(row.contentFrom, row.contentTo - row.contentFrom) ;
        source.cells.emplace_back() ;
        for (const sUndoCharInfo& info : source.content)
        {
            if (info.codepoint == STYLE_TAB)
            {
                source.cells.emplace_back() ;
            }
            else
            {
                source.cells.back().push_back(info) ;
            }
        }
        if (row.from < row.contentFrom)
        {
            source.separator = mDocument->CaptureCharacters(row.from, 1).front() ;
        }
        else if (row.to > row.contentTo)
        {
            source.separator = mDocument->CaptureCharacters(row.contentTo, 1).front() ;
        }
        else
        {
            source.separator = PlainChar(STYLE_TAB) ;
        }
        sources.push_back(source) ;
    }

    // the target lines: the cursor line and those below, dot lines left out
    std::vector<sColumnLine> targets ;
    PARAGRAPH_T paraCount = mDocument->GetNumberofParagraphs() ;
    for (PARAGRAPH_T para = cursorLine.paragraph ; para < paraCount && targets.size() < rows.size() ; para++)
    {
        LayoutForColumns(para) ;
        const sParagraphLayout* paraLayout = mLayout->GetParagraphLayout(para) ;
        if (paraLayout == nullptr || paraLayout->isCommand || paraLayout->isComment)
        {
            continue ;
        }
        size_t index = (para == cursorLine.paragraph) ? cursorIndex : 0 ;
        for ( ; index < paraLayout->lines.size() && targets.size() < rows.size() ; index++)
        {
            sColumnLine line ;
            if (BuildColumnLine(para, index, line))
            {
                targets.push_back(line) ;
            }
        }
    }

    bool replace = mColumnReplace ;
    std::vector<sColumnEdit> edits ;
    size_t markStartEdit = SIZE_MAX, markEndEdit = SIZE_MAX ;
    POSITION_T markStartOffset = 0, markEndOffset = 0 ;

    auto addEdit = [&edits](POSITION_T pos, POSITION_T del, const std::vector<sUndoCharInfo>& ins)
    {
        sColumnEdit edit ;
        edit.pos = pos ;
        edit.del = del ;
        edit.ins = ins ;
        edit.seq = edits.size() ;
        edits.push_back(edit) ;
        return edits.size() - 1 ;
    } ;
    auto markStart = [&](size_t edit, POSITION_T offset)
    {
        if (markStartEdit == SIZE_MAX)
        {
            markStartEdit = edit ;
            markStartOffset = offset ;
        }
    } ;
    auto markEnd = [&](size_t edit, POSITION_T offset)
    {
        markEndEdit = edit ;
        markEndOffset = offset ;
    } ;
    auto spaceCount = [this](const std::string& font, COORD_T width)
    {
        COORD_T space = font.empty() ? mLayout->GetTextWidth(" ") : mLayout->GetTextWidth(" ", font) ;
        if (space <= 0 || width <= 0)
        {
            return POSITION_T(0) ;
        }
        return static_cast<POSITION_T>(std::lround(width / space)) ;
    } ;

    // into the target lines
    for (size_t j = 0 ; j < targets.size() ; j++)
    {
        const sColumnSource& source = sources[j] ;
        const sColumnLine& target = targets[j] ;
        if (source.content.empty())
        {
            continue ;
        }
        POSITION_T length = static_cast<POSITION_T>(source.content.size()) ;

        if (target.tabs.empty() == false)
        {
            std::vector<POSITION_T> cellStarts, cellEnds ;
            LineCells(target.start, target.end, target.tabs, cellStarts, cellEnds) ;

            size_t cell = 0 ;
            for (size_t k = 0 ; k < cellStarts.size() ; k++)
            {
                if (target.XAt(cellStarts[k]) <= cursorX + COLUMN_EPSILON)
                {
                    cell = k ;
                }
            }
            bool atCellStart = std::fabs(target.XAt(cellStarts[cell]) - cursorX) <= COLUMN_EPSILON ;

            if (replace)
            {
                std::vector<std::vector<sUndoCharInfo>> cells = source.tabbed ? source.cells :
                                                                std::vector<std::vector<sUndoCharInfo>>{ source.content } ;
                size_t first = atCellStart ? cell : cell + 1 ;
                std::vector<sUndoCharInfo> appended ;
                POSITION_T appendedStart = 0 ;
                for (size_t c = 0 ; c < cells.size() ; c++)
                {
                    size_t targetCell = first + c ;
                    if (targetCell < cellStarts.size())
                    {
                        size_t edit = addEdit(cellStarts[targetCell], cellEnds[targetCell] - cellStarts[targetCell], cells[c]) ;
                        if (c == 0)
                        {
                            markStart(edit, 0) ;
                        }
                        markEnd(edit, static_cast<POSITION_T>(cells[c].size())) ;
                    }
                    else
                    {
                        appended.push_back(PlainChar(STYLE_TAB)) ;
                        if (c == 0)
                        {
                            appendedStart = static_cast<POSITION_T>(appended.size()) ;
                        }
                        Append(appended, cells[c]) ;
                    }
                }
                if (appended.empty() == false)
                {
                    size_t edit = addEdit(target.end, 0, appended) ;
                    markStart(edit, appendedStart) ;
                    markEnd(edit, static_cast<POSITION_T>(appended.size())) ;
                }
                continue ;
            }

            std::vector<sUndoCharInfo> ins ;
            POSITION_T offset = 0 ;
            POSITION_T pos = atCellStart ? cellStarts[cell] : cellEnds[cell] ;
            if (source.tabbed && atCellStart == false)
            {
                ins.push_back(source.separator) ;
                offset = 1 ;
            }
            Append(ins, source.content) ;
            if (source.tabbed && atCellStart)
            {
                ins.push_back(source.separator) ;
            }
            size_t edit = addEdit(pos, 0, ins) ;
            markStart(edit, offset) ;
            markEnd(edit, offset + length) ;
            continue ;
        }

        // a line without tabs
        POSITION_T pos = target.end ;
        for (POSITION_T position = target.start ; position < target.end ; position++)
        {
            if (target.XAt(position) >= cursorX - COLUMN_EPSILON)
            {
                pos = position ;
                break ;
            }
        }
        std::vector<sUndoCharInfo> ins ;
        if (pos == target.end)
        {
            ins = Spaces(spaceCount(target.font, cursorX - target.endX)) ;
        }
        bool textLeft = (pos > target.start) || (ins.empty() == false) ;
        bool textRight = (pos < target.end) ;
        POSITION_T del = 0 ;
        if (replace)
        {
            del = std::min(target.end - pos, length) ;
            textRight = (pos + del < target.end) ;
        }
        if (source.tabbed && textLeft && replace == false)
        {
            ins.push_back(source.separator) ;
        }
        POSITION_T offset = static_cast<POSITION_T>(ins.size()) ;
        Append(ins, source.content) ;
        if (source.tabbed && textLeft == false && textRight && replace == false)
        {
            ins.push_back(source.separator) ;
        }
        size_t edit = addEdit(pos, del, ins) ;
        markStart(edit, offset) ;
        markEnd(edit, offset + length) ;
    }

    // rows below the last line of the document go on new lines
    if (targets.size() < rows.size())
    {
        const sColumnLine& last = targets.empty() ? cursorLine : targets.back() ;
        POSITION_T padding = spaceCount(last.font, cursorX - last.pagex) ;
        std::vector<sUndoCharInfo> ins ;
        size_t edit = edits.size() ;
        for (size_t j = targets.size() ; j < rows.size() ; j++)
        {
            ins.push_back(PlainChar(HARD_RETURN)) ;
            if (sources[j].content.empty())
            {
                continue ;
            }
            Append(ins, Spaces(padding)) ;
            if (markStartEdit == SIZE_MAX)
            {
                markStartEdit = edit ;
                markStartOffset = static_cast<POSITION_T>(ins.size()) ;
            }
            Append(ins, sources[j].content) ;
            markEndEdit = edit ;
            markEndOffset = static_cast<POSITION_T>(ins.size()) ;
        }
        addEdit(mDocument->GetTextSize() - 1, 0, ins) ;
    }

    // the hole the moved column leaves
    if (move)
    {
        for (size_t j = 0 ; j < rows.size() ; j++)
        {
            const sColumnRow& row = rows[j] ;
            if (row.from == row.to)
            {
                continue ;
            }
            if (replace == false)
            {
                addEdit(row.from, row.to - row.from, {}) ;
            }
            else if (row.tabbed)
            {
                POSITION_T cellStart = row.contentFrom ;
                for (POSITION_T position = row.contentFrom ; position <= row.contentTo ; position++)
                {
                    if (position == row.contentTo || mDocument->GetControlChar(position) == STYLE_TAB)
                    {
                        if (position > cellStart)
                        {
                            addEdit(cellStart, position - cellStart, {}) ;
                        }
                        cellStart = position + 1 ;
                    }
                }
            }
            else
            {
                addEdit(row.from, row.to - row.from, Spaces(VisibleCount(sources[j].content))) ;
            }
        }
    }

    mColumnLaidOut = -1 ;
    if (edits.empty() || markStartEdit == SIZE_MAX || EditsOverlap(edits))
    {
        return ;
    }

    mDocument->BeginUndoGroup() ;
    BeginBatchUpdate() ;

    ApplyEdits(mDocument, edits) ;

    mDocument->mStartBlock = edits[markStartEdit].finalPos + markStartOffset ;
    mDocument->mEndBlock = edits[markEndEdit].finalPos + markEndOffset ;
    mDocument->mBlockSet = mDocument->mEndBlock > mDocument->mStartBlock ;
    mDocument->mBlockHidden = false ;
    mDocument->SetPosition(mDocument->mStartBlock) ;
    mDocument->UnmarkBlockOnUndo() ;

    EndBatchUpdate() ;
    mDocument->EndUndoGroup() ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Deletes the column block. Column replace off closes the gap; on, the
/// cells are emptied and the tabs stay, and characters of a line without
/// tabs become one space each. The cursor goes to where the block began.
///
/////////////////////////////////////////////////////////////////////////////
void cEditorBase::ColumnDelete(void)
{
    CloseTypingGroup() ;
    mColumnLaidOut = -1 ;

    std::vector<sColumnRow> rows ;
    bool badShape = false ;
    bool found = CollectColumnRows(rows, true, &badShape) ;
    mColumnLaidOut = -1 ;
    if (found == false)
    {
        if (badShape)
        {
            ShowError("Column Block", BAD_SHAPE_MESSAGE) ;
        }
        return ;
    }

    std::vector<sColumnEdit> edits ;
    for (const sColumnRow& row : rows)
    {
        if (row.from == row.to)
        {
            continue ;
        }
        sColumnEdit edit ;
        edit.seq = edits.size() ;
        if (mColumnReplace == false)
        {
            edit.pos = row.from ;
            edit.del = row.to - row.from ;
            edits.push_back(edit) ;
        }
        else if (row.tabbed)
        {
            POSITION_T cellStart = row.contentFrom ;
            for (POSITION_T position = row.contentFrom ; position <= row.contentTo ; position++)
            {
                if (position == row.contentTo || mDocument->GetControlChar(position) == STYLE_TAB)
                {
                    if (position > cellStart)
                    {
                        edit.pos = cellStart ;
                        edit.del = position - cellStart ;
                        edit.seq = edits.size() ;
                        edits.push_back(edit) ;
                    }
                    cellStart = position + 1 ;
                }
            }
        }
        else
        {
            edit.pos = row.from ;
            edit.del = row.to - row.from ;
            edit.ins = Spaces(VisibleCount(mDocument->CaptureCharacters(row.from, edit.del))) ;
            edits.push_back(edit) ;
        }
    }

    if (edits.empty())
    {
        return ;
    }

    mDocument->BeginUndoGroup() ;
    BeginBatchUpdate() ;

    ApplyEdits(mDocument, edits) ;

    POSITION_T cursor = std::numeric_limits<POSITION_T>::max() ;
    for (const sColumnEdit& edit : edits)
    {
        cursor = std::min(cursor, edit.finalPos) ;
    }
    mDocument->UnsetBlock() ;
    mDocument->SetPosition(cursor) ;
    mDocument->UnmarkBlockOnUndo() ;

    EndBatchUpdate() ;
    mDocument->EndUndoGroup() ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  ascending [in] true for A-Z order
///
/// @brief
/// ^KZ with a column block: sorts the rows' whole lines, keyed by their
/// part in the rectangle. Dot command lines stay where they are.
///
/////////////////////////////////////////////////////////////////////////////
void cEditorBase::ColumnSort(bool ascending)
{
    CloseTypingGroup() ;
    mColumnLaidOut = -1 ;

    std::vector<sColumnRow> rows ;
    bool badShape = false ;
    bool found = CollectColumnRows(rows, true, &badShape) ;
    mColumnLaidOut = -1 ;
    if (found == false)
    {
        if (badShape)
        {
            ShowError("Column Block", BAD_SHAPE_MESSAGE) ;
        }
        return ;
    }

    std::vector<std::string> keys ;
    std::vector<std::vector<sUndoCharInfo>> lines ;
    for (const sColumnRow& row : rows)
    {
        keys.push_back(mDocument->GetPlainText(row.contentFrom, row.contentTo, "")) ;
        lines.push_back(mDocument->CaptureCharacters(row.lineStart, row.lineEnd - row.lineStart)) ;
    }

    std::vector<size_t> order(rows.size()) ;
    std::iota(order.begin(), order.end(), size_t(0)) ;
    std::stable_sort(order.begin(), order.end(), [&keys, ascending](size_t a, size_t b)
    {
        return ascending ? keys[a] < keys[b] : keys[a] > keys[b] ;
    }) ;

    std::vector<sColumnEdit> edits ;
    for (size_t slot = 0 ; slot < order.size() ; slot++)
    {
        if (order[slot] == slot)
        {
            continue ;
        }
        sColumnEdit edit ;
        edit.pos = rows[slot].lineStart ;
        edit.del = rows[slot].lineEnd - rows[slot].lineStart ;
        edit.ins = lines[order[slot]] ;
        edit.seq = edits.size() ;
        edits.push_back(edit) ;
    }
    if (edits.empty())
    {
        return ;
    }

    POSITION_T start = mDocument->mStartBlock ;
    POSITION_T end = mDocument->mEndBlock ;

    mDocument->BeginUndoGroup() ;
    BeginBatchUpdate() ;

    ApplyEdits(mDocument, edits) ;

    mDocument->mStartBlock = MapPosition(edits, start) ;
    mDocument->mEndBlock = MapPosition(edits, end) ;
    mDocument->mBlockSet = mDocument->mEndBlock > mDocument->mStartBlock ;
    mDocument->SetPosition(mDocument->mEndBlock) ;
    mDocument->UnmarkBlockOnUndo() ;

    EndBatchUpdate() ;
    mDocument->EndUndoGroup() ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return the column block as plain lines joined with LF
///
/////////////////////////////////////////////////////////////////////////////
std::string cEditorBase::ColumnClipboardText(void)
{
    mColumnLaidOut = -1 ;
    std::vector<sColumnRow> rows ;
    CollectColumnRows(rows, true) ;
    mColumnLaidOut = -1 ;

    std::string text ;
    for (size_t i = 0 ; i < rows.size() ; i++)
    {
        if (i > 0)
        {
            text += "\n" ;
        }
        text += mDocument->GetPlainText(rows[i].contentFrom, rows[i].contentTo, "\n") ;
    }
    return text ;
}
