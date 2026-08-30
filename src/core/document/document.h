#ifndef DOCUMENT_H
#define DOCUMENT_H

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


#include <string>
#include <vector>
#include <deque>
#include <algorithm>
#include <optional>


#include "src/core/include/config.h"
#include "src/core/document/documentlistener.h"
//#include "../include/enums.h"
#include "doctstructs.h"
#include "graphemeoffsets.h"
#include "math.h"
//#include "../files/rtf/read/rtfparser.h"


using PairTable = std::pair<POSITION_T, eType> ;
bool TableCompare(const PairTable &first, const PairTable &second) ;

using TabPair = std::pair<POSITION_T, sWSTab > ;
bool TabCompare(const TabPair &firstElem, const TabPair &secondElem) ;

using FormatPair = std::pair<POSITION_T, eModifiers > ;
bool FormatCompare(const FormatPair &first, const FormatPair &second) ;

using ColorPair = std::pair<POSITION_T, sSeqRGBColor> ;
bool ColorCompare(const ColorPair &first, const ColorPair &second) ;

using FontPair = std::pair<POSITION_T, sInternalFonts> ;
bool FontCompare(const FontPair &first, const FontPair &second) ;

using IndexPair = std::pair<POSITION_T, std::string> ;

using FootnotePair = std::pair<POSITION_T, sNote> ;
bool FootnoteCompare(const FootnotePair &first, const FootnotePair &second) ;

using EndnotePair = std::pair<POSITION_T, sNote> ;
bool EndnoteCompare(const EndnotePair &first, const EndnotePair &second) ;

using SavedPositionPair = std::pair<POSITION_T, int> ;
bool SavedPositionCompare(const SavedPositionPair &first, const SavedPositionPair &second) ;

using VariablePair = std::pair<POSITION_T, eVariableType> ;
bool VariableCompare(const VariablePair &first, const VariablePair &second) ;

struct sParagraphData ;
bool ParagraphCompare(const sParagraphData &first, const sParagraphData &second) ;

/////////////////////////////////////////////////////////////////////////////
///
/// @struct sDocumentMemoryUsage
///
/// @brief
/// Memory usage breakdown for the document subsystem.
/// Tracks allocated vs in-use bytes for text, attributes, undo/redo,
/// and per-attribute-type breakdowns. Used by ^O? memory dialog.
///
/////////////////////////////////////////////////////////////////////////////
struct sDocumentMemoryUsage
{
    size_t textBytes ;              ///< allocated (capacity)
    size_t textUsedBytes ;          ///< in use (size)
    size_t attributeBytes ;         ///< allocated (capacity)
    size_t attributeUsedBytes ;     ///< in use (size)
    size_t attrPairsBytes ;         ///< pairs table
    size_t attrFormatBytes ;        ///< format modifiers
    size_t attrFontBytes ;          ///< font modifiers
    size_t attrTabBytes ;           ///< tab positions
    size_t attrColorBytes ;         ///< color modifiers
    size_t attrFootnoteBytes ;      ///< footnotes
    size_t attrEndnoteBytes ;       ///< endnotes
    size_t attrVariableBytes ;      ///< variables
    size_t attrOffsetsBytes ;       ///< grapheme offset cache
    size_t undoBytes ;
    size_t redoBytes ;
    size_t copyBufferBytes ;
    size_t paragraphCount ;
    size_t undoGroupCount ;
    size_t redoGroupCount ;
} ;



// declare a memory policy for using a tracing garbage collector


/////////////////////////////////////////////////////////////////////////////
///
/// @struct sParagraphData
///
/// @brief
/// Complete paragraph storage in the document model.
/// Contains the UTF-8 text buffer, attribute tables (format, font, tab,
/// color, footnotes, endnotes, variables), and grapheme boundary cache.
///
/////////////////////////////////////////////////////////////////////////////
struct sParagraphData
{
    POSITION_T index ;                                   ///< where paragraph starts in the buffer
    std::string buffer ;                                 ///< should I really be using a string to store a paragraph?
//    sModifierParagraph paragraph ;                     ///< end of paragraph attributes
    std::vector<PairTable> pairs ;                       ///< the pairings table
    std::vector<FormatPair> format ;                     ///< format modifiers
    std::vector<FontPair> font ;                         ///< font modifiers
    std::vector<TabPair> tab ;                           ///< tab positions and type
    std::vector<ColorPair> color ;                       ///< color modifiers
    std::vector<FootnotePair> footnote ;                 ///< footnotes
    std::vector<EndnotePair> endnote ;                   ///< endnotes
    std::vector<VariablePair> variable ;                 ///< variable expansions (&@&, &#&, etc.)

