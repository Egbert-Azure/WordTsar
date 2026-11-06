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

#include "tocindexgenerator.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <map>
#include <set>
#include <utility>

#include "unicodelib.h"
#include "unicodelib_encodings.h"

#include "src/core/include/config.h"
#include "src/core/codepage/cp1252.h"
#include "src/core/codepage/cp437.h"
#include "src/core/codepage/cp737.h"
#include "src/core/codepage/cp850.h"
#include "src/core/document/document.h"
#include "src/core/editor/editorbase.h"
#include "src/core/layout/layoutbase.h"
#include "src/core/layout/layoutstructs.h"
#include "src/files/wordstar/wordstarfile.h"


namespace {

/////////////////////////////////////////////////////////////////////////////
// GetParagraphText() includes the paragraph's own terminator; strip it so
// prefix/content checks below see just the dot command and its argument.
/////////////////////////////////////////////////////////////////////////////
std::string TrimTerminator(const std::string &text)
{
    std::string result = text ;
    while (result.empty() == false && (result.back() == '\r' || result.back() == '\n'))
    {
        result.pop_back() ;
    }
    return result ;
}

/////////////////////////////////////////////////////////////////////////////
// Paragraph text without its terminator and without kept sequence markers.
/////////////////////////////////////////////////////////////////////////////
std::string EntryParagraphText(cDocument *doc, PARAGRAPH_T paragraph)
{
    std::string raw = doc->GetParagraphText(paragraph) ;
    if (raw.find(static_cast<char>(MARKER_CHAR)) == std::string::npos)
    {
        return TrimTerminator(raw) ;
    }

    POSITION_T start = 0 ;
    POSITION_T end = 0 ;
    doc->GetParagraphStartandEnd(paragraph, start, end) ;

    std::vector<std::string> graphemes ;
    std::vector<POSITION_T> offsets ;
    doc->GetParagraphGraphemes(paragraph, graphemes, offsets) ;

    std::string text ;
    sRawSequence sequence ;
    for (size_t loop = 0 ; loop < graphemes.size() ; loop++)
    {
        if (graphemes[loop].size() == 1 && graphemes[loop][0] == static_cast<char>(MARKER_CHAR) &&
            doc->GetRawSequence(start + static_cast<POSITION_T>(loop), sequence) == true)
        {
            continue ;
        }
        text += graphemes[loop] ;
    }
    return TrimTerminator(text) ;
}


bool StartsWithNoCase(const std::string &text, const std::string &prefix)
{
    if (text.size() < prefix.size())
    {
        return false ;
    }
    for (size_t loop = 0 ; loop < prefix.size() ; loop++)
    {
        if (std::tolower(static_cast<unsigned char>(text[loop])) != std::tolower(static_cast<unsigned char>(prefix[loop])))
        {
            return false ;
        }
    }
    return true ;
}

std::string ToLowerCopy(const std::string &text)
{
    std::string result = text ;
    for (char &c : result)
    {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c))) ;
    }
    return result ;
}

/////////////////////////////////////////////////////////////////////////////
// Replace an unescaped '#' with the page number, and '\#' with a literal
// '#' -- "Type the # symbol where you want the page number to be printed.
// If you want the # symbol to appear in the entry, precede it with a
// backslash."
/////////////////////////////////////////////////////////////////////////////
std::string SubstitutePageNumber(const std::string &raw, PAGE_T pageNumber)
{
    std::string result ;
    for (size_t pos = 0 ; pos < raw.size() ; pos++)
    {
        if (raw[pos] == '\\' && pos + 1 < raw.size() && raw[pos + 1] == '#')
        {
            result += '#' ;
            pos++ ;
            continue ;
        }
        if (raw[pos] == '#')
        {
            result += std::to_string(pageNumber) ;
            continue ;
        }
        result += raw[pos] ;
    }
    return result ;
}

struct sTOCEntry
{
    std::string text ;
};

struct sIndexEntry
{
    std::string main ;
    std::string sub ;
    bool bold ;
    bool crossref ;
    PAGE_T page ;
};

