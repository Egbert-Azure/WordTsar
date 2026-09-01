#ifndef WORDTSAR_H
#define WORDTSAR_H

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

#include <memory>
#include <string>

#include <QMainWindow>
#include <QVBoxLayout>
#include <QTextEdit>
#include <QLabel>
#include <QScrollArea>
#include <QProgressBar>
#include <QMenuBar>
#include <QMenu>
#include <QTimer>
#include <QSplitter>
#include <thread>

#include "src/gui/editor/editorctrl.h"
#include "src/gui/ruler/rulerctrl.h"
#include "src/gui/misc/busy.h"

class cEditorCtrl ;
class IMenuProvider ;

class cWordTsar : public QMainWindow
{
    Q_OBJECT

    // =================================================================
    // METHODS
    // =================================================================

public:
    // Construction / Destruction
    cWordTsar(int argc, char *argv[], QWidget *parent = nullptr);
    ~cWordTsar(void);

    // File operations
//    void SetFile(QString filename) ;
    void LoadFile(QString name) ;

    // Status bar
    void UpdateStatus(cEditorCtrl *editor) ;
    void SetStatus(QString text, bool progress = false, int percent = 0) ;

    // Configuration
    void ReadConfig(void) ;
    void WriteConfig(void) ;
    void ApplyDisplaySettings(void) ;
    void OnInputModeChanged(eInputMode mode) ;
    void ApplyScreenColors(void) ;

    // Reveal Codes pane management
    void ToggleRevealCodes(void) ;
    bool IsRevealCodesVisible(void) const { return mRevealCodesVisible; }
    void UpdateCommandTagsLabel(eDisplayMode mode) ;

    // used for testing
    cEditorCtrl *GetEditorCtrl(void) { return mEditor ; }

protected :
    // Qt event overrides
    void closeEvent(QCloseEvent *event) override ;
    void showEvent(QShowEvent *event) override ;
    bool eventFilter(QObject *obj, QEvent *event) override ;

private :
    // Menu setup
    void CreateMenus(void) ;
    void UpdateRecentFilesMenu(void) ;
    void ApplyHelpColors(const QColor& helpBg, const QColor& helpFg, const QColor& keyColor, const QColor& keyBgColor) ;

private slots :
    // File Menu
    void Open(void) ;
    void Save(void) ;
    void SaveAs(void) ;
    void SaveandClose(void) ;
    void OpenRecentFile(void) ;
    void Print(void) ;
    void PrintPreview(void) ;
    void AboutWordTsar(void) ;
    void ExitWordTsar(void) ;

    // Edit Menu
    void Undo(void) ;
    void Redo(void) ;
    void MarkBlockStart(void) ;
    void MarkBlockEnd(void) ;
    void MoveBlock(void) ;
    void CopyBlock(void) ;
    void CopyFromClipboard(void) ;
    void CopyToClipboard(void) ;
    void DeleteBlock(void) ;
    void DeleteWord(void) ;
    void DeleteLine(void) ;
    void DeleteLineLeft(void) ;
    void DeleteLineRight(void) ;
    void DeleteToChar(void) ;
    void MarkPrevBlock(void) ;
    void Find(void) ;
    void FindandReplace(void) ;
    void FindNext(void) ;
    void GotoChar(void) ;
    void GotoPage(void) ;
    void Goto1(void) ;
    void Goto2(void) ;
    void Goto3(void) ;
    void Goto4(void) ;
    void Goto5(void) ;
    void Goto6(void) ;
    void Goto7(void) ;
    void Goto8(void) ;
    void Goto9(void) ;
    void Goto0(void) ;
    void GotoFont(void) ;
    void GotoStyle(void) ;
    void GotoNote(void) ;
    void GotoPrevPos(void) ;
    void GotoLastFindandReplace(void) ;
    void GotoStartBlock(void) ;
    void GotoEndBlock(void) ;
    void GotoDocumentStart(void) ;
    void GotoDocumentEnd(void) ;
    void GotoScrollUp(void) ;
    void GotoScrollDown(void) ;
    void Set1(void) ;
    void Set2(void) ;
    void Set3(void) ;
    void Set4(void) ;
    void Set5(void) ;
    void Set6(void) ;
    void Set7(void) ;
    void Set8(void) ;
    void Set9(void) ;
    void Set0(void) ;
    void EditNote(void) ;
    void NoteStartNumber(void) ;
    void NoteCOnvert(void) ;
    void NoteConcertForPrint(void) ;
    void NoteEndNoteLocation(void) ;
    void ColumnBlockMode(void) ;
    void ColumnReplaceMode(void) ;
    void AutoAlign(void) ;
    void CloseDialog(void) ;