    cGraphemeOffsets offsets;                              ///< compact grapheme boundary cache
};


// sColorTable removed -- colors now stored as sSeqRGBColor throughout

/////////////////////////////////////////////////////////////////////////////
///
/// @enum eUndoActionType
///
/// @brief
/// Types of atomic undo/redo actions.
/// Determines whether an undo reverses an insert (by deleting)
/// or a delete (by re-inserting).
///
/////////////////////////////////////////////////////////////////////////////
enum eUndoActionType
{
    UNDO_ACTION_INSERT,     // characters were inserted (undo = delete them)
    UNDO_ACTION_DELETE,     // characters were deleted (undo = re-insert them)
};

/////////////////////////////////////////////////////////////////////////////
///
/// @struct sUndoCharInfo
///
/// @brief
/// Information about a single character for undo/redo operations.
/// Stores the codepoint and optional metadata for special elements
/// (tabs, fonts, colors, footnotes, endnotes, variables) that require
/// more than a simple codepoint to restore.
///
/////////////////////////////////////////////////////////////////////////////
struct sUndoCharInfo
{
    CHAR_T codepoint ;                                  ///< the original codepoint (HARD_RETURN, STYLE_BOLD, regular char, etc.)

    // metadata for special elements (only one populated at a time, empty for regular chars)
    // these are needed because InsertTab/Font/Color require metadata that
    // simple Insert(CHAR_T) cannot restore
    std::optional<sWSTab> tabData ;
    std::optional<sInternalFonts> fontData ;
    std::optional<sSeqRGBColor> colorData ;
    std::optional<sNote> footnoteData ;
    std::optional<sNote> endnoteData ;
    std::optional<eVariableType> variableData ;
};

/////////////////////////////////////////////////////////////////////////////
///
/// @struct sUndoAction
///
/// @brief
/// Single atomic undo/redo action.
/// Records the action type (insert/delete), document position, length,
/// character data, and cursor positions before and after the action.
///
/////////////////////////////////////////////////////////////////////////////
struct sUndoAction
{
    eUndoActionType type ;

    POSITION_T position ;                               ///< document position where action occurred
    POSITION_T length ;                                 ///< number of graphemes involved

    // character data for the operation
    // for INSERT: the characters that were inserted (undo = delete, redo = re-insert)
    // for DELETE: the characters that were deleted (undo = re-insert, redo = delete)
    std::vector<sUndoCharInfo> chars ;

    POSITION_T cursorBefore ;                           ///< cursor position before the action
    POSITION_T cursorAfter ;                            ///< cursor position after the action
};

/////////////////////////////////////////////////////////////////////////////
///
/// @struct sUndoGroup
///
/// @brief
/// Group of undo/redo actions that execute as a single step.
/// Combines multiple atomic actions (e.g., multi-character delete)
/// into one user-visible undo/redo operation.
///
/////////////////////////////////////////////////////////////////////////////
struct sUndoGroup
{
    std::vector<sUndoAction> actions ;                  ///< actions in this group (in order)
    POSITION_T cursorBefore ;                           ///< cursor before first action in group
    POSITION_T cursorAfter ;                            ///< cursor after last action in group
};


class cDocument
{
    // =================================================================
    // METHODS
    // =================================================================

public:
    // --- Construction / Destruction ---
    cDocument(void);
    virtual ~cDocument(void);

    // --- Document Operations ---
    void Clear(void) ;
    void ShrinkToFit(void) ;

    // --- Text Operations ---
    void Insert(CHAR_T ch) ;
    void Insert(const std::string &text) ;
    bool Delete(POSITION_T position, POSITION_T length) ;
    std::string GetChar(POSITION_T &position) ;
    std::string GetCharNoAdvance(POSITION_T position) ;

