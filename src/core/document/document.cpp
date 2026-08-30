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
 * @class cDocument
 *
 * @brief Core document model with paragraph-based UTF-8 storage and grapheme positioning.
 *
 * Implements the cDocument class, which is the central data model for all text
 * content in WordTsar. Stores text as paragraphs of UTF-8 bytes but exposes a
 * grapheme-based positioning API (POSITION_T counts user-visible characters, not
 * bytes). Provides insertion, deletion, and retrieval of text; formatting attributes
 * (bold, italic, underline, strikethrough, super/subscript); font, color, tab, and
 * variable management via sorted pair tables; block selection with begin/end markers;
 * multi-level undo/redo with grouped operations; clipboard copy/cut/paste; text
 * search (forward/backward with wildcard and whole-word support); and a listener
 * interface (cDocumentListener) for change notifications to editors and layouts.
 *
 * @section document_storage Storage Architecture
 * Text is stored as a vector of sParagraphData structures, each holding a UTF-8
 * byte buffer, a cGraphemeOffsets table for efficient grapheme-to-byte mapping,
 * and per-paragraph attribute tables: format modifiers, fonts, tabs, colors,
 * footnotes, endnotes, and variables -- all stored as sorted pair vectors keyed
 * by document position. Paragraphs are delimited by carriage return (0x0D)
 * characters. The document always ends with an immutable EOF marker (Ctrl-Z,
 * 0x1A).
 *
 * @section document_positioning Grapheme-Based Positioning
 * All public API positions use POSITION_T, which represents grapheme count from
 * the start of the document. Internally, the document converts grapheme positions
 * to paragraph index + byte offset using cGraphemeOffsets. Multi-byte UTF-8
 * sequences, combining characters, and emoji sequences each count as one grapheme.
 *
 * @section document_formatting Formatting Attributes
 * Formatting is stored per paragraph inside each sParagraphData as position-keyed
 * sorted pair vectors: each attribute (bold, italic, font, color, tab, etc.) maps
 * a document position to an on/off toggle or a value. Insert and delete operations
 * maintain attribute positions via IncrementAttributes() and DecrementAttributes().
 * When a paragraph is split (e.g., by inserting a hard return),
 * TransferAttributesToNewParagraph() moves attributes at or beyond the split
 * point from the original paragraph into the new one.
 *
 * @section document_undo Undo/Redo System
 * Uses grouped undo actions (sUndoGroup containing sUndoAction entries).
 * Each action records the operation type (eUndoActionType), position, and
 * data needed to reverse it. Typing sequences are automatically coalesced
 * into single undo groups. Undo/redo can be suppressed during file loading
 * (mIsLoading) and batch operations (mUndoDisabled).
 *
 * @section document_notifications Change Notifications
 * Registered cDocumentListener instances receive OnDocumentChanged() calls
 * after mutations and OnDocumentCleared() after Clear(). Notifications are
 * suppressed during file loading (mIsLoading) and batch paste (mSuppressNotify).
 *
 * @section document_search Text Search
 * Forward and backward search with support for case-insensitive matching,
 * whole-word boundaries, and simple wildcard patterns. Search operates on
 * grapheme positions and returns results as POSITION_T ranges.
 *
 * @author Gerald Brandt
 * @copyright GNU Affero General Public License v3.0
 *
 * @see cDocumentListener Change notification interface
 * @see sParagraphData Per-paragraph storage structure
 * @see cGraphemeOffsets Grapheme-to-byte offset mapping
 * @see sUndoAction Single undo operation record
 * @see sUndoGroup Grouped undo operations
 * @see eUndoActionType Undo action type enumeration
 */

#include <math.h>
#include <string.h>
#include <cstdio>

#include <string>
#include <algorithm>
#include <thread>
#include <regex>

#include "unicodelib.h"
#include "unicodelib_encodings.h"

#include "document.h"
#include "src/core/include/config.h"


//#include "src/files/rtf/read/rtfparser.h"           // just for sColorTable


//#include "../../third-party/parallel-for/parallel-for.h"

// base wordstar colors
sSeqRGBColor gBaseWSColors[] =
{
    {   0,   0,   0, 255 },             // black
    {   0,   0, 170, 255 },             // blue
    {   0, 170,   0, 255 },             // green
    {   0, 170, 170, 255 },             // cyan
    { 170,   0,   0, 255 },             // red
    { 170,   0, 170, 255 },             // magenta
    { 170,  85,   0, 255 },             // brown
    { 170, 170, 170, 255 },             // light gray
    {  85,  85,  85, 255 },             // dark gray
    {  85,  85, 255, 255 },             // light blue
    {  85, 255,  85, 255 },             // light green
    {  85, 255, 255, 255 },             // light cyan
    { 255,  85,  85, 255 },             // light red
    { 255,  85, 255, 255 },             // light magenta
    { 255, 255,  85, 255 },             // yellow
    { 155, 255, 255, 255 },             // white
} ;


