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
#include <random>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "src/core/codepage/cp437.h"
#include "src/core/codepage/cp737.h"
#include "src/core/codepage/cp850.h"
#include "src/core/include/config.h"
#include "src/core/document/document.h"
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

// Temporary file path unique to this test file
std::filesystem::path TempPath(void)
{
    static int counter = 0;
    counter++;
    std::filesystem::path path = std::filesystem::temp_directory_path();
    path /= "wordtsar_wordstarfile_test_" + std::to_string(counter) + ".ws";
    return path;
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

// A WordStar 7 file as cWordstarFile::SaveFile() writes it: the 128 byte
// header, the body, then ^Z up to the next multiple of 128 bytes
std::string WordStar7File(const std::string &body)
{
    size_t total = 128 + body.size();
    total += 128 - (total % 128);

    sWSHeader header;
    memset(&header, 0, sizeof(header));
    header.version = 0x70;
    memcpy(header.driver, "LASERJET", 9);
    header.styles = static_cast<uint32_t>(total);

    std::string bytes("\x1D\x7D\x00\x00", 4);
    bytes.append(reinterpret_cast<const char*>(&header), sizeof(header));
    bytes.append("\x7D\x00\x1D", 3);
    bytes += body;
    bytes.append(total - bytes.size(), '\x1A');
    return bytes;
}

std::string ReadFileBytes(const std::filesystem::path &path)
{
    std::ifstream file(path, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
}

// Loads bytes into the editor's document through cWordstarFile, saves the
// document back and returns the saved bytes
std::string RoundTrip(cEditorCtrl &editor, const std::string &bytes)
{
    cWordstarFile loader;
    loader.mDocument = editor.GetDocument();
    cWordstarFile saver(&editor);
    std::filesystem::path in = TempPath();
    std::filesystem::path out = TempPath();
    WriteFileBytes(in, bytes);
    std::string saved;
    if (loader.LoadFile(in.string()) &&
        saver.SaveFile(out.string(), editor.GetDocument()->GetTextSize()))
    {
        saved = ReadFileBytes(out);
    }
    std::filesystem::remove(in);
    std::filesystem::remove(out);
    return saved;
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
// and comments) are skipped. A marker is a word separator when it is a tab,
// a non-break space or a line or page break, and is dropped otherwise.
// Paragraph ends separate words.
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
                if (control == STYLE_TAB || control == STYLE_NOBREAK_SPACE || control == STYLE_RETURN ||
                    control == STYLE_LINEFEED || control == STYLE_FORMFEED)
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


TEST_CASE("cWordstarFile - a footnote sequence saves back byte for byte")
{
    ensureQApplication();
    cEditorCtrl editor;
    editor.GetDocument()->Clear();

    SUBCASE("a simple note")
    {
        std::string body = std::string("\x02\x00\x00\x00\x00", 5) + "note text";
        std::string input = WordStar7File("Hi" + SequenceBytes(SEQ_FOOTNOTE, body) + "!\x0D\x0A");
        CHECK(RoundTrip(editor, input) == input);
    }

    SUBCASE("a note with an embedded footnote sub-sequence")
    {
        std::string body("\x01\x00\x01\x00", 4);                // tag = 1
        body += std::string("\x00\x1D\x09\x00\x03", 5);         // bytes 4-8
        body += std::string("\x00\x00\x01\x03", 4);             // sub-sequence data, bytes 9-12
        body += std::string("\x00\x09\x00\x1D", 4);             // rest of the sub-sequence
        body += "note\x0D\x0Atext";
        std::string input = WordStar7File("Hi" + SequenceBytes(SEQ_FOOTNOTE, body) + "!\x0D\x0A");
        CHECK(RoundTrip(editor, input) == input);
    }
}


TEST_CASE("cWordstarFile - an endnote sequence loads as a marker and saves back byte for byte")
{
    ensureQApplication();
    cEditorCtrl editor;
    editor.GetDocument()->Clear();

    std::string body = std::string("\x01\x00\x00\x00\x00", 5) + "end text";
    std::string input = WordStar7File("Hi" + SequenceBytes(SEQ_ENDNOTE, body) + "!\x0D\x0A");
    std::string saved = RoundTrip(editor, input);

    cDocument* doc = editor.GetDocument();
    CHECK(doc->GetCharNoAdvance(2)[0] == static_cast<char>(MARKER_CHAR));
    CHECK(doc->GetControlChar(2) == STYLE_ENDNOTE);
    CHECK(saved == input);
}


TEST_CASE("cWordstarFile - an annotation sequence loads as a marker and saves back byte for byte")
{
    ensureQApplication();
    cEditorCtrl editor;
    editor.GetDocument()->Clear();

    std::string body = std::string("\x01\x00\x00\x00\x00", 5) + "annotation text";
    std::string input = WordStar7File("Hi" + SequenceBytes(SEQ_ANNOTATION, body) + "!\x0D\x0A");
    std::string saved = RoundTrip(editor, input);

    cDocument* doc = editor.GetDocument();
    CHECK(doc->GetCharNoAdvance(2)[0] == static_cast<char>(MARKER_CHAR));
    CHECK(doc->GetControlChar(2) == STYLE_FOOTNOTE);
    sNote note{};
    CHECK(doc->GetNote(2, note));
    CHECK(note.kind == NOTE_KIND_ANNOTATION);
    CHECK(saved == input);
}


TEST_CASE("cWordstarFile - a comment sequence loads as a marker and saves back byte for byte")
{
    ensureQApplication();
    cEditorCtrl editor;
    editor.GetDocument()->Clear();

    std::string body = std::string("\x01\x00\x00\x00\x00", 5) + "comment text";
    std::string input = WordStar7File("Hi" + SequenceBytes(SEQ_COMMENT, body) + "!\x0D\x0A");
    std::string saved = RoundTrip(editor, input);

    cDocument* doc = editor.GetDocument();
    CHECK(doc->GetCharNoAdvance(2)[0] == static_cast<char>(MARKER_CHAR));
    CHECK(doc->GetControlChar(2) == STYLE_FOOTNOTE);
    sNote note{};
    CHECK(doc->GetNote(2, note));
    CHECK(note.kind == NOTE_KIND_COMMENT);
    CHECK(saved == input);
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


namespace
{

// .HE, .H1-.H5, .FO, .F1-.F5 without regard to case
bool IsHeaderFooterCommand(const std::string& text)
{
    if (text.size() < 3 || text[0] != '.')
    {
        return false;
    }
    char a = static_cast<char>(std::tolower(static_cast<unsigned char>(text[1])));
    char b = static_cast<char>(std::tolower(static_cast<unsigned char>(text[2])));
    if (a == 'h')
    {
        return b == 'e' || (b >= '1' && b <= '5');
    }
    if (a == 'f')
    {
        return b == 'o' || (b >= '1' && b <= '5');
    }
    return false;
}

// Category of a difference at a marker of type control
std::string MarkerCategory(eModifiers control)
{
    if (control == STYLE_TAB)
    {
        return "tab";
    }
    if (control == STYLE_FOOTNOTE || control == STYLE_ENDNOTE)
    {
        return "note";
    }
    return "formatting";
}

bool SameWSFont(const sWSFont& a, const sWSFont& b)
{
    return a.width == b.width && a.height == b.height && a.style == b.style &&
           a.prevwidth == b.prevwidth && a.prevheight == b.prevheight && a.prevstyle == b.prevstyle;
}

// Category of the first difference between paragraph para of a and of b,
// or an empty string when they agree. Text graphemes are compared as UTF-8;
// markers by control type and, for tabs, fonts, colours and variables, by
// their attribute values, for notes by their sequence body bytes. Differences in a dot-command paragraph are
// "dot command", in a header or footer command "header-footer".
std::string CompareParagraph(cDocument* a, cDocument* b, PARAGRAPH_T para)
{
    std::string textA = a->GetParagraphText(para);
    std::string textB = b->GetParagraphText(para);
    std::string kind;
    if (IsHeaderFooterCommand(textA) || IsHeaderFooterCommand(textB))
    {
        kind = "header-footer";
    }
    else if ((textA.empty() == false && textA[0] == '.') || (textB.empty() == false && textB[0] == '.'))
    {
        kind = "dot command";
    }

    POSITION_T startA = 0;
    POSITION_T endA = 0;
    POSITION_T startB = 0;
    POSITION_T endB = 0;
    a->GetParagraphStartandEnd(para, startA, endA);
    b->GetParagraphStartandEnd(para, startB, endB);
    POSITION_T lengthA = endA - startA + 1;
    POSITION_T lengthB = endB - startB + 1;

    std::string category;
    for (POSITION_T i = 0; i < std::max(lengthA, lengthB) && category.empty(); i++)
    {
        bool haveA = i < lengthA;
        bool haveB = i < lengthB;
        std::string chA = haveA ? a->GetCharNoAdvance(startA + i) : std::string();
        std::string chB = haveB ? b->GetCharNoAdvance(startB + i) : std::string();
        bool markerA = chA.size() == 1 && static_cast<unsigned char>(chA[0]) == MARKER_CHAR;
        bool markerB = chB.size() == 1 && static_cast<unsigned char>(chB[0]) == MARKER_CHAR;
        eModifiers controlA = markerA ? a->GetControlChar(startA + i) : STYLE_EOF;
        eModifiers controlB = markerB ? b->GetControlChar(startB + i) : STYLE_EOF;

        if (markerA == false && markerB == false)
        {
            if (chA != chB)
            {
                category = "text";
            }
            continue;
        }
        if (markerA != markerB || controlA != controlB)
        {
            category = MarkerCategory(markerA ? controlA : controlB);
            if (category == "formatting")
            {
                category += markerA == markerB ? " (marker type)" : " (marker against text)";
            }
            continue;
        }
        POSITION_T posA = startA + i;
        POSITION_T posB = startB + i;
        if (controlA == STYLE_TAB)
        {
            sWSTab tabA = a->GetTab(posA);
            sWSTab tabB = b->GetTab(posB);
            if (tabA.tabsize != tabB.tabsize || tabA.abstabsize != tabB.abstabsize ||
                tabA.type != tabB.type || tabA.size != tabB.size)
            {
                category = "tab";
            }
        }
        else if (controlA == STYLE_FONT1)
        {
            sInternalFonts fontA{};
            sInternalFonts fontB{};
            bool gotA = a->GetFont(posA, fontA);
            bool gotB = b->GetFont(posB, fontB);
            if (gotA != gotB || fontA.fontname != fontB.fontname || fontA.size != fontB.size ||
                fontA.name != fontB.name || fontA.haveWSFont != fontB.haveWSFont ||
                (fontA.haveWSFont && SameWSFont(fontA.wsfont, fontB.wsfont) == false))
            {
                category = "formatting (font)";
            }
        }
        else if (controlA == STYLE_INTERNAL_COLOR)
        {
            sSeqRGBColor colorA{};
            sSeqRGBColor colorB{};
            bool gotA = a->GetColor(posA, colorA);
            bool gotB = b->GetColor(posB, colorB);
            if (gotA != gotB || colorA.red != colorB.red || colorA.green != colorB.green ||
                colorA.blue != colorB.blue || colorA.alpha != colorB.alpha)
            {
                category = "formatting (colour)";
            }
        }
        else if (controlA == STYLE_VARIABLE)
        {
            if (a->GetVariable(posA) != b->GetVariable(posB))
            {
                category = "formatting (variable)";
            }
        }
        else if (controlA == STYLE_FOOTNOTE || controlA == STYLE_ENDNOTE)
        {
            sNote noteA{};
            sNote noteB{};
            bool gotA = a->GetNote(posA, noteA);
            bool gotB = b->GetNote(posB, noteB);
            if (gotA != gotB || noteA.kind != noteB.kind || noteA.raw != noteB.raw)
            {
                category = "note";
            }
        }
    }

    if (category.empty() || kind.empty())
    {
        return category;
    }
    return kind;
}

}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// For every WordStar file under WORDTSAR_ARCHIVE_DIR (recursive): files
/// with extension .WS (any case), and files of any other extension that
/// cWordstarFile::CheckType() accepts. Each file is loaded, saved
/// through cWordstarFile::SaveFile() into a temporary directory, the saved
/// copy is loaded, and the two documents are compared paragraph by
/// paragraph (see CompareParagraph()). A file fails when the two loads
/// differ, a load or save fails, or anything throws. A failing file listed
/// in kKnownRoundTripFailures is reported as a message; an unlisted failing
/// file, a listed file that passes and a listed file that is not scanned
/// (missing from the archive or not accepted) fail the test. Byte differences
/// between the original and the saved file are reported as messages only.
/// Output names files, paragraph indices and categories, never text.
/// Without WORDTSAR_ARCHIVE_DIR the test does nothing.
///
/////////////////////////////////////////////////////////////////////////////
// Archive files (relative to WORDTSAR_ARCHIVE_DIR) whose two loads differ
const std::set<std::string> kKnownRoundTripFailures =
{
    "ARTICLES/FORMFEED.WS",
    "DEFAULT/PRINT.TST",
    "DEFAULT/REVIEW.DOC",
    "LSRBOX/LSRBOX.WS",
    "MACROS/HOLYMAC/-HOLYMAC.WS",
    "MACROS/HOLYMAC/4MAC2",
    "MACROS/HOLYMAC/4MAC3",
    "MICKEE/MICKEE.WS",
    "PRINT.TST",
    "PRINTER.PS",
    "PRINTERS/FONTCRIB.PS",
    "PRINTERS/fontcrib.ws",
    "PSPRINT.TST",
    "REF/ASCIITAB.WS",
    "REF/BOOKLET.WS",
    "REF/CONTROL.COD",
    "REF/SYMBOL.CHT",
    "REF/WINGDING.CHT",
    "REVIEW.DOC",
    "RTF-RJS/NOVEL.WS",
    "RTF-RJS/SKIPTEST.WS",
};

TEST_CASE("cWordstarFile - every WordStar file in WORDTSAR_ARCHIVE_DIR loads the same after a save")
{
    const char* archiveEnv = std::getenv("WORDTSAR_ARCHIVE_DIR");
    if (archiveEnv == nullptr || archiveEnv[0] == '\0')
    {
        MESSAGE("WORDTSAR_ARCHIVE_DIR is not set, archive round-trip test skipped");
        return;
    }

    const std::filesystem::path archiveDir(archiveEnv);
    REQUIRE_MESSAGE(std::filesystem::is_directory(archiveDir),
                    "WORDTSAR_ARCHIVE_DIR is not a directory: " << archiveDir.string());

    ensureQApplication();

    std::vector<std::filesystem::path> files;
    size_t wsCount = 0;
    size_t otherCount = 0;
    std::map<std::string, size_t> otherExtensions;
    {
        cWordstarFile checker;
        for (const auto& entry : std::filesystem::recursive_directory_iterator(archiveDir))
        {
            if (entry.is_regular_file() == false)
            {
                continue;
            }
            std::string ext = LowerCase(entry.path().extension().string());
            if (ext == ".ws")
            {
                files.push_back(entry.path());
                wsCount++;
            }
            else if (checker.CheckType(entry.path().string()))
            {
                files.push_back(entry.path());
                otherCount++;
                otherExtensions[ext.empty() ? "(none)" : ext]++;
            }
        }
    }
    std::sort(files.begin(), files.end());
    REQUIRE_MESSAGE(files.empty() == false, "no WordStar files in " << archiveDir.string());

    std::filesystem::path tempDir = UniqueTempDir("wordtsar_archive_roundtrip");

    const size_t shownParagraphs = 10;
    size_t passed = 0;
    size_t failed = 0;
    size_t knownFailed = 0;
    std::vector<std::string> byteDifferences;
    std::map<std::string, std::vector<std::string>> filesByCategory;
    size_t index = 0;
    std::set<std::string> scanned;
    for (const std::filesystem::path& path : files)
    {
        std::string name = std::filesystem::relative(path, archiveDir).string();
        scanned.insert(name);
        std::filesystem::path saved = tempDir / (std::to_string(index++) + ".ws");
        std::map<std::string, size_t> categoryCounts;
        std::ostringstream details;
        size_t shown = 0;
        auto record = [&](const std::string& category, long paragraph)
        {
            categoryCounts[category]++;
            if (shown < shownParagraphs)
            {
                details << "\n  ";
                if (paragraph >= 0)
                {
                    details << "paragraph " << paragraph << ": ";
                }
                details << category;
                shown++;
            }
        };

        try
        {
            cEditorCtrl first;
            if (first.LoadFile(path.string()) == false)
            {
                record("load", -1);
            }
            else
            {
                cWordstarFile saver(&first);
                saver.SetSilent(true);
                if (saver.SaveFile(saved.string(), first.GetDocument()->GetTextSize()) == false)
                {
                    record("save", -1);
                }
                else
                {
                    if (ReadFileBytes(path) != ReadFileBytes(saved))
                    {
                        byteDifferences.push_back(name);
                    }
                    cEditorCtrl second;
                    if (second.LoadFile(saved.string()) == false)
                    {
                        record("reload", -1);
                    }
                    else
                    {
                        cDocument* a = first.GetDocument();
                        cDocument* b = second.GetDocument();
                        PARAGRAPH_T countA = a->GetNumberofParagraphs();
                        PARAGRAPH_T countB = b->GetNumberofParagraphs();
                        if (countA != countB)
                        {
                            record("paragraph count", -1);
                        }
                        for (PARAGRAPH_T para = 0; para < std::min(countA, countB); para++)
                        {
                            std::string category = CompareParagraph(a, b, para);
                            if (category.empty() == false)
                            {
                                record(category, static_cast<long>(para));
                            }
                        }
                    }
                }
            }
        }
        catch (const std::exception&)
        {
            record("exception", -1);
        }
        catch (...)
        {
            record("exception", -1);
        }
        std::filesystem::remove(saved);

        bool known = kKnownRoundTripFailures.count(name) > 0;
        if (categoryCounts.empty())
        {
            passed++;
            CHECK_MESSAGE(known == false, name << ": passes now, remove it from kKnownRoundTripFailures");
            continue;
        }
        failed++;
        std::ostringstream counts;
        for (const auto& category : categoryCounts)
        {
            filesByCategory[category.first].push_back(name);
            counts << " " << category.first << "=" << category.second;
        }
        if (known)
        {
            knownFailed++;
            MESSAGE(name << ": known round-trip failure:" << counts.str() << details.str());
        }
        else
        {
            CHECK_MESSAGE(false, name << ": two loads differ:" << counts.str() << details.str());
        }
    }
    std::filesystem::remove_all(tempDir);

    for (const std::string& name : kKnownRoundTripFailures)
    {
        if (scanned.count(name) == 0)
        {
            bool exists = std::filesystem::exists(archiveDir / name);
            std::string reason = exists ? ": listed in kKnownRoundTripFailures but not accepted by CheckType()"
                                        : ": listed in kKnownRoundTripFailures but missing from the archive";
            CHECK_MESSAGE(false, name << reason);
        }
    }

    std::ostringstream extensions;
    for (const auto& ext : otherExtensions)
    {
        extensions << " " << ext.first << "=" << ext.second;
    }
    std::ostringstream categories;
    for (const auto& category : filesByCategory)
    {
        categories << "\n" << category.first << ": " << category.second.size() << " files";
        for (const std::string& name : category.second)
        {
            categories << "\n  " << name;
        }
    }
    std::ostringstream bytes;
    for (const std::string& name : byteDifferences)
    {
        bytes << "\n  " << name;
    }
    MESSAGE("round trip: " << files.size() << " files (" << wsCount << " .WS, " << otherCount
            << " other extensions:" << extensions.str() << "), " << passed << " passed, " << failed
            << " failed (" << knownFailed << " listed as known)" << categories.str()
            << "\nsaved bytes differ from the original: " << byteDifferences.size() << " files" << bytes.str());
}


TEST_CASE("cWordstarFile - a binding space (0x0F) loads as a non-breaking space and saves back")
{
    ensureQApplication();
    cEditorCtrl editor;
    editor.GetDocument()->Clear();

    SUBCASE("text without a binding space saves back unchanged")
    {
        std::string input = WordStar7File("a b\x0D\x0A");
        CHECK(RoundTrip(editor, input) == input);
    }

    SUBCASE("a binding space")
    {
        std::string input = WordStar7File(std::string("a\x0F" "b\x0D\x0A", 5));
        std::string saved = RoundTrip(editor, input);

        CHECK(editor.GetDocument()->GetParagraphText(0).rfind("a\xC2\xA0" "b", 0) == 0);
        CHECK(saved == input);
    }
}


TEST_CASE("cWordstarFile - binding spaces between words survive load and two saves")
{
    ensureQApplication();
    cEditorCtrl editor;
    editor.GetDocument()->Clear();

    std::string input = WordStar7File("00h\x0F" "through\x0F" "1Fh.\x0D\x0A");
    std::string saved = RoundTrip(editor, input);

    std::string text = editor.GetDocument()->GetParagraphText(0);
    CHECK(text.rfind("00h\xC2\xA0" "through\xC2\xA0" "1Fh.", 0) == 0);
    CHECK(text.find("00hthrough") == std::string::npos);
    CHECK(text.find('\x0F') == std::string::npos);
    CHECK(saved == input);

    editor.GetDocument()->Clear();
    std::string resaved = RoundTrip(editor, saved);
    CHECK(editor.GetDocument()->GetParagraphText(0).rfind("00h\xC2\xA0" "through\xC2\xA0" "1Fh.", 0) == 0);
    CHECK(resaved == input);
}


namespace
{

// sWSTab body: tabsize 720, abstabsize 720, type, size 5
std::string TabBody(unsigned char type)
{
    std::string body("\xD0\x02\xD0\x02", 4);
    body += static_cast<char>(type);
    body += '\x05';
    return body;
}

std::string TabSequence(unsigned char type)
{
    return SequenceBytes(SEQ_TAB, TabBody(type));
}

const sParagraphLayout* LayoutParagraph(cEditorCtrl &editor, PARAGRAPH_T para)
{
    editor.GetLayout()->LayoutDocument(editor.GetDocument());
    return editor.GetLayout()->GetParagraphLayout(para);
}

}


TEST_CASE("cWordstarFile - a soft tab at the start of a paragraph keeps a following period as text")
{
    ensureQApplication();
    cEditorCtrl editor;
    editor.GetDocument()->Clear();
    cDocument *doc = editor.GetDocument();

    std::string input = WordStar7File(TabSequence(TAB_SOFT) + ".xx text\x0D\x0A");

    SUBCASE("loads as a tab marker followed by text")
    {
        RoundTrip(editor, input);

        std::string text = doc->GetParagraphText(0);
        REQUIRE(text.empty() == false);
        CHECK(text[0] == MARKER_CHAR);
        CHECK(text.find(".xx text") == 1);
        CHECK(doc->GetTab(0).type == TAB_SOFT);
        CHECK(doc->GetTab(0).tabsize == 720);
        CHECK(doc->GetTab(0).abstabsize == 720);
        CHECK(doc->GetTab(0).size == 5);
        CHECK(doc->GetWordCount() > 0);

        const sParagraphLayout *layout = LayoutParagraph(editor, 0);
        REQUIRE(layout != nullptr);
        CHECK(layout->isCommand == false);
    }

    SUBCASE("saves back byte-identical")
    {
        CHECK(RoundTrip(editor, input) == input);
    }

    SUBCASE("is still text after a reload")
    {
        std::string saved = RoundTrip(editor, input);
        REQUIRE(saved.empty() == false);
        editor.GetDocument()->Clear();
        CHECK(RoundTrip(editor, saved) == input);

        std::string text = doc->GetParagraphText(0);
        REQUIRE(text.empty() == false);
        CHECK(text[0] == MARKER_CHAR);
        CHECK(doc->GetTab(0).type == TAB_SOFT);
        CHECK(doc->GetWordCount() > 0);
    }
}


TEST_CASE("cWordstarFile - a period at the start of a paragraph without a soft tab")
{
    ensureQApplication();
    cEditorCtrl editor;
    editor.GetDocument()->Clear();
    cDocument *doc = editor.GetDocument();

    SUBCASE("with no tab is a dot command")
    {
        std::string input = WordStar7File(".xx text\x0D\x0A");
        CHECK(RoundTrip(editor, input) == input);

        CHECK(doc->GetParagraphText(0).rfind(".xx text", 0) == 0);
        CHECK(doc->GetWordCount() == 0);

        const sParagraphLayout *layout = LayoutParagraph(editor, 0);
        REQUIRE(layout != nullptr);
        CHECK(layout->isCommand == true);
    }

    SUBCASE("after a normal tab is text")
    {
        std::string input = WordStar7File(TabSequence(TAB_TAB) + ".xx text\x0D\x0A");
        CHECK(RoundTrip(editor, input) == input);

        std::string text = doc->GetParagraphText(0);
        REQUIRE(text.empty() == false);
        CHECK(text[0] == MARKER_CHAR);
        CHECK(doc->GetTab(0).type == TAB_TAB);
        CHECK(doc->GetWordCount() > 0);

        const sParagraphLayout *layout = LayoutParagraph(editor, 0);
        REQUIRE(layout != nullptr);
        CHECK(layout->isCommand == false);
    }

    SUBCASE("a soft tab inside a paragraph is dropped")
    {
        std::string input = WordStar7File("ab" + TabSequence(TAB_SOFT) + "cd\x0D\x0A");
        CHECK(RoundTrip(editor, input) == WordStar7File("abcd\x0D\x0A"));

        std::string text = doc->GetParagraphText(0);
        CHECK(text.rfind("abcd", 0) == 0);
        CHECK(text.find(static_cast<char>(MARKER_CHAR)) == std::string::npos);
        for (POSITION_T pos = 0; pos < 4; ++pos)
        {
            CHECK(doc->GetTab(pos).type == TAB_BAD);
        }
    }
}


TEST_CASE("cWordstarFile - a soft tab at the start of a paragraph after a hard return")
{
    ensureQApplication();
    cEditorCtrl editor;
    editor.GetDocument()->Clear();
    cDocument *doc = editor.GetDocument();

    std::string input = WordStar7File("line one\x0D\x0A" + TabSequence(TAB_SOFT) + ".xx text\x0D\x0A");
    CHECK(RoundTrip(editor, input) == input);

    REQUIRE(doc->GetNumberofParagraphs() >= 2);
    CHECK(doc->GetParagraphText(0).rfind("line one", 0) == 0);

    std::string text = doc->GetParagraphText(1);
    REQUIRE(text.empty() == false);
    CHECK(text[0] == MARKER_CHAR);
    CHECK(text.find(".xx text") == 1);

    POSITION_T start;
    POSITION_T end;
    doc->GetParagraphStartandEnd(1, start, end);
    CHECK(doc->GetTab(start).type == TAB_SOFT);
    CHECK(doc->GetWordCount() == 4);

    const sParagraphLayout *layout = LayoutParagraph(editor, 1);
    REQUIRE(layout != nullptr);
    CHECK(layout->isCommand == false);
}


TEST_CASE("cWordstarFile - two soft tabs at the start of a paragraph keep only the first")
{
    ensureQApplication();
    cEditorCtrl editor;
    editor.GetDocument()->Clear();
    cDocument *doc = editor.GetDocument();

    std::string input = WordStar7File(TabSequence(TAB_SOFT) + TabSequence(TAB_SOFT) + ".xx text\x0D\x0A");

    // lossy: the second soft tab is not at the paragraph start and is not saved
    CHECK(RoundTrip(editor, input) == WordStar7File(TabSequence(TAB_SOFT) + ".xx text\x0D\x0A"));

    std::string text = doc->GetParagraphText(0);
    REQUIRE(text.empty() == false);
    CHECK(text[0] == MARKER_CHAR);
    CHECK(text.find(".xx text") == 1);
    CHECK(doc->GetTab(0).type == TAB_SOFT);
    CHECK(doc->GetTab(1).type == TAB_BAD);
    CHECK(doc->GetWordCount() > 0);

    const sParagraphLayout *layout = LayoutParagraph(editor, 0);
    REQUIRE(layout != nullptr);
    CHECK(layout->isCommand == false);
}


TEST_CASE("cWordstarFile - a soft tab after a style marker at the start of a paragraph is dropped")
{
    ensureQApplication();
    cEditorCtrl editor;
    editor.GetDocument()->Clear();
    cDocument *doc = editor.GetDocument();

    std::string input = WordStar7File("\x02" + TabSequence(TAB_SOFT) + ".xx text\x02\x0D\x0A");

    // the bold marker is at the paragraph start, so the soft tab is not
    CHECK(RoundTrip(editor, input) == WordStar7File("\x02.xx text\x02\x0D\x0A"));

    std::string text = doc->GetParagraphText(0);
    REQUIRE(text.empty() == false);
    CHECK(text[0] == MARKER_CHAR);
    CHECK(text.find(".xx text") == 1);
    CHECK(doc->GetControlChar(0) == STYLE_BOLD);
    CHECK(doc->GetTab(0).type == TAB_BAD);
    CHECK(doc->GetTab(1).type == TAB_BAD);
    CHECK(doc->GetWordCount() > 0);

    const sParagraphLayout *layout = LayoutParagraph(editor, 0);
    REQUIRE(layout != nullptr);
    CHECK(layout->isCommand == false);
}


namespace
{

// Saves the editor's document through cWordstarFile, clears it, loads the
// saved file back and returns the saved bytes
std::string SaveAndReload(cEditorCtrl &editor)
{
    cDocument *doc = editor.GetDocument();
    cWordstarFile saver(&editor);
    std::filesystem::path out = TempPath();
    std::string saved;
    if (saver.SaveFile(out.string(), doc->GetTextSize()))
    {
        saved = ReadFileBytes(out);
        doc->Clear();
        cWordstarFile loader;
        loader.mDocument = doc;
        if (loader.LoadFile(out.string()) == false)
        {
            saved.clear();
        }
    }
    std::filesystem::remove(out);
    return saved;
}

}


TEST_CASE("cWordstarFile - a soft tab moved inside a paragraph is kept after a save")
{
    ensureQApplication();
    cEditorCtrl editor;
    editor.GetDocument()->Clear();
    cDocument *doc = editor.GetDocument();

    SUBCASE("joining two paragraphs")
    {
        std::string input = WordStar7File("abc\x0D\x0A" + TabSequence(TAB_SOFT) + ".xx text\x0D\x0A");
        RoundTrip(editor, input);

        doc->SetPosition(3);
        doc->Delete(3, 1);

        std::string text = doc->GetParagraphText(0);
        REQUIRE(text.size() > 4);
        REQUIRE(text.rfind("abc", 0) == 0);
        REQUIRE(static_cast<unsigned char>(text[3]) == MARKER_CHAR);
        REQUIRE(doc->GetTab(3).type == TAB_SOFT);

        REQUIRE(SaveAndReload(editor).empty() == false);

        text = doc->GetParagraphText(0);
        REQUIRE(text.size() > 3);
        CHECK(text.rfind("abc", 0) == 0);
        CHECK(static_cast<unsigned char>(text[3]) == MARKER_CHAR);
        CHECK(doc->GetControlChar(3) == STYLE_TAB);
        CHECK(doc->GetTab(3).type == TAB_TAB);
        CHECK(text.find(".xx text") == 4);
    }

    SUBCASE("typing in front of a leading soft tab")
    {
        std::string input = WordStar7File(TabSequence(TAB_SOFT) + ".xx text\x0D\x0A");
        RoundTrip(editor, input);

        doc->SetPosition(0);
        doc->Insert(static_cast<CHAR_T>('Z'));
        REQUIRE(doc->GetTab(1).type == TAB_SOFT);

        REQUIRE(SaveAndReload(editor).empty() == false);

        std::string text = doc->GetParagraphText(0);
        REQUIRE(text.size() > 2);
        CHECK(text[0] == 'Z');
        CHECK(static_cast<unsigned char>(text[1]) == MARKER_CHAR);
        CHECK(doc->GetControlChar(1) == STYLE_TAB);
        CHECK(doc->GetTab(1).type == TAB_TAB);
        CHECK(text.find(".xx text") == 2);
    }

    SUBCASE("pasting a soft tab into the middle of a paragraph")
    {
        std::string input = WordStar7File(TabSequence(TAB_SOFT) + ".xx text\x0D\x0A" "hello world\x0D\x0A");
        RoundTrip(editor, input);

        doc->SetPosition(0);
        doc->SetBeginBlock();
        doc->SetPosition(1);
        doc->SetEndBlock();
        doc->Copy();

        POSITION_T start;
        POSITION_T end;
        doc->GetParagraphStartandEnd(1, start, end);
        doc->SetPosition(start + 5);
        doc->Paste();

        std::string text = doc->GetParagraphText(1);
        REQUIRE(text.size() > 6);
        REQUIRE(static_cast<unsigned char>(text[5]) == MARKER_CHAR);
        REQUIRE(doc->GetTab(start + 5).type == TAB_SOFT);

        REQUIRE(SaveAndReload(editor).empty() == false);

        REQUIRE(doc->GetNumberofParagraphs() >= 2);
        doc->GetParagraphStartandEnd(1, start, end);
        text = doc->GetParagraphText(1);
        REQUIRE(text.size() > 6);
        CHECK(text.rfind("hello", 0) == 0);
        CHECK(static_cast<unsigned char>(text[5]) == MARKER_CHAR);
        CHECK(doc->GetControlChar(start + 5) == STYLE_TAB);
        CHECK(doc->GetTab(start + 5).type == TAB_TAB);
        CHECK(text.find(" world") == 6);

        // the copied soft tab is still at the start of paragraph 0
        CHECK(doc->GetTab(0).type == TAB_SOFT);
    }

    SUBCASE("a leading soft tab still saves byte-identical")
    {
        std::string input = WordStar7File(TabSequence(TAB_SOFT) + ".xx text\x0D\x0A");
        CHECK(RoundTrip(editor, input) == input);
    }
}


namespace
{

// ESC x FS: an extended character in a WordStar file
std::string ExtChar(unsigned char x)
{
    std::string bytes("\x1B", 1);
    bytes += static_cast<char>(x);
    bytes += '\x1C';
    return bytes;
}

// Loads bytes through cEditorCtrl::LoadFile, saves the document back
// through cWordstarFile and returns the saved bytes
std::string EditorRoundTrip(cEditorCtrl &editor, const std::string &bytes)
{
    cWordstarFile saver(&editor);
    saver.SetSilent(true);
    std::filesystem::path in = TempPath();
    std::filesystem::path out = TempPath();
    WriteFileBytes(in, bytes);
    std::string saved;
    if (editor.LoadFile(in.string()) &&
        saver.SaveFile(out.string(), editor.GetDocument()->GetTextSize()))
    {
        saved = ReadFileBytes(out);
    }
    std::filesystem::remove(in);
    std::filesystem::remove(out);
    return saved;
}

// Hex dump of up to count bytes from offset, for readable failure messages
std::string Hex(const std::string &bytes, size_t offset = 0, size_t count = 32)
{
    static const char digits[] = "0123456789ABCDEF";
    std::string hex;
    for (size_t loop = offset; loop < bytes.size() && loop < offset + count; loop++)
    {
        unsigned char ch = static_cast<unsigned char>(bytes[loop]);
        hex += digits[ch >> 4];
        hex += digits[ch & 0x0F];
        hex += ' ';
    }
    return hex;
}

bool HasControlByte(const std::string &text)
{
    for (unsigned char ch : text)
    {
        if (ch < 0x20 && ch != 0x0D && ch != 0x0A)
        {
            return true;
        }
    }
    return text.find(static_cast<char>(MARKER_CHAR)) != std::string::npos;
}

struct sControlGlyph
{
    unsigned char code;
    const char *utf8;
};

// CP437 glyphs for code points below 0x20, as in the cCodePage437 table
const sControlGlyph kControlGlyphs[] =
{
    { 0x01, "\xE2\x98\xBA" },       // U+263A smiley
    { 0x07, "\xE2\x80\xA2" },       // U+2022 bullet
    { 0x09, "\xE2\x97\x8B" },       // U+25CB white circle
    { 0x0D, "\xE2\x99\xAA" },       // U+266A eighth note
    { 0x10, "\xE2\x96\xBA" },       // U+25BA right pointing triangle
    { 0x1A, "\xE2\x86\x92" },       // U+2192 right arrow
    { 0x1B, "\xE2\x86\x90" },       // U+2190 left arrow
    { 0x1F, "\xE2\x96\xBC" },       // U+25BC down pointing triangle
};

}


TEST_CASE("cWordstarFile - ESC x FS with x below 0x20 loads as the CP437 glyph")
{
    ensureQApplication();
    cEditorCtrl editor;
    cDocument *doc = editor.GetDocument();

    std::string plain = WordStar7File("aXb\x0D\x0A" "next line\x0D\x0A");
    EditorRoundTrip(editor, plain);
    PARAGRAPH_T plainCount = doc->GetNumberofParagraphs();

    for (const sControlGlyph &glyph : kControlGlyphs)
    {
        CAPTURE(static_cast<int>(glyph.code));
        std::string input = WordStar7File("a" + ExtChar(glyph.code) + "b\x0D\x0A" "next line\x0D\x0A");
        std::string expected = std::string("a") + glyph.utf8 + "b";

        std::string saved = EditorRoundTrip(editor, input);

        std::string text = doc->GetParagraphText(0);
        CHECK(Hex(text, 0, expected.size()) == Hex(expected));
        CHECK(HasControlByte(text) == false);
        CHECK(doc->GetNumberofParagraphs() == plainCount);
        REQUIRE(doc->GetNumberofParagraphs() >= 2);
        CHECK(doc->GetParagraphText(1).rfind("next line", 0) == 0);

        CHECK(Hex(saved, 128) == Hex(input, 128));
        CHECK(saved == input);

        std::string resaved = EditorRoundTrip(editor, saved);
        CHECK(Hex(doc->GetParagraphText(0), 0, expected.size()) == Hex(expected));
        CHECK(doc->GetNumberofParagraphs() == plainCount);
        CHECK(Hex(resaved, 128) == Hex(saved, 128));
        CHECK(resaved == saved);
    }
}


TEST_CASE("cWordstarFile - ESC x FS with x below 0x20 inside a dot command line")
{
    ensureQApplication();
    cEditorCtrl editor;
    cDocument *doc = editor.GetDocument();

    std::string input = WordStar7File(".he " + ExtChar(0x10) + " text\x0D\x0A" "body\x0D\x0A");
    std::string saved = EditorRoundTrip(editor, input);

    std::string text = doc->GetParagraphText(0);
    std::string expected(".he \xE2\x96\xBA text");
    CHECK(Hex(text, 0, expected.size()) == Hex(expected));
    CHECK(HasControlByte(text) == false);
    REQUIRE(doc->GetNumberofParagraphs() >= 2);
    CHECK(doc->GetParagraphText(1).rfind("body", 0) == 0);

    const sParagraphLayout *layout = LayoutParagraph(editor, 0);
    REQUIRE(layout != nullptr);
    CHECK(layout->isCommand == true);

    CHECK(Hex(saved, 128) == Hex(input, 128));
    CHECK(saved == input);
}


TEST_CASE("cWordstarFile - ESC x FS with x at 0x80 or above loads as the CP437 character")
{
    ensureQApplication();
    cEditorCtrl editor;
    cDocument *doc = editor.GetDocument();

    std::string input = WordStar7File("a" + ExtChar(0x81) + "b" + ExtChar(0xDB) + "c\x0D\x0A" "next line\x0D\x0A");
    std::string saved = EditorRoundTrip(editor, input);

    CHECK(doc->GetParagraphText(0).rfind("a\xC3\xBC" "b\xE2\x96\x88" "c", 0) == 0);    // U+00FC, U+2588
    REQUIRE(doc->GetNumberofParagraphs() >= 2);
    CHECK(doc->GetParagraphText(1).rfind("next line", 0) == 0);
    CHECK(saved == input);
}


namespace
{

std::string Utf8(unsigned long cp)
{
    std::string out;
    if (cp < 0x80)
    {
        out += static_cast<char>(cp);
    }
    else if (cp < 0x800)
    {
        out += static_cast<char>(0xC0 | (cp >> 6));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    }
    else
    {
        out += static_cast<char>(0xE0 | (cp >> 12));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    }
    return out;
}

// Bytes 0x01-0x1F and 0x7F, without 0x1C (see the FS test below)
std::vector<unsigned char> ControlBytes(void)
{
    std::vector<unsigned char> bytes;
    for (int ch = 0x01; ch < 0x20; ch++)
    {
        if (ch != 0x1C)
        {
            bytes.push_back(static_cast<unsigned char>(ch));
        }
    }
    bytes.push_back(0x7F);
    return bytes;
}

}


TEST_CASE("cWordstarFile - ESC x FS with x below 0x20 loads as the CP437 glyph under CP737 and CP850")
{
    ensureQApplication();
    cEditorCtrl editor;
    cDocument *doc = editor.GetDocument();
    cCodePage437 cp437;

    const eCodePage pages[] = { CP737, CP850 };
    for (eCodePage page : pages)
    {
        CAPTURE(static_cast<int>(page));
        editor.SetCodePage(page);

        std::string plain = WordStar7File("aXb\x0D\x0A" "next line\x0D\x0A");
        EditorRoundTrip(editor, plain);
        PARAGRAPH_T plainCount = doc->GetNumberofParagraphs();

        for (unsigned char code : ControlBytes())
        {
            CAPTURE(static_cast<int>(code));
            std::string input = WordStar7File("a" + ExtChar(code) + "b\x0D\x0A" "next line\x0D\x0A");
            std::string expected = "a" + Utf8(cp437.controlGlyph(code)) + "b";

            std::string saved = EditorRoundTrip(editor, input);

            std::string text = doc->GetParagraphText(0);
            CHECK(Hex(text, 0, expected.size()) == Hex(expected));
            CHECK(HasControlByte(text) == false);
            CHECK(doc->GetNumberofParagraphs() == plainCount);
            REQUIRE(doc->GetNumberofParagraphs() >= 2);
            CHECK(doc->GetParagraphText(1).rfind("next line", 0) == 0);

            // CP850 saves the pilcrow and the section sign as its own 0xF4 and 0xF5
            std::string expectedSaved = input;
            if (page == CP850 && (code == 0x14 || code == 0x15))
            {
                expectedSaved = WordStar7File("a" + ExtChar(code == 0x14 ? 0xF4 : 0xF5) + "b\x0D\x0A" "next line\x0D\x0A");
            }
            CHECK(Hex(saved, 128) == Hex(expectedSaved, 128));
            CHECK(saved == expectedSaved);

            std::string resaved = EditorRoundTrip(editor, saved);
            CHECK(Hex(doc->GetParagraphText(0), 0, expected.size()) == Hex(expected));
            CHECK(doc->GetNumberofParagraphs() == plainCount);
            CHECK(Hex(resaved, 128) == Hex(saved, 128));
        }
    }
}


TEST_CASE("cWordstarFile - ESC x FS with x at 0x80 or above follows the editor code page")
{
    ensureQApplication();
    cEditorCtrl editor;
    cDocument *doc = editor.GetDocument();

    std::string input = WordStar7File("a" + ExtChar(0x80) + "b" + ExtChar(0x9B) + "c\x0D\x0A");

    editor.SetCodePage(CP737);
    CHECK(EditorRoundTrip(editor, input) == input);
    CHECK(doc->GetParagraphText(0).rfind("a\xCE\x91" "b", 0) == 0);           // U+0391

    editor.SetCodePage(CP850);
    CHECK(EditorRoundTrip(editor, input) == input);
    CHECK(doc->GetParagraphText(0).rfind("a\xC3\x87" "b\xC3\xB8" "c", 0) == 0);    // U+00C7, U+00F8
}


TEST_CASE("cWordstarFile - German letters as ESC x FS load and save under CP437 and CP850")
{
    ensureQApplication();
    cEditorCtrl editor;
    cDocument *doc = editor.GetDocument();

    // a 0x84, o 0x94, u 0x81, A 0x8E, O 0x99, U 0x9A, sz 0xE1 in both code pages
    const std::string ae = ExtChar(0x84);
    const std::string oe = ExtChar(0x94);
    const std::string ue = ExtChar(0x81);
    const std::string AE = ExtChar(0x8E);
    const std::string OE = ExtChar(0x99);
    const std::string UE = ExtChar(0x9A);
    const std::string sz = ExtChar(0xE1);

    std::string input = WordStar7File(
        "Gr" + ue + sz + "e aus M" + ue + "nchen, sch" + oe + "ne H" + ae + "user" + "\x0D\x0A"
        + AE + "rger " + OE + "l " + UE + "bel Stra" + sz + "\x0D\x0A"
        + "Fl" + ue + "sse und B" + ae + "r " + "M" + oe + "\x0D\x0A");

    std::string expected =
        "Gr\xC3\xBC\xC3\x9F" "e aus M\xC3\xBC" "nchen, sch\xC3\xB6" "ne H\xC3\xA4" "user\x0D|"
        "\xC3\x84" "rger \xC3\x96" "l \xC3\x9C" "bel Stra\xC3\x9F\x0D|"
        "Fl\xC3\xBC" "sse und B\xC3\xA4" "r M\xC3\xB6\x0D|";

    const eCodePage pages[] = { CP437, CP850 };
    for (eCodePage page : pages)
    {
        CAPTURE(static_cast<int>(page));
        editor.SetCodePage(page);
        std::string saved = EditorRoundTrip(editor, input);

        std::string loaded;
        for (PARAGRAPH_T para = 0; para < doc->GetNumberofParagraphs(); para++)
        {
            loaded += doc->GetParagraphText(para) + "|";
        }
        CHECK(Hex(loaded, 0, expected.size()) == Hex(expected, 0, expected.size()));
        CHECK(saved.size() == input.size());
        CHECK(Hex(saved, 128, input.size()) == Hex(input, 128, input.size()));
    }
}


TEST_CASE("cWordstarFile - ESC FS FS loads as one right angle glyph")
{
    ensureQApplication();
    cEditorCtrl editor;
    cDocument *doc = editor.GetDocument();

    std::string input = WordStar7File("a" + ExtChar(0x1C) + "b\x0D\x0A" "next line\x0D\x0A");
    std::string expected("a\xE2\x88\x9F" "b");     // U+221F

    const eCodePage pages[] = { CP437, CP737, CP850 };
    for (eCodePage page : pages)
    {
        CAPTURE(static_cast<int>(page));
        editor.SetCodePage(page);
        std::string saved = EditorRoundTrip(editor, input);

        CHECK(Hex(doc->GetParagraphText(0), 0, expected.size() + 1) == Hex(expected + "\x0D"));
        CHECK(Hex(saved, 128) == Hex(input, 128));
    }
}


TEST_CASE("cWordstarFile - ESC x FS with x below 0x20 under CP1252 keeps the raw byte")
{
    ensureQApplication();
    cEditorCtrl editor;
    cDocument *doc = editor.GetDocument();
    editor.SetCodePage(CP1252);

    for (unsigned char code : ControlBytes())
    {
        CAPTURE(static_cast<int>(code));
        EditorRoundTrip(editor, WordStar7File("a" + ExtChar(code) + "b\x0D\x0A"));

        // 0x01 is dropped, 0x0D ends the paragraph, every other byte is a marker
        std::string expected("a\x7F" "b\x0D");
        if (code == 0x01)
        {
            expected = "ab\x0D";
        }
        else if (code == 0x0D)
        {
            expected = "a\x0D";
        }
        CHECK(Hex(doc->GetParagraphText(0), 0, 8) == Hex(expected));
    }
}


namespace
{

// Writes bytes to a file with the given name in a new temp directory and
// returns cWordstarFile::CheckType() for it
bool CheckTypeOf(const std::string &bytes, const std::string &name = "sample.xyz")
{
    std::filesystem::path dir = UniqueTempDir("wordtsar_checktype");
    std::filesystem::path path = dir / name;
    WriteFileBytes(path, bytes);
    cWordstarFile checker;
    bool result = checker.CheckType(path.string());
    std::filesystem::remove_all(dir);
    return result;
}

// Pads bytes with 0x1A up to the next multiple of 128
std::string PadEof(std::string bytes)
{
    bytes.append(128 - (bytes.size() % 128), '\x1A');
    return bytes;
}

}


TEST_CASE("cWordstarFile::CheckType - content without WordStar structure is rejected")
{
    SUBCASE("UTF-8 text with umlauts")
    {
        CHECK(CheckTypeOf("Viele Gr\xC3\xBC\xC3\x9F" "e aus M\xC3\xBC" "nchen und sch\xC3\xB6" "ne Gr\xC3\xBC\xC3\x9F" "e an alle Freunde.\r\n") == false);
    }

    SUBCASE("UTF-8 text with a c caron at a line end")
    {
        CHECK(CheckTypeOf("Dobry den, to je muj klic\xC4\x8D\nA dalsi radek.\n") == false);
    }

    SUBCASE("plain ASCII")
    {
        CHECK(CheckTypeOf("Just some plain text.\r\nAnother line.\r\n") == false);
    }

    SUBCASE("plain ASCII with 0x1A padding")
    {
        CHECK(CheckTypeOf(PadEof("Just some plain text.\r\nAnother line.\r\n")) == false);
    }

    SUBCASE("a dot command line only")
    {
        CHECK(CheckTypeOf(".pa\r\nHello world.\r\n") == false);
    }

    SUBCASE("bold toggles only")
    {
        CHECK(CheckTypeOf("Some \x02" "bold\x02 text and more \x02" "bold\x02.\r\n") == false);
    }

    SUBCASE("a compiled macro")
    {
        std::string text;
        while (text.size() < 390)
        {
            text += "macro text line\r\n";
        }
        text = text.substr(0, 390) + "\x1B\x81\x1C" "1234567";
        std::string bytes = PadEof(text) + std::string("\x14\x00\x11", 3);
        REQUIRE(bytes.size() == 515);
        CHECK(CheckTypeOf(bytes) == false);
    }

    SUBCASE("a NUL and a soft return")
    {
        CHECK(CheckTypeOf(std::string("abc\x00" "def\x8D\x0A" "ghi\r\n", 13)) == false);
    }

    SUBCASE("a 0x1D that starts no sequence and a soft return")
    {
        CHECK(CheckTypeOf("abc\x1D\x05\x07" "xyz def\x8D\x0A" "ghi\r\n") == false);
    }

    SUBCASE("CP437 box drawing and a soft return")
    {
        CHECK(CheckTypeOf("A table of contents with a framed title.\r\n"
                          "\xC9\xCD\xCD\xCD\xCD\xBB\r\n"
                          "Some more text that wraps at the end of this\x8D\x0A"
                          "line and continues here.\r\n") == false);
    }

    SUBCASE("Latin-1 text")
    {
        CHECK(CheckTypeOf("Viele Gr\xFC\xDF" "e aus M\xFC" "nchen.\r\n") == false);
    }

    SUBCASE("UTF-16LE with a byte order mark")
    {
        std::string bytes("\xFF\xFE", 2);
        for (char ch : std::string("Hello world\r\n"))
        {
            bytes += ch;
            bytes += '\0';
        }
        CHECK(CheckTypeOf(bytes) == false);
    }

    SUBCASE("a high bit on the last letter of words only")
    {
        // 0xF3 = 's' | 0x80, 0xE5 = 'e' | 0x80, 0xF4 = 't' | 0x80
        CHECK(CheckTypeOf("Thi\xF3 i\xF3 som\xE5 tex\xF4 tha\xF4 look\xF3 lik\xE5 WordStar.\r\n") == false);
    }
}


TEST_CASE("cWordstarFile::CheckType - content with WordStar structure is accepted")
{
    SUBCASE("a soft return")
    {
        CHECK(CheckTypeOf("This is a line\x8D\x0Athat wraps.\r\n"));
    }

    SUBCASE("a sequence in the middle of the file")
    {
        CHECK(CheckTypeOf("Text before.\r\n" + SequenceBytes(SEQ_COMMENT, "a note") + "Text after.\r\n"));
    }

    SUBCASE("a sequence whose body contains 0x1A")
    {
        CHECK(CheckTypeOf("Text before.\r\n" + SequenceBytes(SEQ_COMMENT, "a\x1A" "b") + "Text after.\r\n"));
    }

    SUBCASE("ESC 9C FS")
    {
        CHECK(CheckTypeOf("Price: 10\x1B\x9C\x1C\r\n"));
    }

    SUBCASE("a control byte and a dot command line")
    {
        CHECK(CheckTypeOf(".he Header\r\nSome \x02" "bold\x02 text.\r\n"));
    }

    SUBCASE("a soft return after byte 512")
    {
        std::string text;
        while (text.size() < 600)
        {
            text += "plain text line\r\n";
        }
        CHECK(CheckTypeOf(text + "wrapped\x8D\x0A" "line\r\n"));
    }

    SUBCASE("the WordStar 5+ header")
    {
        CHECK(CheckTypeOf(WordStar7File("Hello\r\n")));
    }

    SUBCASE("plain ASCII with a .ws extension")
    {
        CHECK(CheckTypeOf("Just some plain text.\r\n", "sample.ws"));
    }

    SUBCASE("WordStar 4 text with soft returns and high-bit word ends")
    {
        // high bit on the last letter before a space or soft return
        CHECK(CheckTypeOf("Sehr geehrt\xE5 Dame\xEE und Herren, wi\xF2 freue\xEE uns\x8D\x0A"
                          "Ihne\xEE mitteile\xEE zu konnen, dass die Strass\xE5\x8D\x0A"
                          "gesperrt ist.\r\n"));
    }
}


TEST_CASE("cWordstarFile::CheckType - a high-bit letter before a tab, ESC, 0x1A or style toggle ends a word")
{
    // 0xE5 = 'e' | 0x80, 0xE4 = 'd' | 0x80
    SUBCASE("a tab")
    {
        CHECK(CheckTypeOf("Nam\xE5\tvalue that wraps\x8D\x0A" "here.\r\n"));
    }

    SUBCASE("ESC x FS")
    {
        CHECK(CheckTypeOf("Pric\xE5\x1B\x9C\x1C" "10 that wraps\x8D\x0A" "here.\r\n"));
    }

    SUBCASE("0x1A")
    {
        CHECK(CheckTypeOf(PadEof("A line that wraps\x8D\x0A" "and ends her\xE5")));
    }

    SUBCASE("a style toggle")
    {
        // bold, underline, superscript, subscript, strikethrough, italics
        const char toggles[] = { 0x02, 0x13, 0x14, 0x16, 0x18, 0x19 };
        for (char toggle : toggles)
        {
            CAPTURE(static_cast<int>(toggle));
            std::string bytes = std::string("Some ") + toggle + "wor\xE4" + toggle + " that wraps\x8D\x0A" "here.\r\n";
            CHECK(CheckTypeOf(bytes));
        }
    }
}


TEST_CASE("cWordstarFile::CheckType - a 0x1D that starts no complete sequence is rejected")
{
    const std::string text = "This line wraps\x8D\x0A" "here.\r\n";
    const std::string seq = SequenceBytes(SEQ_COMMENT, "a note");

    REQUIRE(CheckTypeOf(text + seq + text));

    SUBCASE("a sequence cut by the end of the file")
    {
        CHECK(CheckTypeOf(text + seq.substr(0, seq.size() - 3)) == false);
    }

    SUBCASE("a sequence of length 3")
    {
        CHECK(CheckTypeOf(text + std::string("\x1D\x03\x00\x03\x00\x1D", 6) + text) == false);
    }

    SUBCASE("a sequence whose length is larger than the file")
    {
        std::string longSeq = seq;
        longSeq[1] = '\x00';
        longSeq[2] = '\x10';
        CHECK(CheckTypeOf(text + longSeq + text) == false);
    }
}


TEST_CASE("cWordstarFile::CheckType - a WordStar 5+ header is accepted only if it fits in the file")
{
    // 1D, size (2), type 00, then size - 1 more bytes: size + 3 bytes in all
    auto header = [](uint16_t size, size_t fileSize)
    {
        std::string bytes("\x1D", 1);
        bytes += static_cast<char>(size & 0xFF);
        bytes += static_cast<char>(size >> 8);
        bytes += '\0';
        bytes.resize(fileSize, 'x');
        return bytes;
    };

    SUBCASE("a header that ends at the last byte of the file")
    {
        CHECK(CheckTypeOf(header(10, 13)));
    }

    SUBCASE("a header one byte larger than the file")
    {
        CHECK(CheckTypeOf(header(10, 12)) == false);
    }

    SUBCASE("a header far larger than the file")
    {
        CHECK(CheckTypeOf(header(0xFFFF, 20)) == false);
    }

    SUBCASE("a WordStar 7 file")
    {
        CHECK(CheckTypeOf(WordStar7File("Hello\r\n")));
    }
}


TEST_CASE("cWordstarFile::CheckType - a file of exactly 256 KiB is scanned to its last byte")
{
    const size_t fileSize = 256 * 1024;
    std::string text;
    while (text.size() < fileSize)
    {
        text += "plain text line\r\n";
    }
    text.resize(fileSize - 2);

    SUBCASE("a soft return in the last two bytes")
    {
        std::string bytes = text + "\x8D\x0A";
        REQUIRE(bytes.size() == fileSize);
        CHECK(CheckTypeOf(bytes));
    }

    SUBCASE("a NUL in the last byte")
    {
        std::string bytes = "wraps\x8D\x0A" + text.substr(7) + std::string("\r\x00", 2);
        REQUIRE(bytes.size() == fileSize);
        CHECK(CheckTypeOf(bytes) == false);
    }
}


TEST_CASE("cWordstarFile::CheckType - UTF-8 with a 4-byte character is rejected, a surrogate is not UTF-8")
{
    // a control byte and a dot command line, accepted without UTF-8
    const std::string head = ".he Header\r\nSome \x02" "bold\x02 text ";

    SUBCASE("a 4-byte character")
    {
        // U+20000
        CHECK(CheckTypeOf(head + "\xF0\xA0\x80\x80" " here.\r\n") == false);
    }

    SUBCASE("a 4-byte character cut short")
    {
        CHECK(CheckTypeOf(head + "\xF0\xA0\x80" " here.\r\n"));
    }

    SUBCASE("a surrogate")
    {
        // U+D800 encoded as three bytes
        CHECK(CheckTypeOf(head + "\xED\xA0\x80" " here.\r\n"));
    }
}


namespace
{

// Paragraph texts of the document, each followed by '|'
std::string ParagraphTexts(cDocument *doc)
{
    std::string text;
    for (PARAGRAPH_T para = 0; para < doc->GetNumberofParagraphs(); para++)
    {
        text += doc->GetParagraphText(para) + "|";
    }
    return text;
}

}


// ESC FS without a closing FS loads the glyph of the start byte, then the
// following bytes as usual; it saves back as ESC <start byte> FS.
// An unpadded file ending in ESC FS loads nothing for it and is not tested.
TEST_CASE("cWordstarFile - ESC FS without a closing FS loads the glyph of the start byte")
{
    ensureQApplication();
    cEditorCtrl editor;
    cDocument *doc = editor.GetDocument();

    const std::string leftArrow("\xE2\x86\x90");            // U+2190, glyph of ESC

    struct sPage
    {
        eCodePage page;
        const char *glyph9B;                                // glyph of 0x9B
        const char *glyph81;                                // glyph of 0x81
    };
    const sPage pages[] =
    {
        { CP437, "\xC2\xA2", "\xC3\xBC" },                  // U+00A2, U+00FC
        { CP737, "\xCE\xB4", "\xCE\x92" },                  // U+03B4, U+0392
        { CP850, "\xC3\xB8", "\xC3\xBC" },                  // U+00F8, U+00FC
    };

    for (const sPage &page : pages)
    {
        CAPTURE(static_cast<int>(page.page));
        editor.SetCodePage(page.page);

        SUBCASE("followed by text")
        {
            std::string saved = EditorRoundTrip(editor, WordStar7File("a\x1B\x1C" "bc\x0D\x0A"));
            CHECK(Hex(ParagraphTexts(doc)) == Hex("a" + leftArrow + "bc\x0D|\x7F|"));
            CHECK(Hex(saved, 128, 8) == Hex("a\x1B\x1B\x1C" "bc\x0D\x0A"));
        }

        SUBCASE("followed by a style byte")
        {
            std::string saved = EditorRoundTrip(editor, WordStar7File("a\x1B\x1C\x02" "b\x02\x0D\x0A"));
            CHECK(Hex(ParagraphTexts(doc)) == Hex("a" + leftArrow + "\x7F" "b\x7F\x0D|\x7F|"));
            CHECK(Hex(saved, 128, 9) == Hex("a\x1B\x1B\x1C\x02" "b\x02\x0D\x0A"));
        }

        SUBCASE("followed by CR LF")
        {
            std::string saved = EditorRoundTrip(editor, WordStar7File("a\x1B\x1C\x0D\x0A" "b\x0D\x0A"));
            CHECK(Hex(ParagraphTexts(doc)) == Hex("a" + leftArrow + "\x0D|b\x0D|\x7F|"));
            CHECK(Hex(saved, 128, 9) == Hex("a\x1B\x1B\x1C\x0D\x0A" "b\x0D\x0A"));
        }

        SUBCASE("started by 0x9B")
        {
            std::string saved = EditorRoundTrip(editor, WordStar7File("a\x9B\x1C" "bc\x0D\x0A"));
            CHECK(Hex(ParagraphTexts(doc)) == Hex(std::string("a") + page.glyph9B + "bc\x0D|\x7F|"));
            CHECK(Hex(saved, 128, 8) == Hex("a\x1B\x9B\x1C" "bc\x0D\x0A"));
        }

        SUBCASE("followed by another ESC x FS")
        {
            std::string saved = EditorRoundTrip(editor, WordStar7File("a\x1B\x1C\x1B\x81\x1C" "b\x0D\x0A"));
            CHECK(Hex(ParagraphTexts(doc)) == Hex("a" + leftArrow + page.glyph81 + "b\x0D|\x7F|"));
            CHECK(Hex(saved, 128, 10) == Hex("a\x1B\x1B\x1C\x1B\x81\x1C" "b\x0D\x0A"));
        }

        SUBCASE("at the end of the text before the 0x1A padding")
        {
            std::string saved = EditorRoundTrip(editor, WordStar7File("ab\x1B\x1C"));
            CHECK(Hex(ParagraphTexts(doc)) == Hex("ab" + leftArrow + "\x7F|"));
            CHECK(Hex(saved, 128, 6) == Hex("ab\x1B\x1B\x1C\x1A"));
        }
    }
}


namespace
{

// One character per document position: a marker as its control code, any
// other position as the first byte of its text
std::string PositionMap(cDocument* doc)
{
    std::string map;
    for (POSITION_T pos = 0; pos < doc->GetTextSize(); pos++)
    {
        std::string ch = doc->GetCharNoAdvance(pos);
        if (ch.length() == 1 && static_cast<unsigned char>(ch[0]) == MARKER_CHAR)
        {
            map += static_cast<char>(doc->GetControlChar(pos));
        }
        else
        {
            map += ch.empty() ? '\0' : ch[0];
        }
    }
    return map;
}

// Writes positions start..end of the editor's document with SaveRange() and returns the bytes
std::string SavedRange(cEditorCtrl &editor, POSITION_T start, POSITION_T end)
{
    cWordstarFile saver(&editor);
    saver.mDocument = editor.GetDocument();
    saver.SetSilent(true);
    std::filesystem::path out = TempPath();
    std::string saved;
    if (saver.SaveRange(out.string(), start, end))
    {
        saved = ReadFileBytes(out);
    }
    std::filesystem::remove(out);
    return saved;
}

// Writes the editor's whole document with SaveFile() and returns the bytes
std::string SavedWhole(cEditorCtrl &editor)
{
    cWordstarFile saver(&editor);
    saver.mDocument = editor.GetDocument();
    saver.SetSilent(true);
    std::filesystem::path out = TempPath();
    std::string saved;
    if (saver.SaveFile(out.string(), editor.GetDocument()->GetTextSize()))
    {
        saved = ReadFileBytes(out);
    }
    std::filesystem::remove(out);
    return saved;
}

// Loads WordStar bytes into the editor's document
bool LoadBytes(cEditorCtrl &editor, const std::string &bytes)
{
    cWordstarFile loader;
    loader.mDocument = editor.GetDocument();
    std::filesystem::path in = TempPath();
    WriteFileBytes(in, bytes);
    bool loaded = loader.LoadFile(in.string());
    std::filesystem::remove(in);
    return loaded;
}

}   // anonymous namespace


TEST_CASE("cWordstarFile::SaveRange - the whole document gives the bytes of a full save")
{
    ensureQApplication();
    cEditorCtrl editor;
    editor.GetDocument()->Clear();

    std::string note = std::string("\x02\x00\x00\x00\x00", 5) + "note text";
    std::string body = ".lm 5\x0D\x0A" "one \x02two\x02 Hi" + SequenceBytes(SEQ_FOOTNOTE, note) +
                       "!\x0D\x0A" "a\x09" "b\x0D\x0A" "end\x0D\x0A";
    REQUIRE(LoadBytes(editor, WordStar7File(body)));

    std::string whole = SavedWhole(editor);
    REQUIRE(whole.empty() == false);
    CHECK(SavedRange(editor, 0, editor.GetDocument()->GetTextSize()) == whole);
}


TEST_CASE("cWordstarFile::SaveRange - a block is written as a full save of just the block")
{
    ensureQApplication();
    cEditorCtrl editor;
    cDocument* doc = editor.GetDocument();
    doc->Clear();

    std::string note = std::string("\x02\x00\x00\x00\x00", 5) + "note text";
    std::string footnote = SequenceBytes(SEQ_FOOTNOTE, note);
    std::string body = ".lm 5\x0D\x0A" "one \x02two\x02 Hi" + footnote + "!\x0D\x0A" "end\x0D\x0A";
    REQUIRE(LoadBytes(editor, WordStar7File(body)));

    std::string map = PositionMap(doc);
    size_t bold = map.find("\x02two");
    size_t bang = map.find("!\x0D");
    REQUIRE(bold != std::string::npos);
    REQUIRE(bang != std::string::npos);

    SUBCASE("no dot command from outside the block, the footnote with its text")
    {
        std::string saved = SavedRange(editor, static_cast<POSITION_T>(bold), static_cast<POSITION_T>(bang + 2));
        CHECK(saved == WordStar7File("\x02two\x02 Hi" + footnote + "!\x0D\x0A"));

        cEditorCtrl reader;
        reader.GetDocument()->Clear();
        REQUIRE(LoadBytes(reader, saved));
        std::string loaded = PositionMap(reader.GetDocument());
        CHECK(loaded.rfind(std::string("\x02two\x02 Hi") + static_cast<char>(STYLE_FOOTNOTE) + "!\x0D", 0) == 0);
        sNote loadedNote{};
        CHECK(reader.GetDocument()->GetNote(8, loadedNote));
        CHECK(loadedNote.raw == note);
    }

    SUBCASE("a block that starts inside bold gets no opening bold")
    {
        std::string saved = SavedRange(editor, static_cast<POSITION_T>(bold + 1), static_cast<POSITION_T>(bold + 5));
        CHECK(saved == WordStar7File("two\x02"));
    }

    SUBCASE("a block that ends inside bold gets no closing bold")
    {
        std::string saved = SavedRange(editor, static_cast<POSITION_T>(bold - 4), static_cast<POSITION_T>(bold + 4));
        CHECK(saved == WordStar7File("one \x02two"));
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// For every .WS file under WORDTSAR_ARCHIVE_DIR (recursive) loaded through
/// cEditorCtrl::LoadFile(), SaveRange() over the whole document gives the
/// bytes SaveFile() gives. Without WORDTSAR_ARCHIVE_DIR the test does nothing.
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("cWordstarFile::SaveRange - every .WS file in WORDTSAR_ARCHIVE_DIR: the whole range equals a full save")
{
    const char* archiveEnv = std::getenv("WORDTSAR_ARCHIVE_DIR");
    if (archiveEnv == nullptr || archiveEnv[0] == '\0')
    {
        MESSAGE("WORDTSAR_ARCHIVE_DIR is not set, archive range test skipped");
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
        cEditorCtrl editor;
        if (editor.LoadFile(path.string()) == false)
        {
            continue;
        }
        std::string whole = SavedWhole(editor);
        std::string range = SavedRange(editor, 0, editor.GetDocument()->GetTextSize());
        if (whole.empty() || range != whole)
        {
            failures.push_back(path.string());
        }
    }

    std::string report;
    for (const std::string& failure : failures)
    {
        report += "\n  " + failure;
    }
    CHECK_MESSAGE(failures.empty(), failures.size() << " of " << files.size() << " .WS files differ" << report);
}


namespace
{

// Writes target to a file in a new temp directory, inserts bytes with
// cWordstarFile::InsertAtTextEnd() and returns the file content
std::string InsertedAtTextEnd(const std::string &target, const std::string &bytes)
{
    std::filesystem::path dir = UniqueTempDir("wordtsar_textend");
    std::filesystem::path path = dir / "target.ws";
    WriteFileBytes(path, target);
    bool written = cWordstarFile::InsertAtTextEnd(path.string(), bytes);
    std::string result = ReadFileBytes(path);
    std::filesystem::remove_all(dir);
    return written ? result : std::string();
}

}


TEST_CASE("cWordstarFile::InsertAtTextEnd - ESC x FS triples are text")
{
    SUBCASE("ESC 1A FS")
    {
        std::string text = std::string("A\x1B\x1A\x1C" "B\r\n", 7);
        std::string target = WordStar7File(text);
        CHECK(InsertedAtTextEnd(target, "new\r\n") == PadEof(target.substr(0, 128 + text.size()) + "new\r\n"));
    }

    SUBCASE("ESC 1D FS before a style library")
    {
        std::string text = std::string("A\x1B\x1D\x1C" "B\r\n", 7);
        std::string library = "\x1A\x55" + std::string(17000, 'L');
        std::string target = WordStar7File(text) + library;

        std::string expected = PadEof(target.substr(0, 128 + text.size()) + "new\r\n");
        uint32_t styles = static_cast<uint32_t>(expected.size());
        memcpy(&expected[4 + offsetof(sWSHeader, styles)], &styles, sizeof(styles));
        expected += library;
        CHECK(InsertedAtTextEnd(target, "new\r\n") == expected);
    }
}


TEST_CASE("cWordstarFile::InsertAtTextEnd - a stray 0x1D is not skipped as a sequence")
{
    std::string text = std::string("A\x1D\x05\x00" "B\r\n", 7);
    std::string target = WordStar7File(text);
    CHECK(InsertedAtTextEnd(target, "new\r\n") == PadEof(target.substr(0, 128 + text.size()) + "new\r\n"));
}
