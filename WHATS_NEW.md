# What's New

Release history for WordTsar, in reverse chronological order.

## 0.17.0 (2026-11-07)

**Additions**

- ^MP runs WordStar 7 macros (.WSM): Sub, Goto, Call, Return, Stop, PlayMacro, ChainMacro, Key, PauseForInput, PauseForKey, IfException (ACK, QRY, NCM, DEF) and the settings AutoRestore, Insert, CmdTags, HideDots, SetHelpLevel, ColMode, ColReplace and WSQuiet (#83).
- The whole macro, and every macro it plays, is checked first. If it uses something WordTsar does not have, nothing runs and the status line says "Not supported in WordTsar: <command>, line <n>" (#83).
- A runaway macro is stopped: macros played inside each other, Calls nested too deep, or too many statements without a key (#83).
- A macro can type the file name for ^KR and answer a question with the dialog keys up to F10 (#83).
- ^MP lists macro files with .WSM in upper case, too (#83).
- Macros from the Sawyer archive run if they pass the check (#83).
- New macros in templates/macros: CHWORDS and SCWORDS (word count of the chapter or scene), NEXTSC and PREVSC (next or previous # scene break), MARKPARA, DELPARA and MOVEPARA (mark, delete or move the paragraph), PLAINMS (em dashes, ellipses and curly quotes to plain text) and NBSPACE (non-breaking spaces to spaces) (#83).

**Changes**

- Changed: ^K, now works like ^K<: it removes the block markers (#83).
- Changed: NEXTCH and PREVCH keep the cursor in place after the last or before the first chapter (#83).

**Fixes**

- Fixed: a macro stops after a message or question it does not answer (#84).
- Fixed: SETUP, NEXTCH and PREVCH work with dot commands hidden and in page view (#84).
- Fixed: Writing Tools, AutoFill, Dictation and Emoji & Symbols are gone from the Edit menu (#85).

## 0.16.0 (2026-11-06)

**Fixes**

- Fixed: the WordStar index-item and paragraph-number codes are now kept when you open and save a file. They show as <Index> and <Para#> tags when command tags are on (^OD) (#74).
- Fixed: a .tc or .ix line that starts with a dot is now written to the generated file as a dot command. Index ones go at the top (#72).
- Fixed: the generated table of contents keeps formatting codes in its entries (#79).
- Fixed: numbered tables of contents are named name.TO1 to name.TO9, not name.T01 (#79).
- Fixed: page numbers in the table of contents and index come from the print layout, whatever ^OD shows (#79).
- Fixed: dot-command measurements accept units: i, in, ", c, cm, mm, p and pt. Brackets are accepted. With no unit, .lm .rm and .po are in columns, .mt .mb and .pl in lines, and .lh in 1/48 inch (#80).
- Fixed: .pf on, off and dis are accepted (#82).
- Fixed: ^F from the last word of a paragraph goes to the first word of the next paragraph. Before, it moved one character back (#78).
- Fixed: the text between ^K index marks is trimmed of punctuation at both ends (#78).
- Fixed: case conversion and sort keep tabs, bold and raw bytes in the block (#69).
- Fixed: ^KZ over whole lines no longer joins the last sorted line to the next line (#70).
- Fixed: SIGTERM, from kill or from logout, quits with an emergency save and no crash report (#68).
- Fixed: the app bundle now carries the version and the minimum macOS version (#23).

**Changes**

- Changed: ^PK now marks text for the index. Strikethrough is on ^PX only, and the generator reads the marks. Mark for Index is in the Insert menu in the GUI and ws (#73).
- Changed: the GUI Utilities menu has Generate Table of Contents and Generate Index. The files are written next to the source, as in ws (#77).
- Changed: .ul on and off underline or leave out the spaces between underlined words (#81).

**Additions**

- Added: macros. ^MR records a macro and ^MP plays one. While recording, Alt+- adds a pause and Alt+= stops. Playback stops at a pause and Enter continues. One undo removes a whole playback (#78).
- Added: macros are saved as .wsm files in the WordStar 7 macro language (Key("...") and PauseForInput). Real WordStar 7 can play them after you open one in Macros > Edit/Create and save it (#78).
- Added: ^MD edits a macro as text, ^ME renames it, ^MO copies it and ^MY deletes it (#78).
- Added: the Utilities > Macros menu in the GUI and in ws, and macro playback from the ws Opening screen (#78).
- Added: when the macros folder next to config.ini is missing, it is created with the default macros CHAPTER, SCENE, INDEX, NEXTCH and PREVCH (#78).
- Added: a new recording is saved in the macros folder next to the open document, if the document has one (#78).
- Added: a Find or Replace started from the menu while recording is recorded as ^QF or ^QA (#78).
- Added: Alt+- and Alt+= (Option+- and Option+= in the GUI) type an en dash and a not-equal sign when not recording (#78).
- Added: ws trims spaces around a macro name (#78).
- Added: Option+M is the ws key for the ^M chord. Set Option as Meta in the terminal (#78).
- Added: the templates folder, with the book template BOOK.WS and the macros SETUP, CHAPTER, SCENE, INDEX, NEXTCH and PREVCH (#71, #78).

## 0.15.0 (2026-11-05)

**Fixes**

- Fixed: Write Block to File (^KW) from a WordStar document now writes a WordStar file. Before, it always wrote plain text (#56).
- Fixed: a letter followed by a combining mark that the code page can't hold now saves as the letter, not lost. Only the mark is dropped, as in ^KW (#61).
- Fixed: control bytes in a text file now survive open and save unchanged. They show as ^B, ^[ and so on instead of formatting. A tab pasted into a document is now a real tab (#59).
- Fixed: a text file now ends at its first 0x1A. Anything after it is hidden and written back unchanged when you save as text (#50).
- Fixed: spell check Replace no longer doubles punctuation that already follows the word. "evtl.." becomes "evtl." (#17).

**Changes**

- Changed: ^KW has a Copy in ASCII Format check box. Ticked, it strips the formatting and writes plain text with tabs as spaces. The GUI uses the macOS save panel with the box. The TUI has its own dialog with a file name field and the box (#56).
- Changed: ^KW from a text document writes text, with tabs and dot lines as they are. The box shows ticked and cannot be changed (#56).
- Changed: ^KW in append mode to a WordStar file adds the block at the end of the text, without a second header. Appending to another file adds text (#56).
- Changed: ^KW gives the code page warning for every WordStar-format write. If you decline, nothing is written (#56).
- Changed: ^KW append writes a temporary file and renames it over the target, so a failed write leaves the target as it was. The target keeps its ACL and extended attributes (#56).
- Changed: the GUI no longer shows a splash screen at startup. The About box now shows the WordTsar logo beside the same About text as ws, and ends with the Qt version (#64).
- Changed: the app now states the oldest macOS it runs on. That version is taken from Qt (#23).
- Changed: the download now contains Qt, so WordTsar runs on a Mac without Homebrew. The terminal version, ws, is in the zip too.

## 0.14.3 (2026-11-01)

**Fixes**

- Fixed: a WordStar paragraph that starts with a soft tab and then a period now loads as text instead of being read as a dot command (#35).
- Fixed: extended characters written as ESC, a byte below 0x20 and FS now load as the matching graphic character under CP437, CP737 and CP850 (#43).
- Fixed: files that are binaries, plain text or UTF-8 are no longer opened as WordStar documents.
- Fixed: the file type check now also accepts a WordStar 4 word with an accented letter before a tab, ESC, 0x1A or a style toggle.
- Fixed: a WordStar 5 or later header that is larger than the file is no longer accepted as WordStar.
- Fixed: a crash when pasting RTF from the clipboard before a file is opened (#52).
- Fixed: the document no longer flickers behind the file dialog in the GUI, and the TUI file dialog fills the screen (#39, #38).
- Fixed: footnotes, endnotes, annotations and comments now load and save back unchanged (#47).
- Fixed: a file opened as text is saved as text in the GUI and the TUI (#51).
- Fixed: the save format follows the last save with a WordStar or text extension. Saving as RTF or DOCX leaves it as it was (#51).
- Fixed: plain-text saves no longer contain internal marker bytes. Tabs stay tabs (#51).
- Fixed: a tab in a text file now loads as a tab (#51).
- Fixed: a text file keeps its line endings when it is saved. New text files, Write Block to File and clipboard copy use LF (#51).
- Fixed: a text save that fails to write is now reported (#51).
- Fixed: Opening-screen entries without an action are drawn like the other entries (#24).
- Fixed: Alt plus a letter no longer runs Opening Menu commands (#24).
- Fixed: Escape on the Opening screen does nothing. X exits (#24).
- Fixed: Tab on the Opening screen cancels a pending print, index or table of contents request. Choosing one of the three replaces the pending one (#24).
- Fixed: a pull-down menu entry that WordTsar does not implement closes the menu, so the message is not hidden at 24 rows (#24).
- Fixed: an error message such as a failed backup stays visible when WordTsar says an entry is not available, and when you switch between the Opening screen and the edit screen (#24).
- Fixed: terminal keys that WordTsar does not know, such as Shift+Tab and Ctrl+arrows, are ignored. They used to act as Escape (#58).
- Fixed: Alt+Backspace and Alt with an accented letter are ignored instead of acting as Escape (#58).
- Fixed: ESC ESC is read as one Escape (#58).
- Fixed: the arrow keys, Home and End work when the terminal is in application cursor mode (#58).
- Fixed: printing and the printer lookup (lpstat) wait at most one second (#58).
- Fixed: the Opening status box shows free disk space in decimal GB, matching Finder (#60).

**Changes**

- Changed: help level 4 is the default for new configurations. Existing configurations keep their level (#24).
- Changed: at help level 4 the TUI Opening screen shows the menu bar File, Utilities, Additional and Help over a four-column file list, instead of the command panel. F10, Alt+letter and the plain letters F, U, A and H open the menus (#24).
- Changed: the Help menu has Help (F1) and Change Help Level. F1 F1 no longer changes the level at level 4 (#24).
- Changed: Recent Files, System Preferences and About WordTsar are in the File, Utilities and Help menus of the Opening screen (#24).
- Changed: help levels 0 to 3 keep the classic Opening Menu, now in the WordStar 7 layout with its box and title row. G (recent files), V (preferences) and W (about WordTsar) are added (#24).
- Changed: the Opening file list shows sizes in k, M and G, a "/" after folder names, and shortened names that keep the extension (#24).
- Changed: the Opening file list starts in the folder ws was started from, unless Default Directory is set (#24).
- Changed: the Opening screen colours follow the active colour preset (#24).
- Changed: menu hotkey letters and highlighted entries use colour only, with no bold (#55).
- Changed: the Opening file list shows no selection until you use it. Then a highlight bar follows the arrow keys, a click or letter focus. Enter opens the highlighted entry (#24).
- Changed: a click on a file in the Opening file list opens it. At help levels 0 to 3 a click on a panel entry runs it (#24).
- Changed: Opening screen and menu entries that WordTsar does not implement show "Not available in WordTsar" on the status line (#24, #55).
- Changed: the TUI About box starts with "A WordStar 7 clone for the 21st century.", lists the open-source components with their licences and shows the trademark line. It scrolls, the OK button stays visible, and it uses the dialog colours (#58).

**Additions**

- Added: the colour setting "Selected hotkey" in System Preferences, for the hotkey letter of a highlighted menu entry (#55).
- Added: ws shows a startup screen for 1.5 seconds or until the first key. It shows the WordTsar title, the version, the credits, a system line (macOS version, chip, terminal and its size, and the default printer or Printing to PDF), what the app uses from macOS, the licence and the trademark note (#58).
- Added: keys typed during the startup screen go to the Opening screen. When ws starts with a file name they are dropped. Escape only ends the startup screen (#58).
- Added: the preference "Show startup screen" in System Preferences. It is on by default (#58).

## 0.14.2 (2026-10-30)

WordTsar is still Beta, but I think it is a stable version even if there is a lot of work to do to reach 1.0 - which I would call the first release.

**Fixes**

- Fixed: the title bar kept the previous document's name after opening another file, saving under a new name, or closing. It now shows the file being edited.
- Fixed: Save and Close (^KD) now returns to the Opening screen, as in WordStar 7 (#32).
- Fixed: Save from the unsaved-changes prompt on an untitled document now asks for a file name instead of saving to Unknown.ws, and a named document is saved in its own folder (#37).

**Changes**

- Changed: the Find and Replace dialogs use the WordStar 7 wording ("Find and Replace", "OK", "For Next Occurrence", "Entire File/Global", "Use ? Wildcard Character", "Don't Ask", and so on) (#29, wording only).

## 0.14.1 Beta (2026-10-27)

**Fixes**

- Fixed: a tab inserted with the Tab key, a center tab or a right tab saved uninitialized bytes into WordStar files. It now saves zeros.

## 0.14.0 Beta (2026-10-10)

### Merged from upstream WordTsar 0.5.1850

This release brings in the bug fixes and the editor-core work from Gerald Brandt's WordTsar 0.5.1850 (the upstream this fork started from), adapted to the macOS builds and tested against the existing suites.

**Fixes**

- Fixed: a paragraph containing a very long unbroken run of text (a grapheme over 255 bytes) could be measured and edited wrongly; long multi-byte paragraphs are also faster to navigate.
- Fixed: a malformed or truncated WordStar file could crash the loader or load garbage; truncated files now end cleanly, damaged notes are dropped, and a failed save is reported instead of silently ignored.
- Fixed: an RTF file with content outside its document group could crash the importer.
- Fixed: a WordStar color index outside the palette, and white itself, came out as the wrong color.
- Fixed: a plain-text file with invalid UTF-8, or an oversized one, was not rejected safely.
- Fixed: `.rr` could set a right margin past the paper edge; the margin is now clamped, and a ruler too narrow to use is reported as an error.
- Fixed: deleting at the very end of a document could remove the end-of-file marker and corrupt the undo history.
- Fixed: the document could show as modified after undoing back to the saved state; it now follows the undo history.
- Fixed: a failure to write the backup copy was silent; it now shows on the status bar.
- Fixed: the page count could include empty trailing pages, and leaving a dot command line could leave the following paragraphs laid out stale.
- Fixed: the emergency save now writes next to the document.

**Editor core**

- **Block and place markers are positions, not text.** `<B>`, `<K>` and `<0>`..`<9>` are no longer characters in the document: they never appear in undo, never mark the document modified, never print and are not written to files. They are shown inline before the marked character. `^KH` hides a block together with its markers, and a document's block and bookmarks are restored exactly on reopening (previously they could drift, and a bookmark at the very start was lost).
- **Reveal codes never change the layout.** Showing formatting codes no longer re-wraps the document, and the Show Formatting pane wraps the same as the page. The code badges sit beside the text; the GUI scrolls sideways to keep the caret in view.
- **Word count** counts real words (Unicode word boundaries), is kept per paragraph, and no longer lags behind by a background timer. An empty document is 0 words.
- **Default font** from Preferences is now applied at startup in both the GUI and the terminal UI. The generic Monospace family maps to the system fixed-width font, and Times and Courier no longer fall back to another font on macOS.
- **Headers and footers** are built per page from their stored definitions, each with its own page's margins, always in the default font, and without disturbing the document's formatting state. A header defined mid-document now keeps the earlier pages unchanged. Automatic page numbers (`.pg`, `.pc`) are unchanged.
- **Undo and Redo** share one implementation in the GUI and the terminal UI.

## 0.13.0 Beta (2026-09-07)
.hy auto-hyphenation implemented which makes it look right instead of leaving rivers of whitespace
^OE soft hyphen, ^OH auto-hyphenation toggle
Fixed: pasting from the system clipboard in the terminal UI (^K[) could leave a stray empty paragraph behind when the copied text used Windows-style line endings.
Fixed: GUI Spell Check could show the wrong word for a word near the start or end of a paragraph — truncated, or with a stray character from the neighboring word.

## 0.12.0 Beta (2026-09-05)

### Print Preview and Print now use Apple's own PDF viewer

Fixed: printing from the GUI could cut off or misalign a page. Print Preview and Print now build a real PDF and show it in Apple's built-in PDF viewer, with zoom, page navigation, and its own Print button.

### Five new writing tools

- **Thesaurus (^Q J)** — look up synonyms for the word at the cursor.
- **Write Block to File (^K W)** — save a marked block to its own file, without touching the rest of the document.
- **Sort Block (^K Z, then A or D)** — sort the paragraphs in a marked block alphabetically, ascending or descending. (Descending can misplace one character if the block sits with no other text directly before or after it; ascending, and whole-document blocks either way, are unaffected.)
- **Temporary Indent (^O G)** — indent the current paragraph one more tab stop; press again to add another, or move past the paragraph to return to normal.
- **Center Text Vertically (^O V)** — center the text between the cursor and the next page break vertically on the page.

### Other fixes and changes

- Fixed: a paragraph with word wrap turned off (`.aw off`) could run past the bottom of the page without starting a new one.
- Fixed: a page break could land in the wrong place partway through a paragraph that mixed different font sizes.
- Fixed: quitting the app after opening or saving a file elsewhere could silently change your Default Directory preference to that folder.

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
- Not yet supported: "Index Every Word" auto-indexing, and page-range/odd-even filters.

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
