.. QUICKREF.WS - WordTsar 0.10.9 keyboard quick reference
.. Source of truth: KEY_MAPPING.md and the input-handler source. Edit freely.
.pl 66
.mt 4
.mb 6
.lm 1
.rm 66
.oj off
.aw off
.he WordTsar 0.10.9 Quick Reference
WORDTSAR 0.10.9 - KEYBOARD QUICK REFERENCE

Two input modes, set in System Preferences > Editor >
Keyboard Mode:

  WordStar (default)  classic WS7 control-key sequences
  Modern (CUA)         Ctrl+C/V/X/Z and MS-Word-style keys

Both the terminal app (ws) and the GUI app (WordTsar) support
BOTH modes. The mode is a per-app Preferences setting, not
tied to which binary you run -- switching apps does not
switch modes, and "GUI" does not mean Modern any more than
"terminal" means WordStar.

A chord is a prefix key then a second key: ^K then B. Ctrl
chords (^K ^Q ^O ^P ^M) work everywhere, in both apps, on any
terminal, with no setup. Everything below marked "Alt+" is an
optional alternate way to enter the same chord, not a separate
feature -- see ALTERNATE CHORD ENTRY further down before
relying on it.

Entries marked (WS) exist only in WordStar mode.

.pa
SINGLE CTRL KEYS

Key    WordStar                     Modern (CUA)
----------------------------------------------------------------
^A     Word left                    Select all
^B     Reformat para (not impl)     Bold toggle
^C     Page down                    Copy
^D     Cursor right                 Select font
^E     Cursor up                    Center paragraph
^F     Word right                   Find
^G     Delete forward               Goto page
^H     Delete back (backspace)      Find and replace
^I     Insert tab                   Italic toggle
^J     -- (unassigned in WS7)       Justify paragraph
^K     Block/file chord prefix      Mark block start
^L     Find again                   Left align paragraph
^M     Macro/insert chord prefix    --
^N     Line break                   New document
^O     Format chord prefix          Open file
^P     Style chord prefix           Print preview
^Q     Quick chord prefix           --
^R     Page up                      Right align paragraph
^S     Cursor left                  Save
^T     Delete word right            Insert tab
^U     Undo                         Underline toggle
^V     Insert/overwrite toggle      Paste
^W     Scroll up                    Close document
^X     Cursor down                  Cut
^Y     Delete line                  Redo
^Z     Scroll down                  Undo

MODERN MODE EXTRAS

^Shift+S Save as
^Shift+Z Redo (alternate)
.pa
SPECIAL KEYS (BOTH MODES)

Arrows   Cursor movement
^Left    Word left
^Right   Word right
Home     Start of line
End      End of line
^Home    Start of document
^End     End of document
PgUp     Page up
PgDn     Page down
^PgUp    Scroll up
^PgDn    Scroll down
Del      Delete forward
^Del     Delete word right
Bksp     Delete backward
^Bksp    Delete word left
Tab      Insert tab
Enter    Line break
Esc      Cancel chord

FUNCTION KEYS AND macOS SHORTCUTS

F1       WordStar mode: contextual help
         Modern mode: System Preferences
F3       Find again (Modern mode only)
F11      Toggle fullscreen

F1 then a command key describes that command. F1 twice
sets the help level, 0 to 4. For a chord press F1, the
prefix, then the sub-letter, for example F1 K B.

On macOS F1, F3 and F11 are unreliable: the system claims
them for brightness, Mission Control and Show Desktop
unless you free them in System Settings > Keyboard. In
the GUI, use these instead:

Cmd+,       Preferences
Cmd+/       Help
Cmd+Ctrl+F  Toggle fullscreen
Cmd+G       Find again, both modes

Cmd chords are GUI only; a terminal emulator takes them
before ws ever sees them. In the TUI F1 does reach the
app, and Preferences is menu-only.

F2 and F4 through F10 and F12 are unassigned. F10 opens
the pull-down menu bar in both apps -- every command
below, including the ones with no reachable keystroke, is
also there if you'd rather click/select than remember a
chord.
.pa
ALTERNATE CHORD ENTRY (OPTIONAL -- READ THIS FIRST)

^K ^Q ^O ^P already work with plain Ctrl in every terminal
and in the GUI. You do not need anything below this line
for those four. Skip it unless a chord genuinely won't
enter.

