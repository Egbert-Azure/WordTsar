#ifndef TOCINDEXGEN_TESTUTIL_H
#define TOCINDEXGEN_TESTUTIL_H

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

/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Helpers for the table of contents and index generator tests. They take
/// cEditorBase so the GUI (cEditorCtrl) and TUI (cWSEditorCtrl) tests share
/// them. Include after doctest.h.
///
/////////////////////////////////////////////////////////////////////////////

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <sys/stat.h>
#include <system_error>
#include <unistd.h>
#include <vector>

#include "src/core/document/document.h"
#include "src/core/editor/editorbase.h"
#include "src/core/generate/tocindexgenerator.h"
#include "src/core/include/config.h"
#include "src/core/layout/layoutbase.h"
#include "src/core/layout/layoutstructs.h"
#include "src/files/wordstar/wordstarfile.h"

namespace tocindextest {

/////////////////////////////////////////////////////////////////////////////
/// Fresh scratch directory under the system temp directory; removed by
/// the destructor.
/////////////////////////////////////////////////////////////////////////////
class cScratchDir
{
public:
    explicit cScratchDir(const std::string &tag)
    {
        std::string pattern = (std::filesystem::temp_directory_path() / ("wordtsar-" + tag + "-XXXXXX")).string() ;
        if (mkdtemp(pattern.data()) != nullptr)
        {
            mPath = pattern ;
        }
    }

    ~cScratchDir()
    {
        if (mPath.empty() == false)
        {
            std::error_code ec ;
            std::filesystem::remove_all(mPath, ec) ;
        }
    }