    // --- Formatting ---
    void BeginBold(void) ;
    void EndBold(void) ;
    void BeginItalics(void) ;
    void EndItalics(void) ;
    void BeginUnderline(void) ;
    void EndUnderline(void) ;
    void BeginStrikeThrough(void) ;
    void EndStrikeThrough(void) ;
    void BeginSuperscript(void) ;
    void EndSuperscript(void) ;
    void BeginSubscript(void) ;
    void EndSubscript(void) ;
    void BeginIndex(void) ;
    void EndIndex(void) ;

    // --- Alignment ---
    void BeginCenter(void) ;
    void BeginLeft(void) ;
    void BeginRight(void) ;
    void BeginJustify(void) ;

    // --- Special Inserts ---
    void MaybeInsertHardReturn(void) ;
    void InsertTab(sWSTab &tab) ;
    void InsertColor(sSeqRGBColor &color) ;                  // inserts a color (full RGB, or {-1,-1,-1,-1} for default)
    void InsertColorFromWSPalette(sWSColor &color) ;        // converts WordStar palette index to RGB and inserts
    void InsertFont(sInternalFonts &font) ;                 // used by Wordstar file loader
    void InsertFootnote(sNote &note) ;                      // insert a footnote
    void InsertEndnote(sNote &note) ;                       // insert an endnote
    void InsertVariable(eVariableType type) ;               // insert a variable (&@&, &#&, etc.)

    // --- Loading State ---
    void SetLoading(bool loading) ;
    bool GetLoading(void) ;
    void SetSuppressNotify(bool suppress) ;
    bool GetSuppressNotify(void) ;

    // --- Position ---
    POSITION_T GetTextSize(void) ;
    std::string GetText(void) ;
    POSITION_T GetPosition(void) ;
    void SetPosition(POSITION_T pos) ;
    void GotoPreviousPosition(void) ;

    // --- Paragraph Operations ---
    PARAGRAPH_T GetNumberofParagraphs(void) ;
    void GetParagraphStartandEnd(const PARAGRAPH_T para, POSITION_T &start, POSITION_T &end) ;
    PARAGRAPH_T GetParagraphFromPosition(POSITION_T position) ;

    // --- Attribute Queries ---
    sWSTab GetTab(POSITION_T position) ;
    bool GetColor(POSITION_T position, sSeqRGBColor &color) ;
    bool GetFont(POSITION_T position, sInternalFonts &intfont) ;
    eVariableType GetVariable(POSITION_T position) ;
    eModifiers GetControlChar(POSITION_T) ;

    // --- Text Retrieval ---
    std::string GetParagraphText(PARAGRAPH_T para) ;
    size_t GetParagraphGraphemeOffsets(const PARAGRAPH_T &para, std::vector<PARAGRAPH_T> &offsets);
    void GetParagraphGraphemes(PARAGRAPH_T para, std::vector<std::string>& graphemes, std::vector<POSITION_T>& offsets);
    std::string GetBlockText(POSITION_T start, POSITION_T end) ;

    // --- Block Operations ---
    bool GetBlock(POSITION_T &start, POSITION_T &end) ;
    bool GetPreviousBlock(POSITION_T &start, POSITION_T &end) ;
    void SaveBlocks(void) ;;
    void RestoreBlocks(void) ;
    void SetBeginBlock(void) ;
    void SetEndBlock(void) ;
    void SetPreviousBlock(void) ;
    void CopyBlock(void) ;
    void MoveBlock(void) ;
    void DeleteBlock(void) ;
    void UnsetBlock(void) ;

    // --- Find ---
    POSITION_T FindNext(const std::string &needle, const POSITION_T &start, bool wildcard = false, bool casecmp = false, bool wholeword = false) ;
    POSITION_T FindPrev(const std::string &needle, const POSITION_T &start, bool wildcard = false, bool casecmp = false, bool wholeword = false) ;

    // --- Font List ---
    void GetFontList(std::vector<sInternalFonts> &fontlist) ;

    // --- Conversion Utilities ---
    COORD_T ConvertToTwips(double value, char type, double cwidth = 240) ;
    double GetValue(std::string txt, bool &incdec) ;
    char GetType(std::string text) ;
    double EvaluateExpression(const std::string &text, bool &incdec, bool &hasUnits) ;

