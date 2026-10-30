//////////////////////////////////////////////////////////////////////////////
//
// WordTsar - Wordstar clone for modern systems http://wordtsar.ca
// Copyright (C) 2018 Gerald Brandt
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
#include <cctype>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "src/core/codepage/cp437.h"
#include "src/core/include/config.h"
#include "src/core/document/document.h"
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

// Temporary file path unique to this test file
std::filesystem::path TempPath(void)
{
    static int counter = 0;
    counter++;
    std::filesystem::path path = std::filesystem::temp_directory_path();
    path /= "wordtsar_wordstarfile_test_" + std::to_string(counter) + ".ws";
    return path;
}

void WriteFileBytes(const std::filesystem::path &path, const std::string &bytes)
{
    std::ofstream file(path, std::ios::binary);
    file.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}

// 0x1D sequence: size (LE, counts start + size + type + body), type, body, 3 trailer bytes
std::string SequenceBytes(unsigned char type, const std::string &body)
{
    uint16_t size = static_cast<uint16_t>(body.size() + 4);
    std::string seq;
    seq += '\x1D';
    seq += static_cast<char>(size & 0xFF);
    seq += static_cast<char>(size >> 8);
    seq += static_cast<char>(type);
    seq += body;
    seq += static_cast<char>(size & 0xFF);
    seq += static_cast<char>(size >> 8);
    seq += '\x1D';
    return seq;
}

std::string LowerCase(std::string text)
{
    std::transform(text.begin(), text.end(), text.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return text;
}

// Regular files under dir (recursive) whose extension matches ext without regard to case, sorted
std::vector<std::filesystem::path> FindFilesWithExtension(const std::filesystem::path& dir, const std::string& ext)
{
    std::vector<std::filesystem::path> files;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(dir))
    {
        if (entry.is_regular_file() && LowerCase(entry.path().extension().string()) == ext)
        {
            files.push_back(entry.path());
        }
    }
    std::sort(files.begin(), files.end());
    return files;
}

void AppendUtf8(std::string& out, unsigned long cp)
{
    if (cp < 0x80)
    {
        out += static_cast<char>(cp);
    }
    else if (cp < 0x800)
    {
        out += static_cast<char>(0xC0 | (cp >> 6));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    }
    else if (cp < 0x10000)
    {
        out += static_cast<char>(0xE0 | (cp >> 12));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    }
    else
    {
        out += static_cast<char>(0xF0 | (cp >> 18));
        out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    }
}

// Word separators: ASCII whitespace and U+00A0
bool IsWordSeparator(const std::string& grapheme)
{
    return grapheme == " " || grapheme == "\t" || grapheme == "\r" || grapheme == "\n" ||
           grapheme == "\f" || grapheme == "\v" || grapheme == "\xC2\xA0";
}

// Appends the grapheme to the current word, or ends the current word on a separator
void AddGrapheme(std::vector<std::string>& words, std::string& word, const std::string& grapheme)
{
    if (IsWordSeparator(grapheme))
    {
        if (word.empty() == false)
        {
            words.push_back(word);
            word.clear();
        }
    }
    else
    {
        word += grapheme;
    }
}

// Words of the loaded document. Paragraphs starting with '.' (dot commands
// and comments) are skipped. A marker is a word separator when it is a tab
// or a non-break space and is dropped otherwise. Paragraph ends separate words.
std::vector<std::string> DocumentWords(cDocument* doc)
{
    std::vector<std::string> words;
    std::string word;
    PARAGRAPH_T count = doc->GetNumberofParagraphs();
    for (PARAGRAPH_T para = 0; para < count; para++)
    {
        std::string raw = doc->GetParagraphText(para);
        if (raw.empty() == false && raw[0] == '.')
        {
            continue;
        }
        POSITION_T start = 0;
        POSITION_T end = 0;
        doc->GetParagraphStartandEnd(para, start, end);
        for (POSITION_T pos = start; pos <= end; pos++)
        {
            POSITION_T at = pos;
            std::string grapheme = doc->GetChar(at);
            if (grapheme.size() == 1 && static_cast<unsigned char>(grapheme[0]) == MARKER_CHAR)
            {
                eModifiers control = doc->GetControlChar(pos);
                if (control == STYLE_TAB || control == STYLE_NOBREAK_SPACE)
                {
                    AddGrapheme(words, word, " ");
                }
                continue;
            }
            AddGrapheme(words, word, grapheme);
        }
        AddGrapheme(words, word, "\r");
    }
    return words;
}

