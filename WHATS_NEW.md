# What's New

Release history for WordTsar, in reverse chronological order.

## 0.11.0 Beta (2026-09-04)

### Menu items now match the keyboard, in both keyboard modes

Fixed: some menu items could trigger the wrong command when using the Modern/CUA keyboard mode instead of WordStar mode. Menu clicks now always do what their label says, in either mode.

### Other fixes and changes

- **^K R / Insert File** now inserts the chosen file at the cursor, matching real WordStar 7 — use **File > Open** to replace the whole document instead.
- **Sentence Case (^K .)** now capitalizes the first letter of each sentence and lowercases the rest, instead of capitalizing every word.
- **New dot command `.pc`**: sets the column where automatic page numbers print, instead of always centering them.
- Removed several menu items that had no effect when clicked (Mark Text for Index, Spell Check Rest of Notes, Column Break, Language Change, Repeat Keystroke, Note Options Starting Number/Convert at Print, Style Settings, Current Drive, Variable > Line), along with Reformat Paragraph (^B) — not needed, since WordTsar reflows text automatically as you type.
- The terminal UI's Header, Footer, Keep Lines Together, and Switch Modes menu items now work — these already worked from the keyboard, they just weren't reachable from the menu.

## 0.10.3 Beta (2026-09-02)

### DOCX import fixes

- Fixed: a table whose first row used certain Word formatting (content controls) could import with the entire table missing.
- Fixed: multi-level numbered lists using combined numbering (e.g., "1.1.") could show literal placeholder text instead of real numbers, and a restarted list level could ignore its intended starting number.
- Fixed: an empty table-of-contents or index entry produced a blank line in the generated file.

## 0.10.2 Beta (2026-09-01)

### Table of Contents and Index entries now insert real text

**Insert > TOC Entry** and **Insert > Index Entry** now prompt you for the entry text, matching the WordStar 7 manual. Previously these inserted an empty placeholder.

## 0.10.1 Beta (2026-09-01)

### GUI's Open/Save dialogs now start in your default folder

Fixed: the GUI's Open and Save dialogs opened somewhere unexpected instead of your configured default folder (or `~/Documents`). The terminal UI already did this correctly.

## 0.10.0 Beta (2026-09-01)

### Real table of contents and index generation

The terminal UI's Opening Menu **I** (index) and **T** (table of contents) commands now work. Pick a document, and every `.tc`/`.ix` entry in it is collected and written to a real output file — `name.TOC` (or `name.T01`–`name.T09` for numbered tables) and `name.IDX` — which then opens for editing.

- Table of contents entries support a `#` placeholder that's replaced with the real page number, and leading-space indentation.
- Index entries support `+` (bold page number), `-` (cross-reference, no page number), and `,` (sub-entry).
- Not yet supported: marking an index entry by selecting text (`^PK`) rather than typing a `.ix` line, "Index Every Word" auto-indexing, and page-range/odd-even filters.

## 0.9.0 Beta (2026-09-01)

### DOCX import: real tables, numbered lists, and tab stops

