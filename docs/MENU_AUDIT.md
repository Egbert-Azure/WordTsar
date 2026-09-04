# Menu / Handler Enumeration — Task A Audit

Scope: every menu item in both frontends (GUI: `src/gui/wordtsar.cpp` `CreateMenus()`,
labels from `src/gui/menu/wordstarmenu.cpp` / `modernmenu.cpp`; TUI: `src/tui/wordtsar.cpp`
`BuildMenus()`, lines 1030–1329), under both input modes. Audit only — no code changed.

## STATUS UPDATE — the tables below are the original audit; fixes since then

This audit was written before any fixes were applied. Two rounds of fixes have landed
since, so most of the tables below describe the *pre-fix* state. Rather than re-verify and
rewrite every one of the ~350 rows against current code, this note records what changed and
why the tables are safe to read as history:

- **The [SYS] DISAGREE mechanism is fixed.** Both frontends now dispatch every menu item
  through a dedicated `cWordStarInput* mMenuInput` (`src/gui/wordtsar.h`/`src/tui/wordtsar.h`)
  that is never swapped by `SetInputMode()` — a menu click always runs the real WordStar-mode
  command regardless of the active keyboard mode. Every row below tagged **[SYS] DISAGREE**
  ("WRONG → ..." / "silent no-op" under Modern) is resolved: the "WS outcome" column is now
  what happens under both modes.
- **Ten dead/mislabeled menu items were removed** (not fixed — deleted, since they had no
  real target): Mark Text for Index, Spell Check Rest of Notes, Column Break, Language
  Change, Repeat Keystroke, Note Options Starting Number, Note Options Convert at Print,
  Style Settings, Insert > Other Value > Current Drive, Insert > Variable > Line. Rows below
  for these are marked **REMOVED**.
- **`^K R` / File > Open/Read vs. Insert > File are now two different commands.** Real WS7's
  `^K R` inserts a file at the cursor (`cWordStarInput::OnControlKChar` case `'r'`, now calls
  `InsertFileAtCursor()`); File > Open/Read still replaces the whole document. The GUI's
  Insert > File menu item now calls a separate `InsertFile()` wrapper instead of `Open()`.
  Q1's answer (below) predates this fix and no longer describes current behavior.
- **^B (Reformat Paragraph) was removed**, not fixed — the layout engine reflows every
  paragraph against current margins on every relayout, so real WordStar's manual-reformat
  model has nothing to do; see the row below marked **REMOVED**.
- **Sentence Case (`^K .`) now does real sentence case**, not Title Case — the MISLABEL is
  resolved, not just the label.
- **`.pc` (page number column) is now implemented** — not a menu item in either frontend, so
  it has no row here, but it's no longer `DOT_NOTIMPLEMENTED`.
- TUI Layout > Headers/Footers (Header.../Footer...) and Special Effects > Keep Lines
  Together on Page (flagged under Q2 below as a cross-frontend gap) are now wired live with
  real `.he`/`.fo`/`.cp` insertion, matching the GUI.
- TUI View > Switch Modes (flagged under Q3/DISAGREE below as disabled-while-real) is now
  enabled and wired to the real `^O T` toggle.

Primary sources cross-referenced for every dispatch target:
- `src/input/wordtsarinput.cpp` (`cWordStarInput`) — fully read, every case enumerated.
- `src/input/moderninput.cpp` (`cModernInput`) — fully read, every case enumerated.
- `src/core/layout/dotcommandparser.cpp` — full dispatch table read.
- `src/gui/wordtsar.cpp` — every `cWordTsar::*` wrapper body extracted and read.
- `src/tui/wordtsar.cpp` `BuildMenus()` — read in full (lines 1030–1329).
- `docs/WordStar 7 Manual.pdf` — checked for the ^K R question (see below).

## SYSTEMIC FINDING — the actual drift mechanism (read this first)

Both frontends share one `IInputHandler` pointer (`mInput` in `cEditorCtrl`/`cWSEditorCtrl`,
swapped between `cWordStarInput` and `cModernInput` by `SetInputMode()` —
`src/gui/editor/editorctrl.cpp:575`, `src/tui/editor/editorctrl.cpp:564`). Menu items that
fire a command by calling `HandleKey(PREFIX)` then `HandleKey(letter)` — the GUI's
`cWordTsar::*` wrappers, and the TUI's `InjectChord()` (`src/tui/wordtsar.cpp:470`) — always
send **WordStar-style** codes (`CTRL_K`, `CTRL_O`, `CTRL_P`, `CTRL_Q`, `CTRL_M`), regardless
of which mode is actually active. In WordStar mode these are chord *prefixes* that wait for
the second key. In Modern mode, per `moderninput.cpp`, four of them are not prefixes at all:

| Code | Modern mode's real, unconditional action |
|---|---|
| `CTRL_K` | `SetBeginBlock()` (moderninput.cpp:310-315) |
| `CTRL_O` | `PromptForLoadFile()` / open, replacing the doc (moderninput.cpp:369-394) |
| `CTRL_P` | `PrintPreview()` (moderninput.cpp:396-402) |
| `CTRL_Q` | explicitly unassigned, `handled=false` (moderninput.cpp:404-409) |
| `CTRL_M` | no case at all, falls to `default` (moderninput.cpp, main switch) |

So any menu item built as `InjectChord(CTRL_P, 'b')` (Bold) fires `PrintPreview()` the moment
Modern mode is active — the second key (`'b'`) is silently discarded, since Modern mode
already consumed the first key as a complete, non-chord command. This is not four items; it
is the dispatch shape of **most of the Edit/Style/Layout/Utilities menus in both frontends**.
Concretely, and confirmed **enabled** (not disabled) in **both** modes:

- GUI/TUI **Style menu**: Bold, Italic, Underline, Font, Strikeout, Subscript, Superscript,
  Color — all `CTRL_P`-prefixed. Under Modern mode, every one of these **opens Print Preview**
  instead of applying formatting.
- GUI **Layout**: Center Line, Right Align (line), Page... ; GUI **View**: Switch Modes —
  `CTRL_O`-prefixed. Under Modern mode these **open a file dialog that replaces the current
  document** instead of doing what they say.
- GUI/TUI **Style > Convert Case**: Uppercase, Lowercase, Sentence Case; **Utilities > Word
  Count** — `CTRL_K`-prefixed. Under Modern mode these **mark a block** instead.
- GUI/TUI **Utilities**: Spell Check Global / Rest of Document / Word, Reformat > Rest of
  Document — `CTRL_Q`-prefixed. Under Modern mode these **silently do nothing**.

This reproduces with any keyboard: switch to Modern (CUA) mode in Preferences, then click
Style > Bold in either app. Every row below tagged **[SYS]** is an instance of this one
mechanism, not a separate bug.

## GUI inventory — `src/gui/wordtsar.cpp` `CreateMenus()` (lines 1483–2316)

