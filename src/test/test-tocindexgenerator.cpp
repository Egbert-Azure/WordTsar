//////////////////////////////////////////////////////////////////////////////
//
// WordTsar - Wordstar clone for modern systems http://wordtsar.ca
// Copyright (C) 2018 Gerald Brandt
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

#include <cctype>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "src/core/document/document.h"
#include "src/core/generate/tocindexgenerator.h"
#include "src/core/include/config.h"
#include "src/core/layout/layoutbase.h"
#include "src/gui/editor/editorctrl.h"
#include "src/test/tocindexgen-testutil.h"
#include <QApplication>

/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// cEditorCtrl is a QWidget subclass, so constructing one requires a live
/// QApplication (see test-docxwriter.cpp for the same pattern).
///
/////////////////////////////////////////////////////////////////////////////
namespace {
static int argc = 0;
static char* argv[] = {nullptr};
static QApplication* app = nullptr;

void ensureQApplication()
{
    if (!QApplication::instance())
    {
        app = new QApplication(argc, argv);
    }
}

std::string ReadWholeFile(const std::string &path)
{
    cEditorCtrl reader ;
    reader.LoadFile(path) ;

    std::string all ;
    cDocument *doc = reader.GetDocument() ;
    for (ssize_t i = 0 ; i < doc->GetNumberofParagraphs() ; i++)
    {
        all += doc->GetParagraphText(i) ;
    }
    return all ;
}
}


TEST_CASE("TOC/Index generator - table of contents resolves real page numbers, replaces #, splits by .tcN")
{
    ensureQApplication() ;

    cEditorCtrl editor ;
    cDocument *doc = editor.GetDocument() ;
    doc->Clear() ;

    doc->Insert(".tc Chapter One . . . #\r") ;
    doc->Insert(".pa\r") ;
    doc->Insert("Second page content\r") ;
    doc->Insert(".tc Chapter Two . . . #\r") ;
    doc->Insert(".tc1 Figure 1 . . . #\r") ;

    editor.GetLayout()->LayoutDocument(doc) ;

    std::string sourcePath = "/tmp/wordtsar_tocgen_source.ws" ;
    std::vector<std::string> outputFiles ;
    bool ok = cTOCIndexGenerator::GenerateTOC(&editor, sourcePath, outputFiles) ;

    REQUIRE(ok) ;
    REQUIRE(outputFiles.size() == 2) ;

    std::string tocPath ;
    std::string t01Path ;
    for (auto &path : outputFiles)
    {
        if (path.find(".TOC") != std::string::npos) tocPath = path ;
        if (path.find(".T01") != std::string::npos) t01Path = path ;
    }
    REQUIRE(tocPath.empty() == false) ;
    REQUIRE(t01Path.empty() == false) ;

    std::string tocContent = ReadWholeFile(tocPath) ;
    // Chapter One is on page 1, Chapter Two is on page 2 (after the .pa).
    CHECK(tocContent.find("Chapter One . . . 1") != std::string::npos) ;
    CHECK(tocContent.find("Chapter Two . . . 2") != std::string::npos) ;
    // No unresolved placeholder should survive.
    CHECK(tocContent.find('#') == std::string::npos) ;

    std::string t01Content = ReadWholeFile(t01Path) ;
    CHECK(t01Content.find("Figure 1 . . . 2") != std::string::npos) ;
    // The numbered table must not also appear in the main .TOC file.
    CHECK(tocContent.find("Figure 1") == std::string::npos) ;
}


