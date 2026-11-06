# BOOK.WS

BOOK.WS is a manuscript template for a novel. It has a title page, a running header with the page number, a contents page layout block, two sample chapters with table of contents entries, one index mark and one index entry. The text is placeholder text.

## Use

1. Copy BOOK.WS. Open the copy in WordTsar (the GUI) or in ws.
2. Replace the placeholder text: your name, address, title, the surname in the header, the chapter headings and the body text.
3. Save the file under your own name.

## Other templates

- CHAPTER.WS is one new chapter: a 54 pt drop, a `.tc` entry, a centred heading, three placeholder paragraphs and `.pa`.
- SCENE.WS is a scene break: `.cp 4` and a centred `#`.

Put the caret at the start of a line in your book. Press ^KR (Block > Read, Alt+K,R in Modern mode) and choose the file. The file is read into the document at the caret.

### Table of contents and index

The `.tc` and `.ix` lines and the index marks do not print.

- `.tc` lines make the table of contents. A `.tc` line that starts with a dot command (`.tc .po 1in`) is written to the table as that command. The contents page layout block in BOOK.WS uses this. A blank `.tc` line does nothing.
- ^PK (Alt+P,K in Modern mode; Insert > Index/TOC Entry > Mark for Index in the GUI and ws) puts index marks around a word. BOOK.WS has one, around `story` in chapter one.
- Entries from marks, `.ix` lines and index items merge into one line with all the pages.
- GUI: Utilities > Generate Table of Contents... and Generate Index... ask for the source file. The open document is the default. They write `name.TOC`, `name.TO1` to `name.TO9` and `name.IDX` next to it, then show a message.
- ws: on the Opening screen, the key T (Utilities > TOC) and the key I (Utilities > Index) ask for a file. The Utilities menu is shown at help level 4 only. The keys work at every help level.

## Dot commands in this template

| Command | What it does here |
|---------|-------------------|
| `.op` | Leaves out the automatic page number. The page number is in the header. |
| `.pl 11in` | Page length 11 inches. |
| `.mt 1in` | Top margin 1 inch. |
| `.mb 1in` | Bottom margin 1 inch. |
| `.po 1in` | Page offset 1 inch. |
| `.lm 0` | Left margin 0 columns. |
| `.rm 6.5in` | Right margin 6.5 inches from the page offset. |
| `.lh 12pt` | Line height 12 pt (single spacing). Address block. |
| `.lh 24pt` | Line height 24 pt (double spacing). Title and body text. |
| `.lh 54pt` | Line height 54 pt. The blank line before a chapter heading drops it down the page. |
| `.pa` | Page break. Ends the title page and each chapter. |
| `.h1 Surname / TITLE / #` | Header text on every page. The `#` prints the page number. |
| `.tc Chapter One . . . #` | Table of contents entry. The generator copies the text and replaces `#` with the page number. The `. . .` is plain text. |
| `.tc .po 1in`, `.tc .rm 6.5in`, `.tc .lh 24pt`, `.tc .pf on` | Dot commands for the generated table, used in the contents page layout block. `.pf on` is accepted. Text always reflows, so it changes nothing. |
| `.cp 4` | If less than four lines remain on the page, the next paragraph starts on a new page. |
| `.ix Placeholder Topic` | Index entry. The generator writes it to `name.IDX` with its page number. |

Units: any of `i`, `in`, `"`, `c`, `cm`, `mm`, `p` and `pt` works after a number or a bracketed expression (`1in`, `2.5cm`, `24pt`). With no unit, `.lm`, `.rm` and `.po` count columns (1/10 inch), `.mt`, `.mb` and `.pl` count lines (1/6 inch) and `.lh` counts 1/48 inch. An unknown unit is an error and the command is ignored.

## Notes

- The centered title, the chapter headings and the `#` scene break line are centre tabs. Type ^OC (Alt+O,C in Modern mode) on the line.
- The word count on the first line is a right tab. Type ^O] (Alt+O,] in Modern mode).
- ^OD (Alt+O,D in Modern mode) shows or hides the command tags, which are the dot command lines.