/////////////////////////////////////////////////////////////////////////////
// Parse a .ix entry's argument text: a leading '+' or '-' flags bold or
// cross-reference (mutually exclusive), an unescaped comma splits
// main/subreference, and \+ \- \, \\ escape their literal characters.
// A cross-reference's remaining text is kept whole rather than split on
// comma -- the manual's own example, "-Naming files, see Files", uses a
// comma as ordinary English, not a subreference marker.
/////////////////////////////////////////////////////////////////////////////
sIndexEntry ParseIndexEntry(const std::string &raw)
{
    sIndexEntry entry ;
    entry.bold = false ;
    entry.crossref = false ;
    entry.page = 0 ;

    size_t pos = 0 ;
    if (pos < raw.size() && raw[pos] == '+')
    {
        entry.bold = true ;
        pos++ ;
    }
    else if (pos < raw.size() && raw[pos] == '-')
    {
        entry.crossref = true ;
        pos++ ;
    }

    std::string main ;
    std::string sub ;
    std::string *target = &main ;
    for ( ; pos < raw.size() ; pos++)
    {
        char c = raw[pos] ;
        if (c == '\\' && pos + 1 < raw.size())
        {
            pos++ ;
            *target += raw[pos] ;
            continue ;
        }
        if (c == ',' && entry.crossref == false && target == &main)
        {
            target = &sub ;
            continue ;
        }
        *target += c ;
    }

    entry.main = main ;
    entry.sub = sub ;
    return entry ;
}


/////////////////////////////////////////////////////////////////////////////
// Page of the line holding paragraph-relative grapheme index `index`.
/////////////////////////////////////////////////////////////////////////////
PAGE_T PageOfIndex(const sParagraphLayout *paraLayout, POSITION_T index)
{
    if (paraLayout == nullptr || paraLayout->lines.empty())
    {
        return 1 ;
    }
    PAGE_T page = paraLayout->lines.front().pagenumber ;
    for (const sLineLayout &line : paraLayout->lines)
    {
        if (line.linestart > index)
        {
            break ;
        }
        page = line.pagenumber ;
    }
    return page ;
}


/////////////////////////////////////////////////////////////////////////////
// A kept index item body in UTF-8, bytes from 0x80 read through the code
// page. False for a body that is empty, holds a control byte or a byte
// the code page does not map.
/////////////////////////////////////////////////////////////////////////////
bool DecodeIndexItemBody(const std::string &body, eCodePage codepage, std::string &text)
{
    text.clear() ;
    if (body.empty())
    {
        return false ;
    }

    cCodePage437 cp437 ;
    cCodePage737 cp737 ;
    cCodePage850 cp850 ;
    cCodePageWin1252 cp1252 ;
    for (char c : body)
    {
        unsigned char byte = static_cast<unsigned char>(c) ;
        if (byte < 0x20 || byte == 0x7F)
        {
            return false ;
        }

        unsigned long cp = byte ;
        if (byte >= 0x80)
        {
            switch (codepage)
            {
                case CP437 :  cp = cp437.toUTF8(byte) ;  break ;
                case CP737 :  cp = cp737.toUTF8(byte) ;  break ;
                case CP850 :  cp = cp850.toUTF8(byte) ;  break ;
                case CP1252 : cp = cp1252.toUTF8(byte) ; break ;
            }
            if (cp == UINT32_MAX)
            {
                return false ;
            }
        }
        unicode::utf8::encode_codepoint(static_cast<char32_t>(cp), text) ;
    }
    return true ;
}