TEST_CASE("TOC/Index generator - a bare .tc/.tcN with no entry text is skipped, not written as a blank line")
{
    ensureQApplication() ;

    cEditorCtrl editor ;
    cDocument *doc = editor.GetDocument() ;
    doc->Clear() ;

    doc->Insert(".tc\r") ;                  // bare -- no text at all
    doc->Insert(".tc Real Entry\r") ;
    doc->Insert(".tc3\r") ;                 // bare, numbered table -- no text at all
    doc->Insert(".tc3 Another Entry\r") ;

    editor.GetLayout()->LayoutDocument(doc) ;

    std::string sourcePath = "/tmp/wordtsar_tocgen_blank_source.ws" ;
    std::vector<std::string> outputFiles ;
    bool ok = cTOCIndexGenerator::GenerateTOC(&editor, sourcePath, outputFiles) ;

    REQUIRE(ok) ;
    REQUIRE(outputFiles.size() == 2) ;

    for (auto &path : outputFiles)
    {
        std::string content = ReadWholeFile(path) ;
        bool hasRealEntry = (content.find("Real Entry") != std::string::npos) ||
                             (content.find("Another Entry") != std::string::npos) ;
        CHECK(hasRealEntry) ;

        // No paragraph in either output file should be just a bare page
        // number with nothing in front of it -- that's the blank-line bug.
        std::istringstream stream(content) ;
        std::string line ;
        while (std::getline(stream, line))
        {
            bool isBareNumber = !line.empty() ;
            for (char c : line)
            {
                if (!std::isdigit(static_cast<unsigned char>(c)))
                {
                    isBareNumber = false ;
                    break ;
                }
            }
            CHECK_FALSE(isBareNumber) ;
        }
    }
}


TEST_CASE("TOC/Index generator - index sorts, merges duplicates, bolds, and skips page numbers for cross-references")
{
    ensureQApplication() ;

    cEditorCtrl editor ;
    cDocument *doc = editor.GetDocument() ;
    doc->Clear() ;

    doc->Insert(".ix Zebra\r") ;
    doc->Insert(".ix Apple\r") ;
    doc->Insert(".pa\r") ;
    doc->Insert("More content\r") ;
    doc->Insert(".ix Apple\r") ;                    // second occurrence, later page -- must merge
    doc->Insert(".ix +Bold Entry\r") ;               // bold page number
    doc->Insert(".ix -See Also, cross ref\r") ;      // cross-reference: no page number, comma kept literal

    editor.GetLayout()->LayoutDocument(doc) ;

    std::string sourcePath = "/tmp/wordtsar_indexgen_source.ws" ;
    std::string outputFile ;
    bool ok = cTOCIndexGenerator::GenerateIndex(&editor, sourcePath, outputFile) ;

    REQUIRE(ok) ;
    REQUIRE(outputFile.find(".IDX") != std::string::npos) ;

    std::string content = ReadWholeFile(outputFile) ;

    // Alphabetical: Apple before Bold Entry before See Also before Zebra.
    size_t applePos = content.find("Apple") ;
    size_t boldPos = content.find("Bold Entry") ;
    size_t seePos = content.find("See Also") ;
    size_t zebraPos = content.find("Zebra") ;
    REQUIRE(applePos != std::string::npos) ;
    REQUIRE(boldPos != std::string::npos) ;
    REQUIRE(seePos != std::string::npos) ;
    REQUIRE(zebraPos != std::string::npos) ;
    CHECK(applePos < boldPos) ;
    CHECK(boldPos < seePos) ;
    CHECK(seePos < zebraPos) ;

    // The two "Apple" entries (pages 1 and 2) merged onto one line.
    CHECK(content.find("Apple, 1, 2") != std::string::npos) ;

    // The cross-reference kept its comma literally and carries no page number.
    CHECK(content.find("See Also, cross ref") != std::string::npos) ;

    // Bold Entry's page number is wrapped in a real bold toggle (MARKER_CHAR).
    bool sawMarkerNearBold = false ;
    for (size_t i = boldPos ; i < content.size() && i < boldPos + 40 ; i++)
    {
        if (static_cast<unsigned char>(content[i]) == static_cast<unsigned char>(MARKER_CHAR))
        {
            sawMarkerNearBold = true ;
        }
    }
    CHECK(sawMarkerNearBold) ;
}


TEST_CASE("TOC/Index generator - .tc dot entries are written verbatim in document order as working dot commands")
{
    ensureQApplication() ;
    tocindextest::CheckTOCDotEntries<cEditorCtrl>() ;
}


TEST_CASE("TOC/Index generator - .tc comment, bare, indented, arithmetic and numbered-table dot entries")
{
    ensureQApplication() ;
    tocindextest::CheckTOCDotEntryEdgeCases<cEditorCtrl>() ;
}