^M is the one real exception: the byte for Ctrl+M is
identical to Return/Enter on every terminal there is, so
^M can never be typed as "Ctrl, then M" in the TUI. Use
the Insert menu (F10 > Insert) for dates, times and
filenames instead -- it sends the same command without
touching the keyboard, and it always works.

WordStar mode also offers Alt+K/Q/O/P/M as a second way to
enter these same chords, and Alt+U for Redo. This exists
for terminals that intercept plain Ctrl. It needs "Use
Option as Meta key" turned on in Terminal.app
(Preferences > Profiles > Keyboard), or "Option Key Acts
as: Esc+" in iTerm2 (Preferences > Profiles > Keys) --
macOS does not send the required escape sequence by
default, so out of the box these Alt combinations just
type accented characters instead of entering a chord. This
is a terminal quirk only: the GUI receives Option as a
plain modifier key regardless of any setting, so Alt+K
etc. work there immediately.

Given the setup cost, prefer plain Ctrl chords and the F10
menu; treat Alt-entry as a fallback for a terminal that
truly swallows Ctrl, not the normal way to work.

Modern mode chord prefixes:
Alt+K    Block and marker chord
Alt+Q    Navigation and deletion chord
Alt+O    Onscreen formatting chord
Alt+P    Style chord

Alt chords take the same second key as the WordStar ^K,
^Q, ^O and ^P chords listed below. The ^M chord has no
Modern equivalent.
.pa
^K CHORD - BLOCK AND FILE

^K B     Mark block start
^K K     Mark block end
^K C     Copy block to cursor
^K V     Move block to cursor
^K Y     Delete marked block
^K H     Toggle block highlight
^K <     Unmark block
^K U     Restore previous block
^K [     Paste from system clipboard (WS)
^K ]     Copy to system clipboard (WS)
^K "     Block to uppercase
^K '     Block to lowercase
^K .     Block to title case
^K ?     Word and character count for block
^K 0-9   Set position marker
^K R     Open or insert file (WS)
^K S     Save file (WS)
^K T     Save as (WS)
^K D     Save, clear document, reset (WS)
^K X     Save and quit (WS)
^K Q     Abandon without saving (WS)
^K P     Print preview (WS) -- see GAPS below

Blocks persist. Mark a block, edit elsewhere for an hour,
then act on it. Block copy/move use an internal buffer;
^K [ and ^K ] are the only bridge to the OS clipboard in
WordStar mode.
.pa
^Q CHORD - QUICK NAVIGATION AND DELETION

^Q S     Start of line (WS)
^Q D     End of line (WS)
^Q R     Start of document (WS)
^Q C     End of document (WS)
^Q E     Top left of screen
^Q X     Bottom right of screen
^Q B     Start of block
^Q K     End of block
^Q P     Previous cursor position
^Q V     Last find/replace location
^Q =     Next font tag
^Q G     Goto character, forward
^Q H     Goto character, backward
^Q I     Goto page number (WS)
^Q 0-9   Goto position marker
^Q F     Find (WS)
^Q A     Find and replace (WS)
^Q U     Reformat / re-layout document
^Q L     Spell check document (WS)
^Q N     Spell check word at cursor (WS)
^Q O     Spell check a typed word (WS)
^Q Spc   Delete to start of line
^Q Del   Delete to start of line
^Q Y     Delete to end of line
^Q T     Delete to given character
.pa
^O CHORD - ONSCREEN FORMAT

^O C     Center line
^O ]     Right align line
^O D     Toggle command tags
^O J     Toggle justification
^O Y     Page layout dialog
^O <     Left align paragraph
^O =     Center paragraph
^O >     Right align paragraph
^O +     Justify paragraph
^O P     Print preview (WS)
^O ?     Status and memory info (WS)
^O B     Screen settings (WS)
^O T     GUI: page/continuous view. TUI: center view
^O W     Word wrap on/off

The four paragraph alignment commands bracket the current
paragraph with .oj dot commands, and restore the previous
alignment after it. Repeating the same one removes the
brackets. Modern equivalents: ^L ^E ^R ^J.

^P CHORD - STYLE AND PRINT

