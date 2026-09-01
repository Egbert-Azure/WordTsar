# What's New

Release history for WordTsar, in reverse chronological order.

## 0.8.0 Beta (2026-09-01)

### Terminal UI font discovery now finds real installed fonts on macOS

`ws` had no macOS-specific font discovery at all — it silently fell back to a Linux-only directory scan (`/usr/share/fonts`, etc.) that finds nothing on a Mac. This meant printing or classifying anything other than the three built-in fallback font families (Courier, Times, Helvetica) never resolved to the font's actual installed file. Now backed by Core Text's own font collection, so print embeds the real installed file — e.g. a genuine `CourierNewPSMT` from `Courier New.ttf`, not a generic substitute.

### Removed the last of the STB TrueType dependency

The terminal UI's TrueType-metrics backend (`stbtruetype.cpp`) turned out to already be dead code on macOS — Core Text has handled real text-layout metrics here for a while. The one genuinely live use left was WordStar-format font classification (reading a font's OS/2/PANOSE data and glyph coverage to pick the right typestyle when saving a `.ws` file), shared by both the GUI and the terminal UI. That's now Core Text too, so the vendored STB TrueType library is gone from the build entirely — nothing left in WordTsar depends on it.

### New splash screen artwork

Both the GUI and the terminal UI now open with an updated splash based on the original WordTsar cover art. The terminal UI's version reworks it as a letter-spaced title and rule, since a plain terminal can't display an actual image.

## 0.7.6 Beta (2026-09-01)

### Fixed: garbled accented characters (ü, ä, ö, „ ") in `ws` PDF printing/preview

Printing or previewing a document containing German umlauts, curly quotes, or other non-ASCII characters from the terminal UI could silently corrupt them in the output PDF — e.g. "Glück" became "Glˆ…ck". Root cause: the terminal UI's PDF engine (libharu) drew one grapheme at a time through a single shared, stateful UTF-8 text encoder, and that per-character call pattern could desynchronize the encoder's internal byte-sequence tracking, splitting a multi-byte character into two bogus single-byte glyphs. Replacing the PDF engine with macOS's native Quartz/Core Text (below) draws each grapheme as a proper Unicode string with no shared encoder state to desynchronize, eliminating this entire class of bug rather than patching around it.

### Terminal UI's PDF printing now uses Quartz/Core Text, not a vendored library

`ws`'s `^KP`/Print Preview PDF generation no longer depends on the embedded libharu library — it now draws through the same macOS frameworks (Quartz, Core Text) the GUI already uses for its own printing. Output is unchanged in appearance (same fonts, positions, bold/italic/underline handling); see the fix above for the one behavior change this surfaced.

## 0.7.5 Beta (2026-09-01)

### Help and Level system corrected to real WordStar 7, not WordStar 4