/////////////////////////////////////////////////////////////////////////////
// Index entries from one paragraph: ^K index mark pairs and kept 0Eh index
// items. Marks pair up in order; a mark left without a partner is ignored.
// The text between a pair, without markers, is the entry as it stands. A
// pair holding no visible text gives no entry. An index item's body is
// read as a .ix entry. The page is the page of the line holding the first
// mark or the index item.
/////////////////////////////////////////////////////////////////////////////
void CollectIndexMarks(cDocument *doc, const sParagraphLayout *paraLayout, PARAGRAPH_T paragraph,
                       eCodePage codepage, std::vector<sIndexEntry> &entries)
{
    std::string raw = doc->GetParagraphText(paragraph) ;
    if (raw.find(static_cast<char>(MARKER_CHAR)) == std::string::npos)
    {
        return ;
    }

    POSITION_T start = 0 ;
    POSITION_T end = 0 ;
    doc->GetParagraphStartandEnd(paragraph, start, end) ;

    std::vector<std::string> graphemes ;
    std::vector<POSITION_T> offsets ;
    doc->GetParagraphGraphemes(paragraph, graphemes, offsets) ;

    bool open = false ;
    POSITION_T openIndex = 0 ;
    std::string text ;
    for (size_t loop = 0 ; loop < graphemes.size() ; loop++)
    {
        POSITION_T index = static_cast<POSITION_T>(loop) ;
        bool isMarker = graphemes[loop].size() == 1 && graphemes[loop][0] == static_cast<char>(MARKER_CHAR) ;
        if (isMarker == true)
        {
            sRawSequence sequence ;
            if (doc->GetRawSequence(start + index, sequence) == true && sequence.type == 0x0E)
            {
                std::string body ;
                if (DecodeIndexItemBody(sequence.body, codepage, body) == true)
                {
                    sIndexEntry entry = ParseIndexEntry(body) ;
                    if (entry.main.empty() == false)
                    {
                        if (entry.crossref == false)
                        {
                            entry.page = PageOfIndex(paraLayout, index) ;
                        }
                        entries.push_back(entry) ;
                    }
                }
                continue ;
            }
            if (doc->GetControlChar(start + index) != STYLE_INDEX)
            {
                continue ;
            }
            if (open == false)
            {
                open = true ;
                openIndex = index ;
                text.clear() ;
                continue ;
            }

            open = false ;
            bool visible = std::any_of(text.begin(), text.end(), [](char c)
            {
                return std::isspace(static_cast<unsigned char>(c)) == 0 ;
            }) ;
            if (visible == true)
            {
                sIndexEntry entry ;
                entry.main = text ;
                entry.bold = false ;
                entry.crossref = false ;
                entry.page = PageOfIndex(paraLayout, openIndex) ;
                entries.push_back(entry) ;
            }
            continue ;
        }

        if (open == true && graphemes[loop] != "\r" && graphemes[loop] != "\n")
        {
            text += graphemes[loop] ;
        }
    }
}

} // namespace