    // --- Display Control ---
    void SetShowControl(eShowControl show) ;
    eShowControl GetShowControl(void) ;

    // --- Word Count ---
    void SetWordCount(long count) ;
    long GetWordCount(void) const ;

    // --- Memory ---
    sDocumentMemoryUsage GetMemoryUsage(void) const ;

    // --- Clipboard ---
    void Copy(void) ;
    void Paste(void) ;
    void Cut(void) ;

    // --- Saved Positions ---
    void SetSavePosition(int index) ;

    // --- Undo / Redo ---
    bool Undo(void) ;
    bool Redo(void) ;
    bool CanUndo(void) const ;
    bool CanRedo(void) const ;
    void BeginUndoGroup(void) ;
    void EndUndoGroup(void) ;
    void ClearUndoHistory(void) ;

    // --- Listener Management ---
    void AddListener(cDocumentListener* listener) ;
    void RemoveListener(cDocumentListener* listener) ;

    // --- Unicode / Grapheme ---
    size_t GraphemeCount(const std::string& text, std::vector<POSITION_T>& offsets);
    size_t GetCodePoints(const std::string &text, std::u32string &codepoints);

    // --- Word Navigation ---
    size_t GetWordPositions(PARAGRAPH_T para, std::vector<POSITION_T> &wordstarts) ;
    size_t GetWordPositions(std::string text, std::vector<POSITION_T> &wordstarts) ;
    POSITION_T GetNextWordPosition(POSITION_T pos) ;
    POSITION_T GetPrevWordPosition(POSITION_T pos) ;
    POSITION_T GetWordEndPosition(POSITION_T pos) ;

    // --- Font Navigation ---
    POSITION_T GetNextFontTagPosition(void) ;

    // --- String Utilities ---
    std::string Normalize(const std::string &str) ;
    std::u32string NormalizeToUTF32(const std::string &str) ;
    std::string NormalizeToUTF8(const std::u32string &str32) ;
    std::string LowerCase(const std::string &str) ;
    std::string UpperCase(const std::string &str) ;
    std::string TitleCase(const std::string &str) ;

private:
    // --- Attribute Modification ---
    void SetControlChar(CHAR_T ch) ;
    void DeleteTab(POSITION_T position) ;
    void DeleteColor(POSITION_T position) ;
    void DeleteFont(POSITION_T position) ;
    void DeleteVariable(POSITION_T position) ;
    void DeleteControlChar(POSITION_T position) ;

    // --- Paragraph Management ---
    void InsertParagraph(POSITION_T position, POSITION_T offset, PARAGRAPH_T paragraph) ;
    void DeleteParagraph(POSITION_T position) ;

    // --- Attribute Accounting ---
    void IncrementAttributes(POSITION_T position, POSITION_T length = 1, bool changeparaindex = true) ;
    void DecrementAttributes(POSITION_T, POSITION_T length = 1, bool changeparaindex = true) ;
    void TransferAttributesToNewParagraph(PARAGRAPH_T fromPara, sParagraphData &newData, POSITION_T splitRelPos) ;

    // --- Offset Management ---
    void IncrementOffsets(POSITION_T position, CHAR_T ch) ;
    void SaveOffsets(PARAGRAPH_T paragraph, std::vector<POSITION_T>& offsets);

    // --- Search Helpers ---
    std::vector<std::string> splitGraphemes(const std::string &text) ;
    std::string foldGrapheme(const std::string &grapheme) ;
    POSITION_T graphemeSearch(const std::vector<std::string> &haystackGraphemes, const std::vector<std::string> &haystackFolded, const std::vector<std::string> &needleGraphemes, const std::vector<std::string> &needleFolded, bool wildcard, bool casecmp, bool reverse, size_t startGrapheme) ;
    bool isExtendedWordBoundary(const char32_t *s32, size_t len, size_t pos) ;

    // --- Listener Notification (suppressed during loading and undo/redo) ---
    void NotifyChanged(PARAGRAPH_T fromParagraph) ;
    void NotifyCleared(void) ;