TEST_CASE("TOC/Index generator - a TOC holding only dot entries is written")
{
    ensureQApplication() ;
    tocindextest::CheckTOCOnlyDotEntries<cEditorCtrl>() ;
}


TEST_CASE("TOC/Index generator - .ix dot entries come first in document order, unparsed, without page numbers")
{
    ensureQApplication() ;
    tocindextest::CheckIndexDotEntries<cEditorCtrl>() ;
}


TEST_CASE("TOC/Index generator - .ix comment, page break, bare, indented, escaped and duplicate dot entries")
{
    ensureQApplication() ;
    tocindextest::CheckIndexDotEntryEdgeCases<cEditorCtrl>() ;
}


TEST_CASE("TOC/Index generator - an index holding only dot entries is written")
{
    ensureQApplication() ;
    tocindextest::CheckIndexOnlyDotEntries<cEditorCtrl>() ;
}


TEST_CASE("TOC/Index generator - .tc .po 1i and .ix .lm 5 are written as dot command lines")
{
    ensureQApplication() ;
    tocindextest::CheckDotEntrySource<cEditorCtrl>() ;
}


TEST_CASE("TOC/Index generator - a kept sequence in .tc and .ix entry text leaves no space")
{
    ensureQApplication() ;
    tocindextest::CheckSequenceInEntryText<cEditorCtrl>() ;
}


TEST_CASE("TOC/Index generator - index marks around a word merge with .ix entries for that word")
{
    ensureQApplication() ;
    tocindextest::CheckIndexMarksMergeWithIx<cEditorCtrl>() ;
}


TEST_CASE("TOC/Index generator - an index mark without a partner in its paragraph gives no entry")
{
    ensureQApplication() ;
    tocindextest::CheckUnpairedIndexMarks<cEditorCtrl>() ;
}


TEST_CASE("TOC/Index generator - index marks in dot command lines give no entry")
{
    ensureQApplication() ;
    tocindextest::CheckIndexMarksInDotLines<cEditorCtrl>() ;
}


TEST_CASE("TOC/Index generator - index mark entry text is the plain text, taken as it is")
{
    ensureQApplication() ;
    tocindextest::CheckIndexMarkPlainText<cEditorCtrl>() ;
}


TEST_CASE("TOC/Index generator - an index mark gets the page of its line")
{
    ensureQApplication() ;
    tocindextest::CheckIndexMarkPageOfLine<cEditorCtrl>() ;
}


TEST_CASE("TOC/Index generator - a kept 0Eh index item is an index entry like .ix with its body")
{
    ensureQApplication() ;
    tocindextest::CheckIndexItemSequence<cEditorCtrl>() ;
}


TEST_CASE("TOC/Index generator - a kept 0Eh index item body: empty or control bytes give no entry, high bytes use the code page")
{
    ensureQApplication() ;
    tocindextest::CheckIndexItemSequenceBody<cEditorCtrl>() ;
}


TEST_CASE("TOC/Index generator - files that cannot be written are reported")
{
    ensureQApplication() ;
    tocindextest::CheckUnwritableOutput<cEditorCtrl>() ;
}


namespace {

/////////////////////////////////////////////////////////////////////////////
/// cEditorCtrl with the file chooser, the save question and the message
/// boxes of the Generate commands replaced by recorded answers.
/////////////////////////////////////////////////////////////////////////////
class cGenerateTestEditor : public cEditorCtrl
{
public:
    std::string mChooserAnswer ;
    std::vector<std::string> mChooserCalls ;
    eSaveQuestion mSaveAnswer = SAVE_QUESTION_CANCEL ;
    std::vector<std::string> mSaveQuestions ;
    std::vector<std::string> mMessages ;
    std::vector<std::string> mErrors ;
    int mTopLevelAtMessage = -1 ;

    void ShowMessage(const std::string &title, const std::string &message) override
    {
        mTopLevelAtMessage = static_cast<int>(QApplication::topLevelWidgets().size()) ;
        mMessages.push_back(title + ": " + message) ;
    }