    bool IsValid(void) const { return mPath.empty() == false ; }
    std::string File(const std::string &name) const { return (mPath / name).string() ; }

private:
    std::filesystem::path mPath ;
};


/////////////////////////////////////////////////////////////////////////////
/// Replaces the editor's document with one paragraph per line and lays it
/// out, the state GenerateTOC() and GenerateIndex() expect.
/////////////////////////////////////////////////////////////////////////////
inline void SetSource(cEditorBase &editor, const std::vector<std::string> &lines)
{
    cDocument *doc = editor.GetDocument() ;
    doc->Clear() ;
    for (const std::string &line : lines)
    {
        doc->Insert(line) ;
        doc->Insert(HARD_RETURN) ;
    }
    editor.GetLayout()->LayoutDocument(doc) ;
}


/////////////////////////////////////////////////////////////////////////////
/// Inserts `line` at the document position, with tokens inserted as
/// markers: {K} index mark, {B} bold, {T} tab, {S:body} kept 0Eh index
/// item sequence with that body.
/////////////////////////////////////////////////////////////////////////////
inline void InsertMarkedLine(cDocument *doc, const std::string &line)
{
    size_t pos = 0 ;
    while (pos < line.size())
    {
        size_t open = line.find('{', pos) ;
        size_t close = (open == std::string::npos) ? std::string::npos : line.find('}', open) ;
        if (open == std::string::npos || close == std::string::npos)
        {
            doc->Insert(line.substr(pos)) ;
            return ;
        }
        if (open > pos)
        {
            doc->Insert(line.substr(pos, open - pos)) ;
        }

        std::string token = line.substr(open + 1, close - open - 1) ;
        if (token == "K")
        {
            doc->BeginIndex() ;
        }
        else if (token == "B")
        {
            doc->BeginBold() ;
        }
        else if (token == "T")
        {
            sWSTab tab ;
            tab.type = TAB_TAB ;
            doc->InsertTab(tab) ;
        }
        else if (token.compare(0, 2, "S:") == 0)
        {
            sRawSequence sequence ;
            sequence.type = 0x0E ;
            sequence.body = token.substr(2) ;
            doc->InsertRawSequence(sequence) ;
        }
        else
        {
            doc->Insert(line.substr(open, close - open + 1)) ;
        }
        pos = close + 1 ;
    }
}


/////////////////////////////////////////////////////////////////////////////
/// SetSource() with InsertMarkedLine() tokens in the lines.
/////////////////////////////////////////////////////////////////////////////
inline void SetMarkedSource(cEditorBase &editor, const std::vector<std::string> &lines)
{
    cDocument *doc = editor.GetDocument() ;
    doc->Clear() ;
    for (const std::string &line : lines)
    {
        InsertMarkedLine(doc, line) ;
        doc->Insert(HARD_RETURN) ;
    }
    editor.GetLayout()->LayoutDocument(doc) ;
}


/////////////////////////////////////////////////////////////////////////////
/// Paragraph text without marker bytes and without its terminator.
/////////////////////////////////////////////////////////////////////////////
inline std::string VisibleText(cDocument *doc, PARAGRAPH_T paragraph)
{
    std::string text = doc->GetParagraphText(paragraph) ;
    text.erase(std::remove(text.begin(), text.end(), static_cast<char>(MARKER_CHAR)), text.end()) ;
    while (text.empty() == false && (text.back() == '\r' || text.back() == '\n'))
    {
        text.pop_back() ;
    }
    return text ;
}


/////////////////////////////////////////////////////////////////////////////
/// Visible text of every paragraph of the editor's document, trailing
/// empty paragraphs left out.
/////////////////////////////////////////////////////////////////////////////
inline std::vector<std::string> Paragraphs(cEditorBase &editor)
{
    std::vector<std::string> result ;
    cDocument *doc = editor.GetDocument() ;
    for (PARAGRAPH_T paragraph = 0 ; paragraph < doc->GetNumberofParagraphs() ; paragraph++)
    {
        result.push_back(VisibleText(doc, paragraph)) ;
    }
    while (result.empty() == false && result.back().empty())
    {
        result.pop_back() ;
    }
    return result ;
}


/////////////////////////////////////////////////////////////////////////////
/// Lines joined with '|' so a failed comparison prints them.
/////////////////////////////////////////////////////////////////////////////
inline std::string Join(const std::vector<std::string> &lines)
{
    std::string result ;
    for (size_t loop = 0 ; loop < lines.size() ; loop++)
    {
        if (loop > 0)
        {
            result += '|' ;
        }
        result += lines[loop] ;
    }
    return result ;
}


/////////////////////////////////////////////////////////////////////////////
/// Loads a generated file into a reader editor through the normal file
/// load and layout, and returns its paragraphs.
/////////////////////////////////////////////////////////////////////////////
inline std::vector<std::string> LoadGenerated(cEditorBase &reader, const std::string &path)
{
    REQUIRE(reader.LoadFile(path)) ;
    return Paragraphs(reader) ;
}


/////////////////////////////////////////////////////////////////////////////
/// First paragraph of the reader's document whose visible text is `text`,
/// or NOT_SET.
/////////////////////////////////////////////////////////////////////////////
inline PARAGRAPH_T FindParagraph(cEditorBase &reader, const std::string &text)
{
    cDocument *doc = reader.GetDocument() ;
    for (PARAGRAPH_T paragraph = 0 ; paragraph < doc->GetNumberofParagraphs() ; paragraph++)
    {
        if (VisibleText(doc, paragraph) == text)
        {
            return paragraph ;
        }
    }
    return NOT_SET ;
}


/////////////////////////////////////////////////////////////////////////////
/// First line layout of the paragraph whose visible text is `text`.
/////////////////////////////////////////////////////////////////////////////
inline const sLineLayout *FirstLineOf(cEditorBase &reader, const std::string &text)
{
    PARAGRAPH_T paragraph = FindParagraph(reader, text) ;
    if (paragraph == NOT_SET)
    {
        return nullptr ;
    }
    const sParagraphLayout *layout = reader.GetLayout()->GetParagraphLayout(paragraph) ;
    if (layout == nullptr || layout->lines.empty())
    {
        return nullptr ;
    }
    return &layout->lines[0] ;
}


/////////////////////////////////////////////////////////////////////////////
/// Checks that the paragraph `text` of the loaded document is a dot
/// command the layout carries out.
/////////////////////////////////////////////////////////////////////////////
inline void CheckWorkingDotCommand(cEditorBase &reader, const std::string &text)
{
    CAPTURE(text) ;
    PARAGRAPH_T paragraph = FindParagraph(reader, text) ;
    REQUIRE(paragraph != NOT_SET) ;
    const sParagraphLayout *layout = reader.GetLayout()->GetParagraphLayout(paragraph) ;
    REQUIRE(layout != nullptr) ;
    CHECK(layout->isCommand) ;
    CHECK(reader.GetLayout()->GetParagraphDotStatus(paragraph) == DOT_GOOD) ;
}


/////////////////////////////////////////////////////////////////////////////
/// Runs GenerateTOC() on `source` and returns the written path whose
/// name ends in `extension`, or an empty string.
/////////////////////////////////////////////////////////////////////////////
inline std::string GenerateTOCFile(cEditorBase &editor, const std::vector<std::string> &source,
                                   const std::string &sourcePath, const std::string &extension,
                                   std::vector<std::string> &outputFiles)
{
    SetSource(editor, source) ;
    REQUIRE(cTOCIndexGenerator::GenerateTOC(&editor, sourcePath, outputFiles)) ;
    for (const std::string &path : outputFiles)
    {
        if (path.size() >= extension.size() && path.compare(path.size() - extension.size(), extension.size(), extension) == 0)
        {
            return path ;
        }
    }
    return std::string() ;
}


/////////////////////////////////////////////////////////////////////////////
/// .tc entries starting with a dot are written verbatim in document order
/// and work as dot commands in the generated table of contents.
/////////////////////////////////////////////////////////////////////////////
template <class EDITOR>
void CheckTOCDotEntries(void)
{
    cScratchDir dir("tocdot") ;
    REQUIRE(dir.IsValid()) ;

    EDITOR editor ;
    std::vector<std::string> outputFiles ;
    std::string tocPath = GenerateTOCFile(editor,
        { ".tc .po 1i", ".tc .fo Page #", ".tc Chapter One #", ".pa", ".tc .pa", ".tc Chapter Two #" },
        dir.File("source.ws"), ".TOC", outputFiles) ;
    REQUIRE(tocPath.empty() == false) ;
    CHECK(outputFiles.size() == 1) ;

    EDITOR reader ;
    std::vector<std::string> toc = LoadGenerated(reader, tocPath) ;
    CHECK(Join(toc) == ".po 1i|.fo Page #|Chapter One 1|.pa|Chapter Two 2") ;

    CheckWorkingDotCommand(reader, ".po 1i") ;
    CheckWorkingDotCommand(reader, ".fo Page #") ;
    CheckWorkingDotCommand(reader, ".pa") ;

    const sLineLayout *one = FirstLineOf(reader, "Chapter One 1") ;
    const sLineLayout *two = FirstLineOf(reader, "Chapter Two 2") ;
    REQUIRE(one != nullptr) ;
    REQUIRE(two != nullptr) ;
    CHECK(one->pagenumber == 1) ;
    CHECK(two->pagenumber == 2) ;
}


/////////////////////////////////////////////////////////////////////////////
/// Comment, bare dot, indented, arithmetic and numbered-table dot entries.
/////////////////////////////////////////////////////////////////////////////
template <class EDITOR>
void CheckTOCDotEntryEdgeCases(void)
{
    cScratchDir dir("tocdotedge") ;
    REQUIRE(dir.IsValid()) ;

    EDITOR editor ;
    std::vector<std::string> outputFiles ;
    std::string tocPath = GenerateTOCFile(editor,
        { ".tc ..comment #", ".tc .", ".tc  .indented #", ".tc .po ((8.5-7.8)/2)i",
          ".tc1 .lm 0.5i", ".tc1 Figure #", ".tc2 .pm 0", ".tc2 Table #", ".tc Entry #" },
        dir.File("source.ws"), ".TOC", outputFiles) ;
    REQUIRE(tocPath.empty() == false) ;
    REQUIRE(outputFiles.size() == 3) ;

    std::string t01Path ;
    std::string t02Path ;
    for (const std::string &path : outputFiles)
    {
        if (path.find(".T01") != std::string::npos) t01Path = path ;
        if (path.find(".T02") != std::string::npos) t02Path = path ;
    }
    REQUIRE(t01Path.empty() == false) ;
    REQUIRE(t02Path.empty() == false) ;

    EDITOR reader ;
    std::vector<std::string> toc = LoadGenerated(reader, tocPath) ;
    CHECK(Join(toc) == "..comment #|.| .indented 1|.po ((8.5-7.8)/2)i|Entry 1") ;
    CheckWorkingDotCommand(reader, ".po ((8.5-7.8)/2)i") ;

    PARAGRAPH_T comment = FindParagraph(reader, "..comment #") ;
    REQUIRE(comment != NOT_SET) ;
    const sParagraphLayout *commentLayout = reader.GetLayout()->GetParagraphLayout(comment) ;
    REQUIRE(commentLayout != nullptr) ;
    CHECK(commentLayout->isComment) ;

    PARAGRAPH_T indented = FindParagraph(reader, " .indented 1") ;
    REQUIRE(indented != NOT_SET) ;
    const sParagraphLayout *indentedLayout = reader.GetLayout()->GetParagraphLayout(indented) ;
    REQUIRE(indentedLayout != nullptr) ;
    CHECK_FALSE(indentedLayout->isCommand) ;

    std::vector<std::string> t01 = LoadGenerated(reader, t01Path) ;
    CHECK(Join(t01) == ".lm 0.5i|Figure 1") ;
    CheckWorkingDotCommand(reader, ".lm 0.5i") ;

    std::vector<std::string> t02 = LoadGenerated(reader, t02Path) ;
    CHECK(Join(t02) == ".pm 0|Table 1") ;
    CheckWorkingDotCommand(reader, ".pm 0") ;
}


/////////////////////////////////////////////////////////////////////////////
/// A source whose .tc entries are all dot entries still gets its file.
/////////////////////////////////////////////////////////////////////////////
template <class EDITOR>
void CheckTOCOnlyDotEntries(void)
{
    cScratchDir dir("toconlydot") ;
    REQUIRE(dir.IsValid()) ;

    EDITOR editor ;
    std::vector<std::string> outputFiles ;
    std::string tocPath = GenerateTOCFile(editor, { ".tc .po 1i", "Body text", ".tc .fo Page #" },
                                          dir.File("source.ws"), ".TOC", outputFiles) ;
    REQUIRE(tocPath.empty() == false) ;

    EDITOR reader ;
    CHECK(Join(LoadGenerated(reader, tocPath)) == ".po 1i|.fo Page #") ;
    CheckWorkingDotCommand(reader, ".po 1i") ;
    CheckWorkingDotCommand(reader, ".fo Page #") ;
}


/////////////////////////////////////////////////////////////////////////////
/// Runs GenerateIndex() on `source` and returns the written path.
/////////////////////////////////////////////////////////////////////////////
inline std::string GenerateIndexFile(cEditorBase &editor, const std::vector<std::string> &source,
                                     const std::string &sourcePath)
{
    SetSource(editor, source) ;
    std::string outputFile ;
    REQUIRE(cTOCIndexGenerator::GenerateIndex(&editor, sourcePath, outputFile)) ;
    return outputFile ;
}


/////////////////////////////////////////////////////////////////////////////
/// Dot command status of the paragraph `text` of the loaded document, as
/// a name for test output.
/////////////////////////////////////////////////////////////////////////////
inline std::string DotStatusName(cEditorBase &reader, const std::string &text)
{
    PARAGRAPH_T paragraph = FindParagraph(reader, text) ;
    if (paragraph == NOT_SET)
    {
        return "not found" ;
    }
    const sParagraphLayout *layout = reader.GetLayout()->GetParagraphLayout(paragraph) ;
    std::string result = (layout != nullptr && layout->isCommand) ? "isCommand " : "not a command " ;
    switch (reader.GetLayout()->GetParagraphDotStatus(paragraph))
    {
        case DOT_ERROR:          result += "DOT_ERROR" ; break ;
        case DOT_GOOD:           result += "DOT_GOOD" ; break ;
        case DOT_UNKNOWN:        result += "DOT_UNKNOWN" ; break ;
        case DOT_NOTIMPLEMENTED: result += "DOT_NOTIMPLEMENTED" ; break ;
    }
    return result ;
}


/////////////////////////////////////////////////////////////////////////////
/// Source holding one .tc and one .ix dot entry and nothing else.
/////////////////////////////////////////////////////////////////////////////
inline std::vector<std::string> DotEntrySource(void)
{
    return { ".tc .po 1i", ".ix .lm 5" } ;
}


/////////////////////////////////////////////////////////////////////////////
/// Checks the files generated from DotEntrySource(): each holds its dot
/// entry unchanged as its only line. The reload status of ".lm 5" is
/// reported, not checked.
/////////////////////////////////////////////////////////////////////////////
inline void CheckDotEntryOutputs(cEditorBase &reader, const std::string &tocPath, const std::string &indexPath)
{
    std::vector<std::string> toc = LoadGenerated(reader, tocPath) ;
    REQUIRE(toc.empty() == false) ;
    CHECK(toc[0] == ".po 1i") ;
    CHECK(Join(toc) == ".po 1i") ;
    CheckWorkingDotCommand(reader, ".po 1i") ;
    MESSAGE("TOC lines: " << Join(toc)) ;

    std::vector<std::string> index = LoadGenerated(reader, indexPath) ;
    REQUIRE(index.empty() == false) ;
    CHECK(index[0] == ".lm 5") ;
    CHECK(Join(index) == ".lm 5") ;
    MESSAGE("Index lines: " << Join(index)) ;
    MESSAGE("Reload of \".lm 5\" in the index: " << DotStatusName(reader, ".lm 5")) ;
}


/////////////////////////////////////////////////////////////////////////////
/// The one-source case: a .tc and a .ix dot entry produce both files.
/////////////////////////////////////////////////////////////////////////////
template <class EDITOR>
void CheckDotEntrySource(void)
{
    cScratchDir dir("dotsource") ;
    REQUIRE(dir.IsValid()) ;

    EDITOR editor ;
    std::string sourcePath = dir.File("source.ws") ;
    std::vector<std::string> outputFiles ;
    std::string tocPath = GenerateTOCFile(editor, DotEntrySource(), sourcePath, ".TOC", outputFiles) ;
    REQUIRE(tocPath.empty() == false) ;
    std::string indexPath = GenerateIndexFile(editor, DotEntrySource(), sourcePath) ;
    REQUIRE(indexPath.empty() == false) ;

    EDITOR reader ;
    CheckDotEntryOutputs(reader, tocPath, indexPath) ;
}


/////////////////////////////////////////////////////////////////////////////
/// .ix dot entries come first in document order, unparsed and without a
/// page number; the sorted entries follow and pick up the dot commands.
/////////////////////////////////////////////////////////////////////////////
template <class EDITOR>
void CheckIndexDotEntries(void)
{
    cScratchDir dir("indexdot") ;
    REQUIRE(dir.IsValid()) ;

    EDITOR editor ;
    std::string indexPath = GenerateIndexFile(editor,
        { ".ix Zebra", ".ix .pm 0", ".ix Apple", ".ix .lm 0.5i" }, dir.File("source.ws")) ;

    EDITOR reader ;
    std::vector<std::string> index = LoadGenerated(reader, indexPath) ;
    CHECK(Join(index) == ".pm 0|.lm 0.5i|Apple, 1|Zebra, 1") ;
    CheckWorkingDotCommand(reader, ".pm 0") ;
    CheckWorkingDotCommand(reader, ".lm 0.5i") ;

    const sLineLayout *apple = FirstLineOf(reader, "Apple, 1") ;
    REQUIRE(apple != nullptr) ;

    EDITOR plain ;
    SetSource(plain, { "Apple, 1" }) ;
    const sLineLayout *plainApple = FirstLineOf(plain, "Apple, 1") ;
    REQUIRE(plainApple != nullptr) ;
    CHECK(apple->pagex > plainApple->pagex) ;
}


/////////////////////////////////////////////////////////////////////////////
/// Comment, page break, bare dot, indented, escaped and expression dot
/// entries, duplicates, and ordinary entries merging as before.
/////////////////////////////////////////////////////////////////////////////
template <class EDITOR>
void CheckIndexDotEntryEdgeCases(void)
{
    cScratchDir dir("indexdotedge") ;
    REQUIRE(dir.IsValid()) ;

    EDITOR editor ;
    std::string indexPath = GenerateIndexFile(editor,
        { ".ix ..note, with comma", ".ix .pa", ".ix Beta", ".ix .", ".ix  .indented",
          ".ix .po ((8.5-7.8)/2)i", ".pa", ".ix .fo Page #, part \\+ 1", ".ix .lm 0.5i",
          ".ix .lm 0.5i", ".ix Beta", ".ix Alpha" },
        dir.File("source.ws")) ;

    EDITOR reader ;
    std::vector<std::string> index = LoadGenerated(reader, indexPath) ;
    CHECK(Join(index) == "..note, with comma|.pa|.|.po ((8.5-7.8)/2)i|.fo Page #, part \\+ 1|.lm 0.5i|.lm 0.5i"
                         "| .indented, 1|Alpha, 2|Beta, 1, 2") ;

    PARAGRAPH_T comment = FindParagraph(reader, "..note, with comma") ;
    REQUIRE(comment != NOT_SET) ;
    const sParagraphLayout *commentLayout = reader.GetLayout()->GetParagraphLayout(comment) ;
    REQUIRE(commentLayout != nullptr) ;
    CHECK(commentLayout->isComment) ;

    CheckWorkingDotCommand(reader, ".pa") ;
    CheckWorkingDotCommand(reader, ".po ((8.5-7.8)/2)i") ;
    CheckWorkingDotCommand(reader, ".lm 0.5i") ;

    PARAGRAPH_T indented = FindParagraph(reader, " .indented, 1") ;
    REQUIRE(indented != NOT_SET) ;
    const sParagraphLayout *indentedLayout = reader.GetLayout()->GetParagraphLayout(indented) ;
    REQUIRE(indentedLayout != nullptr) ;
    CHECK_FALSE(indentedLayout->isCommand) ;

    const sLineLayout *first = FirstLineOf(reader, " .indented, 1") ;
    const sLineLayout *beta = FirstLineOf(reader, "Beta, 1, 2") ;
    REQUIRE(first != nullptr) ;
    REQUIRE(beta != nullptr) ;
    CHECK(first->pagenumber == 2) ;
    CHECK(beta->pagenumber == 2) ;
}


/////////////////////////////////////////////////////////////////////////////
/// A source whose .ix entries are all dot entries still gets its index.
/////////////////////////////////////////////////////////////////////////////
template <class EDITOR>
void CheckIndexOnlyDotEntries(void)
{
    cScratchDir dir("indexonlydot") ;
    REQUIRE(dir.IsValid()) ;

    EDITOR editor ;
    std::string indexPath = GenerateIndexFile(editor, { ".ix .pm 0", "Body text", ".ix .lm 0.5i" },
                                              dir.File("source.ws")) ;

    EDITOR reader ;
    CHECK(Join(LoadGenerated(reader, indexPath)) == ".pm 0|.lm 0.5i") ;
    CheckWorkingDotCommand(reader, ".pm 0") ;
    CheckWorkingDotCommand(reader, ".lm 0.5i") ;
}

/////////////////////////////////////////////////////////////////////////////
/// GenerateIndex() on a source built by SetMarkedSource(); returns the
/// written path.
/////////////////////////////////////////////////////////////////////////////
inline std::string GenerateMarkedIndexFile(cEditorBase &editor, const std::vector<std::string> &source,
                                           const std::string &sourcePath)
{
    SetMarkedSource(editor, source) ;
    std::string outputFile ;
    REQUIRE(cTOCIndexGenerator::GenerateIndex(&editor, sourcePath, outputFile)) ;
    return outputFile ;
}


/////////////////////////////////////////////////////////////////////////////
/// A kept sequence inside .tc and .ix entry text is left out of the
/// generated files without leaving a space.
/////////////////////////////////////////////////////////////////////////////
template <class EDITOR>
void CheckSequenceInEntryText(void)
{
    cScratchDir dir("seqentry") ;
    REQUIRE(dir.IsValid()) ;

    EDITOR editor ;
    SetMarkedSource(editor, { ".tc Chapter {S:x}One #", ".tc .lm {S:x}0" }) ;
    std::vector<std::string> outputFiles ;
    REQUIRE(cTOCIndexGenerator::GenerateTOC(&editor, dir.File("source.ws"), outputFiles)) ;
    REQUIRE(outputFiles.size() == 1) ;

    EDITOR reader ;
    CHECK(Join(LoadGenerated(reader, outputFiles[0])) == "Chapter One 1|.lm 0") ;

    std::string indexPath = GenerateMarkedIndexFile(editor, { ".ix .lm {S:x}0", ".ix Al{S:x}pha" },
                                                    dir.File("source.ws")) ;
    CHECK(Join(LoadGenerated(reader, indexPath)) == ".lm 0|Alpha, 1") ;
}

/////////////////////////////////////////////////////////////////////////////
/// Index of a source built by SetMarkedSource(), loaded back.
/////////////////////////////////////////////////////////////////////////////
template <class EDITOR>
std::string MarkedIndex(const std::string &tag, const std::vector<std::string> &source)
{
    cScratchDir dir(tag) ;
    REQUIRE(dir.IsValid()) ;

    EDITOR editor ;
    std::string indexPath = GenerateMarkedIndexFile(editor, source, dir.File("source.ws")) ;

    EDITOR reader ;
    return Join(LoadGenerated(reader, indexPath)) ;
}


/////////////////////////////////////////////////////////////////////////////
/// A marked word on pages 1 and 3 and the same word as .ix on page 2 give
/// one index line.
/////////////////////////////////////////////////////////////////////////////
template <class EDITOR>
void CheckIndexMarksMergeWithIx(void)
{
    CHECK(MarkedIndex<EDITOR>("markmerge",
        { "Page one {K}word{K} text", ".pa", ".ix word", "Page two", ".pa", "Page three {K}word{K}" })
        == "word, 1, 2, 3") ;
}


/////////////////////////////////////////////////////////////////////////////
/// A mark without a partner in its paragraph gives no entry; a third mark
/// opens a new pair; an empty or blank pair gives no entry.
/////////////////////////////////////////////////////////////////////////////
template <class EDITOR>
void CheckUnpairedIndexMarks(void)
{
    CHECK(MarkedIndex<EDITOR>("markunpaired",
        { "One {K}lone mark", "{K}first{K} then {K}third", "Empty {K}{K} and {K}  {K} pairs", "Next {K}line" })
        == "first, 1") ;
}


/////////////////////////////////////////////////////////////////////////////
/// Marks in dot command and comment lines are not index entries.
/////////////////////////////////////////////////////////////////////////////
template <class EDITOR>
void CheckIndexMarksInDotLines(void)
{
    CHECK(MarkedIndex<EDITOR>("markdot",
        { ".h1 {K}Head{K}", ".f1 {K}Foot{K}", ".tc {K}Chapter{K} #", "..{K}note{K}", "Body {K}real{K} text" })
        == "real, 1") ;
}


/////////////////////////////////////////////////////////////////////////////
/// Markers between the index marks are left out of the entry text; the
/// text is taken as it is, without .ix sub-entry rules.
/////////////////////////////////////////////////////////////////////////////
template <class EDITOR>
void CheckIndexMarkPlainText(void)
{
    CHECK(MarkedIndex<EDITOR>("markplain",
        { "A {K}Big {B}bold{B}{T}word{K} b", "{K}-Naming, files+{K}" })
        == "-Naming, files+, 1|Big boldword, 1") ;
}


/////////////////////////////////////////////////////////////////////////////
/// The page of an index mark is the page of the line holding the first
/// mark, not the page the paragraph ends on.
/////////////////////////////////////////////////////////////////////////////
template <class EDITOR>
void CheckIndexMarkPageOfLine(void)
{
    cScratchDir dir("markpage") ;
    REQUIRE(dir.IsValid()) ;

    // a paragraph long enough to run from page 1 onto page 2
    std::string longText ;
    for (int loop = 0 ; loop < 1200 ; loop++)
    {
        longText += "filler " ;
    }
    EDITOR editor ;
    SetMarkedSource(editor, { "{K}start{K} " + longText + "{K}end{K}" }) ;

    const sParagraphLayout *para = editor.GetLayout()->GetParagraphLayout(0) ;
    REQUIRE(para != nullptr) ;
    REQUIRE(para->lines.empty() == false) ;
    PAGE_T first = para->lines.front().pagenumber ;
    PAGE_T last = para->lines.back().pagenumber ;
    REQUIRE(first < last) ;

    std::string indexPath ;
    REQUIRE(cTOCIndexGenerator::GenerateIndex(&editor, dir.File("source.ws"), indexPath)) ;

    EDITOR reader ;
    CHECK(Join(LoadGenerated(reader, indexPath)) ==
          "end, " + std::to_string(last) + "|start, " + std::to_string(first)) ;
}

/////////////////////////////////////////////////////////////////////////////
/// A kept 0Eh index item gives the same index line as .ix with its body,
/// on the page of its line, and merges with that .ix entry.
/////////////////////////////////////////////////////////////////////////////
template <class EDITOR>
void CheckIndexItemSequence(void)
{
    std::string item = MarkedIndex<EDITOR>("itemseq", { "Page one", ".pa", "Text {S:Alpha\\, Beta}here" }) ;
    std::string ix = MarkedIndex<EDITOR>("itemix", { "Page one", ".pa", ".ix Alpha\\, Beta" }) ;
    CHECK(item == ix) ;
    CHECK(item == "Alpha, Beta, 2") ;

    CHECK(MarkedIndex<EDITOR>("itemmerge", { ".ix Alpha\\, Beta", ".pa", "Text {S:Alpha\\, Beta}here" })
          == "Alpha, Beta, 1, 2") ;

    CHECK(MarkedIndex<EDITOR>("itemflags", { "a {S:+Bold}b", "c {S:-See, other}d", "e {S:Sawyer\\, Robert J.}f" })
          == "Bold, 1|Sawyer, Robert J., 1|See, other") ;
}


/////////////////////////////////////////////////////////////////////////////
/// An empty body or one holding control bytes gives no entry; high bytes
/// are read through the editor's code page.
/////////////////////////////////////////////////////////////////////////////
template <class EDITOR>
void CheckIndexItemSequenceBody(void)
{
    CHECK(MarkedIndex<EDITOR>("itembody", { "a {S:}b", "c {S:X\x01Y}d", "e {S:Real}f" }) == "Real, 1") ;

    for (eCodePage page : { CP437, CP1252 })
    {
        CAPTURE(static_cast<int>(page)) ;
        cScratchDir dir("itemcp") ;
        REQUIRE(dir.IsValid()) ;

        EDITOR editor ;
        editor.SetCodePage(page) ;
        std::string body = (page == CP437) ? "M\x81ller" : "M\xFCller" ;
        std::string indexPath = GenerateMarkedIndexFile(editor, { "a {S:" + body + "}b" }, dir.File("source.ws")) ;

        EDITOR reader ;
        reader.SetCodePage(page) ;
        CHECK(Join(LoadGenerated(reader, indexPath)) == "M\xC3\xBCller, 1") ;
    }
}


/////////////////////////////////////////////////////////////////////////////
/// Directory with write permission removed; permission is restored by the
/// destructor so the scratch directory can be removed.
/////////////////////////////////////////////////////////////////////////////
class cLockedDir
{
public:
    explicit cLockedDir(const std::string &path) : mPath(path)
    {
        mValid = (mkdir(mPath.c_str(), 0555) == 0) ;
    }