    // View Menu
    void CommandTags(void) ;
    void BlockHighlight(void) ;
    void ScreenSettings(void) ;
    void SwitchModes(void) ;

    // InsertMenu
    void PageBreak(void) ;
    void ColumnBreak(void) ;
    void InsertDate(void) ;
    void InsertTime(void) ;
    void MathResult(void) ;
    void MathExpression(void) ;
    void MathDollar(void) ;
    void Filename(void) ;
    void Drive(void) ;
    void Directory(void) ;
    void Path(void) ;
    void VarDate(void) ;
    void VarTime(void) ;
    void VarPage(void) ;
    void VarLine(void) ;
    void VarFilename(void) ;
    void VarDirectory(void) ;
    void VarDrive(void) ;
    void VarPath(void) ;
    void VarWordCount(void) ;
    void ExtendedChar(void) ;
    void FileAtPrint(void) ;
    void Graphic(void) ;
    void NoteComment(void) ;
    void NoteFootnote(void) ;
    void NoteEndnote(void) ;
    void NoteAnnotation(void) ;
    void TOCEntry(void) ;
    void IndexEntry(void) ;
    void MarkIndex(void) ;
    void DotLeader(void) ;
    void ParOutlineNumber(void) ;

    // Style Menu
    void Bold(void) ;
    void Italics(void) ;
    void Underline(void) ;
    void font(void) ;
    void Strikeout(void) ;
    void Subscript(void) ;
    void Superscript(void) ;
    void DoubleStrike(void) ;
    void Color(void) ;
    void SelectParStyle(void) ;
    void ReturntoPrevStyle(void) ;
    void DefineParStyle(void) ;
    void CopyStyletoLibrary(void) ;
    void DeleteLibraryStyle(void) ;
    void RenameLibraryStyle(void) ;
    void RenameDocStyle(void) ;
    void Uppercase(void) ;
    void Lowercase(void) ;
    void Sentencecase(void) ;
    void Settings(void) ;

    // Layout Menu
    void CenterLine(void) ;
    void RightAlign(void) ;
    void LeftAlignParagraph(void) ;
    void CenterParagraph(void) ;
    void RightAlignParagraph(void) ;
    void JustifyParagraph(void) ;
    void RulerLine(void) ;
    void Columns(void) ;
    void Page(void) ;
    void Header(void) ;
    void Footer(void) ;
    void PageNumbering(void) ;
    void LineNumbering(void) ;
    void Alignment(void) ;
    void OverprintChar(void) ;
    void OverprintLine(void) ;
    void OptionalHyphen(void) ;
    void VerticalCenter(void) ;
    void KeepWordsTogether(void) ;
    void KeepLinesTogetherPage(void) ;
    void KeepLinesTogetherColumn(void) ;

    // Utilities Menu
    void SpellCheckGlobal(void) ;
    void SpellCheckRest(void) ;
    void SpellCheckWord(void) ;
    void SpellCheckType(void) ;
    void SpellCheckNotes(void) ;
    void Thesaurus(void) ;
    void LanguageChange(void) ;
    void Inset(void) ;
    void Calculator(void) ;
    void BlockMath(void) ;
    void SortBlockAsc(void) ;
    void SortBlockDes(void) ;
    void WordCount(void) ;
    void PlayMacro(void) ;
    void RecordMacro(void) ;
    void EditMacro(void) ;
    void SingleStep(void) ;
    void CopyMacro(void) ;
    void DeleteMacro(void) ;
    void RenameMacro(void) ;
    void DataFile(void) ;
    void NameVars(void) ;
    void SetVar(void) ;
    void SetVarMath(void) ;
    void AskVar(void) ;
    void If(void) ;
    void Else(void) ;
    void EndIf(void) ;
    void Top(void) ;
    void Bottom(void) ;
    void Clear(void) ;
    void Display(void) ;
    void PrintNTimes(void) ;
    void ReformatRest(void) ;
    void ReformatPara(void) ;
    void ReformatNotes(void) ;
    void RepeatKey(void) ;

