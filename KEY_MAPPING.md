# WordTsar Key and Menu Mapping Reference

This document maps WordStar keyboard commands and menus to their Modern/MS Word equivalents in WordTsar, along with the functionality each binding provides.

WordTsar supports two input modes, selectable in **System Preferences > Editor > Keyboard Mode**:
- **WordStar** (default) -- Classic WordStar 7 control-key sequences
- **Modern (CUA/MS Word)** -- Standard Ctrl+C/V/X/Z shortcuts with Alt-prefix chords for advanced operations

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
  - [^O Chord -- Onscreen Format](#o-chord----onscreen-format)
  - [^Q Chord -- Quick Functions](#q-chord----quick-functions)
  - [^P Chord -- Print Controls and Styles](#p-chord----print-controls-and-styles)
- [Complete Modern/MS Word Input Handler Reference](#complete-modernms-word-input-handler-reference)
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

| Key | WordStar Action | MS Word Action |
|-----|----------------|-------------------|
| Ctrl+A | Word left | Select All |
| Ctrl+B | Reformat paragraph | **Bold** toggle |
| Ctrl+C | Page down | **Copy** to clipboard |
| Ctrl+D | Cursor right | Select **Font** |
| Ctrl+E | Cursor up | **Center** align paragraph |
| Ctrl+F | Word right | **Find** |
| Ctrl+G | Delete (forward) | **Goto** page |
| Ctrl+H | Delete (backspace) | Find & **Replace** |
| Ctrl+I | Insert tab | **Italic** toggle |
| Ctrl+J | Help prefix (chord) | **Justify** align paragraph |
| Ctrl+K | Block/File prefix (chord) | Set Begin Block |
| Ctrl+L | Find Again | **Left** align paragraph |
| Ctrl+M | (not implemented) | -- |
| Ctrl+N | Line break (Enter) | **New** document |
| Ctrl+O | Format prefix (chord) | **Open** file |
| Ctrl+P | Style prefix (chord) | **Print** preview |
| Ctrl+Q | Quick nav prefix (chord) | -- |
| Ctrl+R | Page up | **Right** align paragraph |
| Ctrl+S | Cursor left | **Save** file |
| Ctrl+T | Delete word right | -- |
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
| . | ^K,. | Alt+K,. | Convert block to title case |
| ? | ^K,? | Alt+K,? | Word/character count for block |
| 0-9 | ^K,0-9 | Alt+K,0-9 | Save cursor position to marker |
| R | ^K,R | -- | Open/insert file (WS only) |
| S | ^K,S | -- | Save current file (WS only) |
| T | ^K,T | -- | Save file with new name (WS only) |
| D | ^K,D | -- | Save, clear document, reset (WS only) |
| X | ^K,X | -- | Save file then quit (WS only) |
| Q | ^K,Q | -- | Abandon without saving (WS only) |
| P | ^K,P | -- | Print preview (WS only) |

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
| L | ^Q,L | -- | Spell check entire document (WS only) |
| N | ^Q,N | -- | Spell check current word (WS only) |
| O | ^Q,O | -- | Add word to dictionary (WS only) |
| Space/DEL | ^Q,Space | Alt+Q,Space | Delete to start of line |
| Y | ^Q,Y | Alt+Q,Y | Delete to end of line |
| T | ^Q,T | Alt+Q,T | Delete to specified character |
| 0-9 | ^Q,0-9 | Alt+Q,0-9 | Jump to saved marker |

### O Chord -- Onscreen Formatting and Display

| Key | WordStar | Modern | Description |
|-----|----------|--------|-------------|
| D | ^O,D | Alt+O,D | Toggle control code display |
| C | ^O,C | Alt+O,C | Insert center tab stop |
| ] | ^O,] | Alt+O,] | Insert right-aligned tab stop |
| T | ^O,T | Alt+O,T | Toggle page/continuous mode |
| J | ^O,J | Alt+O,J | Toggle justification on/off |
| < | ^O,< | Alt+O,< | Left align paragraph (bracketed) |
| > | ^O,> | Alt+O,> | Right align paragraph (bracketed) |
| = | ^O,= | Alt+O,= | Center paragraph (bracketed) |
| + | ^O,+ | Alt+O,+ | Justify paragraph (bracketed) |
| Y | ^O,Y | Alt+O,Y | Open page layout dialog |
| P | ^O,P | -- | Print preview (WS only) |
| ? | ^O,? | -- | Display status/memory info (WS only) |
| B | ^O,B | -- | Screen Settings dialog (WS only) |

### P Chord -- Style and Print Formatting

| Key | WordStar | Modern | Description |
|-----|----------|--------|-------------|
| B | ^P,B | -- | Toggle bold (WS only; Modern uses Ctrl+B) |
| S | ^P,S | -- | Toggle underline (WS only; Modern uses Ctrl+U) |
| Y | ^P,Y | -- | Toggle italic (WS only; Modern uses Ctrl+I) |
| V | ^P,V | Alt+P,V | Toggle subscript |
| T | ^P,T | Alt+P,T | Toggle superscript |
| X/K | ^P,X | Alt+P,X | Toggle strikethrough |
| =/+ | ^P,= | -- | Open font selection dialog (WS only; Modern uses Ctrl+D) |
| - | ^P,- | Alt+P,- | Open color selection dialog |

### M Chord -- Macros and Insertion (WordStar Only)

The M chord has no Alt+letter equivalent in Modern mode, but Alt+M does work as a terminal-safe alternate entry (see Additional Alt Shortcuts below). These bindings are only available in WordStar mode. `^J` itself is unassigned, matching real WordStar 7 -- its contextual help moved to `F1` (see Function Keys above).

| Key | WordStar | Description |
|-----|----------|-------------|
| @ | ^M,@ | Insert current date |
| ! | ^M,! | Insert current time |
| * | ^M,* | Insert current filename |
| : | ^M,: | Insert drive letter (Windows) |
| . | ^M,. | Insert current directory path |
| \ | ^M,\ | Insert full path + filename |
| P/R/D/S/E/O/Y | ^M,&lt;letter&gt; | Macro play/record/edit/single-step/rename/copy/delete (not yet implemented) |

### Additional Alt Shortcuts

| Key | WordStar Mode | Modern Mode |
|-----|---------------|-------------|
| Alt+K | Enter ^K chord mode | Enter Alt+K chord mode |
| Alt+Q | Enter ^Q chord mode | Enter Alt+Q chord mode |
| Alt+O | Enter ^O chord mode | Enter Alt+O chord mode |
| Alt+P | Enter ^P chord mode | Enter Alt+P chord mode |
| Alt+M | Enter ^M chord mode | -- |
| Alt+U | Redo | -- (Modern uses Ctrl+Y) |

---

## Menu Mapping

Menu shortcuts shown to the user change based on the active input mode. The functionality is identical -- only the displayed shortcut differs.

### File Menu

| Item | WordStar Shortcut | Modern Shortcut | Action |
|------|------------------|----------------|--------|
| Open/Read | ^KR | Ctrl+O | Load file |
| Save | ^KS | Ctrl+S | Save file |
| Save As | ^KT | Ctrl+Shift+S | Save with new name |
| Save and Close | ^KD | -- | Save, clear, reset |
| Print | ^KP | Ctrl+P | Print to a printer (GUI: system print dialog; TUI: CUPS `lp` on macOS/Linux, print spooler on Windows) |
| Print Preview | ^OP | -- | Print preview |
| Preferences | -- | -- | System Preferences dialog |
| Exit WordTsar | ^KX | Alt+F4 | Quit application |

### Edit Menu

| Item | WordStar Shortcut | Modern Shortcut | Action |
|------|------------------|----------------|--------|
| Undo | ^U | Ctrl+Z | Undo last action |
| Redo | Alt+U | Ctrl+Y | Redo last undo |
| Mark Block Beginning | ^KB | Alt+K,B | Set block start |
| Mark Block End | ^KK | Alt+K,K | Set block end |
| **Move submenu** | | | |
| -- Block | ^KV | Alt+K,V | Move block to cursor |
| **Copy submenu** | | | |
| -- Block | ^KC | Alt+K,C | Copy block to cursor |
| -- From Clipboard | ^K[ | Ctrl+V | Paste from clipboard |
| -- To Clipboard | ^K] | Ctrl+C | Copy to clipboard |
| **Delete submenu** | | | |
| -- Block | ^KY | Alt+K,Y | Delete selected block |
| -- Word | ^T | Ctrl+Del | Delete word right |
| -- Line | ^Y | -- | Delete entire line |
| -- Line Left | ^Q Del | Alt+Q,DEL | Delete to line start |
| -- Line Right | ^QY | Alt+Q,Y | Delete to line end |
| -- To Character | ^QT | Alt+Q,T | Delete to character |
| Mark Previous Block | ^KU | -- | Restore previous block |
| Find | ^QF | Ctrl+F | Find text |
| Find and Replace | ^QA | Ctrl+H | Find and replace |
| Find Next | ^L | F3 | Find next occurrence |
| Go to Character | ^QG | -- | Jump to character position |
| Goto Page | ^QI | -- | Jump to page number |
| **Goto Marker submenu** | | | |
| -- 0-9 | ^Q0-^Q9 | Alt+Q,0-9 | Jump to saved position |
| **Goto Other submenu** | | | |
| -- Font Tag | ^Q= | -- | Jump to font tag |
| -- Previous Position | ^QP | -- | Return to previous pos |
| -- Last Find/Replace | ^QV | -- | Goto last search pos |
| -- Beginning of Block | ^QB | -- | Goto block start |
| -- End of Block | ^QK | -- | Goto block end |
| -- Document Beginning | ^QR | -- | Goto document start |
| -- Document End | ^QC | -- | Goto document end |
| **Set Marker submenu** | | | |
| -- 0-9 | ^K0-^K9 | Alt+K,0-9 | Save position to marker |

### View Menu

| Item | WordStar Shortcut | Modern Shortcut | Action |
|------|------------------|----------------|--------|
| Print Preview | ^OP | -- | Print preview |
| Command Tags | ^OD | -- | Toggle control code display |
| Block Highlighting | ^KH | -- | Toggle block visibility |
| Screen Settings | ^OB | -- | Screen Settings dialog |

### Insert Menu

| Item | WordStar Shortcut | Modern Shortcut | Action |
|------|------------------|----------------|--------|
| Page Break | .pa | -- | Insert page break |
| Today's Date | ^M@ | -- | Insert current date |
| **Other Value submenu** | | | |
| -- Current Time | ^M! | -- | Insert current time |
| -- Current Filename | ^M* | -- | Insert filename |
| -- Current Drive | ^M: | -- | Insert drive letter |
| -- Current Directory | ^M. | -- | Insert directory path |
| -- Current Path | ^M\ | -- | Insert full path |
| **Variable submenu** | | | |
| -- Date/Time/Page/etc. | &&@&&, &&!&&, etc. | -- | Insert variable placeholders |
| File | ^KR | -- | Insert file at cursor |

### Style Menu

| Item | WordStar Shortcut | Modern Shortcut | Action |
|------|------------------|----------------|--------|
| Bold | ^PB | Ctrl+B | Toggle bold |
| Italic | ^PY | Ctrl+I | Toggle italic |
| Underline | ^PS | Ctrl+U | Toggle underline |
| Font | ^P= | Ctrl+D | Select font |
| **Other Styles submenu** | | | |
| -- Strikeout | ^PX | Alt+P,X | Toggle strikethrough |
| -- Subscript | ^PV | Alt+P,V | Toggle subscript |
| -- Superscript | ^PT | Alt+P,T | Toggle superscript |
| -- Color | ^P- | Alt+P,- | Select text color |
| **Convert Case submenu** | | | |
| -- Uppercase | ^K" | Alt+K," | Block to uppercase |
| -- Lowercase | ^K' | Alt+K,' | Block to lowercase |
| -- Sentence Case | ^K. | Alt+K,. | Block to title case |

### Layout Menu

| Item | WordStar Shortcut | Modern Shortcut | Action |
|------|------------------|----------------|--------|
| Center Line | ^OC | Alt+O,C | Insert center tab |
| Right Align Line | ^O] | Alt+O,] | Insert right tab |
| Left Align Paragraph | ^O< | Ctrl+L | Bracket paragraph with .oj left |
| Center Paragraph | ^O= | Ctrl+E | Bracket paragraph with .oj center |
| Right Align Paragraph | ^O> | Ctrl+R | Bracket paragraph with .oj right |
| Justify Paragraph | ^O+ | Ctrl+J | Bracket paragraph with .oj justify |
| Page | ^OY | Alt+O,Y | Page layout dialog |
| **Headers/Footers submenu** | | | |
| -- Header | .he | -- | Insert header |
| -- Footer | .fo | -- | Insert footer |