/////////////////////////////////////////////////////////////////////////////
bool cTOCIndexGenerator::GenerateTOC(cEditorBase *editor, const std::string &sourcePath, std::vector<std::string> &outputFiles,
                                     std::vector<std::string> *failedFiles)
{
    outputFiles.clear() ;
    if (failedFiles != nullptr)
    {
        failedFiles->clear() ;
    }

    cDocument *sourceDoc = editor->GetDocument() ;
    cLayoutBase *layout = editor->GetLayout() ;

    std::vector<sTOCEntry> tables[10] ;    // [0] = plain .tc, [1-9] = .tc1-.tc9

    PARAGRAPH_T count = sourceDoc->GetNumberofParagraphs() ;
    for (PARAGRAPH_T loop = 0 ; loop < count ; loop++)
    {
        std::string text = EntryParagraphText(sourceDoc, loop) ;
        if (StartsWithNoCase(text, ".tc") == false)
        {
            continue ;
        }

        size_t prefixLen = 3 ;
        int table = 0 ;
        if (prefixLen < text.size() && std::isdigit(static_cast<unsigned char>(text[prefixLen])))
        {
            table = text[prefixLen] - '0' ;
            prefixLen++ ;
        }

        // the one mandatory separator space between the command and the
        // entry text does not itself count as indentation
        if (prefixLen < text.size() && text[prefixLen] == ' ')
        {
            prefixLen++ ;
        }
        if (prefixLen >= text.size())
        {
            continue ;    // bare .tc/.tcN with no entry text -- nothing to add
        }

        const sParagraphLayout *paraLayout = layout->GetParagraphLayout(loop) ;
        PAGE_T page = (paraLayout != nullptr) ? paraLayout->endPage : 1 ;

        // an entry starting with '.' is a dot command for the generated
        // file and is written unchanged
        sTOCEntry entry ;
        entry.text = text.substr(prefixLen) ;
        if (entry.text[0] != '.')
        {
            entry.text = SubstitutePageNumber(entry.text, page) ;
        }
        tables[table].push_back(entry) ;
    }

    std::filesystem::path source(sourcePath) ;
    std::filesystem::path stem = source.parent_path() / source.stem() ;

    bool wroteAny = false ;
    for (int table = 0 ; table <= 9 ; table++)
    {
        if (tables[table].empty())
        {
            continue ;
        }

        sourceDoc->Clear() ;
        for (const sTOCEntry &entry : tables[table])
        {
            sourceDoc->Insert(entry.text) ;
            sourceDoc->Insert(HARD_RETURN) ;
        }

        char extbuf[8] ;
        if (table == 0)
        {
            snprintf(extbuf, sizeof(extbuf), ".TOC") ;
        }
        else
        {
            snprintf(extbuf, sizeof(extbuf), ".T%02d", table) ;
        }
        std::string outputPath = stem.string() + extbuf ;

        cWordstarFile writer(editor) ;
        if (writer.SaveFile(outputPath, sourceDoc->GetTextSize()) == true)
        {
            outputFiles.push_back(outputPath) ;
            wroteAny = true ;
        }
        else if (failedFiles != nullptr)
        {
            failedFiles->push_back(outputPath) ;
        }
    }

    return wroteAny ;
}