Contextual help now lives on **F1** (press F1, then any command key, for a one-line description), with **F1 F1** cycling a 5-level (0-4) help display — matching real WordStar 7.0D exactly, not the WordStar 4.0 design this was first built against. Date/time/filename/macro insertion moved to **^M** (the real WordStar 7 Macro Menu), freeing `^J` back to unassigned. On the GUI, **⌘/** mirrors F1, and Preferences moved fully to **⌘,** now that F1 has a real job. See [KEY_MAPPING.md](KEY_MAPPING.md) for the full reference.

### The terminal Opening Menu, made honestly WordStar 7

`ws`'s Opening Menu grid now matches real WordStar 7's letter-for-letter: `D S N P \ K I T X` on the left, `L C E O Y F M R A ?` on the right, with `F1` for help. Recent Files and Preferences — WordTsar's own additions, not real WS7 concepts — are reachable by cursor and Enter without occupying a borrowed letter. `P` now prints a file straight from the menu; `?` shows a real status screen (version, current directory, free disk space).

### Fixed in this release

- The Opening Menu's key-letter color, previously a hard-to-read blue on a dark terminal background, now uses the same gold accent as the splash screen.
- The `^J`-to-`^M` rename had left 16 GUI menu functions and 6 TUI Insert-menu items hardcoding the old chord, silently disabling date/time/filename/macro insertion from those menus.

## 0.6.1 Beta (2026-08-31)

### Real printing, not just preview

`File → Print` on the GUI now opens the OS print dialog and submits to a printer, instead of silently reusing the Print Preview flow. The TUI's `^KP` now auto-picks a default CUPS printer when exactly one exists and prompts otherwise (macOS ships with no default destination even with printers configured), and gained a real Windows print path.

### Reliable Preferences, Find Again, and Fullscreen shortcuts on macOS

macOS reserves F1/F2, F3, and F11 for brightness, Mission Control, and Show Desktop, so they never reliably reached the app. **⌘,**, **⌘⌃F**, and **⌘G** are now the primary shortcuts for Preferences, Fullscreen, and Find Again; the F-keys still work as a bonus if freed in System Settings.

### Spell-check language actually applies on macOS

The Spell Check Language preference was silently ignored — now wired through `NSSpellChecker setLanguage:` end to end.

### WordStar comment lines (`..` / `.IG`) survive Save As Word

`.docx` export previously dropped comment lines entirely; they're now preserved as hidden text and correctly round-trip back in on import without becoming visible.

### Also fixed

- Splash screen and main window now open centered on the same screen, instead of disagreeing on placement.
- About dialog: correct version string, this fork's own GitHub link (not upstream SourceForge), and an honest third-party dependency list.
- New documents no longer default to `~` by accident — they use `~/Documents` unless a default directory is set in Preferences.
- The flaky comm-server QoS-1 retransmit test, root-caused (a timing margin, not a logic bug) and fixed, along with two related test-hygiene hangs found while verifying it.

## 0.6.0 Beta (2026-08-30) — macOS fork

This fork trims WordTsar down to a macOS-only build. See the [README](README.md) for the full framing; the highlights:

### Save As Word (.docx)

WordTsar can now write `.docx` files, not just read them. File → Save As offers Word format alongside Wordstar and RTF. Tables and headers/footers aren't covered yet.

### Terminal UI, buildable again

The `ws` terminal UI builds alongside the GUI from the same source (`-DBUILD_TUI=ON`, off by default). See [BUILDING.md](BUILDING.md#build-targets) for how to choose between the GUI, the TUI, or both.

### Fixed

- A crash in the macOS spell checker when adding a word to the dictionary (wrong `NSSpellChecker` API was being called).

### Removed

- Windows and Linux project files, build scripts, and packaging assets. Full cross-platform support remains available from the original project at [wordtsar.ca](http://wordtsar.ca).


## 0.5.1804 Alpha (2026-03-01)

### New Layout and Rendering Engines

Completely rewritten layout and rendering engines for improved performance, accuracy, and future capabilities.

Performance difference is good. version 0.3 took about 800ms to lay out a full novel of around 110,000 words. Version 0.5 does the same novel in about 430ms.

### Wordwrap

Wordwrap now behaves a little more like Word or LibreOffice Writer, etc. Wordstar would not wrap a solid line of characters... a single really long line would be made. We now wrap at the right margin instead of doing that.

### Recent Files

Recent files are remembered for easy file loading.

### Keyboard handlers

- Wordstar keyboard handler (default)

- Microsoft Word/CUA keyboard handler **(very much untested)**

Changing keyboard handlers does not change how WordTsar works... it's still a Wordstar clone. So don't expect MS Word style selection block handling, etc.

### New layout/formatting commands

| Keys | Menu | Action | MS Word (CUA) Matches |
| - | - | - | - |
| ^O\< | ALt-L-L | Left Justify Pargaraph Toggle | ^L |
| ^O= | Alt-L-N | Center Justify Paragraph Toggle | ^E |
| ^O\> | Alt-L-G | Right Align Paragraph Toggle | ^R |
| ^O+ | Alt-L-J | Justift Paragraph Toggle | ^J |


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

items are clickable to select font, set attributes, and alignment

### Bottom Status Bar

Insert/Overwrite and Page number are clickable items.

### Backup file changed

backup file changed from *filename.ws-bak* to *filename-bak.ws* so it can be opened easily by WordTsar

### International Input

CJK (Japanese, Chinese, Korean) and Thai input method support.

### Terminal Interface

Run from a shell or via ssh. ***Pre-alpha***. There is some mouse support, but it's patchy at best. Some dialogs require mouse and keyboard for navigation and/or selection. Use at your own risk. Executable name is **ws** (Gui executable name is **WordTsar**)

- **note**: TUI is not designed to work over a serial interface to a terminal (such at vt-100, Wyse-60, etc)

### Math Expressions in Dot Commands

Dot commands now support full arithmetic with unit-aware calculations. Use expressions like `.rm 4c + 2i` to mix centimeters and inches, `.po 8 \* 2i` for multiplication, or `.lm +2c` to adjust margins relative to their current value. All margin, page setup, and tab commands support math with inches, centimeters, millimeters, and points. (Limitation: addition and subtraction can mix measurements units i.e. .5i + 2c. Multiplation and Division cannot, and the dot command will be marked as an error)

### Math Library

Switch from exprtk to picomath for math routines cut the binary size down by half.

### Wordstar files

If a file being loaded doesn't have a known extension, WordTsar will do its best to figure out if it is a Wordstar file and load it. If that fails, it will load as plain text.

If saving as Wordstar will result in data loss, a dialog will be displayed.

Known extensions for Wordstar files are .ws, .ws3, .ws4, .ws5, .ws6, .ws7, .ws8

### Bug Fixes

Multiple bug fixes, some in the ticket system on Sourceforge, most not.

## Notes

### .rr command

The .rr command currently assumes everthing is Courier New 12 point font, so each dash (-) represents 144 twips or 0.1 of an inch (2.54 mm). The width of a single Courier New 12 point character. As far as I can tell, Worstar 7.0D also uses 0.1 of an inch.

The .rr command has been extended with tab types. ^ is a center align tab, and \> is a right align tab. For example:

`.rr l----------!----------\#----------^-----------!--------\>------------r`

### .tb command

The .tb command has been extended with tab type prefixes. ^ is center tab, \> is right aligned tab, and \# is decimal aligned tab. For example:

`.tb 1.0i ^2.1i 3i \>5i \#6.0i`

numbers with no measurement specifier (i for inches, etc) default to colum positions where each column is assumed to be 144 twips or 0.1 of an inch (2.54 mm). The width of a single Courier New 12 point character.


## 0.4.1505 Alpha (2025-10-03)

- Core changes and foundational work

- UTF-8 in-memory document storage

- Unicode version 16 support

- Linux, Windows, and MacOS builds