### Utilities Menu

| Item | WordStar Shortcut | Modern Shortcut | Action |
|------|------------------|----------------|--------|
| Spell Check Global | ^QL | -- | Spell check entire document |
| **Spell Check submenu** | | | |
| -- Rest of Document | ^QL | -- | Spell check from cursor |
| -- Word | ^QN | -- | Check current word |
| -- Type Word | ^QO | -- | Enter word for dictionary |
| Word Count | ^K? | Alt+K,? | Count words in block |
| **Reformat submenu** | | | |
| -- Rest of Document | ^QU | Alt+Q,U | Reformat/relayout |
| System Preferences | -- | -- | Full configuration dialog |

### Help Menu

| Item | WordStar Shortcut | Modern Shortcut | Action |
|------|------------------|----------------|--------|
| About WordTsar | -- | -- | Version and license info |

Contextual per-command help isn't a menu item -- it's `F1` (WordStar mode only), matching real WordStar 7. Press `F1`, then any command key, to see a description of that command; press `F1` twice to change the help level. See Function Keys above.

---

## Detailed Binding Reference

### How Modern Mode Works

Modern/MS Word mode reassigns all 26 Ctrl+letter keys to standard shortcuts that users expect from Microsoft Word and other modern editors. Operations that lose their Ctrl+letter binding (block operations, navigation, formatting) are available through **Alt-prefix chords**:

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
- Ctrl+M, Ctrl+Q, Ctrl+T -- unassigned (return unhandled)