/////////////////////////////////////////////////////////////////////////////
bool cTOCIndexGenerator::GenerateIndex(cEditorBase *editor, const std::string &sourcePath, std::string &outputFile,
                                       std::string *failedFile)
{
    outputFile.clear() ;
    if (failedFile != nullptr)
    {
        failedFile->clear() ;
    }

    cDocument *sourceDoc = editor->GetDocument() ;
    cLayoutBase *layout = editor->GetLayout() ;

    std::vector<sIndexEntry> entries ;
    std::vector<std::string> dotCommands ;

    PARAGRAPH_T count = sourceDoc->GetNumberofParagraphs() ;
    for (PARAGRAPH_T loop = 0 ; loop < count ; loop++)
    {
        std::string text = EntryParagraphText(sourceDoc, loop) ;
        if (StartsWithNoCase(text, ".ix") == false)
        {
            // dot command and comment lines hold no index marks
            const sParagraphLayout *paraLayout = layout->GetParagraphLayout(loop) ;
            bool dotLine = (text.empty() == false && text[0] == '.') ||
                           (paraLayout != nullptr && (paraLayout->isCommand || paraLayout->isComment)) ;
            if (dotLine == false)
            {
                CollectIndexMarks(sourceDoc, paraLayout, loop, editor->GetCodePage(), entries) ;
            }
            continue ;
        }

        size_t prefixLen = 3 ;
        if (prefixLen < text.size() && text[prefixLen] == ' ')
        {
            prefixLen++ ;
        }
        if (prefixLen > text.size())
        {
            continue ;
        }

        // an entry starting with '.' is a dot command for the generated
        // file: kept unchanged, in document order, ahead of the entries
        if (prefixLen < text.size() && text[prefixLen] == '.')
        {
            dotCommands.push_back(text.substr(prefixLen)) ;
            continue ;
        }

        sIndexEntry entry = ParseIndexEntry(text.substr(prefixLen)) ;
        if (entry.main.empty())
        {
            continue ;
        }

        if (entry.crossref == false)
        {
            const sParagraphLayout *paraLayout = layout->GetParagraphLayout(loop) ;
            entry.page = (paraLayout != nullptr) ? paraLayout->endPage : 1 ;
        }

        entries.push_back(entry) ;
    }

    if (entries.empty() && dotCommands.empty())
    {
        return false ;
    }

    // Group by (lowercased main, lowercased sub, crossref) -- duplicate
    // entries combine into one merged, sorted page list; the same page
    // marked both bold and non-bold keeps only the bold form.
    struct sGroup
    {
        std::string mainDisplay ;
        std::string subDisplay ;
        bool crossref ;
        std::set<PAGE_T> normalPages ;
        std::set<PAGE_T> boldPages ;
    };

    std::map<std::string, sGroup> groups ;

    for (const sIndexEntry &entry : entries)
    {
        std::string key = ToLowerCopy(entry.main) + "\x01" + ToLowerCopy(entry.sub) + "\x01" + (entry.crossref ? "1" : "0") ;

        auto it = groups.find(key) ;
        if (it == groups.end())
        {
            sGroup group ;
            group.mainDisplay = entry.main ;
            group.subDisplay = entry.sub ;
            group.crossref = entry.crossref ;
            it = groups.emplace(key, group).first ;
        }

        if (entry.crossref == false)
        {
            if (entry.bold == true)
            {
                it->second.boldPages.insert(entry.page) ;
                it->second.normalPages.erase(entry.page) ;
            }
            else if (it->second.boldPages.count(entry.page) == 0)
            {
                it->second.normalPages.insert(entry.page) ;
            }
        }
    }

    std::vector<sGroup> sorted ;
    for (auto &pair : groups)
    {
        sorted.push_back(pair.second) ;
    }
    std::sort(sorted.begin(), sorted.end(), [](const sGroup &a, const sGroup &b)
    {
        std::string am = ToLowerCopy(a.mainDisplay) ;
        std::string bm = ToLowerCopy(b.mainDisplay) ;
        if (am != bm)
        {
            return am < bm ;
        }
        return ToLowerCopy(a.subDisplay) < ToLowerCopy(b.subDisplay) ;
    }) ;

    sourceDoc->Clear() ;
    for (const std::string &command : dotCommands)
    {
        sourceDoc->Insert(command) ;
        sourceDoc->Insert(HARD_RETURN) ;
    }
    for (const sGroup &group : sorted)
    {
        // A subreference is displayed indented two spaces under its main
        // entry -- real usage nearly always marks the main entry on its own
        // too, so it gets its own line above; a bare subreference with no
        // plain-main sibling just shows indented on its own, which is a
        // reasonable degradation of a case the manual doesn't spell out.
        std::string line = group.subDisplay.empty() ? group.mainDisplay : ("  " + group.subDisplay) ;
        sourceDoc->Insert(line) ;

        if (group.crossref == false)
        {
            std::vector<std::pair<PAGE_T, bool>> combined ;
            for (PAGE_T page : group.normalPages)
            {
                combined.push_back(std::make_pair(page, false)) ;
            }
            for (PAGE_T page : group.boldPages)
            {
                combined.push_back(std::make_pair(page, true)) ;
            }
            std::sort(combined.begin(), combined.end()) ;

            if (combined.empty() == false)
            {
                sourceDoc->Insert(", ") ;
                bool boldOn = false ;
                for (size_t loop = 0 ; loop < combined.size() ; loop++)
                {
                    if (loop > 0)
                    {
                        sourceDoc->Insert(", ") ;
                    }
                    if (combined[loop].second != boldOn)
                    {
                        sourceDoc->BeginBold() ;
                        boldOn = combined[loop].second ;
                    }
                    sourceDoc->Insert(std::to_string(combined[loop].first)) ;
                }
                if (boldOn == true)
                {
                    sourceDoc->BeginBold() ;
                }
            }
        }

        sourceDoc->Insert(HARD_RETURN) ;
    }

    std::filesystem::path source(sourcePath) ;
    std::filesystem::path stem = source.parent_path() / source.stem() ;
    outputFile = stem.string() + ".IDX" ;

    cWordstarFile writer(editor) ;
    bool written = writer.SaveFile(outputFile, sourceDoc->GetTextSize()) ;
    if (written == false && failedFile != nullptr)
    {
        *failedFile = outputFile ;
    }
    return written ;
}