^P B     Bold (Modern: ^B)
^P S     Underline (Modern: ^U)
^P Y     Italic (Modern: ^I)
^P V     Subscript
^P T     Superscript
^P X     Strikethrough
^P K     Strikethrough (alternate)
^P =     Font dialog (Modern: ^D)
^P +     Font dialog (alternate)
^P -     Color dialog
.pa
^M CHORD - MACROS AND INSERTION (WORDSTAR ONLY)

^M @     Insert current date
^M !     Insert current time
^M *     Insert current filename
^M .     Insert current directory
^M \     Insert full path and filename

In the TUI, reach this chord via F10 > Insert, not the
keyboard -- see ALTERNATE CHORD ENTRY above. In the GUI,
Alt+M works directly.

VARIABLES

These expand on display and in headers and footers:

&@&      Current date
&!&      Current time
&#&      Page number
&_&      Line number
&*&      Filename
&:&      Filesystem root (/)
&.&      Directory
&\&      Full path
.pa
GAPS -- NOT YET IMPLEMENTED

These are on the roadmap; the command exists but doesn't
do its job yet:

  ^B          Reformat paragraph (WordStar mode)
  ^K P        Opens print preview instead of printing
              directly, same as ^O P. Real WordStar's ^K P
              prints with no preview. The preview dialog
              has its own Print button, so printing is
              still reachable meanwhile.
  ^M P/R/D/S/E/O/Y
              Macro play/record/edit/single-step/rename/
              copy/delete -- recognised, not implemented.
  ^M = , ^M #  Math insert/recalculate -- recognised, not
              implemented.

GAPS -- PLATFORM LIMITS, NOT ON THE ROADMAP

These stay this way by design, not by omission:

  ^O T        Not a real WordStar 7 command -- checked
              against the WS7 manual's full command
              reference, which has no ^OT at all. WordTsar
              put it on an unused letter, and the two apps
              use it for platform-appropriate things: the
              GUI toggles its pixel-based page/continuous
              view (and hides reveal codes, like real
              print preview does); the terminal has no
              pixel canvas to paginate, so it instead
              centers the editing pane and ruler
              horizontally -- text floats in the middle of
              a wide terminal instead of sitting flush left.
              Command-tag visibility is unaffected in the
              TUI; toggle that separately with ^O D.
  Alt+ chords Need "Option as Meta" turned on in the
              terminal (see ALTERNATE CHORD ENTRY); off by
              default on macOS. Not needed in the GUI.
  F1 / F3 / F11
              Unreliable in the GUI on macOS unless freed
              from system shortcuts; use the Cmd+ equivalents
              listed under FUNCTION KEYS instead.
.pa
DOT COMMANDS IN COMMON USE

Type at column 1, alone on the line. WordTsar shows any
dot command it does not implement on a red background, so
the editor is the authority, not this page.

  .pa        Page break
  .cp n      Conditional page break
  .pl n      Page length in lines
  .mt n      Top margin
  .mb n      Bottom margin
  .lm n      Left margin
  .rm n      Right margin
  .po n      Page offset
  .pm n      Paragraph margin
  .lh n      Line height, 48ths of an inch
  .ls n      Line spacing, 1 to 9
  .oj        Justify: on, off, c, r
  .aw        Align and word wrap on/off
  .he        Header text
  .fo        Footer text
  .hm n      Header margin
  .fm n      Footer margin
  .pn n      Page number, with format
  .pg        Print page numbers
  .op        Omit page numbers
  .pc n      Page number column
  .pr or=l   Landscape (or=p for portrait)
  .sr n      Sub/superscript roll
  .tb        Tab stops
  .rr        Ruler line
  .ig or ..  Comment, not printed

MEASUREMENTS AND MATH

Arguments take i or in (inches), c or cm, mm, and p or pt
(points). Bare numbers are columns of 0.1 inch. Addition
and subtraction can mix units, for example .rm 4c + 2i,
or .lm +2c to adjust relative to the current value.
Multiply and divide cannot mix units, and are flagged as
an error.

The .tb command takes type prefixes: ^ centers, > right
aligns, and # aligns on the decimal point. For example:

  .tb 1.0i ^2.1i 3i >5i #6.0i

The .rr command uses the same prefixes in the ruler:

  .rr l----------!----------#----------^-------->-----r

Each dash is 0.1 inch, one Courier New 12 point character.