### Shared Bindings

The following bindings are identical in both modes:
- All special keys (arrows, Home/End, Page Up/Down, Delete, Backspace, Tab, Enter, Escape)
- Ctrl+modifier variants of special keys (Ctrl+Left, Ctrl+Right, Ctrl+Home, etc.)
- F11 (Toggle fullscreen)
- Direct character input (typing regular text)

F1 is **not** shared: it's contextual help in WordStar mode (matching real WordStar 7) and System Preferences in Modern mode (Modern/CUA mode isn't trying to emulate WordStar 7's key layout). See Function Keys above.

---

## Complete WordStar Input Handler Reference

Every key recognized by the WordStar input handler. Status column: **Yes** = functional, **No** = not yet implemented, **--** = unassigned.

### Single Control Keys (^A-^Z)

| Key | WS7 Function | WordTsar Action | Implemented |
|-----|-------------|-----------------|-------------|
| ^A | Word left | Move cursor left one word | Yes |
| ^B | Reformat paragraph | Reformat paragraph | No |
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

These provide alternative entry to chord modes (useful in terminals where Ctrl is intercepted).

| Key | Action | Implemented |
|-----|--------|-------------|
| Alt+K | Enter ^K chord mode | Yes |
| Alt+Q | Enter ^Q chord mode | Yes |
| Alt+O | Enter ^O chord mode | Yes |
| Alt+P | Enter ^P chord mode | Yes |
| Alt+M | Enter ^M chord mode | Yes |
| Alt+U | Redo last action | Yes |

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