    void ShowError(const std::string &title, const std::string &message) override
    {
        mTopLevelAtMessage = static_cast<int>(QApplication::topLevelWidgets().size()) ;
        mErrors.push_back(title + ": " + message) ;
    }

protected:
    std::string PromptForGenerateSource(const std::string &title, const std::string &defaultPath) override
    {
        mChooserCalls.push_back(title + ": " + defaultPath) ;
        return mChooserAnswer ;
    }

    eSaveQuestion AskSaveBeforeGenerate(const std::string &filename) override
    {
        mSaveQuestions.push_back(filename) ;
        return mSaveAnswer ;
    }
};


/////////////////////////////////////////////////////////////////////////////
/// Text, modified flag, caret and block of an editor's document.
/////////////////////////////////////////////////////////////////////////////
struct sDocumentState
{
    std::string text ;
    bool modified = false ;
    POSITION_T position = 0 ;
    bool block = false ;
    POSITION_T blockStart = 0 ;
    POSITION_T blockEnd = 0 ;
    bool canRedo = false ;

    bool operator==(const sDocumentState &other) const
    {
        return text == other.text && modified == other.modified && position == other.position &&
               block == other.block && blockStart == other.blockStart && blockEnd == other.blockEnd &&
               canRedo == other.canRedo ;
    }
};

sDocumentState StateOf(cEditorCtrl &editor)
{
    cDocument *doc = editor.GetDocument() ;
    sDocumentState state ;
    state.text = doc->GetText() ;
    state.modified = doc->IsModified() ;
    state.position = doc->GetPosition() ;
    state.block = doc->GetBlock(state.blockStart, state.blockEnd) ;
    state.canRedo = doc->CanRedo() ;
    return state ;
}


/////////////////////////////////////////////////////////////////////////////
/// Number of undo steps; undoes them all.
/////////////////////////////////////////////////////////////////////////////
int UndoDepth(cEditorCtrl &editor)
{
    int depth = 0 ;
    while (editor.GetDocument()->Undo())
    {
        depth++ ;
    }
    return depth ;
}


/////////////////////////////////////////////////////////////////////////////
/// Types text into paragraph `paragraph`, marks a block and moves the caret.
/////////////////////////////////////////////////////////////////////////////
void EditDocument(cEditorCtrl &editor, PARAGRAPH_T paragraph, const std::string &text)
{
    cDocument *doc = editor.GetDocument() ;
    POSITION_T start ;
    POSITION_T end ;
    doc->GetParagraphStartandEnd(paragraph, start, end) ;
    doc->SetPosition(start) ;
    doc->Insert(text) ;
    doc->SetPosition(start + 1) ;
    doc->SetBeginBlock() ;
    doc->SetPosition(start + 4) ;
    doc->SetEndBlock() ;
    doc->SetPosition(start + 2) ;
    editor.LayoutDocument(true) ;
}


std::string JoinLines(const std::vector<std::string> &lines)
{
    std::string result ;
    for (size_t loop = 0 ; loop < lines.size() ; loop++)
    {
        result += (loop > 0 ? "\n" : "") + lines[loop] ;
    }
    return result ;
}

} // namespace


