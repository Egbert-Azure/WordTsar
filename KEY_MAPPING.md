# WordTsar Key and Menu Mapping Reference

This document maps WordStar keyboard commands and menus to their Modern (CUA) equivalents in WordTsar, along with the functionality each binding provides.

WordTsar supports two input modes, selectable in **System Preferences > Editor > Keyboard Mode**:
- **WordStar** (default) -- Classic WordStar 7 control-key sequences
- **Modern (CUA)** -- Standard Ctrl+C/V/X/Z shortcuts with Alt-prefix chords for advanced operations, following the same conventions as most modern text editors

Both apps -- the terminal `ws` and the GUI `WordTsar` -- support both modes independently. The mode is a per-app Preferences setting, not a property of which binary you're running: "GUI" does not imply Modern, and "terminal" does not imply WordStar.

---

## Table of Contents

- [Quick-View Tables](#quick-view-tables)
  - [Single Ctrl+Letter Keys (A-Z)](#single-ctrlletter-keys-a-z)
  - [Additional Modern Shortcuts](#additional-modern-shortcuts)
  - [Special Keys (Both Modes)](#special-keys-both-modes)
  - [Function Keys](#function-keys)
- [Chord Sequences](#chord-sequences)
  - [K Chord -- Block and File Operations](#k-chord----block-and-file-operations)
  - [Q Chord -- Quick Navigation and Deletion](#q-chord----quick-navigation-and-deletion)
  - [O Chord -- Onscreen Formatting and Display](#o-chord----onscreen-formatting-and-display)
  - [P Chord -- Style and Print Formatting](#p-chord----style-and-print-formatting)
  - [M Chord -- Macros and Insertion (WordStar Only)](#m-chord----macros-and-insertion-wordstar-only)
  - [Macros](#macros)
  - [Additional Alt Shortcuts](#additional-alt-shortcuts)
- [Menu Mapping](#menu-mapping)
  - [File Menu](#file-menu)
  - [Edit Menu](#edit-menu)
  - [View Menu](#view-menu)
  - [Insert Menu](#insert-menu)
  - [Style Menu](#style-menu)
  - [Layout Menu](#layout-menu)
  - [Utilities Menu](#utilities-menu)
  - [Help Menu](#help-menu)
  - [Known menu and key mismatches](#known-menu-and-key-mismatches)
- [Detailed Binding Reference](#detailed-binding-reference)
  - [How Modern Mode Works](#how-modern-mode-works)
  - [Input Mode Switching](#input-mode-switching)
  - [Unassigned Keys](#unassigned-keys)
  - [Shared Bindings](#shared-bindings)
- [Complete WordStar Input Handler Reference](#complete-wordstar-input-handler-reference)
  - [Single Control Keys (^A-^Z)](#single-control-keys-a-z)
  - [Alt Shortcuts (WordStar Mode)](#alt-shortcuts-wordstar-mode)
  - [Special / Navigation Keys](#special--navigation-keys)
  - [^M Chord -- Macros and Insertion](#m-chord----macros-and-insertion)
  - [^K Chord -- Block and File](#k-chord----block-and-file)
  - [Column Blocks](#column-blocks)
  - [^O Chord -- Onscreen Format](#o-chord----onscreen-format)
  - [^Q Chord -- Quick Functions](#q-chord----quick-functions)
  - [^P Chord -- Print Controls and Styles](#p-chord----print-controls-and-styles)
- [Complete Modern (CUA) Input Handler Reference](#complete-modern-cua-input-handler-reference)
  - [Ctrl+Letter Keys (Modern)](#ctrlletter-keys-modern)
  - [Alt Prefix Chords (Modern)](#alt-prefix-chords-modern)
  - [Special / Navigation Keys (Modern)](#special--navigation-keys-modern)
  - [Alt+K Chord -- Block Operations](#altk-chord----block-operations)
  - [Alt+Q Chord -- Navigation and Deletion](#altq-chord----navigation-and-deletion)
  - [Alt+O Chord -- Onscreen Formatting](#alto-chord----onscreen-formatting)
  - [Alt+P Chord -- Style Formatting](#altp-chord----style-formatting)

---

## Quick-View Tables

### Single Ctrl+Letter Keys (A-Z)

Every Ctrl+letter key has a different meaning in WordStar vs Modern mode.

| Key | WordStar Action | Modern (CUA) Action |
|-----|----------------|-------------------|
| Ctrl+A | Word left | Select All |
| Ctrl+B | Not bound (layout engine reflows on every edit, so there's no manual "reformat" step) | **Bold** toggle |
| Ctrl+C | Page down | **Copy** to clipboard |
| Ctrl+D | Cursor right | Select **Font** |
| Ctrl+E | Cursor up | **Center** align paragraph |
| Ctrl+F | Word right | **Find** |
| Ctrl+G | Delete (forward) | **Goto** page |
| Ctrl+H | Delete (backspace) | Find & **Replace** |
| Ctrl+I | Insert tab | **Italic** toggle |
| Ctrl+J | -- (unassigned in WS7) | **Justify** align paragraph |
| Ctrl+K | Block/File prefix (chord) | Set Begin Block |
| Ctrl+L | Find Again | **Left** align paragraph |
| Ctrl+M | Macro/Insert prefix (chord) | -- |
| Ctrl+N | Line break (Enter) | **New** document |
| Ctrl+O | Format prefix (chord) | **Open** file |
| Ctrl+P | Style prefix (chord) | **Print** |
| Ctrl+Q | Quick nav prefix (chord) | -- |
| Ctrl+R | Page up | **Right** align paragraph |
| Ctrl+S | Cursor left | **Save** file |
| Ctrl+T | Delete word right | Insert tab |
| Ctrl+U | Undo | **Underline** toggle |
| Ctrl+V | Toggle insert/overwrite | **Paste** from clipboard |
| Ctrl+W | Scroll up | Close (**Window**) |
| Ctrl+X | Cursor down | Cut (copy + delete) |
| Ctrl+Y | Delete line | Redo |
| Ctrl+Z | Scroll down | Undo |

### Additional Modern Shortcuts

| Key | Action |
|-----|--------|
| Ctrl+Shift+S | Save As |
| Ctrl+Shift+Z | Redo (alternative) |

### Special Keys (Both Modes)

Navigation and editing keys work identically in both modes.

| Key | Action |
|-----|--------|
| Up / Down / Left / Right | Cursor movement |
| Ctrl+Left / Ctrl+Right | Word left / right |
| Home / End | Start / end of line |
| Ctrl+Home / Ctrl+End | Start / end of document |
| Page Up / Page Down | Page up / down |
| Ctrl+Page Up / Ctrl+Page Down | Scroll up / down |
| Delete | Delete forward |
| Ctrl+Delete | Delete word right |
| Backspace | Delete backward |
| Ctrl+Backspace | Delete word left |
| Tab | Insert tab |
| Enter | Line break |
| Escape | Cancel chord mode |

### Function Keys

| Key | WordStar | Modern |
|-----|----------|--------|
| F1 | Contextual help ("press F1, then the command you want help with"); F1 F1 changes the help level | System Preferences |
| F3 | -- | Find Again |
| F11 | Toggle fullscreen | Toggle fullscreen |

F1 is WordStar 7's own real Help key (confirmed against `docs/WordStar 7
Manual.pdf`, "Onscreen Help" and "Change Help Level"), unlike F1/F3/F11's
other jobs here, none of which are real WordStar bindings (see the WordStar
4.0 manual: F1-F10 each carried 4 assignments, but every one was just a
shortcut to a command also reachable via the Ctrl-diamond). WordStar mode's
F1 and Modern mode's F1 now differ deliberately -- Modern/CUA mode isn't
trying to emulate WordStar 7 and keeps System Preferences on F1 unchanged.

**On macOS, F1/F3/F11 are unreliable by design and should not be relied on.**
macOS reserves these at the OS level: F1/F2 default to brightness (never reach
any app unless Fn is held or *System Settings > Keyboard > "Use F1, F2, etc.
keys as standard function keys"* is enabled), and F3/F11 are global system
shortcuts (Mission Control / Show Desktop) that are intercepted before any
app -- including this one -- ever sees the keypress, Fn or not.

WordTsar's GUI therefore leads with macOS-native shortcuts for these three
instead, with the F-key kept only as a bonus for anyone who has freed it
(except Preferences, which moved fully to &#8984;, now that F1 has a real
WordStar job of its own):

| Function | Primary (macOS) | Also works if freed in System Settings |
|----------|------------------|------------------------------------------|
| Preferences | &#8984;, (Cmd+Comma) | -- (F1 no longer does this in WordStar mode) |
| Help | &#8984;/ (Cmd+Slash) | F1 |
| Toggle Fullscreen | &#8984;&#8963;F (Cmd+Ctrl+F) | F11 |
| Find Again | &#8984;G (Cmd+G, both input modes) | F3 (Modern mode only) |

To free F3 or F11 for direct use, go to *System Settings > Keyboard >
Keyboard Shortcuts* and disable/reassign Mission Control (F3) or Show Desktop
(F11). These Cmd-chords are GUI-only -- a terminal app has no way to receive
Cmd-key combinations at all, since the terminal emulator (Terminal.app,
iTerm2, etc.) consumes them for its own shortcuts before `ws` ever sees them.
The TUI's primary path for Help is the real F1 (which does reach `ws`, unlike
Cmd-chords); Preferences in the TUI is now menu-only (File or Utilities menu),
since it no longer has a keyboard shortcut of its own.

---

## Chord Sequences

In WordStar mode, chords use Ctrl-prefix (e.g., ^K,B). In Modern mode, the same operations use Alt-prefix (e.g., Alt+K,B). In WordStar mode, Alt+letter also works as an alternative to Ctrl+letter for entering chord mode. Entries marked "WS only" have no Modern Alt equivalent; entries marked "Modern only" have no WordStar Ctrl equivalent.

### K Chord -- Block and File Operations

| Key | WordStar | Modern | Description |
|-----|----------|--------|-------------|
| B | ^K,B | Alt+K,B | Mark start of block selection |
| K | ^K,K | Alt+K,K | Mark end of block selection |
| C | ^K,C | Alt+K,C | Copy block to cursor position |
| V | ^K,V | Alt+K,V | Move block to cursor position |
| Y | ^K,Y | Alt+K,Y | Delete selected block |
| H | ^K,H | Alt+K,H | Toggle block visibility |
| < | ^K,< | Alt+K,< | Clear block selection |
| U | ^K,U | Alt+K,U | Restore previous block |
| [ | ^K,[ | -- | Paste from system clipboard (WS only) |
| ] | ^K,] | -- | Copy to system clipboard (WS only) |
| " | ^K," | Alt+K," | Convert block to uppercase |
| ' | ^K,' | Alt+K,' | Convert block to lowercase |
| . | ^K,. | Alt+K,. | Convert block to sentence case |
| Z,A / Z,D | ^K,Z,A / ^K,Z,D | -- | Sort block ascending/descending (WS only) |
| N | ^K,N | -- | Column block mode on or off (WS only). See Column Blocks |
| I | ^K,I | -- | Column replace mode on or off (WS only). See Column Blocks |
| ? | ^K,? | Alt+K,? | Word/character count for block |
| 0-9 | ^K,0-9 | Alt+K,0-9 | Save cursor position to marker |
| R | ^K,R | -- | Insert file at cursor (WS only) |
| W | ^K,W | -- | Write block to file (WS only) |
| S | ^K,S | -- | Save current file (WS only) |
| T | ^K,T | -- | Save file with new name (WS only) |
| D | ^K,D | -- | Save and Close (WS only) |
| X | ^K,X | -- | Save file then quit (WS only) |
| Q | ^K,Q | -- | Abandon without saving (WS only) |
| P | ^K,P | -- | Print the document (WS only) |

### Q Chord -- Quick Navigation and Deletion

| Key | WordStar | Modern | Description |
|-----|----------|--------|-------------|
| S | ^Q,S | -- | Move to start of line (WS only) |
| D | ^Q,D | -- | Move to end of line (WS only) |
| R | ^Q,R | -- | Move to start of document (WS only) |
| C | ^Q,C | -- | Move to end of document (WS only) |
| E | ^Q,E | Alt+Q,E | Move to top-left of screen |
| X | ^Q,X | Alt+Q,X | Move to bottom-right of screen |
| B | ^Q,B | Alt+Q,B | Move to start of block |
| K | ^Q,K | Alt+Q,K | Move to end of block |
| F | ^Q,F | -- | Open find dialog (WS only) |
| A | ^Q,A | -- | Open find and replace dialog (WS only) |
| G | ^Q,G | Alt+Q,G | Jump to character position |
| H | ^Q,H | Alt+Q,H | Jump backward to character |
| I | ^Q,I | -- | Jump to page number (WS only) |
| P | ^Q,P | Alt+Q,P | Return to previous position |
| V | ^Q,V | Alt+Q,V | Go to last find/replace location |
| = | ^Q,= | Alt+Q,= | Jump to font tag |
| U | ^Q,U | Alt+Q,U | Force re-layout document |
| L | ^Q,L | -- | Spell check from the cursor to the end of the document (WS only) |
| N | ^Q,N | -- | Spell check current word (WS only) |
| O | ^Q,O | -- | Add word to dictionary (WS only) |
| J | ^Q,J | -- | Look up synonyms for word at cursor (WS only) |
| Space/DEL | ^Q,Space | Alt+Q,Space | Delete to start of line |
| Y | ^Q,Y | Alt+Q,Y | Delete to end of line |
| T | ^Q,T | Alt+Q,T | Delete to specified character |
| 0-9 | ^Q,0-9 | Alt+Q,0-9 | Jump to saved marker |

### O Chord -- Onscreen Formatting and Display

| Key | WordStar | Modern | Description |
|-----|----------|--------|-------------|
| D | ^O,D | Alt+O,D | Toggle control code display |
| C | ^O,C | Alt+O,C | Insert center tab stop |
| V | ^O,V | -- | Center text vertically on the page (WS only) |
| ] | ^O,] | Alt+O,] | Insert right-aligned tab stop |
| T | ^O,T | Alt+O,T | Toggle page/continuous mode (GUI); center view (TUI) |
| J | ^O,J | Alt+O,J | Toggle justification on/off |
| < | ^O,< | -- | Left align paragraph (bracketed) (WS only; Modern uses Ctrl+L) |
| > | ^O,> | -- | Right align paragraph (bracketed) (WS only; Modern uses Ctrl+R) |
| = | ^O,= | -- | Center paragraph (bracketed) (WS only; Modern uses Ctrl+E) |
| + | ^O,+ | -- | Justify paragraph (bracketed) (WS only; Modern uses Ctrl+J) |
| G | ^O,G | -- | Temporary indent (WS only) |
| Y | ^O,Y | Alt+O,Y | Open page layout dialog |
| P | ^O,P | -- | Print preview (WS only) |
| ? | ^O,? | -- | Display status/memory info (WS only) |
| B | ^O,B | -- | Screen Settings dialog (WS only) |
| E | ^O,E | -- | Insert soft hyphen (WS only) |
| H | ^O,H | -- | Toggle auto-hyphenation on/off (WS only) |
| W | ^O,W | -- | Toggle word wrap on/off (WS only) |
| A | ^O,A | -- | Status line says "Auto-align is always on" (WS only) |
| N | ^O,N | -- | Notes: F, E, D, G run; other note keys report not implemented (WS only) |
| L R X I O U F S K M Z # | ^O,L etc. | -- | Report not implemented (WS only) |

### P Chord -- Style and Print Formatting

| Key | WordStar | Modern | Description |
|-----|----------|--------|-------------|
| B | ^P,B | -- | Toggle bold (WS only; Modern uses Ctrl+B) |
| S | ^P,S | -- | Toggle underline (WS only; Modern uses Ctrl+U) |
| Y | ^P,Y | -- | Toggle italic (WS only; Modern uses Ctrl+I) |
| V | ^P,V | Alt+P,V | Toggle subscript |
| T | ^P,T | Alt+P,T | Toggle superscript |
| X | ^P,X | Alt+P,X | Toggle strikethrough |
| K | ^P,K | Alt+P,K | Mark for Index |
| =/+ | ^P,= | -- | Open font selection dialog (WS only; Modern uses Ctrl+D) |
| - | ^P,- | Alt+P,- | Open color selection dialog |

### M Chord -- Macros and Insertion (WordStar Only)

The M chord has no Alt+letter equivalent in Modern mode, but Alt+M does work as a terminal-safe alternate entry (see Additional Alt Shortcuts below). These bindings are only available in WordStar mode. `^J` itself is unassigned, matching real WordStar 7 -- its contextual help moved to `F1` (see Function Keys above).

| Key | WordStar | Description |
|-----|----------|-------------|
| @ | ^M,@ | Insert current date |
| ! | ^M,! | Insert current time |
| * | ^M,* | Insert current filename |
| : | ^M,: | Reports an invalid command (no drive letter on macOS) |
| . | ^M,. | Insert current directory path |
| \ | ^M,\ | Insert full path + filename |
| P | ^M,P | Play a macro |
| R | ^M,R | Start recording a macro; again to stop |
| D | ^M,D | Open a macro file as text |
| E | ^M,E | Rename a macro |
| O | ^M,O | Copy a macro |
| Y | ^M,Y | Delete a macro |
| S | ^M,S | Single-step a macro (not yet implemented) |

In `ws`, ^M arrives as Enter, so the chord there is Option+M (the terminal sends `ESC m`; see Additional Alt Shortcuts below).

### Macros

| Key | WordStar | Modern | Description |
|-----|----------|--------|-------------|
| Record | ^M,R | Utilities > Macros > Record | Start recording. The status line says "Recording" |
| Stop | ^M,R or Alt+= | Alt+= | Stop recording and ask for a name. The macro is saved as `<name>.wsm` in the `macros` folder next to the open document if that folder exists, otherwise in the `macros` folder next to config.ini |
| Pause | Alt+- | Alt+- | While recording, add a pause; otherwise Alt+- types – and Alt+= types ≠ |
| Play | ^M,P | Utilities > Macros > Play... | List the macros in the `macros` folder next to the open document, then those in the `macros` folder next to config.ini, and play the chosen one |
| Edit | ^M,D | Utilities > Macros > Edit... | Open the macro file as a text document |
| Rename | ^M,E | Utilities > Macros > Rename... | Rename the macro in its folder |
| Copy | ^M,O | Utilities > Macros > Copy... | Copy the macro under a new name into the same folder |
| Delete | ^M,Y | Utilities > Macros > Delete... | Delete the macro after asking |

In the GUI, Alt is the Option key (Option+- and Option+=). In `ws`, Alt+- and Alt+= arrive as `ESC -` and `ESC =`.

A macro file is WordStar 7 macro source: `Sub MAIN`, `Key("...")` lines, `PauseForInput("{Enter}", 'Type, then press Enter')` for a pause, `End Sub`, then one ^Z. In a key string, ^KB is a control key or a chord, {Enter}, {Tab}, {Left}, {Ctrl+Left} and {Alt+T} name keys, other characters are typed, and `""`, `{{`, `}}` and `^^` stand for one `"`, `{`, `}` or `^`. A `.txt` macro with one key per line in a `macros` folder is converted to `.wsm` the first time a macro list shows it. Playback stops at a pause and the status line says "Type, then press Enter": what you type goes into the document, Enter continues the macro and Esc cancels it. One undo removes a whole playback. Any other statement (IfException, Goto, a label, PlayMacro) stops playback with "Unsupported line N". Most menu commands and dialogs are not recorded; the Find and Replace answers are, and a Find or Replace from the menu is recorded as ^QF or ^QA (^F or ^H in Modern mode). Real WordStar 7 plays a WordTsar macro after it has been opened once in Macros > Edit/Create and saved.

### Additional Alt Shortcuts

| Key | WordStar Mode | Modern Mode |
|-----|---------------|-------------|
| Alt+K | Enter ^K chord mode | Enter Alt+K chord mode |
| Alt+Q | Enter ^Q chord mode | Enter Alt+Q chord mode |
| Alt+O | Enter ^O chord mode | Enter Alt+O chord mode |
| Alt+P | Enter ^P chord mode | Enter Alt+P chord mode |
| Alt+M | Enter ^M chord mode | -- |
| Alt+U | Redo | -- (Modern uses Ctrl+Y) |
| Alt+- | Macro pause while recording; otherwise types – | Macro pause while recording; otherwise types – |
| Alt+= | Stop recording a macro; otherwise types ≠ | Stop recording a macro; otherwise types ≠ |

**These Alt shortcuts are TUI-only, and need a terminal setting most terminal
emulators don't enable by default.** `ws` detects Alt/Option the classic
terminal way: the terminal must send `ESC` immediately followed by the
plain key (e.g. `ESC k` for Alt+K) -- see
`src/tui/wordstartui/src/terminaldriverposix.cpp`. On macOS, Terminal.app and
most other terminal emulators do **not** send that sequence for the
Option key out of the box; Option instead composes accented/special
characters, so Alt+K/Q/O/P/M/U silently produce the wrong character
instead of entering a chord. To fix this:
- **Terminal.app**: Preferences > Profiles > Keyboard > enable "Use Option as
  Meta key"
- **iTerm2**: Preferences > Profiles > Keys > General > set "Option Key Acts
  as: Esc+"

This doesn't affect the GUI -- Qt receives Option as a normal modifier key
there regardless of terminal settings, since there's no terminal in between.
It's the same class of macOS platform quirk as the F1/F3/F11 caveat under
Function Keys above, just for Option/Alt instead of function keys, and TUI
instead of GUI.

---

## Menu Mapping

Menu shortcuts shown to the user change based on the active input mode. The functionality is identical -- only the displayed shortcut differs.

Each table lists every item of the menu, in menu order, as it is in the GUI. The State column says whether the item is enabled or greyed. "ws" is the terminal app. A greyed item does nothing when chosen. Where the GUI and ws differ, the State column says so. In ws, some labels are shorter (for example "Left Align Para") and a few differ in Modern mode (for example "Show Formatting" for Command Tags).

A greyed item whose key is implemented, or an enabled item whose key is not, is a known mismatch. They are listed under the tables and in the State column.

### File Menu

| Item | WordStar Shortcut | Modern Shortcut | Action | State |
|------|------------------|----------------|--------|-------|
| Open/Read | -- | Ctrl+O | Load file, replacing the current document | Enabled |
| Save | ^KS | Ctrl+S | Save file | Enabled |
| Save As | ^KT | Ctrl+Shift+S | Save with new name | Enabled |
| Save and Close | ^KD | -- | Save and close. The text interface returns to the Opening screen; the graphical interface clears to a new, unnamed document | Enabled |
| Recent Files | -- | -- | List of recently opened files (a submenu in the GUI, a dialog in ws) | Enabled |
| Print | ^KP | Ctrl+P | Print the document [^print-deviation] | Enabled |
| Print Preview | ^OP | -- | Print preview | Enabled |
| Preferences | -- | -- | System Preferences dialog. The GUI shows Cmd+, | Enabled |
| Exit WordTsar | ^KX | Cmd+Q (GUI), Alt+F4 (ws) | Quit application | Enabled |

[^print-deviation]: ^KP, File > Print and Modern Ctrl+P print the document
without a preview, as in WordStar 7. File > Print does not save the document
first. In the GUI they make the PDF and show the macOS Print dialog. In ws
they run the same code as File > Print: the PDF goes to the default printer
with CUPS `lp`, or the printer is chosen from a list. ^OP and File > Print
Preview open the print preview.

### Edit Menu

| Item | WordStar Shortcut | Modern Shortcut | Action | State |
|------|------------------|----------------|--------|-------|
| Undo | ^U | Ctrl+Z | Undo last action | Enabled |
| Redo | Alt+U (the menu shows Ctrl+Alt+U) | Ctrl+Y | Redo last undo | Enabled |
| Mark Block Beginning | ^KB | Alt+K,B | Set block start | Enabled |
| Mark Block End | ^KK | Alt+K,K | Set block end | Enabled |
| **Move submenu** | | | | |
| -- Block | ^KV | Alt+K,V | Move block to cursor | Enabled |
| **Copy submenu** | | | | |
| -- Block | ^KC | Alt+K,C | Copy block to cursor | Enabled |
| -- From Clipboard | ^K[ | Ctrl+V | Paste from clipboard | Enabled |
| -- To Clipboard | ^K] | Ctrl+C | Copy to clipboard | Enabled |
| **Delete submenu** | | | | |
| -- Block | ^KY | Alt+K,Y | Delete selected block | Enabled |
| -- Word | ^T | Ctrl+Del | Delete word right | Enabled |
| -- Line | ^Y | -- | Delete entire line | Enabled |
| -- Line Left of Cursor | ^Q Del | Alt+Q,DEL | Delete to line start | Enabled |
| -- Line Right of Cursor | ^QY | Alt+Q,Y | Delete to line end | Enabled |
| -- To Character | ^QT | Alt+Q,T | Delete to character | Enabled |
| Mark Previous Block | ^KU | Alt+K,U | Restore previous block | Enabled |
| Find | ^QF | Ctrl+F | Find text | Enabled |
| Find and Replace | ^QA | Ctrl+H | Find and replace | Enabled |
| Find Next | ^L | F3 | Find next occurrence | Enabled |
| Go to Character | ^QG | Alt+Q,G | Jump to character position | Enabled |
| Goto Page | ^QI | Ctrl+G | Jump to page number | Enabled |
| **Go to Marker submenu** | | | | |
| -- 1-9, 0 | ^Q1-^Q9, ^Q0 | Alt+Q,0-9 | Jump to saved position | Enabled |
| **Go to Other submenu** | | | | |
| -- Font Tag | ^Q= | Alt+Q,= | Jump to font tag | Enabled |
| -- Style Tag | ^Q< | -- | Reports not implemented | Greyed |
| -- Note | ^ONG | -- | Go to the next note | Enabled |
| -- Previous Position | ^QP | Alt+Q,P | Return to previous position | Enabled |
| -- Last Find/Replace | ^QV | Alt+Q,V | Go to last search position | Enabled |
| -- Beginning of Block | ^QB | Alt+Q,B | Go to block start | Enabled |
| -- End of Block | ^QK | Alt+Q,K | Go to block end | Enabled |
| -- Document Beginning | ^QR | Ctrl+Home | Go to document start | Enabled |
| -- Document End | ^QC | Ctrl+End | Go to document end | Enabled |
| -- Scroll Continuously Up (ws: Scroll Up) | ^QW | -- | Reports not implemented | Greyed |
| -- Scroll Continuously Down (ws: Scroll Down) | ^QZ | -- | Reports not implemented | Greyed |
| **Set Marker submenu** | | | | |
| -- 1-9, 0 | ^K1-^K9, ^K0 | Alt+K,0-9 | Save position to marker | Enabled |
| Edit Note | ^OND | -- | Edit the note at the cursor | Enabled |
| **Note Options submenu** | | | | |
| -- Convert Note | ^ONV | -- | Reports not implemented | Greyed |
| -- Endnote Location | .pe | -- | Insert a .pe line | Enabled |
| **Editing Settings submenu** | | | | |
| -- Column Block Mode | ^KN | -- | Turn column mode on or off. The GUI item has a check mark when it is on. The ws item has a bullet before the title when it is on | Enabled |
| -- Column Replace Mode | ^KI | -- | Turn column replace on or off. The GUI item has a check mark when it is on. The ws item has a bullet before the title when it is on | Enabled |
| -- Auto Align | ^OA | -- | The key says "Auto-align is always on" | Greyed |
| -- Closes Dialog | ^O Enter | -- | WordStar 7 lets Enter close a dialog. The key reports an invalid command | Greyed |

### View Menu

| Item | WordStar Shortcut | Modern Shortcut | Action | State |
|------|------------------|----------------|--------|-------|
| Print Preview | ^OP | -- | Print preview | Enabled |
| Command Tags (Modern: Show Formatting) | ^OD | Alt+O,D | Toggle control code display | Enabled |
| Block Highlighting | ^KH | Alt+K,H | Toggle block visibility | Enabled |
| Screen Settings | ^OB | -- | Screen Settings dialog | Enabled |
| Switch Modes | ^OT | Alt+O,T | Toggle page/continuous mode (GUI); center view (ws) | Enabled |

### Insert Menu

| Item | WordStar Shortcut | Modern Shortcut | Action | State |
|------|------------------|----------------|--------|-------|
| Page Break | .pa | -- | Insert a .pa line | Enabled |
| Today's Date | ^M@ | -- | Insert current date | Enabled |
| **Other Value submenu** | | | | |
| -- Current Time | ^M! | -- | Insert current time | Enabled |
| -- Last Math Result | ^M= | -- | Reports not implemented | Greyed |
| -- Last Math Expression | ^M# | -- | Reports not implemented | Greyed |
| -- Last Math as Dollar | ^M$ | -- | Reports not implemented | Greyed |
| -- Current Filename | ^M* | -- | Insert filename | Enabled |
| -- Current Directory | ^M. | -- | Insert directory path | Enabled |
| -- Current Path | ^M\ | -- | Insert full path | Enabled |
| **Variable submenu** | | | | |
| -- Date, Time, Page, Filename, Drive, Directory, Path, Word Count | &&@&&, &&!&&, &&#&&, &&*&&, &&:&&, &&.&&, &&\&&, &&?&& | -- | Insert variable placeholders | Enabled |
| Extended Character | ^P0 | -- | Reports not implemented | Greyed |
| File | ^KR | -- | Insert file at cursor | Enabled |
| File at Print Time | .fi | -- | Insert a .fi line | Greyed |
| Graphic | ^P* | -- | Reports not implemented | Greyed |
| **Note submenu** | | | | |
| -- Comment | ^ONC | -- | Reports not implemented | Greyed |
| -- Footnote | ^ONF | -- | Open a new footnote | Enabled |
| -- Endnote | ^ONE | -- | Open a new endnote | Enabled |
| -- Annotation | ^ONA | -- | Reports not implemented | Greyed |
| **Index/TOC Entry submenu** | | | | |
| -- TOC Entry | .tc | -- | Ask for the text, insert a .tc line | Enabled |
| -- Index Entry | ^ONI | -- | Ask for the text, insert an .ix line. The key is invalid in a note editor | Enabled |
| -- Mark for Index | ^PK | Alt+P,K | Insert an index mark | Enabled |
| -- Dot Leader to Tab | ^P. | -- | Reports not implemented | Greyed |
| Par. Outline Number (ws: Outline Number) | ^OZ | -- | Reports not implemented | Greyed |

### Style Menu

| Item | WordStar Shortcut | Modern Shortcut | Action | State |
|------|------------------|----------------|--------|-------|
| Bold | ^PB | Ctrl+B | Toggle bold | Enabled |
| Italic | ^PY | Ctrl+I | Toggle italic | Enabled |
| Underline | ^PS | Ctrl+U | Toggle underline | Enabled |
| Font | ^P= | Ctrl+D | Select font | Enabled |
| **Other submenu** | | | | |
| -- Strikeout | ^PX | Alt+P,X | Toggle strikethrough | Enabled |
| -- Subscript | ^PV | Alt+P,V | Toggle subscript | Enabled |
| -- Superscript | ^PT | Alt+P,T | Toggle superscript | Enabled |
| -- Doublestrike | ^PD | -- | Reports not implemented | Greyed |
| -- Color | ^P- | Alt+P,- | Select text color | Enabled |
| Select Paragraph Style | ^OFS | -- | Reports not implemented | Greyed |
| Return to Previous Style | ^OFP | -- | Reports not implemented | Greyed |
| Define Paragraph Style | ^OFD | -- | Reports not implemented | Greyed |
| **Manage Paragraph Styles submenu** | | | | |
| -- Copy Style to Library | ^OFO | -- | Reports not implemented | Greyed |
| -- Delete Library Style | ^OFY | -- | Reports not implemented | Greyed |
| -- Rename Library Style | ^OFR | -- | Reports not implemented | Greyed |
| -- Rename Document Style | ^OFE | -- | Reports not implemented | Greyed |
| **Convert Case submenu** | | | | |
| -- Uppercase | ^K" | Alt+K," | Block to uppercase | Enabled |
| -- Lowercase | ^K' | Alt+K,' | Block to lowercase | Enabled |
| -- Sentence Case | ^K. | Alt+K,. | Block to sentence case | Enabled |

### Layout Menu

| Item | WordStar Shortcut | Modern Shortcut | Action | State |
|------|------------------|----------------|--------|-------|
| Center Line | ^OC | Alt+O,C | Insert center tab | Enabled |
| Right Align Line | ^O] | Alt+O,] | Insert right tab | Enabled |
| Left Align Paragraph | ^O< | Ctrl+L | Bracket paragraph with .oj left | Enabled |
| Center Paragraph | ^O= | Ctrl+E | Bracket paragraph with .oj center | Enabled |
| Right Align Paragraph | ^O> | Ctrl+R | Bracket paragraph with .oj right | Enabled |
| Justify Paragraph | ^O+ | Ctrl+J | Bracket paragraph with .oj justify | Enabled |
| Ruler Line | ^OL | -- | Reports not implemented | Greyed |
| Columns | ^OU | -- | Reports not implemented | Greyed |
| Page | ^OY | Alt+O,Y | Page layout dialog | Enabled |
| **Headers/Footers submenu** | | | | |
| -- Header | .he | -- | Insert a .he line | Enabled |
| -- Footer | .fo | -- | Insert a .fo line | Enabled |
| Page Numbering | ^O# | -- | Reports not implemented | Greyed |
| Line Numbering | .l# | -- | Insert a .l# line | Greyed |
| Alignment and Spacing | ^OS | -- | Reports not implemented | Greyed |
| **Special Effects submenu** | | | | |
| -- Overprint Character | ^PH | -- | Reports not implemented | Greyed |
| -- Overprint Line | ^P Enter | -- | The key reports an invalid command | Greyed |
| -- Optional Hyphen | ^OE | -- | Insert a soft hyphen | Enabled |
| -- Vertically Center Text on Page | ^OV | -- | Center text vertically on the page | Enabled |
| -- Keep Word Together on Line | ^PO | -- | Reports not implemented | Greyed |
| -- Keep Lines Together on Page | .cp | -- | Ask for the number of lines, insert a .cp n line. Nothing is inserted for Cancel or a number that is not a positive whole number | Enabled |
| -- Keep Lines Together on Column | .cc | -- | Insert a .cc line | Greyed |

In Modern mode Center Line and Right Align Line show Alt+O,C and Alt+O,] in both apps.

### Utilities Menu

| Item | WordStar Shortcut | Modern Shortcut | Action | State |
|------|------------------|----------------|--------|-------|
| Spell Check Global | ^QL | -- | Spell check from the cursor to the end of the document. The item sends ^QL | Enabled |
| **Spell Check Other submenu** | | | | |
| -- Rest of Document | ^QL | -- | Spell check from the cursor to the end of the document. The item sends ^QL | Enabled |
| -- Word | ^QN | -- | Check current word | Enabled |
| -- Type Word | ^QO | -- | Enter word for dictionary | Enabled |
| Thesaurus | ^QJ | -- | Look up synonyms for the word at the cursor | Enabled |
| Inset | ^P& | -- | Reports not implemented | Greyed |
| Calculator | ^QM | -- | Reports not implemented | Greyed |
| Block Math | ^KM | -- | Reports not implemented | Greyed |
| **Sort Block submenu** | | | | |
| -- Ascending | ^KZA | -- | Sort the block ascending | Enabled |
| -- Descending | ^KZD | -- | Sort the block descending | Enabled |
| Word Count | ^K? | Alt+K,? | Count words in block | Enabled |
| Generate Table of Contents | -- | -- | Write name.TOC and name.TO1-TO9 from a chosen file's .tc entries | Enabled in the GUI; not in the ws menu |
| Generate Index | -- | -- | Write name.IDX from a chosen file's .ix entries and index marks | Enabled in the GUI; not in the ws menu |
| **Macros submenu** | | | | |
| -- Record | ^MR | -- | Start recording a macro | Enabled while no macro records or plays |
| -- Stop Recording | ^MR | Alt+= | Stop recording and ask for a name | Enabled while recording |
| -- Play | ^MP | -- | Play a macro | Enabled while no macro records or plays |
| -- Edit | ^MD | -- | Open a macro file as text | Enabled unless a macro plays |
| -- Rename | ^ME | -- | Rename a macro | Enabled unless a macro plays |
| -- Copy | ^MO | -- | Copy a macro | Enabled unless a macro plays |
| -- Delete | ^MY | -- | Delete a macro | Enabled unless a macro plays |
| -- Single Step | ^MS | -- | Reports not implemented | Greyed |
| **Merge Print Commands submenu** | | | | |
| -- Data File, Name Variables, Set Variable, Set Variable to Math Result, Ask for Variable | .df, .rv, .sv, .ma, .av | -- | Insert the dot command | Greyed |
| -- If, Else, End If, Go to Top, Go to Bottom | .if, .el, .ei, .go t, .go b | -- | Insert the dot command | Greyed |
| -- Clear Screen While Printing, Display Message, Print File n Times | .cs, .dm, .rp | -- | Insert the dot command | Greyed |
| **Reformat submenu** | | | | |
| -- Rest of Document | ^QU | Alt+Q,U | Reformat/relayout | Enabled |
| -- Rest of Notes | ^ONU | -- | Reports not implemented | Greyed |
| System Preferences | -- | -- | Full configuration dialog. The GUI shows Cmd+, | Enabled |

### Help Menu

| Item | WordStar Shortcut | Modern Shortcut | Action | State |
|------|------------------|----------------|--------|-------|
| Help | F1 | -- | Same as pressing F1 | Enabled in ws; not in the GUI menu |
| Change Help Level | -- | -- | Ask for a help level (0-4) | Enabled in ws; not in the GUI menu |
| About WordTsar | -- | -- | Version and license info | Enabled |

Contextual per-command help is `F1` (WordStar mode only), matching real WordStar 7. Press `F1`, then any command key, to see a description of that command; press `F1` twice to change the help level. See Function Keys above.

### Known menu and key mismatches

- Generate Table of Contents and Generate Index: in the GUI Utilities menu, not in the ws Utilities menu. ws runs them from the Opening screen.

---

## Detailed Binding Reference

### How Modern Mode Works

Modern (CUA) mode reassigns all 26 Ctrl+letter keys to the standard shortcuts most modern editors use. Operations that lose their Ctrl+letter binding (block operations, navigation, formatting) are available through **Alt-prefix chords**:

- **Alt+K** replaces ^K (block/file operations)
- **Alt+Q** replaces ^Q (quick navigation and deletion)
- **Alt+O** replaces ^O (onscreen formatting)
- **Alt+P** replaces ^P (style formatting)

Example workflow in Modern mode:
1. Alt+K, B -- Mark block start (same as ^KB in WordStar)
2. Move cursor to end of desired selection
3. Alt+K, K -- Mark block end (same as ^KK in WordStar)
4. Ctrl+C -- Copy to clipboard (Modern binding)
5. Move to paste location
6. Ctrl+V -- Paste from clipboard (Modern binding)

### Paragraph Alignment (Bracket Behavior)

Both WordStar ^O chords and Modern Ctrl keys bracket the current paragraph with `.oj` dot commands:

- **Before the paragraph**: Inserts `.oj` command for the requested alignment (e.g., `.oj c` for center)
- **After the paragraph**: Inserts `.oj` restoration command matching the next text paragraph's alignment (or `.oj off` if no next text paragraph exists)

This ensures only the target paragraph's alignment changes; subsequent paragraphs are unaffected.

**Toggle behavior**: If the paragraph is already bracketed with `.oj` commands and the requested alignment matches, both bracket commands are removed (toggle off). If the paragraph has the requested alignment through inheritance (no brackets), `.oj off` is inserted before it and the restoration command after.

### Input Mode Switching

The keyboard mode is configured in **System Preferences > Editor > Keyboard Mode**. Switching modes at runtime:
- Changes the active input handler (WordStar or Modern)
- Switches the menu provider to show appropriate shortcut labels
- Rebuilds all menus with updated shortcut labels
- Does not affect document content or cursor position

### Unassigned Keys

Some Ctrl+letter keys have no binding in Modern mode:
- Ctrl+K (without Alt) -- marks block start as a convenience
- Ctrl+M, Ctrl+Q -- unassigned (return unhandled)

### Shared Bindings

The following bindings are identical in both modes:
- All special keys (arrows, Home/End, Page Up/Down, Delete, Backspace, Tab, Enter, Escape)
- Ctrl+modifier variants of special keys (Ctrl+Left, Ctrl+Right, Ctrl+Home, etc.)
- F11 (Toggle fullscreen)
- Direct character input (typing regular text)

F1 is **not** shared: it's contextual help in WordStar mode (matching real WordStar 7) and System Preferences in Modern mode (Modern/CUA mode isn't trying to emulate WordStar 7's key layout). See Function Keys above.

---

## Complete WordStar Input Handler Reference

Every key recognized by the WordStar input handler. Implemented column: **Yes** = works, **Partly** = works with a gap that the Action column names, **No** = not implemented (the key reports "not implemented" or an invalid command, as the Action column says), **--** = unassigned.

### Single Control Keys (^A-^Z)

| Key | WS7 Function | WordTsar Action | Implemented |
|-----|-------------|-----------------|-------------|
| ^A | Word left | Move cursor left one word | Yes |
| ^B | Reformat paragraph | Not bound -- the layout engine reflows every paragraph against current margins on every relayout, so there is no stale state to fix | -- |
| ^C | Page down | Move cursor down one page | Yes |
| ^D | Cursor right | Move cursor right | Yes |
| ^E | Cursor up | Move cursor up one line | Yes |
| ^F | Word right | Move cursor right one word | Yes |
| ^G | Delete character | Delete character at cursor | Yes |
| ^H | Backspace | Delete character before cursor | Yes |
| ^I | Insert tab | Insert tab | Yes |
| ^J | (unassigned in WordStar 7) | -- | -- |
| ^K | Block/File prefix | Enter ^K chord mode | Yes |
| ^L | Find again | Find next match | Yes |
| ^M | Macro Menu prefix | Enter ^M chord mode | Yes |
| ^N | Line break | Insert line break | Yes |
| ^O | Format prefix | Enter ^O chord mode | Yes |
| ^P | Style prefix | Enter ^P chord mode | Yes |
| ^Q | Quick nav prefix | Enter ^Q chord mode | Yes |
| ^R | Page up | Move cursor up one page | Yes |
| ^S | Cursor left | Move cursor left | Yes |
| ^T | Delete word right | Delete word to the right | Yes |
| ^U | Undo | Undo last action | Yes |
| ^V | Toggle insert/overwrite | Toggle insert/overwrite mode | Yes |
| ^W | Scroll up | Scroll up one line | Yes |
| ^X | Cursor down | Move cursor down one line | Yes |
| ^Y | Delete line | Delete current line | Yes |
| ^Z | Scroll down | Scroll down one line | Yes |
| Escape | Cancel chord mode | Cancel current chord mode | Yes |

### Alt Shortcuts (WordStar Mode)

These provide alternative entry to chord modes (useful in terminals where Ctrl is intercepted). On macOS, these need a terminal setting that isn't on by default -- see the caveat under Additional Alt Shortcuts above.

| Key | Action | Implemented |
|-----|--------|-------------|
| Alt+K | Enter ^K chord mode | Yes |
| Alt+Q | Enter ^Q chord mode | Yes |
| Alt+O | Enter ^O chord mode | Yes |
| Alt+P | Enter ^P chord mode | Yes |
| Alt+M | Enter ^M chord mode | Yes |
| Alt+U | Redo last action | Yes |
| Alt+- | Macro pause while recording; otherwise types – | Yes |
| Alt+= | Stop recording a macro; otherwise types ≠ | Yes |

### Special / Navigation Keys

| Key | Modifier | WS7 Function | WordTsar Action | Implemented |
|-----|----------|-------------|-----------------|-------------|
| Up | -- | Cursor up | Move cursor up one line | Yes |
| Down | -- | Cursor down | Move cursor down one line | Yes |
| Left | -- | Cursor left | Move cursor left | Yes |
| Left | Ctrl | Word left | Move cursor left one word | Yes |
| Right | -- | Cursor right | Move cursor right | Yes |
| Right | Ctrl | Word right | Move cursor right one word | Yes |
| Home | -- | Start of line | Move cursor to start of line | Yes |
| Home | Ctrl | Start of document | Move cursor to start of document | Yes |
| End | -- | End of line | Move cursor to end of line | Yes |
| End | Ctrl | End of document | Move cursor to end of document | Yes |
| Page Up | -- | Page up | Move cursor up one page | Yes |
| Page Up | Ctrl | Scroll up | Scroll up | Yes |
| Page Down | -- | Page down | Move cursor down one page | Yes |
| Page Down | Ctrl | Scroll down | Scroll down | Yes |
| Delete | -- | Delete forward | Delete character at cursor | Yes |
| Delete | Ctrl | Delete word right | Delete word to the right | Yes |
| Backspace | -- | Delete backward | Delete character before cursor | Yes |
| Backspace | Ctrl | Delete word left | Delete word to the left | Yes |
| Tab | -- | Insert tab | Insert tab | Yes |
| Enter | -- | Line break | Insert line break | Yes |
| F1 | -- | Contextual help | Press F1, then a command, for a description of it; F1 F1 changes the help level | Yes |
| F2 | -- | -- | -- | -- |
| F3 | -- | -- | -- | -- |
| F4 | -- | -- | -- | -- |
| F5 | -- | -- | -- | -- |
| F6 | -- | -- | -- | -- |
| F7 | -- | -- | -- | -- |
| F8 | -- | -- | -- | -- |
| F9 | -- | -- | -- | -- |
| F10 | -- | -- | -- | -- |
| F11 | -- | Toggle fullscreen | Toggle fullscreen | Yes |
| F12 | -- | -- | -- | -- |

### F1 Contextual Help

Not a chord (F1 is a function key, not `^`-anything), but this is where WordStar 7's real per-command help and help-level toggle live now -- see Function Keys above. `F1,<letter>` shows a short description of that single control key; `F1 F1` prompts for and applies a new help level (0-4). For the K/Q/O/P/M chord prefixes, `F1,<prefix>,<sub-letter>` (e.g. `F1,K,B`) waits for a second key and describes that specific chord command instead of the bare chord-entry keystroke -- covers every sub-letter each chord actually implements (see that chord's own table below).

### ^M Chord -- Macros and Insertion

| Key | WS7 Function | WordTsar Action | Implemented |
|-----|-------------|-----------------|-------------|
| ^M@ | Insert date | Insert current date | Yes |
| ^M! | Insert time | Insert current time | Yes |
| ^M* | Insert filename | Insert current filename | Yes |
| ^M: | Insert drive letter | Reports an invalid command | No |
| ^M. | Insert directory | Insert current directory | Yes |
| ^M\ | Insert full path | Insert full file path | Yes |
| ^M= | Insert math result | Reports not implemented | No |
| ^M# | Insert math expression | Reports not implemented | No |
| ^M$ | Insert math as dollar | Reports not implemented | No |
| ^MP | Play macro | Play the chosen macro | Yes |
| ^MR | Record macro | Start recording; ^MR again stops and asks for a name | Yes |
| ^MD | Edit/create macro | Open the chosen macro file as a text document | Yes |
| ^MS | Single-step macro | Reports not implemented | No |
| ^ME | Rename macro | Rename the chosen macro | Yes |
| ^MO | Copy macro | Copy the chosen macro under a new name | Yes |
| ^MY | Delete macro | Delete the chosen macro | Yes |

### ^K Chord -- Block and File

| Key | WS7 Function | WordTsar Action | Implemented |
|-----|-------------|-----------------|-------------|
| ^KB | Mark block start | Mark block start | Yes |
| ^KK | Mark block end | Mark block end | Yes |
| ^KC | Copy block | Copy block to cursor | Yes |
| ^KV | Move block | Move block to cursor | Yes |
| ^KY | Delete block | Delete marked block | Yes |
| ^KH | Toggle block display | Toggle block highlight | Yes |
| ^K< | Unmark block | Remove block markers | Yes |
| ^KU | Mark previous block | Restore previous block | Yes |
| ^K[ | Paste from clipboard | Paste from clipboard | Yes |
| ^K] | Copy to clipboard | Copy to clipboard | Yes |
| ^K" | Uppercase block | Convert block to uppercase | Yes |
| ^K' | Lowercase block | Convert block to lowercase | Yes |
| ^K. | Sentence case block | Convert block to sentence case | Yes |
| ^K? | Word count block | Count words in block | Yes |
| ^K0-9 | Set marker 0-9 | Set position marker 0-9 | Yes |
| ^KR | Insert file at cursor | Insert file at cursor | Yes |
| ^KW | Write block to disk | Write marked block to a file | Yes |
| ^KS | Save file | Save file | Yes |
| ^KT | Save as | Save file as | Yes |
| ^KD | Save and Close | Save file and close it. The text interface returns to the Opening screen; the graphical interface clears to a new, unnamed document | Yes |
| ^KX | Save and exit | Save file and exit | Yes |
| ^KQ | Abandon file | Abandon changes and close | Yes |
| ^KP | Print | Print the document | Yes |
| ^KO | Copy file | Reports not implemented | No |
| ^KE | Rename file | Reports not implemented | No |
| ^KJ | Delete file | Reports not implemented | No |
| ^K\ | Fax | Reports not implemented | No |
| ^KL | Change drive/directory | Reports not implemented | No |
| ^KF | Run DOS command | Reports not implemented | No |
| ^KM | Block math | Reports not implemented | No |
| ^KZ | Sort block | Sort block, ascending (^KZA) or descending (^KZD) | Yes |
| ^KN | Column mode on/off | Turns column mode on or off. See Column Blocks below. Not done: ^KR as a column, a rectangle marked with the mouse, and ^KW, ^K" ^K' ^K. and ^K? on a column block | Partly |
| ^KI | Column replace on/off | Turns column replace on or off. It only matters while column mode is on. See Column Blocks below | Partly |
| ^KA | Copy between windows | Reports not implemented | No |
| ^KG | Move between windows | Reports not implemented | No |

### Column Blocks

^KN turns column mode on. The block is then the rectangle between the ^KB and ^KK markers, not the text between them. The status line shows Column.

With column mode on, ^KC, ^KV, ^KY, ^K] and ^KZ work on the rectangle. Each of ^KC, ^KV and ^KY is one undo step and marks the result as the block. ^K] copies the rows as plain lines. ^KZ sorts the whole lines of the rows by their part in the rectangle.

Dot command lines are never changed or copied by a column command. In a line with tabs, the rectangle takes whole cells and one separator tab, so tab tables keep their columns. A line without tabs is cut at character positions.

^KI turns column replace on. The status line shows Column-Repl. A copy then overwrites the text at the cursor, and a move or delete leaves a hole at the source. With it off, a copy pushes the text at the cursor to the right, and a move or delete closes the gap.

Both modes are off at start, are one setting for every window, and are not saved. The macro settings ColMode and ColReplace (also written ColRepl) turn them on or off, and AutoRestore puts them back when the macro ends.

Not done: ^KR as a column, a rectangle marked with the mouse, and ^KW, the case commands and word count on a column block. They use the marked text, not the rectangle.

### ^O Chord -- Onscreen Format

| Key | WS7 Function | WordTsar Action | Implemented |
|-----|-------------|-----------------|-------------|
| ^OC | Center line | Center current line | Yes |
| ^O] | Right align line | Right align current line | Yes |
| ^OD | Toggle command tags | Toggle control code display | Yes |
| ^OT | Toggle page/continuous | GUI: page/continuous mode. TUI: centers the editing pane and ruler horizontally instead (no pixel canvas to paginate) | Yes |
| ^OJ | Toggle justification | Toggle justification | Yes |
| ^OY | Page layout dialog | Open page layout dialog | Yes |
| ^OP | Print preview | Print preview | Yes |
| ^O? | Status/memory info | Show about/status info | Yes |
| ^OB | Screen settings | Open screen settings | Yes |
| ^O< | Left align paragraph | Left align paragraph (bracketed) | Yes |
| ^O= | Center paragraph | Center paragraph (bracketed) | Yes |
| ^O> | Right align paragraph | Right align paragraph (bracketed) | Yes |
| ^O+ | Justify paragraph | Justify paragraph (bracketed) | Yes |
| ^OL | Set left margin | Reports not implemented | No |
| ^OR | Set right margin | Reports not implemented | No |
| ^OG | Temporary indent | Indent current paragraph one more tab stop, reverting after it | Yes |
| ^OX | Release margin | Reports not implemented | No |
| ^OI | Set/clear tabs | Reports not implemented | No |
| ^OO | Ruler from text | Reports not implemented | No |
| ^OU | Column layout | Reports not implemented | No |
| ^OF | Paragraph styles | Reports not implemented | No |
| ^OS | Set line spacing | Reports not implemented | No |
| ^OV | Vertically center | Center cursor-to-page-break text on the page | Yes |
| ^OE | Soft hyphen | Insert a soft hyphen | Yes |
| ^OH | Auto-hyphenation on/off | Toggle automatic hyphenation | Yes |
| ^OA | Auto-align on/off | Status line says "Auto-align is always on"; it cannot be turned off | Partly |
| ^OW | Word wrap on/off | Toggles word wrap (inserts .aw on/off) | Yes |
| ^O(space) | (none) | Opens the Screen Settings dialog, as ^OB | No |
| ^O Enter | Enter closes dialog on/off | Reports an invalid command | No |
| ^OK | Open/switch window | Reports not implemented | No |
| ^OM | Size window | Reports not implemented | No |
| ^OZ | Paragraph number | Reports not implemented | No |
| ^O# | Page numbering | Reports not implemented | No |
| ^ON | Notes | Waits for a note key: F new footnote, E new endnote, D edit the note at the cursor, G go to the next note, I ask for an index entry and insert an .ix line. A, C, V, U and the other note keys report not implemented | Partly |

### ^Q Chord -- Quick Functions

| Key | WS7 Function | WordTsar Action | Implemented |
|-----|-------------|-----------------|-------------|
| ^QF | Find | Find text | Yes |
| ^QA | Find and replace | Find and replace text | Yes |
| ^QE | Move to screen top-left | Move cursor to screen top-left | Yes |
| ^QX | Move to screen bottom-right | Move cursor to screen bottom-right | Yes |
| ^QR | Move to document start | Move cursor to document start | Yes |
| ^QC | Move to document end | Move cursor to document end | Yes |
| ^QB | Move to block start | Move cursor to block start | Yes |
| ^QK | Move to block end | Move cursor to block end | Yes |
| ^QS | Move to line start | Move cursor to start of line | Yes |
| ^QD | Move to line end | Move cursor to end of line | Yes |
| ^QP | Previous position | Go to previous cursor position | Yes |
| ^QV | Last find/replace | Go to last find/replace location | Yes |
| ^Q= | Next font tag | Go to next font tag | Yes |
| ^QG | Goto character forward | Go to character (forward) | Yes |
| ^QH | Goto character backward | Go to character (backward) | Yes |
| ^QI | Goto page | Go to page number | Yes |
| ^QU | Reformat document | Reformat entire document | Yes |
| ^QL | Spell check document | Spell check from the cursor to the end of the document | Yes |
| ^QN | Spell check word | Spell check word at cursor | Yes |
| ^QO | Add word to dictionary | Add word to spell dictionary | Yes |
| ^Q0-9 | Goto marker 0-9 | Go to position marker 0-9 | Yes |
| ^Q(space) | Delete line left | Delete from cursor to line start | Yes |
| ^Q(DEL) | Delete line left | Delete from cursor to line start | Yes |
| ^QY | Delete line right | Delete from cursor to line end | Yes |
| ^QT | Delete to character | Delete to specified character | Yes |
| ^Q< | Next style | Reports not implemented | No |
| ^QM | Math | Reports not implemented | No |
| ^QJ | Thesaurus | Look up synonyms for word at cursor | Yes |
| ^QW | Scroll up repeat | Reports not implemented | No |
| ^QZ | Scroll down repeat | Reports not implemented | No |

### ^P Chord -- Print Controls and Styles

| Key | WS7 Function | WordTsar Action | Implemented |
|-----|-------------|-----------------|-------------|
| ^PB | Bold | Toggle bold | Yes |
| ^PS | Underline | Toggle underline | Yes |
| ^PY | Italic | Toggle italic | Yes |
| ^PV | Subscript | Toggle subscript | Yes |
| ^PT | Superscript | Toggle superscript | Yes |
| ^PX | Strikethrough | Toggle strikethrough | Yes |
| ^PK | Mark for Index | Insert an index mark | Yes |
| ^P= | Select font | Select font | Yes |
| ^P+ | Select font (alt) | Select font | Yes |
| ^P- | Select color | Select color | Yes |
| ^PD | Double strike | Reports not implemented | No |
| ^PN | Normal font | Reports not implemented | No |
| ^PA | Alternate font | Reports not implemented | No |
| ^PH | Overprint character | Reports not implemented | No |
| ^P(space) | Overprint line | Reports not implemented | No |
| ^PF | Phantom space | Reports not implemented | No |
| ^PG | Phantom rubout | Reports not implemented | No |
| ^P* | Graphics tag | Reports not implemented | No |
| ^P& | Start inset | Reports not implemented | No |
| ^PO | Keep word together on line | Reports not implemented | No |
| ^PC | Print pause | Reports not implemented | No |
| ^PI | 8-column tab | Reports not implemented | No |
| ^P. | Dot leader | Reports not implemented | No |
| ^P0 | Extended characters | Reports not implemented | No |
| ^PQ | Custom font Q | Reports not implemented | No |
| ^PW | Custom font W | Reports not implemented | No |
| ^PE | Custom font E | Reports not implemented | No |
| ^PR | Custom font R | Reports not implemented | No |
| ^P! | Custom font ! | Reports not implemented | No |
| ^P? | Select printer | Reports not implemented | No |

---

## Complete Modern (CUA) Input Handler Reference

Every key recognized by the Modern (CUA) input handler. Modern mode uses standard CUA shortcuts (Ctrl+C/V/X/Z) and Alt-prefix chords for advanced operations.

### Ctrl+Letter Keys (Modern)

| Key | Standard Function | WordTsar Action | Implemented |
|-----|-------------|-----------------|-------------|
| Ctrl+A | Select all | Select entire document | Yes |
| Ctrl+B | Bold | Toggle bold | Yes |
| Ctrl+C | Copy | Copy to clipboard | Yes |
| Ctrl+D | Font | Select font | Yes |
| Ctrl+E | Center paragraph | Center paragraph (bracketed) | Yes |
| Ctrl+F | Find | Find text | Yes |
| Ctrl+G | Goto page | Go to page number | Yes |
| Ctrl+H | Replace | Find and replace text | Yes |
| Ctrl+I | Italic | Toggle italic | Yes |
| Ctrl+J | Justify paragraph | Justify paragraph (bracketed) | Yes |
| Ctrl+K | Mark block start | Mark block start | Yes |
| Ctrl+L | Left align paragraph | Left align paragraph (bracketed) | Yes |
| Ctrl+M | -- | -- | -- |
| Ctrl+N | New document | New document | Yes |
| Ctrl+O | Open file | Open file | Yes |
| Ctrl+P | Print | Print the document | Yes |
| Ctrl+Q | -- | -- | -- |
| Ctrl+R | Right align paragraph | Right align paragraph (bracketed) | Yes |
| Ctrl+S | Save | Save file | Yes |
| Ctrl+Shift+S | Save As | Save file as | Yes |
| Ctrl+T | Insert tab | Insert tab | Yes |
| Ctrl+U | Underline | Toggle underline | Yes |
| Ctrl+V | Paste | Paste from clipboard | Yes |
| Ctrl+W | Close | Close document | Yes |
| Ctrl+X | Cut | Cut to clipboard | Yes |
| Ctrl+Y | Redo | Redo last action | Yes |
| Ctrl+Z | Undo | Undo last action | Yes |
| Ctrl+Shift+Z | Redo (alt) | Redo last action | Yes |
| Escape | Cancel chord mode | Cancel current chord mode | Yes |

### Alt Prefix Chords (Modern)

These activate chord modes identical to WordStar's ^K, ^Q, ^O, ^P groups.

| Key | Action | Implemented |
|-----|--------|-------------|
| Alt+K | Enter block/marker chord mode | Yes |
| Alt+Q | Enter navigation/deletion chord mode | Yes |
| Alt+O | Enter onscreen formatting chord mode | Yes |
| Alt+P | Enter style/print chord mode | Yes |
| Alt+- | Macro pause while recording; otherwise types – | Yes |
| Alt+= | Stop recording a macro; otherwise types ≠ | Yes |

### Special / Navigation Keys (Modern)

Same as WordStar mode, plus F3 = Find Again.

| Key | Modifier | Action | Implemented |
|-----|----------|--------|-------------|
| Up | -- | Move cursor up one line | Yes |
| Down | -- | Move cursor down one line | Yes |
| Left | -- | Move cursor left | Yes |
| Left | Ctrl | Move cursor left one word | Yes |
| Right | -- | Move cursor right | Yes |
| Right | Ctrl | Move cursor right one word | Yes |
| Home | -- | Move cursor to start of line | Yes |
| Home | Ctrl | Move cursor to start of document | Yes |
| End | -- | Move cursor to end of line | Yes |
| End | Ctrl | Move cursor to end of document | Yes |
| Page Up | -- | Move cursor up one page | Yes |
| Page Up | Ctrl | Scroll up | Yes |
| Page Down | -- | Move cursor down one page | Yes |
| Page Down | Ctrl | Scroll down | Yes |
| Delete | -- | Delete character at cursor | Yes |
| Delete | Ctrl | Delete word to the right | Yes |
| Backspace | -- | Delete character before cursor | Yes |
| Backspace | Ctrl | Delete word to the left | Yes |
| Tab | -- | Insert tab | Yes |
| Enter | -- | Insert line break | Yes |
| F1 | -- | Open System Preferences | Yes |
| F2 | -- | -- | -- |
| F3 | -- | Find next match | Yes |
| F4 | -- | -- | -- |
| F5 | -- | -- | -- |
| F6 | -- | -- | -- |
| F7 | -- | -- | -- |
| F8 | -- | -- | -- |
| F9 | -- | -- | -- |
| F10 | -- | -- | -- |
| F11 | -- | Toggle fullscreen | Yes |
| F12 | -- | -- | -- |

### Alt+K Chord -- Block Operations

| Key | Action | Implemented |
|-----|--------|-------------|
| Alt+K, B | Mark block start | Yes |
| Alt+K, K | Mark block end | Yes |
| Alt+K, C | Copy block to cursor | Yes |
| Alt+K, V | Move block to cursor | Yes |
| Alt+K, Y | Delete marked block | Yes |
| Alt+K, H | Toggle block highlight | Yes |
| Alt+K, < | Remove block markers | Yes |
| Alt+K, U | Restore previous block | Yes |
| Alt+K, " | Convert block to uppercase | Yes |
| Alt+K, ' | Convert block to lowercase | Yes |
| Alt+K, . | Convert block to sentence case | Yes |
| Alt+K, ? | Count words in block | Yes |
| Alt+K, 0-9 | Set position marker 0-9 | Yes |

### Alt+Q Chord -- Navigation and Deletion

| Key | Action | Implemented |
|-----|--------|-------------|
| Alt+Q, E | Move cursor to screen top-left | Yes |
| Alt+Q, X | Move cursor to screen bottom-right | Yes |
| Alt+Q, B | Move cursor to block start | Yes |
| Alt+Q, K | Move cursor to block end | Yes |
| Alt+Q, P | Go to previous cursor position | Yes |
| Alt+Q, V | Go to last find/replace location | Yes |
| Alt+Q, = | Go to next font tag | Yes |
| Alt+Q, G | Go to character (forward) | Yes |
| Alt+Q, H | Go to character (backward) | Yes |
| Alt+Q, U | Reformat entire document | Yes |
| Alt+Q, Y | Delete from cursor to line end | Yes |
| Alt+Q, T | Delete to specified character | Yes |
| Alt+Q, 0-9 | Go to position marker 0-9 | Yes |
| Alt+Q, Space | Delete from cursor to line start | Yes |
| Alt+Q, DEL | Delete from cursor to line start | Yes |

### Alt+O Chord -- Onscreen Formatting

| Key | Action | Implemented |
|-----|--------|-------------|
| Alt+O, C | Center current line | Yes |
| Alt+O, ] | Right align current line | Yes |
| Alt+O, D | Toggle control code display | Yes |
| Alt+O, T | Toggle page/continuous mode (GUI); center view (TUI) | Yes |
| Alt+O, J | Toggle justification | Yes |
| Alt+O, Y | Open page layout dialog | Yes |

### Alt+P Chord -- Style Formatting

| Key | Action | Implemented |
|-----|--------|-------------|
| Alt+P, V | Toggle subscript | Yes |
| Alt+P, T | Toggle superscript | Yes |
| Alt+P, X | Toggle strikethrough | Yes |
| Alt+P, K | Mark for Index | Yes |
| Alt+P, - | Select color | Yes |
