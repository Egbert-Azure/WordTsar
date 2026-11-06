# BOOK.WS

BOOK.WS is a manuscript template for a novel. It has a title page, a running header with the page number, two sample chapters with table of contents entries, and one index entry. The text is placeholder text.

## Use

1. Copy BOOK.WS. Open the copy in WordTsar (the GUI) or in ws.
2. Replace the placeholder text: your name, address, title, the surname in the header, the chapter headings and the body text.
3. Save the file under your own name.

To add a chapter, copy a chapter from its first dot command (`.lh 36`) to its page break (`.pa`) and paste it after the last chapter.

### Table of contents and index

The `.tc` and `.ix` lines only mark entries. They do not print. The GUI inserts them. ws also generates the files.

- GUI: Insert > Index/TOC Entry > TOC Entry or Index Entry adds a `.tc` or `.ix` line. The GUI has no command that generates the table of contents or the index.
- ws: on the Opening screen, the key T (Utilities > TOC) and the key I (Utilities > Index) ask for a file. The Utilities menu is shown at help level 4 only. The keys work at every help level. T writes `name.TOC` and I writes `name.IDX` next to it. ws then opens the new file. ws can also insert the entries from its Insert menu.

## Dot commands in this template

| Command | What it does here |
|---------|-------------------|
| `.op` | Leaves out the automatic page number. The page number is in the header. |
| `.pl 11i` | Page length 11 inches. |
| `.mt 1i` | Top margin 1 inch. |
| `.mb 1i` | Bottom margin 1 inch. |
| `.po 1i` | Page offset 1 inch: the left edge of the text, because `.lm 0i` adds nothing. |
| `.lm 0i` | Left margin 0 inches. |
| `.rm 6.5i` | Text width 6.5 inches, measured from the page offset. With the 1 inch offset the right margin is 1 inch. |
| `.lh 8` | Line height 8/48 inch (12 pt, single spacing). Used for the address block. |
| `.lh 16` | Line height 16/48 inch (24 pt, double spacing). Used for the title and the body text. |
| `.lh 36` | Line height 36/48 inch (54 pt). The blank line before a chapter heading drops it down the page. |
| `.pa` | Page break. Ends the title page and each chapter. |
| `.h1 Surname / TITLE / #` | Header text on every page. The `#` prints the page number. |
| `.tc Chapter One . . . #` | Table of contents entry. It does not print. When ws generates the table, it copies the text as written and replaces `#` with the page number. The `. . .` is plain text, not a leader. |
| `.cp 4` | If less than four lines (at the current line height) remain on the page, the next paragraph starts on a new page. It keeps the scene break with the text after it. |
| `.ix Placeholder Topic` | Index entry. It does not print. ws writes it to the `.IDX` file with its page number. |

Units: write inch values with the `i` suffix (`1i`, `6.5i`). Do not write `in`, `mm` or `pt`. WordTsar reads them wrongly. `.lh` takes numbers in 1/48 inch.

## Notes

- The centered title, the chapter headings and the `#` scene break line are centre tabs. Type ^OC (Alt+O,C in Modern mode) on the line.
- The word count on the first line is a right tab. Type ^O] (Alt+O,] in Modern mode).
- ^OD (Alt+O,D in Modern mode) shows or hides the command tags, which are the dot command lines.