TEST_CASE("GUI Generate - table of contents and index of a file on disk match the generator byte for byte")
{
    ensureQApplication() ;

    tocindextest::cScratchDir dir("guigolden") ;
    REQUIRE(dir.IsValid()) ;
    std::string sourcePath = dir.File("golden.ws") ;
    tocindextest::WriteGoldenSource<cEditorCtrl>(sourcePath) ;
    std::string stem = dir.File("golden") ;

    cGenerateTestEditor editor ;
    editor.mFileDir = dir.File("") ;
    editor.mChooserAnswer = sourcePath ;
    editor.GenerateTOCFromFile() ;
    editor.GenerateIndexFromFile() ;

    CHECK(JoinLines(editor.mChooserCalls) == "Generate Table of Contents: " + dir.File("") + "\n" +
                                             "Generate Index: " + dir.File("")) ;
    CHECK(editor.mSaveQuestions.empty()) ;
    CHECK(editor.mErrors.empty()) ;
    CHECK(JoinLines(editor.mMessages) == "Table of Contents: Wrote:\n" + stem + ".TOC\n" + stem + ".T01\n" +
                                         "Index: Wrote:\n" + stem + ".IDX") ;
    CHECK(tocindextest::DirectoryListing(dir.File("")) == "golden.IDX|golden.T01|golden.TOC|golden.ws") ;

    std::string toc = tocindextest::ReadBytes(stem + ".TOC") ;
    std::string t01 = tocindextest::ReadBytes(stem + ".T01") ;
    std::string idx = tocindextest::ReadBytes(stem + ".IDX") ;
    REQUIRE(toc.empty() == false) ;
    REQUIRE(t01.empty() == false) ;
    REQUIRE(idx.empty() == false) ;
    tocindextest::CheckGoldenOutputs<cEditorCtrl>(stem) ;

    for (const char *extension : { ".TOC", ".T01", ".IDX" })
    {
        std::filesystem::remove(stem + extension) ;
    }
    std::vector<std::string> written = tocindextest::GenerateFromDisk<cEditorCtrl>(sourcePath) ;
    CHECK(tocindextest::Join(written) == stem + ".TOC|" + stem + ".T01|" + stem + ".IDX") ;
    CHECK(tocindextest::ReadBytes(stem + ".TOC") == toc) ;
    CHECK(tocindextest::ReadBytes(stem + ".T01") == t01) ;
    CHECK(tocindextest::ReadBytes(stem + ".IDX") == idx) ;
}


TEST_CASE("GUI Generate - the open document is unchanged when it is the source and when it is not")
{
    ensureQApplication() ;

    tocindextest::cScratchDir dir("guiunchanged") ;
    REQUIRE(dir.IsValid()) ;
    std::string sourcePath = dir.File("golden.ws") ;
    tocindextest::WriteGoldenSource<cEditorCtrl>(sourcePath) ;

    for (bool isSource : { true, false })
    {
        CAPTURE(isSource) ;

        cEditorCtrl control ;
        cGenerateTestEditor editor ;
        if (isSource)
        {
            REQUIRE(control.LoadFile(sourcePath)) ;
            REQUIRE(editor.LoadFile(sourcePath)) ;
        }
        EditDocument(control, 1, "Typed ") ;
        EditDocument(editor, 1, "Typed ") ;
        if (isSource)
        {
            REQUIRE(editor.SaveFile(sourcePath)) ;
            editor.MarkDocumentSaved() ;
            control.MarkDocumentSaved() ;
            CHECK(editor.mChooserCalls.empty()) ;
        }
        int controlDepth = UndoDepth(control) ;
        REQUIRE(controlDepth > 0) ;

        sDocumentState before = StateOf(editor) ;
        CHECK(before.modified == (isSource == false)) ;
        CHECK(before.block) ;

        editor.mChooserAnswer = sourcePath ;
        editor.GenerateTOCFromFile() ;
        editor.GenerateIndexFromFile() ;
        CHECK(editor.mErrors.empty()) ;
        CHECK(editor.mMessages.size() == 2) ;
        CHECK(editor.mSaveQuestions.empty()) ;
        if (isSource)
        {
            CHECK(editor.mChooserCalls.front() == "Generate Table of Contents: " + sourcePath) ;
        }

        sDocumentState after = StateOf(editor) ;
        CHECK(after == before) ;
        CHECK(after.text == before.text) ;
        CHECK(after.modified == before.modified) ;
        CHECK(after.position == before.position) ;
        CHECK(after.blockStart == before.blockStart) ;
        CHECK(after.blockEnd == before.blockEnd) ;
        CHECK(UndoDepth(editor) == controlDepth) ;
    }
}


