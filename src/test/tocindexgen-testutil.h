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
#include <string>
#include <system_error>
#include <unistd.h>
#include <vector>

#include "src/core/document/document.h"
#include "src/core/editor/editorbase.h"
#include "src/core/generate/tocindexgenerator.h"
#include "src/core/include/config.h"
#include "src/core/layout/layoutbase.h"
#include "src/core/layout/layoutstructs.h"

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

} // namespace tocindextest

#endif // TOCINDEXGEN_TESTUTIL_H
