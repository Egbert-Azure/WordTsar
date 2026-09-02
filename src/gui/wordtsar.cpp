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
 * @class cWordTsar
 *
 * @brief Main application window implementation for the WordTsar GUI.
 *
 * Implements the cWordTsar class, the top-level QMainWindow that assembles
 * the complete WordTsar interface and coordinates all GUI components.
 *
 * @section guiwordtsar_assembly Window Assembly
 * Creates and manages all major UI components:
 * - Menu bar: built using IMenuProvider (cWordStarMenuProvider by default)
 *   with File, Edit, View, Style, Layout, Utilities, and Help menus
 * - Status bars: top (help text, formatting indicators) and bottom
 *   (line, column, page, insert/overwrite, busy indicator)
 * - Ruler control: cRulerCtrl positioned above the editor
 * - Editor widget: cEditorCtrl as the central editing surface
 * - Help panels: WordStar-style help text displayed below the editor
 * - Reveal codes: QSplitter-based split view with sibling editor
 *
 * @section guiwordtsar_startup Application Startup
 * - Reads configuration from cConfig (INI file)
 * - Processes command-line arguments for file loading
 * - Sets up the window title, size, and initial state
 * - Creates the cLayout and attaches it to the editor
 *
 * @section guiwordtsar_status Idle-Time Updates
 * A QTimer drives periodic status bar updates during idle time:
 * - Line, column, and page number from the editor
 * - Insert/overwrite mode indicator
 * - Memory usage reporting for debugging
 * - Formatting state indicators (bold, italic, underline, etc.)
 *
 * @section guiwordtsar_lifecycle Window Lifecycle
 * - closeEvent(): prompts to save unsaved changes, writes config, exits
 * - ReadConfig()/WriteConfig(): load/save application settings via cConfig
 *
 * @author Gerald Brandt
 * @copyright GNU Affero General Public License v3.0
 *
 * @see cWordTsar Main application window class
 * @see cEditorCtrl GUI editor control
 * @see cLayout GUI layout engine
 * @see cRulerCtrl Ruler widget
 * @see IMenuProvider Menu label provider interface
 * @see cWordStarMenuProvider WordStar menu label provider
 * @see cConfig Application configuration manager
 */

#include <algorithm>
#include <string>

#include <QtGlobal>
#include <QtWidgets>
#include <QVBoxLayout>
#include <QFont>
#include <QInputDialog>

#include "src/core/include/version.h"
#include "src/core/include/utils.h"
#include "src/gui/wordtsar.h"
#include "src/gui/editor/editorctrl.h"
#include "src/gui/utils/fontutils.h"
#include "src/gui/misc/busy.h"
#include "src/core/comm/commserver.h"
#include "src/gui/menu/menuprovider.h"
#include "src/gui/menu/wordstarmenu.h"
#include "src/gui/menu/modernmenu.h"
#include "src/input/wordtsarinput.h"

#include "src/core/utils/config.h"


#ifdef Q_OS_WINDOWS
#include <psapi.h>
#endif

#ifdef Q_OS_MACOS
#include <mach/mach.h>
#endif

#ifdef Q_OS_LINUX
#include <sys/resource.h>
#endif

const char HELPDELAY = 5 ;          // wait x * 200 ms before showing help


cWordTsar::cWordTsar(int argc, char *argv[], QWidget *parent)
    : QMainWindow(parent)
{
    UNUSED_ARGUMENT(argc) ;
    UNUSED_ARGUMENT(argv) ;


#ifdef Q_OS_MACOS
    QCoreApplication::setAttribute(Qt::AA_MacDontSwapCtrlAndMeta) ;
#endif
QString appFilePath = QCoreApplication::applicationFilePath();
printf("Application file path: %s\n", appFilePath.toStdString().c_str());
    // create the base widget and set it's layout
    if (objectName().isEmpty())
    {
        setObjectName(QStringLiteral("MainWindow"));
    }
    resize(400, 300);
    mBaseWidget = new QWidget(this) ;
    mBaseWidget->setObjectName(QStringLiteral("mBaseWidget"));
    mBaseLayout = new QVBoxLayout(mBaseWidget);
    mBaseLayout->setSpacing(0);
    mBaseLayout->setObjectName(QStringLiteral("mBaseLayout"));
    mBaseLayout->setContentsMargins(0, 0, 0, 0);

    mLayout = new QVBoxLayout();
    mLayout->setSpacing(0);
    mLayout->setObjectName(QStringLiteral("mLayout"));

    // Initialize menu provider (WordStar mode by default)
    mMenuProvider = new cWordStarMenuProvider() ;

    CreateMenus() ;

    mLayout->addWidget(mMenuBar) ;


    // create the top status bar
    mStatusTop = new QStatusBar(this) ;
    mStatusTop->setObjectName("mStatusTop") ;
    mStatusTop->setSizeGripEnabled(false);
    mStatusTop->setMaximumSize(16777215, 20);                   // @TODO replace with https://stackoverflow.com/questions/22508296/how-to-get-the-size-height-of-a-label-after-the-word-wrap

    mStatStyle = new QLabel("Body Text", this) ;
    mStatusTop->addPermanentWidget(mStatStyle, 1) ;

    mStatFont = new QLabel("Courier 10 Pitch 12", this) ;
    mStatusTop->addPermanentWidget(mStatFont, 1) ;

    mStatBold = new QLabel(" B ", this) ;
    mStatusTop->addPermanentWidget(mStatBold);

    mStatItalic = new QLabel(" I ", this) ;
    mStatusTop->addPermanentWidget(mStatItalic);

    mStatUnderline = new QLabel( " U ", this) ;
    mStatusTop->addPermanentWidget(mStatUnderline);

    mStatChange = new QLabel(" * ", this) ;
    mStatusTop->addPermanentWidget(mStatChange);

    mStatLeft = new QLabel(" L ", this) ;
    mStatusTop->addPermanentWidget(mStatLeft);

    mStatCenter = new QLabel(" C ", this) ;
    mStatusTop->addPermanentWidget(mStatCenter);

    mStatRight = new QLabel(" R ", this) ;
    mStatusTop->addPermanentWidget(mStatRight);

    mStatJustify = new QLabel(" J ", this) ;
    mStatusTop->addPermanentWidget(mStatJustify);

    QLabel *mStatFill = new QLabel("   ", this) ;
    mStatusTop->addPermanentWidget(mStatFill) ;

    mOnFont = mStatItalic->font() ;
    mOnFont.setBold(true) ;
    mOnFont.setItalic(false) ;

    mOffFont = mStatItalic->font() ;
    mOffFont.setBold(false) ;
    mOffFont.setItalic(true) ;

    // Make status indicators clickable via event filter, show hand cursor on hover
    mStatChange->installEventFilter(this) ;
    mStatChange->setCursor(Qt::PointingHandCursor) ;
    mStatBold->installEventFilter(this) ;
    mStatBold->setCursor(Qt::PointingHandCursor) ;
    mStatItalic->installEventFilter(this) ;
    mStatItalic->setCursor(Qt::PointingHandCursor) ;
    mStatUnderline->installEventFilter(this) ;
    mStatUnderline->setCursor(Qt::PointingHandCursor) ;
    mStatLeft->installEventFilter(this) ;
    mStatLeft->setCursor(Qt::PointingHandCursor) ;
    mStatCenter->installEventFilter(this) ;
    mStatCenter->setCursor(Qt::PointingHandCursor) ;
    mStatRight->installEventFilter(this) ;
    mStatRight->setCursor(Qt::PointingHandCursor) ;
    mStatJustify->installEventFilter(this) ;
    mStatJustify->setCursor(Qt::PointingHandCursor) ;
    mStatFont->installEventFilter(this) ;
    mStatFont->setCursor(Qt::PointingHandCursor) ;

    mLayout->addWidget(mStatusTop, 1) ;

    // Create platform-specific monospace font for help panels
#if defined(Q_OS_MACOS)
    QFont helpFont("Menlo", 12) ;
#elif defined(Q_OS_WINDOWS)
    QFont helpFont("Consolas", 12) ;
#else
    QFont helpFont("Monospace", 10) ;
#endif
#ifndef Q_OS_MACOS
    helpFont.setPointSize(10) ;
#endif

    // Help text strings wrapped in HTML <pre> for monospace columnar layout.
    // Formatting: <b> for key bindings, <u> for section headers, <i> for less common items.
    // Matches original WordStar control codes: ^B=bold, ^S=underline, ^Y=italic.
    QString helpj = "<pre style=\"margin:0\">"
        "                        <b>----- E D I T   M E N U -----</b>\n"
        "  <u>CURSOR</u>      <u>SCROLL</u>        <u>DELETE</u>      <u>OTHER</u>                 <u>MENUS</u>\n"
        " <b>^E</b> up       <b>^W</b> up         <b>^G</b> char    <b>F1</b> help                <b>^O</b> onscreen format\n"
        " <b>^X</b> down     <b>^Z</b> down       <b>^T</b> word    <b>^I</b> tab                 <b>^K</b> block &amp; save\n"
        " <b>^S</b> left     <b>^R</b> up screen  <b>^Y</b> line    <b>^V</b> turn insert off     <b>^M</b> macros\n"
        " <b>^D</b> right    <b>^C</b> down      <b>Del</b> char    <b>^L</b> find/replace again  <b>^P</b> print controls\n"
        " <b>^A</b> word left   screen     <b>^U</b> unerase                        <b>^Q</b> quick functions\n"
        " <b>^F</b> word right                   <b>&#8984;,</b> Preferences\n"
        "                                <b>&#8984;&#8963;F</b> Full Screen (or F11)"
        "</pre>" ;

    QString helpmm = "<pre style=\"margin:0\">"
        "                  <b>----- MACRO MENU -----</b>\n"
        " "
        "    <u>MACRO FUNCTIONS</u>                   <u>INSERT</u>\n"
        " <i>P play</i>          <i>E rename</i>        <b>@</b> today's date         <b>*</b> current filename\n"
        " <i>R record</i>        <i>O copy</i>          <b>!</b> current time         <b>:</b> current drive\n"
        " <i>D edit/create</i>   <i>Y delete</i>        <i>= last math result</i>     <b>.</b> current directory\n"
        " <i>S single step</i>                   <i># last math expression</i> <b>\\</b> current path\n"
        "                                 <i>$ last math as dollar</i>"
        "</pre>" ;

    QString helpk = "<pre style=\"margin:0\">"
        "                  <b>----- B L O C K   &amp;   S A V E   M E N U -----</b>\n"
        "    <u>SAVE</u>                     <u>BLOCK</u>                       <u>WINDOW</u>\n"
        "  <b>D</b> save                   <b>B</b> begin block               <i>A copy between</i>\n"
        "  <b>T</b> save as                <b>K</b> end block                 <i>G move between</i>\n"
        "  <b>S</b> save and resume        <b>C</b> copy                      \n"
        "  <b>X</b> save and exit          <b>V</b> move                        <u>CASE</u>\n"
        "  <b>Q</b> abandon changes        <b>Y</b> delete                    <b>\"</b> upper\n"
        "    <u>FILE</u>                   <i>W write to disk</i>             <b>'</b> lower\n"
        "  <i>O copy</i>                   <i>M math</i>                      <b>.</b> sentence\n"
        "  <i>E rename</i>                 <i>Z sort</i>                      \n"
        "  <i>J delete</i>                 <b>?</b> word count                  <u>CURSOR</u>\n"
        "  <b>P</b> print                  <b>H</b> turn disp on/off        <b>0-9</b> set marker\n"
        "  <i>\\ fax</i>                    <b>U</b> mark previous block       \n"
        "  <i>L change drive/dir</i>       <b>&lt;</b> unmark block                <u>SYSTEM CLIPBOARD</u>\n"
        "  <b>R</b> insert a file          <i>N turn column mode on</i>       <b>[</b> copy from clipboard\n"
        "  <i>F run command</i>            <i>I turn column replace on</i>    <b>]</b> copy to clipboard\n"
        "</pre>" ;

    QString helpp = "<pre style=\"margin:0\">"
        "              <b>----- P R I N T   C O N T R O L S   M E N U -----</b>\n"
        "            <u>BEGIN &amp; END</u>                                <u>OTHER</u> \n"
        "    <b>B</b> bold         <b>X</b> strike out         <i>H overprint char</i>   <i>O binding space</i>\n"
        "    <b>S</b> underline    <i>D double strike</i>    <i>RET overprint line</i>   <i>C print pause</i>\n"
        "    <b>V</b> subscript    <b>Y</b> italics            <i>F phantom space</i>    <i>I 8-column tab</i>\n"
        "    <b>T</b> superscript  <i>K indexing</i>           <i>G phantom rubout</i>   <i>. dot leader</i>\n"
        "                                        <i>* graphics tag</i>     <i>0 extended characters</i>\n"
        "               <u>STYLE</u>                    <i>&amp; start Inset</i>\n"
        "    <b>=/+</b> select font <i>N Normal Font</i>\n"
        "    <b>-</b> select color  <i>A alternate font</i>    <i>Q W E R ! custom</i>    <i>? select printer</i>"
        "</pre>" ;

    QString helpq = "<pre style=\"margin:0\">"
        "                      <b>----- Q U I C K   M E N U -----</b>\n"
        "            <u>CURSOR</u>              <u>FIND</u>            <u>OTHER</u>             <u>SPELL</u>\n"
        " <b>E</b> upper left   <b>P</b> previous   <b>F</b> find text     <b>U</b> align rest doc  <b>L</b> check document\n"
        " <b>X</b> lower right  <b>V</b> prev find  <b>A</b> find/replace  <i>M math</i>  <i>Q repeat</i>  <b>N</b> check word\n"
        " <b>S</b> begin line   <b>B</b> beg block  <b>G</b> char forward  <i>J thesaurus</i>       <b>O</b> enter word\n"
        " <b>D</b> end line     <b>K</b> end block  <b>H</b> char back                         <u>DELETE</u>\n"
        " <b>R</b> beg doc    <b>0-9</b> marker     <b>I</b> page/line       <u>SCROLL</u>        <b>Del</b> line to left\n"
        " <b>C</b> end doc                   <b>=</b> next font     <i>W up, repeat</i>      <b>Y</b> line to right\n"
        "                             <i>&lt; next style</i>    <i>Z dn, repeat</i>      <b>T</b> to character\n"
        "</pre>" ;

    QString helpo = "<pre style=\"margin:0\">"
        "           <b>----- O N S C R E E N   F O R M A T   M E N U -----</b>\n"
        "   <u>MARGINS &amp; TABS</u>            <u>TYPING</u>                         <u>DISPLAY</u>\n"
        " <i>L left</i>  <i>R right</i>     <b>C</b> center line                   <b>P</b> page preview\n"
        " <i>G temporary indent</i>   <b>]</b> right flush line              <b>D</b> turn command tags off\n"
        " <i>X release margin</i>    <i>V vertically center</i>             <i>B change screen settings</i>\n"
        " <i>I set/clear tabs</i>    <i>E enter soft hyphen</i>             <i>K open or switch window</i>\n"
        " <i>O ruler to text</i>     <i>H turn auto-hyphenation off</i>     <i>M size current window</i>\n"
        " <i>U column layout</i>     <b>J</b> turn justification on         <b>?</b> status\n"
        " <b>Y</b> page layout       <i>A turn auto-align off</i>           <i>Z paragraph number</i>\n"
        " <i>F paragraph styles</i>  <i>W turn word wrap off</i>            <i># page numbering</i>\n"
        "                     --------\n"
        "                     <b>&lt;</b> left align paragraph\n"
        "                     <b>=</b> center paragraph\n"
        "                     <b>&gt;</b> right align paragraph\n"
        "                     <b>+</b> justify paragraph\n"
        "                                                     <b>T</b> toggle display mode\n"
        " <i>S set line spacing</i> <i>RET turn Enter closes dialog off</i> <i>N notes</i>\n"
        "</pre>" ;

    // Save original help text strings (plain <b> tags) for color re-application
    mHelpTextMain = helpj ;
    mHelpTextM = helpmm ;
    mHelpTextK = helpk ;
    mHelpTextO = helpo ;
    mHelpTextP = helpp ;
    mHelpTextQ = helpq ;

    // Create help panels as lightweight QLabel widgets with rich text
    mHelpCtrl = new QLabel(mBaseWidget) ;
    mHelpCtrl->setObjectName(QStringLiteral("mHelpCtrl")) ;
    mHelpCtrl->setTextFormat(Qt::RichText) ;
    mHelpCtrl->setFont(helpFont) ;
    mHelpCtrl->setText(helpj) ;
    mHelpCtrl->setFocusPolicy(Qt::NoFocus) ;
    mLayout->addWidget(mHelpCtrl) ;

    mHelpMCtrl = new QLabel(mBaseWidget) ;
    mHelpMCtrl->setObjectName(QStringLiteral("mHelpMCtrl")) ;
    mHelpMCtrl->setTextFormat(Qt::RichText) ;
    mHelpMCtrl->setFont(helpFont) ;
    mHelpMCtrl->setText(helpmm) ;
    mHelpMCtrl->setFocusPolicy(Qt::NoFocus) ;
    mLayout->addWidget(mHelpMCtrl) ;
    mHelpMCtrl->hide() ;

    mHelpKCtrl = new QLabel(mBaseWidget) ;
    mHelpKCtrl->setObjectName(QStringLiteral("mHelpKCtrl")) ;
    mHelpKCtrl->setTextFormat(Qt::RichText) ;
    mHelpKCtrl->setFont(helpFont) ;
    mHelpKCtrl->setText(helpk) ;
    mHelpKCtrl->setFocusPolicy(Qt::NoFocus) ;
    mLayout->addWidget(mHelpKCtrl) ;
    mHelpKCtrl->hide() ;

    mHelpOCtrl = new QLabel(mBaseWidget) ;
    mHelpOCtrl->setObjectName(QStringLiteral("mHelpOCtrl")) ;
    mHelpOCtrl->setTextFormat(Qt::RichText) ;
    mHelpOCtrl->setFont(helpFont) ;
    mHelpOCtrl->setText(helpo) ;
    mHelpOCtrl->setFocusPolicy(Qt::NoFocus) ;
    mLayout->addWidget(mHelpOCtrl) ;
    mHelpOCtrl->hide() ;

    mHelpPCtrl = new QLabel(mBaseWidget) ;
    mHelpPCtrl->setObjectName(QStringLiteral("mHelpPCtrl")) ;
    mHelpPCtrl->setTextFormat(Qt::RichText) ;
    mHelpPCtrl->setFont(helpFont) ;
    mHelpPCtrl->setText(helpp) ;
    mHelpPCtrl->setFocusPolicy(Qt::NoFocus) ;
    mLayout->addWidget(mHelpPCtrl) ;
    mHelpPCtrl->hide() ;

    mHelpQCtrl = new QLabel(mBaseWidget) ;
    mHelpQCtrl->setObjectName(QStringLiteral("mHelpQCtrl")) ;
    mHelpQCtrl->setTextFormat(Qt::RichText) ;
    mHelpQCtrl->setFont(helpFont) ;
    mHelpQCtrl->setText(helpq) ;
    mHelpQCtrl->setFocusPolicy(Qt::NoFocus) ;
    mLayout->addWidget(mHelpQCtrl) ;
    mHelpQCtrl->hide() ;


    // create the ruler control
    mRuler = new cRulerCtrl(this) ;
    mLayout->addWidget(mRuler);


    // create the main editor with resizable splitter
    mBaseEditor = new QWidget(this) ;
    mEditorLayout = new QHBoxLayout(mBaseEditor) ;
    mEditorLayout->setSpacing(0);
    mEditorLayout->setObjectName(QStringLiteral("mEditorLayout"));
    mEditorLayout->setContentsMargins(0, 0, 0, 0);

    mSplitter = new QSplitter(Qt::Vertical, mBaseEditor) ;
    mSplitter->setChildrenCollapsible(false) ;

    // Main editor + its scrollbar in a container widget
    mMainEditorWidget = new QWidget(mSplitter) ;
    QHBoxLayout *mainLayout = new QHBoxLayout(mMainEditorWidget) ;
    mainLayout->setSpacing(0) ;
    mainLayout->setContentsMargins(0, 0, 0, 0) ;
    mScrollbar = new QScrollBar(mMainEditorWidget) ;
    mEditor = new cEditorCtrl(mMainEditorWidget) ;
    mainLayout->addWidget(mEditor) ;
    mainLayout->addWidget(mScrollbar) ;
    mSplitter->addWidget(mMainEditorWidget) ;

    // Codes editor starts hidden
    mCodesEditorWidget = nullptr ;
    mCodesScrollbar = nullptr ;
    mRevealCodesEditor = nullptr ;
    mRevealCodesVisible = false ;

    mEditorLayout->addWidget(mSplitter) ;

    mEditor->setObjectName(QStringLiteral("mEditor"));
    mEditor->SetFrame(this) ;
    mEditor->SetScrollbar(mScrollbar) ;  // Connect scrollbar to editor

    // Set editor font (hideous font, but what Wordstar defaulted to)
    QFont font("Monospace", 10) ;
    mEditor->SetFont(font.toString() .toStdString()) ;

    mLayout->addWidget(mBaseEditor) ;

    mBaseLayout->addLayout(mLayout);

    setCentralWidget(mBaseWidget);

//    mMainToolBar = new QToolBar(this);
//    mMainToolBar->setObjectName(QStringLiteral("mMainToolBar"));
//    addToolBar(Qt::TopToolBarArea, mMainToolBar);

    mStatusBottom = new QStatusBar(this);
    mStatusBottom->setObjectName("mStatusBottom");
    mStatusBottom->setSizeGripEnabled(true);
//    mStatusBottom->setMaximumSize(16777215, 20);                   // @TODO replace with https://stackoverflow.com/questions/22508296/how-to-get-the-size-height-of-a-label-after-the-word-wrap

    setStatusBar(mStatusBottom);

    mStatText = new QLabel(" ", this) ;
    mStatusBottom->addPermanentWidget(mStatText, 1);

    mBusy = new cBusyIndicator(this) ;
    mStatusBottom->addPermanentWidget(mBusy) ;

    mStatMode = new QLabel("Insert", this) ;
    mStatusBottom->addPermanentWidget(mStatMode) ;

    mStatPage = new QLabel("  Page 1 of 1", this) ;
    mStatusBottom->addPermanentWidget(mStatPage) ;

    mStatInfo = new QLabel(" Line 1 V0.00\"  Column 1 H0.00\"  Words: 0 Chars: 0", this) ;
    mStatusBottom->addPermanentWidget(mStatInfo, 1) ;

    // Make bottom status bar mode and page labels clickable
    mStatMode->installEventFilter(this) ;
    mStatMode->setCursor(Qt::PointingHandCursor) ;
    mStatPage->installEventFilter(this) ;
    mStatPage->setCursor(Qt::PointingHandCursor) ;

    mEditor->SetRuler(mRuler);
    mEditor->setFocusPolicy(Qt::StrongFocus);
    mEditor->setFocus();  // Give keyboard focus to main editor (not help panel)

    setWindowIcon(QIcon(":/gui/images/icon64x64.png")) ;

    // Set initial window title with version info
    mEditor->SetTitle(mEditor->mFileName);

    ReadConfig();
    ApplyDisplaySettings();
    ApplyScreenColors() ;    // Initialize mOnStyle/mOffStyle for status indicator colors
    UpdateCommandTagsLabel(DISPLAY_CONTINUOUS) ;    // Set initial menu label (starts in continuous mode)

    mStatusTextTimer = new QTimer(this) ;
    connect(mStatusTextTimer, &QTimer::timeout, this, &cWordTsar::ClearStatus) ;

    // Create status update timer
    mStatusTimer = new QTimer(this) ;
    mStatusTimer->setSingleShot(false) ;  // Repeating timer
    connect(mStatusTimer, &QTimer::timeout, this, &cWordTsar::OnStatusTimer) ;
    mStatusTimer->start(STATUS_UPDATE_INTERVAL_MS) ;
}