Menu path | WS label | Modern label | Dispatch (file:line) | WS outcome | Modern outcome | Enabled | Class
---|---|---|---|---|---|---|---
File > Open/Read | Open/Read\t^KR | Open...\tCtrl+O | `Open()`→`cEditorCtrl::Open()` gui/editor/editorctrl.cpp:7046 | replaces doc (see Q1 below) | same, mode-independent (direct call) | yes | see Q1 (behavior bug vs WS7, not mode drift)
File > Save | ^KS / Ctrl+S | | `Save()` wordtsar.cpp:2326→`mEditor->Save()` | real | real | yes | LIVE
File > Save As... | ^KT / Ctrl+Shift+S | | `SaveAs()`:2333 | real | real | yes | LIVE
File > Save and Close | ^KD / "" | | `SaveandClose()`→`SaveAndClose()` | real | real | yes | LIVE
File > Print... | ^KP / Ctrl+P | | `Print()`:→`Save();Print()` | real | real | yes | LIVE
File > Print Preview | ^OP / "" | | `PrintPreview()`→`Save();PrintPreview()` | real | real | yes | LIVE
File > Preferences... | hardcoded "⌘," | | lambda→`SystemPreferences()` wordtsar.cpp:1551-1554 | real | real | yes | LIVE
File > Exit WordTsar | ^KX / Alt+F4 | | `ExitWordTsar()`→`ExitApplication()` | real | real | yes | LIVE — Modern label **FIXED** to `Cmd+Q` (real macOS quit accelerator now wired in `editorctrl.cpp`'s `keyPressEvent`)
Edit > Undo | ^U / Ctrl+Z | | `Undo()`→`mEditor->Undo()` | real | real | yes | LIVE
Edit > Redo | Ctrl+Alt+U / Ctrl+Y | | `Redo()`→`mEditor->Redo()` | real | real | yes | LIVE
Edit > Mark Block Beginning | ^KB / Alt+K,B | | `MarkBlockStart()`→`SetBeginBlock()` direct | real | real | yes | LIVE
Edit > Mark Block End | ^KK / Alt+K,K | | `MarkBlockEnd()`→`SetEndBlock()` direct | real | real | yes | LIVE
Edit > Move > Block | ^KV / Alt+K,V | | `MoveBlock()`→`mEditor->MoveBlock()` direct | real | real | yes | LIVE
Edit > Copy > Block | ^KC / Alt+K,C | | `CopyBlock()` direct | real | real | yes | LIVE
Edit > Copy > From Clipboard/Paste | ^K\[ / Ctrl+V | | `CopyFromClipboard()`→`ClipboardPaste()` direct | real | real | yes | LIVE
Edit > Copy > To Clipboard/Copy | ^K\] / Ctrl+C | | `CopyToClipboard()`→`ClipboardCopy()` direct | real | real | yes | LIVE
Edit > Delete > Block | ^KY / Alt+K,Y | | `DeleteBlock()` direct | real | real | yes | LIVE
Edit > Delete > Word | ^T / Ctrl+Del | | `DeleteWord()`→`DeleteWordRight()` direct | real | real | yes | LIVE
Edit > Delete > Line | ^Y / "" | | `DeleteLine()` direct | real | real | yes | LIVE
Edit > Delete > Line Left | ^QDel / Alt+Q,DEL | | `DeleteLineLeft()` direct | real | real | yes | LIVE
Edit > Delete > Line Right | ^QY / Alt+Q,Y | | `DeleteLineRight()` direct | real | real | yes | LIVE
Edit > Delete > To Character | ^QT / Alt+Q,T | | `DeleteToChar()` direct | real | real | yes | LIVE
Edit > Mark Previous Block | ^KU / Alt+K,U | | `MarkPrevBlock()`→`SetPreviousBlock()` direct | real | real | yes | LIVE
Edit > Find | ^QF / Ctrl+F | | `Find()` direct | real | real | yes | LIVE
Edit > Find and Replace | ^QA / Ctrl+H | | `FindandReplace()`→`Replace()` direct | real | real | yes | LIVE
Edit > Find Next | ^L / F3 | | `FindNext()`→`FindAgain()` direct | real | real | yes | LIVE
Edit > Go to Character | ^QG / Alt+Q,G | | `GotoChar()`→`GotoCharacter()` direct | real | real | yes | LIVE
Edit > Goto Page | ^QI / Ctrl+G | | `GotoPage()` direct | real | real | yes | LIVE
Edit > Go to Marker 1-9,0 (×10) | ^Q1-9/0 / Alt+Q,1-9/0 | | `Goto1..Goto0()`→`GotoSavePosition(n)` direct | real | real | yes | LIVE ×10
Edit > Go to Other > Font Tag | ^Q= / Alt+Q,= | | `GotoFont()`→`GotoFontTag()` direct | real | real | yes | LIVE
Edit > Go to Other > Style Tag | ^Q< / "" | | `GotoStyle()`→`NotImplemented("^Q-<")` wordtsar.cpp:~2745 | stub msg | stub msg | **no** | STUB
Edit > Go to Other > Note... | ^ONG / "" | | `GotoNote()`→`NotImplemented("^Q-N-G")` | stub msg | stub msg | **no** | STUB
Edit > Go to Other > Previous Position | ^QP / Alt+Q,P | | `GotoPrevPos()`→`GotoPreviousPosition()` direct | real | real | yes | LIVE
Edit > Go to Other > Last Find/Replace | ^QV / Alt+Q,V | | `GotoLastFindandReplace()` direct | real | real | yes | LIVE
Edit > Go to Other > Beginning of Block | ^QB / Alt+Q,B | | `GotoStartBlock()`→`MoveCursorStartBlock()` direct | real | real | yes | LIVE
Edit > Go to Other > End of Block | ^QK / Alt+Q,K | | `GotoEndBlock()`→`MoveCursorEndBlock()` direct | real | real | yes | LIVE
Edit > Go to Other > Document Beginning | ^QR / Ctrl+Home | | `GotoDocumentStart()`→`MoveCursorTopofFile()` direct | real | real | yes | LIVE
Edit > Go to Other > Document End | ^QC / Ctrl+End | | `GotoDocumentEnd()`→`MoveCursorEndofFile()` direct | real | real | yes | LIVE
Edit > Go to Other > Scroll Up | ^QW / "" | | `GotoScrollUp()`→`NotImplemented("^Q-w")` | stub msg | stub msg | **no** | STUB
Edit > Go to Other > Scroll Down | ^QZ / "" | | `GotoScrollDown()`→`NotImplemented("^Q-z")` | stub msg | stub msg | **no** | STUB
Edit > Set Marker 1-9,0 (×10) | ^K1-9/0 / Alt+K,1-9/0 | | `Set1..Set0()`→`SavePosition(n)` direct | real | real | yes | LIVE ×10
Edit > Edit Note | ^OND / "" | | `EditNote()`→`NotImplemented("^O-n-d")` | stub msg | stub msg | **no** | STUB
Edit > Note Options > Starting Number | "" | | `NoteStartNumber()` — **empty body**, wordtsar.cpp:~2760 | nothing | nothing | **no** | DEAD (empty impl, not even a stub message) — **REMOVED**
Edit > Note Options > Convert Note | ^ONV / "" | | `NoteCOnvert()`→`NotImplemented("^O-n-v")` | stub msg | stub msg | **no** | STUB
Edit > Note Options > Convert at Print | .cv / "" | | `NoteConcertForPrint()` — **empty body** | nothing | nothing | **no** | DEAD (empty impl) — **REMOVED**
Edit > Note Options > Endnote Location | .pe / "" | | `NoteEndNoteLocation()`→ inserts `.pe` dot command, real per dotcommandparser.cpp:474 (`DOT_NOTIMPLEMENTED`) | inserts unimplemented dot cmd (red bg) | same | **no** | STUB
Edit > Editing Settings > Column Block Mode | ^KN / "" | | `ColumnBlockMode()`→`NotImplemented("^K-n")` | stub msg | stub msg | **no** | STUB
Edit > Editing Settings > Column Replace | ^KI / "" | | `ColumnReplaceMode()`→`NotImplemented("^K-i")` | stub msg | stub msg | **no** | STUB
Edit > Editing Settings > Auto Align | ^OA / "" | | `AutoAlign()`→`mEditor->Preferences()` direct | opens Preferences (mislabeled) | same | **no** | STUB
Edit > Editing Settings > Close Dialog | "" / "" | | `CloseDialog()`→`InvalidCommand("^O-<CR>")` | stub msg | stub msg | **no** | STUB
View > Print Preview | ^OP / "" | | `PrintPreview()` direct | real | real | yes | LIVE
View > Command Tags/Show Formatting | ^OD / Alt+O,D | | `CommandTags()`→`ToggleShowControl()` direct | real | real | yes | LIVE
View > Block Highlighting | ^KH / Alt+K,H | | `BlockHighlight()`→`ToggleHideBlock()` direct | real | real | yes | LIVE
View > Screen Settings... | ^OB / "" | | `ScreenSettings()`→`mEditor->Preferences()` direct | real (opens relevant prefs) | real | yes | LIVE
View > Switch Modes | ^OT / Alt+O,T | | `SwitchModes()`: `HandleKey(CTRL_O);HandleKey('t')` wordtsar.cpp:2789-2793 | **real** (ToggleDisplayMode/ToggleCenterView) | **WRONG** — `CTRL_O`→opens file dialog, replaces doc; `'t'` discarded | **yes** | **[SYS] DISAGREE**
Insert > Page Break | .pa / "" | | `PageBreak()`→inserts `.pa\n` (real, dotcommandparser.cpp `PA`→`ParsePageBreak`) | real | real | yes | LIVE
Insert > Column Break | .cb / "" | | `ColumnBreak()`→inserts `.cb\n` — **`.cb` has no case in dotcommandparser.cpp at all** (falls to `DOT_UNKNOWN`) | nonexistent dot cmd | same | **no** | DEAD — **REMOVED**
Insert > Today's Date | ^M@ / "" | | `InsertDate()`: `HandleKey(CTRL_M);HandleKey('@')` | real | **silent no-op** (`CTRL_M` has no Modern case) | yes | **[SYS] DISAGREE**
Insert > Other Value > Current Time | ^M! / "" | | `InsertTime()` same shape | real | silent no-op | yes | **[SYS] DISAGREE**
Insert > Other Value > Last Math Result | ^M= / "" | | `MathResult()` same shape, WS = `NotImplemented` | stub msg | silent no-op | **no** | STUB
Insert > Other Value > Last Math Expr. | ^M# / "" | | `MathExpression()` | stub msg | silent no-op | **no** | STUB
Insert > Other Value > Last Math Dollar | ^M$ / "" | | `MathDollar()` | stub msg | silent no-op | **no** | STUB
Insert > Other Value > Current Filename | ^M* / "" | | `Filename()` | real | silent no-op | yes | **[SYS] DISAGREE**
Insert > Other Value > Current Drive | ^M: / "" | | `Drive()`→`case ':'` is `#ifdef _WINDOWS`-only (wordtsarinput.cpp:1242-1249) | **no-op on macOS** | silent no-op | yes | STUB — **REMOVED** (menu item and the dead `case ':'` both deleted)
Insert > Other Value > Current Directory | ^M. / "" | | `Directory()` | real | silent no-op | yes | **[SYS] DISAGREE**
Insert > Other Value > Current Path | ^M\\ / "" | | `Path()` | real | silent no-op | yes | **[SYS] DISAGREE**
Insert > Variable > Date/Time/Page/Filename/Drive/Directory/Path/Word Count (×8) | "" / "" | | `Var*()`→`mEditor->InsertText("&X&")` direct, then `CheckAndReplaceVariable()` editorbase.cpp:5619 | real (mode-independent) | real | yes | LIVE ×8
Insert > Variable > Line | "" / "" | | `VarLine()`→`InsertText("&_&")`→`VAR_LINE_NUMBER` always returns `"0"` (layoutbase.cpp `GetVariableExpansion`, confirmed by test `test-baselayout.cpp:7871`) | **wrong value, always 0** | same | yes | STUB — **REMOVED** (menu item deleted)
Insert > Extended Char... | ^PO / "" | | `ExtendedChar()`: `HandleKey(CTRL_P);HandleKey('0')` — WS: `'0'` in P's not-impl set | stub msg | **WRONG** → PrintPreview | **no** | STUB (disabled; would be [SYS] DISAGREE if enabled)
Insert > File... | ^KR / "" | | `Open()` direct (same as File>Open/Read) | replaces doc | same | yes | see Q1
Insert > File at Print Time | .fi / "" | | `FileAtPrint()`→inserts `.fi\n`, `FI`→`DOT_NOTIMPLEMENTED` | stub (red bg) | same | **no** | STUB
Insert > Graphic... | ^P* / "" | | `Graphic()`: `HandleKey(CTRL_P);HandleKey('*')` — WS: `'*'` not in P's real set → default/Invalid | invalid cmd | **WRONG** → PrintPreview | **no** | STUB (disabled)
Insert > Note > Comment/Footnote/Endnote/Annotation (×4) | ^ONC/F/E/A / "" | | `Note*()`: `HandleKey(CTRL_O);HandleKey('n');HandleKey(letter)` | WS: opens Preferences (mislabeled) | **WRONG** → opens file dialog, replaces doc | **no** | STUB (disabled; would be [SYS] DISAGREE ×4 if enabled)
Insert > Index/TOC > TOC Entry... | .tc / "" | | `TOCEntry()`→prompts, `InsertDotCommandEntry(".tc",...)` — real, `TC`→`DOT_GOOD` | real | real | yes | LIVE
Insert > Index/TOC > Index Entry... | ^ONI / "" | | `IndexEntry()`→`InsertDotCommandEntry(".ix",...)` — real, `IX`→`DOT_GOOD` | real | real | yes | LIVE
Insert > Index/TOC > Mark Text for Index | ^PK / "" | | `MarkIndex()`: `HandleKey(CTRL_P);HandleKey('k')` — WS: `'k'`→`BeginStrikeThrough()` (same slot as Strikeout!) | **mislabeled**: fires Strikethrough, not indexing | **WRONG** → PrintPreview | **no** | STUB (disabled) + MISLABEL underneath — **REMOVED**
Insert > Index/TOC > Dot Leader to Tab | ^P. / "" | | `DotLeader()`: `HandleKey(CTRL_P);HandleKey('.')` — WS: `.` in P's not-impl set | stub msg | WRONG → PrintPreview | **no** | STUB (disabled)
Insert > Par. Outline Number... | ^OZ / "" | | `ParOutlineNumber()`: `HandleKey(CTRL_O);HandleKey('z')` — WS: `z` in O's not-impl set | stub msg | WRONG → opens file dialog | **no** | STUB (disabled)
Style > Bold | ^PB / Ctrl+B | | `Bold()`: `HandleKey(CTRL_P);HandleKey('b')` | real | **WRONG** → PrintPreview | **yes** | **[SYS] DISAGREE**
Style > Italic | ^PY / Ctrl+I | | `Italics()` same shape | real | WRONG → PrintPreview | yes | **[SYS] DISAGREE**
Style > Underline | ^PS / Ctrl+U | | `Underline()` same shape | real | WRONG → PrintPreview | yes | **[SYS] DISAGREE**
Style > Font... | ^P= / Ctrl+D | | `font()` same shape | real | WRONG → PrintPreview | yes | **[SYS] DISAGREE**
Style > Other > Strikeout | ^PX / Alt+P,X | | `Strikeout()` same shape | real | WRONG → PrintPreview | yes | **[SYS] DISAGREE**
Style > Other > Subscript | ^PV / Alt+P,V | | `Subscript()` same shape | real | WRONG → PrintPreview | yes | **[SYS] DISAGREE**
Style > Other > Superscript | ^PT / Alt+P,T | | `Superscript()` same shape | real | WRONG → PrintPreview | yes | **[SYS] DISAGREE**
Style > Other > Doublestrike | ^PD / "" | | `DoubleStrike()` — WS: `d` in P's not-impl set | stub msg | WRONG → PrintPreview | **no** | STUB (disabled)
Style > Other > Color... | ^P- / Alt+P,- | | `Color()`→`SelectColor()` | real | WRONG → PrintPreview | yes | **[SYS] DISAGREE**
Style > Select/Return/Define Para Style (×3) | ^OFS/P/D / "" | | `HandleKey(CTRL_O);HandleKey('f');HandleKey(letter)` — WS: `f`→opens Preferences | mislabeled STUB | WRONG → file dialog | **no** | STUB (disabled) ×3
Style > Manage Paragraph Styles (×4) | ^OFO/Y/R/E / "" | | same shape ×4 | mislabeled STUB | WRONG → file dialog | **no** | STUB (disabled) ×4
Style > Convert Case > Uppercase | ^K" / Alt+K," | | `Uppercase()`: `HandleKey(CTRL_K);HandleKey('"')` | real | **WRONG** → SetBeginBlock | **yes** | **[SYS] DISAGREE**
Style > Convert Case > Lowercase | ^K' / Alt+K,' | | `Lowercase()` same shape | real | WRONG → SetBeginBlock | yes | **[SYS] DISAGREE**
Style > Convert Case > Sentence Case | ^K. / Alt+K,. | | `Sentencecase()`: `HandleKey(CTRL_K);HandleKey('.')` — dispatches to `cEditorBase::SentenceCaseBlock()` | **FIXED** — real sentence case | same | yes | **FIXED**
Style > Settings | "" / "" | | `Settings()` — body is `// @TODO implement`, empty | nothing | nothing | **no** | DEAD (empty) — **REMOVED**
Layout > Center Line | ^OC / Alt+O,C | | `CenterLine()`: `HandleKey(CTRL_O);HandleKey('c')` | real (InsertCenterTab) | **WRONG** → file dialog | **yes** | **[SYS] DISAGREE**
Layout > Right Align Line | ^OJ / Alt+O,] | | `RightAlign()` same shape | real (InsertRightTab) | WRONG → file dialog | yes | **[SYS] DISAGREE**
Layout > Left Align Para | ^O< / Ctrl+L | | `LeftAlignParagraph()`→`SetParagraphAlignment(JUST_LEFT)` **direct**, no HandleKey | real | real | yes | LIVE
Layout > Center Paragraph | ^O= / Ctrl+E | | `CenterParagraph()` direct | real | real | yes | LIVE
Layout > Right Align Para | ^O> / Ctrl+R | | `RightAlignParagraph()` direct | real | real | yes | LIVE
Layout > Justify Paragraph | ^O+ / Ctrl+J | | `JustifyParagraph()` direct | real | real | yes | LIVE
Layout > Ruler Line... | ^OL / "" | | `RulerLine()`: `HandleKey(CTRL_O);HandleKey('l')` — WS: `l`→opens Preferences | mislabeled STUB | WRONG → file dialog | **no** | STUB (disabled)
Layout > Columns... | ^OU / "" | | `Columns()` same shape, `'U'`→tolower `u`→opens Preferences | mislabeled STUB | WRONG → file dialog | **no** | STUB (disabled)
Layout > Page... | ^OY / Alt+O,Y | | `Page()`: `HandleKey(CTRL_O);HandleKey('Y')` — WS: `y`→`PageLayout()` real | real | **WRONG** → file dialog | **yes** | **[SYS] DISAGREE**
Layout > Headers/Footers > Header... | .he / "" | | `Header()`→inserts `.he\n`, real (`HE`→`ParseHeader`) | real | real | yes | LIVE
Layout > Headers/Footers > Footer... | .fo / "" | | `Footer()`→inserts `.fo\n`, real | real | real | yes | LIVE
Layout > Page Numbering... | ^O# / "" | | `PageNumbering()`: `HandleKey(CTRL_O);HandleKey('#')` — WS: `#` in O's not-impl set | stub msg | WRONG → file dialog | **no** | STUB (disabled)
Layout > Line Numbering... | .l# / "" | | `LineNumbering()`→inserts `.l#\n`, `L#`→`DOT_NOTIMPLEMENTED` | stub (red bg) | same | **no** | STUB (disabled)
Layout > Alignment/Spacing | ^OS / "" | | `Alignment()`: `HandleKey(CTRL_O);HandleKey('s')` — WS: `s`→opens Preferences | mislabeled STUB | WRONG → file dialog | **no** | STUB (disabled)
Layout > Special Effects > Overprint Char | ^PH / "" | | `OverprintChar()`: `HandleKey(CTRL_P);HandleKey('H')` — WS: `h`(tolower) in P not-impl | stub msg | WRONG → PrintPreview | **no** | STUB (disabled)
Layout > Special Effects > Overprint Line | ^P↵ / "" | | `OverprintLine()`: `HandleKey(CTRL_P);HandleKey(HARD_RETURN)` — no case for `HARD_RETURN` in P's switch | invalid cmd | WRONG → PrintPreview | **no** | STUB (disabled)
Layout > Special Effects > Option Hyphen | ^OE / "" | | `OptionalHyphen()`: `HandleKey(CTRL_O);HandleKey('e')` — WS: `e`→opens Preferences | mislabeled STUB | WRONG → file dialog | **no** | STUB (disabled)
Layout > Special Effects > Vertically Center | ^OV / "" | | `VerticalCenter()` same shape, `v`→opens Preferences | mislabeled STUB | WRONG → file dialog | **no** | STUB (disabled)
Layout > Special Effects > Keep Word Together | ^PO / "" | | `KeepWordsTogether()`: `HandleKey(CTRL_P);HandleKey('O')` — WS: `o`(tolower) in P not-impl | stub msg | WRONG → PrintPreview | **no** | STUB (disabled)
Layout > Special Effects > Keep Lines/Page | .cp / "" | | `KeepLinesTogetherPage()`→inserts `.cp\n`, real (`CP`→`ParseConditionalPageBreak`) | real | real | yes | LIVE
Layout > Special Effects > Keep Lines/Column | .cc / "" | | `KeepLinesTogetherColumn()`→inserts `.cc\n`, `CC`→`DOT_NOTIMPLEMENTED` | stub (red bg) | same | **no** | STUB (disabled)
Utilities > Spell Check Global | ^QL / "" | | `SpellCheckGlobal()`: `HandleKey(CTRL_Q);HandleKey('r');HandleKey(CTRL_Q);HandleKey('l')` | real (goes to top, then spell-checks) | **silent no-op** (`CTRL_Q` unassigned in Modern) | **yes** | **[SYS] DISAGREE**
Utilities > Spell Check Other > Rest of Document | ^QL / "" | | `SpellCheckRest()`: `HandleKey(CTRL_Q);HandleKey('l')` | real | silent no-op | yes | **[SYS] DISAGREE**
Utilities > Spell Check Other > Word | ^QN / "" | | `SpellCheckWord()`: `HandleKey(CTRL_Q);HandleKey('N')` | real | silent no-op | yes | **[SYS] DISAGREE**
Utilities > Spell Check Other > Type Word... | ^QO / "" | | `SpellCheckType()`: `HandleKey(CTRL_Q);HandleKey('o')` — real (`SpellCheckEnterWord`) | real, **but disabled** | silent no-op | **no** | **DISAGREE** (disabled despite real WS dispatch)
Utilities > Spell Check Other > Rest of Notes | ^ONL / "" | | `SpellCheckNotes()`: `HandleKey(CTRL_Q);HandleKey('O');HandleKey('L')` — 2nd call completes chord (`SpellCheckEnterWord`), 3rd discarded | **MISLABEL**: fires "type a word", not "rest of notes" | silent no-op | **no** | STUB (disabled) + MISLABEL — **REMOVED**
Utilities > Thesaurus | ^QJ / "" | | `Thesaurus()`: `HandleKey(CTRL_Q);HandleKey('j')` — WS: `j` in Q not-impl | stub msg | silent no-op | **no** | STUB (disabled)
Utilities > Language Change... | .la / "" | | `LanguageChange()`→inserts `.la\n` — **no `LA` case anywhere in dotcommandparser.cpp** (`L` bucket only has LM/LH/LS/LQ/L#) | nonexistent dot cmd | same | **no** | DEAD (disabled) — **REMOVED**
Utilities > Inset | ^P& / "" | | `Inset()`: `HandleKey(CTRL_P);HandleKey('&')` — WS: `&` in P not-impl | stub msg | WRONG → PrintPreview | **no** | STUB (disabled)
Utilities > Calculator | ^QM / "" | | `Calculator()`: `HandleKey(CTRL_Q);HandleKey('M')` — WS: `m` in Q not-impl | stub msg | silent no-op | **no** | STUB (disabled)
Utilities > Block Math | ^KM / "" | | `BlockMath()`: `HandleKey(CTRL_K);HandleKey('m')` — WS: `m` in K not-impl | stub msg | WRONG → SetBeginBlock | **no** | STUB (disabled)
Utilities > Sort Block > Ascending/Descending (×2) | ^KZA/^KZD / "" | | `HandleKey(CTRL_K);HandleKey('z');HandleKey(letter)` — WS: `z` in K not-impl, 3rd key discarded | stub msg | WRONG → SetBeginBlock | **no** | STUB (disabled) ×2
Utilities > Word Count | ^K? / Alt+K,? | | `WordCount()`: `HandleKey(CTRL_K);HandleKey('?')` | real | **WRONG** → SetBeginBlock | **yes** | **[SYS] DISAGREE**
Utilities > Macros > Play/Record/Edit/Single Step/Copy/Delete/Rename (×7) | ^MP/R/D/S/O/Y/E / "" | | `HandleKey(CTRL_M);HandleKey(letter)` — WS: all in M's not-impl set | stub msg | silent no-op | **no** | STUB (disabled) ×7
Utilities > Merge Print Commands (×10: Data File, Name Vars, Set Var, Set Var Math, Ask Var, If, Else, End If, Top, Bottom) | .df/.rv/.sv/.ma/.av/.if/.el/.ei/.go t/.go b | | insert dot commands direct — all `DOT_NOTIMPLEMENTED` (DF, RV, SV, MA, AV, IF, EL, EI, GO×2) | stub (red bg) ×10 | same | **no** | STUB (disabled) ×10
Utilities > Reformat > Rest of Document | ^QU / Alt+Q,U | | `ReformatRest()`: `HandleKey(CTRL_Q);HandleKey('u')` | real (`LayoutDocument(true)`) | **silent no-op** | **yes** | **[SYS] DISAGREE**
Utilities > Reformat > Paragraph | ^B / "" | | `ReformatPara()`: `HandleKey(CTRL_B)` — **no case anywhere in `HandleKey`'s main switch** | nothing happens | `CTRL_B`→`Bold()` (real Modern action!) | **no** | **DEAD** — **REMOVED** (vestigial: layout engine reflows every paragraph on every relayout, so real WordStar's manual-reformat model has nothing to do)
Utilities > Reformat > Rest of Notes | ^ONU / "" | | `ReformatNotes()`: `HandleKey(CTRL_O);HandleKey('n');HandleKey('u')` — WS: `n`→opens Preferences, `u` discarded | mislabeled STUB | WRONG → file dialog | **no** | STUB (disabled)
Utilities > Repeat Keystroke | ^QQ / "" | | `RepeatKey()`: `HandleKey(CTRL_Q);HandleKey('q')` — **no case for `q` in `OnControlQChar`** | invalid cmd | silent no-op | **no** | DEAD (disabled) — **REMOVED**
Utilities > System Preferences | hardcoded "⌘," | | lambda→`SystemPreferences()` wordtsar.cpp:2296-2299 | real | real | yes | LIVE
Help > About WordTsar | "" | | `AboutWordTsar()` | real | real | yes | LIVE