// Words of a text file read as CP437, up to the first ^Z
std::vector<std::string> Cp437FileWords(const std::filesystem::path& path)
{
    std::ifstream file(path, std::ios::binary);
    std::string bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    cCodePage437 cp;
    std::vector<std::string> words;
    std::string word;
    for (char ch : bytes)
    {
        unsigned char c = static_cast<unsigned char>(ch);
        if (c == 0x1A)
        {
            break;
        }
        unsigned long cpoint = cp.toUTF8(c);
        std::string grapheme;
        AppendUtf8(grapheme, cpoint == UINT32_MAX ? c : cpoint);
        AddGrapheme(words, word, grapheme);
    }
    AddGrapheme(words, word, "\r");
    return words;
}

std::string JoinWords(const std::vector<std::string>& words, size_t from, size_t to)
{
    std::string out;
    for (size_t i = from; i < to && i < words.size(); i++)
    {
        if (out.empty() == false)
        {
            out += ' ';
        }
        out += words[i];
    }
    return out;
}

// One stretch of words that differs between the two sequences
struct sWordHunk
{
    size_t aIndex;
    size_t aCount;
    size_t bIndex;
    size_t bCount;
};

// Differences between two word sequences. After a mismatch both sides are
// resynchronised at the nearest point where three words (or the rest of both
// sequences) agree, searching up to `window` words ahead. Without such a point
// the rest of both sequences is one hunk.
std::vector<sWordHunk> DiffWords(const std::vector<std::string>& a, const std::vector<std::string>& b, size_t window = 200)
{
    const size_t run = 3;
    auto agrees = [&](size_t i, size_t j) -> bool
    {
        if (i == a.size() && j == b.size())
        {
            return true;
        }
        size_t n = std::min(run, std::min(a.size() - i, b.size() - j));
        if (n == 0)
        {
            return false;
        }
        for (size_t k = 0; k < n; k++)
        {
            if (a[i + k] != b[j + k])
            {
                return false;
            }
        }
        return n == run || (i + n == a.size() && j + n == b.size());
    };

    std::vector<sWordHunk> hunks;
    size_t i = 0;
    size_t j = 0;
    while (i < a.size() || j < b.size())
    {
        if (i < a.size() && j < b.size() && a[i] == b[j])
        {
            i++;
            j++;
            continue;
        }
        bool found = false;
        for (size_t k = 1; k <= 2 * window && found == false; k++)
        {
            for (size_t x = 0; x <= k; x++)
            {
                size_t y = k - x;
                if (x > window || y > window || i + x > a.size() || j + y > b.size())
                {
                    continue;
                }
                if (agrees(i + x, j + y))
                {
                    hunks.push_back({i, x, j, y});
                    i += x;
                    j += y;
                    found = true;
                    break;
                }
            }
        }
        if (found == false)
        {
            hunks.push_back({i, a.size() - i, j, b.size() - j});
            break;
        }
    }
    return hunks;
}

}