cWordTsar::~cWordTsar(void)
{
    // Defensive: stop status timers before teardown so a queued timeout can never
    // reach mEditor during destruction (mEditor lives under mBaseWidget).
    if (mStatusTimer)
    {
        mStatusTimer->stop() ;
    }
    if (mStatusTextTimer)
    {
        mStatusTextTimer->stop() ;
    }

    WriteConfig() ;

    // Clean up menu provider
    delete mMenuProvider ;

}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  event   [IN] - the close event
///
/// @return nothing
///
/// @brief
/// handle a window close event
///
/////////////////////////////////////////////////////////////////////////////
void cWordTsar::closeEvent(QCloseEvent *event)
{
    if (mEditor->CloseEvent())
    {
        event->accept();
    }
    else
    {
        event->ignore();
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  QShowEvent *event [in] - the show event
///
/// @return nothing
///
/// @brief
/// Handle window show event. Sets menu bar height constraint AFTER the window
/// is shown, when DPI information is correctly available. This fixes the menu
/// bar overflow issue on Windows with fractional scaling (125%, 150%, etc.)
/// where QFontMetrics returns incorrect values during construction.
///
/// @see https://forum.qt.io/topic/140620/qfontmetrics-height-and-weight-does-not-change-when-windows-scale-changed
///
/////////////////////////////////////////////////////////////////////////////
void cWordTsar::showEvent(QShowEvent *event)
{
    QMainWindow::showEvent(event) ;

    // Set menu bar height constraint AFTER window is shown (when DPI is known)
    static bool firstShow = true ;
    if (firstShow)
    {
        firstShow = false ;

        // Use QFontMetrics with paint device for DPI-aware mMeasurement
        const QFont &f = mMenuBar->font() ;
        QFontMetrics fmetrics(f, mMenuBar) ;  // Pass mMenuBar as paint device
        auto fheight = fmetrics.height() ;
        mMenuBar->setMaximumHeight(fheight + 10) ;
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  obj [in] object that received the event
/// @param  event [in] the event to filter
///
/// @return bool - true if the event was handled
///
/// @brief
/// Handle mouse clicks on status bar indicator labels. Clicking an
/// indicator toggles the corresponding formatting or alignment mode.
///
/////////////////////////////////////////////////////////////////////////////
bool cWordTsar::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::MouseButtonPress)
    {
        bool handled = false ;

        if (obj == mStatBold)
        {
            Bold() ;
            handled = true ;
        }
        else if (obj == mStatItalic)
        {
            Italics() ;
            handled = true ;
        }
        else if (obj == mStatUnderline)
        {
            Underline() ;
            handled = true ;
        }
        else if (obj == mStatChange)
        {
            CommandTags() ;
            handled = true ;
        }
        else if (obj == mStatLeft)
        {
            mEditor->SetAlignment(JUST_LEFT) ;
            handled = true ;
        }
        else if (obj == mStatCenter)
        {
            mEditor->SetAlignment(JUST_CENTER) ;
            handled = true ;
        }
        else if (obj == mStatRight)
        {
            mEditor->SetAlignment(JUST_RIGHT) ;
            handled = true ;
        }
        else if (obj == mStatJustify)
        {
            mEditor->SetAlignment(JUST_JUST) ;
            handled = true ;
        }
        else if (obj == mStatFont)
        {
            font() ;
            handled = true ;
        }
        else if (obj == mStatMode)
        {
            mEditor->ToggleInsertOverwrite() ;
            handled = true ;
        }
        else if (obj == mStatPage)
        {
            GotoPage() ;
            handled = true ;
        }

        if (handled)
        {
            // Same update as menu actions (see mMenuBar::triggered connection)
            mEditor->PerformPostCommandUpdate() ;
            QTimer::singleShot(1, mEditor, SLOT(OnIdle())) ;
            return true ;
        }
    }

    return QMainWindow::eventFilter(obj, event) ;
}



void cWordTsar::LoadFile(QString name)
{
    mLoadFileName = name ;
    if (mEditor->LoadFile(name.toStdString()))
    {
        // Close the reveal codes pane if it was open for the previous document --
        // it holds a stale layout for the old document and must be re-created
        if (mRevealCodesVisible)
        {
            ToggleRevealCodes() ;
        }

        // Record in recent files list
        cConfig config ;
        config.Load() ;
        config.AddRecentFile(QFileInfo(name).absoluteFilePath().toStdString()) ;
        config.Save() ;
        UpdateRecentFilesMenu() ;

        // Restore reveal codes pane if the loaded file state calls for it.
        // Only valid in continuous mode (reveal codes is not shown in page mode).
        if (mEditor->mRevealCodesVisible && mEditor->GetDisplayMode() == DISPLAY_CONTINUOUS)
        {
            ToggleRevealCodes() ;
        }
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Rebuild the Recent Files submenu from the config file. Clears existing
/// items and repopulates with up to 10 recent file paths. Each entry is
/// numbered 1-10 with an accelerator on the number.
///
/////////////////////////////////////////////////////////////////////////////
void cWordTsar::UpdateRecentFilesMenu(void)
{
    mRecentFilesMenu->clear() ;

    cConfig config ;
    config.Load() ;

    bool hasEntries = false ;
    for (int i = 0; i < 10; i++)
    {
        if (!config.mRecentFiles[i].empty())
        {
            // Number prefix with accelerator: "&1 /path/to/file"
            QString label = QString("&%1 %2")
                .arg((i + 1) % 10)
                .arg(QString::fromStdString(config.mRecentFiles[i])) ;

            QAction *action = mRecentFilesMenu->addAction(label) ;
            action->setData(QString::fromStdString(config.mRecentFiles[i])) ;
            connect(action, &QAction::triggered, this, &cWordTsar::OpenRecentFile) ;
            hasEntries = true ;
        }
    }

    if (!hasEntries)
    {
        QAction *emptyAction = mRecentFilesMenu->addAction("No Recent Files") ;
        emptyAction->setEnabled(false) ;
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Slot called when a recent file menu item is clicked. Extracts the file
/// path from the action's data and loads it.
///
/////////////////////////////////////////////////////////////////////////////
void cWordTsar::OpenRecentFile(void)
{
    QAction *action = qobject_cast<QAction*>(sender()) ;
    if (action)
    {
        QString path = action->data().toString() ;
        if (!path.isEmpty())
        {
            LoadFile(path) ;
        }
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  nothing
///
/// @return nothing
///
/// @brief
/// Timer callback to update status bar with current editor state.
/// Called every STATUS_UPDATE_INTERVAL_MS (200ms).
///
/////////////////////////////////////////////////////////////////////////////
void cWordTsar::OnStatusTimer(void)
{
    UpdateStatus(mEditor) ;
}



void cWordTsar::UpdateStatus(cEditorCtrl *editor)
{
    if(editor == NULL)
    {
        return ;
    }
    
    if(mStatusBottom == NULL)
    {
        return ;
    }

    // Skip while document/layout is being mutated to avoid racing reads.
    if (editor->IsBusy())
    {
        return ;
    }

    sStatus cstatus ;
    editor->GetStatus(cstatus) ;

    // Help level gates whether the delayed ^K/^Q/^O/^P/^M submenus ever
    // appear (WordStar 7 manual, "Change Help Level": submenus show only at
    // levels 2-3 -- level 4 relies on the pull-down bar instead, and 0-1
    // show no classic panels at all). The main Edit Menu's own level-3-only
    // gating already happens in ChangeHelpLevel(), which only ever sets the
    // idle mHelpDisplay to HELP_MAIN or HELP_NONE -- this only needs to
    // catch the transient HELP_CTRLx states a chord prefix sets regardless
    // of level.
    if((editor->mHelpLevel < 2 || editor->mHelpLevel > 3) &&
       (cstatus.help == HELP_CTRLM || cstatus.help == HELP_CTRLK ||
        cstatus.help == HELP_CTRLO || cstatus.help == HELP_CTRLP ||
        cstatus.help == HELP_CTRLQ))
    {
        cstatus.help = HELP_NONE ;
    }

    std::string str ;

    // Build block markers string
    std::string blockMarkers = "" ;
    if (cstatus.blockSet)
    {
        blockMarkers = " <B><K>" ;
    }

    // Build place markers string (e.g. "150" if markers 1, 5, 0 are set)
    std::string placeMarkers = "" ;
    if (!cstatus.markers.empty())
    {
        placeMarkers = cstatus.markers + "  " ;
    }

    // Build WordStar-style status line in 3 parts (mode, page, info)
    // Split into separate labels so mode and page are clickable
    std::string modeStr = string_sprintf("%s%s%s",
                         placeMarkers.c_str(),
                         (cstatus.mode ? "Insert" : "Overwrite"),
                         blockMarkers.c_str()) ;

    std::string pageStr = string_sprintf("  Page %ld of %ld",
                         cstatus.page, cstatus.pagecount) ;

    str = string_sprintf(" Line %ld V%.2f%s  Column %ld H%.2f%s  Words: %ld Chars: %ld",
                         cstatus.line,
                         cstatus.vPosition, cstatus.measureSuffix.c_str(),
                         cstatus.column,
                         cstatus.hPosition, cstatus.measureSuffix.c_str(),
                         cstatus.wordcount, cstatus.charcount) ;

#ifdef DEBUG
    // Append memory usage in debug builds
    double memoryusage = 0 ;

#ifdef Q_OS_WINDOWS
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc)))
    {
        memoryusage = pmc.WorkingSetSize ;
    }
#endif

#ifdef Q_OS_MACOS
    mach_task_basic_info info;
    mach_msg_type_number_t infoCount = MACH_TASK_BASIC_INFO_COUNT;
    if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO,
                  reinterpret_cast<task_info_t>(&info), &infoCount) == KERN_SUCCESS)
    {
        memoryusage = (double)info.resident_size / 1024.0 / 1024.0 ;
    }
#endif

#ifdef Q_OS_LINUX
    struct rusage usage ;
    if(getrusage(RUSAGE_SELF, &usage) == 0)
    {
        memoryusage = (double)(usage.ru_maxrss) / 1024.0 ;
    }
#endif

    str += string_sprintf("  %.2f MB", static_cast<double>(memoryusage)) ;
#endif

    // Combine all parts for change detection, then update labels
    std::string combined = modeStr + pageStr + str ;
    if(mStatusOldStr != combined)
    {
        mStatMode->setText(modeStr.c_str()) ;
        mStatPage->setText(pageStr.c_str()) ;
        mStatInfo->setText(str.c_str()) ;
        mStatusOldStr = combined ;
    }

    mStatStyle->setText(cstatus.style.c_str()) ;
    mStatFont->setText(cstatus.font.c_str()) ;

    // Track previous indicator state to avoid calling setStyleSheet every tick
    // (calling setStyleSheet on a 200ms timer leaks memory in Qt)
    if (!mStatusPrevInit || cstatus.showcontrol != mStatusPrevChange)
    {
        mStatChange->setStyleSheet(cstatus.showcontrol ? mOnStyle : mOffStyle) ;
        mStatusPrevChange = cstatus.showcontrol ;
    }

    if (!mStatusPrevInit || cstatus.bold != mStatusPrevBold)
    {
        mStatBold->setStyleSheet(cstatus.bold ? mOnStyle : mOffStyle) ;
        mStatusPrevBold = cstatus.bold ;
    }

    if (!mStatusPrevInit || cstatus.italic != mStatusPrevItalic)
    {
        mStatItalic->setStyleSheet(cstatus.italic ? mOnStyle : mOffStyle) ;
        mStatusPrevItalic = cstatus.italic ;
    }

    if (!mStatusPrevInit || cstatus.underline != mStatusPrevUnderline)
    {
        mStatUnderline->setStyleSheet(cstatus.underline ? mOnStyle : mOffStyle) ;
        mStatusPrevUnderline = cstatus.underline ;
    }

    if (!mStatusPrevInit || cstatus.just != mStatusPrevJust)
    {
        mStatLeft->setStyleSheet(cstatus.just == JUST_LEFT ? mOnStyle : mOffStyle) ;
        mStatCenter->setStyleSheet(cstatus.just == JUST_CENTER ? mOnStyle : mOffStyle) ;
        mStatRight->setStyleSheet(cstatus.just == JUST_RIGHT ? mOnStyle : mOffStyle) ;
        mStatJustify->setStyleSheet(cstatus.just == JUST_JUST ? mOnStyle : mOffStyle) ;
        mStatusPrevJust = cstatus.just ;
    }

    mStatusPrevInit = true ;

//    if(mStatusLastHelp != cstatus.help)
    {
        // Reset the help-display delay whenever help state changes
        if (cstatus.help != mStatusLastHelp)
        {
            mStatusHelpCounter = 0 ;
        }
        mStatusHelpCounter++ ;
        if(cstatus.help == HELP_NONE && mStatusLastHelp != HELP_NONE)
        {
            mHelpCtrl->hide(); ;
            mHelpMCtrl->hide() ;
            mHelpKCtrl->hide() ;
            mHelpPCtrl->hide(); ;
            mHelpQCtrl->hide() ;
            mHelpOCtrl->hide(); ;
            mStatusLastHelp = HELP_NONE ;
        }
        else if(cstatus.help == HELP_MAIN && mStatusLastHelp != HELP_MAIN)
        {
            mHelpCtrl->show() ;
            mHelpMCtrl->hide() ;
            mHelpKCtrl->hide() ;
            mHelpPCtrl->hide() ;
            mHelpQCtrl->hide() ;
            mHelpOCtrl->hide() ;
            mStatusLastHelp = HELP_MAIN ;
        }

        if(cstatus.help == HELP_CTRLM && mStatusLastHelp != HELP_CTRLM)
        {
            mStatusHelpCounter = 0 ;
            mStatusLastHelp = HELP_CTRLM ;
        }

        if(cstatus.help == HELP_CTRLM && mStatusHelpCounter >= HELPDELAY)
        {
            mHelpCtrl->hide() ;
            mHelpMCtrl->show() ;
            mHelpKCtrl->hide() ;
            mHelpPCtrl->hide() ;
            mHelpQCtrl->hide() ;
            mHelpOCtrl->hide() ;
            mStatusLastHelp = HELP_CTRLM ;
        }

        if(cstatus.help == HELP_CTRLK && mStatusLastHelp != HELP_CTRLK)
        {
            mStatusHelpCounter = 0 ;
            mStatusLastHelp = HELP_CTRLK ;
        }

        if(cstatus.help == HELP_CTRLK && mStatusHelpCounter >= HELPDELAY)
        {
            mHelpCtrl->hide() ;
            mHelpMCtrl->hide() ;
            mHelpKCtrl->show() ;
            mHelpPCtrl->hide() ;
            mHelpQCtrl->hide() ;
            mHelpOCtrl->hide() ;
            mStatusLastHelp = HELP_CTRLK ;
        }

        if(cstatus.help == HELP_CTRLP && mStatusLastHelp != HELP_CTRLP)
        {
            mStatusHelpCounter = 0 ;
            mStatusLastHelp = HELP_CTRLP ;
        }

        if(cstatus.help == HELP_CTRLP && mStatusHelpCounter >= HELPDELAY)
        {
            mHelpCtrl->hide() ;
            mHelpMCtrl->hide() ;
            mHelpKCtrl->hide() ;
            mHelpPCtrl->show() ;
            mHelpQCtrl->hide() ;
            mHelpOCtrl->hide() ;
            mStatusLastHelp = HELP_CTRLP ;
        }

        if(cstatus.help == HELP_CTRLQ && mStatusLastHelp != HELP_CTRLQ)
        {
            mStatusHelpCounter = 0 ;
            mStatusLastHelp = HELP_CTRLQ ;
        }

        if(cstatus.help == HELP_CTRLQ && mStatusHelpCounter >= HELPDELAY)
        {
            mHelpCtrl->hide() ;
            mHelpMCtrl->hide() ;
            mHelpKCtrl->hide() ;
            mHelpPCtrl->hide() ;
            mHelpQCtrl->show() ;
            mHelpOCtrl->hide() ;
            mStatusLastHelp = HELP_CTRLQ ;
        }

        if(cstatus.help == HELP_CTRLO && mStatusLastHelp != HELP_CTRLO)
        {
            mStatusHelpCounter = 0 ;
            mStatusLastHelp = HELP_CTRLO ;
        }

        if(cstatus.help == HELP_CTRLO && mStatusHelpCounter >= HELPDELAY)
        {
            mHelpCtrl->hide() ;
            mHelpMCtrl->hide() ;
            mHelpKCtrl->hide() ;
            mHelpPCtrl->hide() ;
            mHelpQCtrl->hide() ;
            mHelpOCtrl->show() ;
            mStatusLastHelp = HELP_CTRLO ;
        }
    }

    // ChangeHelpLevel(0) hides the status line at runtime (not just at
    // startup/Preferences-OK, unlike the other mDisp* flags applied via
    // ApplyDisplaySettings()), so react to it here every tick.
    if(editor->mDispStatusBar != mStatusBottomPrevVisible)
    {
        mStatusBottom->setVisible(editor->mDispStatusBar) ;
        mStatusBottomPrevVisible = editor->mDispStatusBar ;
    }
}


void cWordTsar::ClearStatus(void)
{
    mStatText->setText("") ;
    mStatusTextTimer->stop() ;
}

void cWordTsar::SetStatus(QString text, bool progress, int percent)
{
    mStatusTextTimer->stop() ;

    if(text.length() > 0)
    {
        mStatText->setText(text) ;
    }
    else
    {
        mStatusTextTimer->start(1000) ;
    }

    if(progress == true)
    {
        mBusy->show() ;
        mBusy->Start() ;

//        QApplication::processEvents() ;
        static int oldpercent = 0 ;
        if(oldpercent != percent)
        {
/*
            QRect fr = mStatText->frameRect() ;

            QPixmap pixmap(fr.width(), fr.height()) ;
            QPainter paint(&pixmap) ;

            QColor background = QWidget::palette().color(QWidget::backgroundRole()) ;
            paint.setPen(background) ;
            paint.setBrush(background) ;
            paint.drawRect(fr) ;

            fr.setWidth(fr.width() * ((double)percent / 100.0)) ;

            QColor green(69, 139, 0) ;
            paint.setPen(green) ;
            paint.setBrush(green) ;
            paint.drawRect(fr) ;

            QColor black(0, 0, 0) ;
            paint.setPen(black) ;
            paint.setBrush(black) ;
            paint.drawText(5, fr.height() - 5, text) ;

            mStatText->setPixmap(pixmap) ;
            oldpercent = percent ;
*/
        }
    }
    else
    {
        mBusy->hide() ;
        mBusy->Stop() ;

//        QApplication::processEvents();
    }

}


void cWordTsar::ReadConfig(void)
{
    // Load configuration from XDG-standard path via cConfig
    cConfig config ;
    config.Load() ;

    // Show controls
    if (config.mShowControls)
    {
        mEditor->SetShowControls(SHOW_ALL) ;
    }
    else
    {
        mEditor->SetShowControls(SHOW_NONE) ;
    }

    // Helper: convert sRGB to QColor (opaque)
    auto toQColor = [](sRGB c) { return QColor(c.r, c.g, c.b) ; } ;

    // Helper: convert sRGB to QColor with alpha
    auto toQColorA = [](sRGB c, int a) { return QColor(c.r, c.g, c.b, a) ; } ;

    // Apply GUI colors to main editor
    mEditor->SetBGroundColour(toQColor(config.mGuiBackground)) ;
    mEditor->SetTextColour(toQColor(config.mGuiForeground)) ;
    mEditor->SetHighlightColour(toQColorA(config.mGuiHighlightBackground, 127)) ;
    mEditor->SetDotColour(toQColorA(config.mGuiDotBackground, 190)) ;
    mEditor->SetBlockColour(toQColorA(config.mGuiBlockBackground, 190)) ;
    mEditor->SetCommentColour(toQColorA(config.mGuiCommentBackground, 190)) ;
    mEditor->SetErrorColour(toQColorA(config.mGuiErrorBackground, 190)) ;
    mEditor->SetUnknownColour(toQColorA(config.mGuiUnknownBackground, 190)) ;
    mEditor->SetNotImplementedColour(toQColorA(config.mGuiNotImplementedBackground, 190)) ;
    mEditor->SetSearchColour(toQColorA(config.mGuiSearchBackground, 75)) ;

    // Editor overlay foreground colors (opaque)
    mEditor->SetHighlightFgColour(toQColor(config.mGuiHighlightForeground)) ;
    mEditor->SetDotFgColour(toQColor(config.mGuiDotForeground)) ;
    mEditor->SetCommentFgColour(toQColor(config.mGuiCommentForeground)) ;
    mEditor->SetErrorFgColour(toQColor(config.mGuiErrorForeground)) ;
    mEditor->SetUnknownFgColour(toQColor(config.mGuiUnknownForeground)) ;
    mEditor->SetNotImplementedFgColour(toQColor(config.mGuiNotImplementedForeground)) ;

    // Apply gui.screen colors to help panel labels
    ApplyHelpColors(
        toQColor(config.mGuiHelpPanelBackground),
        toQColor(config.mGuiHelpPanelForeground),
        toQColor(config.mGuiHelpPanelKeystrokeForeground),
        toQColor(config.mGuiHelpPanelKeystrokeBackground)
    ) ;

    // Apply gui.screen colors to ruler
    mRuler->SetBackgroundColour(toQColor(config.mGuiRulerBackground)) ;
    mRuler->SetForegroundColour(toQColor(config.mGuiRulerForeground)) ;

    // Apply gui.screen colors to status bars
    // No type selector so color cascades to child QLabel widgets
    QColor statusBg = toQColor(config.mGuiStatusBarBackground) ;
    QColor statusFg = toQColor(config.mGuiStatusBarForeground) ;
    QString statusStyle = QString("background-color: %1; color: %2;")
        .arg(statusBg.name()).arg(statusFg.name()) ;
    mStatusTop->setStyleSheet(statusStyle) ;
    mStatusBottom->setStyleSheet(statusStyle) ;

    // User info
    mEditor->mShortName = config.mShortName ;
    mEditor->mLongName = config.mLongName ;

    // Window size
    resize(config.mWindowWidth, config.mWindowHeight) ;

    // Help level (WordStar 7 manual's "Change Help Level", 0-4)
    mEditor->mHelpLevel = std::clamp(config.mGuiShowHelp, 0, 4) ;
    mEditor->mHelpDisplay = (mEditor->mHelpLevel == 3) ? HELP_MAIN : HELP_NONE ;

    // Measurement units
    mEditor->SetMeasurement(config.mMeasurement) ;

    // Display flags
    mEditor->mDispRuler = config.mGuiShowRuler ;
    mEditor->mDispScrollBar = config.mGuiShowScrollBar ;
    mEditor->mDispStatusBar = config.mGuiShowStatusBar ;
    mEditor->mDispStyleBar = config.mGuiShowStyleBar ;
    mEditor->mDispMenu = config.mGuiShowMenu ;
    mEditor->mAlwaysDot = config.mGuiAlwaysDotCommands ;
    if (mEditor->GetShowControls() == SHOW_DOT && mEditor->mAlwaysDot == false)
    {
        mEditor->SetShowDot(false) ;
    }
    mEditor->mAlwaysFlag = config.mGuiAlwaysFlagColumn ;

    // Code page
    mEditor->SetCodePage(static_cast<eCodePage>(config.mCodePage)) ;

    // Input mode (keyboard layout)
    eInputMode inputMode = static_cast<eInputMode>(config.mInputMode) ;
    OnInputModeChanged(inputMode) ;

    // Configurable editor settings
    mEditor->mSpellCheckLanguage = config.mSpellCheckLanguage ;
    mEditor->mSpellCheckDotCommands = config.mSpellCheckDotCommands ;
    mEditor->mCaretBlinkRate = config.mCaretBlinkRate ;
    mEditor->mAutoSaveIntervalSec = config.mAutoSaveInterval ;
    mEditor->mDefaultFormat = config.mDefaultFormat ;

    // Default directory
    if (!config.mDefaultDirectory.empty())
    {
        mEditor->mFileDir = config.mDefaultDirectory ;
    }
}


void cWordTsar::WriteConfig(void)
{
    // Load existing config to preserve TUI and other settings
    cConfig config ;
    config.Load() ;

    // Helper: convert QColor to sRGB
    auto fromQColor = [](QColor c) -> sRGB
    {
        return { static_cast<short>(c.red()), static_cast<short>(c.green()), static_cast<short>(c.blue()) } ;
    } ;

    // Show controls
    config.mShowControls = (mEditor->GetShowControls() == SHOW_ALL) ;

    // GUI colors
    config.mGuiBackground = fromQColor(mEditor->GetBGroundColour()) ;
    config.mGuiForeground = fromQColor(mEditor->GetTextColour()) ;
    config.mGuiHighlightBackground = fromQColor(mEditor->GetHighlightColour()) ;
    config.mGuiDotBackground = fromQColor(mEditor->GetDotColour()) ;
    config.mGuiBlockBackground = fromQColor(mEditor->GetBlockColour()) ;
    config.mGuiCommentBackground = fromQColor(mEditor->GetCommentColour()) ;
    config.mGuiErrorBackground = fromQColor(mEditor->GetErrorColour()) ;
    config.mGuiUnknownBackground = fromQColor(mEditor->GetUnknownColour()) ;
    config.mGuiNotImplementedBackground = fromQColor(mEditor->GetNotImplementedColour()) ;

    // User info
    config.mShortName = mEditor->mShortName ;
    config.mLongName = mEditor->mLongName ;

    // Window size
    QSize wsize = size() ;
    config.mWindowWidth = wsize.width() ;
    config.mWindowHeight = wsize.height() ;

    // Help level (WordStar 7 manual's "Change Help Level", 0-4): unlike
    // mHelpDisplay, which also carries momentary chord-submenu state
    // (HELP_CTRLM/K/P/Q/O), mHelpLevel only ever changes via
    // ChangeHelpLevel() (F1 F1 or the Preferences dialog), so it's always
    // safe to persist directly.
    config.mGuiShowHelp = mEditor->mHelpLevel ;

    // Measurement units
    config.mMeasurement = mEditor->GetMeasurement() ;

    // Display flags
    config.mGuiShowRuler = mEditor->mDispRuler ;
    config.mGuiShowScrollBar = mEditor->mDispScrollBar ;
    config.mGuiShowStatusBar = mEditor->mDispStatusBar ;
    config.mGuiShowStyleBar = mEditor->mDispStyleBar ;
    config.mGuiShowMenu = mEditor->mDispMenu ;
    config.mGuiAlwaysDotCommands = mEditor->mAlwaysDot ;
    config.mGuiAlwaysFlagColumn = mEditor->mAlwaysFlag ;

    // Code page
    config.mCodePage = mEditor->GetCodePage() ;

    // Input mode
    config.mInputMode = mEditor->GetInputMode() ;

    // Configurable editor settings
    config.mSpellCheckLanguage = mEditor->mSpellCheckLanguage ;
    config.mCaretBlinkRate = mEditor->mCaretBlinkRate ;
    config.mAutoSaveInterval = mEditor->mAutoSaveIntervalSec ;
    config.mDefaultFormat = mEditor->mDefaultFormat ;

    // Default directory
    config.mDefaultDirectory = mEditor->mFileDir ;

    config.Save() ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  mode [in] the new input mode (INPUT_WORDSTAR or INPUT_MODERN)
///
/// @return nothing
///
/// @brief
/// Switch the keyboard input mode, menu provider, and rebuild menus.
/// Called from ReadConfig at startup and from preferences dialog when
/// the keyboard mode combo box changes.
///
/////////////////////////////////////////////////////////////////////////////
void cWordTsar::OnInputModeChanged(eInputMode mode)
{
    // Switch the input handler on the editor
    mEditor->SetInputMode(mode) ;

    // Switch the menu provider
    delete mMenuProvider ;
    mMenuProvider = nullptr ;

    switch (mode)
    {
        case INPUT_MODERN:
        {
            mMenuProvider = new cModernMenuProvider() ;
            break ;
        }
        default:
        {
            mMenuProvider = new cWordStarMenuProvider() ;
            break ;
        }
    }

    // Rebuild all menus with the new provider (CreateMenus clears+reuses mMenuBar)
    CreateMenus() ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Apply the Display On Screen preferences to the actual Qt widgets.
/// Reads the mDisp* flags from mEditor and shows/hides each widget.
/// Called after ReadConfig() at startup and after Preferences dialog OK.
///
/////////////////////////////////////////////////////////////////////////////
void cWordTsar::ApplyDisplaySettings(void)
{
    mMenuBar->setVisible(mEditor->mDispMenu) ;
    mRuler->setVisible(mEditor->mDispRuler) ;
    mStatusTop->setVisible(mEditor->mDispStyleBar) ;
    mStatusBottom->setVisible(mEditor->mDispStatusBar) ;
    mScrollbar->setVisible(mEditor->mDispScrollBar) ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  helpBg [in] help panel background color
/// @param  helpFg [in] help panel foreground (text) color
/// @param  keyColor [in] keystroke highlight color
///
/// @return nothing
///
/// @brief
/// Apply colors to all 6 help panel labels. Uses the original help text
/// strings (with plain <b> tags) so this works correctly on repeated calls.
///
/////////////////////////////////////////////////////////////////////////////
void cWordTsar::ApplyHelpColors(const QColor& helpBg, const QColor& helpFg, const QColor& keyColor, const QColor& keyBgColor)
{
    QString helpStyle = QString("QLabel { background-color: %1; color: %2; }")
        .arg(helpBg.name()).arg(helpFg.name()) ;
    QString keyTag = QString("<b style=\"color:%1; background-color:%2\">")
        .arg(keyColor.name()).arg(keyBgColor.name()) ;

    // Pair each label with its original text
    struct HelpPanel
    {
        QLabel* label ;
        const QString* original ;
    };

    HelpPanel panels[] = {
        { mHelpCtrl,  &mHelpTextMain },
        { mHelpMCtrl, &mHelpTextM },
        { mHelpKCtrl, &mHelpTextK },
        { mHelpOCtrl, &mHelpTextO },
        { mHelpPCtrl, &mHelpTextP },
        { mHelpQCtrl, &mHelpTextQ },
    };

    for (const auto& panel : panels)
    {
        // Start from original text (plain <b> tags) and apply keystroke color
        QString text = *panel.original ;
        text.replace("<b>", keyTag) ;
        panel.label->setText(text) ;
        panel.label->setStyleSheet(helpStyle) ;
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Re-read screen colors from config and apply to help panels, ruler,
/// and status bars. Called after SystemPreferences dialog changes colors.
///
/////////////////////////////////////////////////////////////////////////////
void cWordTsar::ApplyScreenColors(void)
{
    cConfig config ;
    config.Load() ;

    auto toQColor = [](sRGB c) { return QColor(c.r, c.g, c.b) ; } ;

    // Help panel colors
    ApplyHelpColors(
        toQColor(config.mGuiHelpPanelBackground),
        toQColor(config.mGuiHelpPanelForeground),
        toQColor(config.mGuiHelpPanelKeystrokeForeground),
        toQColor(config.mGuiHelpPanelKeystrokeBackground)
    ) ;

    // Ruler colors
    mRuler->SetBackgroundColour(toQColor(config.mGuiRulerBackground)) ;
    mRuler->SetForegroundColour(toQColor(config.mGuiRulerForeground)) ;

    // Status bar colors
    // No type selector so color cascades to child QLabel widgets
    QColor statusBg = toQColor(config.mGuiStatusBarBackground) ;
    QColor statusFg = toQColor(config.mGuiStatusBarForeground) ;
    QString statusStyle = QString("background-color: %1; color: %2;")
        .arg(statusBg.name()).arg(statusFg.name()) ;
    mStatusTop->setStyleSheet(statusStyle) ;
    mStatusBottom->setStyleSheet(statusStyle) ;

    // Active indicator style (help panel keystroke colors, bold, no italic)
    // QLabel selector overrides the parent QStatusBar cascade
    QColor keyFg = toQColor(config.mGuiHelpPanelKeystrokeForeground) ;
    QColor keyBg = toQColor(config.mGuiHelpPanelKeystrokeBackground) ;
    mOnStyle = QString("QLabel { background-color: %1; color: %2; font-weight: bold; font-style: normal; }")
        .arg(keyBg.name()).arg(keyFg.name()) ;

    // Inactive indicator style (normal status bar colors, italic, not bold)
    mOffStyle = QString("QLabel { background-color: %1; color: %2; font-weight: normal; font-style: italic; }")
        .arg(statusBg.name()).arg(statusFg.name()) ;
}


void cWordTsar::CreateMenus(void)
{
    if (mMenuBar == nullptr)
    {
        mMenuBar = new QMenuBar() ;
    }
    else
    {
        // mMenuBar->clear() alone only removes action references from menu
        // display lists; the QMenu and QAction objects (all parented to
        // mMenuBar) stay alive with their slot connections intact, leaking
        // handles and causing each menu action to fire N+1 times after N
        // input-mode switches.
        //
        // Keep the SAME mMenuBar instance because the constructor added it
        // to mLayout exactly once (wordtsar.cpp:140); deleting it would
        // leave us with an unparented replacement that Qt shows as its own
        // top-level window.
        disconnect(mMenuBar, &QMenuBar::triggered, this, nullptr) ;
        qDeleteAll(mMenuBar->findChildren<QMenu*>(QString(), Qt::FindDirectChildrenOnly)) ;
        qDeleteAll(mMenuBar->findChildren<QAction*>(QString(), Qt::FindDirectChildrenOnly)) ;
    }
    // Height constraint moved to showEvent() for correct DPI handling on Windows
    // (QFontMetrics doesn't have correct DPI info until window is shown)

    QMenu *filemenu = mMenuBar->addMenu("&File") ;
    QMenu *editmenu = mMenuBar->addMenu("&Edit") ;
    QMenu *viewmenu = mMenuBar->addMenu("&View") ;
    QMenu *insertmenu = mMenuBar->addMenu("&Insert") ;
    QMenu *stylemenu = mMenuBar->addMenu("&Style") ;
    QMenu *layoutmenu = mMenuBar->addMenu("&Layout") ;
    QMenu *utilitiesmenu = mMenuBar->addMenu("&Utilities") ;
    QMenu *helpmenu = mMenuBar->addMenu("&Help") ;

    // removed (commented) all SetSHortcuts, as Qt steals the keypresses, and command entry no longer works from the keyboard.

    // file menu
    QAction *OpenAction = new QAction(mMenuProvider->GetFileOpenLabel(), mMenuBar) ;
    QAction *SaveAction = new QAction(mMenuProvider->GetFileSaveLabel(), mMenuBar) ;
    QAction *SaveAsAction = new QAction(mMenuProvider->GetFileSaveAsLabel(), mMenuBar) ;
    QAction *SaveandCloseAction = new QAction(mMenuProvider->GetFileSaveAndCloseLabel(), mMenuBar) ;
    QAction *PrintAction = new QAction(mMenuProvider->GetFilePrintLabel(), mMenuBar) ;
    QAction *PrintPreviewAction = new QAction(mMenuProvider->GetFilePrintPreviewLabel(), mMenuBar) ;
    QAction *PreferencesAction = new QAction("Pr&eferences...\t⌘,", mMenuBar) ;
    QAction *ExitAction = new QAction(mMenuProvider->GetFileExitLabel(), mMenuBar) ;

    filemenu->addAction(OpenAction) ;
    filemenu->addAction(SaveAction) ;
    filemenu->addAction(SaveAsAction) ;
    filemenu->addAction(SaveandCloseAction) ;
    filemenu->addSeparator() ;
    mRecentFilesMenu = filemenu->addMenu("Recent &Files") ;
    UpdateRecentFilesMenu() ;
    filemenu->addSeparator() ;
    filemenu->addAction(PrintAction) ;
    filemenu->addAction(PrintPreviewAction) ;
    filemenu->addSeparator() ;
    filemenu->addAction(PreferencesAction) ;
    filemenu->addSeparator() ;
    filemenu->addAction(ExitAction) ;

    // connect things up
    connect(OpenAction, &QAction::triggered, this, &cWordTsar::Open) ;
    connect(SaveAction, &QAction::triggered, this, &cWordTsar::Save) ;
    connect(SaveAsAction, &QAction::triggered, this, &cWordTsar::SaveAs) ;
    connect(SaveandCloseAction, &QAction::triggered, this, &cWordTsar::SaveandClose) ;
    connect(PrintAction, &QAction::triggered, this, &cWordTsar::PrintPreview) ;
    connect(PrintPreviewAction, &QAction::triggered, this, &cWordTsar::PrintPreview) ;
    connect(PreferencesAction, &QAction::triggered, this, [this]()
    {
        mEditor->SystemPreferences() ;
    }) ;
    connect(ExitAction, &QAction::triggered, this, &cWordTsar::ExitWordTsar) ;

    //edit menu
    QAction *UndoAction = new QAction(mMenuProvider->GetEditUndoLabel(), mMenuBar) ;
    QAction *RedoAction = new QAction(mMenuProvider->GetEditRedoLabel(), mMenuBar) ;
    QAction *MarkBlockStartAction = new QAction(mMenuProvider->GetEditMarkBlockStartLabel(), mMenuBar) ;
    QAction *MarkBlockEndAction = new QAction(mMenuProvider->GetEditMarkBlockEndLabel(), mMenuBar) ;

    QAction *MoveBlock = new QAction(mMenuProvider->GetEditMoveBlockLabel(), mMenuBar) ;
//    QAction *MoveBlockWindow = new QAction("&Block from Other Window", mMenuBar) ;

    QAction *CopyBlockAction = new QAction(mMenuProvider->GetEditCopyBlockLabel(), mMenuBar) ;
//    QAction *CopyBlockWindowAction = new QAction("Mark Block &Previous", mMenuBar) ;
    QAction *CopyfromClipboardAction = new QAction(mMenuProvider->GetEditCopyFromClipboardLabel(), mMenuBar) ;
    QAction *CopytoClipboardAction = new QAction(mMenuProvider->GetEditCopyToClipboardLabel(), mMenuBar) ;
//    QAction *CopytoOtherFileAction = new QAction("Mark Block &Previous", mMenuBar) ;

    QAction *DeleteBlockAction = new QAction(mMenuProvider->GetEditDeleteBlockLabel(), mMenuBar) ;
    QAction *DeleteWordAction = new QAction(mMenuProvider->GetEditDeleteWordLabel(), mMenuBar) ;
    QAction *DeleteLineAction = new QAction(mMenuProvider->GetEditDeleteLineLabel(), mMenuBar) ;
    QAction *DeleteLineLeftAction = new QAction(mMenuProvider->GetEditDeleteLineLeftLabel(), mMenuBar) ;
    QAction *DeleteLineRightAction = new QAction(mMenuProvider->GetEditDeleteLineRightLabel(), mMenuBar) ;
    QAction *DeleteToCharAction = new QAction(mMenuProvider->GetEditDeleteToCharLabel(), mMenuBar) ;

    QAction *MarkPreviousBlockAction = new QAction(mMenuProvider->GetEditMarkPreviousBlockLabel(), mMenuBar) ;

    QAction *FindAction = new QAction(mMenuProvider->GetEditFindLabel(), mMenuBar) ;
    QAction *FindandReplaceAction = new QAction(mMenuProvider->GetEditFindAndReplaceLabel(), mMenuBar) ;
    QAction *FindNextAction = new QAction(mMenuProvider->GetEditFindNextLabel(), mMenuBar) ;
    QAction *GotoCharAction = new QAction(mMenuProvider->GetEditGotoCharLabel(), mMenuBar) ;
    QAction *GotoPageAction = new QAction(mMenuProvider->GetEditGotoPageLabel(), mMenuBar) ;
    QAction *GotoMarker1Action = new QAction(mMenuProvider->GetEditGotoMarker1Label(), mMenuBar) ;
    QAction *GotoMarker2Action = new QAction(mMenuProvider->GetEditGotoMarker2Label(), mMenuBar) ;
    QAction *GotoMarker3Action = new QAction(mMenuProvider->GetEditGotoMarker3Label(), mMenuBar) ;
    QAction *GotoMarker4Action = new QAction(mMenuProvider->GetEditGotoMarker4Label(), mMenuBar) ;
    QAction *GotoMarker5Action = new QAction(mMenuProvider->GetEditGotoMarker5Label(), mMenuBar) ;
    QAction *GotoMarker6Action = new QAction(mMenuProvider->GetEditGotoMarker6Label(), mMenuBar) ;
    QAction *GotoMarker7Action = new QAction(mMenuProvider->GetEditGotoMarker7Label(), mMenuBar) ;
    QAction *GotoMarker8Action = new QAction(mMenuProvider->GetEditGotoMarker8Label(), mMenuBar) ;
    QAction *GotoMarker9Action = new QAction(mMenuProvider->GetEditGotoMarker9Label(), mMenuBar) ;
    QAction *GotoMarker0Action = new QAction(mMenuProvider->GetEditGotoMarker0Label(), mMenuBar) ;

    QAction *GotoFontTagAction = new QAction(mMenuProvider->GetEditGotoFontTagLabel(), mMenuBar) ;
    QAction *GotoStyleTagAction = new QAction(mMenuProvider->GetEditGotoStyleTagLabel(), mMenuBar) ;
GotoStyleTagAction->setEnabled(false) ;
    QAction *GotoNoteAction = new QAction(mMenuProvider->GetEditGotoNoteLabel(), mMenuBar) ;
GotoNoteAction->setEnabled(false) ;
    QAction *GotoPreviousPositionAction = new QAction(mMenuProvider->GetEditGotoPreviousPositionLabel(), mMenuBar) ;
    QAction *GotoLastFindReplaceAction = new QAction(mMenuProvider->GetEditGotoLastFindReplaceLabel(), mMenuBar) ;
    QAction *GotoBeginningofBlockAction = new QAction(mMenuProvider->GetEditGotoBeginningOfBlockLabel(), mMenuBar) ;
    QAction *GotoEndofBlockAction = new QAction(mMenuProvider->GetEditGotoEndOfBlockLabel(), mMenuBar) ;
    QAction *GotoDocumentStartAction = new QAction(mMenuProvider->GetEditGotoDocumentStartLabel(), mMenuBar) ;
    QAction *GotoDocumentEndAction = new QAction(mMenuProvider->GetEditGotoDocumentEndLabel(), mMenuBar) ;
    QAction *GotoScrollUpAction = new QAction(mMenuProvider->GetEditGotoScrollUpLabel(), mMenuBar) ;
GotoScrollUpAction->setEnabled(false) ;
    QAction *GotoScrollDownAction = new QAction(mMenuProvider->GetEditGotoScrollDownLabel(), mMenuBar) ;
GotoScrollDownAction->setEnabled(false) ;

    QAction *SetMarker1Action = new QAction(mMenuProvider->GetEditSetMarker1Label(), mMenuBar) ;
    QAction *SetMarker2Action = new QAction(mMenuProvider->GetEditSetMarker2Label(), mMenuBar) ;
    QAction *SetMarker3Action = new QAction(mMenuProvider->GetEditSetMarker3Label(), mMenuBar) ;
    QAction *SetMarker4Action = new QAction(mMenuProvider->GetEditSetMarker4Label(), mMenuBar) ;
    QAction *SetMarker5Action = new QAction(mMenuProvider->GetEditSetMarker5Label(), mMenuBar) ;
    QAction *SetMarker6Action = new QAction(mMenuProvider->GetEditSetMarker6Label(), mMenuBar) ;
    QAction *SetMarker7Action = new QAction(mMenuProvider->GetEditSetMarker7Label(), mMenuBar) ;
    QAction *SetMarker8Action = new QAction(mMenuProvider->GetEditSetMarker8Label(), mMenuBar) ;
    QAction *SetMarker9Action = new QAction(mMenuProvider->GetEditSetMarker9Label(), mMenuBar) ;
    QAction *SetMarker0Action = new QAction(mMenuProvider->GetEditSetMarker0Label(), mMenuBar) ;

    QAction *EditNoteAction = new QAction(mMenuProvider->GetEditEditNoteLabel(), mMenuBar) ;
EditNoteAction->setEnabled(false) ;

    QAction *NoteStartNumberAction = new QAction(mMenuProvider->GetEditNoteStartNumberLabel(), mMenuBar) ;
NoteStartNumberAction->setEnabled(false) ;
    QAction *NoteConvertAction = new QAction(mMenuProvider->GetEditNoteConvertLabel(), mMenuBar) ;
NoteConvertAction->setEnabled(false) ;
    QAction *NoteCovertPrintAction = new QAction(mMenuProvider->GetEditNoteConvertPrintLabel(), mMenuBar) ;
NoteCovertPrintAction->setEnabled(false) ;
    QAction *NotEndnoteLocationAction = new QAction(mMenuProvider->GetEditNoteEndnoteLocationLabel(), mMenuBar) ;
NotEndnoteLocationAction->setEnabled(false) ;

    QAction *ColumnBlockModeAction = new QAction(mMenuProvider->GetEditColumnBlockModeLabel(), mMenuBar) ;
ColumnBlockModeAction->setEnabled(false) ;
    QAction *ColumnReplaceModeAction = new QAction(mMenuProvider->GetEditColumnReplaceModeLabel(), mMenuBar) ;
ColumnReplaceModeAction->setEnabled(false) ;
    QAction *AutoAlineAction = new QAction(mMenuProvider->GetEditAutoAlignLabel(), mMenuBar) ;
AutoAlineAction->setEnabled(false) ;
    QAction *CloseDialogAction = new QAction(mMenuProvider->GetEditCloseDialogLabel(), mMenuBar) ;
CloseDialogAction->setEnabled(false) ;

    editmenu->addAction(UndoAction) ;
    editmenu->addAction(RedoAction) ;
    editmenu->addSeparator() ;
    editmenu->addAction(MarkBlockStartAction) ;
    editmenu->addAction(MarkBlockEndAction) ;

    QMenu *MoveMenu = editmenu->addMenu("Mo&ve") ;
    MoveMenu->addAction(MoveBlock) ;

    QMenu *CopyMenu = editmenu->addMenu("&Copy") ;
    CopyMenu->addAction(CopyBlockAction) ;
    CopyMenu->addAction(CopyfromClipboardAction) ;
    CopyMenu->addAction(CopytoClipboardAction) ;

    QMenu *DeleteMenu = editmenu->addMenu("&Delete") ;
    DeleteMenu->addAction(DeleteBlockAction) ;
    DeleteMenu->addAction(DeleteWordAction) ;
    DeleteMenu->addAction(DeleteLineAction) ;
    DeleteMenu->addAction(DeleteLineLeftAction) ;
    DeleteMenu->addAction(DeleteLineRightAction) ;
    DeleteMenu->addAction(DeleteToCharAction) ;

    editmenu->addAction(MarkPreviousBlockAction) ;
    editmenu->addSeparator() ;
    editmenu->addAction(FindAction) ;
    editmenu->addAction(FindandReplaceAction) ;
    editmenu->addAction(FindNextAction) ;
    editmenu->addAction(GotoCharAction) ;
    editmenu->addAction(GotoPageAction) ;

    QMenu *GotoMarkerMenu = editmenu->addMenu("Go to &Marker") ;
    GotoMarkerMenu->addAction(GotoMarker1Action) ;
    GotoMarkerMenu->addAction(GotoMarker2Action) ;
    GotoMarkerMenu->addAction(GotoMarker3Action) ;
    GotoMarkerMenu->addAction(GotoMarker4Action) ;
    GotoMarkerMenu->addAction(GotoMarker5Action) ;
    GotoMarkerMenu->addAction(GotoMarker6Action) ;
    GotoMarkerMenu->addAction(GotoMarker7Action) ;
    GotoMarkerMenu->addAction(GotoMarker8Action) ;
    GotoMarkerMenu->addAction(GotoMarker9Action) ;
    GotoMarkerMenu->addAction(GotoMarker0Action) ;

    QMenu *GotoOtherMenu = editmenu->addMenu("Go to &Other") ;
    GotoOtherMenu->addAction(GotoFontTagAction) ;
    GotoOtherMenu->addAction(GotoStyleTagAction) ;
    GotoOtherMenu->addAction(GotoNoteAction) ;
    GotoOtherMenu->addAction(GotoPreviousPositionAction) ;
    GotoOtherMenu->addAction(GotoLastFindReplaceAction) ;
    GotoOtherMenu->addAction(GotoBeginningofBlockAction) ;
    GotoOtherMenu->addAction(GotoEndofBlockAction) ;
    GotoOtherMenu->addAction(GotoDocumentStartAction) ;
    GotoOtherMenu->addAction(GotoDocumentEndAction) ;
    GotoOtherMenu->addAction(GotoScrollUpAction) ;
    GotoOtherMenu->addAction(GotoScrollDownAction) ;

    QMenu *SetMarkerMenu = editmenu->addMenu("&Set Marker") ;
    SetMarkerMenu->addAction(SetMarker1Action) ;
    SetMarkerMenu->addAction(SetMarker2Action) ;
    SetMarkerMenu->addAction(SetMarker3Action) ;
    SetMarkerMenu->addAction(SetMarker4Action) ;
    SetMarkerMenu->addAction(SetMarker5Action) ;
    SetMarkerMenu->addAction(SetMarker6Action) ;
    SetMarkerMenu->addAction(SetMarker7Action) ;
    SetMarkerMenu->addAction(SetMarker8Action) ;
    SetMarkerMenu->addAction(SetMarker9Action) ;
    SetMarkerMenu->addAction(SetMarker0Action) ;

    editmenu->addAction(EditNoteAction) ;

    QMenu *NoteOptionsMenu = editmenu->addMenu("No&te Options") ;
    NoteOptionsMenu->addAction(NoteStartNumberAction) ;
    NoteOptionsMenu->addAction(NoteConvertAction) ;
    NoteOptionsMenu->addAction(NoteCovertPrintAction) ;
    NoteOptionsMenu->addAction(NotEndnoteLocationAction) ;

    QMenu *EditSettingsMenu = editmenu->addMenu("Ed&iting Settings") ;
    EditSettingsMenu->addAction(ColumnBlockModeAction) ;
    EditSettingsMenu->addAction(ColumnReplaceModeAction) ;
    EditSettingsMenu->addAction(AutoAlineAction) ;
    EditSettingsMenu->addAction(CloseDialogAction) ;

    // connect things up
    connect(UndoAction, &QAction::triggered, this, &cWordTsar::Undo) ;
    connect(RedoAction, &QAction::triggered, this, &cWordTsar::Redo) ;
    connect(MarkBlockStartAction, &QAction::triggered, this, &cWordTsar::MarkBlockStart) ;
    connect(MarkBlockEndAction, &QAction::triggered, this, &cWordTsar::MarkBlockEnd) ;
    connect(MoveBlock, &QAction::triggered, this, &cWordTsar::MoveBlock) ;
    connect(CopyBlockAction, &QAction::triggered, this, &cWordTsar::CopyBlock) ;
    connect(CopyfromClipboardAction, &QAction::triggered, this, &cWordTsar::CopyFromClipboard) ;
    connect(CopytoClipboardAction, &QAction::triggered, this, &cWordTsar::CopyToClipboard) ;
    connect(DeleteBlockAction, &QAction::triggered, this, &cWordTsar::DeleteBlock) ;
    connect(DeleteWordAction, &QAction::triggered, this, &cWordTsar::DeleteWord) ;
    connect(DeleteLineAction, &QAction::triggered, this, &cWordTsar::DeleteLine) ;
    connect(DeleteLineLeftAction, &QAction::triggered, this, &cWordTsar::DeleteLineLeft) ;
    connect(DeleteLineRightAction, &QAction::triggered, this, &cWordTsar::DeleteLineRight) ;
    connect(DeleteToCharAction, &QAction::triggered, this, &cWordTsar::DeleteToChar) ;
    connect(MarkPreviousBlockAction, &QAction::triggered, this, &cWordTsar::MarkPrevBlock) ;
    connect(FindAction, &QAction::triggered, this, &cWordTsar::Find) ;
    connect(FindandReplaceAction, &QAction::triggered, this, &cWordTsar::FindandReplace) ;
    connect(FindNextAction, &QAction::triggered, this, &cWordTsar::FindNext) ;
    connect(GotoCharAction, &QAction::triggered, this, &cWordTsar::GotoChar) ;
    connect(GotoPageAction, &QAction::triggered, this, &cWordTsar::GotoPage) ;
    connect(GotoMarker1Action, &QAction::triggered, this, &cWordTsar::Goto1) ;
    connect(GotoMarker2Action, &QAction::triggered, this, &cWordTsar::Goto2) ;
    connect(GotoMarker3Action, &QAction::triggered, this, &cWordTsar::Goto3) ;
    connect(GotoMarker4Action, &QAction::triggered, this, &cWordTsar::Goto4) ;
    connect(GotoMarker5Action, &QAction::triggered, this, &cWordTsar::Goto5) ;
    connect(GotoMarker6Action, &QAction::triggered, this, &cWordTsar::Goto6) ;
    connect(GotoMarker7Action, &QAction::triggered, this, &cWordTsar::Goto7) ;
    connect(GotoMarker8Action, &QAction::triggered, this, &cWordTsar::Goto8) ;
    connect(GotoMarker9Action, &QAction::triggered, this, &cWordTsar::Goto9) ;
    connect(GotoMarker0Action, &QAction::triggered, this, &cWordTsar::Goto0) ;
    connect(GotoFontTagAction, &QAction::triggered, this, &cWordTsar::GotoFont) ;
    connect(GotoStyleTagAction, &QAction::triggered, this, &cWordTsar::GotoStyle) ;
    connect(GotoNoteAction, &QAction::triggered, this, &cWordTsar::GotoNote) ;
    connect(GotoPreviousPositionAction, &QAction::triggered, this, &cWordTsar::GotoPrevPos) ;
    connect(GotoLastFindReplaceAction, &QAction::triggered, this, &cWordTsar::GotoLastFindandReplace) ;
    connect(GotoBeginningofBlockAction, &QAction::triggered, this, &cWordTsar::GotoStartBlock) ;
    connect(GotoEndofBlockAction, &QAction::triggered, this, &cWordTsar::GotoEndBlock) ;
    connect(GotoDocumentStartAction, &QAction::triggered, this, &cWordTsar::GotoDocumentStart) ;
    connect(GotoDocumentEndAction, &QAction::triggered, this, &cWordTsar::GotoDocumentEnd) ;
    connect(GotoScrollUpAction, &QAction::triggered, this, &cWordTsar::GotoScrollUp) ;
    connect(GotoScrollDownAction, &QAction::triggered, this, &cWordTsar::GotoScrollDown) ;
    connect(SetMarker1Action, &QAction::triggered, this, &cWordTsar::Set1) ;
    connect(SetMarker2Action, &QAction::triggered, this, &cWordTsar::Set2) ;
    connect(SetMarker3Action, &QAction::triggered, this, &cWordTsar::Set3) ;
    connect(SetMarker4Action, &QAction::triggered, this, &cWordTsar::Set4) ;
    connect(SetMarker5Action, &QAction::triggered, this, &cWordTsar::Set5) ;
    connect(SetMarker6Action, &QAction::triggered, this, &cWordTsar::Set6) ;
    connect(SetMarker7Action, &QAction::triggered, this, &cWordTsar::Set7) ;
    connect(SetMarker8Action, &QAction::triggered, this, &cWordTsar::Set8) ;
    connect(SetMarker9Action, &QAction::triggered, this, &cWordTsar::Set9) ;
    connect(SetMarker0Action, &QAction::triggered, this, &cWordTsar::Set0) ;
    connect(EditNoteAction, &QAction::triggered, this, &cWordTsar::EditNote) ;
    connect(NoteStartNumberAction, &QAction::triggered, this, &cWordTsar::NoteStartNumber) ;
    connect(NoteConvertAction, &QAction::triggered, this, &cWordTsar::NoteCOnvert) ;
    connect(NoteCovertPrintAction, &QAction::triggered, this, &cWordTsar::NoteConcertForPrint) ;
    connect(NotEndnoteLocationAction, &QAction::triggered, this, &cWordTsar::NoteEndNoteLocation) ;
    connect(ColumnBlockModeAction, &QAction::triggered, this, &cWordTsar::ColumnBlockMode) ;
    connect(ColumnReplaceModeAction, &QAction::triggered, this, &cWordTsar::ColumnReplaceMode) ;
    connect(AutoAlineAction, &QAction::triggered, this, &cWordTsar::AutoAlign) ;
    connect(CloseDialogAction, &QAction::triggered, this, &cWordTsar::CloseDialog) ;

    // View menu
    QAction *PreviewAction = new QAction(mMenuProvider->GetViewPreviewLabel(), mMenuBar) ;
    mCommandTagsAction = new QAction(mMenuProvider->GetViewCommandTagsLabel(), mMenuBar) ;
    QAction *BlockHighlightingAction = new QAction(mMenuProvider->GetViewBlockHighlightingLabel(), mMenuBar) ;
    QAction *ScreenSettingsAction = new QAction(mMenuProvider->GetViewScreenSettingsLabel(), mMenuBar) ;

    viewmenu->addAction(PreviewAction) ;
    viewmenu->addSeparator() ;
    viewmenu->addAction(mCommandTagsAction) ;
    viewmenu->addAction(BlockHighlightingAction) ;
    viewmenu->addSeparator() ;
    viewmenu->addAction(ScreenSettingsAction) ;

    QAction *SwitchModesAction = new QAction(mMenuProvider->GetViewSwitchModesLabel(), mMenuBar) ;
    viewmenu->addSeparator() ;
    viewmenu->addAction(SwitchModesAction) ;

    connect(PreviewAction, &QAction::triggered, this, &cWordTsar::PrintPreview) ;
    connect(mCommandTagsAction, &QAction::triggered, this, &cWordTsar::CommandTags) ;
    connect(BlockHighlightingAction, &QAction::triggered, this, &cWordTsar::BlockHighlight) ;
    connect(ScreenSettingsAction, &QAction::triggered, this, &cWordTsar::ScreenSettings) ;
    connect(SwitchModesAction, &QAction::triggered, this, &cWordTsar::SwitchModes) ;

    // Menu label updated at mode-change time via OnAfterDisplayModeChange calls UpdateCommandTagsLabel()

    // Insert menu
    QAction *PageBreakAction = new QAction(mMenuProvider->GetInsertPageBreakLabel(), mMenuBar) ;
    QAction *ColumnBreakAction = new QAction(mMenuProvider->GetInsertColumnBreakLabel(), mMenuBar) ;
ColumnBreakAction->setEnabled(false) ;
    QAction *DateAction = new QAction(mMenuProvider->GetInsertDateLabel(), mMenuBar) ;

    QAction *TimeAction = new QAction(mMenuProvider->GetInsertTimeLabel(), mMenuBar) ;
    QAction *ResultAction = new QAction(mMenuProvider->GetInsertMathResultLabel(), mMenuBar) ;
ResultAction->setEnabled(false) ;
    QAction *ExpressionAction = new QAction(mMenuProvider->GetInsertMathExpressionLabel(), mMenuBar) ;
ExpressionAction->setEnabled(false) ;
    QAction *DollarAction = new QAction(mMenuProvider->GetInsertMathDollarLabel(), mMenuBar) ;
DollarAction->setEnabled(false) ;
    QAction *FilenameAction = new QAction(mMenuProvider->GetInsertFilenameLabel(), mMenuBar) ;
    QAction *DriveAction = new QAction(mMenuProvider->GetInsertDriveLabel(), mMenuBar) ;
    QAction *DirectoryAction = new QAction(mMenuProvider->GetInsertDirectoryLabel(), mMenuBar) ;
    QAction *PathAction = new QAction(mMenuProvider->GetInsertPathLabel(), mMenuBar) ;

    QAction *VarDateAction = new QAction(mMenuProvider->GetInsertVarDateLabel(), mMenuBar) ;
    QAction *VarTimeAction = new QAction(mMenuProvider->GetInsertVarTimeLabel(), mMenuBar) ;
    QAction *VarPageAction = new QAction(mMenuProvider->GetInsertVarPageLabel(), mMenuBar) ;
    QAction *VarLineAction = new QAction(mMenuProvider->GetInsertVarLineLabel(), mMenuBar) ;
    QAction *VarFileAction = new QAction(mMenuProvider->GetInsertVarFilenameLabel(), mMenuBar) ;
    QAction *VarDriveAction = new QAction(mMenuProvider->GetInsertVarDriveLabel(), mMenuBar) ;
    QAction *VarDirAction = new QAction(mMenuProvider->GetInsertVarDirectoryLabel(), mMenuBar) ;
    QAction *VarPathAction = new QAction(mMenuProvider->GetInsertVarPathLabel(), mMenuBar) ;
    QAction *VarWordCountAction = new QAction(mMenuProvider->GetInsertVarWordCountLabel(), mMenuBar) ;

    QAction *ExtendedCharAction = new QAction(mMenuProvider->GetInsertExtendedCharLabel(), mMenuBar) ;
ExtendedCharAction->setEnabled(false) ;

    QAction *FileAction = new QAction(mMenuProvider->GetInsertFileLabel(), mMenuBar) ;
    QAction *FileAtPrintAction = new QAction(mMenuProvider->GetInsertFileAtPrintLabel(), mMenuBar) ;
FileAtPrintAction->setEnabled(false) ;
    QAction *GraphicAction = new QAction(mMenuProvider->GetInsertGraphicLabel(), mMenuBar) ;
GraphicAction->setEnabled(false) ;

    QAction *NoteCommentAction = new QAction(mMenuProvider->GetInsertNoteCommentLabel(), mMenuBar) ;
NoteCommentAction->setEnabled(false) ;
    QAction *NoteFootnoteAction = new QAction(mMenuProvider->GetInsertNoteFootnoteLabel(), mMenuBar) ;
NoteFootnoteAction->setEnabled(false) ;
    QAction *NoteEndnoteAction = new QAction(mMenuProvider->GetInsertNoteEndnoteLabel(), mMenuBar) ;
NoteEndnoteAction->setEnabled(false) ;
    QAction *NoteAnnotationAction = new QAction(mMenuProvider->GetInsertNoteAnnotationLabel(), mMenuBar) ;
NoteAnnotationAction->setEnabled(false) ;

    QAction *TOCEntryAction = new QAction(mMenuProvider->GetInsertTOCEntryLabel(), mMenuBar) ;
    QAction *IndexEntryAction = new QAction(mMenuProvider->GetInsertIndexEntryLabel(), mMenuBar) ;
    QAction *MarkTextforIndexAction = new QAction(mMenuProvider->GetInsertMarkTextForIndexLabel(), mMenuBar) ;
MarkTextforIndexAction->setEnabled(false) ;
    QAction *DotLeaderAction = new QAction(mMenuProvider->GetInsertDotLeaderLabel(), mMenuBar) ;
DotLeaderAction->setEnabled(false) ;

    QAction *ParOutlineNumberAction = new QAction(mMenuProvider->GetInsertParOutlineNumberLabel(), mMenuBar) ;
ParOutlineNumberAction->setEnabled(false) ;

//    QAction *ChangePrinterCodesAction = new QAction("C&hange Printer Codes...", mMenuBar) ;


    insertmenu->addAction(PageBreakAction) ;
    insertmenu->addAction(ColumnBreakAction) ;
    insertmenu->addSeparator() ;
    insertmenu->addAction(DateAction) ;

    QMenu *OtherValueMenu = insertmenu->addMenu("Other V&alue") ;
    OtherValueMenu->addAction(TimeAction) ;
    OtherValueMenu->addAction(ResultAction) ;
    OtherValueMenu->addAction(ExpressionAction) ;
    OtherValueMenu->addAction(DollarAction) ;
    OtherValueMenu->addAction(FilenameAction) ;
    OtherValueMenu->addAction(DriveAction) ;
    OtherValueMenu->addAction(DirectoryAction) ;
    OtherValueMenu->addAction(PathAction) ;

    QMenu *VariableMenu = insertmenu->addMenu("&Variable") ;
    VariableMenu->addAction(VarDateAction) ;
    VariableMenu->addAction(VarTimeAction) ;
    VariableMenu->addAction(VarPageAction) ;
    VariableMenu->addAction(VarLineAction) ;
    VariableMenu->addAction(VarFileAction) ;
    VariableMenu->addAction(VarDriveAction) ;
    VariableMenu->addAction(VarDirAction) ;
    VariableMenu->addAction(VarPathAction) ;
    VariableMenu->addAction(VarWordCountAction) ;

    insertmenu->addAction(ExtendedCharAction) ;
    insertmenu->addSeparator() ;
    insertmenu->addAction(FileAction) ;
    insertmenu->addAction(FileAtPrintAction) ;
    insertmenu->addAction(GraphicAction) ;

    QMenu *NoteMenu = insertmenu->addMenu("&Note") ;
    NoteMenu->addAction(NoteCommentAction) ;
    NoteMenu->addAction(NoteFootnoteAction) ;
    NoteMenu->addAction(NoteEndnoteAction) ;
    NoteMenu->addAction(NoteAnnotationAction) ;

    insertmenu->addSeparator() ;

    QMenu *IndexMenu = insertmenu->addMenu("&Index/TOC Entry") ;
    IndexMenu->addAction(TOCEntryAction) ;
    IndexMenu->addAction(IndexEntryAction) ;
    IndexMenu->addAction(MarkTextforIndexAction) ;
    IndexMenu->addAction(DotLeaderAction) ;

    insertmenu->addAction(ParOutlineNumberAction) ;

    connect(PageBreakAction, &QAction::triggered, this, &cWordTsar::PageBreak) ;
    connect(ColumnBreakAction, &QAction::triggered, this, &cWordTsar::ColumnBreak) ;
    connect(DateAction, &QAction::triggered, this, &cWordTsar::InsertDate) ;
    connect(TimeAction, &QAction::triggered, this, &cWordTsar::InsertTime) ;
    connect(ResultAction, &QAction::triggered, this, &cWordTsar::MathResult) ;
    connect(ExpressionAction, &QAction::triggered, this, &cWordTsar::MathExpression) ;
    connect(DollarAction, &QAction::triggered, this, &cWordTsar::MathDollar) ;
    connect(FilenameAction, &QAction::triggered, this, &cWordTsar::Filename) ;
    connect(DriveAction, &QAction::triggered, this, &cWordTsar::Drive) ;
    connect(DirectoryAction, &QAction::triggered, this, &cWordTsar::Directory) ;
    connect(PathAction, &QAction::triggered, this, &cWordTsar::Path) ;
    connect(VarDateAction, &QAction::triggered, this, &cWordTsar::VarDate) ;
    connect(VarTimeAction, &QAction::triggered, this, &cWordTsar::VarTime) ;
    connect(VarPageAction, &QAction::triggered, this, &cWordTsar::VarPage) ;
    connect(VarLineAction, &QAction::triggered, this, &cWordTsar::VarLine) ;
    connect(VarFileAction, &QAction::triggered, this, &cWordTsar::VarFilename) ;
    connect(VarDriveAction, &QAction::triggered, this, &cWordTsar::VarDrive) ;
    connect(VarDirAction, &QAction::triggered, this, &cWordTsar::VarDirectory) ;
    connect(VarPathAction, &QAction::triggered, this, &cWordTsar::VarPath) ;
    connect(VarWordCountAction, &QAction::triggered, this, &cWordTsar::VarWordCount) ;
    connect(ExtendedCharAction, &QAction::triggered, this, &cWordTsar::ExtendedChar) ;
    connect(FileAction, &QAction::triggered, this, &cWordTsar::Open) ;
    connect(FileAtPrintAction, &QAction::triggered, this, &cWordTsar::FileAtPrint) ;
    connect(GraphicAction, &QAction::triggered, this, &cWordTsar::Graphic) ;
    connect(NoteCommentAction, &QAction::triggered, this, &cWordTsar::NoteComment) ;
    connect(NoteFootnoteAction, &QAction::triggered, this, &cWordTsar::NoteFootnote) ;
    connect(NoteEndnoteAction, &QAction::triggered, this, &cWordTsar::NoteEndnote) ;
    connect(NoteAnnotationAction, &QAction::triggered, this, &cWordTsar::NoteAnnotation) ;
    connect(TOCEntryAction, &QAction::triggered, this, &cWordTsar::TOCEntry) ;
    connect(IndexEntryAction, &QAction::triggered, this, &cWordTsar::IndexEntry) ;
    connect(MarkTextforIndexAction, &QAction::triggered, this, &cWordTsar::MarkIndex) ;
    connect(DotLeaderAction, &QAction::triggered, this, &cWordTsar::DotLeader) ;
    connect(ParOutlineNumberAction, &QAction::triggered, this, &cWordTsar::ParOutlineNumber) ;

    // Style menu
    QAction *BoldAction = new QAction(mMenuProvider->GetStyleBoldLabel(), mMenuBar) ;
    QAction *ItalicAction = new QAction(mMenuProvider->GetStyleItalicLabel(), mMenuBar) ;
    QAction *UnderlineAction = new QAction(mMenuProvider->GetStyleUnderlineLabel(), mMenuBar) ;
    QAction *FontAction = new QAction(mMenuProvider->GetStyleFontLabel(), mMenuBar) ;

    QAction *StrikeoutAction = new QAction(mMenuProvider->GetStyleStrikeoutLabel(), mMenuBar) ;
    QAction *SubscriptAction = new QAction(mMenuProvider->GetStyleSubscriptLabel(), mMenuBar) ;
    QAction *SuperscriptAction = new QAction(mMenuProvider->GetStyleSuperscriptLabel(), mMenuBar) ;
    QAction *DoubleStrikeAction = new QAction(mMenuProvider->GetStyleDoubleStrikeLabel(), mMenuBar) ;
DoubleStrikeAction->setEnabled(false) ;
    QAction *ColorAction = new QAction(mMenuProvider->GetStyleColorLabel(), mMenuBar) ;

    QAction *SelectParStyleAction = new QAction(mMenuProvider->GetStyleSelectParStyleLabel(), mMenuBar) ;
SelectParStyleAction->setEnabled(false) ;
    QAction *ReturntoPrevStyleAction = new QAction(mMenuProvider->GetStyleReturnToPrevStyleLabel(), mMenuBar) ;
ReturntoPrevStyleAction->setEnabled(false) ;
    QAction *DefineParStyleAction = new QAction(mMenuProvider->GetStyleDefineParStyleLabel(), mMenuBar) ;
DefineParStyleAction->setEnabled(false) ;

    QAction *CopyStyletoLibraryAction = new QAction(mMenuProvider->GetStyleCopyStyleToLibraryLabel(), mMenuBar) ;
CopyStyletoLibraryAction->setEnabled(false) ;
    QAction *DelLibraryStyleAction = new QAction(mMenuProvider->GetStyleDeleteLibraryStyleLabel(), mMenuBar) ;
DelLibraryStyleAction->setEnabled(false) ;
    QAction *RenameLibraryStyleAction = new QAction(mMenuProvider->GetStyleRenameLibraryStyleLabel(), mMenuBar) ;
RenameLibraryStyleAction->setEnabled(false) ;
    QAction *RenameDocStyleAction = new QAction(mMenuProvider->GetStyleRenameDocStyleLabel(), mMenuBar) ;
RenameDocStyleAction->setEnabled(false) ;

    QAction *UppercaseAction = new QAction(mMenuProvider->GetStyleUppercaseLabel(), mMenuBar) ;
    QAction *LowercaseAction = new QAction(mMenuProvider->GetStyleLowercaseLabel(), mMenuBar) ;
    QAction *SentenceCaseAction = new QAction(mMenuProvider->GetStyleSentenceCaseLabel(), mMenuBar) ;

    QAction *SettingsAction = new QAction(mMenuProvider->GetStyleSettingsLabel(), mMenuBar) ;
SettingsAction->setEnabled(false) ;

    stylemenu->addAction(BoldAction) ;
    stylemenu->addAction(ItalicAction) ;
    stylemenu->addAction(UnderlineAction) ;
    stylemenu->addAction(FontAction) ;

    QMenu *OtherMenu = stylemenu->addMenu("&Other") ;
    OtherMenu->addAction(StrikeoutAction) ;
    OtherMenu->addAction(SubscriptAction) ;
    OtherMenu->addAction(SuperscriptAction) ;
    OtherMenu->addAction(DoubleStrikeAction) ;
    OtherMenu->addAction(ColorAction) ;

    stylemenu->addAction(SelectParStyleAction) ;
    stylemenu->addAction(ReturntoPrevStyleAction) ;
    stylemenu->addAction(DefineParStyleAction) ;

    QMenu *ManageMenu = stylemenu->addMenu("&Manage Paragraph Styles") ;
    ManageMenu->addAction(CopyStyletoLibraryAction) ;
    ManageMenu->addAction(DelLibraryStyleAction) ;
    ManageMenu->addAction(RenameLibraryStyleAction) ;
    ManageMenu->addAction(RenameDocStyleAction) ;

    QMenu *ConvertMenu = stylemenu->addMenu("&Convert Case") ;
    ConvertMenu->addAction(UppercaseAction) ;
    ConvertMenu->addAction(LowercaseAction) ;
    ConvertMenu->addAction(SentenceCaseAction) ;

    stylemenu->addAction(SettingsAction) ;

    connect(BoldAction, &QAction::triggered, this, &cWordTsar::Bold) ;
    connect(ItalicAction, &QAction::triggered, this, &cWordTsar::Italics) ;
    connect(UnderlineAction, &QAction::triggered, this, &cWordTsar::Underline) ;
    connect(FontAction, &QAction::triggered, this, &cWordTsar::font) ;
    connect(StrikeoutAction, &QAction::triggered, this, &cWordTsar::Strikeout) ;
    connect(SubscriptAction, &QAction::triggered, this, &cWordTsar::Subscript) ;
    connect(SuperscriptAction, &QAction::triggered, this, &cWordTsar::Superscript) ;
    connect(DoubleStrikeAction, &QAction::triggered, this, &cWordTsar::DoubleStrike) ;
    connect(ColorAction, &QAction::triggered, this, &cWordTsar::Color) ;
    connect(SelectParStyleAction, &QAction::triggered, this, &cWordTsar::SelectParStyle) ;
    connect(ReturntoPrevStyleAction, &QAction::triggered, this, &cWordTsar::ReturntoPrevStyle) ;
    connect(DefineParStyleAction, &QAction::triggered, this, &cWordTsar::DefineParStyle) ;
    connect(CopyStyletoLibraryAction, &QAction::triggered, this, &cWordTsar::CopyStyletoLibrary) ;
    connect(DelLibraryStyleAction, &QAction::triggered, this, &cWordTsar::DeleteLibraryStyle) ;
    connect(RenameLibraryStyleAction, &QAction::triggered, this, &cWordTsar::RenameLibraryStyle) ;
    connect(RenameDocStyleAction, &QAction::triggered, this, &cWordTsar::RenameDocStyle) ;
    connect(UppercaseAction, &QAction::triggered, this, &cWordTsar::Uppercase) ;
    connect(LowercaseAction, &QAction::triggered, this, &cWordTsar::Lowercase) ;
    connect(SentenceCaseAction, &QAction::triggered, this, &cWordTsar::Sentencecase) ;
    connect(SettingsAction, &QAction::triggered, this, &cWordTsar::Settings) ;

    // Layout Menu
    QAction *CenterLineAction = new QAction(mMenuProvider->GetLayoutCenterLineLabel(), mMenuBar) ;
    QAction *RightAlignAction = new QAction(mMenuProvider->GetLayoutRightAlignLabel(), mMenuBar) ;
    QAction *LeftAlignParaAction = new QAction(mMenuProvider->GetLayoutLeftAlignParaLabel(), mMenuBar) ;
    QAction *CenterParaAction = new QAction(mMenuProvider->GetLayoutCenterParaLabel(), mMenuBar) ;
    QAction *RightAlignParaAction = new QAction(mMenuProvider->GetLayoutRightAlignParaLabel(), mMenuBar) ;
    QAction *JustifyParaAction = new QAction(mMenuProvider->GetLayoutJustifyParaLabel(), mMenuBar) ;
    QAction *RulerLineAction = new QAction(mMenuProvider->GetLayoutRulerLineLabel(), mMenuBar) ;
RulerLineAction->setEnabled(false) ;
    QAction *ColumnsAction = new QAction(mMenuProvider->GetLayoutColumnsLabel(), mMenuBar) ;
ColumnsAction->setEnabled(false) ;
    QAction *PageAction = new QAction(mMenuProvider->GetLayoutPageLabel(), mMenuBar) ;
    QAction *HeaderAction = new QAction(mMenuProvider->GetLayoutHeaderLabel(), mMenuBar) ;
    QAction *FooterAction = new QAction(mMenuProvider->GetLayoutFooterLabel(), mMenuBar) ;
    QAction *PageNumberingAction = new QAction(mMenuProvider->GetLayoutPageNumberingLabel(), mMenuBar) ;
PageNumberingAction->setEnabled(false) ;
    QAction *LineNumberingAction = new QAction(mMenuProvider->GetLayoutLineNumberingLabel(), mMenuBar) ;
LineNumberingAction->setEnabled(false) ;
    QAction *AlignmentSpacingAction = new QAction(mMenuProvider->GetLayoutAlignmentLabel(), mMenuBar) ;
AlignmentSpacingAction->setEnabled(false) ;
    QAction *OverPrintCharAction = new QAction(mMenuProvider->GetLayoutOverprintCharLabel(), mMenuBar) ;
OverPrintCharAction->setEnabled(false) ;
    QAction *OverprintLineAction = new QAction(mMenuProvider->GetLayoutOverprintLineLabel(), mMenuBar) ;
OverprintLineAction->setEnabled(false) ;
    QAction *OptionalHyphenAction = new QAction(mMenuProvider->GetLayoutOptionalHyphenLabel(), mMenuBar) ;
OptionalHyphenAction->setEnabled(false) ;
    QAction *VerticalCenterAction = new QAction(mMenuProvider->GetLayoutVerticalCenterLabel(), mMenuBar) ;
VerticalCenterAction->setEnabled(false) ;
    QAction *KeepWordsTogetherAction = new QAction(mMenuProvider->GetLayoutKeepWordsTogetherLabel(), mMenuBar) ;
KeepWordsTogetherAction->setEnabled(false) ;
    QAction *KeepLinesTogetherPageAction = new QAction(mMenuProvider->GetLayoutKeepLinesTogetherPageLabel(), mMenuBar) ;
KeepLinesTogetherPageAction->setEnabled(false) ;
    QAction *KeepLinesTogetherColumnAction = new QAction(mMenuProvider->GetLayoutKeepLinesTogetherColumnLabel(), mMenuBar) ;
KeepLinesTogetherColumnAction->setEnabled(false) ;

    layoutmenu->addAction(CenterLineAction) ;
    layoutmenu->addAction(RightAlignAction) ;
    layoutmenu->addSeparator() ;
    layoutmenu->addAction(LeftAlignParaAction) ;
    layoutmenu->addAction(CenterParaAction) ;
    layoutmenu->addAction(RightAlignParaAction) ;
    layoutmenu->addAction(JustifyParaAction) ;
    layoutmenu->addSeparator() ;
    layoutmenu->addAction(RulerLineAction) ;
    layoutmenu->addAction(ColumnsAction) ;
    layoutmenu->addAction(PageAction) ;

    QMenu *hfMenu = layoutmenu->addMenu("&Headers/Footers") ;
    hfMenu->addAction(HeaderAction) ;
    hfMenu->addAction(FooterAction) ;

    layoutmenu->addAction(PageNumberingAction) ;
    layoutmenu->addAction(LineNumberingAction) ;
    layoutmenu->addAction(AlignmentSpacingAction) ;

    QMenu *SpecialMenu = layoutmenu->addMenu("Special &Effects") ;
    SpecialMenu->addAction(OverPrintCharAction) ;
    SpecialMenu->addAction(OverprintLineAction) ;
    SpecialMenu->addAction(OptionalHyphenAction) ;
    SpecialMenu->addAction(VerticalCenterAction) ;
    SpecialMenu->addAction(KeepWordsTogetherAction) ;
    SpecialMenu->addAction(KeepLinesTogetherPageAction) ;
    SpecialMenu->addAction(KeepLinesTogetherColumnAction) ;

    connect(CenterLineAction, &QAction::triggered, this, &cWordTsar::CenterLine) ;
    connect(RightAlignAction, &QAction::triggered, this, &cWordTsar::RightAlign) ;
    connect(LeftAlignParaAction, &QAction::triggered, this, &cWordTsar::LeftAlignParagraph) ;
    connect(CenterParaAction, &QAction::triggered, this, &cWordTsar::CenterParagraph) ;
    connect(RightAlignParaAction, &QAction::triggered, this, &cWordTsar::RightAlignParagraph) ;
    connect(JustifyParaAction, &QAction::triggered, this, &cWordTsar::JustifyParagraph) ;
    connect(RulerLineAction, &QAction::triggered, this, &cWordTsar::RulerLine) ;
    connect(ColumnsAction, &QAction::triggered, this, &cWordTsar::Columns) ;
    connect(PageAction, &QAction::triggered, this, &cWordTsar::Page) ;
    connect(HeaderAction, &QAction::triggered, this, &cWordTsar::Header) ;
    connect(FooterAction, &QAction::triggered, this, &cWordTsar::Footer) ;
    connect(PageNumberingAction, &QAction::triggered, this, &cWordTsar::PageNumbering) ;
    connect(LineNumberingAction, &QAction::triggered, this, &cWordTsar::LineNumbering) ;
    connect(AlignmentSpacingAction, &QAction::triggered, this, &cWordTsar::Alignment) ;
    connect(OverPrintCharAction, &QAction::triggered, this, &cWordTsar::OverprintChar) ;
    connect(OverprintLineAction, &QAction::triggered, this, &cWordTsar::OverprintLine) ;
    connect(OptionalHyphenAction, &QAction::triggered, this, &cWordTsar::OptionalHyphen) ;
    connect(VerticalCenterAction, &QAction::triggered, this, &cWordTsar::VerticalCenter) ;
    connect(KeepWordsTogetherAction, &QAction::triggered, this, &cWordTsar::KeepWordsTogether) ;
    connect(KeepLinesTogetherPageAction, &QAction::triggered, this, &cWordTsar::KeepLinesTogetherPage) ;
    connect(KeepLinesTogetherColumnAction, &QAction::triggered, this, &cWordTsar::KeepLinesTogetherColumn) ;


    // Utilities Menu
    QAction *SpellCheckGlobalAction = new QAction(mMenuProvider->GetUtilSpellCheckGlobalLabel(), mMenuBar) ;
    QAction *SpellCheckRestAction = new QAction(mMenuProvider->GetUtilSpellCheckRestLabel(), mMenuBar) ;
    QAction *SpellCheckWordAction = new QAction(mMenuProvider->GetUtilSpellCheckWordLabel(), mMenuBar) ;
    QAction *SpellCheckTypeAction = new QAction(mMenuProvider->GetUtilSpellCheckTypeLabel(), mMenuBar) ;
SpellCheckTypeAction->setEnabled(false) ;
    QAction *SpellCheckNotesAction = new QAction(mMenuProvider->GetUtilSpellCheckNotesLabel(), mMenuBar) ;
SpellCheckNotesAction->setEnabled(false) ;
    QAction *ThesaurusAction = new QAction(mMenuProvider->GetUtilThesaurusLabel(), mMenuBar) ;
ThesaurusAction->setEnabled(false) ;
    QAction *LanguageChangeAction = new QAction(mMenuProvider->GetUtilLanguageChangeLabel(), mMenuBar) ;
LanguageChangeAction->setEnabled(false) ;
    QAction *InsetAction = new QAction(mMenuProvider->GetUtilInsetLabel(), mMenuBar) ;
InsetAction->setEnabled(false) ;
    QAction *CalculatorAction = new QAction(mMenuProvider->GetUtilCalculatorLabel(), mMenuBar) ;
CalculatorAction->setEnabled(false) ;
    QAction *BlockMathAction = new QAction(mMenuProvider->GetUtilBlockMathLabel(), mMenuBar) ;
BlockMathAction->setEnabled(false) ;
    QAction *SortBlockAscAction = new QAction(mMenuProvider->GetUtilSortBlockAscLabel(), mMenuBar) ;
SortBlockAscAction->setEnabled(false) ;
    QAction *SortBlockDesAction = new QAction(mMenuProvider->GetUtilSortBlockDesLabel(), mMenuBar) ;
SortBlockDesAction->setEnabled(false) ;
    QAction *WordCountAction = new QAction(mMenuProvider->GetUtilWordCountLabel(), mMenuBar) ;
    QAction *PlayMacroAction = new QAction(mMenuProvider->GetUtilPlayMacroLabel(), mMenuBar) ;
PlayMacroAction->setEnabled(false) ;
    QAction *RecordMacroAction = new QAction(mMenuProvider->GetUtilRecordMacroLabel(), mMenuBar) ;
RecordMacroAction->setEnabled(false) ;
    QAction *EditMacroAction = new QAction(mMenuProvider->GetUtilEditMacroLabel(), mMenuBar) ;
EditMacroAction->setEnabled(false) ;
    QAction *SingleStepAction = new QAction(mMenuProvider->GetUtilSingleStepLabel(), mMenuBar) ;
SingleStepAction->setEnabled(false) ;
    QAction *CopyMacroAction = new QAction(mMenuProvider->GetUtilCopyMacroLabel(), mMenuBar) ;
CopyMacroAction->setEnabled(false) ;
    QAction *DeleteMacroAction = new QAction(mMenuProvider->GetUtilDeleteMacroLabel(), mMenuBar) ;
DeleteMacroAction->setEnabled(false) ;
    QAction *RenameMacroAction = new QAction(mMenuProvider->GetUtilRenameMacroLabel(), mMenuBar) ;
RenameMacroAction->setEnabled(false) ;
    QAction *DataFileAction = new QAction(mMenuProvider->GetUtilDataFileLabel(), mMenuBar) ;
DataFileAction->setEnabled(false) ;
    QAction *NameVarsAction = new QAction(mMenuProvider->GetUtilNameVarsLabel(), mMenuBar) ;
NameVarsAction->setEnabled(false) ;
    QAction *SetVarAction = new QAction(mMenuProvider->GetUtilSetVarLabel(), mMenuBar) ;
SetVarAction->setEnabled(false) ;
    QAction *SetVarMathAction = new QAction(mMenuProvider->GetUtilSetVarMathLabel(), mMenuBar) ;
SetVarMathAction->setEnabled(false) ;
    QAction *AskVarAction = new QAction(mMenuProvider->GetUtilAskVarLabel(), mMenuBar) ;
AskVarAction->setEnabled(false) ;
    QAction *IfAction = new QAction(mMenuProvider->GetUtilIfLabel(), mMenuBar) ;
IfAction->setEnabled(false) ;
    QAction *ElseAction = new QAction(mMenuProvider->GetUtilElseLabel(), mMenuBar) ;
ElseAction->setEnabled(false) ;
    QAction *EndIfAction = new QAction(mMenuProvider->GetUtilEndIfLabel(), mMenuBar) ;
EndIfAction->setEnabled(false) ;
    QAction *TopAction = new QAction(mMenuProvider->GetUtilTopLabel(), mMenuBar) ;
TopAction->setEnabled(false) ;
    QAction *BottomAction = new QAction(mMenuProvider->GetUtilBottomLabel(), mMenuBar) ;
BottomAction->setEnabled(false) ;
    QAction *ClearAction = new QAction(mMenuProvider->GetUtilClearLabel(), mMenuBar) ;
ClearAction->setEnabled(false) ;
    QAction *DisplayAction = new QAction(mMenuProvider->GetUtilDisplayLabel(), mMenuBar) ;
DisplayAction->setEnabled(false) ;
    QAction *PrintNTimesAction = new QAction(mMenuProvider->GetUtilPrintNTimesLabel(), mMenuBar) ;
PrintNTimesAction->setEnabled(false) ;
    QAction *ReformatRestAction = new QAction(mMenuProvider->GetUtilReformatRestLabel(), mMenuBar) ;
    QAction *ReformatParaAction = new QAction(mMenuProvider->GetUtilReformatParaLabel(), mMenuBar) ;
ReformatParaAction->setEnabled(false) ;
    QAction *ReformatNotesAction = new QAction(mMenuProvider->GetUtilReformatNotesLabel(), mMenuBar) ;
ReformatNotesAction->setEnabled(false) ;
    QAction *RepeatKeyAction = new QAction(mMenuProvider->GetUtilRepeatKeyLabel(), mMenuBar) ;
RepeatKeyAction->setEnabled(false) ;

    utilitiesmenu->addAction(SpellCheckGlobalAction) ;

    QMenu *SpellMenu = utilitiesmenu->addMenu("Spell Check &Other") ;
    SpellMenu->addAction(SpellCheckRestAction) ;
    SpellMenu->addAction(SpellCheckWordAction) ;
    SpellMenu->addAction(SpellCheckTypeAction) ;
    SpellMenu->addAction(SpellCheckNotesAction) ;

    utilitiesmenu->addAction(ThesaurusAction) ;
    utilitiesmenu->addAction(LanguageChangeAction) ;
    utilitiesmenu->addSeparator() ;

    utilitiesmenu->addAction(InsetAction) ;
    utilitiesmenu->addAction(CalculatorAction) ;
    utilitiesmenu->addAction(BlockMathAction) ;

    QMenu *SortMenu = utilitiesmenu->addMenu("Sort Bloc&k") ;
    SortMenu->addAction(SortBlockAscAction) ;
    SortMenu->addAction(SortBlockDesAction) ;

    utilitiesmenu->addAction(WordCountAction) ;
    utilitiesmenu->addSeparator() ;

    QMenu *MacrosMenu = utilitiesmenu->addMenu("&Macros") ;
    MacrosMenu->addAction(PlayMacroAction) ;
    MacrosMenu->addAction(RecordMacroAction) ;
    MacrosMenu->addAction(EditMacroAction) ;
    MacrosMenu->addAction(SingleStepAction) ;
    MacrosMenu->addAction(CopyMacroAction) ;
    MacrosMenu->addAction(DeleteMacroAction) ;
    MacrosMenu->addAction(RenameMacroAction) ;

    QMenu *MergeMenu = utilitiesmenu->addMenu("Merge &Print Commands") ;
    MergeMenu->addAction(DataFileAction) ;
    MergeMenu->addAction(NameVarsAction) ;
    MergeMenu->addAction(SetVarAction) ;
    MergeMenu->addAction(SetVarMathAction) ;
    MergeMenu->addAction(AskVarAction) ;
    MergeMenu->addSeparator() ;
    MergeMenu->addAction(IfAction) ;
    MergeMenu->addAction(ElseAction) ;
    MergeMenu->addAction(EndIfAction) ;
    MergeMenu->addAction(TopAction) ;
    MergeMenu->addAction(BottomAction) ;
    MergeMenu->addSeparator() ;
    MergeMenu->addAction(ClearAction) ;
    MergeMenu->addAction(DisplayAction) ;
    MergeMenu->addAction(PrintNTimesAction) ;

    QMenu *ReformatMenu = utilitiesmenu->addMenu("&Reformat") ;
    ReformatMenu->addAction(ReformatRestAction) ;
    ReformatMenu->addAction(ReformatParaAction) ;
    ReformatMenu->addAction(ReformatNotesAction) ;

    utilitiesmenu->addAction(RepeatKeyAction) ;

    connect(SpellCheckGlobalAction, &QAction::triggered, this, &cWordTsar::SpellCheckGlobal) ;
    connect(SpellCheckRestAction, &QAction::triggered, this, &cWordTsar::SpellCheckRest) ;
    connect(SpellCheckWordAction, &QAction::triggered, this, &cWordTsar::SpellCheckWord) ;
    connect(SpellCheckTypeAction, &QAction::triggered, this, &cWordTsar::SpellCheckType) ;
    connect(SpellCheckNotesAction, &QAction::triggered, this, &cWordTsar::SpellCheckNotes) ;
    connect(ThesaurusAction, &QAction::triggered, this, &cWordTsar::Thesaurus) ;
    connect(LanguageChangeAction, &QAction::triggered, this, &cWordTsar::LanguageChange) ;
    connect(InsetAction, &QAction::triggered, this, &cWordTsar::Inset) ;
    connect(CalculatorAction, &QAction::triggered, this, &cWordTsar::Calculator) ;
    connect(BlockMathAction, &QAction::triggered, this, &cWordTsar::BlockMath) ;
    connect(SortBlockAscAction, &QAction::triggered, this, &cWordTsar::SortBlockAsc) ;
    connect(SortBlockDesAction, &QAction::triggered, this, &cWordTsar::SortBlockDes) ;
    connect(WordCountAction, &QAction::triggered, this, &cWordTsar::WordCount) ;
    connect(PlayMacroAction, &QAction::triggered, this, &cWordTsar::PlayMacro) ;
    connect(RecordMacroAction, &QAction::triggered, this, &cWordTsar::RecordMacro) ;
    connect(EditMacroAction, &QAction::triggered, this, &cWordTsar::EditMacro) ;
    connect(SingleStepAction, &QAction::triggered, this, &cWordTsar::SingleStep) ;
    connect(CopyMacroAction, &QAction::triggered, this, &cWordTsar::CopyMacro) ;
    connect(DeleteMacroAction, &QAction::triggered, this, &cWordTsar::DeleteMacro) ;
    connect(RenameMacroAction, &QAction::triggered, this, &cWordTsar::RenameMacro) ;
    connect(DataFileAction, &QAction::triggered, this, &cWordTsar::DataFile) ;
    connect(NameVarsAction, &QAction::triggered, this, &cWordTsar::NameVars) ;
    connect(SetVarAction, &QAction::triggered, this, &cWordTsar::SetVar) ;
    connect(SetVarMathAction, &QAction::triggered, this, &cWordTsar::SetVarMath) ;
    connect(AskVarAction, &QAction::triggered, this, &cWordTsar::AskVar) ;
    connect(IfAction, &QAction::triggered, this, &cWordTsar::If) ;
    connect(ElseAction, &QAction::triggered, this, &cWordTsar::Else) ;
    connect(EndIfAction, &QAction::triggered, this, &cWordTsar::EndIf) ;
    connect(TopAction, &QAction::triggered, this, &cWordTsar::Top) ;
    connect(BottomAction, &QAction::triggered, this, &cWordTsar::Bottom) ;
    connect(ClearAction, &QAction::triggered, this, &cWordTsar::Clear) ;
    connect(DisplayAction, &QAction::triggered, this, &cWordTsar::Display) ;
    connect(PrintNTimesAction, &QAction::triggered, this, &cWordTsar::PrintNTimes) ;
    connect(ReformatRestAction, &QAction::triggered, this, &cWordTsar::ReformatRest) ;
    connect(ReformatParaAction, &QAction::triggered, this, &cWordTsar::ReformatPara) ;
    connect(ReformatNotesAction, &QAction::triggered, this, &cWordTsar::ReformatNotes) ;
    connect(RepeatKeyAction, &QAction::triggered, this, &cWordTsar::RepeatKey) ;

    // System Preferences (separate from existing Preferences dialog)
    utilitiesmenu->addSeparator() ;
    QAction *SystemPreferencesAction = new QAction("S&ystem Preferences...\t⌘,", mMenuBar) ;
    utilitiesmenu->addAction(SystemPreferencesAction) ;
    connect(SystemPreferencesAction, &QAction::triggered, this, [this]()
    {
        mEditor->SystemPreferences() ;
    }) ;


    // Help Menu
    QAction *AboutAction = new QAction(mMenuProvider->GetHelpAboutLabel(), mMenuBar) ;

    helpmenu->addAction(AboutAction) ;

    connect(AboutAction, &QAction::triggered, this, &cWordTsar::AboutWordTsar) ;

    // After any menu action: incremental layout + caret + scroll + paint
    // (same update as keyboard handler -- shared via PerformPostCommandUpdate)
    connect(mMenuBar, &QMenuBar::triggered, this, [this](QAction*)
    {
        mEditor->PerformPostCommandUpdate() ;
        QTimer::singleShot(1, mEditor, SLOT(OnIdle())) ;
    }) ;
}

// we handle menus by generating keys as if from the user
void cWordTsar::Open(void)
{
    mEditor->Open() ;
}



void cWordTsar::Save(void)
{
    mEditor->Save() ;
}



void cWordTsar::SaveAs(void)
{
    mEditor->SaveAs() ;
}



void cWordTsar::SaveandClose(void)
{
    mEditor->SaveAndClose() ;
}



void cWordTsar::Print(void)
{
    mEditor->Save() ;
    mEditor->Print() ;
}



void cWordTsar::PrintPreview(void)
{
    mEditor->Save() ;
    mEditor->PrintPreview() ;
}


void cWordTsar::AboutWordTsar(void)
{
    std::string abouttext ;
    abouttext = string_sprintf("<h2>WordTsar %s %s</h2>" \
                    "<p>Wordstar for the 21st century. This is a macOS-focused fork of "
                    "Gerald Brandt's WordTsar &mdash; the Wordstar-clone editing engine, "
                    "document formats, and original design are his work; this fork adds "
                    "macOS-specific packaging and features on top.</p>" \
                    "Licensed under the GNU Affero General Public License v3.0<br>" \
                    "https://github.com/Egbert-Azure/WordTsar<br>" \
                    "<h4>Embedded Third-Party Code</h4>" \
                    "</p>"
                    , FULLVERSION_STRING, STATUS) ;

    abouttext += "Chillout - MIT License - crash handling<br>" ;
    abouttext += "cpp-unicodelib - MIT License - Unicode text processing<br>" ;
    abouttext += "kuba-zip - MIT License - ZIP archive handling<br>" ;
    abouttext += "PicoMath - BSD 3-Clause License - math expression parser<br>" ;
    abouttext += "pugixml - MIT License - XML parsing<br>" ;
    abouttext += "SimpleINI - MIT License - INI file parsing<br>" ;
    abouttext += "<h4>System Libraries</h4>" ;
    abouttext += "Qt version " ;
    abouttext += QT_VERSION_STR ;
    abouttext += "<br>" ;
    abouttext += "macOS CoreText - font shaping, discovery, and metrics (TUI)<br>" ;
    abouttext += "macOS CoreGraphics/Quartz - PDF generation (TUI)<br>" ;
    abouttext += "macOS NSSpellChecker - spell checking<br>" ;

    QMessageBox::about(this, "About WordTsar", abouttext.c_str()) ;
}

void cWordTsar::ExitWordTsar(void)
{
    mEditor->ExitApplication() ;
}


void cWordTsar::Undo(void)
{
    mEditor->Undo() ;
}


void cWordTsar::Redo(void)
{
    mEditor->Redo() ;
}


void cWordTsar::MarkBlockStart(void)
{
    mEditor->SetBeginBlock() ;
}


void cWordTsar::MarkBlockEnd(void)
{
    mEditor->SetEndBlock() ;
}


void cWordTsar::MoveBlock(void)
{
    mEditor->MoveBlock() ;
}


void cWordTsar::CopyBlock(void)
{
    mEditor->CopyBlock() ;
}


void cWordTsar::CopyFromClipboard(void)
{
    mEditor->ClipboardPaste() ;
}


void cWordTsar::CopyToClipboard(void)
{
    mEditor->ClipboardCopy() ;
}


void cWordTsar::DeleteBlock(void)
{
    mEditor->DeleteBlock() ;
}


void cWordTsar::DeleteWord(void)
{
    mEditor->DeleteWordRight() ;
}


void cWordTsar::DeleteLine(void)
{
    mEditor->DeleteLine() ;
}


void cWordTsar::DeleteLineLeft(void)
{
    mEditor->DeleteLineLeft() ;
}


void cWordTsar::DeleteLineRight(void)
{
    mEditor->DeleteLineRight() ;
}


void cWordTsar::DeleteToChar(void)
{
    mEditor->DeleteToChar() ;
}


void cWordTsar::MarkPrevBlock(void)
{
    mEditor->SetPreviousBlock() ;
}


void cWordTsar::Find(void)
{
    mEditor->Find() ;
}


void cWordTsar::FindandReplace(void)
{
    mEditor->Replace() ;
}


void cWordTsar::FindNext(void)
{
    mEditor->FindAgain() ;
}


void cWordTsar::GotoChar(void)
{
    mEditor->GotoCharacter() ;
}


void cWordTsar::GotoPage(void)
{
    mEditor->GotoPage() ;
}


void cWordTsar::Goto1(void)
{
    mEditor->GotoSavePosition(1) ;
}


void cWordTsar::Goto2(void)
{
    mEditor->GotoSavePosition(2) ;
}


void cWordTsar::Goto3(void)
{
    mEditor->GotoSavePosition(3) ;
}


void cWordTsar::Goto4(void)
{
    mEditor->GotoSavePosition(4) ;
}


void cWordTsar::Goto5(void)
{
    mEditor->GotoSavePosition(5) ;
}


void cWordTsar::Goto6(void)
{
    mEditor->GotoSavePosition(6) ;
}


void cWordTsar::Goto7(void)
{
    mEditor->GotoSavePosition(7) ;
}


void cWordTsar::Goto8(void)
{
    mEditor->GotoSavePosition(8) ;
}


void cWordTsar::Goto9(void)
{
    mEditor->GotoSavePosition(9) ;
}


void cWordTsar::Goto0(void)
{
    mEditor->GotoSavePosition(0) ;
}


void cWordTsar::GotoFont(void)
{
    mEditor->GotoFontTag() ;
}


void cWordTsar::GotoStyle(void)
{
    mEditor->NotImplemented("^Q-<") ;
}


void cWordTsar::GotoNote(void)
{
    mEditor->NotImplemented("^Q-N-G") ;
}


void cWordTsar::GotoPrevPos(void)
{
    mEditor->GotoPreviousPosition() ;
}


void cWordTsar::GotoLastFindandReplace(void)
{
    mEditor->GotoLastFindandReplace() ;
}


void cWordTsar::GotoStartBlock(void)
{
    mEditor->MoveCursorStartBlock() ;
}


void cWordTsar::GotoEndBlock(void)
{
    mEditor->MoveCursorEndBlock() ;
}


void cWordTsar::GotoDocumentStart(void)
{
    mEditor->MoveCursorTopofFile() ;
}


void cWordTsar::GotoDocumentEnd(void)
{
    mEditor->MoveCursorEndofFile() ;
}


void cWordTsar::GotoScrollUp(void)
{
    mEditor->NotImplemented("^Q-w") ;
}


void cWordTsar::GotoScrollDown(void)
{
    mEditor->NotImplemented("^Q-z") ;
}


void cWordTsar::Set1(void)
{
    mEditor->SavePosition(1) ;
}


void cWordTsar::Set2(void)
{
    mEditor->SavePosition(2) ;
}


void cWordTsar::Set3(void)
{
    mEditor->SavePosition(3) ;
}


void cWordTsar::Set4(void)
{
    mEditor->SavePosition(4) ;
}



void cWordTsar::Set5(void)
{
    mEditor->SavePosition(5) ;
}


void cWordTsar::Set6(void)
{
    mEditor->SavePosition(6) ;
}


void cWordTsar::Set7(void)
{
    mEditor->SavePosition(7) ;
}


void cWordTsar::Set8(void)
{
    mEditor->SavePosition(8) ;
}


void cWordTsar::Set9(void)
{
    mEditor->SavePosition(9) ;
}



void cWordTsar::Set0(void)
{
    mEditor->SavePosition(0) ;
}


void cWordTsar::EditNote(void)
{
    mEditor->NotImplemented("^O-n-d") ;
}



void cWordTsar::NoteStartNumber(void)
{
}


void cWordTsar::NoteCOnvert(void)
{
    mEditor->NotImplemented("^O-n-v") ;
}


void cWordTsar::NoteConcertForPrint(void)
{
}


void cWordTsar::NoteEndNoteLocation(void)
{
    mEditor->MoveCursorStartLine() ;

    mEditor->GetDocument()->MaybeInsertHardReturn() ;
    mEditor->GetDocument()->Insert(".pe\n") ;
}


void cWordTsar::ColumnBlockMode(void)
{
    mEditor->NotImplemented("^K-n") ;
}


void cWordTsar::ColumnReplaceMode(void)
{
    mEditor->NotImplemented("^K-i") ;
}


void cWordTsar::AutoAlign(void)
{
    mEditor->Preferences() ;
}


void cWordTsar::CloseDialog(void)
{
    mEditor->InvalidCommand("^O-<CR>") ;
}


void cWordTsar::CommandTags(void)
{
    mEditor->ToggleShowControl() ;
}


void cWordTsar::BlockHighlight(void)
{
    mEditor->ToggleHideBlock() ;
}


void cWordTsar::ScreenSettings(void)
{
    mEditor->Preferences() ;
}


void cWordTsar::SwitchModes(void)
{
    mEditor->mInput->HandleKey(CTRL_O) ;
    mEditor->mInput->HandleKey('t') ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  eDisplayMode mode [in] the display mode to set the label for
///
/// @return nothing
///
/// @brief
/// Update the Command Tags / Show Formatting menu action text.
/// In page mode the label says "Show Formatting", in continuous mode it says
/// "Command Tags". Called from cEditorCtrl::OnAfterDisplayModeChange().
///
/////////////////////////////////////////////////////////////////////////////
void cWordTsar::UpdateCommandTagsLabel(eDisplayMode mode)
{
    if (mCommandTagsAction == nullptr)
    {
        return ;
    }

    if (mode == DISPLAY_PAGE)
    {
        mCommandTagsAction->setText("&Show Formatting\t^OD") ;
    }
    else
    {
        mCommandTagsAction->setText(mMenuProvider->GetViewCommandTagsLabel()) ;
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Toggle the reveal codes pane. Creates or destroys a second cEditorCtrl
/// that shares the main editor's document, displayed in continuous mode
/// with SHOW_ALL to show all control codes. The two editors are wired as
/// siblings for bidirectional caret sync.
///
/////////////////////////////////////////////////////////////////////////////
void cWordTsar::ToggleRevealCodes(void)
{
    if (mRevealCodesVisible)
    {
        // Hide: transfer focus back to main editor, disconnect siblings, destroy codes editor
        mEditor->mHasFocusDim = true ;
        mEditor->setFocus() ;
        mEditor->SetSiblingEditor(nullptr) ;

        delete mCodesEditorWidget ;  // deletes child widgets too
        mCodesEditorWidget = nullptr ;
        mRevealCodesEditor = nullptr ;
        mCodesScrollbar = nullptr ;
        mRevealCodesVisible = false ;
    }
    else
    {
        // Show: create codes editor + scrollbar in container with title bar
        mCodesEditorWidget = new QWidget(mSplitter) ;
        QVBoxLayout *containerLayout = new QVBoxLayout(mCodesEditorWidget) ;
        containerLayout->setSpacing(0) ;
        containerLayout->setContentsMargins(0, 0, 0, 0) ;

        // Title bar label -- colors derived from editor theme
        QColor bg = mEditor->GetBGroundColour() ;
        QColor fg = mEditor->GetTextColour() ;
        int titleR = static_cast<int>(bg.red()   * 0.7 + fg.red()   * 0.3) ;
        int titleG = static_cast<int>(bg.green() * 0.7 + fg.green() * 0.3) ;
        int titleB = static_cast<int>(bg.blue()  * 0.7 + fg.blue()  * 0.3) ;

        QLabel *titleLabel = new QLabel("Show Formatting", mCodesEditorWidget) ;
        titleLabel->setAlignment(Qt::AlignCenter) ;
        titleLabel->setStyleSheet(
            QString("background-color: rgb(%1, %2, %3); "
                    "color: rgb(%4, %5, %6); "
                    "padding: 2px; "
                    "font-weight: bold; "
                    "border-bottom: 1px solid rgb(%4, %5, %6);")
            .arg(titleR).arg(titleG).arg(titleB)
            .arg(fg.red()).arg(fg.green()).arg(fg.blue())) ;
        containerLayout->addWidget(titleLabel, 0) ;

        // Editor + scrollbar row
        QWidget *editorRow = new QWidget(mCodesEditorWidget) ;
        QHBoxLayout *codesLayout = new QHBoxLayout(editorRow) ;
        codesLayout->setSpacing(0) ;
        codesLayout->setContentsMargins(0, 0, 0, 0) ;

        mRevealCodesEditor = new cEditorCtrl(mEditor->GetDocument(), editorRow) ;
        mCodesScrollbar = new QScrollBar(editorRow) ;
        codesLayout->addWidget(mRevealCodesEditor) ;
        codesLayout->addWidget(mCodesScrollbar) ;
        containerLayout->addWidget(editorRow, 1) ;

        mRevealCodesEditor->SetFrame(this) ;
        mRevealCodesEditor->SetScrollbar(mCodesScrollbar) ;

        // Configure for codes display: continuous mode, show all codes
        mRevealCodesEditor->SetDisplayMode(DISPLAY_CONTINUOUS) ;
        mRevealCodesEditor->SetShowControls(SHOW_ALL) ;

        // Copy visual settings from main editor
        mRevealCodesEditor->SetBGroundColour(mEditor->GetBGroundColour()) ;
        mRevealCodesEditor->SetTextColour(mEditor->GetTextColour()) ;
        mRevealCodesEditor->SetHighlightColour(mEditor->GetHighlightColour()) ;
        mRevealCodesEditor->SetDotColour(mEditor->GetDotColour()) ;

        // Wire up siblings for bidirectional caret sync
        mEditor->SetSiblingEditor(mRevealCodesEditor) ;
        mRevealCodesEditor->SetSiblingEditor(mEditor) ;

        // Add to splitter and show
        mSplitter->addWidget(mCodesEditorWidget) ;
        mCodesEditorWidget->show() ;

        // Layout and calculate caret now
        mRevealCodesEditor->LayoutDocument(true) ;
        mRevealCodesEditor->CalculateCaretPosition() ;

        // Set splitter proportions (70% main, 30% codes)
        mSplitter->setSizes({700, 300}) ;
        mRevealCodesVisible = true ;

        // Main editor stays active, codes editor starts dimmed
        mRevealCodesEditor->mHasFocusDim = false ;
        mEditor->mHasFocusDim = true ;
        mEditor->setFocus() ;

        // Defer scroll sync -- widget needs valid geometry from Qt layout pass
        QTimer::singleShot(0, mRevealCodesEditor, [this]()
        {
            mRevealCodesEditor->ScrollIntoView() ;
            mRevealCodesEditor->Repaint() ;
        }) ;
    }

}


// InsertMenu
void cWordTsar::PageBreak(void)
{
    mEditor->MoveCursorStartLine() ;

    mEditor->GetDocument()->MaybeInsertHardReturn() ;
    mEditor->GetDocument()->Insert(".pa\n") ;
};


void cWordTsar::ColumnBreak(void)
{
    mEditor->MoveCursorStartLine() ;

    mEditor->GetDocument()->MaybeInsertHardReturn() ;
    mEditor->GetDocument()->Insert(".cb\n") ;
}


void cWordTsar::InsertDate(void)
{
    mEditor->mInput->HandleKey(CTRL_M) ;
    mEditor->mInput->HandleKey('@') ;
}


void cWordTsar::InsertTime(void)
{
    mEditor->mInput->HandleKey(CTRL_M) ;
    mEditor->mInput->HandleKey('!') ;
}


void cWordTsar::MathResult(void)
{
    mEditor->mInput->HandleKey(CTRL_M) ;
    mEditor->mInput->HandleKey('=') ;
}


void cWordTsar::MathExpression(void)
{
    mEditor->mInput->HandleKey(CTRL_M) ;
    mEditor->mInput->HandleKey('#') ;
}


void cWordTsar::MathDollar(void)
{
    mEditor->mInput->HandleKey(CTRL_M) ;
    mEditor->mInput->HandleKey('$') ;
}


void cWordTsar::Filename(void)
{
    mEditor->mInput->HandleKey(CTRL_M) ;
    mEditor->mInput->HandleKey('*') ;
}


void cWordTsar::Drive(void)
{
    mEditor->mInput->HandleKey(CTRL_M) ;
    mEditor->mInput->HandleKey(':') ;
}


void cWordTsar::Directory(void)
{
    mEditor->mInput->HandleKey(CTRL_M) ;
    mEditor->mInput->HandleKey('.') ;
}


void cWordTsar::Path(void)
{
    mEditor->mInput->HandleKey(CTRL_M) ;
    mEditor->mInput->HandleKey('\\') ;
}


void cWordTsar::VarDate(void)
{
    mEditor->InsertText("&@&") ;
}


void cWordTsar::VarTime(void)
{
    mEditor->InsertText("&!&") ;
}


void cWordTsar::VarPage(void)
{
    mEditor->InsertText("&#&") ;
}


void cWordTsar::VarLine(void)
{
    mEditor->InsertText("&_&") ;
}


void cWordTsar::VarFilename(void)
{
    mEditor->InsertText("&*&") ;
}


void cWordTsar::VarDrive(void)
{
    mEditor->InsertText("&:&") ;
}


void cWordTsar::VarDirectory(void)
{
    mEditor->InsertText("&.&") ;
}


void cWordTsar::VarPath(void)
{
    mEditor->InsertText("&\\&") ;
}


void cWordTsar::VarWordCount(void)
{
    mEditor->InsertText("&?&") ;
}


void cWordTsar::ExtendedChar(void)
{
    mEditor->mInput->HandleKey(CTRL_P) ;
    mEditor->mInput->HandleKey('0') ;
}


void cWordTsar::FileAtPrint(void)
{
    // @TODO - get file via dialog
    mEditor->MoveCursorStartLine() ;

    mEditor->GetDocument()->MaybeInsertHardReturn() ;
    mEditor->GetDocument()->Insert(".fi\n") ;
}


void cWordTsar::Graphic(void)
{
    // @TODO - get file via dialog
    mEditor->mInput->HandleKey(CTRL_P) ;
    mEditor->mInput->HandleKey('*') ;
}


void cWordTsar::NoteComment(void)
{
    mEditor->mInput->HandleKey(CTRL_O) ;
    mEditor->mInput->HandleKey('n') ;
    mEditor->mInput->HandleKey('c') ;
}


void cWordTsar::NoteFootnote(void)
{
    mEditor->mInput->HandleKey(CTRL_O) ;
    mEditor->mInput->HandleKey('n') ;
    mEditor->mInput->HandleKey('f') ;
}


void cWordTsar::NoteEndnote(void)
{
    mEditor->mInput->HandleKey(CTRL_O) ;
    mEditor->mInput->HandleKey('n') ;
    mEditor->mInput->HandleKey('e') ;
}


void cWordTsar::NoteAnnotation(void)
{
    mEditor->mInput->HandleKey(CTRL_O) ;
    mEditor->mInput->HandleKey('n') ;
    mEditor->mInput->HandleKey('a') ;
}


void cWordTsar::TOCEntry(void)
{
    bool ok = false ;
    QString text = QInputDialog::getText(this, "Table of Contents Entry",
        "Text to appear in the table of contents:", QLineEdit::Normal, "", &ok) ;

    if (!ok || text.isEmpty())
    {
        return ;
    }

    mEditor->MoveCursorStartLine() ;
    mEditor->GetDocument()->MaybeInsertHardReturn() ;
    mEditor->GetDocument()->Insert(".tc " + text.toStdString() + "\n") ;
}


void cWordTsar::IndexEntry(void)
{
    bool ok = false ;
    QString text = QInputDialog::getText(this, "Index Entry",
        "Word or phrase to appear in the index:", QLineEdit::Normal, "", &ok) ;

    if (!ok || text.isEmpty())
    {
        return ;
    }

    mEditor->MoveCursorStartLine() ;
    mEditor->GetDocument()->MaybeInsertHardReturn() ;
    mEditor->GetDocument()->Insert(".ix " + text.toStdString() + "\n") ;
}


void cWordTsar::MarkIndex(void)
{
    mEditor->mInput->HandleKey(CTRL_P) ;
    mEditor->mInput->HandleKey('k') ;
}


void cWordTsar::DotLeader(void)
{
    mEditor->mInput->HandleKey(CTRL_P) ;
    mEditor->mInput->HandleKey('.') ;
}


void cWordTsar::ParOutlineNumber(void)
{
    mEditor->mInput->HandleKey(CTRL_O) ;
    mEditor->mInput->HandleKey('z') ;
}


void cWordTsar::Bold(void)
{
    mEditor->mInput->HandleKey(CTRL_P) ;
    mEditor->mInput->HandleKey('b') ;
}


void cWordTsar::Italics(void)
{
    mEditor->mInput->HandleKey(CTRL_P) ;
    mEditor->mInput->HandleKey('y') ;
}


void cWordTsar::Underline(void)
{
    mEditor->mInput->HandleKey(CTRL_P) ;
    mEditor->mInput->HandleKey('s') ;
}


void cWordTsar::font(void)
{
    mEditor->mInput->HandleKey(CTRL_P) ;
    mEditor->mInput->HandleKey('=') ;
}


void cWordTsar::Strikeout(void)
{
    mEditor->mInput->HandleKey(CTRL_P) ;
    mEditor->mInput->HandleKey('x') ;
}


void cWordTsar::Subscript(void)
{
    mEditor->mInput->HandleKey(CTRL_P) ;
    mEditor->mInput->HandleKey('v') ;
}


void cWordTsar::Superscript(void)
{
    mEditor->mInput->HandleKey(CTRL_P) ;
    mEditor->mInput->HandleKey('t') ;
}


void cWordTsar::DoubleStrike(void)
{
    mEditor->mInput->HandleKey(CTRL_P) ;
    mEditor->mInput->HandleKey('d') ;
}


void cWordTsar::Color(void)
{
    mEditor->mInput->HandleKey(CTRL_P) ;
    mEditor->mInput->HandleKey('-') ;
}


void cWordTsar::SelectParStyle(void)
{
    mEditor->mInput->HandleKey(CTRL_O) ;
    mEditor->mInput->HandleKey('f') ;
    mEditor->mInput->HandleKey('s') ;
}


void cWordTsar::ReturntoPrevStyle(void)
{
    mEditor->mInput->HandleKey(CTRL_O) ;
    mEditor->mInput->HandleKey('f') ;
    mEditor->mInput->HandleKey('p') ;
}


void cWordTsar::DefineParStyle(void)
{
    mEditor->mInput->HandleKey(CTRL_O) ;
    mEditor->mInput->HandleKey('f') ;
    mEditor->mInput->HandleKey('d') ;
}


void cWordTsar::CopyStyletoLibrary(void)
{
    mEditor->mInput->HandleKey(CTRL_O) ;
    mEditor->mInput->HandleKey('f') ;
    mEditor->mInput->HandleKey('o') ;
}


void cWordTsar::DeleteLibraryStyle(void)
{
    mEditor->mInput->HandleKey(CTRL_O) ;
    mEditor->mInput->HandleKey('f') ;
    mEditor->mInput->HandleKey('y') ;
}


void cWordTsar::RenameLibraryStyle(void)
{
    mEditor->mInput->HandleKey(CTRL_O) ;
    mEditor->mInput->HandleKey('f') ;
    mEditor->mInput->HandleKey('r') ;
}


void cWordTsar::RenameDocStyle(void)
{
    mEditor->mInput->HandleKey(CTRL_O) ;
    mEditor->mInput->HandleKey('f') ;
    mEditor->mInput->HandleKey('e') ;
}


void cWordTsar::Uppercase(void)
{
    mEditor->mInput->HandleKey(CTRL_K) ;
    mEditor->mInput->HandleKey('\"') ;
}


void cWordTsar::Lowercase(void)
{
    mEditor->mInput->HandleKey(CTRL_K) ;
    mEditor->mInput->HandleKey('\'') ;
}


void cWordTsar::Sentencecase(void)
{
    mEditor->mInput->HandleKey(CTRL_K) ;
    mEditor->mInput->HandleKey('.') ;
}


void cWordTsar::Settings(void)
{
    // @TODO implement
}


void cWordTsar::CenterLine(void)
{
    mEditor->mInput->HandleKey(CTRL_O) ;
    mEditor->mInput->HandleKey('c') ;
}


void cWordTsar::RightAlign(void)
{
    mEditor->mInput->HandleKey(CTRL_O) ;
    mEditor->mInput->HandleKey(']') ;
}


void cWordTsar::LeftAlignParagraph(void)
{
    mEditor->SetParagraphAlignment(JUST_LEFT) ;
}


void cWordTsar::CenterParagraph(void)
{
    mEditor->SetParagraphAlignment(JUST_CENTER) ;
}


void cWordTsar::RightAlignParagraph(void)
{
    mEditor->SetParagraphAlignment(JUST_RIGHT) ;
}


void cWordTsar::JustifyParagraph(void)
{
    mEditor->SetParagraphAlignment(JUST_JUST) ;
}


void cWordTsar::RulerLine(void)
{
    mEditor->mInput->HandleKey(CTRL_O) ;
    mEditor->mInput->HandleKey('l') ;
}


void cWordTsar::Columns(void)
{
    mEditor->mInput->HandleKey(CTRL_O) ;
    mEditor->mInput->HandleKey('U') ;
}


void cWordTsar::Page(void)
{
    mEditor->mInput->HandleKey(CTRL_O) ;
    mEditor->mInput->HandleKey('Y') ;
}


void cWordTsar::Header(void)
{
    mEditor->MoveCursorStartLine() ;

    mEditor->GetDocument()->MaybeInsertHardReturn() ;
    mEditor->GetDocument()->Insert(".he\n") ;
}


void cWordTsar::Footer(void)
{
    mEditor->MoveCursorStartLine() ;

    mEditor->GetDocument()->MaybeInsertHardReturn() ;
    mEditor->GetDocument()->Insert(".fo\n") ;
}


void cWordTsar::PageNumbering(void)
{
    mEditor->mInput->HandleKey(CTRL_O) ;
    mEditor->mInput->HandleKey('#') ;
}


void cWordTsar::LineNumbering(void)
{
    mEditor->MoveCursorStartLine() ;

    mEditor->GetDocument()->MaybeInsertHardReturn() ;
    mEditor->GetDocument()->Insert(".l#\n") ;
}


void cWordTsar::Alignment(void)
{
    mEditor->mInput->HandleKey(CTRL_O) ;
    mEditor->mInput->HandleKey('s') ;
}


void cWordTsar::OverprintChar(void)
{
    mEditor->mInput->HandleKey(CTRL_P) ;
    mEditor->mInput->HandleKey('H') ;
}


void cWordTsar::OverprintLine(void)
{
    mEditor->mInput->HandleKey(CTRL_P) ;
    mEditor->mInput->HandleKey(HARD_RETURN) ;
}


void cWordTsar::OptionalHyphen(void)
{
    mEditor->mInput->HandleKey(CTRL_O) ;
    mEditor->mInput->HandleKey('e') ;
}


void cWordTsar::VerticalCenter(void)
{
    mEditor->mInput->HandleKey(CTRL_O) ;
    mEditor->mInput->HandleKey('v') ;
}


void cWordTsar::KeepWordsTogether(void)
{
    mEditor->mInput->HandleKey(CTRL_P) ;
    mEditor->mInput->HandleKey('O') ;
}


void cWordTsar::KeepLinesTogetherPage(void)
{
    mEditor->MoveCursorStartLine() ;

    mEditor->GetDocument()->MaybeInsertHardReturn() ;
    mEditor->GetDocument()->Insert(".cp\n") ;
}


void cWordTsar::KeepLinesTogetherColumn(void)
{
    mEditor->MoveCursorStartLine() ;

    mEditor->GetDocument()->MaybeInsertHardReturn() ;
    mEditor->GetDocument()->Insert(".cc\n") ;
}


void cWordTsar::SpellCheckGlobal(void)
{
    mEditor->mInput->HandleKey(CTRL_Q) ;
    mEditor->mInput->HandleKey('r') ;
    mEditor->mInput->HandleKey(CTRL_Q) ;
    mEditor->mInput->HandleKey('l') ;
}


void cWordTsar::SpellCheckRest(void)
{
    mEditor->mInput->HandleKey(CTRL_Q) ;
    mEditor->mInput->HandleKey('l') ;
}


void cWordTsar::SpellCheckWord(void)
{
    mEditor->mInput->HandleKey(CTRL_Q) ;
    mEditor->mInput->HandleKey('N') ;
}


void cWordTsar::SpellCheckType(void)
{
    mEditor->mInput->HandleKey(CTRL_Q) ;
    mEditor->mInput->HandleKey('o') ;
}


void cWordTsar::SpellCheckNotes(void)
{
    mEditor->mInput->HandleKey(CTRL_Q) ;
    mEditor->mInput->HandleKey('O') ;
    mEditor->mInput->HandleKey('L') ;
}


void cWordTsar::Thesaurus(void)
{
    mEditor->mInput->HandleKey(CTRL_Q) ;
    mEditor->mInput->HandleKey('j') ;
}


void cWordTsar::LanguageChange(void)
{
    mEditor->MoveCursorStartLine() ;

    mEditor->GetDocument()->MaybeInsertHardReturn() ;
    mEditor->GetDocument()->Insert(".la\n") ;
}


void cWordTsar::Inset(void)
{
    mEditor->mInput->HandleKey(CTRL_P) ;
    mEditor->mInput->HandleKey('&') ;
}


void cWordTsar::Calculator(void)
{
    mEditor->mInput->HandleKey(CTRL_Q) ;
    mEditor->mInput->HandleKey('M') ;
}


void cWordTsar::BlockMath(void)
{
    mEditor->mInput->HandleKey(CTRL_K) ;
    mEditor->mInput->HandleKey('m') ;
}


void cWordTsar::SortBlockAsc(void)
{
    mEditor->mInput->HandleKey(CTRL_K) ;
    mEditor->mInput->HandleKey('z') ;
    mEditor->mInput->HandleKey('a') ;
}


void cWordTsar::SortBlockDes(void)
{
    mEditor->mInput->HandleKey(CTRL_K) ;
    mEditor->mInput->HandleKey('z') ;
    mEditor->mInput->HandleKey('d') ;
}


void cWordTsar::WordCount(void)
{
    mEditor->mInput->HandleKey(CTRL_K) ;
    mEditor->mInput->HandleKey('?') ;
}


void cWordTsar::PlayMacro(void)
{
    mEditor->mInput->HandleKey(CTRL_M) ;
    mEditor->mInput->HandleKey('p') ;
}


void cWordTsar::RecordMacro(void)
{
    mEditor->mInput->HandleKey(CTRL_M) ;
    mEditor->mInput->HandleKey('r') ;
}


void cWordTsar::EditMacro(void)
{
    mEditor->mInput->HandleKey(CTRL_M) ;
    mEditor->mInput->HandleKey('D') ;
}


void cWordTsar::SingleStep(void)
{
    mEditor->mInput->HandleKey(CTRL_M) ;
    mEditor->mInput->HandleKey('s') ;
}


void cWordTsar::CopyMacro(void)
{
    mEditor->mInput->HandleKey(CTRL_M) ;
    mEditor->mInput->HandleKey('O') ;
}


void cWordTsar::DeleteMacro(void)
{
    mEditor->mInput->HandleKey(CTRL_M) ;
    mEditor->mInput->HandleKey('y') ;
}


void cWordTsar::RenameMacro(void)
{
    mEditor->mInput->HandleKey(CTRL_M) ;
    mEditor->mInput->HandleKey('e') ;
}


void cWordTsar::DataFile(void)
{
    mEditor->MoveCursorStartLine() ;

    mEditor->GetDocument()->MaybeInsertHardReturn() ;
    mEditor->GetDocument()->Insert(".df\n") ;
}


void cWordTsar::NameVars(void)
{
    mEditor->MoveCursorStartLine() ;

    mEditor->GetDocument()->MaybeInsertHardReturn() ;
    mEditor->GetDocument()->Insert(".rv\n") ;
}


void cWordTsar::SetVar(void)
{
    mEditor->MoveCursorStartLine() ;

    mEditor->GetDocument()->MaybeInsertHardReturn() ;
    mEditor->GetDocument()->Insert(".sv\n") ;
}


void cWordTsar::SetVarMath(void)
{
    mEditor->MoveCursorStartLine() ;

    mEditor->GetDocument()->MaybeInsertHardReturn() ;
    mEditor->GetDocument()->Insert(".ma\n") ;
}


void cWordTsar::AskVar(void)
{
    mEditor->MoveCursorStartLine() ;

    mEditor->GetDocument()->MaybeInsertHardReturn() ;
    mEditor->GetDocument()->Insert(".av\n") ;
}


void cWordTsar::If(void)
{
    mEditor->MoveCursorStartLine() ;

    mEditor->GetDocument()->MaybeInsertHardReturn() ;
    mEditor->GetDocument()->Insert(".if\n") ;
}


void cWordTsar::Else(void)
{
    mEditor->MoveCursorStartLine() ;

    mEditor->GetDocument()->MaybeInsertHardReturn() ;
    mEditor->GetDocument()->Insert(".el\n") ;
}


void cWordTsar::EndIf(void)
{
    mEditor->MoveCursorStartLine() ;

    mEditor->GetDocument()->MaybeInsertHardReturn() ;
    mEditor->GetDocument()->Insert(".ei\n") ;
}


void cWordTsar::Top(void)
{
    mEditor->MoveCursorStartLine() ;

    mEditor->GetDocument()->MaybeInsertHardReturn() ;
    mEditor->GetDocument()->Insert(".go t\n") ;
}


void cWordTsar::Bottom(void)
{
    mEditor->MoveCursorStartLine() ;

    mEditor->GetDocument()->MaybeInsertHardReturn() ;
    mEditor->GetDocument()->Insert(".go b\n") ;
}


void cWordTsar::Clear(void)
{
    mEditor->MoveCursorStartLine() ;

    mEditor->GetDocument()->MaybeInsertHardReturn() ;
    mEditor->GetDocument()->Insert(".cs\n") ;
}


void cWordTsar::Display(void)
{
    mEditor->MoveCursorStartLine() ;

    mEditor->GetDocument()->MaybeInsertHardReturn() ;
    mEditor->GetDocument()->Insert(".dm\n") ;
}


void cWordTsar::PrintNTimes(void)
{
    mEditor->MoveCursorStartLine() ;

    mEditor->GetDocument()->MaybeInsertHardReturn() ;
    mEditor->GetDocument()->Insert(".rp\n") ;
}


void cWordTsar::ReformatRest(void)
{
    mEditor->mInput->HandleKey(CTRL_Q) ;
    mEditor->mInput->HandleKey('u') ;
}


void cWordTsar::ReformatPara(void)
{
    mEditor->mInput->HandleKey(CTRL_B) ;
}


void cWordTsar::ReformatNotes(void)
{
    mEditor->mInput->HandleKey(CTRL_O) ;
    mEditor->mInput->HandleKey('n') ;
    mEditor->mInput->HandleKey('u') ;
}


void cWordTsar::RepeatKey(void)
{
    mEditor->mInput->HandleKey(CTRL_Q) ;
    mEditor->mInput->HandleKey('q') ;
}