TEST_CASE("GUI Generate - unsaved changes in the source: Cancel, Save and Don't save")
{
    ensureQApplication() ;

    for (bool index : { false, true })
    {
        for (cEditorCtrl::eSaveQuestion answer : { cEditorCtrl::SAVE_QUESTION_CANCEL, cEditorCtrl::SAVE_QUESTION_SAVE,
                                                   cEditorCtrl::SAVE_QUESTION_DONT_SAVE })
        {
            CAPTURE(index) ;
            CAPTURE(static_cast<int>(answer)) ;

            tocindextest::cScratchDir dir("guisave") ;
            REQUIRE(dir.IsValid()) ;
            std::string sourcePath = dir.File("golden.ws") ;
            tocindextest::WriteGoldenSource<cEditorCtrl>(sourcePath) ;
            std::string original = tocindextest::ReadBytes(sourcePath) ;
            std::string stem = dir.File("golden") ;

            cGenerateTestEditor editor ;
            REQUIRE(editor.LoadFile(sourcePath)) ;
            cDocument *doc = editor.GetDocument() ;
            doc->SetPosition(0) ;
            doc->Insert(index ? ".ix Added" : ".tc Added #") ;
            doc->Insert(HARD_RETURN) ;
            editor.LayoutDocument(true) ;
            REQUIRE(doc->IsModified()) ;
            std::string edited = doc->GetText() ;

            editor.mChooserAnswer = sourcePath ;
            editor.mSaveAnswer = answer ;
            if (index)
            {
                editor.GenerateIndexFromFile() ;
            }
            else
            {
                editor.GenerateTOCFromFile() ;
            }

            CHECK(JoinLines(editor.mSaveQuestions) == "golden.ws") ;
            CHECK(editor.mErrors.empty()) ;
            CHECK(doc->GetText() == edited) ;

            std::string output = stem + (index ? ".IDX" : ".TOC") ;
            if (answer == cEditorCtrl::SAVE_QUESTION_CANCEL)
            {
                CHECK(editor.mMessages.empty()) ;
                CHECK(doc->IsModified()) ;
                CHECK(tocindextest::ReadBytes(sourcePath) == original) ;
                CHECK_FALSE(std::filesystem::exists(stem + ".TOC")) ;
                CHECK_FALSE(std::filesystem::exists(stem + ".T01")) ;
                CHECK_FALSE(std::filesystem::exists(stem + ".IDX")) ;
                continue ;
            }

            CHECK(editor.mMessages.size() == 1) ;
            cEditorCtrl reader ;
            std::string generated = tocindextest::Join(tocindextest::LoadGenerated(reader, output)) ;
            if (answer == cEditorCtrl::SAVE_QUESTION_SAVE)
            {
                CHECK_FALSE(doc->IsModified()) ;
                CHECK(tocindextest::ReadBytes(sourcePath) != original) ;
                CHECK(generated == (index ? std::string(".pm 0|Added, 1|Apple, 1|marked, 1|Zebra, 2")
                                          : std::string("Added 1|Chapter One 1|.lm 0|Chapter Two 2"))) ;
            }
            else
            {
                CHECK(doc->IsModified()) ;
                CHECK(tocindextest::ReadBytes(sourcePath) == original) ;
                CHECK(generated == (index ? tocindextest::GoldenIndex() : tocindextest::GoldenTOC())) ;
            }
        }
    }
}


TEST_CASE("GUI Generate - a source without entries writes nothing and says so")
{
    ensureQApplication() ;

    tocindextest::cScratchDir dir("guinone") ;
    REQUIRE(dir.IsValid()) ;
    std::string sourcePath = dir.File("plain.ws") ;
    {
        cEditorCtrl writer ;
        tocindextest::SetSource(writer, { "Plain text", ".pa", "More text" }) ;
        cWordstarFile file(&writer) ;
        REQUIRE(file.SaveFile(sourcePath, writer.GetDocument()->GetTextSize())) ;
    }

    cGenerateTestEditor editor ;
    editor.mChooserAnswer = sourcePath ;
    editor.GenerateTOCFromFile() ;
    editor.GenerateIndexFromFile() ;

    CHECK(editor.mErrors.empty()) ;
    CHECK(JoinLines(editor.mMessages) == "Table of Contents: No table of contents entries were found in\n" + sourcePath + "\n" +
                                         "Index: No index entries were found in\n" + sourcePath) ;
    CHECK(tocindextest::DirectoryListing(dir.File("")) == "plain.ws") ;
}