Not a chord (F1 is a function key, not `^`-anything), but this is where WordStar 7's real per-command help and help-level toggle live now -- see Function Keys above. `F1<letter>` shows a one-line description of that command; `F1 F1` prompts for and applies a new help level (0-4).

### ^M Chord -- Macros and Insertion

| Key | WS7 Function | WordTsar Action | Implemented |
|-----|-------------|-----------------|-------------|
| ^M@ | Insert date | Insert current date | Yes |
| ^M! | Insert time | Insert current time | Yes |
| ^M* | Insert filename | Insert current filename | Yes |
| ^M: | Insert drive letter | Insert drive letter (Windows only) | Yes |
| ^M. | Insert directory | Insert current directory | Yes |
| ^M\ | Insert full path | Insert full file path | Yes |
| ^M= | Insert math result | -- | No |
| ^M# | Insert math expression | -- | No |
| ^M$ | Insert math as dollar | -- | No |
| ^MP | Play macro | -- | No |
| ^MR | Record macro | -- | No |
| ^MD | Edit/create macro | -- | No |
| ^MS | Single-step macro | -- | No |
| ^ME | Rename macro | -- | No |
| ^MO | Copy macro | -- | No |
| ^MY | Delete macro | -- | No |

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
| ^K. | Title case block | Convert block to title case | Yes |
| ^K? | Word count block | Count words in block | Yes |
| ^K0-9 | Set marker 0-9 | Set position marker 0-9 | Yes |
| ^KR | Open/insert file | Open or insert file | Yes |
| ^KS | Save file | Save file | Yes |
| ^KT | Save as | Save file as | Yes |
| ^KD | Save and clear | Save file and clear editor | Yes |
| ^KX | Save and exit | Save file and exit | Yes |
| ^KQ | Abandon file | Abandon changes and close | Yes |
| ^KP | Print preview | Print preview | Yes |
| ^KO | Copy file | -- | No |
| ^KE | Rename file | -- | No |
| ^KJ | Delete file | -- | No |
| ^K\ | Fax | -- | No |
| ^KL | Change drive/directory | -- | No |
| ^KF | Run DOS command | -- | No |
| ^KW | Write block to disk | -- | No |
| ^KM | Block math | -- | No |
| ^KZ | Sort block | -- | No |
| ^KN | Column mode on | -- | No |
| ^KI | Column replace on | -- | No |
| ^KA | Copy between windows | -- | No |
| ^KG | Move between windows | -- | No |