TEST_CASE("cWordstarFile::LoadFile - text bytes with and without the high bit")
{
    cDocument doc;
    doc.Clear();
    cWordstarFile file;
    file.mDocument = &doc;

    SUBCASE("plain ASCII text loads as is")
    {
        std::filesystem::path path = TempPath();
        WriteFileBytes(path, std::string("Hello\x0D\x0A\x1A", 8));
        CHECK(file.LoadFile(path.string()));
        CHECK(doc.GetParagraphText(0).rfind("Hello", 0) == 0);
        std::filesystem::remove(path);
    }

    SUBCASE("a high-bit ASCII byte loads with the high bit stripped")
    {
        std::filesystem::path path = TempPath();
        WriteFileBytes(path, std::string("A\xC2" "B\x0D\x0A\x1A", 6));    // 0xC2 = 'B' | 0x80
        CHECK(file.LoadFile(path.string()));
        CHECK(doc.GetParagraphText(0).rfind("ABB", 0) == 0);
        std::filesystem::remove(path);
    }

    SUBCASE("a 0x80 byte loads as U+0080, not as an invalid code point")
    {
        std::filesystem::path path = TempPath();
        WriteFileBytes(path, std::string("A\x80" "B\x0D\x0A\x1A", 6));
        CHECK(file.LoadFile(path.string()));
        CHECK(doc.GetParagraphText(0).rfind("A\xC2\x80" "B", 0) == 0);
        std::filesystem::remove(path);
    }
}


TEST_CASE("cWordstarFile::LoadFile - colour sequences: palette index range")
{
    cDocument document;
    document.Clear();
    cWordstarFile file;
    file.mDocument = &document;

    // a colour sequence between "Hi" and "!" is a marker at position 2
    auto loadColor = [&](unsigned char colornumber, sSeqRGBColor& color) -> bool
    {
        std::string body;
        body += static_cast<char>(colornumber);
        body += '\0';
        std::filesystem::path path = TempPath();
        WriteFileBytes(path, "Hi" + SequenceBytes(SEQ_COLOR, body) + "!\x0D\x0A");
        bool loaded = file.LoadFile(path.string());
        std::filesystem::remove(path);
        if (loaded == false || document.GetControlChar(2) != STYLE_INTERNAL_COLOR)
        {
            return false;
        }
        return document.GetColor(2, color);
    };

    SUBCASE("index 4 loads as red")
    {
        sSeqRGBColor color;
        REQUIRE(loadColor(4, color));
        CHECK(color.red == 170);
        CHECK(color.green == 0);
        CHECK(color.blue == 0);
    }

    SUBCASE("index 15 loads as white")
    {
        sSeqRGBColor color;
        REQUIRE(loadColor(15, color));
        CHECK(color.red == 255);
        CHECK(color.green == 255);
        CHECK(color.blue == 255);
    }

    SUBCASE("an index outside the palette loads as Automatic")
    {
        for (unsigned char index : {16, 200, 255})
        {
            CAPTURE(static_cast<int>(index));
            sSeqRGBColor color;
            REQUIRE(loadColor(index, color));
            CHECK(color.IsDefault());
        }
    }
}


TEST_CASE("cWordstarFile::SaveFile - returns false when the file cannot be written")
{
    ensureQApplication();
    cEditorCtrl editor;
    editor.GetDocument()->Insert("Hello\r");
    cWordstarFile file(&editor);
    POSITION_T size = editor.GetDocument()->GetTextSize();

    SUBCASE("a writable path saves")
    {
        std::filesystem::path path = TempPath();
        CHECK(file.SaveFile(path.string(), size));
        CHECK(std::filesystem::file_size(path) > 0);
        std::filesystem::remove(path);
    }

    SUBCASE("a path that cannot be opened fails")
    {
        std::filesystem::path path = std::filesystem::temp_directory_path() / "wordtsar_no_such_dir" / "x.ws";
        CHECK_FALSE(file.SaveFile(path.string(), size));
    }

    SUBCASE("a full disk fails")
    {
        if (std::filesystem::exists("/dev/full"))
        {
            CHECK_FALSE(file.SaveFile("/dev/full", size));
        }
    }
}