- Tables now import with their actual content — previously a table produced an empty placeholder marker.
- Numbered and bulleted lists now keep their numbers/bullets on import, including multi-level lists. (Since WordStar has no live numbering field, the numbers won't automatically renumber if you edit the list afterward.)
- Custom tab stops (centered, right-aligned, decimal-aligned) now carry over correctly instead of being dropped.
- Fixed a crash opening some DOCX files with unusual style definitions.
- Paragraph borders and shading are still not supported — WordStar has no equivalent formatting to convert them to.

## 0.8.0 Beta (2026-09-01)

### Terminal UI now finds your installed fonts correctly on macOS

Fixed: the terminal UI (`ws`) couldn't find installed fonts on macOS, so printing always used a generic substitute instead of your actual font.

### New splash screen

Both the GUI and terminal UI open with updated splash artwork.

## 0.7.6 Beta (2026-09-01)

### Fixed: garbled accented characters in terminal UI PDF printing/preview

Printing or previewing a document from the terminal UI could corrupt accented characters (ü, ä, ö) and curly quotes in the resulting PDF. Fixed.

## 0.7.5 Beta (2026-09-01)

### Help and keyboard layout corrected to match real WordStar 7

Contextual help now lives on **F1** — press F1, then any command key, for a one-line description. **F1 F1** cycles through 5 help detail levels. Date/time/filename/macro insertion moved to **^M** (the real WordStar 7 Macro Menu). On the GUI, **⌘/** also opens help, and Preferences moved to **⌘,**. See [KEY_MAPPING.md](KEY_MAPPING.md) for the full reference.

### Terminal UI Opening Menu now matches real WordStar 7

The letter grid now matches WordStar 7 exactly. **P** prints a file directly from the menu; **?** shows version, current directory, and free disk space.

### Fixed in this release

- The Opening Menu's key-letter color is now easier to read on a dark terminal background.
- Date/time/filename/macro insertion from several GUI and terminal UI menu items had silently stopped working after the ^M change above — fixed.

## 0.6.1 Beta (2026-08-31)

### Real printing, not just preview

**File > Print** on the GUI now opens the system print dialog and prints directly. The terminal UI's `^KP` now picks a default printer automatically when there's only one, and prompts otherwise.

### Reliable Preferences, Find Again, and Fullscreen shortcuts on macOS

macOS reserves F1/F2, F3, and F11 for its own use, so they didn't always reach the app. **⌘,**, **⌘⌃F**, and **⌘G** are now the primary shortcuts for Preferences, Fullscreen, and Find Again; the F-keys still work if you've freed them in System Settings.

### Spell-check language now applies correctly on macOS

Fixed: the Spell Check Language preference was ignored.

### WordStar comment lines survive Save As Word

`..` and `.IG` comment lines used to be dropped when saving as `.docx`. They're now preserved and round-trip correctly.

### Also fixed

- Splash screen and main window now open centered on the same screen.
- About dialog now shows the correct version, this fork's GitHub link, and an accurate list of included libraries.
- New documents default to `~/Documents` instead of your home folder, unless you've set a different default in Preferences.

## 0.6.0 Beta (2026-08-30) — macOS fork

This fork trims WordTsar down to a macOS-only build. See the [README](README.md) for the full framing; the highlights:

### Save As Word (.docx)

WordTsar can now write `.docx` files, not just read them. **File > Save As** offers Word format alongside WordStar and RTF. Tables and headers/footers aren't covered yet.

### Terminal UI, buildable again

The `ws` terminal UI builds alongside the GUI. See [BUILDING.md](BUILDING.md#build-targets) for how to choose between the GUI, the TUI, or both.

### Fixed

- A crash in the macOS spell checker when adding a word to the dictionary.

### Removed

- Windows and Linux project files, build scripts, and packaging assets. Full cross-platform support remains available from the original project at [wordtsar.ca](http://wordtsar.ca).

## 0.5.1804 Alpha (2026-03-01)

### New Layout and Rendering Engines

Completely rewritten layout and rendering engines for improved performance, accuracy, and future capabilities.

Performance difference is good. Version 0.3 took about 800ms to lay out a full novel of around 110,000 words. Version 0.5 does the same novel in about 430ms.

### Wordwrap

Wordwrap now behaves a little more like Word or LibreOffice Writer, etc. WordStar would not wrap a solid line of characters — a single really long line would be made. We now wrap at the right margin instead of doing that.

### Recent Files

Recent files are remembered for easy file loading.

### Keyboard handlers

- WordStar keyboard handler (default)

- Microsoft Word/CUA keyboard handler **(very much untested)**

Changing keyboard handlers does not change how WordTsar works — it's still a WordStar clone. So don't expect MS Word style selection block handling, etc.

### New layout/formatting commands

| Keys | Menu | Action | MS Word (CUA) Matches |
| - | - | - | - |
| ^O\< | Alt-L-L | Left Justify Paragraph Toggle | ^L |
| ^O= | Alt-L-N | Center Justify Paragraph Toggle | ^E |
| ^O\> | Alt-L-G | Right Align Paragraph Toggle | ^R |
| ^O+ | Alt-L-J | Justify Paragraph Toggle | ^J |

### Editor Color Configuration

Full color configuration of editor components.

### System Preferences

New comprehensive preferences dialog. Accessible from File Menu (Alt-F-E) or F1. If your terminal (XFCE Terminal for example) intercepts F1, you'll have to edit the terminal preferences.

### Configuration File

New configuration file. No data is copied from the old configuration to the new.

- Linux: `$XDG\_CONFIG\_HOME/wordtsar/config.ini` → `~/.config/wordtsar/config.ini` → `./config.ini`

- macOS: `~/Library/Application Support/WordTsar/config.ini` → `./config.ini`

- Windows: `%APPDATA%\\WordTsar\\config.ini` → `.\\config.ini`

The old config file \<HOME\_DIR\>/WordTsar.ini is not copied and it is not deleted.

### Undo/Redo

Full undo and redo support. ^U for Undo, Ctrl+Alt+U for Redo.

### Page Mode

New page display mode shows documents with visual page breaks. Toggle between page and continuous modes with ^OT. A Show Formatting pane can be opened and closed with ^OD (a cheap WordPerfect Reveal Codes). Not feature complete.

### Page Numbering

- Page number formats: Arabic (1, 2, 3), Roman lower (i, ii, iii), Roman upper (I, II, III)

- Set custom starting page number

### New Dot Commands

- .lh (Line Height)

- .aw (Auto Word Wrap)

- .op (Omit Page Numbers)

- .pl (Page Length)

- .pn (Page Number with format)

- .pr (Printer Orientation)

- .pg (Print Page Numbers)

- .sr (Sub/Superscript Roll)

- .cp (Conditional Page Break)

- .tb (Extended Tab Stops with type prefixes)

- .rr (Extended Ruler with type prefixes)

### Variable Expansion

Variables now expand during display and in headers/footers:

- `&@&` - Current date

- `&!&` - Current time

- `&\#&` - Page number

- `&\_&` - Line number

- `&\*&` - Filename

- `&:&` - Drive letter

- `&.&` - Directory

- `&\\&` - Full path

### RTF Import/Export

Large improvements to RTF import and export including Unicode support, paragraph formatting, header/footer preservation, and codepage handling.

### Spell Check

- Spell check individual word ^QN

- Spell check typed in word ^QO

### Top Status Bar

Items are clickable to select font, set attributes, and alignment.

### Bottom Status Bar

Insert/Overwrite and Page number are clickable items.

### Backup file changed

Backup file changed from *filename.ws-bak* to *filename-bak.ws* so it can be opened easily by WordTsar.

### International Input

CJK (Japanese, Chinese, Korean) and Thai input method support.

### Terminal Interface

Run from a shell or via ssh. ***Pre-alpha***. There is some mouse support, but it's patchy at best. Some dialogs require mouse and keyboard for navigation and/or selection. Use at your own risk. Executable name is **ws** (GUI executable name is **WordTsar**).

- **note**: TUI is not designed to work over a serial interface to a terminal (such as vt-100, Wyse-60, etc)

### Math Expressions in Dot Commands

Dot commands now support full arithmetic with unit-aware calculations. Use expressions like `.rm 4c + 2i` to mix centimeters and inches, `.po 8 \* 2i` for multiplication, or `.lm +2c` to adjust margins relative to their current value. All margin, page setup, and tab commands support math with inches, centimeters, millimeters, and points. (Limitation: addition and subtraction can mix measurement units, e.g. .5i + 2c. Multiplication and Division cannot, and the dot command will be marked as an error.)

### Wordstar files

If a file being loaded doesn't have a known extension, WordTsar will do its best to figure out if it is a WordStar file and load it. If that fails, it will load as plain text.

If saving as WordStar will result in data loss, a dialog will be displayed.

Known extensions for WordStar files are .ws, .ws3, .ws4, .ws5, .ws6, .ws7, .ws8

### Bug Fixes

Multiple bug fixes, some in the ticket system on Sourceforge, most not.

## Notes

### .rr command

The .rr command currently assumes everything is Courier New 12 point font, so each dash (-) represents 144 twips or 0.1 of an inch (2.54 mm) — the width of a single Courier New 12 point character. As far as we can tell, WordStar 7.0D also uses 0.1 of an inch.

The .rr command has been extended with tab types. ^ is a center align tab, and \> is a right align tab. For example:

`.rr l----------!----------\#----------^-----------!--------\>------------r`

### .tb command

The .tb command has been extended with tab type prefixes. ^ is center tab, \> is right aligned tab, and \# is decimal aligned tab. For example:

`.tb 1.0i ^2.1i 3i \>5i \#6.0i`

Numbers with no measurement specifier (i for inches, etc) default to column positions where each column is assumed to be 144 twips or 0.1 of an inch (2.54 mm) — the width of a single Courier New 12 point character.

## 0.4.1505 Alpha (2025-10-03)

- Core changes and foundational work

- UTF-8 in-memory document storage

- Unicode version 16 support

- Linux, Windows, and macOS builds