TEST_CASE("GUI Generate - cancelling the file chooser does nothing")
{
    ensureQApplication() ;

    tocindextest::cScratchDir dir("guichooser") ;
    REQUIRE(dir.IsValid()) ;
    std::string sourcePath = dir.File("golden.ws") ;
    tocindextest::WriteGoldenSource<cEditorCtrl>(sourcePath) ;

    cGenerateTestEditor editor ;
    REQUIRE(editor.LoadFile(sourcePath)) ;
    EditDocument(editor, 1, "Typed ") ;
    sDocumentState before = StateOf(editor) ;

    editor.mChooserAnswer = "" ;
    editor.mSaveAnswer = cEditorCtrl::SAVE_QUESTION_SAVE ;
    editor.GenerateTOCFromFile() ;
    editor.GenerateIndexFromFile() ;

    CHECK(editor.mChooserCalls.size() == 2) ;
    CHECK(editor.mSaveQuestions.empty()) ;
    CHECK(editor.mMessages.empty()) ;
    CHECK(editor.mErrors.empty()) ;
    CHECK(StateOf(editor) == before) ;
    CHECK(tocindextest::DirectoryListing(dir.File("")) == "golden.ws") ;
}


TEST_CASE("GUI Generate - a file that cannot be read or written gives an error naming it")
{
    ensureQApplication() ;

    tocindextest::cScratchDir dir("guierror") ;
    REQUIRE(dir.IsValid()) ;
    std::string lockedDir = dir.File("locked") ;
    REQUIRE(std::filesystem::create_directory(lockedDir)) ;
    std::string sourcePath = (std::filesystem::path(lockedDir) / "golden.ws").string() ;
    tocindextest::WriteGoldenSource<cEditorCtrl>(sourcePath) ;
    std::string stem = (std::filesystem::path(lockedDir) / "golden").string() ;

    cGenerateTestEditor editor ;
    REQUIRE(editor.LoadFile(sourcePath)) ;
    EditDocument(editor, 1, "Typed ") ;
    sDocumentState before = StateOf(editor) ;

    REQUIRE(chmod(lockedDir.c_str(), 0555) == 0) ;
    editor.mChooserAnswer = sourcePath ;
    editor.mSaveAnswer = cEditorCtrl::SAVE_QUESTION_DONT_SAVE ;
    editor.GenerateTOCFromFile() ;
    editor.GenerateIndexFromFile() ;
    chmod(lockedDir.c_str(), 0755) ;

    CHECK(editor.mMessages.empty()) ;
    CHECK(JoinLines(editor.mErrors) == "Table of Contents: Cannot write\n" + stem + ".TOC\n" + stem + ".T01\n" +
                                       "Index: Cannot write\n" + stem + ".IDX") ;
    CHECK(StateOf(editor) == before) ;
    CHECK(tocindextest::DirectoryListing(lockedDir) == "golden.ws") ;

    std::string missing = dir.File("missing.ws") ;
    editor.mErrors.clear() ;
    editor.mChooserAnswer = missing ;
    editor.GenerateTOCFromFile() ;
    editor.GenerateIndexFromFile() ;
    CHECK(editor.mMessages.empty()) ;
    CHECK(JoinLines(editor.mErrors) == "Table of Contents: Cannot read\n" + missing + "\n" +
                                       "Index: Cannot read\n" + missing) ;
    CHECK(StateOf(editor) == before) ;
}


