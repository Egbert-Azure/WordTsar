//////////////////////////////////////////////////////////////////////////////
//
// WordTsar - Wordstar clone for modern systems http://wordtsar.ca
// Copyright (C) 2026 Egbert H. Schroeer
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as
// published by the Free Software Foundation, either version 3 of the
// License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//////////////////////////////////////////////////////////////////////////////

#include "doctest.h"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <random>
#include <string>
#include <vector>

#include "src/core/document/document.h"
#include "src/core/include/config.h"
#include "src/core/layout/layoutbase.h"
#include "src/files/wordstar/wordstarfile.h"
#include "src/gui/editor/editorctrl.h"
#include <QApplication>


namespace
{

int argc = 0;
char* argv[] = {nullptr};

void ensureQApplication(void)
{
    if (!QApplication::instance())
    {
        new QApplication(argc, argv);
    }
}

// The tests run with test/ as the working directory
const std::string BOOK_TEMPLATE = "../templates/BOOK.WS";

std::string ReadFileBytes(const std::filesystem::path &path)
{
    std::ifstream file(path, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
}

// New empty directory under the system temp directory, unique per call
std::filesystem::path UniqueTempDir(const std::string &prefix)
{
    std::random_device random;
    std::filesystem::path dir;
    do
    {
        dir = std::filesystem::temp_directory_path() / (prefix + "_" + std::to_string(random()) + std::to_string(random()));
    }
    while (std::filesystem::create_directory(dir) == false);
    return dir;
}

// Paragraph text without control markers and without the line end
std::string VisibleText(cDocument *doc, PARAGRAPH_T paragraph)
{
    std::string text = doc->GetParagraphText(paragraph);
    text.erase(std::remove(text.begin(), text.end(), static_cast<char>(MARKER_CHAR)), text.end());
    while (text.empty() == false && (text.back() == '\r' || text.back() == '\n'))
    {
        text.pop_back();
    }
    return text;
}

// Header / footer lines as text, '|' after each line; the line end of the
// defining dot command is left out
std::string HeaderFooterText(const std::vector<sHeaderFooterLine> &lines)
{
    std::string text;
    for (const sHeaderFooterLine &line : lines)
    {
        for (const std::string &grapheme : line.graphemes)
        {
            if (grapheme != "\r" && grapheme != "\n")
            {
                text += grapheme;
            }
        }
        text += "|";
    }
    return text;
}

// Paragraph whose visible text is text, NOT_SET if there is none
PARAGRAPH_T FindParagraph(cEditorCtrl &editor, const std::string &text)
{
    cDocument *doc = editor.GetDocument();
    for (PARAGRAPH_T paragraph = 0; paragraph < editor.GetLayout()->GetNumberOfParagraphs(); paragraph++)
    {
        if (VisibleText(doc, paragraph) == text)
        {
            return paragraph;
        }
    }
    return NOT_SET;
}

// Dot command lines of the document, in order
std::vector<std::string> DotLines(cEditorCtrl &editor)
{
    std::vector<std::string> lines;
    cDocument *doc = editor.GetDocument();
    cLayoutBase *layout = editor.GetLayout();
    for (PARAGRAPH_T paragraph = 0; paragraph < layout->GetNumberOfParagraphs(); paragraph++)
    {
        const sParagraphLayout *paragraphLayout = layout->GetParagraphLayout(paragraph);
        if (paragraphLayout != nullptr && paragraphLayout->isCommand)
        {
            lines.push_back(VisibleText(doc, paragraph));
        }
    }
    return lines;
}

size_t CountStartingWith(const std::vector<std::string> &lines, const std::string &prefix)
{
    return static_cast<size_t>(std::count_if(lines.begin(), lines.end(),
        [&prefix](const std::string &line) { return line.compare(0, prefix.size(), prefix) == 0; }));
}

}


TEST_CASE("Book template - loads as a WordStar 7 file")
{
    ensureQApplication();

    REQUIRE(std::filesystem::exists(BOOK_TEMPLATE));
    CHECK(cWordstarFile::IsWordStarData(BOOK_TEMPLATE));
    CHECK(cWordstarFile::HasHeader(BOOK_TEMPLATE));

    // WordStar 7 header: 0x1D sequence of type 0, version byte 0x70
    std::string bytes = ReadFileBytes(BOOK_TEMPLATE);
    REQUIRE(bytes.size() > 128);
    CHECK(static_cast<unsigned char>(bytes[0]) == 0x1D);
    CHECK(static_cast<unsigned char>(bytes[4]) == 0x70);
    CHECK(bytes.size() % 128 == 0);

    cEditorCtrl editor;
    REQUIRE(editor.LoadFile(BOOK_TEMPLATE));
    CHECK_FALSE(editor.GetDocument()->IsTextFile());
}


TEST_CASE("Book template - every dot command is one WordTsar carries out")
{
    ensureQApplication();

    cEditorCtrl editor;
    REQUIRE(editor.LoadFile(BOOK_TEMPLATE));
    cDocument *doc = editor.GetDocument();
    cLayoutBase *layout = editor.GetLayout();

    for (PARAGRAPH_T paragraph = 0; paragraph < layout->GetNumberOfParagraphs(); paragraph++)
    {
        std::string text = VisibleText(doc, paragraph);
        if (text.empty() == false && text[0] == '.')
        {
            CAPTURE(text);
            const sParagraphLayout *paragraphLayout = layout->GetParagraphLayout(paragraph);
            REQUIRE(paragraphLayout != nullptr);
            CHECK(paragraphLayout->isCommand);
            CHECK(layout->GetParagraphDotStatus(paragraph) == DOT_GOOD);
        }
    }

    std::vector<std::string> dots = DotLines(editor);
    REQUIRE(dots.size() >= 10);

    // Page setup: Letter, 1 inch margins, no automatic page number
    std::vector<std::string> setup = {".op", ".pl 11\"", ".mt 1\"", ".mb 1\"", ".po 1\"", ".lm 0", ".rm 6.5\""};
    CHECK(std::vector<std::string>(dots.begin(), dots.begin() + static_cast<long>(setup.size())) == setup);

    // Line heights: single spaced contact block, double spaced body, chapter drop
    CHECK(CountStartingWith(dots, ".lh 8") == 1);
    CHECK(CountStartingWith(dots, ".lh 16") == 3);
    CHECK(CountStartingWith(dots, ".lh 36") == 2);

    // Title page ends with .pa; the running header follows it
    auto firstPageBreak = std::find(dots.begin(), dots.end(), ".pa");
    REQUIRE(firstPageBreak != dots.end());
    REQUIRE(firstPageBreak + 1 != dots.end());
    CHECK(*(firstPageBreak + 1) == ".h1 Surname / TITLE / #");
    CHECK(CountStartingWith(std::vector<std::string>(dots.begin(), firstPageBreak), ".h") == 0);

    // Two chapters, each with a .tc entry and a closing .pa
    CHECK(CountStartingWith(dots, ".tc ") == 2);
    CHECK(CountStartingWith(dots, ".pa") == 3);
    CHECK(dots.back() == ".pa");
    auto chapterOne = std::find(dots.begin(), dots.end(), ".tc Chapter One . . . #");
    auto chapterTwo = std::find(dots.begin(), dots.end(), ".tc Chapter Two . . . #");
    REQUIRE(chapterOne != dots.end());
    REQUIRE(chapterTwo != dots.end());
    CHECK(chapterOne > firstPageBreak);
    CHECK(std::find(chapterOne, chapterTwo, ".pa") != chapterTwo);

    // One index entry, one conditional page break before the scene break
    CHECK(CountStartingWith(dots, ".ix ") == 1);
    CHECK(CountStartingWith(dots, ".cp 4") == 1);
}


TEST_CASE("Book template - lays out as a title page and two chapters")
{
    ensureQApplication();

    cEditorCtrl editor;
    REQUIRE(editor.LoadFile(BOOK_TEMPLATE));
    cLayoutBase *layout = editor.GetLayout();
    layout->LayoutDocument(editor.GetDocument());

    REQUIRE(layout->GetNumberOfPages() == 3);

    // The running header starts on page 2; no page has a footer
    CHECK(HeaderFooterText(layout->GetPageHeaders(1)).empty());
    CHECK(HeaderFooterText(layout->GetPageHeaders(2)) == "Surname / TITLE / 2|");
    CHECK(HeaderFooterText(layout->GetPageHeaders(3)) == "Surname / TITLE / 3|");
    for (PAGE_T page = 1; page <= 3; page++)
    {
        CAPTURE(page);
        CHECK(HeaderFooterText(layout->GetPageFooters(page)).empty());
    }

    struct sExpected
    {
        std::string text;
        PAGE_T page;
        bool centered;
        COORD_T lineHeight;
    };
    std::vector<sExpected> expected = {
        {"Street Address", 1, false, 240},
        {"TITLE", 1, true, 480},
        {"by Your Name", 1, true, 480},
        {"Chapter One", 2, true, 480},
        {"#", 2, true, 480},
        {"Chapter Two", 3, true, 480},
    };
    for (const sExpected &line : expected)
    {
        CAPTURE(line.text);
        PARAGRAPH_T paragraph = FindParagraph(editor, line.text);
        REQUIRE(paragraph != NOT_SET);
        const sParagraphLayout *paragraphLayout = layout->GetParagraphLayout(paragraph);
        REQUIRE(paragraphLayout != nullptr);
        REQUIRE(paragraphLayout->lines.empty() == false);
        CHECK(paragraphLayout->lines[0].pagenumber == line.page);
        CHECK(paragraphLayout->lines[0].centerLine == line.centered);
        CHECK(paragraphLayout->lines[0].lineheight == line.lineHeight);
    }

    // The word count is right aligned on the author's line
    PARAGRAPH_T author = FindParagraph(editor, "Your NameAbout 80,000 words");
    REQUIRE(author != NOT_SET);
    REQUIRE(layout->GetParagraphLayout(author)->lines.empty() == false);
    CHECK(layout->GetParagraphLayout(author)->lines[0].rightLine);

    // The title is about a third of the way down the page
    PARAGRAPH_T title = FindParagraph(editor, "TITLE");
    COORD_T titleY = layout->GetParagraphLayout(title)->lines[0].pagey;
    COORD_T paperHeight = layout->GetPageInfo(1).paperheight;
    CHECK(titleY > paperHeight / 4);
    CHECK(titleY < paperHeight * 2 / 5);
}


TEST_CASE("Book template - saves back byte for byte")
{
    ensureQApplication();

    std::string original = ReadFileBytes(BOOK_TEMPLATE);
    REQUIRE(original.empty() == false);

    std::filesystem::path dir = UniqueTempDir("wordtsar_book_template");
    std::filesystem::path saved = dir / "BOOK.WS";
    {
        cEditorCtrl editor;
        REQUIRE(editor.LoadFile(BOOK_TEMPLATE));
        REQUIRE(editor.SaveFile(saved.string()));
    }
    std::string written = ReadFileBytes(saved);
    std::filesystem::remove_all(dir);

    CHECK(written.size() == original.size());
    CHECK(written == original);
}


TEST_CASE("Book template - the text is plain ASCII")
{
    std::string bytes = ReadFileBytes(BOOK_TEMPLATE);
    REQUIRE(bytes.size() > 128);

    // A 0x1D sequence: 0x1D, length (LE), type, body, length, 0x1D; the
    // length counts from the first length byte to the second length byte
    std::string text;
    size_t pos = 0;
    while (pos < bytes.size() && static_cast<unsigned char>(bytes[pos]) != 0x1A)
    {
        unsigned char c = static_cast<unsigned char>(bytes[pos]);
        if (c == 0x1D)
        {
            REQUIRE(pos + 2 < bytes.size());
            size_t length = static_cast<unsigned char>(bytes[pos + 1]) |
                            (static_cast<size_t>(static_cast<unsigned char>(bytes[pos + 2])) << 8);
            REQUIRE(pos + length + 2 < bytes.size());
            CHECK(static_cast<unsigned char>(bytes[pos + length + 2]) == 0x1D);
            pos += length + 3;
            continue;
        }
        text += static_cast<char>(c);
        pos++;
    }

    CHECK(text.empty() == false);
    for (unsigned char c : text)
    {
        CAPTURE(static_cast<int>(c));
        CHECK((c == '\r' || c == '\n' || (c >= 0x20 && c <= 0x7E)));
    }

    // Only ^Z padding after the text
    CHECK(pos >= 128);
    CHECK(std::all_of(bytes.begin() + static_cast<long>(pos), bytes.end(),
                      [](char c) { return static_cast<unsigned char>(c) == 0x1A; }));
}