TEST_CASE("cWordstarFile::LoadFile - sequences: bounded reads and the end of the file")
{
    cDocument document;
    document.Clear();
    cDocument* doc = &document;
    cWordstarFile file;
    file.mDocument = doc;

    SUBCASE("an unknown sequence is skipped and text after it loads, without a ^Z")
    {
        std::filesystem::path path = TempPath();
        WriteFileBytes(path, "Hello" + SequenceBytes(SEQ_RESERVED1, "abcd") + "World\x0D\x0A");
        CHECK(file.LoadFile(path.string()));
        CHECK(doc->GetParagraphText(0).rfind("HelloWorld", 0) == 0);
        std::filesystem::remove(path);
    }

    SUBCASE("a footnote sequence longer than the file reads only what is there")
    {
        std::filesystem::path path = TempPath();
        WriteFileBytes(path, std::string("Hi\x1D\xFF\xFF\x03" "abc", 9));    // size 0xFFFF, 3 body bytes
        CHECK(file.LoadFile(path.string()));
        CHECK(doc->GetParagraphText(0).rfind("Hi", 0) == 0);
        CHECK(doc->GetTextSize() < 10);
        std::filesystem::remove(path);
    }

    SUBCASE("an unknown sequence longer than the file stops the load cleanly")
    {
        std::filesystem::path path = TempPath();
        WriteFileBytes(path, std::string("Hi\x1D\xFF\xFF\x07" "abc", 9));
        CHECK(file.LoadFile(path.string()));
        CHECK(doc->GetParagraphText(0).rfind("Hi", 0) == 0);
        CHECK(doc->GetTextSize() < 10);
        std::filesystem::remove(path);
    }
}