GUI total: 165 rows (10 collapsed as ×10/×8/×7/×4/×3/×2 identical-shape groups). Non-LIVE: 62 rows (34 unique + collapsed groups), of which **20 are [SYS] DISAGREE while enabled** — the headline finding.

## TUI inventory — `src/tui/wordtsar.cpp` `BuildMenus()` (lines 1030–1329)

Every WS-mode chord/letter target below was cross-checked against the same
`wordtsarinput.cpp`/`moderninput.cpp` tables used for the GUI above; citations reuse the
line numbers already given there rather than repeating them. TUI dispatch is always
`InjectChord(prefix, letter)` (wordtsar.cpp:470) or `InjectControl(code)` (wordtsar.cpp:496)
unless noted "direct."

Menu path | WS label / Modern label | Dispatch call site | WS outcome | Modern outcome | Enabled | Class
---|---|---|---|---|---|---
File > Open/Read | ^KR / Ctrl+O | `InjectChord(CTRL_K,'r')` :1056 | replaces doc (Q1) | **WRONG** → SetBeginBlock, `r` discarded | yes | **[SYS] DISAGREE**
File > Save | ^KS / Ctrl+S | `InjectChord(CTRL_K,'s')` :1057 | real | WRONG → SetBeginBlock | yes | **[SYS] DISAGREE**
File > Save As... | ^KT / Ctrl+Shift+S | `InjectChord(CTRL_K,'t')` :1058 | real | WRONG → SetBeginBlock | yes | **[SYS] DISAGREE**
File > Save and Close | ^KD / "" | `InjectChord(CTRL_K,'d')` :1059 | real | WRONG → SetBeginBlock | yes | **[SYS] DISAGREE**
File > Print... | ^KP / Ctrl+P | direct `mEditor->Print()` :1061 | real | real | yes | LIVE
File > Print Preview | ^OP / "" | direct `PrintPreview()` :1062 | real | real | yes | LIVE
File > Recent Files... | "" | direct `OpenRecentFiles()` :1064 | real | real | yes | LIVE
File > Preferences... | "" | direct `OpenSystemPreferences()` :1065 | real | real | yes | LIVE
File > Exit WordTsar | ^KX / Alt+F4 | `InjectChord(CTRL_K,'x')` :1067 | real | WRONG → SetBeginBlock | yes | **[SYS] DISAGREE** + label uses Windows convention (moot in TUI, no accelerator actually fires there either)
Edit > Undo | ^U / Ctrl+Z | `InjectControl(CTRL_U)` :1071 | real | **WRONG** → Underline toggle | yes | **[SYS] DISAGREE**
Edit > Redo | Ctrl+Alt+U / Ctrl+Y | direct `mEditor->Redo()` :1072 | real | real | yes | LIVE
Edit > Mark Block Beginning | ^KB / Alt+K,B | `InjectChord(CTRL_K,'b')` :1074 | real | real (coincidence: Modern's `CTRL_K` single action *is* begin-block) | yes | LIVE
Edit > Mark Block End | ^KK / Alt+K,K | `InjectChord(CTRL_K,'k')` :1075 | real | WRONG → begin block (not end) | yes | **[SYS] DISAGREE**
Edit > Move > Block | ^KV / Alt+K,V | `InjectChord(CTRL_K,'v')` :1078 | real | WRONG → begin block | yes | **[SYS] DISAGREE**
Edit > Copy > Block | ^KC / Alt+K,C | `InjectChord(CTRL_K,'c')` :1081 | real | WRONG → begin block | yes | **[SYS] DISAGREE**
Edit > Copy > From Clipboard/Paste | ^K\[ / Ctrl+V | `InjectChord(CTRL_K,'[')` :1082 | real | WRONG → begin block | yes | **[SYS] DISAGREE**
Edit > Copy > To Clipboard/Copy | ^K\] / Ctrl+C | `InjectChord(CTRL_K,']')` :1083 | real | WRONG → begin block | yes | **[SYS] DISAGREE**
Edit > Delete > Block | ^KY / Alt+K,Y | `InjectChord(CTRL_K,'y')` :1086 | real | WRONG → begin block | yes | **[SYS] DISAGREE**
Edit > Delete > Word | ^T / Ctrl+Del | `InjectControl(CTRL_T)` :1087 | real | **WRONG** → Insert tab | yes | **[SYS] DISAGREE**
Edit > Delete > Line | ^Y / "" | `InjectControl(CTRL_Y)` :1088 | real | **WRONG** → Redo | yes | **[SYS] DISAGREE**
Edit > Delete > Line Left | ^QDel / Alt+Q,DEL | direct :1089 | real | real | yes | LIVE
Edit > Delete > Line Right | ^QY / Alt+Q,Y | direct :1090 | real | real | yes | LIVE
Edit > Delete > To Character | ^QT / Alt+Q,T | `InjectChord(CTRL_Q,'t')` :1091 | real | silent no-op | yes | **[SYS] DISAGREE**
Edit > Mark Previous Block | ^KU / Alt+K,U | `InjectChord(CTRL_K,'u')` :1093 | real | WRONG → begin block | yes | **[SYS] DISAGREE**
Edit > Find | ^QF / Ctrl+F | `InjectChord(CTRL_Q,'f')` :1095 | real | silent no-op | yes | **[SYS] DISAGREE**
Edit > Find and Replace | ^QA / Ctrl+H | `InjectChord(CTRL_Q,'a')` :1096 | real | silent no-op | yes | **[SYS] DISAGREE**
Edit > Find Next | ^L / F3 | `InjectControl(CTRL_L)` :1097 | real | **WRONG** → Left Align Paragraph | yes | **[SYS] DISAGREE**
Edit > Go to Character | ^QG / Alt+Q,G | `InjectChord(CTRL_Q,'G')` :1098 | real | silent no-op | yes | **[SYS] DISAGREE**
Edit > Goto Page | ^QI / Ctrl+G | `InjectChord(CTRL_Q,'I')` :1099 | real | silent no-op | yes | **[SYS] DISAGREE**
Edit > Go to Marker 1-9,0 (×10) | ^Q1-9/0 / Alt+Q,1-9/0 | `InjectChord(CTRL_Q,digit)` :1104-1107 | real | silent no-op | yes | **[SYS] DISAGREE** ×10
Edit > Set Marker 1-9,0 (×10) | ^K1-9/0 / Alt+K,1-9/0 | `InjectChord(CTRL_K,digit)` :1123-1128 | real | WRONG → begin block | yes | **[SYS] DISAGREE** ×10
Edit > Go to Other > Font Tag | ^Q= / Alt+Q,= | `InjectChord(CTRL_Q,'=')` :1110 | real | silent no-op | yes | **[SYS] DISAGREE**
Edit > Go to Other > Style Tag | ^Q< / "" | `none,false` :1111 | — | — | **no** | STUB
Edit > Go to Other > Note... | ^ONG / "" | `none,false` :1112 | — | — | **no** | STUB
Edit > Go to Other > Previous Position | ^QP / Alt+Q,P | `InjectChord(CTRL_Q,'p')` :1113 | real | silent no-op | yes | **[SYS] DISAGREE**
Edit > Go to Other > Last Find/Replace | ^QV / Alt+Q,V | `InjectChord(CTRL_Q,'v')` :1114 | real | silent no-op | yes | **[SYS] DISAGREE**
Edit > Go to Other > Beginning of Block | ^QB / Alt+Q,B | `InjectChord(CTRL_Q,'b')` :1115 | real | silent no-op | yes | **[SYS] DISAGREE**
Edit > Go to Other > End of Block | ^QK / Alt+Q,K | `InjectChord(CTRL_Q,'k')` :1116 | real | silent no-op | yes | **[SYS] DISAGREE**
Edit > Go to Other > Document Beginning | ^QR / Ctrl+Home | `InjectChord(CTRL_Q,'r')` :1117 | real | silent no-op | yes | **[SYS] DISAGREE**
Edit > Go to Other > Document End | ^QC / Ctrl+End | `InjectChord(CTRL_Q,'c')` :1118 | real | silent no-op | yes | **[SYS] DISAGREE**
Edit > Go to Other > Scroll Up | ^QW / "" | `none,false` :1119 | — | — | **no** | STUB
Edit > Go to Other > Scroll Down | ^QZ / "" | `none,false` :1120 | — | — | **no** | STUB
Edit > Edit Note | ^OND / "" | `none,false` :1130 | — | — | **no** | STUB
Edit > Note Options > Starting Number/Convert Note/Convert at Print/Endnote Location (×4) | various / "" | `none,false` :1133-1136 | — | — | **no** | STUB ×4 — Starting Number and Convert at Print **REMOVED** (the other two are real STUBs, kept)
Edit > Editing Settings > Column Block/Column Replace/Auto Align (×3) | ^KN/^KI/^OA / "" | `none,false` :1139-1141 | — | — | **no** | STUB ×3
View > Print Preview | ^OP / "" | direct :1145 | real | real | yes | LIVE
View > Command Tags/Show Formatting | ^OD / Alt+O,D | `InjectChord(CTRL_O,'d')` :1147 | real (ToggleShowControl) | **WRONG** → opens file dialog, replaces doc | yes | **[SYS] DISAGREE**
View > Block Highlighting | ^KH / Alt+K,H | `InjectChord(CTRL_K,'H')` :1148 | real | WRONG → begin block | yes | **[SYS] DISAGREE**
View > Screen Settings... | ^OB / "" | direct `OpenPreferences()` :1150 | real | real | yes | LIVE
View > Switch Modes | ^OT / Alt+O,T | `none,false` :1152 | — | — | **no** | **DISAGREE** — disabled while the real `^O T` keyboard shortcut works (Task B item 2 target; this is the TUI-only instance of the originally-reported bug)
Insert > Page Break | .pa / "" | direct, inserts `.pa\n` real | real | real | yes | LIVE
Insert > Column Break | .cb / "" | `none,false` :1157 | — | — | **no** | STUB (and `.cb` doesn't exist in the parser at all — moot since disabled) — **REMOVED**
Insert > Today's Date | ^M@ / "" | `InjectChord(CTRL_M,'@')` :1159 | real | silent no-op (`CTRL_M` no Modern case) | yes | **[SYS] DISAGREE**
Insert > Other Value > Current Time | ^M! / "" | `InjectChord(CTRL_M,'!')` :1162 | real | silent no-op | yes | **[SYS] DISAGREE**
Insert > Other Value > Last Math Result/Expr./Dollar (×3) | ^M=/#/$ / "" | `none,false` :1163-1165 | — | — | **no** | STUB ×3
Insert > Other Value > Current Filename | ^M* / "" | `InjectChord(CTRL_M,'*')` :1166 | real | silent no-op | yes | **[SYS] DISAGREE**
Insert > Other Value > Current Drive | ^M: / "" | `InjectChord(CTRL_M,':')` :1167 | `#ifdef _WINDOWS`-only, no-op here | silent no-op | yes | STUB — **REMOVED**
Insert > Other Value > Current Directory | ^M. / "" | `InjectChord(CTRL_M,'.')` :1168 | real | silent no-op | yes | **[SYS] DISAGREE**
Insert > Other Value > Current Path | ^M\\ / "" | `InjectChord(CTRL_M,'\\')` :1169 | real | silent no-op | yes | **[SYS] DISAGREE**
Insert > Variable > Date/Time/Page/Filename/Drive/Directory/Path/Word Count (×8) | "" | direct `InsertText("&X&")` :1172-1180 | real | real | yes | LIVE ×8
Insert > Variable > Line | "" | direct `InsertText("&_&")` :1175 | wrong value (0) | same | yes | STUB — **REMOVED**
Insert > Extended Char... | ^PO / "" | `none,false` :1182 | — | — | **no** | STUB
Insert > File... | ^KR / "" | `InjectChord(CTRL_K,'r')` :1184 | replaces doc (Q1) | WRONG → begin block | yes | **[SYS] DISAGREE**
Insert > File at Print Time | .fi / "" | `none,false` :1185 | — | — | **no** | STUB
Insert > Graphic... | ^P* / "" | `none,false` :1186 | — | — | **no** | STUB
Insert > Note > Comment/Footnote/Endnote/Annotation (×4) | ^ONC/F/E/A / "" | `none,false` :1189-1192 | — | — | **no** | STUB ×4
Insert > Index/TOC > TOC Entry... | .tc / "" | direct `InsertTOCEntry()` :1196 | real | real | yes | LIVE
Insert > Index/TOC > Index Entry... | ^ONI / "" | direct `InsertIndexEntry()` :1197 | real | real | yes | LIVE
Insert > Index/TOC > Mark for Index/Dot Leader (×2) | ^PK/^P. / "" | `none,false` :1198-1199 | — | — | **no** | STUB ×2
Insert > Outline Number... | ^OZ / "" | `none,false` :1201 | — | — | **no** | STUB
Style > Bold | ^PB / Ctrl+B | `InjectChord(CTRL_P,'b')` :1205 | real | **WRONG** → PrintPreview | yes | **[SYS] DISAGREE**
Style > Italic | ^PY / Ctrl+I | `InjectChord(CTRL_P,'y')` :1206 | real | WRONG → PrintPreview | yes | **[SYS] DISAGREE**
Style > Underline | ^PS / Ctrl+U | `InjectChord(CTRL_P,'s')` :1207 | real | WRONG → PrintPreview | yes | **[SYS] DISAGREE**
Style > Font... | ^P= / Ctrl+D | `InjectChord(CTRL_P,'=')` :1208 | real | WRONG → PrintPreview | yes | **[SYS] DISAGREE**
Style > Other > Strikeout | ^PX / Alt+P,X | `InjectChord(CTRL_P,'x')` :1211 | real | WRONG → PrintPreview | yes | **[SYS] DISAGREE**
Style > Other > Subscript | ^PV / Alt+P,V | `InjectChord(CTRL_P,'v')` :1212 | real | WRONG → PrintPreview | yes | **[SYS] DISAGREE**
Style > Other > Superscript | ^PT / Alt+P,T | `InjectChord(CTRL_P,'t')` :1213 | real | WRONG → PrintPreview | yes | **[SYS] DISAGREE**
Style > Other > Doublestrike | ^PD / "" | `none,false` :1214 | — | — | **no** | STUB
Style > Other > Color... | ^P- / Alt+P,- | `InjectChord(CTRL_P,'-')` :1215 | real | WRONG → PrintPreview | yes | **[SYS] DISAGREE**
Style > Select/Previous/Define Para Style (×3) | ^OFS/P/D / "" | `none,false` :1218-1220 | — | — | **no** | STUB ×3
Style > Manage Paragraph Styles (×4) | ^OFO/Y/R/E / "" | `none,false` :1223-1226 | — | — | **no** | STUB ×4
Style > Convert Case > Uppercase | ^K" / Alt+K," | `InjectChord(CTRL_K,'"')` :1229 | real | WRONG → begin block | yes | **[SYS] DISAGREE**
Style > Convert Case > Lowercase | ^K' / Alt+K,' | `InjectChord(CTRL_K,'\'')` :1230 | real | WRONG → begin block | yes | **[SYS] DISAGREE**
Style > Convert Case > Sentence Case | ^K. / Alt+K,. | `InjectChord(CTRL_K,'.')` :1231 | **FIXED** — real sentence case (`SentenceCaseBlock()`) | same | yes | **FIXED**
Style > Settings | "" | `none,false` :1234 | — | — | **no** | STUB — **REMOVED**
Layout > Center Line/Paragraph | ^OC / Alt+O,C | `InjectChord(CTRL_O,'c')` :1238 | real | **WRONG** → file dialog | yes | **[SYS] DISAGREE**
Layout > Right Align Line/Paragraph | ^OJ / Alt+O,] | `InjectChord(CTRL_O,']')` :1239 | real | WRONG → file dialog | yes | **[SYS] DISAGREE**
Layout > Left Align Para | ^O< / Ctrl+L | `InjectChord(CTRL_O,'<')` :1241 | real | WRONG → file dialog | yes | **[SYS] DISAGREE**
Layout > Center Paragraph | ^O= / Ctrl+E | `InjectChord(CTRL_O,'=')` :1242 | real | WRONG → file dialog | yes | **[SYS] DISAGREE**
Layout > Right Align Para | ^O> / Ctrl+R | `InjectChord(CTRL_O,'>')` :1243 | real | WRONG → file dialog | yes | **[SYS] DISAGREE**
Layout > Justify Paragraph | ^O+ / Ctrl+J | `InjectChord(CTRL_O,'+')` :1244 | real | WRONG → file dialog | yes | **[SYS] DISAGREE**
Layout > Ruler Line.../Columns... (×2) | ^OL/^OU / "" | `none,false` :1246-1247 | — | — | **no** | STUB ×2
Layout > Page... | ^OY / Alt+O,Y | `InjectChord(CTRL_O,'Y')` :1248 | real | WRONG → file dialog | yes | **[SYS] DISAGREE**
Layout > Headers/Footers > Header/Footer (×2) | .he/.fo / "" | `none,false` :1251-1252 | — | — | **no** | STUB ×2 (note: real dot commands, just this *menu item* isn't wired — GUI's equivalent items are live)
Layout > Page Numbering/Line Numbering/Alignment (×3) | ^O#/.l#/^OS / "" | `none,false` :1254-1256 | — | — | **no** | STUB ×3
Layout > Special Effects (×7: Overprint Char/Line, Option Hyphen, Vert. Center, Keep Word Together, Keep Lines/Page, Keep Lines/Column) | various / "" | `none,false` :1259-1265 | — | — | **no** | STUB ×7 (note: GUI's Keep Lines/Page equivalent IS live — this is a cross-frontend absence, see Q2)
Utilities > Spell Check Global | ^QL / "" | direct `SpellCheckDocument()` :1269 | real | real | yes | LIVE
Utilities > Spell Check Other > Rest of Document | ^QL / "" | direct :1272 | real | real | yes | LIVE
Utilities > Spell Check Other > Word | ^QN / "" | direct `SpellCheckWord()` :1273 | real | real | yes | LIVE
Utilities > Spell Check Other > Type Word.../Rest of Notes (×2) | ^QO/^ONL / "" | `none,false` :1274-1275 | — | — | **no** | STUB ×2
Utilities > Thesaurus | ^QJ / "" | `none,false` :1277 | — | — | **no** | STUB
Utilities > Language Change... | .la / "" | `none,false` :1278 | — | — | **no** | STUB (and `.la` doesn't exist in the parser — moot) — **REMOVED**
Utilities > Inset/Calculator/Block Math (×3) | ^P&/^QM/^KM / "" | `none,false` :1280-1282 | — | — | **no** | STUB ×3
Utilities > Sort Block > Ascending/Descending (×2) | ^KZA/^KZD / "" | `none,false` :1285-1286 | — | — | **no** | STUB ×2
Utilities > Word Count | ^K? / Alt+K,? | `InjectChord(CTRL_K,'?')` :1288 | real | **WRONG** → begin block | yes | **[SYS] DISAGREE**
Utilities > Macros (×7) | ^MP/R/D/S/O/Y/E / "" | `none,false` :1292-1298 | — | — | **no** | STUB ×7
Utilities > Merge Print Commands (×10) | various dot cmds | `none,false` :1301-1311 | — | — | **no** | STUB ×10
Utilities > Reformat > Rest of Document | ^QU / Alt+Q,U | `InjectChord(CTRL_Q,'u')` :1318 | real | silent no-op | yes | **[SYS] DISAGREE**
Utilities > Reformat > Paragraph | ^B / "" | `none,false` :1319 | — | — | **no** | STUB — **REMOVED** (vestigial, see GUI row above)
Utilities > Reformat > Rest of Notes | ^ONU / "" | `none,false` :1320 | — | — | **no** | STUB
Utilities > Repeat Keystroke | ^QQ / "" | `none,false` :1322 | — | — | **no** | STUB (^Q Q is DEAD in WS mode too — no case in `OnControlQChar`) — **REMOVED**
Utilities > System Preferences | "" | direct `OpenSystemPreferences()` :1324 | real | real | yes | LIVE
Help > About WordTsar | "" | direct `ShowAboutWordTsar()` :1328 | real | real | yes | LIVE

TUI total: 168 rows (collapsed groups as marked). Non-LIVE: 145 rows, of which **44 are [SYS] DISAGREE while enabled**.

## Answers to the three specific questions

**1. `^K R` — wording bug or behavior bug?**
**Behavior bug** (code wrong), confirmed against the manual text, not assumed. `docs/WordStar 7 Manual.pdf` line ~1081: *"To insert a file at the cursor location, give the command [^KR], and specify the file..."*, and the index (lines 4526, 4712, 23639) files it under *"insert file into document (^KR)"* — real WS7's ^KR inserts a file's content at the cursor, into the document already open. WordTsar's `^K R` (`OnControlKChar` case `'r'`, wordtsarinput.cpp:1391-1440; identical logic in `cEditorCtrl::Open()`, gui/editor/editorctrl.cpp:7046, which the source comment there explicitly says "mirrors" it) instead **clears the whole document and loads a different file** — that's real WordStar's "Open a document," a different command entirely. Every menu label offering "^KR" (GUI's "Open/Read" and "Insert > File...", TUI's same two) is therefore *describing what the code does*, not what real WS7's ^KR does — the mislabeling is downstream of the actual implementation choice. Not changed, per your instruction.

**2. Items present in one frontend but absent in the other.** *(RESOLVED — see STATUS UPDATE)*
Beyond the expected GUI-only pixel/print machinery, one gap looked like an oversight rather than a platform limit: **Layout > Headers/Footers (Header.../Footer...)** and **Layout > Special Effects > Keep Lines Together on Page** were wired live in the GUI (insert real `.he`/`.fo`/`.cp`) but were `none,false` placeholders in the TUI. Both are now wired live in the TUI too.

**3. Accelerator labels differing between the two mode providers in a way the mode doesn't explain.** *(RESOLVED — see STATUS UPDATE)*
- **Modern's "Exit WordTsar" = `Alt+F4`** (modernmenu.cpp:switch, `GetFileExitLabel`) — a Windows convention, not a macOS one; now `Cmd+Q`.
- **"Sentence Case" labeled identically in both providers** (`wordstarmenu.cpp` and `modernmenu.cpp` both say "&Sentence Case"), but the shared dispatch (`^K .` → `TitleCaseBlock()`) did **Title Case**, not sentence case, in both modes equally; now dispatches to a real `SentenceCaseBlock()`.
No other label pair disagreed in a way that wasn't just the expected different-keys-for-different-modes.

## Summary — every non-LIVE row grouped by class (as originally audited; see STATUS UPDATE)

**DEAD** (dispatches to a case/slot that doesn't exist) — **all six removed**:
- GUI/TUI `^B` (Reformat Paragraph) — no case in `HandleKey`'s main switch at all. REMOVED.
- GUI Insert > Column Break / TUI same — `.cb` has no case in `dotcommandparser.cpp`. REMOVED.
- GUI Utilities > Language Change / TUI same — `.la` has no case in `dotcommandparser.cpp`. REMOVED.
- GUI Utilities > Repeat Keystroke / TUI same — no case for `q` in `OnControlQChar`. REMOVED.
- GUI Edit > Note Options > Starting Number, Convert at Print — empty-body wrapper functions (not even a stub message). REMOVED.
- GUI Style > Settings — empty-body wrapper (`// @TODO implement`). REMOVED.

**STUB** (real dispatch, but no-op/placeholder, or silently does something else — *not* mode-dependent):
- All `none,false` TUI menu items still remaining — disabled placeholders, consistent with their real dispatch status (unchanged; the ones with a live GUI counterpart were wired, see STATUS UPDATE).
- All GUI items disabled via `setEnabled(false)` whose underlying `HandleKey`/dot-command call resolves to `NotImplemented`/`DOT_NOTIMPLEMENTED` — unchanged except as noted below.
- `^K R` / "Open/Read" / "File..." (both frontends) — **FIXED**: `^K R` now inserts at the cursor (real WS7 behavior); File > Open/Read is a separate, correctly-named "replace document" command.
- `^M :` (Current Drive, both frontends, both menu locations) — REMOVED (menu item and dead `case ':'` both deleted).
- Insert > Variable > Line (`&_&`, both frontends) — REMOVED.
- GUI Edit > Note Options > Endnote Location, Auto Align, Screen Settings-adjacent items — unchanged (still real call to a generic/wrong target).
- Assorted mislabeled-but-disabled items noted inline above (Mark Text for Index, Spell Check Notes) — both REMOVED.

**DISAGREE** (enabled state contradicts keyboard reality, *or* — the dominant case here — the real dispatch differs by input mode in a way the label doesn't disclose) — **all resolved**:
- **[SYS] rows: 20 in the GUI, 44 in the TUI** — real command in WordStar mode, wrong-or-silent in Modern mode, item fully enabled in both. **FIXED** — both frontends dispatch menu items through a dedicated always-WordStar-mode `mMenuInput`; see STATUS UPDATE.
- TUI View > Switch Modes — was disabled while the real `^O T` keyboard shortcut worked. **FIXED** — now enabled and wired to the real toggle.
- GUI Utilities > Spell Check Other > Type Word... — still disabled despite a real, working WordStar-mode dispatch (`SpellCheckEnterWord`); unchanged, not in scope of either fix pass.

**MISLABEL** (works, but wording is wrong or platform-inappropriate) — **all resolved**:
- "Sentence Case" (both frontends, both modes) — was dispatching to Title Case. **FIXED** — now real sentence case.
- Modern "Exit WordTsar" → `Alt+F4` label. **FIXED** — now `Cmd+Q`.
- GUI/TUI "Mark Text for Index" → fires Strikethrough. REMOVED.
- GUI "Spell Check Notes" → fires "type a word to check," not "rest of notes." REMOVED.