    // Status bar timer slots
    void ClearStatus(void) ;
    void OnStatusTimer(void) ;

    // =================================================================
    // MEMBER VARIABLES
    // =================================================================

public:
    QScrollBar *mScrollbar ;
    QStatusBar *mStatusTop ;
    QStatusBar *mStatusBottom;
    cRulerCtrl *mRuler ;
    QTimer *mStatusTextTimer ;
    QTimer *mStatusTimer ;          // Status bar update timer

private:
    // Window layout widgets
//    QWidget *mBaseWidget;
    QWidget *mBaseWidget ;
    QVBoxLayout *mBaseLayout;
    QVBoxLayout *mLayout;
    QMenuBar *mMenuBar = nullptr;
    QToolBar *mMainToolBar = nullptr;

    // Status bar labels
    QLabel *mStatStyle ;
    QLabel *mStatFont ;
    QLabel *mStatBold ;
    QLabel *mStatItalic ;
    QLabel *mStatUnderline ;
    QLabel *mStatChange ;
    QLabel *mStatLeft ;
    QLabel *mStatCenter ;
    QLabel *mStatRight ;
    QLabel *mStatJustify ;
    QLabel *mStatMode ;
    QLabel *mStatPage ;
    QLabel *mStatInfo ;
    QLabel *mStatText ;

    // Busy indicator widgets
    cBusyIndicator *mBusy ;

    // Editor area widgets
    QHBoxLayout *mEditorLayout ;
    QWidget *mBaseEditor ;
    QSplitter *mSplitter ;                    // vertical splitter between editors
    QWidget *mMainEditorWidget ;              // container for main editor + its scrollbar
    QWidget *mCodesEditorWidget ;             // container for codes editor + its scrollbar
    QScrollBar *mCodesScrollbar ;             // codes editor scrollbar

    // Help panel labels
    QLabel *mHelpCtrl;
    QLabel *mHelpJCtrl;
    QLabel *mHelpPCtrl;
    QLabel *mHelpKCtrl;
    QLabel *mHelpQCtrl;
    QLabel *mHelpOCtrl;

    // Original help text strings (plain <b> tags, before color styling)
    QString mHelpTextMain;
    QString mHelpTextJ;
    QString mHelpTextK;
    QString mHelpTextO;
    QString mHelpTextP;
    QString mHelpTextQ;

    // Editor and reveal codes state
    cEditorCtrl *mEditor;
    cEditorCtrl *mRevealCodesEditor ;         // codes pane editor (nullptr when hidden)
    bool mRevealCodesVisible ;                // reveal codes toggle state
    QAction *mCommandTagsAction ;             // Command Tags / Show Formatting menu action
    QMenu *mRecentFilesMenu ;                 // File > Recent Files submenu

    // File loading
    QString mLoadFileName ;

    // Help timer
    QDeadlineTimer mHelpTimer ;

    // Status bar fonts and styles
    QFont mOnFont ;                     // font for active status indicators
    QFont mOffFont ;                    // font for inactive status indicators (italic)
    QString mOnStyle ;                  // stylesheet for active status indicators (bold + keystroke colors)
    QString mOffStyle ;                 // stylesheet for inactive status indicators (italic + status bar colors)

    // Menu provider
    IMenuProvider *mMenuProvider ;      // Menu label provider (Strategy Pattern)

    // UpdateStatus() change-detection caches (per-instance; previously function statics)
    int          mStatusHelpCounter = 0 ;
    eHelpDisplay mStatusLastHelp = HELP_MAIN ;
    std::string  mStatusOldStr ;
    bool         mStatusPrevChange = false ;
    bool         mStatusPrevBold = false ;
    bool         mStatusPrevItalic = false ;
    bool         mStatusPrevUnderline = false ;
    int          mStatusPrevJust = -1 ;
    bool         mStatusPrevInit = false ;
    bool         mStatusBottomPrevVisible = true ;  // tracks mEditor->mDispStatusBar for ChangeHelpLevel(0)
};



#endif   // _WORDTSAR_H_