TEST_CASE("cWordstarFile::LoadFile - footnote sequences")
{
    cDocument document;
    document.Clear();
    cWordstarFile file;
    file.mDocument = &document;

    // a footnote between "Hi" and "!" is a marker at position 2
    auto loadNote = [&](const std::string &body) -> bool
    {
        std::filesystem::path path = TempPath();
        WriteFileBytes(path, "Hi" + SequenceBytes(SEQ_FOOTNOTE, body) + "!\x0D\x0A");
        bool loaded = file.LoadFile(path.string());
        std::filesystem::remove(path);
        return loaded;
    };
    auto hasFootnote = [&](void) -> bool
    {
        return document.GetCharNoAdvance(2)[0] == static_cast<char>(MARKER_CHAR) &&
               document.GetControlChar(2) == STYLE_FOOTNOTE;
    };

    SUBCASE("a simple note is inserted")
    {
        CHECK(loadNote(std::string("\x00\x00\x00\x00\x00", 5) + "note text"));
        CHECK(hasFootnote());
    }

    SUBCASE("a note with an embedded footnote sub-sequence is inserted")
    {
        std::string body("\x00\x00\x01\x00", 4);               // tag = 1
        body += std::string(5, '\x00');                          // bytes 4-8
        body += std::string("\x00\x00\x01\x03", 4);            // sub-sequence data, bytes 9-12
        body += "xx\x1D";                                       // rest of the sub-sequence
        body += "note text";
        CHECK(loadNote(body));
        CHECK(hasFootnote());
    }

    SUBCASE("a body shorter than the note header is dropped")
    {
        CHECK(loadNote(std::string("\x00\x00", 2)));
        CHECK_FALSE(hasFootnote());
        CHECK(document.GetParagraphText(0).rfind("Hi!", 0) == 0);
    }

    SUBCASE("an embedded footnote without its sub-sequence data is dropped")
    {
        CHECK(loadNote(std::string("\x00\x00\x01\x00", 4)));
        CHECK_FALSE(hasFootnote());
        CHECK(document.GetParagraphText(0).rfind("Hi!", 0) == 0);
    }

    SUBCASE("an embedded footnote whose sub-sequence never ends is dropped")
    {
        std::string body("\x00\x00\x01\x00", 4);
        body += std::string(9, 'a');                              // bytes 4-12, no 0x1D after them
        body += "no end";
        CHECK(loadNote(body));
        CHECK_FALSE(hasFootnote());
        CHECK(document.GetParagraphText(0).rfind("Hi!", 0) == 0);
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Loads every .WS file under the directory named by WORDTSAR_ARCHIVE_DIR
/// (searched recursively, extension matched without regard to case) through
/// cEditorCtrl::LoadFile(). A file fails when the load returns false, throws,
/// or leaves an empty document. All failures are reported together. Without
/// WORDTSAR_ARCHIVE_DIR the test does nothing.
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("cEditorCtrl::LoadFile - every .WS file in WORDTSAR_ARCHIVE_DIR loads")
{
    const char* archiveEnv = std::getenv("WORDTSAR_ARCHIVE_DIR");
    if (archiveEnv == nullptr || archiveEnv[0] == '\0')
    {
        MESSAGE("WORDTSAR_ARCHIVE_DIR is not set, archive load test skipped");
        return;
    }

    const std::filesystem::path archiveDir(archiveEnv);
    REQUIRE_MESSAGE(std::filesystem::is_directory(archiveDir),
                    "WORDTSAR_ARCHIVE_DIR is not a directory: " << archiveDir.string());

    std::vector<std::filesystem::path> files = FindFilesWithExtension(archiveDir, ".ws");
    REQUIRE_MESSAGE(files.empty() == false, "no .WS files in " << archiveDir.string());

    ensureQApplication();

    std::vector<std::string> failures;
    for (const std::filesystem::path& path : files)
    {
        std::string error;
        try
        {
            cEditorCtrl editor;
            if (editor.LoadFile(path.string()) == false)
            {
                error = "LoadFile returned false";
            }
            else if (editor.GetDocument()->GetTextSize() == 0)
            {
                error = "document is empty after load";
            }
        }
        catch (const std::exception& e)
        {
            error = std::string("exception: ") + e.what();
        }
        catch (...)
        {
            error = "unknown exception";
        }

        if (error.empty() == false)
        {
            failures.push_back(path.string() + ": " + error);
        }
    }

    std::string report;
    for (const std::string& failure : failures)
    {
        report += "\n  " + failure;
    }
    MESSAGE(files.size() << " .WS files loaded, " << failures.size() << " failed" << report);
    CHECK_MESSAGE(failures.empty(), failures.size() << " of " << files.size() << " .WS files failed to load" << report);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// For every NAME.WS under WORDTSAR_ARCHIVE_DIR (recursive) with a NAME.TXT
/// in the same folder (base name and extension matched without regard to
/// case), compares the word sequence of the loaded document with the word
/// sequence of the .TXT read as CP437. Words are split on whitespace; line
/// breaks do not count. Dot-command paragraphs are left out on the .WS side.
/// Word differences are reported as messages only. A pair fails when the
/// .WS load returns false or throws, or the comparison throws. Without
/// WORDTSAR_ARCHIVE_DIR the test does nothing.
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("cEditorCtrl::LoadFile - .WS text matches the .TXT beside it in WORDTSAR_ARCHIVE_DIR")
{
    const char* archiveEnv = std::getenv("WORDTSAR_ARCHIVE_DIR");
    if (archiveEnv == nullptr || archiveEnv[0] == '\0')
    {
        MESSAGE("WORDTSAR_ARCHIVE_DIR is not set, archive text test skipped");
        return;
    }

    const std::filesystem::path archiveDir(archiveEnv);
    REQUIRE_MESSAGE(std::filesystem::is_directory(archiveDir),
                    "WORDTSAR_ARCHIVE_DIR is not a directory: " << archiveDir.string());

    // folder + lower-case base name -> .TXT path
    std::map<std::string, std::filesystem::path> texts;
    for (const std::filesystem::path& txt : FindFilesWithExtension(archiveDir, ".txt"))
    {
        texts[txt.parent_path().string() + "/" + LowerCase(txt.stem().string())] = txt;
    }

    std::vector<std::pair<std::filesystem::path, std::filesystem::path>> pairs;
    for (const std::filesystem::path& ws : FindFilesWithExtension(archiveDir, ".ws"))
    {
        auto found = texts.find(ws.parent_path().string() + "/" + LowerCase(ws.stem().string()));
        if (found != texts.end())
        {
            pairs.emplace_back(ws, found->second);
        }
    }
    if (pairs.empty())
    {
        MESSAGE("no .WS file with a .TXT beside it in " << archiveDir.string());
        return;
    }

    ensureQApplication();

    const size_t shownHunks = 5;
    const size_t context = 4;
    const size_t shownWords = 12;
    std::ostringstream report;
    std::vector<std::string> failures;
    size_t identical = 0;
    for (const auto& pair : pairs)
    {
        std::string name = std::filesystem::relative(pair.first, archiveDir).string();
        try
        {
            cEditorCtrl editor;
            if (editor.LoadFile(pair.first.string()) == false)
            {
                failures.push_back(name + ": LoadFile returned false");
                report << "\n" << name << ": LoadFile returned false";
                continue;
            }
            std::vector<std::string> wsWords = DocumentWords(editor.GetDocument());
            std::vector<std::string> txtWords = Cp437FileWords(pair.second);
            std::vector<sWordHunk> hunks = DiffWords(wsWords, txtWords);

            report << "\n" << name << ": " << wsWords.size() << " words in .WS, "
                   << txtWords.size() << " words in .TXT, ";
            if (hunks.empty())
            {
                identical++;
                report << "identical";
                continue;
            }
            report << hunks.size() << " differences";
            for (size_t h = 0; h < hunks.size() && h < shownHunks; h++)
            {
                const sWordHunk& hunk = hunks[h];
                size_t before = hunk.aIndex >= context ? hunk.aIndex - context : 0;
                report << "\n  at WS word " << hunk.aIndex << " / TXT word " << hunk.bIndex
                       << ", after \"" << JoinWords(wsWords, before, hunk.aIndex) << "\""
                       << "\n    WS : [" << JoinWords(wsWords, hunk.aIndex, hunk.aIndex + std::min(hunk.aCount, shownWords))
                       << (hunk.aCount > shownWords ? " ..." : "") << "] (" << hunk.aCount << " words) then \""
                       << JoinWords(wsWords, hunk.aIndex + hunk.aCount, hunk.aIndex + hunk.aCount + context) << "\""
                       << "\n    TXT: [" << JoinWords(txtWords, hunk.bIndex, hunk.bIndex + std::min(hunk.bCount, shownWords))
                       << (hunk.bCount > shownWords ? " ..." : "") << "] (" << hunk.bCount << " words) then \""
                       << JoinWords(txtWords, hunk.bIndex + hunk.bCount, hunk.bIndex + hunk.bCount + context) << "\"";
            }
            if (hunks.size() > shownHunks)
            {
                report << "\n  ... " << hunks.size() - shownHunks << " more";
            }
        }
        catch (const std::exception& e)
        {
            failures.push_back(name + ": exception: " + e.what());
            report << "\n" << name << ": exception: " << e.what();
        }
        catch (...)
        {
            failures.push_back(name + ": unknown exception");
            report << "\n" << name << ": unknown exception";
        }
    }

    MESSAGE(pairs.size() << " .WS/.TXT pairs, " << identical << " identical, "
            << failures.size() << " failed" << report.str());
    std::string failed;
    for (const std::string& failure : failures)
    {
        failed += "\n  " + failure;
    }
    CHECK_MESSAGE(failures.empty(), failures.size() << " of " << pairs.size() << " pairs failed" << failed);
}