### ^O Chord -- Onscreen Format

| Key | WS7 Function | WordTsar Action | Implemented |
|-----|-------------|-----------------|-------------|
| ^OC | Center line | Center current line | Yes |
| ^O] | Right align line | Right align current line | Yes |
| ^OD | Toggle command tags | Toggle control code display | Yes |
| ^OT | Toggle page/continuous | Toggle page/continuous mode (GUI only) | Yes |
| ^OJ | Toggle justification | Toggle justification | Yes |
| ^OY | Page layout dialog | Open page layout dialog | Yes |
| ^OP | Print preview | Print preview | Yes |
| ^O? | Status/memory info | Show about/status info | Yes |
| ^OB | Screen settings | Open screen settings | Yes |
| ^O< | Left align paragraph | Left align paragraph (bracketed) | Yes |
| ^O= | Center paragraph | Center paragraph (bracketed) | Yes |
| ^O> | Right align paragraph | Right align paragraph (bracketed) | Yes |
| ^O+ | Justify paragraph | Justify paragraph (bracketed) | Yes |
| ^OL | Set left margin | -- | No |
| ^OR | Set right margin | -- | No |
| ^OG | Temporary indent | -- | No |
| ^OX | Release margin | -- | No |
| ^OI | Set/clear tabs | -- | No |
| ^OO | Ruler from text | -- | No |
| ^OU | Column layout | -- | No |
| ^OF | Paragraph styles | -- | No |
| ^OS | Set line spacing | -- | No |
| ^OV | Vertically center | -- | No |
| ^OE | Soft hyphen | -- | No |
| ^OH | Auto-hyphenation off | -- | No |
| ^OA | Auto-align off | -- | No |
| ^OW | Word wrap off | -- | No |
| ^O(space) | (reserved) | -- | No |
| ^OK | Open/switch window | -- | No |
| ^OM | Size window | -- | No |
| ^OZ | Paragraph number | -- | No |
| ^O# | Page numbering | -- | No |
| ^ON | Notes | -- | No |

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
| ^QL | Spell check document | Spell check document | Yes |
| ^QN | Spell check word | Spell check word at cursor | Yes |
| ^QO | Add word to dictionary | Add word to spell dictionary | Yes |
| ^Q0-9 | Goto marker 0-9 | Go to position marker 0-9 | Yes |
| ^Q(space) | Delete line left | Delete from cursor to line start | Yes |
| ^Q(DEL) | Delete line left | Delete from cursor to line start | Yes |
| ^QY | Delete line right | Delete from cursor to line end | Yes |
| ^QT | Delete to character | Delete to specified character | Yes |
| ^Q< | Next style | -- | No |
| ^QM | Math | -- | No |
| ^QJ | Thesaurus | -- | No |
| ^QW | Scroll up repeat | -- | No |
| ^QZ | Scroll down repeat | -- | No |