TEST_CASE("GUI Generate - the editor that runs the generator is gone before the message")
{
    ensureQApplication() ;

    tocindextest::cScratchDir dir("guileak") ;
    REQUIRE(dir.IsValid()) ;
    std::string sourcePath = dir.File("golden.ws") ;
    tocindextest::WriteGoldenSource<cEditorCtrl>(sourcePath) ;

    cGenerateTestEditor editor ;
    int topLevel = static_cast<int>(QApplication::topLevelWidgets().size()) ;
    int children = static_cast<int>(editor.children().size()) ;
    QWidget *active = QApplication::activeWindow() ;

    editor.mChooserAnswer = sourcePath ;
    editor.GenerateTOCFromFile() ;
    CHECK(editor.mTopLevelAtMessage == topLevel) ;
    editor.mTopLevelAtMessage = -1 ;
    editor.GenerateIndexFromFile() ;
    CHECK(editor.mTopLevelAtMessage == topLevel) ;

    CHECK(editor.mMessages.size() == 2) ;
    CHECK(static_cast<int>(QApplication::topLevelWidgets().size()) == topLevel) ;
    CHECK(static_cast<int>(editor.children().size()) == children) ;
    CHECK(QApplication::activeWindow() == active) ;
    CHECK(editor.isVisible() == false) ;
}


namespace {

/////////////////////////////////////////////////////////////////////////////
/// Runs the core generator on an editor with `font` that loads
/// `sourcePath`; returns the TOC and index lines read back.
/////////////////////////////////////////////////////////////////////////////
std::string CoreOutputWithFont(const std::string &sourcePath, const std::string &font)
{
    std::string stem = (std::filesystem::path(sourcePath).parent_path() / std::filesystem::path(sourcePath).stem()).string() ;
    {
        cEditorCtrl editor ;
        editor.SetFont(font) ;
        REQUIRE(editor.LoadFile(sourcePath)) ;
        std::vector<std::string> outputFiles ;
        REQUIRE(cTOCIndexGenerator::GenerateTOC(&editor, sourcePath, outputFiles)) ;
    }
    {
        cEditorCtrl editor ;
        editor.SetFont(font) ;
        REQUIRE(editor.LoadFile(sourcePath)) ;
        std::string outputFile ;
        REQUIRE(cTOCIndexGenerator::GenerateIndex(&editor, sourcePath, outputFile)) ;
    }
    cEditorCtrl reader ;
    std::string toc = tocindextest::Join(tocindextest::LoadGenerated(reader, stem + ".TOC")) ;
    return toc + " / " + tocindextest::Join(tocindextest::LoadGenerated(reader, stem + ".IDX")) ;
}

} // namespace


TEST_CASE("GUI Generate - page numbers come from the open editor's font")
{
    ensureQApplication() ;

    tocindextest::cScratchDir dir("guifont") ;
    REQUIRE(dir.IsValid()) ;
    std::string sourcePath = dir.File("long.ws") ;
    {
        std::vector<std::string> lines ;
        for (int loop = 1 ; loop <= 150 ; loop++)
        {
            lines.push_back("Line " + std::to_string(loop)) ;
        }
        lines.push_back(".tc End #") ;
        lines.push_back(".ix Last") ;
        cEditorCtrl writer ;
        tocindextest::SetSource(writer, lines) ;
        cWordstarFile file(&writer) ;
        REQUIRE(file.SaveFile(sourcePath, writer.GetDocument()->GetTextSize())) ;
    }

    const std::string bigFont = "Monospace|18.0|0|0|0|0|0" ;
    std::string defaultRun = CoreOutputWithFont(sourcePath, "Monospace|12.0|0|0|0|0|0") ;
    std::string bigRun = CoreOutputWithFont(sourcePath, bigFont) ;
    MESSAGE("12pt: " << defaultRun << "  18pt: " << bigRun) ;
    REQUIRE(defaultRun != bigRun) ;

    cGenerateTestEditor editor ;
    editor.SetFont(bigFont) ;
    editor.mChooserAnswer = sourcePath ;
    editor.GenerateTOCFromFile() ;
    editor.GenerateIndexFromFile() ;
    REQUIRE(editor.mMessages.size() == 2) ;

    cEditorCtrl reader ;
    std::string stem = dir.File("long") ;
    std::string guiRun = tocindextest::Join(tocindextest::LoadGenerated(reader, stem + ".TOC")) + " / " +
                         tocindextest::Join(tocindextest::LoadGenerated(reader, stem + ".IDX")) ;
    CHECK(guiRun == bigRun) ;
    CHECK(guiRun != defaultRun) ;
}