    ~cLockedDir()
    {
        chmod(mPath.c_str(), 0755) ;
    }

    bool IsValid(void) const { return mValid ; }
    std::string File(const std::string &name) const { return (std::filesystem::path(mPath) / name).string() ; }

private:
    std::string mPath ;
    bool mValid = false ;
};


/////////////////////////////////////////////////////////////////////////////
/// GenerateTOC() and GenerateIndex() next to a source in a directory that
/// cannot be written report the paths they could not write; a source
/// without entries reports none.
/////////////////////////////////////////////////////////////////////////////
template <class EDITOR>
void CheckUnwritableOutput(void)
{
    cScratchDir dir("unwritable") ;
    REQUIRE(dir.IsValid()) ;
    cLockedDir locked(dir.File("locked")) ;
    REQUIRE(locked.IsValid()) ;
    std::string sourcePath = locked.File("source.ws") ;

    EDITOR editor ;
    SetSource(editor, { ".tc Chapter #", ".tc1 Figure #", ".ix Apple" }) ;
    std::vector<std::string> outputFiles ;
    std::vector<std::string> failedFiles ;
    CHECK_FALSE(cTOCIndexGenerator::GenerateTOC(&editor, sourcePath, outputFiles, &failedFiles)) ;
    CHECK(outputFiles.empty()) ;
    CHECK(Join(failedFiles) == locked.File("source.TOC") + "|" + locked.File("source.T01")) ;

    SetSource(editor, { ".tc Chapter #", ".tc1 Figure #", ".ix Apple" }) ;
    std::string outputFile ;
    std::string failedFile ;
    CHECK_FALSE(cTOCIndexGenerator::GenerateIndex(&editor, sourcePath, outputFile, &failedFile)) ;
    CHECK(failedFile == locked.File("source.IDX")) ;

    CHECK(std::filesystem::is_empty(dir.File("locked"))) ;

    SetSource(editor, { "No entries" }) ;
    failedFiles.push_back("stale") ;
    CHECK_FALSE(cTOCIndexGenerator::GenerateTOC(&editor, sourcePath, outputFiles, &failedFiles)) ;
    CHECK(failedFiles.empty()) ;
    failedFile = "stale" ;
    CHECK_FALSE(cTOCIndexGenerator::GenerateIndex(&editor, sourcePath, outputFile, &failedFile)) ;
    CHECK(failedFile.empty()) ;
}


/////////////////////////////////////////////////////////////////////////////
/// Source with .tc, .tc1, .ix, an index mark pair and dot entries, in
/// SetMarkedSource() form, and the files generated from it.
/////////////////////////////////////////////////////////////////////////////
inline std::vector<std::string> GoldenSource(void)
{
    return { ".tc Chapter One #", ".ix Apple", "Some {K}marked{K} words", ".tc1 Figure #", ".tc .lm 0",
             ".pa", ".tc Chapter Two #", ".ix .pm 0", ".ix Zebra" } ;
}

inline const char *GoldenTOC(void)   { return "Chapter One 1|.lm 0|Chapter Two 2" ; }
inline const char *GoldenT01(void)   { return "Figure 1" ; }
inline const char *GoldenIndex(void) { return ".pm 0|Apple, 1|marked, 1|Zebra, 2" ; }


/////////////////////////////////////////////////////////////////////////////
/// Writes GoldenSource() to `path` as a WordStar file.
/////////////////////////////////////////////////////////////////////////////
template <class EDITOR>
void WriteGoldenSource(const std::string &path)
{
    EDITOR editor ;
    SetMarkedSource(editor, GoldenSource()) ;
    cWordstarFile writer(&editor) ;
    REQUIRE(writer.SaveFile(path, editor.GetDocument()->GetTextSize())) ;
}


/////////////////////////////////////////////////////////////////////////////
/// Loads `sourcePath` into a fresh editor for each table, as ws does, and
/// runs GenerateTOC() and GenerateIndex(); returns the written paths.
/////////////////////////////////////////////////////////////////////////////
template <class EDITOR>
std::vector<std::string> GenerateFromDisk(const std::string &sourcePath)
{
    std::vector<std::string> written ;
    {
        EDITOR editor ;
        REQUIRE(editor.LoadFile(sourcePath)) ;
        std::vector<std::string> outputFiles ;
        CHECK(cTOCIndexGenerator::GenerateTOC(&editor, sourcePath, outputFiles)) ;
        written = outputFiles ;
    }
    {
        EDITOR editor ;
        REQUIRE(editor.LoadFile(sourcePath)) ;
        std::string outputFile ;
        CHECK(cTOCIndexGenerator::GenerateIndex(&editor, sourcePath, outputFile)) ;
        written.push_back(outputFile) ;
    }
    return written ;
}


/////////////////////////////////////////////////////////////////////////////
/// Checks the files generated next to `stem` from GoldenSource().
/////////////////////////////////////////////////////////////////////////////
template <class EDITOR>
void CheckGoldenOutputs(const std::string &stem)
{
    EDITOR reader ;
    CHECK(Join(LoadGenerated(reader, stem + ".TOC")) == GoldenTOC()) ;
    CHECK(Join(LoadGenerated(reader, stem + ".T01")) == GoldenT01()) ;
    CHECK(Join(LoadGenerated(reader, stem + ".IDX")) == GoldenIndex()) ;
}


/////////////////////////////////////////////////////////////////////////////
/// Whole file content, empty if it cannot be read.
/////////////////////////////////////////////////////////////////////////////
inline std::string ReadBytes(const std::string &path)
{
    std::ifstream in(path, std::ios::binary) ;
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()) ;
}


/////////////////////////////////////////////////////////////////////////////
/// Names of the files in `directory`, sorted, joined with '|'.
/////////////////////////////////////////////////////////////////////////////
inline std::string DirectoryListing(const std::string &directory)
{
    std::vector<std::string> names ;
    std::error_code ec ;
    for (const auto &entry : std::filesystem::directory_iterator(directory, ec))
    {
        names.push_back(entry.path().filename().string()) ;
    }
    std::sort(names.begin(), names.end()) ;
    return Join(names) ;
}

} // namespace tocindextest

#endif // TOCINDEXGEN_TESTUTIL_H