### ^P Chord -- Print Controls and Styles

| Key | WS7 Function | WordTsar Action | Implemented |
|-----|-------------|-----------------|-------------|
| ^PB | Bold | Toggle bold | Yes |
| ^PS | Underline | Toggle underline | Yes |
| ^PY | Italic | Toggle italic | Yes |
| ^PV | Subscript | Toggle subscript | Yes |
| ^PT | Superscript | Toggle superscript | Yes |
| ^PX | Strikethrough | Toggle strikethrough | Yes |
| ^PK | Strikethrough (alt) | Toggle strikethrough | Yes |
| ^P= | Select font | Select font | Yes |
| ^P+ | Select font (alt) | Select font | Yes |
| ^P- | Select color | Select color | Yes |
| ^PD | Double strike | -- | No |
| ^PN | Normal font | -- | No |
| ^PA | Alternate font | -- | No |
| ^PH | Overprint character | -- | No |
| ^P(space) | Overprint line | -- | No |
| ^PF | Phantom space | -- | No |
| ^PG | Phantom rubout | -- | No |
| ^P* | Graphics tag | -- | No |
| ^P& | Start inset | -- | No |
| ^PO | Binding space | -- | No |
| ^PC | Print pause | -- | No |
| ^PI | 8-column tab | -- | No |
| ^P. | Dot leader | -- | No |
| ^P0 | Extended characters | -- | No |
| ^PQ | Custom font Q | -- | No |
| ^PW | Custom font W | -- | No |
| ^PE | Custom font E | -- | No |
| ^PR | Custom font R | -- | No |
| ^P! | Custom font ! | -- | No |
| ^P? | Select printer | -- | No |

---

## Complete Modern/MS Word Input Handler Reference

Every key recognized by the Modern/MS Word input handler. Modern mode uses standard MS Word shortcuts (Ctrl+C/V/X/Z) and Alt-prefix chords for advanced operations.

### Ctrl+Letter Keys (Modern)

| Key | MS Word Function | WordTsar Action | Implemented |
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
| Ctrl+P | Print preview | Print preview | Yes |
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
| Alt+K, . | Convert block to title case | Yes |
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
| Alt+O, T | Toggle page/continuous mode (GUI only) | Yes |
| Alt+O, J | Toggle justification | Yes |
| Alt+O, Y | Open page layout dialog | Yes |

### Alt+P Chord -- Style Formatting

| Key | Action | Implemented |
|-----|--------|-------------|
| Alt+P, V | Toggle subscript | Yes |
| Alt+P, T | Toggle superscript | Yes |
| Alt+P, X | Toggle strikethrough | Yes |
| Alt+P, K | Toggle strikethrough | Yes |
| Alt+P, - | Select color | Yes |