    // --- Undo Helpers ---
    void RecordAction(eUndoActionType type, POSITION_T position, POSITION_T length,
                      const std::vector<sUndoCharInfo> &chars, POSITION_T cursorBefore) ;
    std::vector<sUndoCharInfo> CaptureCharacters(POSITION_T position, POSITION_T length) ;
    void ReinsertCharacters(POSITION_T position, const std::vector<sUndoCharInfo> &chars) ;

    // =================================================================
    // MEMBER VARIABLES
    // =================================================================

public:
    // --- Document State ---
    bool mChanged ;                                     ///< flag for 'has the file changed'

    // --- Block Selection ---
    POSITION_T mStartBlock ;                            ///< position where a selected block starts
    POSITION_T mEndBlock ;                              ///< position where a selected block ends
    POSITION_T mOldStartBlock ;                         ///< old position where a selected block starts
    POSITION_T mOldEndBlock ;                           ///< old position where a selected block ends
    bool mBlockSet ;                                    ///< true if a block is marked, else false
    bool mOldBlockSet ;                                 ///< true if a previous block is set, else false
    POSITION_T mSavedBlocks[4] ;                        ///< saved blocks
    bool mSavedBlockSet[2] ;                            ///< saved block flags

    // --- Saved Positions ---
    POSITION_T mSavePosition[10] ;                      ///< saved positions

private:
    // --- Paragraph Storage ---
    std::vector<sParagraphData> mParagraphData ;             ///< the documents paragraphs

    // --- Caret Position ---
    POSITION_T mCurrentPosition ;                       ///< the current caret postion
    POSITION_T mPreviousPosition ;                           ///< the previous position of the caret

    // --- Temporary / Cache ---
//    std::deque<std::vector<sParagraphData>> mUndo ;               ///< undo stack
//    std::deque<POSITION_T> mUndoCaret ;                      ///< the caret position for undo stack
//    std::deque<std::vector<sParagraphData>> mRedo ;               ///< redo stack
//    std::deque<POSITION_T> mRedoCaret ;                      ///< the caret position for redo stack
    sParagraphData mTempParagraph ;                     ///< speed up MULTI_BUFFER since I don't have to create a cBuffer everytime
    std::u32string mCodePoints ;                             ///< buffer to hold current paragraph codepoints
    PARAGRAPH_T mCPParagraph ;                          ///< the paragraph for the mCodePoints array

    // --- Display / Loading State ---
    bool mRedrawFullDisplay ;                           ///< set to true if the editor should redraw everything
    bool mIsLoading ;                                   ///< true if loading file, else false
    bool mSuppressNotify ;                              ///< true to suppress listener notifications without affecting undo
    bool mSuppressUndo ;                                ///< true to suppress undo recording without implying file-load semantics

    // --- Paragraph Lookup Cache ---
    POSITION_T mLastParagraphFromPosition ;
    PARAGRAPH_T mLastParagraphFromPositionResult ;
    PARAGRAPH_T mLastNumParagraph ;

    // --- Math ---
    cMath mMath ;

    // --- Display Control ---
    eShowControl mShowControl ;

    // --- Word Count ---
    long mWordCount ;                                   ///< cached word count for VAR_WORD_COUNT variable

    // --- Clipboard ---
    std::vector<sUndoCharInfo> mCopyBuffer ;                  ///< the buffer used for copy/paste (with metadata)

    // --- Undo / Redo ---
    std::deque<sUndoGroup> mUndoStack ;                  ///< undo stack (groups of actions)
    std::deque<sUndoGroup> mRedoStack ;                  ///< redo stack (groups of actions)
    bool mUndoGroupOpen ;                                ///< true if accumulating into current group
    sUndoGroup mCurrentGroup ;                           ///< group being built
    int mUndoGroupNesting ;                              ///< for nested Begin/End calls
    bool mUndoDisabled ;                                 ///< true during undo/redo execution

    // --- Listeners ---
    std::vector<cDocumentListener*> mListeners ;        ///< registered listeners (not owned)

    // --- Threading ---
//    ctpl::thread_pool mThreadPool  ;
    int mMaxThreads ;
//    TP::ThreadPool mThreadPool ;
};

#endif // DOCUMENT_H