/////////////////////////////////////////////////////////////////////////////
///
/// @param  first  [in] left-hand PairTable element
/// @param  second [in] right-hand PairTable element
///
/// @return bool - true if first's position is less than second's
///
/// @brief
/// Comparator for sorted array of PairTable (position, eType) entries.
/// Used by std::lower_bound and std::sort to keep pairs ordered by
/// document position.
///
/////////////////////////////////////////////////////////////////////////////
bool TableCompare(PairTable const & first, PairTable const & second)
{
    return first.first < second.first;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  first  [in] left-hand TabPair element
/// @param  second [in] right-hand TabPair element
///
/// @return bool - true if first's position is less than second's
///
/// @brief
/// Comparator for sorted array of TabPair (position, sWSTab) entries
///
/////////////////////////////////////////////////////////////////////////////
bool TabCompare(TabPair const & first, TabPair const & second)
{
    return first.first < second.first;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  first  [in] left-hand FormatPair element
/// @param  second [in] right-hand FormatPair element
///
/// @return bool - true if first's position is less than second's
///
/// @brief
/// Comparator for sorted array of FormatPair (position, eModifiers) entries
///
/////////////////////////////////////////////////////////////////////////////
bool FormatCompare(const FormatPair &first, const FormatPair &second)
{
    return first.first < second.first;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  first  [in] left-hand ColorPair element
/// @param  second [in] right-hand ColorPair element
///
/// @return bool - true if first's position is less than second's
///
/// @brief
/// Comparator for sorted array of ColorPair (position, sSeqRGBColor) entries
///
/////////////////////////////////////////////////////////////////////////////
bool ColorCompare(const ColorPair &first, const ColorPair &second)
{
    return first.first < second.first ;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  first  [in] left-hand FontPair element
/// @param  second [in] right-hand FontPair element
///
/// @return bool - true if first's position is less than second's
///
/// @brief
/// Comparator for sorted array of FontPair (position, sInternalFonts) entries
///
/////////////////////////////////////////////////////////////////////////////
bool FontCompare(const FontPair &first, const FontPair &second)
{
    return first.first < second.first ;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  first  [in] left-hand FootnotePair element
/// @param  second [in] right-hand FootnotePair element
///
/// @return bool - true if first's position is less than second's
///
/// @brief
/// Comparator for sorted array of FootnotePair (position, sNote) entries
///
/////////////////////////////////////////////////////////////////////////////
bool FootnoteCompare(const FootnotePair &first, const FootnotePair &second)
{
    return first.first < second.first ;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  first  [in] left-hand EndnotePair element
/// @param  second [in] right-hand EndnotePair element
///
/// @return bool - true if first's position is less than second's
///
/// @brief
/// Comparator for sorted array of EndnotePair (position, sNote) entries
///
/////////////////////////////////////////////////////////////////////////////
bool EndnoteCompare(const EndnotePair &first, const EndnotePair &second)
{
    return first.first < second.first ;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  first  [in] left-hand VariablePair element
/// @param  second [in] right-hand VariablePair element
///
/// @return bool - true if first's position is less than second's
///
/// @brief
/// Comparator for sorted array of VariablePair (position, eVariableType) entries
///
/////////////////////////////////////////////////////////////////////////////
bool VariableCompare(const VariablePair &first, const VariablePair &second)
{
    return first.first < second.first;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  first  [in] left-hand sParagraphData element
/// @param  second [in] right-hand sParagraphData element
///
/// @return bool - true if first's start index is less than second's
///
/// @brief
/// Comparator for sorted array of sParagraphData entries.
/// Orders paragraphs by their starting position in the document.
///
/////////////////////////////////////////////////////////////////////////////
bool ParagraphCompare(const sParagraphData &first, const sParagraphData &second)
{
    return first.index < second.index ;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  t [in] double value
///
/// @return double - absolute value of t
///
/// @brief
/// Return the absolute value of a double
///
/////////////////////////////////////////////////////////////////////////////
double myabs(const double &t)
{
    return t >= 0 ? t : -t;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  a [in] first value to compare
/// @param  b [in] second value to compare
///
/// @return bool - true if a and b are approximately equal
///
/// @brief
/// Compare two doubles for approximate equality using relative epsilon.
/// The difference scaled by 10^12 must be less than or equal to the
/// larger of the two absolute values. Using the larger magnitude keeps
/// the comparison well-behaved when one operand is zero.
///
/////////////////////////////////////////////////////////////////////////////
bool FuzzyCompare(double a, double b)
{
    return (myabs(a - b) * 1000000000000. <= std::max(myabs(a), myabs(b)));
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Constructor. Initializes the document by calling Clear() and
/// resetting the changed flag.
///
/////////////////////////////////////////////////////////////////////////////
cDocument::cDocument(void)
{
    Clear();
    mChanged = false ;
/*
    int numthreads = static_cast<int>(std::thread::hardware_concurrency()) ;
    if(numthreads == 0)
    {
        numthreads = 4 ;
    }
    mMaxThreads = numthreads ;
*/    
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Destructor.
///
/////////////////////////////////////////////////////////////////////////////
cDocument::~cDocument(void)
{
//dtor
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  cDocumentListener* listener [in] listener to register
///
/// @return nothing
///
/// @brief
/// Registers a listener to receive document change notifications.
/// The listener is NOT owned by the document -- caller manages lifetime.
/// The listener must call RemoveListener() before being destroyed.
///
/////////////////////////////////////////////////////////////////////////////
void cDocument::AddListener(cDocumentListener* listener)
{
    mListeners.push_back(listener) ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  cDocumentListener* listener [in] listener to deregister
///
/// @return nothing
///
/// @brief
/// Removes a previously registered listener. Safe to call if listener
/// was never registered (no-op).
///
/////////////////////////////////////////////////////////////////////////////
void cDocument::RemoveListener(cDocumentListener* listener)
{
    mListeners.erase(
        std::remove(mListeners.begin(), mListeners.end(), listener),
        mListeners.end()) ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  PARAGRAPH_T fromParagraph [in] first paragraph affected
///
/// @return nothing
///
/// @brief
/// Notifies all registered listeners that document content changed.
/// Suppressed during file loading and undo/redo replay -- those cases
/// are handled explicitly by the GUI layer.
///
/////////////////////////////////////////////////////////////////////////////
void cDocument::NotifyChanged(PARAGRAPH_T fromParagraph)
{
    // suppress during loading, undo/redo, or batch paste
    if (mIsLoading || mUndoDisabled || mSuppressNotify)
    {
        return ;
    }

    for (auto* listener : mListeners)
    {
        listener->OnDocumentChanged(fromParagraph) ;
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Notifies all registered listeners that the document was cleared.
/// Always fires (not suppressed by loading/undo flags) because Clear()
/// is a complete reset that all listeners must know about.
///
/////////////////////////////////////////////////////////////////////////////
void cDocument::NotifyCleared(void)
{
    for (auto* listener : mListeners)
    {
        listener->OnDocumentCleared() ;
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Clear the document completely.
///
/////////////////////////////////////////////////////////////////////////////
void cDocument::Clear(void)
{
    // clear tabs, color, etc tables
    mParagraphData.clear();

    mRedrawFullDisplay = true;
    mIsLoading = false;
    mSuppressNotify = false ;
    mSuppressUndo = false ;

    // initialize undo/redo state
    mUndoStack.clear() ;
    mRedoStack.clear() ;
    mUndoGroupOpen = false ;
    mCurrentGroup = sUndoGroup{} ;
    mUndoGroupNesting = 0 ;
    mUndoDisabled = false ;

    sParagraphData pdata;
    pdata.index = 0;
    mParagraphData.push_back(pdata);

    mChanged = false;

    mLastParagraphFromPosition = 0;
    mLastParagraphFromPositionResult = 0;
    mLastNumParagraph = 0;

    SetPosition(0);

    mUndoDisabled = true ;
    Insert(STYLE_EOF) ;
    mUndoDisabled = false ;
    SetPosition(0);

    mBlockSet = false ;
    mStartBlock = NOT_SET ;
    mEndBlock = NOT_SET ;

    mOldBlockSet = false ;
    mOldStartBlock = NOT_SET ;
    mOldEndBlock = NOT_SET ;

    mCPParagraph = 1024 ;           // random number just so it's no 0 to start (see GetParagraphCodepoints())

    mWordCount = 0 ;

    for(int loop = 0; loop < 10; loop++)
    {
        mSavePosition[loop] = NOT_SET ;
    }

    NotifyCleared() ;
}



/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Shrink all internal vectors and paragraph buffers to fit their
/// current contents. Reduces memory usage after large deletions.
///
/////////////////////////////////////////////////////////////////////////////
void cDocument::ShrinkToFit(void)
{
    mParagraphData.shrink_to_fit() ;
    mUndoStack.shrink_to_fit() ;
    mRedoStack.shrink_to_fit() ;

    size_t len = mParagraphData.size() ;
    for(size_t loop = 0; loop < len; loop++)
    {
        mParagraphData[loop].buffer.shrink_to_fit() ;
        mParagraphData[loop].pairs.shrink_to_fit() ;
        mParagraphData[loop].format.shrink_to_fit() ;
        mParagraphData[loop].font.shrink_to_fit() ;
        mParagraphData[loop].tab.shrink_to_fit() ;
        mParagraphData[loop].color.shrink_to_fit() ;
        mParagraphData[loop].footnote.shrink_to_fit() ;
        mParagraphData[loop].endnote.shrink_to_fit() ;
        mParagraphData[loop].variable.shrink_to_fit() ;
        mParagraphData[loop].offsets.shrink_to_fit() ;
    }
}



/////////////////////////////////////////////////////////////////////////////
///
/// @param  char   [IN] - CHAR_T charcater to insert into buffer
///
/// @return nothing
///
/// @brief
/// This will insert charcaters into the text buffer and build all the
/// tables required for special characters
///
/////////////////////////////////////////////////////////////////////////////
void cDocument::Insert(CHAR_T ch)
{
    mChanged = true ;

    // save original codepoint before it gets converted to MARKER_CHAR
    CHAR_T originalCodepoint = ch ;
    POSITION_T cursorBefore = mCurrentPosition ;

    // convert the CHAR_T codepoint into a utf8 array
    std::string utf8 ;
    int ulen = unicode::utf8::encode_codepoint(ch, utf8) ;
    POSITION_T pos = GetPosition() ;
    POSITION_T parapos = 0;
    PARAGRAPH_T currentparagraphnumber = GetParagraphFromPosition(pos) ;
    auto paraiter = mParagraphData.begin() + currentparagraphnumber ;

    MY_ASSERT(pos >= 0)
    MY_ASSERT(currentparagraphnumber >= 0)

    parapos = mCurrentPosition - paraiter->index;
    std::vector<POSITION_T> offsets;
    size_t len = GetParagraphGraphemeOffsets(currentparagraphnumber, offsets);
    if(len == 0)
    {
        offsets.push_back(0);
    }

    // if this is a formatting character, deal with it
    // we insert MARKER_CHAR into the buffer, and read our control char tables to figure out what to do
    // because CP437 uses these values as well
    // REPLACE_CHAR (block-begin marker) and SAVE_CHAR (^K0..^K9 bookmark) stay
    // as raw bytes in the buffer
    bool isControl = (ch <= STYLE_END_OF_STYLES) && (ch != HARD_RETURN) && (ch != SPACE) && (ch != REPLACE_CHAR) && (ch != SAVE_CHAR) ;
    if(isControl)
    {
        // A control char is always exactly one new grapheme. Shift attribute
        // positions up first to make room, then insert the new control attribute.
        IncrementAttributes(pos) ;
        SetControlChar(ch) ;
        ch = MARKER_CHAR;
        utf8[0] = MARKER_CHAR ;
    }

    // insert our utf8 array into the buffer
    // index against offsets.size(): parapos == offsets.size() means end of buffer
    POSITION_T index ;
    if(static_cast<size_t>(parapos) < offsets.size())
    {
        index = offsets[static_cast<size_t>(parapos)] ;
    }
    else
    {
        index = static_cast<POSITION_T>(paraiter->buffer.size()) ;
    }

    if(index < CAST_POSITION_SIZE(paraiter->buffer.size()))
    {
        for(int loop = 0; loop < ulen; loop++)
        {
            auto bstart = paraiter->buffer.begin() ;
            paraiter->buffer.insert(bstart + index, utf8[loop]) ;
            index++ ;
        }
    }
    else
    {
        for(int loop = 0; loop < ulen; loop++)
        {
            paraiter->buffer.push_back(utf8[loop]) ;
            index++ ;
        }
    }

    // refresh our grapheme offsets
    SaveOffsets(currentparagraphnumber, offsets);

    // A text codepoint that did not add a grapheme is a combining mark or a
    // joiner (e.g. e + U+0301, a ZWJ emoji sequence, a flag). Re-normalize the
    // paragraph so the in-memory document stays NFC (e.g. compose e+U+0301 -> é).
    if (!isControl && offsets.size() == len)
    {
        paraiter->buffer = Normalize(paraiter->buffer) ;
        SaveOffsets(currentparagraphnumber, offsets);
    }

    // A new grapheme emerged: advance the cursor, and (for text) shift the
    // attribute positions that follow. Control chars already shifted above.
    if (len < offsets.size())
    {
        mCurrentPosition++;
        if (!isControl)
        {
            IncrementAttributes(pos) ;
        }
    }

    // deal with a hard return to create paragraphs
    if (ch == HARD_RETURN)
    {
        parapos = mCurrentPosition - paraiter->index;

        // index against offsets.size(): parapos == offsets.size() means split at
        // the end of the buffer
        MY_ASSERT(parapos >= 0)
        MY_ASSERT(static_cast<size_t>(parapos) < offsets.size())

        POSITION_T splitOffset ;
        if(static_cast<size_t>(parapos) < offsets.size())
        {
            splitOffset = offsets[static_cast<size_t>(parapos)] ;
        }
        else
        {
            splitOffset = static_cast<POSITION_T>(paraiter->buffer.size()) ;
        }

        InsertParagraph(mCurrentPosition, splitOffset, currentparagraphnumber);
    }

    // record undo action for the insert
    if (!mUndoDisabled && !mIsLoading && !mSuppressUndo)
    {
        sUndoCharInfo charInfo ;
        charInfo.codepoint = originalCodepoint ;
        std::vector<sUndoCharInfo> chars ;
        chars.push_back(charInfo) ;
        RecordAction(UNDO_ACTION_INSERT, cursorBefore, 1, chars, cursorBefore) ;
    }

    // notify listeners of document content change
    NotifyChanged(GetParagraphFromPosition(mCurrentPosition)) ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  text   [IN] - stl string to insert into buffer
///
/// @return nothing
///
/// @brief
/// insert an stl string into the buffer. Normalizes (NFC) the string first.
///
/////////////////////////////////////////////////////////////////////////////
void cDocument::Insert(const std::string &text)
{
    std::u32string codepoints ;

    std::string ntext = Normalize(text) ;

    GetCodePoints(ntext, codepoints) ;

    // save state for undo recording
    bool shouldRecord = !mUndoDisabled && !mIsLoading && !mSuppressUndo ;
    POSITION_T insertPosition = mCurrentPosition ;
    POSITION_T cursorBefore = mCurrentPosition ;

    // Batch the per-character inserts into ONE undo action and ONE notification.
    // Use mSuppressUndo (not mIsLoading) so file-loading semantics are not falsely
    // implied, and mSuppressNotify so the per-char NotifyChanged calls are coalesced
    // into the single notification fired below.
    bool oldSuppressUndo = mSuppressUndo ;
    bool oldSuppressNotify = mSuppressNotify ;
    mSuppressUndo = true ;
    mSuppressNotify = true ;

    for(size_t loop = 0; loop < codepoints.size() ; ++loop)
    {
        if(codepoints[loop] == 10)
        {
            Insert(static_cast<CHAR_T>(HARD_RETURN)) ;
        }
        else
        {
            Insert(static_cast<CHAR_T>(codepoints[loop])) ;
        }
    }

    mSuppressUndo = oldSuppressUndo ;
    mSuppressNotify = oldSuppressNotify ;

    // record one combined undo action for the entire string insert
    if (shouldRecord && !codepoints.empty())
    {
        std::vector<sUndoCharInfo> chars ;
        for (size_t loop = 0 ; loop < codepoints.size() ; ++loop)
        {
            sUndoCharInfo charInfo ;
            if (codepoints[loop] == 10)
            {
                charInfo.codepoint = HARD_RETURN ;
            }
            else
            {
                charInfo.codepoint = codepoints[loop] ;
            }
            chars.push_back(charInfo) ;
        }
        RecordAction(UNDO_ACTION_INSERT, insertPosition,
                     static_cast<POSITION_T>(codepoints.size()), chars, cursorBefore) ;
    }

    // notify listeners of document content change
    // (mIsLoading was restored above, so this fires only for normal edits)
    NotifyChanged(GetParagraphFromPosition(mCurrentPosition)) ;
}



/////////////////////////////////////////////////////////////////////////////
///
/// @param  position   [IN] - the current cursor position in the editor
/// @param  length     [IN] - the length of text we are deleting
///
/// @return bool - true on success
///
/// @brief
/// delete text from the editor (grapheme based)
///
/////////////////////////////////////////////////////////////////////////////
bool cDocument::Delete(POSITION_T position, POSITION_T length)
{
    POSITION_T bsize = GetTextSize() ;
    // nothing to delete (can't delete STYLE_EOF)
    if (bsize <= 1)
    {
        return false;
    }

    if(position >= bsize - 1)
    {
        return false ;
    }


    MY_ASSERT(length <= GetTextSize() - position)
    MY_ASSERT(position >= 0)
    MY_ASSERT(length > 0)

    // capture characters before deletion for undo recording
    bool shouldRecord = !mUndoDisabled && !mIsLoading ;
    POSITION_T cursorBefore = mCurrentPosition ;
    std::vector<sUndoCharInfo> capturedChars ;
    if (shouldRecord)
    {
        capturedChars = CaptureCharacters(position, length) ;
    }

    mChanged = true ;

    mCurrentPosition = position ;

    for(POSITION_T ctr = position + length -1; ctr >= position; ctr--)
    {

        std::string grapheme = GetChar(position);

        // A control marker is stored in the buffer as MARKER_CHAR; decode it to
        // its STYLE_ code so the dispatch below removes the matching pairs/table
        // entry instead of orphaning it.
        CHAR_T code = static_cast<CHAR_T>(grapheme[0]);
        if (grapheme.length() == 1 && code == MARKER_CHAR)
        {
            code = static_cast<CHAR_T>(GetControlChar(position));
        }

        if (code < STYLE_END_OF_STYLES && grapheme.length() == 1)
        {
            switch (code)
            {
                case STYLE_TAB:
                    DeleteTab(position);
                    break;

                case STYLE_FONT1:
                    DeleteFont(position);
                    break;

                case STYLE_INTERNAL_COLOR:
                    DeleteColor(position);
                    break;

                case STYLE_VARIABLE:
                    DeleteVariable(position);
                    break;

                case 13: // HARD_RETURN :
                    DeleteParagraph(position);
                    break;

                default:
                    DeleteControlChar(position);
                    break;
            }
        }

        if(grapheme[0] != HARD_RETURN)
        {
            PARAGRAPH_T currentparagraphnumber = GetParagraphFromPosition(position);
            auto paraiter = mParagraphData.begin() + currentparagraphnumber ;
            POSITION_T buffer_position = position - paraiter->index;        // makeposition relative to paragraph start

            std::vector<POSITION_T> offsets;
            size_t len = GetParagraphGraphemeOffsets(currentparagraphnumber, offsets); //  GraphemeCount(paraiter->buffer, offsets);
            if (len == 0)
            {
                offsets.push_back(0);
            }

            MY_ASSERT(buffer_position >= 0)
            MY_ASSERT(static_cast<size_t>(buffer_position) < offsets.size())

            auto bstart = paraiter->buffer.begin() + offsets[static_cast<size_t>(buffer_position)] ;
            size_t end ; 
            if(grapheme.size() == 0)
            {
                end = 1 ;
            }
            else
            {
                end = grapheme.size() ;
            }
            paraiter->buffer.erase(bstart, bstart + end) ;

            // refresh our grapheme offsets
            SaveOffsets(currentparagraphnumber, offsets);

            DecrementAttributes(position, 1) ;
        }
    }

    // record undo action for the delete
    if (shouldRecord && !capturedChars.empty())
    {
        RecordAction(UNDO_ACTION_DELETE, position, length, capturedChars, cursorBefore) ;
    }

    // notify listeners of document content change
    NotifyChanged(GetParagraphFromPosition(position)) ;

    return true ; // retval ;
}



/////////////////////////////////////////////////////////////////////////////
///
/// @param  position   [IN] - the grapheme position in the buffer
///
/// @return string - the grapheme at position
///
/// @brief
/// Returns the raw grapheme at the given position. MARKER_CHAR bytes
/// are returned as-is -- callers that need the underlying control code
/// should call GetControlChar() separately.
///
/////////////////////////////////////////////////////////////////////////////
std::string cDocument::GetChar(POSITION_T &position)
{
    std::string grapheme;
    POSITION_T textsize = GetTextSize();

    MY_ASSERT(position >= 0)

    // if the position requested is not in our buffer
    if (position >= textsize)
    {
        return grapheme;
    }

    PARAGRAPH_T currentparagraphnumber = GetParagraphFromPosition(position);
    auto paraiter = mParagraphData.begin() + currentparagraphnumber;
    POSITION_T newposition = position - paraiter->index; // make position relative to paragraph start

    size_t len = paraiter->offsets.size();

    // this should never happen, but let's test for it anyway. Safety first.
    if (newposition >= static_cast<POSITION_T>(len))
    {
        return grapheme;
    }

    MY_ASSERT(newposition >= 0)
    MY_ASSERT(newposition < static_cast<POSITION_T>(paraiter->offsets.size()))
    MY_ASSERT(newposition + 1 <= static_cast<POSITION_T>(paraiter->offsets.size()))

    // calculate byte range for this grapheme
    POSITION_T index = paraiter->offsets[static_cast<size_t>(newposition)];
    POSITION_T glength;
    if (static_cast<size_t>(newposition) < len - 1)
    {
        glength = paraiter->offsets[static_cast<size_t>(newposition + 1)] - index;
    }
    else
    {
        glength = static_cast<POSITION_T>(paraiter->buffer.size()) - index;
    }

    MY_ASSERT(index < static_cast<POSITION_T>(paraiter->buffer.size()))

    // extract the grapheme bytes from the paragraph buffer
    grapheme.assign(paraiter->buffer.begin() + index, paraiter->buffer.begin() + index + glength);

    return grapheme;
}


/* OLD GetChar -- kept for reference during testing, delete after verification
/////////////////////////////////////////////////////////////////////////////
///
/// @param  position   [IN/OUT] - the position in the buffer to get grapheme from
///
/// @return string - the grapheme at position
///
/// @brief
/// if mShowControl is not SHOW_ALL, then the next showable character
/// is returned and position is incremented to the right value
///
/// @todo This issumes the end of a paragraph is not a control code
/////////////////////////////////////////////////////////////////////////////
std::string cDocument::GetChar(POSITION_T &position)
{
    std::string grapheme;
    POSITION_T textsize = GetTextSize();

    MY_ASSERT(position >= 0)

    // if the position requested is not in our buffer
    if (position >= textsize)
    {
        return grapheme;
    }

    // we use a do loop to get past control chars if we don't want them
    do
    {
        PARAGRAPH_T currentparagraphnumber = GetParagraphFromPosition(position);
        auto paraiter = mParagraphData.begin() + currentparagraphnumber;
        POSITION_T newposition = position - paraiter->index; // make position relative to paragraph start

        size_t len = paraiter->offsets.size();

        // this should never happen, but let's test for it anyway. Safety first.
        if (newposition >= static_cast<POSITION_T>(len))
        {
            break;
        }

        MY_ASSERT(newposition >= 0)
        MY_ASSERT(newposition < static_cast<POSITION_T>(paraiter->offsets.size()))
        MY_ASSERT(newposition + 1 <= static_cast<POSITION_T>(paraiter->offsets.size()))

        POSITION_T index = paraiter->offsets[static_cast<size_t>(newposition)];
        POSITION_T glength;
        if (static_cast<size_t>(newposition) < len - 1)
        {
            glength = paraiter->offsets[static_cast<size_t>(newposition + 1)] - index;
        }
        else
        {
            glength = static_cast<POSITION_T>(paraiter->buffer.size()) - index;
        }

        MY_ASSERT(index < static_cast<POSITION_T>(paraiter->buffer.size()))

        grapheme.assign(paraiter->buffer.begin() + index, paraiter->buffer.begin() + index + glength);

        // if this is a MARKER_CHAR then get the control code for this position
        char ch = grapheme[0];

        if (ch == MARKER_CHAR)
        {
            char ch1 = GetControlChar(position);

            // if we are showing control characters or if this is a tab, show the control character
            if (mShowControl == SHOW_ALL || ch1 == STYLE_TAB || ch1 == STYLE_VARIABLE)
            {
                grapheme[0] = ch1;
                break;
            }
        }
        else
        {
            break;
        }

        // if this is a control char, and we are not showing them, then skip to next position
        position++;
    } while (false);

    return grapheme;
}
*/




/////////////////////////////////////////////////////////////////////////////
///
/// @param  POSITION_T position [in] grapheme position to read
///
/// @return the grapheme string at the given position
///
/// @brief
/// Get the character at a position without advancing the cursor.
/// Delegates to GetChar().
///
/////////////////////////////////////////////////////////////////////////////
std::string cDocument::GetCharNoAdvance(POSITION_T position)
{
    return GetChar(position);
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Toggle Boldface
///
/////////////////////////////////////////////////////////////////////////////
void cDocument::BeginBold(void)
{
    Insert(STYLE_BOLD) ;
    mRedrawFullDisplay = true ;         // make whole screen redraw, since this will toggle any text after it
}



/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Toggle Boldface
///
/////////////////////////////////////////////////////////////////////////////
void cDocument::EndBold(void)
{
    BeginBold() ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Toggle Italics
///
/////////////////////////////////////////////////////////////////////////////
void cDocument::BeginItalics(void)
{
    Insert(STYLE_ITALICS) ;
    mRedrawFullDisplay = true ;         // make whole screen redraw, since this will toggle any text after it
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Toggle Italics
///
/////////////////////////////////////////////////////////////////////////////
void cDocument::EndItalics(void)
{
    BeginItalics() ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Toggle Underline
///
/////////////////////////////////////////////////////////////////////////////
void cDocument::BeginUnderline(void)
{
    Insert(STYLE_UNDERLINE) ;
    mRedrawFullDisplay = true ;         // make whole screen redraw, since this will toggle any text after it
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Toggle Underline
///
/////////////////////////////////////////////////////////////////////////////
void cDocument::EndUnderline(void)
{
    BeginUnderline() ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Toggle Strikethrough
///
/////////////////////////////////////////////////////////////////////////////
void cDocument::BeginStrikeThrough(void)
{
    Insert(STYLE_STRIKETHROUGH) ;
    mRedrawFullDisplay = true ;         // make whole screen redraw, since this will toggle any text after it
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Toggle Strikethrough
///
/////////////////////////////////////////////////////////////////////////////
void cDocument::EndStrikeThrough(void)
{
    BeginStrikeThrough() ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Toggle Superscript
///
/////////////////////////////////////////////////////////////////////////////
void cDocument::BeginSuperscript(void)
{
    Insert(STYLE_SUPERSCRIPT) ;
    mRedrawFullDisplay = true ;         // make whole screen redraw, since this will toggle any text after it
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Toggle Superscript
///
/////////////////////////////////////////////////////////////////////////////
void cDocument::EndSuperscript(void)
{
    BeginSuperscript() ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Toggle Subscript
///
/////////////////////////////////////////////////////////////////////////////
void cDocument::BeginSubscript(void)
{
    Insert(STYLE_SUBSCRIPT) ;
    mRedrawFullDisplay = true ;         // make whole screen redraw, since this will toggle any text after it
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Toggle Subscript
///
/////////////////////////////////////////////////////////////////////////////
void cDocument::EndSubscript(void)
{
    BeginSubscript() ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Toggle Index
///
/////////////////////////////////////////////////////////////////////////////
void cDocument::BeginIndex(void)
{
    Insert(STYLE_INDEX) ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Toggle Index
///
/////////////////////////////////////////////////////////////////////////////
void cDocument::EndIndex(void)
{
    BeginIndex() ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Turn on centering
///
/////////////////////////////////////////////////////////////////////////////
void cDocument::BeginCenter(void)
{
    char out[100] ;
    snprintf(out, 99, ".ojc%c", HARD_RETURN) ;
    Insert(out) ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Turn on left justification
///
/////////////////////////////////////////////////////////////////////////////
void cDocument::BeginLeft(void)
{
    char out[100] ;
    snprintf(out, 99, ".oj off%c", HARD_RETURN) ;
    Insert(out) ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Turn on right justification
///
/////////////////////////////////////////////////////////////////////////////
void cDocument::BeginRight(void)
{
    char out[100] ;
    snprintf(out, 99, ".ojr%c", HARD_RETURN) ;
    Insert(out) ;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Turn on full justification
///
/////////////////////////////////////////////////////////////////////////////
void cDocument::BeginJustify(void)
{
    char out[100] ;
    snprintf(out, 99, ".oj on%c", HARD_RETURN) ;
    Insert(out) ;
}



/////////////////////////////////////////////////////////////////////////////
///
/// @return  nothing
///
/// @brief
/// Insert a hard return only if we are not at the start of a paragraph
///
/////////////////////////////////////////////////////////////////////////////
void cDocument::MaybeInsertHardReturn(void)
{
    POSITION_T cpos = GetPosition() ;
    PARAGRAPH_T para = GetParagraphFromPosition(cpos) ;

    POSITION_T start, end ;
    GetParagraphStartandEnd(para, start, end) ;
    if(start != cpos)
    {
        Insert(HARD_RETURN) ;
    }
}



/////////////////////////////////////////////////////////////////////////////
///
/// @return  nothing
///
/// @brief
/// Insert a tab
///
/////////////////////////////////////////////////////////////////////////////
void cDocument::InsertTab(sWSTab &tab)
{
    POSITION_T pos = GetPosition() ;
    POSITION_T cursorBefore = mCurrentPosition ;
    bool shouldRecord = !mUndoDisabled && !mIsLoading ;

    PARAGRAPH_T currentparagraphnumber = GetParagraphFromPosition(pos) ;
    auto paraiter = mParagraphData.begin() + currentparagraphnumber ;
    pos -= paraiter->index ;        // makeposition relative to paragraph start

    PairTable t  = std::make_pair(pos, TYPE_TAB) ;
    TabPair t1 = std::make_pair(pos,  tab) ;

    // suppress undo recording from Insert(MARKER_CHAR) -- we record our own
    bool oldDisabled = mUndoDisabled ;
    mUndoDisabled = true ;
    Insert(MARKER_CHAR) ;
    mUndoDisabled = oldDisabled ;

    auto iter = lower_bound(paraiter->pairs.begin(), paraiter->pairs.end(), t, TableCompare) ;
    auto iter1 = lower_bound(paraiter->tab.begin(), paraiter->tab.end(), t1, TabCompare) ;

    size_t tpos = static_cast<size_t>(distance(paraiter->tab.begin(), iter1)) ;

    paraiter->tab.insert(paraiter->tab.begin() + tpos, t1) ;

    tpos = static_cast<size_t>(distance(paraiter->pairs.begin(), iter)) ;
    paraiter->pairs.insert(paraiter->pairs.begin() + tpos, t) ;

    // record undo action with tab metadata
    if (shouldRecord)
    {
        sUndoCharInfo charInfo ;
        charInfo.codepoint = STYLE_TAB ;
        charInfo.tabData = tab ;
        std::vector<sUndoCharInfo> chars ;
        chars.push_back(charInfo) ;
        RecordAction(UNDO_ACTION_INSERT, cursorBefore, 1, chars, cursorBefore) ;
    }

    // notify listeners (inner Insert was suppressed by mUndoDisabled)
    NotifyChanged(currentparagraphnumber) ;
}



/////////////////////////////////////////////////////////////////////////////
///
/// @param  color [in] - RGB color to insert, or {-1,-1,-1,-1} for default
///
/// @return  nothing
///
/// @brief
/// Insert a color change at the current cursor position. Colors are stored
/// as full sSeqRGBColor values. The sentinel {-1,-1,-1,-1} means "use
/// default text color" (editor's configured foreground on screen, black
/// when printing).
///
/////////////////////////////////////////////////////////////////////////////
void cDocument::InsertColor(sSeqRGBColor &color)
{
    POSITION_T pos = GetPosition() ;
    POSITION_T cursorBefore = mCurrentPosition ;
    bool shouldRecord = !mUndoDisabled && !mIsLoading ;

    PARAGRAPH_T currentparagraphnumber = GetParagraphFromPosition(pos) ;
    auto paraiter = mParagraphData.begin() + currentparagraphnumber ;
    pos -= paraiter->index ;        // makeposition relative to paragraph start

    PairTable t  = std::make_pair(pos, TYPE_COLOR) ;
    ColorPair t1 = std::make_pair(pos,  color) ;

    // suppress undo recording from Insert(MARKER_CHAR) -- we record our own
    bool oldDisabled = mUndoDisabled ;
    mUndoDisabled = true ;
    Insert(MARKER_CHAR) ;
    mUndoDisabled = oldDisabled ;

    auto iter = lower_bound(paraiter->pairs.begin(), paraiter->pairs.end(), t, TableCompare) ;
    auto iter1 = lower_bound(paraiter->color.begin(), paraiter->color.end(), t1, ColorCompare) ;

    size_t tpos = static_cast<size_t>(distance(paraiter->color.begin(), iter1)) ;

    paraiter->color.insert(paraiter->color.begin() + tpos, t1) ;

    tpos = static_cast<size_t>(distance(paraiter->pairs.begin(), iter)) ;
    paraiter->pairs.insert(paraiter->pairs.begin() + tpos, t) ;

    // record undo action with color metadata
    if (shouldRecord)
    {
        sUndoCharInfo charInfo ;
        charInfo.codepoint = STYLE_INTERNAL_COLOR ;
        charInfo.colorData = color ;
        std::vector<sUndoCharInfo> chars ;
        chars.push_back(charInfo) ;
        RecordAction(UNDO_ACTION_INSERT, cursorBefore, 1, chars, cursorBefore) ;
    }

    // notify listeners (inner Insert was suppressed by mUndoDisabled)
    NotifyChanged(currentparagraphnumber) ;
}



/////////////////////////////////////////////////////////////////////////////
///
/// @param  color [in] - WordStar color with palette index (0-15)
///
/// @return  nothing
///
/// @brief
/// Convert a WordStar palette color to full RGB and insert it. Used by
/// file I/O readers (WordStar format) that read palette indices from disk.
/// Converts via gBaseWSColors[] lookup.
///
/////////////////////////////////////////////////////////////////////////////
void cDocument::InsertColorFromWSPalette(sWSColor &color)
{
    sSeqRGBColor rgbColor ;
    rgbColor.red = gBaseWSColors[color.colornumber].red ;
    rgbColor.green = gBaseWSColors[color.colornumber].green ;
    rgbColor.blue = gBaseWSColors[color.colornumber].blue ;
    rgbColor.alpha = 255 ;
    InsertColor(rgbColor) ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return  nothing
///
/// @brief
/// insert a font change
///
/////////////////////////////////////////////////////////////////////////////
void cDocument::InsertFont(sInternalFonts &font)
{
    POSITION_T pos = GetPosition() ;
    POSITION_T cursorBefore = mCurrentPosition ;
    bool shouldRecord = !mUndoDisabled && !mIsLoading ;

    PARAGRAPH_T currentparagraphnumber = GetParagraphFromPosition(pos) ;
    auto paraiter = mParagraphData.begin() + currentparagraphnumber ;
    pos -= paraiter->index ;        // makeposition relative to paragraph start

    PairTable t  = std::make_pair(pos, TYPE_FONT) ;
    FontPair t1 = std::make_pair(pos,  font) ;

    // suppress undo recording from Insert(MARKER_CHAR) -- we record our own
    bool oldDisabled = mUndoDisabled ;
    mUndoDisabled = true ;
    Insert(MARKER_CHAR) ;
    mUndoDisabled = oldDisabled ;

    auto iter = lower_bound(paraiter->pairs.begin(), paraiter->pairs.end(), t, TableCompare) ;
    auto iter1 = lower_bound(paraiter->font.begin(), paraiter->font.end(), t1, FontCompare) ;

    size_t tpos = static_cast<size_t>(distance(paraiter->font.begin(), iter1)) ;

    paraiter->font.insert(paraiter->font.begin() + tpos, t1) ;

    tpos = static_cast<size_t>(distance(paraiter->pairs.begin(), iter)) ;
    paraiter->pairs.insert(paraiter->pairs.begin() + tpos, t) ;

    // record undo action with font metadata
    if (shouldRecord)
    {
        sUndoCharInfo charInfo ;
        charInfo.codepoint = STYLE_FONT1 ;
        charInfo.fontData = font ;
        std::vector<sUndoCharInfo> chars ;
        chars.push_back(charInfo) ;
        RecordAction(UNDO_ACTION_INSERT, cursorBefore, 1, chars, cursorBefore) ;
    }

    // notify listeners (inner Insert was suppressed by mUndoDisabled)
    NotifyChanged(currentparagraphnumber) ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  type [in] variable type to insert
///
/// @return nothing
///
/// @brief
/// Inserts a variable marker at the current position.
/// Stores the variable type in the variable metadata table.
///
/////////////////////////////////////////////////////////////////////////////
void cDocument::InsertVariable(eVariableType type)
{
    POSITION_T pos = GetPosition();
    POSITION_T cursorBefore = mCurrentPosition ;
    bool shouldRecord = !mUndoDisabled && !mIsLoading ;

    PARAGRAPH_T currentparagraphnumber = GetParagraphFromPosition(pos);
    auto paraiter = mParagraphData.begin() + currentparagraphnumber;
    pos -= paraiter->index;

    PairTable t = std::make_pair(pos, TYPE_VARIABLE);
    VariablePair t1 = std::make_pair(pos, type);

    // suppress undo recording from Insert(MARKER_CHAR) -- we record our own
    bool oldDisabled = mUndoDisabled ;
    mUndoDisabled = true ;
    Insert(MARKER_CHAR);
    mUndoDisabled = oldDisabled ;

    auto iter = lower_bound(paraiter->pairs.begin(), paraiter->pairs.end(), t, TableCompare);
    auto iter1 = lower_bound(paraiter->variable.begin(), paraiter->variable.end(), t1, VariableCompare);

    size_t tpos = static_cast<size_t>(distance(paraiter->variable.begin(), iter1));

    paraiter->variable.insert(paraiter->variable.begin() + static_cast<ssize_t>(tpos), t1);

    tpos = static_cast<size_t>(distance(paraiter->pairs.begin(), iter));
    paraiter->pairs.insert(paraiter->pairs.begin() + static_cast<ssize_t>(tpos), t);

    // record undo action with variable metadata
    if (shouldRecord)
    {
        sUndoCharInfo charInfo ;
        charInfo.codepoint = STYLE_VARIABLE ;
        charInfo.variableData = type ;
        std::vector<sUndoCharInfo> chars ;
        chars.push_back(charInfo) ;
        RecordAction(UNDO_ACTION_INSERT, cursorBefore, 1, chars, cursorBefore) ;
    }

    // notify listeners (inner Insert was suppressed by mUndoDisabled)
    NotifyChanged(currentparagraphnumber) ;
}



/////////////////////////////////////////////////////////////////////////////
///
/// @return  nothing
///
/// @brief
/// insert a footnote
///
/////////////////////////////////////////////////////////////////////////////
void cDocument::InsertFootnote(sNote &note)
{
    POSITION_T pos = GetPosition() ;
    POSITION_T cursorBefore = mCurrentPosition ;
    bool shouldRecord = !mUndoDisabled && !mIsLoading ;

    PARAGRAPH_T currentparagraphnumber = GetParagraphFromPosition(pos) ;
    auto paraiter = mParagraphData.begin() + currentparagraphnumber ;
    pos -= paraiter->index ;        // makeposition relative to paragraph start

    PairTable t  = std::make_pair(pos, TYPE_FOOTNOTE) ;
    FootnotePair t1 = std::make_pair(pos,  note) ;

    // suppress undo recording from Insert(MARKER_CHAR) -- we record our own
    bool oldDisabled = mUndoDisabled ;
    mUndoDisabled = true ;
    Insert(MARKER_CHAR) ;
    mUndoDisabled = oldDisabled ;

    auto iter = lower_bound(paraiter->pairs.begin(), paraiter->pairs.end(), t, TableCompare) ;
    auto iter1 = lower_bound(paraiter->footnote.begin(), paraiter->footnote.end(), t1, FootnoteCompare) ;

    size_t tpos = static_cast<size_t>(distance(paraiter->footnote.begin(), iter1)) ;

    paraiter->footnote.insert(paraiter->footnote.begin() + tpos, t1) ;

    tpos = static_cast<size_t>(distance(paraiter->pairs.begin(), iter)) ;
    paraiter->pairs.insert(paraiter->pairs.begin() + tpos, t) ;

    // record undo action with footnote metadata
    if (shouldRecord)
    {
        sUndoCharInfo charInfo ;
        charInfo.codepoint = STYLE_FOOTNOTE ;
        charInfo.footnoteData = note ;
        std::vector<sUndoCharInfo> chars ;
        chars.push_back(charInfo) ;
        RecordAction(UNDO_ACTION_INSERT, cursorBefore, 1, chars, cursorBefore) ;
    }

    // notify listeners (inner Insert was suppressed by mUndoDisabled)
    NotifyChanged(currentparagraphnumber) ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return  nothing
///
/// @brief
/// insert a end note
///
/////////////////////////////////////////////////////////////////////////////
void cDocument::InsertEndnote(sNote &note)
{
    POSITION_T pos = GetPosition() ;
    POSITION_T cursorBefore = mCurrentPosition ;
    bool shouldRecord = !mUndoDisabled && !mIsLoading ;

    PARAGRAPH_T currentparagraphnumber = GetParagraphFromPosition(pos) ;
    auto paraiter = mParagraphData.begin() + currentparagraphnumber ;
    pos -= paraiter->index ;        // makeposition relative to paragraph start

    PairTable t  = std::make_pair(pos, TYPE_ENDNOTE) ;
    EndnotePair t1 = std::make_pair(pos,  note) ;

    // suppress undo recording from Insert(MARKER_CHAR) -- we record our own
    bool oldDisabled = mUndoDisabled ;
    mUndoDisabled = true ;
    Insert(MARKER_CHAR) ;
    mUndoDisabled = oldDisabled ;

    auto iter = lower_bound(paraiter->pairs.begin(), paraiter->pairs.end(), t, TableCompare) ;
    auto iter1 = lower_bound(paraiter->endnote.begin(), paraiter->endnote.end(), t1, EndnoteCompare) ;

    size_t tpos = static_cast<size_t>(distance(paraiter->endnote.begin(), iter1)) ;

    paraiter->endnote.insert(paraiter->endnote.begin() + tpos, t1) ;

    tpos = static_cast<size_t>(distance(paraiter->pairs.begin(), iter)) ;
    paraiter->pairs.insert(paraiter->pairs.begin() + tpos, t) ;

    // record undo action with endnote metadata
    if (shouldRecord)
    {
        sUndoCharInfo charInfo ;
        charInfo.codepoint = STYLE_ENDNOTE ;
        charInfo.endnoteData = note ;
        std::vector<sUndoCharInfo> chars ;
        chars.push_back(charInfo) ;
        RecordAction(UNDO_ACTION_INSERT, cursorBefore, 1, chars, cursorBefore) ;
    }

    // notify listeners (inner Insert was suppressed by mUndoDisabled)
    NotifyChanged(currentparagraphnumber) ;
}



/////////////////////////////////////////////////////////////////////////////
///
/// @param  loading   [IN] - true if loading file, else false
///
/// @return nothing
///
/// @brief
/// Set the loading flag
///
/////////////////////////////////////////////////////////////////////////////
void cDocument::SetLoading(bool loading)
{
    mIsLoading = loading ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return bool
///
/// @brief
/// Get the loading flag: true if file loading, else flase
///
/////////////////////////////////////////////////////////////////////////////
bool cDocument::GetLoading(void)
{
    return mIsLoading ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  bool suppress [in] true to suppress listener notifications
///
/// @return nothing
///
/// @brief
/// Set the suppress-notify flag. When true, document mutations will NOT
/// fire NotifyChanged() to listeners, but undo recording is preserved
/// (unlike SetLoading which also suppresses undo). Use for batch
/// operations like RTF paste where many inserts should be treated as
/// one visual update.
///
/////////////////////////////////////////////////////////////////////////////
void cDocument::SetSuppressNotify(bool suppress)
{
    mSuppressNotify = suppress ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return bool - true if notifications are suppressed
///
/// @brief
/// Get the suppress-notify flag.
///
/////////////////////////////////////////////////////////////////////////////
bool cDocument::GetSuppressNotify(void)
{
    return mSuppressNotify ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return POSITION_T - number of graphemes in document (including formatting chars)
///
/// @brief
/// Get the number of graphemes in the document
///
/////////////////////////////////////////////////////////////////////////////
POSITION_T cDocument::GetTextSize(void)
{
    PARAGRAPH_T size = static_cast<PARAGRAPH_T>(mParagraphData.size()) - 1 ;

    MY_ASSERT(size >= 0)
    MY_ASSERT(size < GetNumberofParagraphs())

    return mParagraphData[size].index + mParagraphData[size].offsets.size() ;
}



/////////////////////////////////////////////////////////////////////////////
///
/// @return the entire document text as a single string
///
/// @brief
/// Get all text in the document by iterating through every grapheme
/// position and concatenating the results.
///
/////////////////////////////////////////////////////////////////////////////
std::string cDocument::GetText(void)
{
    std::string text ;
    POSITION_T size = GetTextSize() ;

    for(POSITION_T loop = 0; loop < size; loop++)
    {
        std::string grapheme = GetChar(loop) ;
        text += grapheme ;
    }

    return text ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return POSITION_T - the position in the buffer
///
/// @brief
/// Get the position of the caret in the buffer
///
/////////////////////////////////////////////////////////////////////////////
POSITION_T cDocument::GetPosition(void)
{
    POSITION_T position = mCurrentPosition ;

    return position ;
}



/////////////////////////////////////////////////////////////////////////////
///
/// @param position   [IN] the position to set
///
/// @return nothing
///
/// @brief
/// set the current position in the buffer
///
/////////////////////////////////////////////////////////////////////////////
void cDocument::SetPosition(POSITION_T position)
{
    if(position >= GetTextSize() && position != 0)
    {
        position = GetTextSize() - 1 ;
    }
    else if(position < 0)
    {
        position = 0 ;
    }

    mPreviousPosition = mCurrentPosition ;
    mCurrentPosition = position ;
}



/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// set the current position to the previous position
///
/////////////////////////////////////////////////////////////////////////////
void cDocument::GotoPreviousPosition(void)
{
    SetPosition(mPreviousPosition) ;
}




/////////////////////////////////////////////////////////////////////////////
///
/// @return number of paragraph
///
/// @brief
/// Get the number of paragraphs in the document
///
/////////////////////////////////////////////////////////////////////////////
PARAGRAPH_T cDocument::GetNumberofParagraphs(void)
{
    return static_cast<PARAGRAPH_T>(mParagraphData.size()) ;
}




/////////////////////////////////////////////////////////////////////////////
///
/// @param paragraph   [IN] the paragraph to check
/// @param start       [IN/OUT] the start position
/// @param end         [IN/OUT] the end position
///
/// @return nothing
///
/// @brief
/// returns start bufferpos and end bufferpos of paragraph 
///
/////////////////////////////////////////////////////////////////////////////
void cDocument::GetParagraphStartandEnd(const PARAGRAPH_T paragraph, POSITION_T &start, POSITION_T &end)
{
    MY_ASSERT(paragraph >= 0)
    MY_ASSERT(paragraph < static_cast<POSITION_T>(mParagraphData.size()))
    auto iter = mParagraphData.begin() + paragraph ;
    start = iter->index ;

    std::vector<POSITION_T> offsets ;
    size_t numgraphemes = iter->offsets.size(); // GraphemeCount(iter->buffer, offsets);
    end =  iter->index + static_cast<POSITION_T>(numgraphemes) - 1 ;
}



/////////////////////////////////////////////////////////////////////////////
///
/// @param  position    [IN] - the position in the buffer
///
/// @return PARAGRAPH_T
///
/// @brief Get the paragraph number from the character position
///
/////////////////////////////////////////////////////////////////////////////
PARAGRAPH_T cDocument::GetParagraphFromPosition(POSITION_T position)
{
#ifdef DEBUG
    POSITION_T tsize = GetTextSize() ;

    MY_ASSERT(position >= 0)
    MY_ASSERT(position <= tsize)
#endif
    mTempParagraph.index = position ;

    auto iter = lower_bound(mParagraphData.begin(), mParagraphData.end(), mTempParagraph, ParagraphCompare) ;
    // handle things if we are at end of line
	if (iter != mParagraphData.end())
	{
		if (iter->index > position)
        {
		    if (iter != mParagraphData.begin())
			{
				iter--;
			}
		}
	}

    PARAGRAPH_T pos = static_cast<PARAGRAPH_T>(std::distance(mParagraphData.begin(), iter)) ;
    if(static_cast<size_t>(pos) >= mParagraphData.size())
    {
        pos = static_cast<PARAGRAPH_T>(mParagraphData.size()) - 1 ;
    }
    
    return pos ;
}



/////////////////////////////////////////////////////////////////////////////
///
/// @param position   [IN] the position to check tab type for
///
/// @return sWSTab
///
/// @brief
/// We have a tab in the buffer, check what type it is
///
/////////////////////////////////////////////////////////////////////////////
sWSTab cDocument::GetTab(POSITION_T position)
{
    if(position < 0 || position >= GetTextSize())
    {
        sWSTab tab ;
        tab.type = TAB_BAD ;
        return tab ;
    }

    TabPair comp ;

    PARAGRAPH_T currentparagraphnumber = GetParagraphFromPosition(position) ;
    auto paraiter = mParagraphData.begin() + currentparagraphnumber ;
    position -= paraiter->index ;
    comp.first = position ;
    auto  iter1 = lower_bound(paraiter->tab.begin(), paraiter->tab.end(), comp, TabCompare) ;

    if(iter1 == paraiter->tab.end() || iter1->first != position)
    {
        sWSTab tab ;
        tab.type = TAB_BAD ;
        return tab ;
    }

    return static_cast<sWSTab>(iter1->second) ;
}



/////////////////////////////////////////////////////////////////////////////
///
/// @param position   [IN] the position to check color for
/// @param color      [OUT] the found color
///
/// @return bool - true if color found at position
///
/// @brief
/// We have a color in the buffer, check what it is
///
/////////////////////////////////////////////////////////////////////////////
bool cDocument::GetColor(POSITION_T position, sSeqRGBColor &color)
{
    if(position < 0 || position >= GetTextSize())
    {
        return false ;
    }

    ColorPair comp ;
    bool retval = false ;

    PARAGRAPH_T currentparagraphnumber = GetParagraphFromPosition(position) ;
    auto paraiter = mParagraphData.begin() + currentparagraphnumber ;

    position -= paraiter->index ;
    comp.first = position ;
    auto  iter1 = lower_bound(paraiter->color.begin(), paraiter->color.end(), comp, ColorCompare) ;
    if(iter1 != paraiter->color.end() && iter1->first == position)
    {
        color = iter1->second ;
        retval = true ;
    }

    return retval ;
}



/////////////////////////////////////////////////////////////////////////////
///
/// @param position   [IN] the position to check font for
/// @param intfont    [OUT] sInternalFonts data
///
/// @return bool - true on success else false
///
/// @brief
/// We have a font in the buffer, check what it is
///
/////////////////////////////////////////////////////////////////////////////
bool cDocument::GetFont(POSITION_T position, sInternalFonts &intfont)
{
    if(position < 0 || position >= GetTextSize())
    {
        return false ;
    }

    FontPair comp ;
    bool retval = false ;

    PARAGRAPH_T currentparagraphnumber = GetParagraphFromPosition(position) ;
    auto paraiter = mParagraphData.begin() + currentparagraphnumber ;
    position -= paraiter->index ;
    comp.first = position ;
    auto  iter1 = lower_bound(paraiter->font.begin(), paraiter->font.end(), comp, FontCompare) ;

    if(iter1 != paraiter->font.end() && iter1->first == position)
    {
        intfont =  iter1->second ;
        retval = true ;
    }

    return retval ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  position [in] document position to query
///
/// @return variable type at position, or VAR_DATE as default
///
/// @brief
/// Returns the variable type stored at the given document position.
///
/////////////////////////////////////////////////////////////////////////////
eVariableType cDocument::GetVariable(POSITION_T position)
{
    if (position < 0 || position >= GetTextSize())
    {
        return VAR_DATE;
    }

    VariablePair comp;
    PARAGRAPH_T currentparagraphnumber = GetParagraphFromPosition(position);
    auto paraiter = mParagraphData.begin() + currentparagraphnumber;
    position -= paraiter->index;
    comp.first = position;

    auto iter = lower_bound(paraiter->variable.begin(), paraiter->variable.end(), comp, VariableCompare);

    if (iter != paraiter->variable.end() && iter->first == position)
    {
        return iter->second;
    }

    return VAR_DATE;
}



/////////////////////////////////////////////////////////////////////////////
///
/// @param  fontlist   [IN/OUT] - All the fonts in the document
///
/// @return nothing
///
/// @brief
/// fill fontlist with alll te font used in the document, including duplicates
///
/////////////////////////////////////////////////////////////////////////////
void cDocument::GetFontList(std::vector<sInternalFonts> &fontlist)
{
    for(auto &paraiter : mParagraphData)
    {
        for(auto &fontiter : paraiter.font)
        {
            fontlist.push_back(fontiter.second) ;
        }
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param   value          [in] - the value we are converting
/// @param   type           [in] - the type we are converting from
///
/// @return  double - twips value
///
/// @brief
/// convert known data values into twip (rows, cm, mm, points, inches)
///
/////////////////////////////////////////////////////////////////////////////
COORD_T cDocument::ConvertToTwips(double value, char type, double cwidth)
{
    COORD_T retval = 0 ;

    type = toupper(type) ;
    switch(type)
    {
        case 'C' :              // centimeters
            retval = static_cast<COORD_T>((value * 10) * TWIPSPERMM) ;
            break ;

        case 'M' :              // millimeters
            retval = static_cast<COORD_T>(value * TWIPSPERMM) ;
            break ;
            
        case 'P' :              // points
            retval = static_cast<COORD_T>(value * POINTSTOTWIPS) ;
            break ;

        case '\"' :               // inches
        case 'I' :
            retval = static_cast<COORD_T>(value * TWIPSPERINCH) ;
            break ;

        case 'R' :              // rows (we assume 12pt font)
            retval = static_cast<COORD_T>(value * 240) ;            // 240 twips per 12 point font
            break ;

        case ' ' :              // basically, just the separator (columns)
            retval = static_cast<COORD_T>((value - 1) * cwidth) ;            // 120 twips per 12 point font
            break ;

        default :
            retval = 0 ;
            break ;
    }

    return retval ;
}



/////////////////////////////////////////////////////////////////////////////
///
/// @param   txt          [in] - the string we are converting to a double
/// @param   incdec       [out] - true if this is an increment or decrement, else false
///
/// @return  double
///
/// @brief
/// Converts a string to a double, removing the type indicator (M, ", C, etc)
///
/////////////////////////////////////////////////////////////////////////////
double cDocument::GetValue(std::string txt, bool &incdec)
{
     double value = -32768.0 ;
    incdec = false ;

    // trim trailing whitespace
    const std::string WHITESPACE = " \n\r\t\f\v\177";           // \177 is ^Z - end of file
    size_t end = txt.find_last_not_of(WHITESPACE);
    size_t start = txt.find_first_not_of(" ") ;     // strip leading spaces
    txt = (end == std::string::npos) ? "" : txt.substr(start, end + 1);

    if(txt.empty())
    {
        return value ;
    }

    // now we replace any trailing non-numeric characters from the string
    unsigned long loop ;
    for(loop = txt.length() - 1; loop > 0; loop--)
    {
        if(isdigit(txt.at(loop))) // .unicode()))
        {
            break ;
        }
//         txt[loop] = 0 ;
        txt = txt.substr(0, loop) ;
    }


    if(txt[0] == '+')
    {
        // if all we have is the +, then the number is incomplete
        if(txt.length() == 1)
        {
            return value ;
        }

        incdec = true ;
    }
    else if(txt[0] == '-')
    {
        // if all we have is the -, then the number is incomplete
        if(txt.length() == 1)
        {
            return value ;
        }

        incdec = true ;
    }

    std::string expression_str = txt.c_str() ;

    value = mMath.DoMath(expression_str) ;
    
    return value ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param   txxt          [in] - return the type of the passed in value
///
/// @return  char
///
/// @brief
/// finds out the type of a value and returns it (M, ", C, etc)
///
/// M - millimeter, C - centimeter, " and I is inches  X - no type specified
/////////////////////////////////////////////////////////////////////////////
char cDocument::GetType(std::string text)
{
    // trim trailing whitespace
    constexpr char RWS[] = " \n\r\t\f\v\177";           // \177 is ^Z - end of file
    text.erase(text.find_last_not_of(RWS) + 1) ;

    // trim leading whitespace and digits
    constexpr char LWS[] = "+-*/()01234567890. \n\r\t\f\v\177" ;
    text.erase(0, text.find_first_not_of(LWS)) ;

    // uppercase remaining text for case-insensitive unit comparison
    for (size_t i = 0 ; i < text.length() ; i++)
    {
        text[i] = toupper(text[i]) ;
    }

    char retval = 'x' ;

    if(text.length() <= 1)                              // units are always 1 charcater, or none
    {
        if(text.find('\"') != std::string::npos)              // inches
        {
            retval = '\"' ;
        }
        else if(text.find('I') != std::string::npos)          // inches
        {
            retval = '\"' ;
        }
        else if(text.find('C') != std::string::npos)          // centimeters
        {
            retval = 'C' ;
        }
        else if(text.find('M') != std::string::npos)          // millimeters
        {
            retval = 'M' ;
        }
        else if(text.find('P') != std::string::npos)          // points
        {
            retval = 'P' ;
        }
        else if(text.find('R') != std::string::npos)         // it's rows
        {
            retval = 'R' ;
        }
        else                                            // check if it's a column
        {
            if(text.length() == 0)
            {
                retval = 'I' ;
            }
            else
            {
                char last = text[text.length() - 1] ;
                if(isdigit(last))
                {
                    retval = ' ' ;
                }
            }
        }
    }

    return retval ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  text     [in]  - the expression string (e.g. "4c + 2i", "80 - 5")
/// @param  incdec   [out] - true if leading +/- (increment/decrement)
/// @param  hasUnits [out] - true if any unit chars found (result is twips)
///
/// @return double - evaluated result (in twips if hasUnits, raw number otherwise)
///
/// @brief
/// Evaluate a math expression with optional mixed unit annotations.
/// Uses two strategies based on operators present:
///
/// Strategy 1 (add/sub only): Convert each unit-annotated number to
/// centimeters, evaluate, convert result to twips. Handles mixed units
/// like "4c + 2i" correctly.
///
/// Strategy 2 (has * or /): All unit-annotated numbers must use the same
/// unit. Strip units, evaluate raw numbers, convert result to twips.
/// Mixed units in multiply/divide are an error. This avoids the
/// squared-units problem (".5i * .5i" = 0.25 inches, not 1.6129 cm).
///
/// Supported units: i/" (inches), c (cm), m (mm), p (points), r (rows)
///
/////////////////////////////////////////////////////////////////////////////
double cDocument::EvaluateExpression(const std::string &text, bool &incdec, bool &hasUnits)
{
    double value = -32768.0 ;
    incdec = false ;
    hasUnits = false ;

    // trim trailing whitespace
    const std::string WHITESPACE = " \n\r\t\f\v\177" ;
    size_t end = text.find_last_not_of(WHITESPACE) ;
    size_t start = text.find_first_not_of(" ") ;
    std::string trimmed = (end == std::string::npos) ? "" : text.substr(start, end - start + 1) ;

    if (trimmed.empty())
    {
        return value ;
    }

    // check for increment/decrement
    if (trimmed[0] == '+' || trimmed[0] == '-')
    {
        if (trimmed.length() == 1)
        {
            return value ;
        }
        incdec = true ;
    }

    // pre-scan: check for * or / operators and collect distinct unit types
    bool hasMulDiv = false ;
    char foundUnitChar = 0 ;      // normalized unit char for strategy 2
    bool mixedUnits = false ;
    bool anyUnitsFound = false ;

    for (size_t scan = 0 ; scan < trimmed.length() ; scan++)
    {
        char ch = trimmed[scan] ;

        // skip leading +/- (those are incdec, not operators)
        if (scan == 0 && (ch == '+' || ch == '-'))
        {
            continue ;
        }

        if (ch == '*' || ch == '/')
        {
            hasMulDiv = true ;
        }

        // check if this char is a unit suffix (must follow a digit)
        if (scan > 0 && isdigit(trimmed[scan - 1]))
        {
            char upper = toupper(ch) ;
            char normalized = 0 ;

            if (upper == 'I' || ch == '"')
            {
                normalized = 'I' ;
            }
            else if (upper == 'C')
            {
                normalized = 'C' ;
            }
            else if (upper == 'M')
            {
                normalized = 'M' ;
            }
            else if (upper == 'P')
            {
                normalized = 'P' ;
            }
            else if (upper == 'R')
            {
                normalized = 'R' ;
            }

            if (normalized != 0)
            {
                anyUnitsFound = true ;
                if (foundUnitChar == 0)
                {
                    foundUnitChar = normalized ;
                }
                else if (foundUnitChar != normalized)
                {
                    mixedUnits = true ;
                }
            }
            else if (isalpha(ch))
            {
                // alphabetic char after a digit that is not a valid unit is an error
                return -32768.0 ;
            }
        }
    }

    // strategy 2: has multiply or divide
    if (hasMulDiv)
    {
        // mixed units in multiply/divide expressions are an error
        if (mixedUnits)
        {
            return -32768.0 ;
        }

        // strip unit chars, evaluate raw numbers, convert result
        std::string processed ;
        size_t i = 0 ;
        size_t len = trimmed.length() ;

        while (i < len)
        {
            char ch = trimmed[i] ;

            // consume numbers (digits and decimal points)
            if (isdigit(ch) || (ch == '.' && i + 1 < len && isdigit(trimmed[i + 1])))
            {
                while (i < len && (isdigit(trimmed[i]) || trimmed[i] == '.'))
                {
                    processed += trimmed[i] ;
                    i++ ;
                }

                // skip the unit character if present
                if (i < len)
                {
                    char upper = toupper(trimmed[i]) ;
                    if (upper == 'I' || upper == 'C' || upper == 'M' ||
                        upper == 'P' || upper == 'R' || trimmed[i] == '"')
                    {
                        i++ ;  // skip the unit char
                    }
                }
            }
            else
            {
                // copy operators, whitespace, parentheses, signs as-is
                processed += ch ;
                i++ ;
            }
        }

        // evaluate the stripped expression
        value = mMath.DoMath(processed) ;

        // convert result to twips using the single unit found
        if (anyUnitsFound)
        {
            hasUnits = true ;
            // use the " char for inches since ConvertToTwips expects it
            char convChar = foundUnitChar ;
            if (convChar == 'I')
            {
                convChar = '"' ;
            }
            value = static_cast<double>(ConvertToTwips(value, convChar, 0)) ;
        }

        return value ;
    }

    // strategy 1: addition/subtraction only -- convert to centimeters first
    // unit-to-centimeter conversion factors (derived from config.h constants)
    constexpr double INCHES_TO_CM = TWIPSPERINCH / TWIPSPERCM ;
    constexpr double CM_TO_CM     = 1.0 ;
    constexpr double MM_TO_CM     = TWIPSPERMM / TWIPSPERCM ;
    constexpr double POINTS_TO_CM = POINTSTOTWIPS / TWIPSPERCM ;
    constexpr double ROWS_TO_CM   = 240.0 / TWIPSPERCM ;

    std::string processed ;
    size_t i = 0 ;
    size_t len = trimmed.length() ;

    while (i < len)
    {
        char ch = trimmed[i] ;

        // check if we are at the start of a number (digit or decimal point)
        if (isdigit(ch) || (ch == '.' && i + 1 < len && isdigit(trimmed[i + 1])))
        {
            // consume the number
            size_t numStart = i ;
            while (i < len && (isdigit(trimmed[i]) || trimmed[i] == '.'))
            {
                i++ ;
            }
            std::string numStr = trimmed.substr(numStart, i - numStart) ;

            // check for a unit character following the number
            if (i < len)
            {
                char unitChar = trimmed[i] ;
                char upperUnit = toupper(unitChar) ;
                double cmFactor = 0.0 ;
                bool foundUnit = false ;

                if (upperUnit == 'I' || unitChar == '"')
                {
                    cmFactor = INCHES_TO_CM ;
                    foundUnit = true ;
                }
                else if (upperUnit == 'C')
                {
                    cmFactor = CM_TO_CM ;
                    foundUnit = true ;
                }
                else if (upperUnit == 'M')
                {
                    cmFactor = MM_TO_CM ;
                    foundUnit = true ;
                }
                else if (upperUnit == 'P')
                {
                    cmFactor = POINTS_TO_CM ;
                    foundUnit = true ;
                }
                else if (upperUnit == 'R')
                {
                    cmFactor = ROWS_TO_CM ;
                    foundUnit = true ;
                }

                if (foundUnit)
                {
                    // convert the number to centimeters with full double precision
                    double numVal = atof(numStr.c_str()) ;
                    double cmVal = numVal * cmFactor ;
                    char buf[32] ;
                    snprintf(buf, sizeof(buf), "%.17g", cmVal) ;
                    processed += buf ;
                    hasUnits = true ;
                    i++ ;  // skip the unit character
                }
                else
                {
                    // no unit, copy number as-is
                    processed += numStr ;
                }
            }
            else
            {
                // end of string, no unit, copy number as-is
                processed += numStr ;
            }
        }
        else
        {
            // copy operators, whitespace, parentheses, +/- signs as-is
            processed += ch ;
            i++ ;
        }
    }

    // evaluate the processed expression via PicoMath
    value = mMath.DoMath(processed) ;

    // if units were found, result is in centimeters -- convert to twips
    if (hasUnits)
    {
        value = value * TWIPSPERCM ;
    }

    return value ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param   show          [in] - eSHowCOntrol type
///
/// @return  nothing
///
/// @brief
/// Set whether we show control codes and/or dot commands
///
/////////////////////////////////////////////////////////////////////////////
void cDocument::SetShowControl(eShowControl show)
{
    mShowControl = show ;
}


eShowControl cDocument::GetShowControl(void)
{
    return mShowControl ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  count [in] - word count to store
///
/// @return nothing
///
/// @brief
/// Set cached word count for VAR_WORD_COUNT variable expansion.
///
/////////////////////////////////////////////////////////////////////////////
void cDocument::SetWordCount(long count)
{
    mWordCount = count ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return long - cached word count
///
/// @brief
/// Get cached word count for VAR_WORD_COUNT variable expansion.
///
/////////////////////////////////////////////////////////////////////////////
long cDocument::GetWordCount(void) const
{
    return mWordCount ;
}



/////////////////////////////////////////////////////////////////////////////
///
/// @return  nothing
///
/// @brief
/// Copy a marked block into the copy buffer, preserving all metadata
/// (tabs, fonts, colors, footnotes, endnotes, variables).
///
/// Uses CaptureCharacters() to capture both text and metadata, which is
/// the same infrastructure used by the undo system.
///
/////////////////////////////////////////////////////////////////////////////
void cDocument::Copy(void)
{
    if (mBlockSet)
    {
        mCopyBuffer.clear() ;

        // Use CaptureCharacters to get text + metadata
        // This preserves tabs, fonts, colors, footnotes, endnotes, variables
        POSITION_T length = mEndBlock - mStartBlock ;
        mCopyBuffer = CaptureCharacters(mStartBlock, length) ;
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return  nothing
///
/// @brief
/// Paste the copy buffer at the current caret position, restoring all
/// metadata (tabs, fonts, colors, footnotes, endnotes, variables).
///
/// Uses ReinsertCharacters() to restore both text and metadata, which is
/// the same infrastructure used by the undo system.
///
/////////////////////////////////////////////////////////////////////////////
void cDocument::Paste(void)
{
    if (!mCopyBuffer.empty())
    {
        BeginUndoGroup() ;
        POSITION_T pos = GetPosition() ;

        // suppress per-character notifications during paste
        // uses mSuppressNotify (not mIsLoading) to preserve undo recording
        mSuppressNotify = true ;

        ReinsertCharacters(pos, mCopyBuffer) ;

        mSuppressNotify = false ;

        EndUndoGroup() ;

        // fire one notification for the entire paste operation
        NotifyChanged(GetParagraphFromPosition(pos)) ;
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return  nothing
///
/// @brief
/// copy the marked block into the copy buffer, and delete it
///
/////////////////////////////////////////////////////////////////////////////
void cDocument::Cut(void)
{
    if(mBlockSet)
    {
        POSITION_T current = GetPosition() ;
        POSITION_T blockStart = mStartBlock ;
        POSITION_T cutsize = mEndBlock - mStartBlock ;

        SetPosition(blockStart) ;
        Delete(blockStart, cutsize) ;

        if(current > blockStart)
        {
            current = std::max<POSITION_T>(blockStart, current - cutsize) ;
        }
        SetPosition(current) ;

        UnsetBlock() ;
    }
}



/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Sets the current position as a block start
///
/////////////////////////////////////////////////////////////////////////////
void cDocument::SetBeginBlock(void)
{
    POSITION_T newstart = GetPosition() ;

    // if a block is set, save the info
    if(mBlockSet)
    {
        mOldStartBlock = mStartBlock ;
        mOldEndBlock = mEndBlock ;
        mOldBlockSet = mBlockSet ;

        if(GetCharNoAdvance(mStartBlock)[0] == REPLACE_CHAR)
        {
            Delete(mStartBlock, 1) ;
            if(newstart > mStartBlock)
            {
                newstart-- ;
            }
        }
    }

    if(mStartBlock != NOT_SET)
    {
        if(GetCharNoAdvance(mStartBlock)[0] == REPLACE_CHAR)
        {
            Delete(mStartBlock, 1) ;
            if(newstart > mStartBlock)
            {
                newstart-- ;
            }
        }
    }

    if(newstart < 0)
    {
        newstart = 0 ;
    }   

    SetPosition(newstart) ;
    mStartBlock = newstart ;

    if(mEndBlock > mStartBlock)
    {
        mBlockSet = true ;
    }
    else
    {
        mBlockSet = false ;

        std::string ch = GetCharNoAdvance(mStartBlock) ;
        if(ch[0] != REPLACE_CHAR)
        {
            SetPosition(mStartBlock) ;
            Insert(REPLACE_CHAR) ;
            mStartBlock-- ;             // insert increments start block, so put it back where it belongs
            if(mStartBlock < 0)
            {
                mStartBlock = 0 ;
            }
        }
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Sets the current position as a block end
///
/////////////////////////////////////////////////////////////////////////////
void cDocument::SetEndBlock(void)
{
    // if a block is set, save the info
    if(mBlockSet)
    {
        mOldStartBlock = mStartBlock ;
        mOldEndBlock = mEndBlock ;
        mOldBlockSet = mBlockSet ;
    }

    mEndBlock = GetPosition() ;

    if(mEndBlock > mStartBlock && mStartBlock != NOT_SET)
    {
        mBlockSet = true ;
        POSITION_T start  = mStartBlock ;
        if(GetCharNoAdvance(mStartBlock)[0] == REPLACE_CHAR)
        {
            Delete(mStartBlock, 1) ;
            mStartBlock = start ;
        }
    }
    else
    {
        mBlockSet = false ;

        if(mStartBlock != NOT_SET)
        {
            std::string ch = GetCharNoAdvance(mStartBlock) ;
            if(ch[0] != REPLACE_CHAR)
            {
                SetPosition(mStartBlock) ;
                Insert(REPLACE_CHAR) ;
                mStartBlock-- ;             // insert invrements start block, so put it back where it belongs
                if(mStartBlock < 0)
                {
                    mStartBlock = 0 ;
                }
            }
        }
    }

    SetPosition(mEndBlock) ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// select previous block and make current selection new previous
///
/////////////////////////////////////////////////////////////////////////////
void cDocument::SetPreviousBlock(void)
{
    if(mOldBlockSet)
    {
        POSITION_T start, end ;
        bool set ;

        POSITION_T currentpos = GetPosition() ;

        start = mStartBlock ;
        end = mEndBlock ;
        set = mBlockSet ;

        if(GetCharNoAdvance(start)[0] == REPLACE_CHAR)
        {
            Delete(start, 1) ;
        }

        mStartBlock = mOldStartBlock ;
        mEndBlock = mOldEndBlock ;
        mBlockSet = mOldBlockSet ;

        SetPosition(mStartBlock) ;
        std::string ch = GetCharNoAdvance(mStartBlock) ;
        if(mBlockSet == false)
        {
            SetPosition(mStartBlock) ;
            Insert(REPLACE_CHAR) ;
        }

        mOldStartBlock = start ;
        mOldEndBlock = end ;
        mOldBlockSet = set ;

        SetPosition(currentpos) ;
    }
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  int offset [in] save position slot index (0-9)
///
/// @return nothing
///
/// @brief
/// Set or toggle a saved position marker. If a marker already exists
/// at the current position, it is removed. Otherwise a REPLACE_CHAR
/// marker is inserted at the current position and the slot is updated.
///
/////////////////////////////////////////////////////////////////////////////
void cDocument::SetSavePosition(int offset)
{
    POSITION_T position = GetPosition() ;

    // if we are already saved at this position, then delete it
    // add one because we inserted a SAVE_CHAR
    POSITION_T checkpos = mSavePosition[offset] ;
    if(checkpos + 1 == position && checkpos != NOT_SET)
    {
        // remove the saved position
        mSavePosition[offset] = NOT_SET ;
        if(GetCharNoAdvance(checkpos)[0] == SAVE_CHAR)
        {
            Delete(checkpos, 1) ;
        }
    }
    else
    {
        // if the position is already set, delete the SAVE_CHAR
        if(checkpos != NOT_SET)
        {
            if(GetCharNoAdvance(checkpos)[0] == SAVE_CHAR)
            {
                Delete(checkpos, 1) ;
            }
        }

        if(checkpos < position && checkpos != NOT_SET)
        {
            position-- ;
        }

        // save the position
        mSavePosition[offset] = position ;
        SetPosition(position) ;
        Insert(SAVE_CHAR) ;
    }
}



/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Copy the currently selected block and paste it at the cursor position.
/// Only operates if a block is currently set.
///
/////////////////////////////////////////////////////////////////////////////
void cDocument::CopyBlock(void)
{
    if(mBlockSet == true)
    {
        POSITION_T position = GetPosition() ;

        Copy() ;
        SetPosition(position) ;
        Paste() ;
        SetPosition(position) ;
    }
}



/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Move the currently selected block to the cursor position.
/// Copies the block to the cursor, then cuts the original. Adjusts
/// block markers to highlight the pasted text at the new location.
/// Only operates if a block is set and the cursor is outside the block.
///
/////////////////////////////////////////////////////////////////////////////
void cDocument::MoveBlock(void)
{
    if(mBlockSet == true)
    {
        POSITION_T position = GetPosition() ;
        POSITION_T diff = mEndBlock - mStartBlock ;       // highlight our pasted text

        if((position < mStartBlock) || (position > mEndBlock))
        {

            Copy() ;
            SetPosition(position) ;
            Paste() ;

            if(position > mEndBlock)
            {
                Cut() ;

                mStartBlock = position - diff ;
                mEndBlock = mStartBlock + diff ;

                SetPosition(mEndBlock ) ;
            }
            else
            {
                Cut() ;

                mStartBlock = position ;
                mEndBlock = mStartBlock + diff ;

                SetPosition(mEndBlock) ;
            }
        }
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Delete the currently selected block. Cuts the block content and
/// repositions the cursor at the block start position.
///
/////////////////////////////////////////////////////////////////////////////
void cDocument::DeleteBlock(void)
{
    if(mBlockSet == true)
    {
        POSITION_T blockStart = mStartBlock ;    // Save original position before Cut() modifies it

        SetPosition(mStartBlock) ;

        Cut() ;

        mBlockSet = false ;
        SetPosition(blockStart) ;                // Use saved position (Cut() decrements mStartBlock)

        UnsetBlock() ;
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Clear block selection markers. Resets start and end to NOT_SET
/// and sets the block flag to false.
///
/////////////////////////////////////////////////////////////////////////////
void cDocument::UnsetBlock(void)
{
    mStartBlock = NOT_SET ;
    mEndBlock = NOT_SET ;
    mBlockSet = false ;
}




/////////////////////////////////////////////////////////////////////////////
///
/// @param  type          [IN] - UNDO_ACTION_INSERT or UNDO_ACTION_DELETE
/// @param  position      [IN] - document position where action occurred
/// @param  length        [IN] - number of graphemes involved
/// @param  chars         [IN] - character data for the action
/// @param  cursorBefore  [IN] - cursor position before the action
///
/// @return nothing
///
/// @brief
/// Record a single undo action. Appends to current group if one is open,
/// otherwise wraps in a single-action group and pushes to the undo stack.
/// Clears redo stack on new action.
///
/////////////////////////////////////////////////////////////////////////////
void cDocument::RecordAction(eUndoActionType type, POSITION_T position, POSITION_T length,
                             const std::vector<sUndoCharInfo> &chars, POSITION_T cursorBefore)
{
    if (mUndoDisabled || mIsLoading || mSuppressUndo)
    {
        return ;
    }

    sUndoAction action ;
    action.type = type ;
    action.position = position ;
    action.length = length ;
    action.chars = chars ;
    action.cursorBefore = cursorBefore ;
    action.cursorAfter = mCurrentPosition ;

    // clear redo stack on new action
    mRedoStack.clear() ;

    if (mUndoGroupOpen)
    {
        // append to current group
        mCurrentGroup.actions.push_back(action) ;
        mCurrentGroup.cursorAfter = mCurrentPosition ;
    }
    else
    {
        // wrap in a single-action group
        sUndoGroup group ;
        group.cursorBefore = cursorBefore ;
        group.cursorAfter = mCurrentPosition ;
        group.actions.push_back(action) ;

        mUndoStack.push_back(group) ;

        // trim oldest entries if stack exceeds limit
        while (static_cast<int>(mUndoStack.size()) > MAX_UNDO_STEPS)
        {
            mUndoStack.pop_front() ;
        }
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  position  [IN] - document position to start capturing from
/// @param  length    [IN] - number of graphemes to capture
///
/// @return vector of sUndoCharInfo -- one entry per grapheme
///
/// @brief
/// Read characters and their metadata from the document before deletion.
/// For regular text, stores the Unicode codepoint. For MARKER_CHAR
/// entries, looks up the control code type and associated metadata
/// (tab, font, color, footnote, endnote, variable).
///
/////////////////////////////////////////////////////////////////////////////
std::vector<sUndoCharInfo> cDocument::CaptureCharacters(POSITION_T position, POSITION_T length)
{
    std::vector<sUndoCharInfo> result ;

    for (POSITION_T i = 0 ; i < length ; ++i)
    {
        POSITION_T absPos = position + i ;
        sUndoCharInfo charInfo ;

        // get the paragraph and relative position
        PARAGRAPH_T para = GetParagraphFromPosition(absPos) ;
        auto &paraData = mParagraphData[static_cast<size_t>(para)] ;
        POSITION_T relPos = absPos - paraData.index ;

        // bounds check
        if (relPos < 0 || relPos >= static_cast<POSITION_T>(paraData.offsets.size()))
        {
            continue ;
        }

        // read the raw byte(s) from the buffer at this grapheme position
        POSITION_T byteIndex = paraData.offsets[static_cast<size_t>(relPos)] ;
        POSITION_T byteLen ;
        if (static_cast<size_t>(relPos) < paraData.offsets.size() - 1)
        {
            byteLen = paraData.offsets[static_cast<size_t>(relPos + 1)] - byteIndex ;
        }
        else
        {
            byteLen = static_cast<POSITION_T>(paraData.buffer.size()) - byteIndex ;
        }

        unsigned char firstByte = static_cast<unsigned char>(paraData.buffer[static_cast<size_t>(byteIndex)]) ;

        if (firstByte == MARKER_CHAR)
        {
            // MARKER_CHAR -- look up the actual control code and metadata
            eModifiers controlCode = GetControlChar(absPos) ;
            charInfo.codepoint = static_cast<CHAR_T>(controlCode) ;

            // look up metadata based on pairs table type
            PairTable pcomp ;
            pcomp.first = relPos ;
            auto piter = lower_bound(paraData.pairs.begin(), paraData.pairs.end(), pcomp, TableCompare) ;

            if (piter != paraData.pairs.end() && piter->first == relPos)
            {
                switch (piter->second)
                {
                    case TYPE_TAB:
                    {
                        TabPair tcomp ;
                        tcomp.first = relPos ;
                        auto titer = lower_bound(paraData.tab.begin(), paraData.tab.end(), tcomp, TabCompare) ;
                        if (titer != paraData.tab.end() && titer->first == relPos)
                        {
                            charInfo.tabData = titer->second ;
                        }
                        break ;
                    }

                    case TYPE_FONT:
                    {
                        FontPair fcomp ;
                        fcomp.first = relPos ;
                        auto fiter = lower_bound(paraData.font.begin(), paraData.font.end(), fcomp, FontCompare) ;
                        if (fiter != paraData.font.end() && fiter->first == relPos)
                        {
                            charInfo.fontData = fiter->second ;
                        }
                        break ;
                    }

                    case TYPE_COLOR:
                    {
                        ColorPair ccomp ;
                        ccomp.first = relPos ;
                        auto citer = lower_bound(paraData.color.begin(), paraData.color.end(), ccomp, ColorCompare) ;
                        if (citer != paraData.color.end() && citer->first == relPos)
                        {
                            charInfo.colorData = citer->second ;
                        }
                        break ;
                    }

                    case TYPE_FOOTNOTE:
                    {
                        FootnotePair fncomp ;
                        fncomp.first = relPos ;
                        auto fniter = lower_bound(paraData.footnote.begin(), paraData.footnote.end(), fncomp, FootnoteCompare) ;
                        if (fniter != paraData.footnote.end() && fniter->first == relPos)
                        {
                            charInfo.footnoteData = fniter->second ;
                        }
                        break ;
                    }

                    case TYPE_ENDNOTE:
                    {
                        EndnotePair encomp ;
                        encomp.first = relPos ;
                        auto eniter = lower_bound(paraData.endnote.begin(), paraData.endnote.end(), encomp, EndnoteCompare) ;
                        if (eniter != paraData.endnote.end() && eniter->first == relPos)
                        {
                            charInfo.endnoteData = eniter->second ;
                        }
                        break ;
                    }

                    case TYPE_VARIABLE:
                    {
                        VariablePair vcomp ;
                        vcomp.first = relPos ;
                        auto viter = lower_bound(paraData.variable.begin(), paraData.variable.end(), vcomp, VariableCompare) ;
                        if (viter != paraData.variable.end() && viter->first == relPos)
                        {
                            charInfo.variableData = viter->second ;
                        }
                        break ;
                    }

                    case TYPE_FORMAT:
                    case TYPE_INDEX:
                    case TYPE_SAVED_POSITION:
                    {
                        // format codes / index / saved positions -- codepoint is sufficient
                        break ;
                    }
                }
            }
        }
        else if (firstByte < STYLE_END_OF_STYLES && byteLen == 1)
        {
            // raw control code stored directly (e.g. HARD_RETURN, REPLACE_CHAR)
            charInfo.codepoint = static_cast<CHAR_T>(firstByte) ;
        }
        else
        {
            // regular text -- convert UTF-8 grapheme to char32_t codepoint
            std::string grapheme(paraData.buffer.begin() + byteIndex,
                                paraData.buffer.begin() + byteIndex + byteLen) ;
            std::u32string codepoints ;
            GetCodePoints(grapheme, codepoints) ;
            if (!codepoints.empty())
            {
                charInfo.codepoint = codepoints[0] ;
            }
        }

        result.push_back(charInfo) ;
    }

    return result ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  position  [IN] - document position to re-insert at
/// @param  chars     [IN] - character data to re-insert
///
/// @return nothing
///
/// @brief
/// Re-insert previously captured characters into the document.
/// Uses the appropriate cDocument method for each character type:
/// InsertTab for tabs, InsertFont for fonts, InsertColor for colors,
/// InsertFootnote/InsertEndnote for notes, InsertVariable for variables,
/// and Insert(CHAR_T) for regular characters and format codes.
///
/////////////////////////////////////////////////////////////////////////////
void cDocument::ReinsertCharacters(POSITION_T position, const std::vector<sUndoCharInfo> &chars)
{
    for (size_t i = 0 ; i < chars.size() ; ++i)
    {
        SetPosition(position + static_cast<POSITION_T>(i)) ;

        const sUndoCharInfo &charInfo = chars[i] ;

        if (charInfo.tabData.has_value())
        {
            sWSTab tab = charInfo.tabData.value() ;
            InsertTab(tab) ;
        }
        else if (charInfo.fontData.has_value())
        {
            sInternalFonts font = charInfo.fontData.value() ;
            InsertFont(font) ;
        }
        else if (charInfo.colorData.has_value())
        {
            sSeqRGBColor color = charInfo.colorData.value() ;
            InsertColor(color) ;
        }
        else if (charInfo.footnoteData.has_value())
        {
            sNote note = charInfo.footnoteData.value() ;
            InsertFootnote(note) ;
        }
        else if (charInfo.endnoteData.has_value())
        {
            sNote note = charInfo.endnoteData.value() ;
            InsertEndnote(note) ;
        }
        else if (charInfo.variableData.has_value())
        {
            InsertVariable(charInfo.variableData.value()) ;
        }
        else
        {
            // regular character, HARD_RETURN, or format code (STYLE_BOLD, etc.)
            Insert(charInfo.codepoint) ;
        }
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return bool - true if undo performed, else false if nothing to undo
///
/// @brief
/// Undo the last group of actions. Processes actions in reverse order
/// and pushes the inverse group to the redo stack.
///
/////////////////////////////////////////////////////////////////////////////
bool cDocument::Undo(void)
{
    if (mUndoStack.empty())
    {
        return false ;
    }

    sUndoGroup group = mUndoStack.back() ;
    mUndoStack.pop_back() ;

    mUndoDisabled = true ;

    // process actions in reverse order
    for (auto it = group.actions.rbegin() ; it != group.actions.rend() ; ++it)
    {
        const sUndoAction &action = *it ;

        if (action.type == UNDO_ACTION_INSERT)
        {
            // undo insert = delete the inserted characters
            Delete(action.position, action.length) ;
        }
        else if (action.type == UNDO_ACTION_DELETE)
        {
            // undo delete = re-insert the deleted characters
            ReinsertCharacters(action.position, action.chars) ;
        }
    }

    // restore cursor to position before the group
    SetPosition(group.cursorBefore) ;

    // push the same group to redo stack (redo replays forward)
    mRedoStack.push_back(group) ;

    mUndoDisabled = false ;

    return true ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return bool - true if redo performed, else false if nothing to redo
///
/// @brief
/// Redo the last undone group of actions. Processes actions in forward
/// order and pushes the group back to the undo stack.
///
/////////////////////////////////////////////////////////////////////////////
bool cDocument::Redo(void)
{
    if (mRedoStack.empty())
    {
        return false ;
    }

    sUndoGroup group = mRedoStack.back() ;
    mRedoStack.pop_back() ;

    mUndoDisabled = true ;

    // process actions in forward order
    for (auto &action : group.actions)
    {
        if (action.type == UNDO_ACTION_INSERT)
        {
            // redo insert = re-insert the characters
            ReinsertCharacters(action.position, action.chars) ;
        }
        else if (action.type == UNDO_ACTION_DELETE)
        {
            // redo delete = delete the characters again
            Delete(action.position, action.length) ;
        }
    }

    // restore cursor to position after the group
    SetPosition(group.cursorAfter) ;

    // push the same group back to undo stack
    mUndoStack.push_back(group) ;

    mUndoDisabled = false ;

    return true ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return bool - true if there are actions to undo
///
/// @brief
/// Check if the undo stack has any groups
///
/////////////////////////////////////////////////////////////////////////////
bool cDocument::CanUndo(void) const
{
    return !mUndoStack.empty() ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return bool - true if there are actions to redo
///
/// @brief
/// Check if the redo stack has any groups
///
/////////////////////////////////////////////////////////////////////////////
bool cDocument::CanRedo(void) const
{
    return !mRedoStack.empty() ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Open an undo group. All subsequent RecordAction calls will be
/// accumulated into one group until EndUndoGroup is called. Supports
/// nesting -- only the outermost Begin/End pair creates the group.
///
/////////////////////////////////////////////////////////////////////////////
void cDocument::BeginUndoGroup(void)
{
    if (mUndoDisabled || mIsLoading)
    {
        return ;
    }

    mUndoGroupNesting++ ;

    if (!mUndoGroupOpen)
    {
        mUndoGroupOpen = true ;
        mCurrentGroup = sUndoGroup{} ;
        mCurrentGroup.cursorBefore = mCurrentPosition ;
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Close an undo group. Only the outermost End call pushes the group
/// to the undo stack. If the group has no actions, nothing is pushed.
///
/////////////////////////////////////////////////////////////////////////////
void cDocument::EndUndoGroup(void)
{
    if (mUndoDisabled || mIsLoading)
    {
        return ;
    }

    if (mUndoGroupNesting > 0)
    {
        mUndoGroupNesting-- ;
    }

    if (mUndoGroupNesting == 0 && mUndoGroupOpen)
    {
        mUndoGroupOpen = false ;

        if (!mCurrentGroup.actions.empty())
        {
            mCurrentGroup.cursorAfter = mCurrentPosition ;
            mUndoStack.push_back(mCurrentGroup) ;

            // trim oldest entries if stack exceeds limit
            while (static_cast<int>(mUndoStack.size()) > MAX_UNDO_STEPS)
            {
                mUndoStack.pop_front() ;
            }
        }

        mCurrentGroup = sUndoGroup{} ;
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Clear all undo and redo history. Called after file load.
///
/////////////////////////////////////////////////////////////////////////////
void cDocument::ClearUndoHistory(void)
{
    mUndoStack.clear() ;
    mRedoStack.clear() ;
    mUndoGroupOpen = false ;
    mCurrentGroup = sUndoGroup{} ;
    mUndoGroupNesting = 0 ;
    mUndoDisabled = false ;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  text     [IN]  the string we are finding grapheme boundaries for
///  @param offsets  [OUT] array of grapheme start positions
///
/// @return size_t - the number of graphemes in the string
///
/// @brief
/// Get the count and grapheme boundaries in the string
///
/////////////////////////////////////////////////////////////////////////////
size_t cDocument::GraphemeCount(const std::string &text, std::vector<POSITION_T> &offsets)
{
    offsets.clear() ;
    // if our paragraph is empty
    if(text.length() == 0)
    {
        offsets.emplace_back(0);
        return 0 ;
    }

    // now count our grapheme breaks
    std::u32string text32 = unicode::utf8::decode(text) ;
    size_t length32 = text32.length() ;

    // vars used in loop
    std::string str ;
    size_t len ;
    size_t cumalativelen = 0 ;

    for(size_t loop = 0; loop < length32; loop++)
    {
        if(unicode::is_grapheme_boundary(text32.data(), text32.length(), loop)
            && !unicode::is_combining_character(text32[loop]))
        {
            len = unicode::utf8::encode_codepoint(text32[loop], str) ;
            offsets.push_back(cumalativelen) ;
            cumalativelen += len ;
        }
        else
        {
            len = unicode::utf8::encode_codepoint(text32[loop], str) ;
            cumalativelen += len ;
        }
    }

    return offsets.size() ;
}



/////////////////////////////////////////////////////////////////////////////
///
/// @param  text     [IN]  the buffer we are getting codepoints for
///  @param codepoints  [OUT] array of codepoints
///
/// @return size_t - the number of codepoints in the string
///
/// @brief
/// Get the count and codepoints in the string
///
/////////////////////////////////////////////////////////////////////////////
size_t cDocument::GetCodePoints(const std::string& text, std::u32string& codepoints)
{
    codepoints = unicode::utf8::decode(text) ;

    return codepoints.length() ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  para    [IN] the paragraph we are looking at
/// @param wordstarts [OUT] the position in the buffer of each word
///
/// @return size_t - number of words (including end of paragraph)
///
/// @brief
/// Get the number of words in a paragraph. Adds an extra marker for the end
/// of paragraph
///
/////////////////////////////////////////////////////////////////////////////
size_t cDocument::GetWordPositions(PARAGRAPH_T para, std::vector<POSITION_T> &wordstarts)
{
    MY_ASSERT(para >= 0)
    MY_ASSERT(para < static_cast<POSITION_T>(mParagraphData.size()))

    std::string text = GetParagraphText(para) ;

    std::u32string utf32 = unicode::utf8::decode(text) ;

    // Replace MARKER_CHAR, REPLACE_CHAR and SAVE_CHAR with 'A' so the word
    // boundary algorithm treats them as regular letters (not break points).
    // Without this, markers create false word boundaries mid-word and can
    // prevent lines from wrapping at all.
    for (size_t i = 0; i < utf32.length(); i++)
    {
        if (utf32[i] == static_cast<char32_t>(MARKER_CHAR) ||
            utf32[i] == static_cast<char32_t>(REPLACE_CHAR) ||
            utf32[i] == static_cast<char32_t>(SAVE_CHAR))
        {
            utf32[i] = U'A';
        }
    }

    POSITION_T size = utf32.length() ;

    // Step 1: Collect paragraph-relative word positions
    std::vector<POSITION_T> paraRelativePositions;

    if(size >= 0)
    {
        POSITION_T i = 0 ;

        while (i < size)
        {
            while (i < size && !unicode::is_cased(utf32[i]))
            {
                i++;
            }

            if (i == size)
            {
                break;
            }

            paraRelativePositions.push_back(i) ;
            i++;

            if (i == size)
            {
              break;
            }

            while (i < size && !unicode::is_word_boundary(reinterpret_cast<const char32_t *>(&utf32[0]), size, i))
            {
                i++;
            }
        }

        paraRelativePositions.push_back(size - 1) ;
    }

    // Step 2: Filter out space positions (FIX for missing space bug)
    // unicode::is_word_boundary() marks both sides of spaces, causing splits AT spaces
    // This filters out the space positions, keeping only actual word starts

    paraRelativePositions.erase(
        std::remove_if(paraRelativePositions.begin(), paraRelativePositions.end(),
            [&utf32](POSITION_T pos) {
                return pos < static_cast<POSITION_T>(utf32.length()) && unicode::is_white_space(utf32[pos]);
            }),
        paraRelativePositions.end()
    );

    // Step 3: Convert to document-relative positions and add to output
    for (POSITION_T pos : paraRelativePositions)
    {
        wordstarts.push_back(pos + mParagraphData[para].index);
    }

    return wordstarts.size() ;
}




/////////////////////////////////////////////////////////////////////////////
///
/// @param  para    [IN] the paragraph we are looking at
/// @param wordstarts [OUT] the position in the buffer of each word
///
/// @return size_t - number of words (including end of paragraph)
///
/// @brief
/// Get the number of words in the passed in text. Adds an extra marker for the end
/// of paragraph
///
/// MARKER_CHAR should be stripped out before calling this function
///
/////////////////////////////////////////////////////////////////////////////
size_t cDocument::GetWordPositions(std::string text, std::vector<POSITION_T> &wordstarts)
{

//    std::string text = GetParagraphText(para) ;

    std::u32string utf32 = unicode::utf8::decode(text) ;
    POSITION_T size = utf32.length() ;

    if(size >= 0)
    {
        POSITION_T i = 0 ;

        while (i < size)
        {
            while (i < size && !unicode::is_word_boundary(reinterpret_cast<const char32_t *>(&utf32[0]), size, i))
            {
                i++;
            }

            if (i == size)
            {
                break;
            }

            wordstarts.push_back(i) ;
            i++;

            if (i == size)
            {
              break;
            }

        }

        wordstarts.push_back(size - 1 ) ;
    }

    // remove every second item, as we only need word starts
    // I can do this because we strip out all MARKER_CHARS first
    for (size_t i = 1; i < wordstarts.size(); i += 2)
    {
        wordstarts.erase(wordstarts.begin() + i);
        --i; // Adjust index after erasing
    }

    // FIX: Remove any word boundaries that point to space characters
    // is_word_boundary() marks both sides of spaces, causing splits AT spaces
    // This filters out the space positions, keeping only actual word starts
    wordstarts.erase(
        std::remove_if(wordstarts.begin(), wordstarts.end(),
            [&utf32](POSITION_T pos) {
                return pos < static_cast<POSITION_T>(utf32.length()) && unicode::is_white_space(utf32[pos]);
            }),
        wordstarts.end()
    );

    return wordstarts.size() ;
}




/////////////////////////////////////////////////////////////////////////////
///
/// @param  pos     [IN]  our current caret position
///
/// @return POSITION_T - position of the first grapheme of the next word
///
/// @brief
/// Find the start of the next word in the paragraph.
///
/////////////////////////////////////////////////////////////////////////////
POSITION_T cDocument::GetNextWordPosition(POSITION_T pos)
{
    pos++ ;

    MY_ASSERT(pos >= 0)
//    MY_ASSERT(pos < GetTextSize())

    PARAGRAPH_T para = GetParagraphFromPosition(pos) ;
    std::vector<POSITION_T> wordstarts ;
    POSITION_T retpos = pos ;

    GetWordPositions(para, wordstarts) ;

    for(size_t loop = 0; loop < wordstarts.size(); loop++)
    {
        if(wordstarts[loop] > pos)
        {
            retpos = wordstarts[loop] ;
            break ;
        }
    }

    return retpos ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  pos     [IN]  the buffer we start looking for word breaks
///
/// @return POSITION_T - position of the first grapheme of the prev word
///
/// @brief
/// Find the start of the prev word in the paragraph.
///
/////////////////////////////////////////////////////////////////////////////
POSITION_T cDocument::GetPrevWordPosition(POSITION_T pos)
{
    pos-- ;

    if(pos <= 0)
    {
        return 0 ;
    }

    MY_ASSERT(pos < GetTextSize())

    PARAGRAPH_T para = GetParagraphFromPosition(pos) ;
    std::vector<POSITION_T> wordstarts ;
    POSITION_T retpos = pos ;

    GetWordPositions(para, wordstarts) ;

    for(auto iter = wordstarts.rbegin() ; iter != wordstarts.rend(); iter++)
    {
        if(*iter < pos)
        {
            retpos = *iter ;
            break ;
        }
    }

    return retpos ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  pos     [IN]  document position to start from
///
/// @return POSITION_T - position just past the end of the current token
///
/// @brief
/// Find the end of the current word or whitespace run. If pos is at a
/// word character, advances past word characters and stops at the first
/// non-word character. If pos is at whitespace, advances past whitespace
/// and stops at the first non-whitespace character. For other characters
/// (punctuation), advances by one position.
///
/// Used by DeleteWordRight to delete only the word without trailing space.
///
/////////////////////////////////////////////////////////////////////////////
POSITION_T cDocument::GetWordEndPosition(POSITION_T pos)
{
    POSITION_T textSize = GetTextSize();
    if (pos >= textSize)
    {
        return pos;
    }

    PARAGRAPH_T para = GetParagraphFromPosition(pos);

    MY_ASSERT(para >= 0)
    MY_ASSERT(para < static_cast<POSITION_T>(mParagraphData.size()))

    std::string text = GetParagraphText(para);
    std::u32string utf32 = unicode::utf8::decode(text);

    POSITION_T size = static_cast<POSITION_T>(utf32.length());
    POSITION_T paraStart = mParagraphData[para].index;
    POSITION_T relPos = pos - paraStart;

    // Guard: if past end of this paragraph or at the paragraph terminator
    // (MARKER_CHAR), return pos unchanged
    if (relPos >= size)
    {
        return pos;
    }

    if (utf32[relPos] == static_cast<char32_t>(MARKER_CHAR) ||
        utf32[relPos] == static_cast<char32_t>(REPLACE_CHAR) ||
        utf32[relPos] == static_cast<char32_t>(SAVE_CHAR))
    {
        return pos;
    }

    // Word character definition: alphabetic, numeric, or underscore
    // Uses unicode::is_alphabetic() to cover all scripts (Latin, CJK, Arabic, etc.)
    // and unicode::is_number() for all number systems
    auto isWordChar = [](char32_t ch) -> bool
    {
        return unicode::is_alphabetic(ch) || unicode::is_number(ch) || ch == U'_';
    };

    if (isWordChar(utf32[relPos]))
    {
        // At a word character: advance past all word characters
        while (relPos < size && isWordChar(utf32[relPos]))
        {
            relPos++;
        }
    }
    else if (unicode::is_white_space(utf32[relPos]))
    {
        // At whitespace: advance past all whitespace
        while (relPos < size && unicode::is_white_space(utf32[relPos]))
        {
            relPos++;
        }
    }
    else
    {
        // Punctuation or other: advance one character
        relPos++;
    }

    return paraStart + relPos;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return POSITION_T - the position of the next font tag, or the current position if none found
///
/// @brief
/// Find the next font tag in the document
///
/////////////////////////////////////////////////////////////////////////////
POSITION_T cDocument::GetNextFontTagPosition(void)
{

    PARAGRAPH_T numpara = GetNumberofParagraphs() ;
    POSITION_T pos = GetPosition() ;
    PARAGRAPH_T para = GetParagraphFromPosition(pos) ;

    // go through paragraph from current to end of list
    for(PARAGRAPH_T ploop = para; ploop < numpara; ploop++)
    {
        size_t numfonts = mParagraphData[ploop].font.size() ;
        if(numfonts != 0)
        {
            for(size_t floop = 0; floop < numfonts; floop++)
            {
                POSITION_T fpos = mParagraphData[ploop].font[floop].first ;
                if(fpos + mParagraphData[ploop].index > pos)
                {
                    pos = fpos + mParagraphData[ploop].index ;
                    ploop = numpara ;       // get out of outer loop
                    break ;
                }
            }
        }
    }

    return pos ;
}




/////////////////////////////////////////////////////////////////////////////
///
/// @param  str     [IN]  the string to normalize in UTF8
///
/// @return string - the normalized string in UTF8
///
/// @brief
/// NFC normalize a UTF8 string
///
/////////////////////////////////////////////////////////////////////////////
std::string cDocument::Normalize(const std::string &str)
{
    // normalize the string
    std::u32string str32 = unicode::utf8::decode(str) ;
    str32 = unicode::to_nfc(str32) ;

    return unicode::utf8::encode(str32) ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  str     [IN]  the string to normalize in UTF8
///
/// @return string - the normalized string in UTF32
///
/// @brief
/// NFC normalize the UTF8 string to UTF32
///
/////////////////////////////////////////////////////////////////////////////
std::u32string cDocument::NormalizeToUTF32(const std::string &str)
{
    // normalize the string
    std::u32string str32 = unicode::utf8::decode(str) ;
    str32 = unicode::to_nfc(str32) ;

    return str32 ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  str     [IN]  the string to normalize in UTF32
///
/// @return string - the normalized string in UTF8
///
/// @brief
/// NFC normalize the UTF32 string to UTF8
///
/////////////////////////////////////////////////////////////////////////////
std::string cDocument::NormalizeToUTF8(const std::u32string &str32)
{
    std::u32string nstr32 = unicode::to_nfc(str32) ;
    std::string str = unicode::utf8::encode(nstr32) ;

    return str ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  str     [IN]  the string to lowercase
///
/// @return string - the lowercased string
///
/// @brief
/// lowercase the string
///
/////////////////////////////////////////////////////////////////////////////
std::string cDocument::LowerCase(const std::string &str)
{
    std::u32string str32 = NormalizeToUTF32(str) ;
    str32 = unicode::to_lowercase(str32) ;
    str32 = unicode::to_nfc(str32) ;

    return unicode::utf8::encode(str32) ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  str     [IN]  the string to uppercase
///
/// @return string - the uppercased string
///
/// @brief
/// uppercase the string
///
/////////////////////////////////////////////////////////////////////////////
std::string cDocument::UpperCase(const std::string &str)
{
    std::u32string str32 = NormalizeToUTF32(str) ;
    str32 = unicode::to_uppercase(str32) ;
    str32 = unicode::to_nfc(str32) ;

    return unicode::utf8::encode(str32) ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  str     [IN]  the string to titlecase
///
/// @return string - the titlecased string
///
/// @brief
/// titlecase the string
///
/////////////////////////////////////////////////////////////////////////////
std::string cDocument::TitleCase(const std::string &str)
{
    std::u32string str32 = NormalizeToUTF32(str) ;
    str32 = unicode::to_titlecase(str32) ;
    str32 = unicode::to_nfc(str32) ;

    return unicode::utf8::encode(str32) ;
}



/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//
// PRIVATE
//
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////



/////////////////////////////////////////////////////////////////////////////
///
/// @param  ch   [IN] - the control charcater to insert
///
/// @return nothing
///
/// @brief
/// Insert the control into the right table and update the main lookup
/// table. ONLY DEAL WITH bold, italic, etc)
///
/////////////////////////////////////////////////////////////////////////////
void cDocument::SetControlChar(CHAR_T ch)
{
    POSITION_T pos = GetPosition() ;
    PARAGRAPH_T currentparagraphnumber = GetParagraphFromPosition(pos) ;
    auto paraiter = mParagraphData.begin() + currentparagraphnumber ;
    pos -= paraiter->index ;        // makeposition relative to paragraph start

    switch(ch)
    {
        case STYLE_TAB :                    // these styles get taken care of in their respective methods
        case STYLE_INTERNAL_COLOR :
        case STYLE_FONT1 :
            break ;


        default :
        {
            // first create the pairs for the main lookup table and the format modifier (sorted)
            PairTable t = std::make_pair(pos, TYPE_FORMAT) ;
            FormatPair t1 = std::make_pair(pos,  static_cast<eModifiers>(ch)) ;

            auto iter = lower_bound(paraiter->pairs.begin(), paraiter->pairs.end(), t, TableCompare) ;
            auto iter1 = lower_bound(paraiter->format.begin(), paraiter->format.end(), t1, FormatCompare) ;

            PARAGRAPH_T index = static_cast<PARAGRAPH_T>(distance(paraiter->format.begin(), iter1)) ;
            paraiter->format.insert(paraiter->format.begin() + index, t1) ;

            index = static_cast<PARAGRAPH_T>(distance(paraiter->pairs.begin(), iter)) ;
            paraiter->pairs.insert(paraiter->pairs.begin() + index, t) ;

            break ;
        }
    }
}




/////////////////////////////////////////////////////////////////////////////
///
/// @param  position   [IN] - the position to get character from
///
/// @return ch - the character
///
/// @brief
/// Get the control from the right table
///
/////////////////////////////////////////////////////////////////////////////
eModifiers cDocument::GetControlChar(POSITION_T position)
{
    PARAGRAPH_T currentparagraphnumber = GetParagraphFromPosition(position) ;
    auto paraiter = mParagraphData.begin() + currentparagraphnumber ;
    auto iter = paraiter->pairs.begin() ;
    eModifiers ch = STYLE_END_OF_STYLES ;

    // we'll make sure its in our pairs table. If it is, we assume its in the other table(s)
    PairTable pcomp ;
    position -= paraiter->index ;
    pcomp.first = position ;

    iter = lower_bound(paraiter->pairs.begin(), paraiter->pairs.end(), pcomp, TableCompare) ;
    if(iter != paraiter->pairs.end() && !(position < iter->first))
    {
        switch(iter->second)
        {
            case TYPE_FORMAT :
                {
                    FormatPair comp ;
                    comp.first = position ;
                    auto  iter1 = lower_bound(paraiter->format.begin(), paraiter->format.end(), comp, FormatCompare) ;
                    ch = iter1->second ;
                }
                break ;

            case TYPE_TAB :
                ch = STYLE_TAB ;
                break ;

            case TYPE_COLOR :
                ch = STYLE_INTERNAL_COLOR ;
                break ;

            case TYPE_FONT :
                ch = STYLE_FONT1 ;
                break ;
                
            case TYPE_INDEX :
                ch = STYLE_INDEX ;
                break ;

            // non standard styles
            case TYPE_FOOTNOTE :
                ch = STYLE_FOOTNOTE ;
                break ;

            case TYPE_ENDNOTE :
                ch = STYLE_ENDNOTE ;
                break ;

            case TYPE_SAVED_POSITION :
                ch = STYLE_CTRL_O ;
                break ;

            case TYPE_VARIABLE:
                ch = STYLE_VARIABLE;
                break;

        }
    }

    return ch ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  para   [IN] - the paragraph to get
///
/// @return string - the paragraph in UTF8
///
/// @brief
/// dump the entire paragraph into a string. MARKER_CHAR is not converted
/// to it's STYLE_* value.
///
/////////////////////////////////////////////////////////////////////////////
std::string cDocument::GetParagraphText(PARAGRAPH_T para)
{
    std::string text ;

    MY_ASSERT(para >= 0)

    if(static_cast<size_t>(para) < mParagraphData.size())
    {
        for(char c: mParagraphData[para].buffer)
        {
            text += c ;
        }
    }
    return text ;
}



/////////////////////////////////////////////////////////////////////////////
///
/// @param  para   [IN] - the paragraph to get
/// @param  offsets [OUT] - the offset positions
///
/// @return size_t - number of offsets
///
/// @brief
/// Copy the paragraph offsets (cached) to the passed in vector
///
/////////////////////////////////////////////////////////////////////////////
size_t cDocument::GetParagraphGraphemeOffsets(const PARAGRAPH_T &para, std::vector<POSITION_T> &offsets)
{
    if (static_cast<size_t>(para) >= mParagraphData.size())
    {
        offsets.clear();
        return 0;
    }

    mParagraphData[static_cast<size_t>(para)].offsets.CopyTo(offsets) ;
    return offsets.size();
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  para [in] - paragraph number
/// @param  graphemes [out] - vector to populate with extracted graphemes
/// @param  offsets [out] - vector to populate with byte offsets
///
/// @return nothing
///
/// @brief
/// Gets both graphemes and offsets for a paragraph in one call.
///
/// This is the recommended API for layout and iteration. It pre-extracts
/// graphemes as individual strings, which is efficient due to Small String
/// Optimization (SSO). Typical graphemes are 1-4 bytes and fit in the
/// 15-23 byte SSO buffer on all platforms (macOS/Linux/Windows).
///
/// Result: Zero heap allocations for typical text.
///
/// Each entry in the graphemes vector represents ONE complete grapheme
/// (user-visible character), which may be 1-4 bytes in UTF-8 encoding.
/// The offsets vector contains byte positions where each grapheme starts.
///
/// @see GetParagraphText() for raw UTF-8 bytes
/// @see GetParagraphGraphemeOffsets() for offsets only
///
/////////////////////////////////////////////////////////////////////////////
void cDocument::GetParagraphGraphemes(PARAGRAPH_T para, std::vector<std::string>& graphemes, std::vector<POSITION_T>& offsets)
{
    // Get the paragraph text as UTF-8 bytes
    std::string text = GetParagraphText(para);

    // Get the cached grapheme boundary offsets
    GetParagraphGraphemeOffsets(para, offsets);

    // Guard against empty text with stale offsets from layout
    if (text.empty())
    {
        offsets.clear();
        graphemes.clear();
        return;
    }

    // Pre-allocate graphemes vector for efficiency
    graphemes.clear();
    graphemes.reserve(offsets.size());

    // Extract each grapheme as a complete string
    // SSO (Small String Optimization) prevents heap allocation for graphemes < 15-23 bytes
    // Typical graphemes: 1 byte (ASCII), 2 bytes (e-acute), 3 bytes (U+4E16), 4 bytes (emoji)
    for (size_t i = 0; i < offsets.size(); ++i)
    {
        size_t byteStart = offsets[i];

        // Bounds check: stale offsets may exceed current text length
        if (byteStart >= text.length())
        {
            break;
        }

        size_t byteEnd = (i + 1 < offsets.size()) ? offsets[i + 1] : text.length();

        // Clamp end to text length for safety
        if (byteEnd > text.length())
        {
            byteEnd = text.length();
        }

        // Extract one complete grapheme
        std::string grapheme = text.substr(byteStart, byteEnd - byteStart);
        graphemes.push_back(std::move(grapheme));
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  start   [IN] - the start of the block
/// @param  end     [IN] - the end position of the block
///
/// @return string - the block in UTF8
///
/// @brief
/// dump the marked block into a string
///
/////////////////////////////////////////////////////////////////////////////
std::string cDocument::GetBlockText(POSITION_T start, POSITION_T end)
{
    std::string ret ;

    for(POSITION_T loop = start; loop < end; loop++)
    {
        ret += GetChar(loop) ;
    }

    return ret ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  start [OUT] Reference to a POSITION_T variable where the start position will be stored.
/// @param  end   [OUT] Reference to a POSITION_T variable where the end position will be stored.
///
/// @return true if the block is set, false otherwise.
///
/// @brief Retrieves the start and end positions of the current block.
///
/// This function assigns the start and end positions of the current block
/// to the provided references and returns whether the block is set.
///
/////////////////////////////////////////////////////////////////////////////
bool cDocument::GetBlock(POSITION_T &start, POSITION_T &end)
{
    start = mStartBlock ;
    end = mEndBlock ;
    return mBlockSet ;
}



/////////////////////////////////////////////////////////////////////////////
///
/// @param  start [OUT] Reference to a POSITION_T variable where the start position will be stored.
/// @param  end   [OUT] Reference to a POSITION_T variable where the end position will be stored.
///
/// @return true if the previous block is set, false otherwise.
///
/// @brief Retrieves the start and end positions of the previous block.
///
/// This function assigns the start and end positions of the previous block
/// to the provided references and returns whether the previous block is set.
///
/////////////////////////////////////////////////////////////////////////////
bool cDocument::GetPreviousBlock(POSITION_T &start, POSITION_T &end)
{
    start = mOldStartBlock ;
    end = mOldEndBlock ;
    return mOldBlockSet ;
}



/////////////////////////////////////////////////////////////////////////////
///
/// @return none
///
/// @brief Saves the block positions for later restore
///
/////////////////////////////////////////////////////////////////////////////
void cDocument::SaveBlocks(void)
{
    mSavedBlocks[0] = mStartBlock ;
    mSavedBlocks[1] = mEndBlock ;
    mSavedBlocks[2] = mOldStartBlock ;
    mSavedBlocks[3] = mOldEndBlock ;
    mSavedBlockSet[0] = mBlockSet ;
    mSavedBlockSet[1] = mOldBlockSet ;
}



/////////////////////////////////////////////////////////////////////////////
///
/// @return none
///
/// @brief restores the previous saved blocks
///
/////////////////////////////////////////////////////////////////////////////
void cDocument::RestoreBlocks(void)
{
    mStartBlock = mSavedBlocks[0] ;
    mEndBlock = mSavedBlocks[1] ;
    mOldStartBlock = mSavedBlocks[2] ;
    mOldEndBlock = mSavedBlocks[3] ;
    mBlockSet = mSavedBlockSet[0] ;
    mOldBlockSet = mSavedBlockSet[1] ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  needle      [IN} - the string we are looking for
/// @param  start       [IN] - the position in the buffer to start the search
/// @param  wildcard    [IN] - allow wildcard search (brute force, ascii only)
/// @param  casecmp     [IN] - ignore case (brute force, ascii only)
/// @param  wholeword   [IN] - find only whole words
///
/// @return POSITION_T
///
/// @brief search the buffer starting at position start for the string in
///        needle
///
/////////////////////////////////////////////////////////////////////////////
POSITION_T cDocument::FindNext(const std::string &needle, const POSITION_T &start, bool wildcard, bool casecmp, bool wholeword)
{
    // NFC-normalize the needle so it matches the document, which is stored NFC
    // (Insert normalizes), then split into grapheme clusters. Matching is done
    // grapheme-by-grapheme so '?' matches one grapheme, case-folding is per
    // grapheme, and positions are grapheme indices (what POSITION_T means).
    std::string nee = Normalize(needle);
    std::vector<std::string> needleGraphemes = splitGraphemes(nee);
    if (needleGraphemes.empty())
    {
        return GetTextSize();
    }
    std::vector<std::string> needleFolded;
    if (casecmp)
    {
        needleFolded.reserve(needleGraphemes.size());
        for (const std::string &grapheme : needleGraphemes)
        {
            needleFolded.push_back(foldGrapheme(grapheme));
        }
    }

    PARAGRAPH_T startpara = GetParagraphFromPosition(start);

    POSITION_T adjustedStart = start;
    for (PARAGRAPH_T ploop = startpara; ploop < GetNumberofParagraphs(); ploop++)
    {
        POSITION_T index = mParagraphData[ploop].index;
        if (adjustedStart < index)
        {
            adjustedStart = index;
        }
        std::string haystack = GetParagraphText(ploop);

        // don't search commands or comment if we are not displaying them
        if(GetShowControl() == SHOW_NONE)
        {
            if(!haystack.empty())
            {
                if (haystack.front() == '.')
                {
                    continue ;
                }
            }
        }

        std::vector<std::string> haystackGraphemes = splitGraphemes(haystack);
        std::vector<std::string> haystackFolded;
        if (casecmp)
        {
            haystackFolded.reserve(haystackGraphemes.size());
            for (const std::string &grapheme : haystackGraphemes)
            {
                haystackFolded.push_back(foldGrapheme(grapheme));
            }
        }

        // Codepoint array + grapheme->codepoint index map for whole-word checks.
        std::u32string haystackCodepoints;
        std::vector<size_t> codepointIndexOf(haystackGraphemes.size() + 1, 0);
        for (size_t graphemeIndex = 0; graphemeIndex < haystackGraphemes.size(); graphemeIndex++)
        {
            std::u32string codepoints = unicode::utf8::decode(haystackGraphemes[graphemeIndex]);
            codepointIndexOf[graphemeIndex + 1] = codepointIndexOf[graphemeIndex] + codepoints.size();
            haystackCodepoints += codepoints;
        }

        size_t startGrapheme = static_cast<size_t>(adjustedStart - index);

        POSITION_T matchGrapheme = graphemeSearch(haystackGraphemes, haystackFolded, needleGraphemes, needleFolded, wildcard, casecmp, false, startGrapheme);
        while (matchGrapheme != (POSITION_T)std::string::npos)
        {
            size_t matchIndex = static_cast<size_t>(matchGrapheme);
            size_t matchEndIndex = matchIndex + needleGraphemes.size();
            if (wholeword)
            {
                bool leftBoundary = isExtendedWordBoundary(haystackCodepoints.c_str(), haystackCodepoints.size(), codepointIndexOf[matchIndex]);
                bool rightBoundary = isExtendedWordBoundary(haystackCodepoints.c_str(), haystackCodepoints.size(), codepointIndexOf[matchEndIndex]);
                if (!leftBoundary || !rightBoundary)
                {
                    matchGrapheme = graphemeSearch(haystackGraphemes, haystackFolded, needleGraphemes, needleFolded, wildcard, casecmp, false, matchIndex + 1);
                    continue;
                }
            }
            return matchGrapheme + index;
        }
    }
    return GetTextSize();
}



/////////////////////////////////////////////////////////////////////////////
///
/// @param  needle      [IN} - the string we are looking for
/// @param  start       [IN] - the position in the buffer to start the search
/// @param  wildcard    [IN] - allow wildcard search (brute force, ascii only)
/// @param  casecmp     [IN] - ignore case (brute force, ascii only)
/// @param  wholeword   [IN] - find only whole words
///
/// @return POSITION_T
///
/// @brief search the buffer backwards starting at position start for the string in
///        needle
///
/////////////////////////////////////////////////////////////////////////////
POSITION_T cDocument::FindPrev(const std::string &needle, const POSITION_T &start, bool wildcard, bool casecmp, bool wholeword)
{
    // NFC-normalize the needle and split into grapheme clusters (see FindNext).
    std::string nee = Normalize(needle);
    std::vector<std::string> needleGraphemes = splitGraphemes(nee);
    if (needleGraphemes.empty())
    {
        return std::string::npos;
    }
    std::vector<std::string> needleFolded;
    if (casecmp)
    {
        needleFolded.reserve(needleGraphemes.size());
        for (const std::string &grapheme : needleGraphemes)
        {
            needleFolded.push_back(foldGrapheme(grapheme));
        }
    }

    PARAGRAPH_T startpara = GetParagraphFromPosition(start);
    POSITION_T adjustedStart = (start > 0) ? start - 1 : 0;
    for (PARAGRAPH_T ploop = startpara; ploop >= 0; ploop--)
    {
        POSITION_T index = mParagraphData[ploop].index;
        if (adjustedStart < index)
        {
            adjustedStart = index;
        }
        std::string haystack = GetParagraphText(ploop);

        // don't search commands or comment if we are not displaying them
        if(GetShowControl() == SHOW_NONE)
        {
            if(!haystack.empty())
            {
                if (haystack.front() == '.')
                {
                    continue ;
                }
            }
        }

        std::vector<std::string> haystackGraphemes = splitGraphemes(haystack);
        std::vector<std::string> haystackFolded;
        if (casecmp)
        {
            haystackFolded.reserve(haystackGraphemes.size());
            for (const std::string &grapheme : haystackGraphemes)
            {
                haystackFolded.push_back(foldGrapheme(grapheme));
            }
        }

        // Codepoint array + grapheme->codepoint index map for whole-word checks.
        std::u32string haystackCodepoints;
        std::vector<size_t> codepointIndexOf(haystackGraphemes.size() + 1, 0);
        for (size_t graphemeIndex = 0; graphemeIndex < haystackGraphemes.size(); graphemeIndex++)
        {
            std::u32string codepoints = unicode::utf8::decode(haystackGraphemes[graphemeIndex]);
            codepointIndexOf[graphemeIndex + 1] = codepointIndexOf[graphemeIndex] + codepoints.size();
            haystackCodepoints += codepoints;
        }

        size_t startGrapheme = static_cast<size_t>(adjustedStart - index);

        POSITION_T matchGrapheme = graphemeSearch(haystackGraphemes, haystackFolded, needleGraphemes, needleFolded, wildcard, casecmp, true, startGrapheme);
        while (matchGrapheme != (POSITION_T)std::string::npos)
        {
            size_t matchIndex = static_cast<size_t>(matchGrapheme);
            size_t matchEndIndex = matchIndex + needleGraphemes.size();
            if (wholeword)
            {
                bool leftBoundary = isExtendedWordBoundary(haystackCodepoints.c_str(), haystackCodepoints.size(), codepointIndexOf[matchIndex]);
                bool rightBoundary = isExtendedWordBoundary(haystackCodepoints.c_str(), haystackCodepoints.size(), codepointIndexOf[matchEndIndex]);
                if (!leftBoundary || !rightBoundary)
                {
                    if (matchIndex == 0) break;
                    matchGrapheme = graphemeSearch(haystackGraphemes, haystackFolded, needleGraphemes, needleFolded, wildcard, casecmp, true, matchIndex - 1);
                    continue;
                }
            }
            return matchGrapheme + index;
        }
    }
    return std::string::npos ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  position    [IN] - the position in the buffer
///
/// @return nothing
///
/// @brief Delete a tab entry from the tabs table
///
/// This should never be called directly, only from Delete()
/////////////////////////////////////////////////////////////////////////////
void cDocument::DeleteTab(POSITION_T position)
{
    MY_ASSERT(position >= 0)
    MY_ASSERT(position <= GetTextSize())

    // we'll make sure its in our pairs table. If it is, we assume its in the other table(s)
    PairTable pcomp ;
    PARAGRAPH_T currentparagraphnumber = GetParagraphFromPosition(position) ;
    auto paraiter = mParagraphData.begin() + currentparagraphnumber ;
    position -= paraiter->index ;
    pcomp.first = position ;

    auto iter = lower_bound(paraiter->pairs.begin(), paraiter->pairs.end(), pcomp, TableCompare) ;
    if(iter != paraiter->pairs.end())
    {
        pcomp.second = iter->second ;

        if(iter->first == position)
        {
            if(iter->second == TYPE_TAB)
            {
                TabPair t1 ;
                t1.first = position ;

                auto iter1 = lower_bound(paraiter->tab.begin(), paraiter->tab.end(), t1, TabCompare) ;
                if(iter1 != paraiter->tab.end() && iter1->first == position)
                {
                    paraiter->tab.erase(iter1) ;
                    paraiter->pairs.erase(iter) ;
                }
            }
        }
    }
}




/////////////////////////////////////////////////////////////////////////////
///
/// @param  position    [IN] - the position in the buffer
///
/// @return nothing
///
/// @brief Delete a font entry from the fonts table
///
/// This should never be called directly, only from Delete()
/////////////////////////////////////////////////////////////////////////////
void cDocument::DeleteFont(POSITION_T position)
{
    MY_ASSERT(position >= 0)
    MY_ASSERT(position <= GetTextSize())

    // we'll make sure its in our pairs table. If it is, we assume its in the other table(s)
    PairTable pcomp ;
    PARAGRAPH_T currentparagraphnumber = GetParagraphFromPosition(position) ;
    auto paraiter = mParagraphData.begin() + currentparagraphnumber ;
    position -= paraiter->index ;
    pcomp.first = position ;

    auto iter = lower_bound(paraiter->pairs.begin(), paraiter->pairs.end(), pcomp, TableCompare) ;
    if(iter != paraiter->pairs.end())
    {
        pcomp.second = iter->second ;

        if(iter->first == position)
        {
            if(iter->second == TYPE_FONT)
            {
                FontPair t1 ;
                t1.first = position ;

                auto iter1 = lower_bound(paraiter->font.begin(), paraiter->font.end(), t1, FontCompare) ;
                if(iter1 != paraiter->font.end() && iter1->first == position)
                {
                    paraiter->font.erase(iter1) ;
                    paraiter->pairs.erase(iter) ;
                }
            }
        }
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  position [in] document position to delete variable from
///
/// @return nothing
///
/// @brief
/// Deletes a variable marker from the given position.
/// Removes entries from both the variable and pairs tables.
///
/////////////////////////////////////////////////////////////////////////////
void cDocument::DeleteVariable(POSITION_T position)
{
    MY_ASSERT(position >= 0)
    MY_ASSERT(position <= GetTextSize())

    PairTable pcomp;
    PARAGRAPH_T currentparagraphnumber = GetParagraphFromPosition(position);
    auto paraiter = mParagraphData.begin() + currentparagraphnumber;
    position -= paraiter->index;
    pcomp.first = position;

    auto iter = lower_bound(paraiter->pairs.begin(), paraiter->pairs.end(), pcomp, TableCompare);
    if (iter != paraiter->pairs.end())
    {
        if (iter->first == position && iter->second == TYPE_VARIABLE)
        {
            VariablePair t1;
            t1.first = position;

            auto iter1 = lower_bound(paraiter->variable.begin(), paraiter->variable.end(), t1, VariableCompare);
            if (iter1 != paraiter->variable.end() && iter1->first == position)
            {
                paraiter->variable.erase(iter1);
                paraiter->pairs.erase(iter);
            }
        }
    }
}



/////////////////////////////////////////////////////////////////////////////
///
/// @param  position    [IN] - the position in the buffer
///
/// @return nothing
///
/// @brief Delete a color entry from the colorss table
///
/// This should never be called directly, only from Delete()
/////////////////////////////////////////////////////////////////////////////
void cDocument::DeleteColor(POSITION_T position)
{
    MY_ASSERT(position >= 0)
    MY_ASSERT(position <= GetTextSize())

    // we'll make sure its in our pairs table. If it is, we assume its in the other table(s)
    PairTable pcomp ;
    PARAGRAPH_T currentparagraphnumber = GetParagraphFromPosition(position) ;
    auto paraiter = mParagraphData.begin() + currentparagraphnumber ;
    position -= paraiter->index ;
    pcomp.first = position ;

    auto iter = lower_bound(paraiter->pairs.begin(), paraiter->pairs.end(), pcomp, TableCompare) ;
    if(iter != paraiter->pairs.end())
    {
        pcomp.second = iter->second ;

        if(iter->first == position)
        {
            if(iter->second == TYPE_COLOR)
            {
                ColorPair t1 ;
                t1.first = position ;

                auto iter1 = lower_bound(paraiter->color.begin(), paraiter->color.end(), t1, ColorCompare) ;
                if(iter1 != paraiter->color.end() && iter1->first == position)
                {
                    paraiter->color.erase(iter1) ;
                    paraiter->pairs.erase(iter) ;
                }
            }
        }
    }
}




/////////////////////////////////////////////////////////////////////////////
///
/// @param  position    [IN] - the position in the buffer
///
/// @return nothing
///
/// @brief Delete a control char entry from the control chars table
///
/// This should never be called directly, only from Delete()
/////////////////////////////////////////////////////////////////////////////
void cDocument::DeleteControlChar(POSITION_T position)
{
    MY_ASSERT(position  >= 0)
    MY_ASSERT(position <= GetTextSize())

    // we'll make sure its in our pairs table. If it is, we assume its in the other table(s)
    PairTable pcomp ;
    PARAGRAPH_T currentparagraphnumber = GetParagraphFromPosition(position) ;
    auto paraiter = mParagraphData.begin() + currentparagraphnumber ;
    position -= paraiter->index ;
    pcomp.first = position ;

    auto iter = lower_bound(paraiter->pairs.begin(), paraiter->pairs.end(), pcomp, TableCompare) ;
    if(iter != paraiter->pairs.end())
    {
        pcomp.second = iter->second ;

        if(iter->first == position)
        {
            if(iter->second == TYPE_FORMAT)
            {
                FormatPair t1 ;
                t1.first = position ;

                auto iter1 = lower_bound(paraiter->format.begin(), paraiter->format.end(), t1, FormatCompare) ;
                if(iter1 != paraiter->format.end() && iter1->first == position)
                {
                    paraiter->format.erase(iter1) ;
                    paraiter->pairs.erase(iter) ;
                }
            }
        }
    }
}




/////////////////////////////////////////////////////////////////////////////
///
/// @param  position    [IN] - the position in the buffer
///
/// @return nothing
///
/// @brief Delete a paragraph from the document and merge para data
///
/// This should never be called directly, only from Delete()
/////////////////////////////////////////////////////////////////////////////
void cDocument::DeleteParagraph(POSITION_T position)
{
    MY_ASSERT(position >= 0)
    MY_ASSERT(position <= GetTextSize())

    PARAGRAPH_T currentparagraphnumber = GetParagraphFromPosition(position) ;
    auto paraiter = mParagraphData.begin() + currentparagraphnumber ;
    POSITION_T length = static_cast<POSITION_T>(paraiter->buffer.size()) ;
//    auto paraiternext = paraiter + 1 ; //  + currentparagraphnumber ;

    // if the only thing in this paragraph is the HARD_RETURN char, we'll delete the paragraph
    if(length == 1)
    {
        mParagraphData.erase(paraiter) ;

        // get the new iterator and decrement all the indexes
        paraiter = mParagraphData.begin() + currentparagraphnumber ;
        for(auto iter = paraiter; iter != mParagraphData.end(); iter++)
        {
            iter->index -= 1 ;
        }
    }
    else
    {
        auto paraiternext = paraiter + 1 ;
        if(paraiternext != mParagraphData.end())
        {
            // change the index value of any pairs in the joining paragraph
            // subtract one for the HARD_RETURN char we are deleting
            IncrementAttributes(paraiternext->index, length - 1, false) ;

            // delete the HARD_RETURN char
            paraiter->buffer.pop_back();

            // merge the two paragraphs
            paraiter->buffer.insert(paraiter->buffer.end(), paraiternext->buffer.begin(), paraiternext->buffer.end()) ;
            // offsets are not merged here -- SaveOffsets() recalculates them below
            paraiter->color.insert(paraiter->color.end(), paraiternext->color.begin(), paraiternext->color.end()) ;
            paraiter->font.insert(paraiter->font.end(), paraiternext->font.begin(), paraiternext->font.end()) ;
            paraiter->format.insert(paraiter->format.end(), paraiternext->format.begin(), paraiternext->format.end()) ;
            paraiter->tab.insert(paraiter->tab.end(), paraiternext->tab.begin(), paraiternext->tab.end()) ;
            paraiter->footnote.insert(paraiter->footnote.end(), paraiternext->footnote.begin(), paraiternext->footnote.end()) ;
            paraiter->endnote.insert(paraiter->endnote.end(), paraiternext->endnote.begin(), paraiternext->endnote.end()) ;
            paraiter->variable.insert(paraiter->variable.end(), paraiternext->variable.begin(), paraiternext->variable.end()) ;
            paraiter->pairs.insert(paraiter->pairs.end(), paraiternext->pairs.begin(), paraiternext->pairs.end()) ;

            // delete the next paragraph
            mParagraphData.erase(paraiternext) ;

            // calculate new grapheme offsets
            std::vector<POSITION_T> offsets ;
            SaveOffsets(currentparagraphnumber, offsets) ;

            // decrement the index of all following paragraphs
            for (auto iter = paraiter + 1; iter != mParagraphData.end(); ++iter)
            {
                iter->index -= 1;
            }
        }
    }

    if(mStartBlock >= position)
    {
        mStartBlock -= 1 ;
    }
    if(mEndBlock >= position)
    {
        mEndBlock -= 1 ;
    }
    if(mOldStartBlock >= position)
    {
        mOldStartBlock -= 1 ;
    }
    if(mOldEndBlock >= position)
    {
        mOldEndBlock -= 1 ;
    }
    
    for(int loop = 0; loop <  10; loop++)
    {
        if(mSavePosition[loop] >= position)
        {
            mSavePosition[loop] -= 1 ;
        }
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  POSITION_T position [in] document position for the new paragraph
/// @param  POSITION_T offset [in] byte offset within the source paragraph to split at
/// @param  PARAGRAPH_T paragraph [in] source paragraph number to split
///
/// @return nothing
///
/// @brief
/// Insert a new paragraph by splitting an existing one at the given
/// byte offset. Creates a new sParagraphData entry, copies the trailing
/// portion of the source paragraph into it, and updates paragraph indices.
///
/////////////////////////////////////////////////////////////////////////////
void cDocument::InsertParagraph(POSITION_T position, POSITION_T offset, PARAGRAPH_T paragraph)
{
    MY_ASSERT(position >= 0)
    MY_ASSERT(position <= GetTextSize())
    MY_ASSERT(paragraph >= 0)
    MY_ASSERT(paragraph < GetNumberofParagraphs())

    sParagraphData data ;
    PARAGRAPH_T para = GetParagraphFromPosition(position) ;
    PARAGRAPH_T currentparagraphnumber = para ;
    auto paraiter = mParagraphData.begin() + currentparagraphnumber ;

    data.index = position ; // + 1 ;                  // the index of this paragraph in the buffer

    POSITION_T start, end ;                             // start and end + 1 of paragraph
    start = 0;
    end = static_cast<POSITION_T>(mParagraphData[static_cast<size_t>(para)].buffer.size()) ;
    para++ ;

    // if we are in the middle of a paragraph
    POSITION_T textsize = GetTextSize() ;
    if((position != textsize) && (offset != start) && (offset != end - 1)) //  && (position != start) && (position != end - 2)) // end -2 because char has alreday been added and end is always a < check
    {
// @todo this needs to be grapheme aware
        POSITION_T length = position - paraiter->index ;

        // move format data to new paragraph and delete from old
        POSITION_T splitRelPos = position - mParagraphData[currentparagraphnumber].index ;
        TransferAttributesToNewParagraph(currentparagraphnumber, data, splitRelPos) ;

        // the glyphs
        data.buffer = paraiter->buffer ;
        data.buffer.erase(data.buffer.begin(), data.buffer.begin() + offset) ;
        paraiter->buffer.erase(paraiter->buffer.begin() + offset, paraiter->buffer.end()) ;


        auto iter = mParagraphData.begin() ;
        std::advance(iter, para) ;

        mParagraphData.insert(iter, data) ;

        // now correct any formatting we may have to the right offset
        if(position != mParagraphData[static_cast<size_t>(paragraph)].index)
        {
            std::vector<POSITION_T> offsets;
            SaveOffsets(currentparagraphnumber, offsets);
            SaveOffsets(currentparagraphnumber + 1, offsets);

            currentparagraphnumber++ ;
            DecrementAttributes((mParagraphData.begin() + currentparagraphnumber)->index, length, false) ;
            currentparagraphnumber-- ;
        }
    }

    // we are at the end of the document
    else if((position == textsize) || (para >= static_cast<PARAGRAPH_T>(mParagraphData.size())))
    {
        POSITION_T length = position - paraiter->index ;

        // transfer format tables to new paragraph
        POSITION_T splitRelPos = position - mParagraphData[currentparagraphnumber].index ;
        TransferAttributesToNewParagraph(currentparagraphnumber, data, splitRelPos) ;

        // the glyphs
        data.buffer = paraiter->buffer ;
        data.buffer.erase(data.buffer.begin(), data.buffer.begin() + offset) ;
        paraiter->buffer.erase(paraiter->buffer.begin() + offset, paraiter->buffer.end()) ;

        mParagraphData.push_back(data) ;

        // correct format positions in the new paragraph
        if(position != mParagraphData[static_cast<size_t>(paragraph)].index)
        {
            std::vector<POSITION_T> offsets;
            SaveOffsets(currentparagraphnumber, offsets);
            SaveOffsets(currentparagraphnumber + 1, offsets);

            currentparagraphnumber++ ;
            DecrementAttributes((mParagraphData.begin() + currentparagraphnumber)->index, length, false) ;
            currentparagraphnumber-- ;
        }
    }

    // we are at the start of a paragraph
    else if(position == static_cast<POSITION_T>(mParagraphData[static_cast<size_t>(para)].index))
    {
        auto iter = mParagraphData.begin() ;
        std::advance(iter, para) ;
        mParagraphData.insert(iter, data) ;

        // increment the index of all paragraphs following our insert
        iter = mParagraphData.begin() ;
        std::advance(iter, para + 2) ;
        for( ; iter != mParagraphData.end(); iter++)
        {
            iter->index++ ;
        }
    }

    // we are at the end of a line
    else
    {
        POSITION_T length = position - paraiter->index ;

        // transfer format tables to new paragraph
        POSITION_T splitRelPos = position - mParagraphData[currentparagraphnumber].index ;
        TransferAttributesToNewParagraph(currentparagraphnumber, data, splitRelPos) ;

        // the glyphs
        data.buffer = paraiter->buffer ;
        data.buffer.erase(data.buffer.begin(), data.buffer.begin() + offset) ;
        paraiter->buffer.erase(paraiter->buffer.begin() + offset, paraiter->buffer.end()) ;

        auto iter = mParagraphData.begin() ;
        std::advance(iter, para) ;
        mParagraphData.insert(iter, data) ;

        // correct format positions in the new paragraph
        if(position != mParagraphData[static_cast<size_t>(paragraph)].index)
        {
            std::vector<POSITION_T> offsets2;
            SaveOffsets(currentparagraphnumber, offsets2);
            SaveOffsets(currentparagraphnumber + 1, offsets2);

            currentparagraphnumber++ ;
            DecrementAttributes((mParagraphData.begin() + currentparagraphnumber)->index, length, false) ;
            currentparagraphnumber-- ;
        }
    }

    std::vector<POSITION_T> offsets;
    SaveOffsets(currentparagraphnumber, offsets);
    SaveOffsets(currentparagraphnumber + 1, offsets);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  position    [IN] - the position in the buffer
/// @param  length      [IN] - the length to decrement
/// @param  changeparaindex [IN] - defaults to true
///
/// @return nothing
///
/// @brief Increment the attributes (modifiers, tab, colors, etc) for the current paragraph
///
/////////////////////////////////////////////////////////////////////////////
void cDocument::IncrementAttributes(POSITION_T position, POSITION_T length, bool changeparaindex)
{
    MY_ASSERT(position >= 0)
    MY_ASSERT(position <= GetTextSize())

    POSITION_T orgpos = position ;
    const size_t glength = static_cast<size_t>(length) ;
    PARAGRAPH_T currentparagraphnumber = GetParagraphFromPosition(position) ;
    auto paraiter = mParagraphData.begin() + currentparagraphnumber ;
    position -= paraiter->index ;
    
    TabPair comp ;
    comp.first = position ;
    auto iter = lower_bound(paraiter->tab.begin(), paraiter->tab.end(), comp, TabCompare) ;
    auto iterend = paraiter->tab.end() ;
    for_each(iter, iterend, [&](TabPair &x){x.first += glength; return x;}) ;


    FormatPair comp1 ;
    comp1.first = position ;
    auto iter1 = lower_bound(paraiter->format.begin(), paraiter->format.end(), comp1, FormatCompare) ;
    auto iter1end = paraiter->format.end() ;
    for_each(iter1, iter1end, [&](FormatPair &x){x.first += glength; return x;}) ;

    FontPair comp2 ;
    comp2.first = position ;
    auto iter2 = lower_bound(paraiter->font.begin(), paraiter->font.end(), comp2, FontCompare) ;
    auto iter2end = paraiter->font.end() ;
    for_each(iter2, iter2end, [&](FontPair &x){x.first += glength; return x;}) ;


    // Wordstar style colors
    ColorPair comp3 ;
    comp3.first = position ;
    auto iter3 = lower_bound(paraiter->color.begin(), paraiter->color.end(), comp3, ColorCompare) ;
    auto iter3end = paraiter->color.end() ;
    for_each(iter3, iter3end, [&](ColorPair &x){x.first += glength; return x;}) ;

    // Footnotes
    FootnotePair comp4 ;
    comp4.first = position ;
    auto iter4 = lower_bound(paraiter->footnote.begin(), paraiter->footnote.end(), comp4, FootnoteCompare) ;
    auto iter4end = paraiter->footnote.end() ;
    for_each(iter4, iter4end, [&](FootnotePair &x){x.first += glength; return x;}) ;

    // Endnotes
    EndnotePair comp5 ;
    comp5.first = position ;
    auto iter5 = lower_bound(paraiter->endnote.begin(), paraiter->endnote.end(), comp5, EndnoteCompare) ;
    auto iter5end = paraiter->endnote.end() ;
    for_each(iter5, iter5end, [&](EndnotePair &x){x.first += glength; return x;}) ;

    // variable
    VariablePair compVar ;
    compVar.first = position ;
    auto iterVar = lower_bound(paraiter->variable.begin(), paraiter->variable.end(), compVar, VariableCompare) ;
    auto iterVarend = paraiter->variable.end() ;
    for_each(iterVar, iterVarend, [&](VariablePair &x){x.first += glength ; return x;}) ;

    PairTable comp10 ;
    comp10.first = position ;
    auto iter10 = lower_bound(paraiter->pairs.begin(), paraiter->pairs.end(), comp10, TableCompare) ;
    auto iter10end = paraiter->pairs.end() ;
    for_each(iter10, iter10end, [&](PairTable &x){x.first += glength; return x;}) ;

    // increment paragraph index value
    if(changeparaindex)
    {
        mTempParagraph.index = orgpos ;
        auto iter5 = upper_bound(mParagraphData.begin(), mParagraphData.end(), mTempParagraph, ParagraphCompare) ;
        for(;iter5 != mParagraphData.end(); iter5++)
        {
            if(iter5->index != 0)                   // never increment the first index position
            {
                iter5->index += length ;
            }
        }
    
        if(orgpos <=mStartBlock)
        {
            mStartBlock++ ;
            if(mStartBlock > GetTextSize())
            {
                mStartBlock = GetTextSize() ;
            }
        }
        if(orgpos < mEndBlock)
        {
            mEndBlock++ ;
            if(mEndBlock > GetTextSize())
            {
                mEndBlock = GetTextSize() ;
            }
        }

        if(mStartBlock == mEndBlock)
        {
            mStartBlock = NOT_SET ;
            mEndBlock = NOT_SET ;
            mBlockSet = false ;
        }

        if(orgpos <= mOldStartBlock)
        {
            mOldStartBlock++ ;
        }
        if(orgpos < mOldEndBlock)
        {
            mOldEndBlock++ ;
        }

        // and finally, any saved positions
        for(int loop = 0; loop < 10; loop++)
        {
            if(mSavePosition[loop] != NOT_SET)
            {
                if(mSavePosition[loop] > orgpos)
                    mSavePosition[loop]++ ;
                {
                }
            }
        }
    }
}




/////////////////////////////////////////////////////////////////////////////
///
/// @param  position    [IN] - the position in the buffer
/// @param  length      [IN] - the length to decrement
/// @param  changeparaindex [IN] - defaults to true
///
/// @return nothing
///
/// @brief Decrement the attributes (modifiers, tab, colors, etc) for the document
///        if changeparaindex is false, we are calling this function because of a
///        paragraph being split into two, and we don't change any of the lower
///        paragraphs index values.
///
/////////////////////////////////////////////////////////////////////////////
void cDocument::DecrementAttributes(POSITION_T position, POSITION_T length, bool changeparaindex)
{
    MY_ASSERT(position >= 0)
    MY_ASSERT(position <= GetTextSize())

    size_t glength = static_cast<size_t>(length) ;
    TabPair comp ;
    POSITION_T orgpos = position ;
    PARAGRAPH_T currentparagraphnumber = GetParagraphFromPosition(position) ;
    auto paraiter = mParagraphData.begin() + currentparagraphnumber ;
    position -= paraiter->index ;
        
    comp.first = position ;
    auto iter = lower_bound(paraiter->tab.begin(), paraiter->tab.end(), comp, TabCompare) ;
    auto iterend = paraiter->tab.end() ;
    for_each(iter, iterend, [&](TabPair &x){x.first -= glength; return x;}) ;

    FormatPair comp1 ;
    comp1.first = position ;
    auto iter1 = lower_bound(paraiter->format.begin(), paraiter->format.end(), comp1, FormatCompare) ;
    auto iter1end = paraiter->format.end() ;
    for_each(iter1, iter1end, [&](FormatPair &x){x.first -= glength; return x;}) ;

    FontPair comp2 ;
    comp2.first = position ;
    auto iter2 = lower_bound(paraiter->font.begin(), paraiter->font.end(), comp2, FontCompare) ;
    auto iter2end = paraiter->font.end() ;
    for_each(iter2, iter2end, [&](FontPair &x){x.first -= glength; return x;}) ;

    // Wordstar style colors
    ColorPair comp3 ;
    comp3.first = position ;
    auto iter3 = lower_bound(paraiter->color.begin(), paraiter->color.end(), comp3, ColorCompare) ;
    auto iter3end = paraiter->color.end() ;
    for_each(iter3, iter3end, [&](ColorPair &x){x.first -= glength; return x;}) ;

    // Footnotes
    FootnotePair comp4 ;
    comp4.first = position ;
    auto iter4 = lower_bound(paraiter->footnote.begin(), paraiter->footnote.end(), comp4, FootnoteCompare) ;
    auto iter4end = paraiter->footnote.end() ;
    for_each(iter4, iter4end, [&](FootnotePair &x){x.first -= glength; return x;}) ;

    // Endnotes
    EndnotePair comp5 ;
    comp5.first = position ;
    auto iter5 = lower_bound(paraiter->endnote.begin(), paraiter->endnote.end(), comp5, EndnoteCompare) ;
    auto iter5end = paraiter->endnote.end() ;
    for_each(iter5, iter5end, [&](EndnotePair &x){x.first -= glength; return x;}) ;

    // variable
    VariablePair compVar ;
    compVar.first = position ;
    auto iterVar = lower_bound(paraiter->variable.begin(), paraiter->variable.end(), compVar, VariableCompare) ;
    auto iterVarend = paraiter->variable.end() ;
    for_each(iterVar, iterVarend, [&](VariablePair &x){x.first -= glength ; return x;}) ;

    PairTable comp10 ;
    comp10.first = position ;
    auto iter10 = lower_bound(paraiter->pairs.begin(), paraiter->pairs.end(), comp10, TableCompare) ;
    auto iter10end = paraiter->pairs.end() ;
    for_each(iter10, iter10end, [&](PairTable &x){x.first -= glength; return x;}) ;


    // decrement paragraph index value
    if(changeparaindex)
    {
        sParagraphData para ;
        para.index = orgpos ;
        auto iter5 = upper_bound(mParagraphData.begin(), mParagraphData.end(), para, ParagraphCompare) ;

        for(;iter5 != mParagraphData.end(); iter5++)
        {
            if(iter5->index != 0)                   // never decrement the first position
            {
                iter5->index -= length ;
            }
        }

        if(orgpos < mStartBlock)
        {
            mStartBlock-- ;
            if(mStartBlock < 0)
            {
                mStartBlock = 0 ;
            }
        }
        if(orgpos < mEndBlock)
        {
            mEndBlock-- ;
            if(mEndBlock < 0)
            {
                mEndBlock = 0 ;
            }
        }

        if(mStartBlock == mEndBlock)
        {
            mStartBlock = NOT_SET ;
            mEndBlock = NOT_SET ;
            mBlockSet = false ;
        }

        if(orgpos <= mOldStartBlock)
        {
            mOldStartBlock-- ;
        }
        if(orgpos < mOldEndBlock)
        {
            mOldEndBlock-- ;
        }

        // and finally, any saved positions
        for(int loop = 0; loop < 10; ++loop)
        {
            if(mSavePosition[loop] != NOT_SET)
            {
                if(mSavePosition[loop] > orgpos)
                {
                    mSavePosition[loop]-- ;
                }
            }
        }
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  fromPara    [IN] - paragraph number to transfer FROM
/// @param  newData     [IN/OUT] - new paragraph data to transfer INTO
/// @param  splitRelPos [IN] - relative position in fromPara at the split point
///
/// @return nothing
///
/// @brief
/// Transfer all format/metadata table entries at positions >= splitRelPos
/// from the source paragraph to the new paragraph data. Entries are erased
/// from the source paragraph after copying. Tables transferred: color,
/// font, format, tab, footnote, endnote, variable, pairs.
///
/////////////////////////////////////////////////////////////////////////////
void cDocument::TransferAttributesToNewParagraph(PARAGRAPH_T fromPara, sParagraphData &newData, POSITION_T splitRelPos)
{
    // color
    ColorPair c1 ;
    c1.first = splitRelPos ;
    auto citer = lower_bound(mParagraphData[fromPara].color.begin(), mParagraphData[fromPara].color.end(), c1, ColorCompare) ;
    newData.color.insert(newData.color.begin(), citer, mParagraphData[fromPara].color.end()) ;
    mParagraphData[fromPara].color.erase(citer, mParagraphData[fromPara].color.end()) ;

    // font
    FontPair f1 ;
    f1.first = splitRelPos ;
    auto fiter = lower_bound(mParagraphData[fromPara].font.begin(), mParagraphData[fromPara].font.end(), f1, FontCompare) ;
    newData.font.insert(newData.font.begin(), fiter, mParagraphData[fromPara].font.end()) ;
    mParagraphData[fromPara].font.erase(fiter, mParagraphData[fromPara].font.end()) ;

    // format (bold, italic, underline, etc.)
    FormatPair f2 ;
    f2.first = splitRelPos ;
    auto fiter2 = lower_bound(mParagraphData[fromPara].format.begin(), mParagraphData[fromPara].format.end(), f2, FormatCompare) ;
    newData.format.insert(newData.format.begin(), fiter2, mParagraphData[fromPara].format.end()) ;
    mParagraphData[fromPara].format.erase(fiter2, mParagraphData[fromPara].format.end()) ;

    // tab
    TabPair t1 ;
    t1.first = splitRelPos ;
    auto titer = lower_bound(mParagraphData[fromPara].tab.begin(), mParagraphData[fromPara].tab.end(), t1, TabCompare) ;
    newData.tab.insert(newData.tab.begin(), titer, mParagraphData[fromPara].tab.end()) ;
    mParagraphData[fromPara].tab.erase(titer, mParagraphData[fromPara].tab.end()) ;

    // footnote
    FootnotePair fn1 ;
    fn1.first = splitRelPos ;
    auto fniter = lower_bound(mParagraphData[fromPara].footnote.begin(), mParagraphData[fromPara].footnote.end(), fn1, FootnoteCompare) ;
    newData.footnote.insert(newData.footnote.begin(), fniter, mParagraphData[fromPara].footnote.end()) ;
    mParagraphData[fromPara].footnote.erase(fniter, mParagraphData[fromPara].footnote.end()) ;

    // endnote
    EndnotePair en1 ;
    en1.first = splitRelPos ;
    auto eniter = lower_bound(mParagraphData[fromPara].endnote.begin(), mParagraphData[fromPara].endnote.end(), en1, EndnoteCompare) ;
    newData.endnote.insert(newData.endnote.begin(), eniter, mParagraphData[fromPara].endnote.end()) ;
    mParagraphData[fromPara].endnote.erase(eniter, mParagraphData[fromPara].endnote.end()) ;

    // variable
    VariablePair var1 ;
    var1.first = splitRelPos ;
    auto variter = lower_bound(mParagraphData[fromPara].variable.begin(), mParagraphData[fromPara].variable.end(), var1, VariableCompare) ;
    newData.variable.insert(newData.variable.begin(), variter, mParagraphData[fromPara].variable.end()) ;
    mParagraphData[fromPara].variable.erase(variter, mParagraphData[fromPara].variable.end()) ;

    // pairs (main lookup table)
    PairTable p1 ;
    p1.first = splitRelPos ;
    auto piter = lower_bound(mParagraphData[fromPara].pairs.begin(), mParagraphData[fromPara].pairs.end(), p1, TableCompare) ;
    newData.pairs.insert(newData.pairs.begin(), piter, mParagraphData[fromPara].pairs.end()) ;
    mParagraphData[fromPara].pairs.erase(piter, mParagraphData[fromPara].pairs.end()) ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  paragraph   [IN] - the paragraph to work on
/// @param  offset      [OUT] - the grapheme offsets of the paragraph
///
/// @return nothing
/// @brief get the grapheme offsets of the paragraph and save them for later
///
/////////////////////////////////////////////////////////////////////////////
void cDocument::SaveOffsets(PARAGRAPH_T paragraph, std::vector<POSITION_T>& offsets)
{
    /// @todo we actually do the entire paragraph. No optimizations yet.
    GraphemeCount(mParagraphData[paragraph].buffer, offsets);

    mParagraphData[paragraph].offsets.Store(mParagraphData[paragraph].buffer, offsets) ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  text [in] UTF-8 text to split
///
/// @return the text split into grapheme-cluster substrings
///
/// @brief
/// Split a UTF-8 string into its grapheme clusters, using the same
/// segmentation the document uses internally.
///
/////////////////////////////////////////////////////////////////////////////
std::vector<std::string> cDocument::splitGraphemes(const std::string &text)
{
    std::vector<std::string> graphemes;
    if (text.empty())
    {
        return graphemes;
    }

    std::vector<POSITION_T> offsets;
    GraphemeCount(text, offsets);   // fills offsets with the byte offset of each grapheme
    graphemes.reserve(offsets.size());

    for (size_t i = 0; i < offsets.size(); ++i)
    {
        size_t byteStart = static_cast<size_t>(offsets[i]);
        if (byteStart >= text.length())
        {
            break;
        }
        size_t byteEnd = (i + 1 < offsets.size()) ? static_cast<size_t>(offsets[i + 1]) : text.length();
        if (byteEnd > text.length())
        {
            byteEnd = text.length();
        }
        graphemes.push_back(text.substr(byteStart, byteEnd - byteStart));
    }
    return graphemes;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  grapheme [in] a single UTF-8 grapheme cluster
///
/// @return the grapheme case-folded for case-insensitive comparison
///
/// @brief
/// Case-fold a grapheme by applying simple (1:1, length-preserving) case
/// folding to each codepoint. Used only for comparison; the original
/// graphemes keep their positions.
///
/////////////////////////////////////////////////////////////////////////////
std::string cDocument::foldGrapheme(const std::string &grapheme)
{
    std::u32string codepoints = unicode::utf8::decode(grapheme);
    for (char32_t &codepoint : codepoints)
    {
        codepoint = unicode::simple_case_folding(codepoint);
    }
    return unicode::utf8::encode(codepoints);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  haystackGraphemes [in] haystack graphemes
/// @param  haystackFolded [in] case-folded haystack graphemes (used when casecmp)
/// @param  needleGraphemes [in] needle graphemes
/// @param  needleFolded [in] case-folded needle graphemes (used when casecmp)
/// @param  wildcard [in] if true, a needle grapheme of "?" matches any one grapheme
/// @param  casecmp [in] if true, compare folded forms (case-insensitive)
/// @param  reverse [in] if true, search backwards
/// @param  startGrapheme [in] grapheme index to start from (inclusive)
///
/// @return the grapheme index of the match, or std::string::npos if none
///
/// @brief
/// Match the needle against the haystack one grapheme at a time. Operating in
/// grapheme units makes '?' match a whole grapheme, keeps positions as
/// grapheme indices, and never splits a grapheme cluster.
///
/////////////////////////////////////////////////////////////////////////////
POSITION_T cDocument::graphemeSearch(const std::vector<std::string> &haystackGraphemes, const std::vector<std::string> &haystackFolded, const std::vector<std::string> &needleGraphemes, const std::vector<std::string> &needleFolded, bool wildcard, bool casecmp, bool reverse, size_t startGrapheme)
{
    size_t haystackCount = haystackGraphemes.size();
    size_t needleCount = needleGraphemes.size();
    if (needleCount == 0 || needleCount > haystackCount)
    {
        return std::string::npos;
    }

    size_t lastStartIndex = haystackCount - needleCount;

    auto matchesAt = [&](size_t startIndex) -> bool
    {
        for (size_t needleIndex = 0; needleIndex < needleCount; needleIndex++)
        {
            if (wildcard && needleGraphemes[needleIndex] == "?")
            {
                continue;
            }
            const std::string &haystackGrapheme = casecmp ? haystackFolded[startIndex + needleIndex] : haystackGraphemes[startIndex + needleIndex];
            const std::string &needleGrapheme = casecmp ? needleFolded[needleIndex] : needleGraphemes[needleIndex];
            if (haystackGrapheme != needleGrapheme)
            {
                return false;
            }
        }
        return true;
    };

    if (!reverse)
    {
        for (size_t candidateIndex = startGrapheme; candidateIndex <= lastStartIndex; candidateIndex++)
        {
            if (matchesAt(candidateIndex))
            {
                return static_cast<POSITION_T>(candidateIndex);
            }
        }
    }
    else
    {
        size_t candidateIndex = (startGrapheme < lastStartIndex) ? startGrapheme : lastStartIndex;
        while (true)
        {
            if (matchesAt(candidateIndex))
            {
                return static_cast<POSITION_T>(candidateIndex);
            }
            if (candidateIndex == 0)
            {
                break;
            }
            candidateIndex--;
        }
    }
    return std::string::npos;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  const char32_t *s32 [in] UTF-32 string to check
/// @param  size_t len [in] length of the string in code points
/// @param  size_t pos [in] position to check for word boundary
///
/// @return true if the position is a word boundary
///
/// @brief
/// Check if a position is a word boundary. Extends the standard Unicode
/// word boundary check to also treat MARKER_CHAR as a boundary.
///
/////////////////////////////////////////////////////////////////////////////
bool cDocument::isExtendedWordBoundary(const char32_t *s32, size_t len, size_t pos)
{
    return unicode::is_word_boundary(s32, len, pos) || s32[pos] == MARKER_CHAR;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return sDocumentMemoryUsage with byte counts for each subsystem
///
/// @brief
/// Calculate approximate memory usage of the document's data structures.
/// Walks paragraph storage, attribute vectors, and undo/redo stacks.
///
/////////////////////////////////////////////////////////////////////////////
sDocumentMemoryUsage cDocument::GetMemoryUsage(void) const
{
    sDocumentMemoryUsage usage = {} ;

    usage.paragraphCount = mParagraphData.size() ;
    usage.undoGroupCount = mUndoStack.size() ;
    usage.redoGroupCount = mRedoStack.size() ;

    // Paragraph text and attribute storage
    for (const auto& para : mParagraphData)
    {
        // text: capacity vs size
        usage.textBytes += para.buffer.capacity() ;
        usage.textUsedBytes += para.buffer.size() ;

        // attributes: per-type breakdown (capacity = allocated)
        size_t pairs = para.pairs.capacity() * sizeof(PairTable) ;
        size_t format = para.format.capacity() * sizeof(FormatPair) ;
        size_t font = para.font.capacity() * sizeof(FontPair) ;
        size_t tab = para.tab.capacity() * sizeof(TabPair) ;
        size_t color = para.color.capacity() * sizeof(ColorPair) ;
        size_t footnote = para.footnote.capacity() * sizeof(FootnotePair) ;
        size_t endnote = para.endnote.capacity() * sizeof(EndnotePair) ;
        size_t variable = para.variable.capacity() * sizeof(VariablePair) ;
        size_t offsets = para.offsets.memoryAllocated() ;

        usage.attrPairsBytes += pairs ;
        usage.attrFormatBytes += format ;
        usage.attrFontBytes += font ;
        usage.attrTabBytes += tab ;
        usage.attrColorBytes += color ;
        usage.attrFootnoteBytes += footnote ;
        usage.attrEndnoteBytes += endnote ;
        usage.attrVariableBytes += variable ;
        usage.attrOffsetsBytes += offsets ;

        usage.attributeBytes += pairs + format + font + tab + color + footnote + endnote + variable + offsets ;

        // attributes: size (in use)
        usage.attributeUsedBytes += para.pairs.size() * sizeof(PairTable) ;
        usage.attributeUsedBytes += para.format.size() * sizeof(FormatPair) ;
        usage.attributeUsedBytes += para.font.size() * sizeof(FontPair) ;
        usage.attributeUsedBytes += para.tab.size() * sizeof(TabPair) ;
        usage.attributeUsedBytes += para.color.size() * sizeof(ColorPair) ;
        usage.attributeUsedBytes += para.footnote.size() * sizeof(FootnotePair) ;
        usage.attributeUsedBytes += para.endnote.size() * sizeof(EndnotePair) ;
        usage.attributeUsedBytes += para.variable.size() * sizeof(VariablePair) ;
        usage.attributeUsedBytes += para.offsets.memoryUsed() ;
    }

    // Undo stack
    for (const auto& group : mUndoStack)
    {
        usage.undoBytes += sizeof(sUndoGroup) ;
        for (const auto& action : group.actions)
        {
            usage.undoBytes += sizeof(sUndoAction) ;
            usage.undoBytes += action.chars.capacity() * sizeof(sUndoCharInfo) ;
        }
    }

    // Redo stack
    for (const auto& group : mRedoStack)
    {
        usage.redoBytes += sizeof(sUndoGroup) ;
        for (const auto& action : group.actions)
        {
            usage.redoBytes += sizeof(sUndoAction) ;
            usage.redoBytes += action.chars.capacity() * sizeof(sUndoCharInfo) ;
        }
    }

    // Copy buffer
    usage.copyBufferBytes = mCopyBuffer.capacity() * sizeof(sUndoCharInfo) ;

    return usage ;
}


