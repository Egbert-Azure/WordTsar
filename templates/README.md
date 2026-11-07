# Book template

BOOK.WS is a manuscript template for a novel: a title page, a running header with the page number, a contents page layout block, two sample chapters with contents entries, one index mark and one index entry. The folder `macros` holds the macros that fill it in and add to it. Each macro is a `.wsm` file in WordStar 7's macro language: `Key("...")` lines with the keys and a `PauseForInput` line where it stops for typing.

## Use

In short: copy the templates folder, open BOOK.WS, then ^MP SETUP (in ws: Option+M, then P).

1. Copy this folder (BOOK.WS and `macros`). Open BOOK.WS in WordTsar or in ws. ^MP lists the macros in the `macros` folder next to the open document first. In ws, type ^M as Option+M (set Option as Meta in the terminal, see KEY_MAPPING.md). In Modern mode, use Utilities > Macros > Play....
2. ^MP SETUP. It asks, in this order: title, author's name, surname, street, city/state/ZIP, country, e-mail, phone. At each stop the status line says "Type, then press Enter". Type the value and press Enter.
3. For each new chapter, put the cursor at the end of the last paragraph of the chapter before and press ^MP CHAPTER. It asks for the chapter title the same way, adds a page break, the drop line, the contents entry and the centred heading, and leaves the cursor at the first paragraph.

## Macros

- SETUP: fills in the title page, the address block and the header.
- CHAPTER: a new chapter, see above.
- SCENE: a centred `#` scene break with its `.cp 4`, after the paragraph at the cursor.
- INDEX: marks the word at the cursor for the index with ^PK. Put the cursor inside the word. It does not mark the last word of the document: mark that one by hand with ^PK before and after it.
- NEXTCH and PREVCH: jump to the start of the next or previous chapter. After the last or before the first chapter the cursor stays.
- NEXTSC: jumps to the next centred # scene break. After the last one the cursor stays.
- PREVSC: jumps to the previous centred # scene break. Before the first one the cursor stays.
- CHWORDS: marks the chapter at the cursor as the block, from its .lh 54pt line to the next .pa or the end, and shows its word count.

^MD opens a macro as a text file. New macros are recorded with ^MR, see KEY_MAPPING.md.

If you have the Sawyer archive, copy its .WSM files into your `macros` folder. ^MP does not run a macro that uses a command WordTsar does not have; the status line names the command and the line.

## Table of contents and index

The `.tc` and `.ix` lines and the index marks do not print.

- GUI: Utilities > Generate Table of Contents... and Generate Index... ask for the source file. The open document is the default. They write `name.TOC`, `name.TO1` to `name.TO9` and `name.IDX` next to it.
- ws: on the Opening screen, the key T makes the table of contents and the key I makes the index. Both ask for a file.

## Dot commands

The page layout is `.pl 11in`, `.mt 1in`, `.mb 1in`, `.po 1in`, `.lm 0`, `.rm 6.5in`. The address block uses `.lh 12pt`, the body `.lh 24pt` (double spacing). The blank line before a chapter heading has `.lh 54pt` and drops the heading down the page. `.h1` is the header; `#` in it prints the page number.

Units: any of `i`, `in`, `"`, `c`, `cm`, `mm`, `p`, `pt`. With no unit, `.lm`, `.rm` and `.po` count columns, `.mt`, `.mb` and `.pl` count lines and `.lh` counts 1/48 inch.

^OD (Alt+O,D in Modern mode) shows or hides the dot command lines.
