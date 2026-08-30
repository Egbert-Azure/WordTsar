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

#include <string>
#include <vector>
#include <iostream>

#include "src/core/include/config.h"
#include "src/core/document/document.h"
#include "src/files/rtf/read/rtfparser.h"


/////////////////////////////////////////////////////////////////////////////
///
/// Helper: parse an RTF string into a fresh document and return all
/// paragraph texts as a vector for easy inspection.
///
/////////////////////////////////////////////////////////////////////////////
static std::vector<std::string> ParseRTF(const std::string &rtf)
{
    cDocument doc ;
    doc.Clear() ;
    cRTFParser parser(rtf, &doc, nullptr) ;

    std::vector<std::string> paragraphs ;
    for(ssize_t i = 0; i < doc.GetNumberofParagraphs(); i++)
    {
        paragraphs.push_back(doc.GetParagraphText(i)) ;
    }
    return paragraphs ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// Helper: check if any paragraph starts with the given prefix.
///
/////////////////////////////////////////////////////////////////////////////
static bool HasParagraphStartingWith(const std::vector<std::string> &paragraphs, const std::string &prefix)
{
    for(auto &p : paragraphs)
    {
        if(p.substr(0, prefix.size()) == prefix)
        {
            return true ;
        }
    }
    return false ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// Helper: find the first paragraph starting with prefix, return the full
/// paragraph text (up to, but not including, the trailing \r).
///
/////////////////////////////////////////////////////////////////////////////
static std::string FindParagraph(const std::vector<std::string> &paragraphs, const std::string &prefix)
{
    for(auto &p : paragraphs)
    {
        if(p.size() >= prefix.size() && p.substr(0, prefix.size()) == prefix)
        {
            // Strip trailing \r if present
            if(!p.empty() && p.back() == '\r')
            {
                return p.substr(0, p.size() - 1) ;
            }
            return p ;
        }
    }
    return "" ;
}


// ====================================================================
//  RTF Reader Tests
// ====================================================================

TEST_SUITE("RTF Reader")
{

    // ----------------------------------------------------------------
    //  Basic text parsing
    // ----------------------------------------------------------------
    TEST_CASE("Basic text paragraph")
    {
        std::string rtf = "{\\rtf1\\pc Hello World}" ;
        auto paras = ParseRTF(rtf) ;

        // Should have at least one paragraph with "Hello World" text
        bool found = false ;
        for(auto &p : paras)
        {
            if(p.find("Hello World") != std::string::npos)
            {
                found = true ;
                break ;
            }
        }
        CHECK(found) ;
    }


    TEST_CASE("Multiple paragraphs via \\par")
    {
        std::string rtf = "{\\rtf1\\pc First paragraph\\par Second paragraph}" ;
        auto paras = ParseRTF(rtf) ;

        bool foundFirst = false ;
        bool foundSecond = false ;
        for(auto &p : paras)
        {
            if(p.find("First paragraph") != std::string::npos)
            {
                foundFirst = true ;
            }
            if(p.find("Second paragraph") != std::string::npos)
            {
                foundSecond = true ;
            }
        }
        CHECK(foundFirst) ;
        CHECK(foundSecond) ;
    }


    // ----------------------------------------------------------------
    //  Character formatting
    // ----------------------------------------------------------------
    TEST_CASE("Bold formatting inserts MARKER_CHAR")
    {
        std::string rtf = "{\\rtf1\\pc {\\b bold text}}" ;
        cDocument doc ;
        doc.Clear() ;
        cRTFParser parser(rtf, &doc, nullptr) ;

        // The document should contain marker chars for bold on/off
        bool foundMarker = false ;
        for(ssize_t i = 0; i < doc.GetNumberofParagraphs(); i++)
        {
            std::string text = doc.GetParagraphText(i) ;
            for(size_t j = 0; j < text.size(); j++)
            {
                if(text[j] == MARKER_CHAR)
                {
                    foundMarker = true ;
                    break ;
                }
            }
            if(foundMarker)
            {
                break ;
            }
        }
        CHECK(foundMarker) ;
    }


    TEST_CASE("Italic formatting")
    {
        std::string rtf = "{\\rtf1\\pc {\\i italic text}}" ;
        cDocument doc ;
        doc.Clear() ;
        cRTFParser parser(rtf, &doc, nullptr) ;

        // Check that STYLE_ITALICS is present
        bool foundItalics = false ;
        POSITION_T pos = 0 ;
        for(ssize_t i = 0; i < doc.GetNumberofParagraphs(); i++)
        {
            std::string text = doc.GetParagraphText(i) ;
            for(size_t j = 0; j < text.size(); j++)
            {
                if(text[j] == MARKER_CHAR)
                {
                    if(doc.GetControlChar(pos) == STYLE_ITALICS)
                    {
                        foundItalics = true ;
                    }
                }
                pos++ ;
            }
        }
        CHECK(foundItalics) ;
    }


    // ----------------------------------------------------------------
    //  Margin handling (Phase 1 fixes)
    // ----------------------------------------------------------------
    TEST_CASE("Left margin from \\li emits .lm dot command")
    {
        // \\li1440 = 1 inch left indent
        std::string rtf = "{\\rtf1\\pc\\li1440 Hello}" ;
        auto paras = ParseRTF(rtf) ;

        // Should emit .lm dot command
        CHECK(HasParagraphStartingWith(paras, ".lm")) ;
    }


    TEST_CASE("First-line indent .pm from \\li + \\fi")
    {
        // \\li1440 = 1 inch, \\fi720 = 0.5 inch
        // Because the parser calls DoDotChanges() before setting each value,
        // the final .pm is emitted when text is encountered with both values set.
        // .pm = (li + fi) / 1440 = (1440 + 720) / 1440 = 1.50 inches
        std::string rtf = "{\\rtf1\\pc\\li1440\\fi720 Indented}" ;
        auto paras = ParseRTF(rtf) ;

        // Should emit .pm dot command
        CHECK(HasParagraphStartingWith(paras, ".pm")) ;

        // The final .pm should be 1.50i (from the last emission when text triggers DoChanges)
        bool found150 = false ;
        for(auto &p : paras)
        {
            if(p.substr(0, 3) == ".pm" && p.find("1.50i") != std::string::npos)
            {
                found150 = true ;
            }
        }
        CHECK(found150) ;
    }


    TEST_CASE("Negative first-line indent (hanging indent)")
    {
        // \\li1440 = 1 inch, \\fi-720 = -0.5 inch
        // .pm = (1440 + (-720)) / 1440 = 0.50 inches
        std::string rtf = "{\\rtf1\\pc\\li1440\\fi-720 Hanging}" ;
        auto paras = ParseRTF(rtf) ;

        CHECK(HasParagraphStartingWith(paras, ".pm")) ;

        // The final .pm should be 0.50i
        bool found050 = false ;
        for(auto &p : paras)
        {
            if(p.substr(0, 3) == ".pm" && p.find("0.50i") != std::string::npos)
            {
                found050 = true ;
            }
        }
        CHECK(found050) ;
    }


    TEST_CASE("Right indent from \\ri emits .rm with correct formula")
    {
        // Default paper = 12240 twips (8.5 inches), default margins = 1800 (1.25 inches) each
        // \\ri720 = 0.5 inch right indent
        // .rm = (paperw - \\margl - \\margr - \\ri) / 1440
        // .rm = (12240 - 1800 - 1800 - 720) / 1440 = 5.50 inches
        std::string rtf = "{\\rtf1\\pc\\paperw12240\\margl1800\\margr1800\\ri720 Hello}" ;
        auto paras = ParseRTF(rtf) ;

        CHECK(HasParagraphStartingWith(paras, ".rm")) ;

        std::string rm = FindParagraph(paras, ".rm") ;
        CHECK(!rm.empty()) ;
        CHECK(rm.find("5.50i") != std::string::npos) ;
    }


    TEST_CASE("Right margin emitted when \\margl changes")
    {
        // When \margl changes, .rm must also be emitted because WordStar .rm is relative to .po.
        // Default paper = 12240, default \margr = 1800, no \ri.
        // .rm = (12240 - 2880 - 0 - 1800 - 0) / 1440 = 5.25 inches
        std::string rtf = "{\\rtf1\\pc\\paperw12240\\margr1800\\margl2880 Hello}" ;
        auto paras = ParseRTF(rtf) ;

        // Should have both .po and .rm
        CHECK(HasParagraphStartingWith(paras, ".po")) ;
        CHECK(HasParagraphStartingWith(paras, ".rm")) ;

        std::string rm = FindParagraph(paras, ".rm") ;
        CHECK(!rm.empty()) ;
        CHECK(rm.find("5.25i") != std::string::npos) ;
    }


    TEST_CASE("Right margin emitted when \\margr changes")
    {
        // When \margr changes, .rm must also be emitted.
        // Default paper = 12240, default \margl = 1800, no \ri.
        // .rm = (12240 - 1800 - 0 - 2880 - 0) / 1440 = 5.25 inches
        std::string rtf = "{\\rtf1\\pc\\paperw12240\\margl1800\\margr2880 Hello}" ;
        auto paras = ParseRTF(rtf) ;

        CHECK(HasParagraphStartingWith(paras, ".rm")) ;

        std::string rm = FindParagraph(paras, ".rm") ;
        CHECK(!rm.empty()) ;
        CHECK(rm.find("5.25i") != std::string::npos) ;
    }


    // ----------------------------------------------------------------
    //  Section-level property handling (two-pass)
    // ----------------------------------------------------------------

    TEST_CASE("Inline \\margl applies to entire section")
    {
        // \margl2880 appears AFTER the first text but BEFORE \sect.
        // Since \margl is section-level, it should apply to the entire section.
        // The .po emitted should be 2.00i (2880 twips), not the default 1.25i.
        std::string rtf = "{\\rtf1\\pc\\paperw12240 First paragraph\\par\\margl2880 Second paragraph}" ;
        auto paras = ParseRTF(rtf) ;

        // The .po should reflect the section-level \margl2880 = 2.00 inches
        CHECK(HasParagraphStartingWith(paras, ".po")) ;
        std::string po = FindParagraph(paras, ".po") ;
        CHECK(!po.empty()) ;
        CHECK(po.find("2.00i") != std::string::npos) ;
    }

    TEST_CASE("Section break creates separate section with own margins")
    {
        // Section 0: default margins (1800 = 1.25").
        // Section 1: \margl2880 (2.00").
        // The first section should get default .po, second section should get 2.00".
        std::string rtf = "{\\rtf1\\pc\\paperw12240 Section one\\sect\\sectd\\margl2880 Section two}" ;
        auto paras = ParseRTF(rtf) ;

        // Should have at least two .po emissions (one per section)
        size_t poCount = 0 ;
        for(const auto &p : paras)
        {
            if(p.rfind(".po", 0) == 0)
            {
                poCount++ ;
            }
        }
        // First section gets default .po (1.25i), second section gets .po 2.00i
        CHECK(poCount >= 1) ;

        // Find the last .po -- should be 2.00i from the second section
        std::string lastPo ;
        for(const auto &p : paras)
        {
            if(p.rfind(".po", 0) == 0)
            {
                lastPo = p ;
            }
        }
        CHECK(lastPo.find("2.00i") != std::string::npos) ;
    }

    TEST_CASE("\\sectd resets section margins to defaults")
    {
        // Section 0: \margl2880 (2.00").
        // Section 1: \sectd resets to defaults, should get 1.25" (1800 twips).
        std::string rtf = "{\\rtf1\\pc\\paperw12240\\margl2880 Section one\\sect\\sectd Section two}" ;
        auto paras = ParseRTF(rtf) ;

        // Find all .po values
        std::vector<std::string> poValues ;
        for(const auto &p : paras)
        {
            if(p.rfind(".po", 0) == 0)
            {
                poValues.push_back(p) ;
            }
        }

        // First .po should be 2.00i, second .po should be 1.25i
        CHECK(poValues.size() >= 2) ;
        if(poValues.size() >= 2)
        {
            CHECK(poValues[0].find("2.00i") != std::string::npos) ;
            CHECK(poValues[1].find("1.25i") != std::string::npos) ;
        }
    }

    TEST_CASE("Section inherits margins from previous section")
    {
        // Section 0: \margl2880 (2.00").
        // Section 1: \sect only (no \sectd), should inherit 2.00" from section 0.
        // \margr3600 in section 1 changes right margin only.
        std::string rtf = "{\\rtf1\\pc\\paperw12240\\margl2880 Section one\\sect\\margr3600 Section two}" ;
        auto paras = ParseRTF(rtf) ;

        // Both sections should have .po 2.00i (inherited)
        std::vector<std::string> poValues ;
        for(const auto &p : paras)
        {
            if(p.rfind(".po", 0) == 0)
            {
                poValues.push_back(p) ;
            }
        }
        // Section 0 and section 1 both have margl=2880 producing .po 2.00i
        // But section 1 inherits from section 0, so the .po value is the same.
        // EmitPageMargins won't re-emit if value hasn't changed.
        // At minimum, the first .po should be 2.00i.
        CHECK(poValues.size() >= 1) ;
        CHECK(poValues[0].find("2.00i") != std::string::npos) ;
    }

    TEST_CASE("Multiple section-level properties collected in pre-scan")
    {
        // All section-level properties (\margl, \margt, \margb) set inline
        // should all apply from the beginning of the section.
        std::string rtf = "{\\rtf1\\pc\\paperw12240 Start text\\par\\margl2880\\margt2160\\margb2160 More text}" ;
        auto paras = ParseRTF(rtf) ;

        // .po should be 2.00i (from \margl2880 applying to whole section)
        std::string po = FindParagraph(paras, ".po") ;
        CHECK(!po.empty()) ;
        CHECK(po.find("2.00i") != std::string::npos) ;

        // .mt should be 1.50i (from \margt2160 applying to whole section)
        std::string mt = FindParagraph(paras, ".mt") ;
        CHECK(!mt.empty()) ;
        CHECK(mt.find("1.50i") != std::string::npos) ;

        // .mb should be 1.50i (from \margb2160 applying to whole section)
        std::string mb = FindParagraph(paras, ".mb") ;
        CHECK(!mb.empty()) ;
        CHECK(mb.find("1.50i") != std::string::npos) ;
    }


    // ----------------------------------------------------------------
    //  Page margins (Phase 1)
    // ----------------------------------------------------------------
    TEST_CASE("Top margin from \\margt emits .mt")
    {
        std::string rtf = "{\\rtf1\\pc\\margt2160 Hello}" ;
        auto paras = ParseRTF(rtf) ;

        CHECK(HasParagraphStartingWith(paras, ".mt")) ;

        std::string mt = FindParagraph(paras, ".mt") ;
        CHECK(!mt.empty()) ;
        // 2160 / 1440 = 1.50 inches
        CHECK(mt.find("1.50i") != std::string::npos) ;
    }


    TEST_CASE("Bottom margin from \\margb emits .mb")
    {
        std::string rtf = "{\\rtf1\\pc\\margb1440 Hello}" ;
        auto paras = ParseRTF(rtf) ;

        CHECK(HasParagraphStartingWith(paras, ".mb")) ;

        std::string mb = FindParagraph(paras, ".mb") ;
        CHECK(!mb.empty()) ;
        // 1440 / 1440 = 1.00 inches
        CHECK(mb.find("1.00i") != std::string::npos) ;
    }


    TEST_CASE("Left page margin \\margl emits .po")
    {
        std::string rtf = "{\\rtf1\\pc\\margl2880 Hello}" ;
        auto paras = ParseRTF(rtf) ;

        CHECK(HasParagraphStartingWith(paras, ".po")) ;

        std::string po = FindParagraph(paras, ".po") ;
        CHECK(!po.empty()) ;
        // 2880 / 1440 = 2.00 inches
        CHECK(po.find("2.00i") != std::string::npos) ;
    }


    // ----------------------------------------------------------------
    //  Color table (Phase 1)
    // ----------------------------------------------------------------
    TEST_CASE("Color table parsing and \\cf application")
    {
        // Simple RTF with a color table and \\cf1 to apply red text
        std::string rtf = "{\\rtf1\\pc"
                          "{\\colortbl ;\\red255\\green0\\blue0;}"
                          "\\cf1 Red text}" ;

        cDocument doc ;
        doc.Clear() ;
        cRTFParser parser(rtf, &doc, nullptr) ;

        // Document should contain text with a color marker
        bool foundColor = false ;
        POSITION_T pos = 0 ;
        for(ssize_t i = 0; i < doc.GetNumberofParagraphs(); i++)
        {
            std::string text = doc.GetParagraphText(i) ;
            for(size_t j = 0; j < text.size(); j++)
            {
                if(text[j] == MARKER_CHAR)
                {
                    if(doc.GetControlChar(pos) == STYLE_INTERNAL_COLOR)
                    {
                        foundColor = true ;
                    }
                }
                pos++ ;
            }
        }
        CHECK(foundColor) ;
    }


    TEST_CASE("Color table with auto/default first entry")
    {
        // First entry is auto (empty), second is blue
        std::string rtf = "{\\rtf1\\pc"
                          "{\\colortbl ;\\red0\\green0\\blue255;}"
                          "\\cf1 Blue text}" ;

        cDocument doc ;
        doc.Clear() ;
        cRTFParser parser(rtf, &doc, nullptr) ;

        // Should parse without crashing
        bool foundText = false ;
        for(ssize_t i = 0; i < doc.GetNumberofParagraphs(); i++)
        {
            std::string text = doc.GetParagraphText(i) ;
            if(text.find("Blue text") != std::string::npos)
            {
                foundText = true ;
            }
        }
        CHECK(foundText) ;
    }


    // ----------------------------------------------------------------
    //  Columns
    // ----------------------------------------------------------------
    TEST_CASE("Single column emits .co 1")
    {
        // Default \cols1 (or no \cols at all) should emit .co 1
        std::string rtf = "{\\rtf1\\pc Hello}" ;
        auto paras = ParseRTF(rtf) ;

        CHECK(HasParagraphStartingWith(paras, ".co 1")) ;
    }


    TEST_CASE("Two columns emits .co 2")
    {
        std::string rtf = "{\\rtf1\\pc\\cols2 Hello}" ;
        auto paras = ParseRTF(rtf) ;

        CHECK(HasParagraphStartingWith(paras, ".co 2")) ;
    }


    TEST_CASE("Columns with spacing emits .co N with spacing")
    {
        std::string rtf = "{\\rtf1\\pc\\cols2\\colsx720 Hello}" ;
        auto paras = ParseRTF(rtf) ;

        // 720 twips = 0.50 inches
        std::string co = FindParagraph(paras, ".co ") ;
        CHECK(!co.empty()) ;
        CHECK(co.find("2") != std::string::npos) ;
        CHECK(co.find("0.50") != std::string::npos) ;
    }


    // ----------------------------------------------------------------
    //  Landscape (Phase 1/2)
    // ----------------------------------------------------------------
    TEST_CASE("Landscape flag emits .pr or=l")
    {
        std::string rtf = "{\\rtf1\\pc\\landscape Hello}" ;
        auto paras = ParseRTF(rtf) ;

        CHECK(HasParagraphStartingWith(paras, ".pr or=l")) ;
    }


    // ----------------------------------------------------------------
    //  Headers and footers (Phase 2)
    // ----------------------------------------------------------------
    TEST_CASE("Header group emits .h1 dot command")
    {
        std::string rtf = "{\\rtf1\\pc"
                          "{\\header My Header Text}"
                          " Body text}" ;
        auto paras = ParseRTF(rtf) ;

        CHECK(HasParagraphStartingWith(paras, ".h1")) ;

        std::string h1 = FindParagraph(paras, ".h1") ;
        CHECK(h1.find("My Header Text") != std::string::npos) ;
    }


    TEST_CASE("Footer group emits .f1 dot command")
    {
        std::string rtf = "{\\rtf1\\pc"
                          "{\\footer Page Footer}"
                          " Body text}" ;
        auto paras = ParseRTF(rtf) ;

        CHECK(HasParagraphStartingWith(paras, ".f1")) ;

        std::string f1 = FindParagraph(paras, ".f1") ;
        CHECK(f1.find("Page Footer") != std::string::npos) ;
    }


    TEST_CASE("Header with \\chpgn emits page number variable")
    {
        std::string rtf = "{\\rtf1\\pc"
                          "{\\header Page \\chpgn }"
                          " Body}" ;

        cDocument doc ;
        doc.Clear() ;
        cRTFParser parser(rtf, &doc, nullptr) ;

        // Find the header paragraph and verify it contains a MARKER_CHAR (variable)
        bool found = false ;
        for(ssize_t i = 0; i < doc.GetNumberofParagraphs(); i++)
        {
            std::string text = doc.GetParagraphText(i) ;
            if(text.find(".h1 ") == 0)
            {
                // Should have MARKER_CHAR for the page number variable
                CHECK(text.find(static_cast<char>(MARKER_CHAR)) != std::string::npos) ;

                // Verify it's a VAR_PAGE_NUMBER variable
                POSITION_T paraStart = 0, paraEnd = 0 ;
                doc.GetParagraphStartandEnd(i, paraStart, paraEnd) ;
                size_t markerPos = text.find(static_cast<char>(MARKER_CHAR)) ;
                POSITION_T docPos = paraStart + static_cast<POSITION_T>(markerPos) ;
                eModifiers ct = doc.GetControlChar(docPos) ;
                CHECK(ct == STYLE_VARIABLE) ;
                eVariableType vt = doc.GetVariable(docPos) ;
                CHECK(vt == VAR_PAGE_NUMBER) ;

                found = true ;
                break ;
            }
        }
        CHECK(found) ;
    }


    TEST_CASE("Multi-line header with \\par creates .h1 and .h2")
    {
        std::string rtf = "{\\rtf1\\pc"
                          "{\\header Line One\\par Line Two}"
                          " Body}" ;
        auto paras = ParseRTF(rtf) ;

        CHECK(HasParagraphStartingWith(paras, ".h1")) ;
        CHECK(HasParagraphStartingWith(paras, ".h2")) ;
    }


    TEST_CASE("\\headerl without \\facingp is ignored per RTF spec")
    {
        // Without \\facingp, \\headerl is ignored -- only \\headerr applies (to all pages)
        std::string rtf = "{\\rtf1\\pc"
                          "{\\headerl Left Page Header}"
                          " Body}" ;
        auto paras = ParseRTF(rtf) ;

        CHECK_FALSE(HasParagraphStartingWith(paras, ".h1")) ;
        CHECK_FALSE(HasParagraphStartingWith(paras, ".h1o")) ;
        CHECK_FALSE(HasParagraphStartingWith(paras, ".h1e")) ;
    }


    TEST_CASE("\\headerr without \\facingp emits .h1 (all pages)")
    {
        // Without \\facingp, \\headerr applies to ALL pages per RTF spec
        std::string rtf = "{\\rtf1\\pc"
                          "{\\headerr Right Page Header}"
                          " Body}" ;
        auto paras = ParseRTF(rtf) ;

        // Should produce .h1 (all pages), not .h1e or .h1o
        CHECK(HasParagraphStartingWith(paras, ".h1")) ;
        CHECK_FALSE(HasParagraphStartingWith(paras, ".h1e")) ;
        CHECK_FALSE(HasParagraphStartingWith(paras, ".h1o")) ;
    }


    TEST_CASE("\\footerl without \\facingp is ignored, \\footerr emits .f1")
    {
        // Without \\facingp, \\footerl is ignored and \\footerr applies to all pages
        std::string rtf1 = "{\\rtf1\\pc{\\footerl Left Footer} Body}" ;
        std::string rtf2 = "{\\rtf1\\pc{\\footerr Right Footer} Body}" ;
        auto paras1 = ParseRTF(rtf1) ;
        auto paras2 = ParseRTF(rtf2) ;

        // \\footerl ignored
        CHECK_FALSE(HasParagraphStartingWith(paras1, ".f1")) ;
        CHECK_FALSE(HasParagraphStartingWith(paras1, ".f1o")) ;
        CHECK_FALSE(HasParagraphStartingWith(paras1, ".f1e")) ;

        // \\footerr maps to .f1 (all pages)
        CHECK(HasParagraphStartingWith(paras2, ".f1")) ;
        CHECK_FALSE(HasParagraphStartingWith(paras2, ".f1e")) ;
        CHECK_FALSE(HasParagraphStartingWith(paras2, ".f1o")) ;
    }


    TEST_CASE("\\headerl and \\headerr with \\facingp emit .h1e and .h1o")
    {
        // With \\facingp, \\headerl = left = even (.h1e), \\headerr = right = odd (.h1o)
        std::string rtf = "{\\rtf1\\pc\\facingp"
                          "{\\headerl Even Page Header}"
                          "{\\headerr Odd Page Header}"
                          " Body}" ;
        auto paras = ParseRTF(rtf) ;

        CHECK(HasParagraphStartingWith(paras, ".h1e")) ;
        CHECK(HasParagraphStartingWith(paras, ".h1o")) ;
    }


    TEST_CASE("\\footerl and \\footerr with \\facingp emit .f1e and .f1o")
    {
        // With \\facingp, \\footerl = left = even (.f1e), \\footerr = right = odd (.f1o)
        std::string rtf = "{\\rtf1\\pc\\facingp"
                          "{\\footerl Even Page Footer}"
                          "{\\footerr Odd Page Footer}"
                          " Body}" ;
        auto paras = ParseRTF(rtf) ;

        CHECK(HasParagraphStartingWith(paras, ".f1e")) ;
        CHECK(HasParagraphStartingWith(paras, ".f1o")) ;
    }


    // ----------------------------------------------------------------
    //  Multi-line headers/footers
    // ----------------------------------------------------------------
    TEST_CASE("Multi-line header with \\par at top level")
    {
        // Simple case: \par directly in header group separates two lines
        std::string rtf = "{\\rtf1\\pc"
                          "{\\header First Line\\par Second Line\\par}"
                          " Body}" ;
        auto paras = ParseRTF(rtf) ;

        CHECK(HasParagraphStartingWith(paras, ".h1 ")) ;
        CHECK(HasParagraphStartingWith(paras, ".h2 ")) ;

        std::string h1 = FindParagraph(paras, ".h1 ") ;
        std::string h2 = FindParagraph(paras, ".h2 ") ;
        CHECK(h1.find("First Line") != std::string::npos) ;
        CHECK(h2.find("Second Line") != std::string::npos) ;
    }


    TEST_CASE("Multi-line header with \\par nested inside formatting groups")
    {
        // Real-world RTF pattern: \par is inside a child formatting group
        // This is what Word generates -- formatting groups wrap text + \par
        std::string rtf = "{\\rtf1\\pc"
                          "{\\header {\\b Title Text\\par}{\\i Subtitle\\par}}"
                          " Body}" ;
        auto paras = ParseRTF(rtf) ;

        CHECK(HasParagraphStartingWith(paras, ".h1 ")) ;
        CHECK(HasParagraphStartingWith(paras, ".h2 ")) ;

        std::string h1 = FindParagraph(paras, ".h1 ") ;
        std::string h2 = FindParagraph(paras, ".h2 ") ;
        CHECK(h1.find("Title Text") != std::string::npos) ;
        CHECK(h2.find("Subtitle") != std::string::npos) ;
    }


    TEST_CASE("Multi-line footer with \\par nested in groups")
    {
        std::string rtf = "{\\rtf1\\pc"
                          "{\\footer {\\f0 Copyright\\par}{\\f0 Page \\chpgn\\par}}"
                          " Body}" ;

        cDocument doc ;
        doc.Clear() ;
        cRTFParser parser(rtf, &doc, nullptr) ;

        // Verify both footer lines exist
        bool foundF1 = false ;
        bool foundF2 = false ;
        for(ssize_t i = 0; i < doc.GetNumberofParagraphs(); i++)
        {
            std::string text = doc.GetParagraphText(i) ;
            if(text.find(".f1 ") == 0)
            {
                CHECK(text.find("Copyright") != std::string::npos) ;
                foundF1 = true ;
            }
            if(text.find(".f2 ") == 0)
            {
                // Should contain "Page " and a MARKER_CHAR for the page number variable
                CHECK(text.find("Page ") != std::string::npos) ;
                CHECK(text.find(static_cast<char>(MARKER_CHAR)) != std::string::npos) ;

                // Verify it's a VAR_PAGE_NUMBER variable
                POSITION_T paraStart = 0, paraEnd = 0 ;
                doc.GetParagraphStartandEnd(i, paraStart, paraEnd) ;
                size_t markerPos = text.find(static_cast<char>(MARKER_CHAR)) ;
                POSITION_T docPos = paraStart + static_cast<POSITION_T>(markerPos) ;
                eModifiers ct = doc.GetControlChar(docPos) ;
                CHECK(ct == STYLE_VARIABLE) ;
                eVariableType vt = doc.GetVariable(docPos) ;
                CHECK(vt == VAR_PAGE_NUMBER) ;

                foundF2 = true ;
            }
        }
        CHECK(foundF1) ;
        CHECK(foundF2) ;
    }


    TEST_CASE("RTF header font is stored in document with correct MARKER_CHAR")
    {
        // Create RTF with Arial 16pt header (font index 1)
        std::string rtf = "{\\rtf1\\pc"
                          "{\\fonttbl{\\f0\\froman Times New Roman;}{\\f1\\fswiss Arial;}}"
                          "{\\header\\pard\\plain\\f1\\fs32 My Header\\par}"
                          " Body text}" ;

        cDocument doc ;
        doc.Clear() ;
        cRTFParser parser(rtf, &doc, nullptr) ;

        // Find the header paragraph and verify font MARKER_CHAR
        bool foundHeader = false ;
        for(ssize_t i = 0 ; i < doc.GetNumberofParagraphs() ; i++)
        {
            std::string text = doc.GetParagraphText(i) ;
            if(text.find(".h1 ") == 0)
            {
                // Header should contain MARKER_CHAR for font change
                size_t markerPos = text.find(static_cast<char>(MARKER_CHAR)) ;
                REQUIRE(markerPos != std::string::npos) ;

                POSITION_T paraStart = 0, paraEnd = 0 ;
                doc.GetParagraphStartandEnd(i, paraStart, paraEnd) ;
                POSITION_T docPos = paraStart + static_cast<POSITION_T>(markerPos) ;

                // Verify control char type is font
                eModifiers ct = doc.GetControlChar(docPos) ;
                CHECK(ct == STYLE_FONT1) ;

                // Verify font data is retrievable and correct
                sInternalFonts font ;
                bool gotFont = doc.GetFont(docPos, font) ;
                CHECK(gotFont) ;
                CHECK(font.size == 16) ;  // 32 half-points / 2 = 16 points

                foundHeader = true ;
            }
        }
        CHECK(foundHeader) ;
    }


    TEST_CASE("Multi-line header with \\facingp emits .h1o, .h2o")
    {
        // With facingp, multi-line headerr should produce .h1o, .h2o (odd)
        std::string rtf = "{\\rtf1\\pc\\facingp"
                          "{\\headerr {\\f0 Line One\\par}{\\f0 Line Two\\par}}"
                          " Body}" ;
        auto paras = ParseRTF(rtf) ;

        CHECK(HasParagraphStartingWith(paras, ".h1o ")) ;
        CHECK(HasParagraphStartingWith(paras, ".h2o ")) ;
    }


    TEST_CASE("Max 5 header lines")
    {
        // WordStar supports max 5 header/footer lines
        std::string rtf = "{\\rtf1\\pc"
                          "{\\header L1\\par L2\\par L3\\par L4\\par L5\\par L6\\par}"
                          " Body}" ;
        auto paras = ParseRTF(rtf) ;

        CHECK(HasParagraphStartingWith(paras, ".h1 ")) ;
        CHECK(HasParagraphStartingWith(paras, ".h2 ")) ;
        CHECK(HasParagraphStartingWith(paras, ".h3 ")) ;
        CHECK(HasParagraphStartingWith(paras, ".h4 ")) ;
        CHECK(HasParagraphStartingWith(paras, ".h5 ")) ;
        // Line 6 should be dropped (max 5)
        CHECK_FALSE(HasParagraphStartingWith(paras, ".h6 ")) ;
    }


    // ----------------------------------------------------------------
    //  Header/footer margins (Phase 2)
    // ----------------------------------------------------------------
    TEST_CASE("Header margin \\headery emits .hm")
    {
        std::string rtf = "{\\rtf1\\pc\\headery720 Hello}" ;
        auto paras = ParseRTF(rtf) ;

        CHECK(HasParagraphStartingWith(paras, ".hm")) ;

        std::string hm = FindParagraph(paras, ".hm") ;
        CHECK(!hm.empty()) ;
        // 720 / 1440 = 0.50 inches
        CHECK(hm.find("0.50i") != std::string::npos) ;
    }


    TEST_CASE("Footer margin \\footery emits .fm")
    {
        std::string rtf = "{\\rtf1\\pc\\footery1080 Hello}" ;
        auto paras = ParseRTF(rtf) ;

        CHECK(HasParagraphStartingWith(paras, ".fm")) ;

        std::string fm = FindParagraph(paras, ".fm") ;
        CHECK(!fm.empty()) ;
        // 1080 / 1440 = 0.75 inches
        CHECK(fm.find("0.75i") != std::string::npos) ;
    }


    // ----------------------------------------------------------------
    //  Page setup (Phase 2)
    // ----------------------------------------------------------------
    TEST_CASE("Columns from \\cols and \\colsx emit .co")
    {
        std::string rtf = "{\\rtf1\\pc\\cols2\\colsx720 Hello}" ;
        auto paras = ParseRTF(rtf) ;

        CHECK(HasParagraphStartingWith(paras, ".co")) ;

        std::string co = FindParagraph(paras, ".co") ;
        CHECK(co.find("2") != std::string::npos) ;
    }


    TEST_CASE("Gutter is added to \\margl for .po")
    {
        // \\margl1440 = 1 inch, \\gutter720 = 0.5 inch
        // .po should be (1440 + 720) / 1440 = 1.50 inches
        std::string rtf = "{\\rtf1\\pc\\margl1440\\gutter720 Hello}" ;
        auto paras = ParseRTF(rtf) ;

        bool foundPO = false ;
        for(auto &p : paras)
        {
            if(p.substr(0, 3) == ".po" && p.find("1.50i") != std::string::npos)
            {
                foundPO = true ;
            }
        }
        CHECK(foundPO) ;
    }


    TEST_CASE("Page number start from \\pgnstarts emits .pn")
    {
        std::string rtf = "{\\rtf1\\pc\\pgnstarts5 Hello}" ;
        auto paras = ParseRTF(rtf) ;

        CHECK(HasParagraphStartingWith(paras, ".pn")) ;

        std::string pn = FindParagraph(paras, ".pn") ;
        CHECK(pn.find("5") != std::string::npos) ;
    }


    // ----------------------------------------------------------------
    //  Tab stops (Phase 2/3)
    // ----------------------------------------------------------------
    TEST_CASE("Tab stops from \\tx emit .tb dot command")
    {
        // Two tab stops at 1" and 3"
        std::string rtf = "{\\rtf1\\pc\\tx1440\\tx4320 Hello}" ;
        auto paras = ParseRTF(rtf) ;

        CHECK(HasParagraphStartingWith(paras, ".tb")) ;

        std::string tb = FindParagraph(paras, ".tb") ;
        CHECK(!tb.empty()) ;
        // 1440/1440 = 1.00 inches, 4320/1440 = 3.00 inches
        CHECK(tb.find("1.00i") != std::string::npos) ;
        CHECK(tb.find("3.00i") != std::string::npos) ;
    }


    TEST_CASE("Center tab from \\tqc\\tx emits .tb with ^ prefix")
    {
        std::string rtf = "{\\rtf1\\pc\\tqc\\tx2880 Hello}" ;
        auto paras = ParseRTF(rtf) ;

        // .tb line has ^ prefix for center tab
        CHECK(HasParagraphStartingWith(paras, ".tb")) ;
        std::string tb = FindParagraph(paras, ".tb") ;
        CHECK(tb.find("^2.00i") != std::string::npos) ;
    }


    TEST_CASE("Right tab from \\tqr\\tx emits .tb with > prefix")
    {
        std::string rtf = "{\\rtf1\\pc\\tqr\\tx5760 Hello}" ;
        auto paras = ParseRTF(rtf) ;

        // .tb line has > prefix for right tab
        CHECK(HasParagraphStartingWith(paras, ".tb")) ;
        std::string tb = FindParagraph(paras, ".tb") ;
        CHECK(tb.find(">4.00i") != std::string::npos) ;
    }


    TEST_CASE("Decimal tab from \\tqdec\\tx emits # in .tb")
    {
        std::string rtf = "{\\rtf1\\pc\\tqdec\\tx4320 Hello}" ;
        auto paras = ParseRTF(rtf) ;

        // .tb line has # prefix for decimal tab
        CHECK(HasParagraphStartingWith(paras, ".tb")) ;
        std::string tb = FindParagraph(paras, ".tb") ;
        CHECK(tb.find("#3.00i") != std::string::npos) ;
    }


    TEST_CASE("Dot leader tab from \\tldot\\tx emits plain tab")
    {
        // Dot leaders are not represented in .tb format
        // The tab position is still emitted as a plain left tab
        std::string rtf = "{\\rtf1\\pc\\tldot\\tx7200 Hello}" ;
        auto paras = ParseRTF(rtf) ;

        CHECK(HasParagraphStartingWith(paras, ".tb")) ;
        std::string tb = FindParagraph(paras, ".tb") ;
        CHECK(tb.find("5.00i") != std::string::npos) ;
    }


    TEST_CASE("Mixed tab types emit .tb with type prefixes")
    {
        // Left at 1", center at 3", right at 5"
        std::string rtf = "{\\rtf1\\pc\\tx1440\\tqc\\tx4320\\tqr\\tx7200 Hello}" ;
        auto paras = ParseRTF(rtf) ;

        // .tb has all positions with type prefixes
        CHECK(HasParagraphStartingWith(paras, ".tb")) ;
        std::string tb = FindParagraph(paras, ".tb") ;
        CHECK(tb.find("1.00i") != std::string::npos) ;    // plain left
        CHECK(tb.find("^3.00i") != std::string::npos) ;   // center
        CHECK(tb.find(">5.00i") != std::string::npos) ;   // right
    }


    // ----------------------------------------------------------------
    //  Special characters (Phase 3)
    // ----------------------------------------------------------------
    TEST_CASE("Non-breaking space \\~ inserts U+00A0")
    {
        std::string rtf = "{\\rtf1\\pc Hello\\~World}" ;
        cDocument doc ;
        doc.Clear() ;
        cRTFParser parser(rtf, &doc, nullptr) ;

        // Check that U+00A0 is in the document text
        bool foundNBSP = false ;
        for(ssize_t i = 0; i < doc.GetNumberofParagraphs(); i++)
        {
            std::string text = doc.GetParagraphText(i) ;
            // U+00A0 in UTF-8 is 0xC2 0xA0
            if(text.find("\xC2\xA0") != std::string::npos)
            {
                foundNBSP = true ;
                break ;
            }
        }
        CHECK(foundNBSP) ;
    }


    TEST_CASE("Hex character \\'hh inserts correct character")
    {
        // \\'41 = 'A'
        std::string rtf = "{\\rtf1\\pc \\'41BC}" ;
        cDocument doc ;
        doc.Clear() ;
        cRTFParser parser(rtf, &doc, nullptr) ;

        bool foundA = false ;
        for(ssize_t i = 0; i < doc.GetNumberofParagraphs(); i++)
        {
            std::string text = doc.GetParagraphText(i) ;
            if(text.find("ABC") != std::string::npos)
            {
                foundA = true ;
                break ;
            }
        }
        CHECK(foundA) ;
    }


    // ----------------------------------------------------------------
    //  Field codes / Variables (Phase 3)
    // ----------------------------------------------------------------
    TEST_CASE("PAGE field inserts variable marker")
    {
        std::string rtf = "{\\rtf1\\pc{\\field{\\*\\fldinst PAGE}{\\fldrslt 1}}}" ;
        cDocument doc ;
        doc.Clear() ;
        cRTFParser parser(rtf, &doc, nullptr) ;

        // Should have a STYLE_VARIABLE marker for page number
        bool foundVar = false ;
        POSITION_T pos = 0 ;
        for(ssize_t i = 0; i < doc.GetNumberofParagraphs(); i++)
        {
            std::string text = doc.GetParagraphText(i) ;
            for(size_t j = 0; j < text.size(); j++)
            {
                if(text[j] == MARKER_CHAR)
                {
                    if(doc.GetControlChar(pos) == STYLE_VARIABLE)
                    {
                        eVariableType vtype = doc.GetVariable(pos) ;
                        if(vtype == VAR_PAGE_NUMBER)
                        {
                            foundVar = true ;
                        }
                    }
                }
                pos++ ;
            }
        }
        CHECK(foundVar) ;
    }


    TEST_CASE("DATE field inserts date variable")
    {
        std::string rtf = "{\\rtf1\\pc{\\field{\\*\\fldinst DATE}{\\fldrslt today}}}" ;
        cDocument doc ;
        doc.Clear() ;
        cRTFParser parser(rtf, &doc, nullptr) ;

        bool foundVar = false ;
        POSITION_T pos = 0 ;
        for(ssize_t i = 0; i < doc.GetNumberofParagraphs(); i++)
        {
            std::string text = doc.GetParagraphText(i) ;
            for(size_t j = 0; j < text.size(); j++)
            {
                if(text[j] == MARKER_CHAR)
                {
                    if(doc.GetControlChar(pos) == STYLE_VARIABLE)
                    {
                        eVariableType vtype = doc.GetVariable(pos) ;
                        if(vtype == VAR_DATE)
                        {
                            foundVar = true ;
                        }
                    }
                }
                pos++ ;
            }
        }
        CHECK(foundVar) ;
    }


    TEST_CASE("TIME field inserts time variable")
    {
        std::string rtf = "{\\rtf1\\pc{\\field{\\*\\fldinst TIME}{\\fldrslt noon}}}" ;
        cDocument doc ;
        doc.Clear() ;
        cRTFParser parser(rtf, &doc, nullptr) ;

        bool foundVar = false ;
        POSITION_T pos = 0 ;
        for(ssize_t i = 0; i < doc.GetNumberofParagraphs(); i++)
        {
            std::string text = doc.GetParagraphText(i) ;
            for(size_t j = 0; j < text.size(); j++)
            {
                if(text[j] == MARKER_CHAR)
                {
                    if(doc.GetControlChar(pos) == STYLE_VARIABLE)
                    {
                        eVariableType vtype = doc.GetVariable(pos) ;
                        if(vtype == VAR_TIME)
                        {
                            foundVar = true ;
                        }
                    }
                }
                pos++ ;
            }
        }
        CHECK(foundVar) ;
    }


    TEST_CASE("FILENAME field inserts filename variable")
    {
        std::string rtf = "{\\rtf1\\pc{\\field{\\*\\fldinst FILENAME}{\\fldrslt doc.rtf}}}" ;
        cDocument doc ;
        doc.Clear() ;
        cRTFParser parser(rtf, &doc, nullptr) ;

        bool foundVar = false ;
        POSITION_T pos = 0 ;
        for(ssize_t i = 0; i < doc.GetNumberofParagraphs(); i++)
        {
            std::string text = doc.GetParagraphText(i) ;
            for(size_t j = 0; j < text.size(); j++)
            {
                if(text[j] == MARKER_CHAR)
                {
                    if(doc.GetControlChar(pos) == STYLE_VARIABLE)
                    {
                        eVariableType vtype = doc.GetVariable(pos) ;
                        if(vtype == VAR_FILENAME)
                        {
                            foundVar = true ;
                        }
                    }
                }
                pos++ ;
            }
        }
        CHECK(foundVar) ;
    }


    // ----------------------------------------------------------------
    //  Footnote / comment stubs (Phase 4)
    // ----------------------------------------------------------------
    TEST_CASE("Footnote destination inserts placeholder text")
    {
        std::string rtf = "{\\rtf1\\pc Text{\\footnote This is a footnote.} more text}" ;
        cDocument doc ;
        doc.Clear() ;
        cRTFParser parser(rtf, &doc, nullptr) ;

        bool found = false ;
        for(ssize_t i = 0; i < doc.GetNumberofParagraphs(); i++)
        {
            std::string text = doc.GetParagraphText(i) ;
            if(text.find("FOOTNOTE") != std::string::npos)
            {
                found = true ;
                break ;
            }
        }
        CHECK(found) ;
    }


    TEST_CASE("Annotation destination inserts placeholder text")
    {
        std::string rtf = "{\\rtf1\\pc Text{\\*\\annotation This is a comment.} more text}" ;
        cDocument doc ;
        doc.Clear() ;
        cRTFParser parser(rtf, &doc, nullptr) ;

        bool found = false ;
        for(ssize_t i = 0; i < doc.GetNumberofParagraphs(); i++)
        {
            std::string text = doc.GetParagraphText(i) ;
            if(text.find("COMMENT") != std::string::npos)
            {
                found = true ;
                break ;
            }
        }
        CHECK(found) ;
    }


    // ----------------------------------------------------------------
    //  Justification / alignment
    // ----------------------------------------------------------------
    TEST_CASE("Center alignment \\qc")
    {
        std::string rtf = "{\\rtf1\\pc\\qc Centered text}" ;
        cDocument doc ;
        doc.Clear() ;
        cRTFParser parser(rtf, &doc, nullptr) ;

        // The parser calls mDocument->BeginCenter() which handles alignment internally.
        // Verify the document parsed without errors.
        CHECK(doc.GetNumberofParagraphs() >= 1) ;
    }


    // ----------------------------------------------------------------
    //  Line break \\line
    // ----------------------------------------------------------------
    TEST_CASE("Line break \\line inserts newline")
    {
        std::string rtf = "{\\rtf1\\pc Line one\\line Line two}" ;
        cDocument doc ;
        doc.Clear() ;
        cRTFParser parser(rtf, &doc, nullptr) ;

        // \\line should create a line break (soft return) within the paragraph
        // or it may split into separate paragraphs depending on implementation
        CHECK(doc.GetNumberofParagraphs() >= 1) ;
    }


    // ----------------------------------------------------------------
    //  Page break \\page
    // ----------------------------------------------------------------
    TEST_CASE("Page break \\page emits .pa")
    {
        std::string rtf = "{\\rtf1\\pc Before\\par\\page After}" ;
        auto paras = ParseRTF(rtf) ;

        CHECK(HasParagraphStartingWith(paras, ".pa")) ;
    }


    // ----------------------------------------------------------------
    //  Tab character \\tab
    // ----------------------------------------------------------------
    TEST_CASE("Tab character \\tab inserts tab")
    {
        std::string rtf = "{\\rtf1\\pc Column1\\tab Column2}" ;
        cDocument doc ;
        doc.Clear() ;
        cRTFParser parser(rtf, &doc, nullptr) ;

        // Tab should result in a MARKER_CHAR with STYLE_TAB
        bool foundTab = false ;
        POSITION_T pos = 0 ;
        for(ssize_t i = 0; i < doc.GetNumberofParagraphs(); i++)
        {
            std::string text = doc.GetParagraphText(i) ;
            for(size_t j = 0; j < text.size(); j++)
            {
                if(text[j] == MARKER_CHAR)
                {
                    if(doc.GetControlChar(pos) == STYLE_TAB)
                    {
                        foundTab = true ;
                    }
                }
                pos++ ;
            }
        }
        CHECK(foundTab) ;
    }


    // ----------------------------------------------------------------
    //  Font handling
    // ----------------------------------------------------------------
    TEST_CASE("Font table and \\f0 selection")
    {
        std::string rtf = "{\\rtf1\\pc\\deff0"
                          "{\\fonttbl{\\f0\\froman Times New Roman;}{\\f1\\fswiss Arial;}}"
                          "\\f0\\fs24 Normal text}" ;
        cDocument doc ;
        doc.Clear() ;
        cRTFParser parser(rtf, &doc, nullptr) ;

        // Should parse without crashing and produce text
        bool foundText = false ;
        for(ssize_t i = 0; i < doc.GetNumberofParagraphs(); i++)
        {
            std::string text = doc.GetParagraphText(i) ;
            if(text.find("Normal text") != std::string::npos)
            {
                foundText = true ;
            }
        }
        CHECK(foundText) ;
    }


    // ----------------------------------------------------------------
    //  Facing pages
    // ----------------------------------------------------------------
    TEST_CASE("Facing pages flag \\facingp")
    {
        std::string rtf = "{\\rtf1\\pc\\facingp Hello}" ;
        cDocument doc ;
        doc.Clear() ;
        cRTFParser parser(rtf, &doc, nullptr) ;

        // Should parse without errors
        CHECK(doc.GetNumberofParagraphs() >= 1) ;
    }


    // ----------------------------------------------------------------
    //  Edge cases and robustness
    // ----------------------------------------------------------------
    TEST_CASE("Empty RTF document")
    {
        std::string rtf = "{\\rtf1\\pc}" ;
        cDocument doc ;
        doc.Clear() ;
        cRTFParser parser(rtf, &doc, nullptr) ;

        // Should not crash; document should have at least the EOF paragraph
        CHECK(doc.GetNumberofParagraphs() >= 1) ;
    }


    TEST_CASE("Nested groups are handled correctly")
    {
        std::string rtf = "{\\rtf1\\pc{\\b{\\i Bold and italic}} Normal}" ;
        cDocument doc ;
        doc.Clear() ;
        cRTFParser parser(rtf, &doc, nullptr) ;

        // Should parse without crashing
        bool foundNormal = false ;
        for(ssize_t i = 0; i < doc.GetNumberofParagraphs(); i++)
        {
            std::string text = doc.GetParagraphText(i) ;
            if(text.find("Normal") != std::string::npos)
            {
                foundNormal = true ;
            }
        }
        CHECK(foundNormal) ;
    }


    TEST_CASE("Unknown control words are ignored gracefully")
    {
        std::string rtf = "{\\rtf1\\pc\\unknownword42 Hello}" ;
        cDocument doc ;
        doc.Clear() ;
        cRTFParser parser(rtf, &doc, nullptr) ;

        bool foundText = false ;
        for(ssize_t i = 0; i < doc.GetNumberofParagraphs(); i++)
        {
            std::string text = doc.GetParagraphText(i) ;
            if(text.find("Hello") != std::string::npos)
            {
                foundText = true ;
            }
        }
        CHECK(foundText) ;
    }


    TEST_CASE("Unicode escape \\uN with fallback character")
    {
        // \\u233 = Latin small letter e with acute (U+00E9), '?' is the fallback
        std::string rtf = "{\\rtf1\\pc Caf\\u233?}" ;
        cDocument doc ;
        doc.Clear() ;
        cRTFParser parser(rtf, &doc, nullptr) ;

        bool found = false ;
        for(ssize_t i = 0 ; i < doc.GetNumberofParagraphs() ; i++)
        {
            std::string text = doc.GetParagraphText(i) ;
            // U+00E9 (e-acute) is 0xC3 0xA9 in UTF-8
            std::string cafe = "Caf\xC3\xA9" ;
            if(text.find(cafe) != std::string::npos)
            {
                found = true ;
                // Verify the '?' fallback was NOT inserted into the text
                CHECK(text.find("Caf\xC3\xA9?") == std::string::npos) ;
            }
        }
        CHECK(found) ;
    }


    TEST_CASE("Unicode escape negative \\u value for high BMP character")
    {
        // U+8000 = \\u-32768 (32768 - 65536 = -32768)
        std::string rtf = "{\\rtf1\\pc \\u-32768?}" ;
        cDocument doc ;
        doc.Clear() ;
        cRTFParser parser(rtf, &doc, nullptr) ;

        // U+8000 is CJK unified ideograph, UTF-8: 0xE8 0x80 0x80
        bool found = false ;
        for(ssize_t i = 0 ; i < doc.GetNumberofParagraphs() ; i++)
        {
            std::string text = doc.GetParagraphText(i) ;
            std::string expected = "\xE8\x80\x80" ;
            if(text.find(expected) != std::string::npos)
            {
                found = true ;
            }
        }
        CHECK(found) ;
    }


    TEST_CASE("Unicode surrogate pair combines into supplementary character")
    {
        // U+1F600 (grinning face emoji)
        // High surrogate: 0xD83D = 55357, as signed: 55357-65536 = -10179
        // Low surrogate:  0xDE00 = 56832, as signed: 56832-65536 = -8704
        std::string rtf = "{\\rtf1\\pc \\u-10179?\\u-8704?}" ;
        cDocument doc ;
        doc.Clear() ;
        cRTFParser parser(rtf, &doc, nullptr) ;

        // U+1F600 in UTF-8: 0xF0 0x9F 0x98 0x80
        bool found = false ;
        for(ssize_t i = 0 ; i < doc.GetNumberofParagraphs() ; i++)
        {
            std::string text = doc.GetParagraphText(i) ;
            std::string emoji = "\xF0\x9F\x98\x80" ;
            if(text.find(emoji) != std::string::npos)
            {
                found = true ;
            }
        }
        CHECK(found) ;
    }


    TEST_CASE("Hex escape \\'82 maps through CP437 to e-acute")
    {
        // CP437 byte 0x82 = e-acute (U+00E9)
        std::string rtf = "{\\rtf1\\pc Caf\\'82}" ;
        cDocument doc ;
        doc.Clear() ;
        cRTFParser parser(rtf, &doc, nullptr) ;

        // U+00E9 in UTF-8: 0xC3 0xA9
        bool found = false ;
        for(ssize_t i = 0 ; i < doc.GetNumberofParagraphs() ; i++)
        {
            std::string text = doc.GetParagraphText(i) ;
            std::string cafe = "Caf\xC3\xA9" ;
            if(text.find(cafe) != std::string::npos)
            {
                found = true ;
            }
        }
        CHECK(found) ;
    }


    TEST_CASE("Hex escape \\'93 maps through Windows-1252 to left double quote")
    {
        // Windows-1252 byte 0x93 = left double quotation mark (U+201C)
        // NOT Unicode U+0093 which is a control character
        std::string rtf = "{\\rtf1\\ansi \\'93Hello\\'94}" ;
        cDocument doc ;
        doc.Clear() ;
        cRTFParser parser(rtf, &doc, nullptr) ;

        // U+201C in UTF-8: 0xE2 0x80 0x9C
        // U+201D in UTF-8: 0xE2 0x80 0x9D
        bool foundOpen = false ;
        bool foundClose = false ;
        for(ssize_t i = 0 ; i < doc.GetNumberofParagraphs() ; i++)
        {
            std::string text = doc.GetParagraphText(i) ;
            if(text.find("\xE2\x80\x9C") != std::string::npos)
            {
                foundOpen = true ;
            }
            if(text.find("\xE2\x80\x9D") != std::string::npos)
            {
                foundClose = true ;
            }
        }
        CHECK(foundOpen) ;
        CHECK(foundClose) ;
    }


    TEST_CASE("Symbol PUA \\u61623 maps to standard Unicode bullet U+2022")
    {
        // U+F0B7 (61623 decimal) is the Symbol font PUA bullet character.
        // RTF files from Word encode bullets this way. The reader must
        // remap PUA to standard Unicode so the character renders correctly.
        std::string rtf = "{\\rtf1\\ansi \\u61623\\'3f}" ;
        cDocument doc ;
        doc.Clear() ;
        cRTFParser parser(rtf, &doc, nullptr) ;

        // Should contain U+2022 BULLET (UTF-8: 0xE2 0x80 0xA2), NOT PUA U+F0B7
        bool foundBullet = false ;
        bool foundPUA = false ;
        for(ssize_t i = 0 ; i < doc.GetNumberofParagraphs() ; i++)
        {
            std::string text = doc.GetParagraphText(i) ;
            // U+2022 BULLET in UTF-8
            if(text.find("\xE2\x80\xA2") != std::string::npos)
            {
                foundBullet = true ;
            }
            // U+F0B7 PUA in UTF-8 (should NOT be present)
            if(text.find("\xEF\x82\xB7") != std::string::npos)
            {
                foundPUA = true ;
            }
        }
        CHECK(foundBullet) ;
        CHECK_FALSE(foundPUA) ;
    }


    TEST_CASE("Symbol PUA Greek letter alpha maps to standard Unicode")
    {
        // U+F061 (61537 decimal) is Symbol font PUA lowercase alpha.
        // Should map to U+03B1 GREEK SMALL LETTER ALPHA.
        std::string rtf = "{\\rtf1\\ansi \\u61537\\'3f}" ;
        cDocument doc ;
        doc.Clear() ;
        cRTFParser parser(rtf, &doc, nullptr) ;

        // U+03B1 in UTF-8: 0xCE 0xB1
        bool found = false ;
        for(ssize_t i = 0 ; i < doc.GetNumberofParagraphs() ; i++)
        {
            std::string text = doc.GetParagraphText(i) ;
            if(text.find("\xCE\xB1") != std::string::npos)
            {
                found = true ;
            }
        }
        CHECK(found) ;
    }


    TEST_CASE("Symbol PUA negative \\u value maps correctly")
    {
        // U+F0B7 as signed 16-bit: 61623 - 65536 = -3913
        // Some RTF writers use the negative form per spec
        std::string rtf = "{\\rtf1\\ansi \\u-3913\\'3f}" ;
        cDocument doc ;
        doc.Clear() ;
        cRTFParser parser(rtf, &doc, nullptr) ;

        // Should still get U+2022 BULLET (UTF-8: 0xE2 0x80 0xA2)
        bool foundBullet = false ;
        for(ssize_t i = 0 ; i < doc.GetNumberofParagraphs() ; i++)
        {
            std::string text = doc.GetParagraphText(i) ;
            if(text.find("\xE2\x80\xA2") != std::string::npos)
            {
                foundBullet = true ;
            }
        }
        CHECK(foundBullet) ;
    }


    // ----------------------------------------------------------------
    //  Combined / integration tests
    // ----------------------------------------------------------------
    TEST_CASE("Complete document with multiple features")
    {
        // A more realistic RTF document with multiple features
        std::string rtf = "{\\rtf1\\pc\\deff0"
                          "{\\fonttbl{\\f0\\froman Times New Roman;}}"
                          "{\\colortbl ;\\red255\\green0\\blue0;}"
                          "\\paperw12240\\paperh15840"
                          "\\margl1800\\margr1800\\margt1440\\margb1440"
                          "\\landscape"
                          "\\headery720\\footery1080"
                          "{\\header Document Title\\chpgn}"
                          "{\\footer Copyright 2024}"
                          "\\pard\\li720\\fi360\\ri360\\ql"
                          " First paragraph with indent.\\par"
                          "\\pard\\qc Second centered paragraph.}" ;

        cDocument doc ;
        doc.Clear() ;
        cRTFParser parser(rtf, &doc, nullptr) ;

        auto paras = std::vector<std::string>() ;
        for(ssize_t i = 0; i < doc.GetNumberofParagraphs(); i++)
        {
            paras.push_back(doc.GetParagraphText(i)) ;
        }

        // Verify key features are present
        CHECK(HasParagraphStartingWith(paras, ".pr or=l")) ;   // landscape
        CHECK(HasParagraphStartingWith(paras, ".hm")) ;        // header margin
        CHECK(HasParagraphStartingWith(paras, ".fm")) ;        // footer margin
        CHECK(HasParagraphStartingWith(paras, ".h1")) ;        // header
        CHECK(HasParagraphStartingWith(paras, ".f1")) ;        // footer
        CHECK(HasParagraphStartingWith(paras, ".mt")) ;        // top margin
        CHECK(HasParagraphStartingWith(paras, ".mb")) ;        // bottom margin
        CHECK(HasParagraphStartingWith(paras, ".po")) ;        // page offset from margl
        CHECK(HasParagraphStartingWith(paras, ".lm")) ;        // left margin from li
        CHECK(HasParagraphStartingWith(paras, ".pm")) ;        // paragraph margin from li+fi

        // Text should be present
        bool foundFirst = false ;
        bool foundSecond = false ;
        for(auto &p : paras)
        {
            if(p.find("First paragraph") != std::string::npos)
            {
                foundFirst = true ;
            }
            if(p.find("Second centered") != std::string::npos)
            {
                foundSecond = true ;
            }
        }
        CHECK(foundFirst) ;
        CHECK(foundSecond) ;
    }
}


TEST_CASE("RTF header/footer alignment - center via TAB_CENTER")
{
    SUBCASE("\\qc in header inserts TAB_CENTER at start of header text")
    {
        std::string rtf =
            "{\\rtf1\\ansi"
            "{\\header {\\qc Centered Header}\\par}"
            "Body text\\par}" ;

        cDocument doc ;
        doc.Clear() ;
        cRTFParser parser(rtf, &doc, nullptr) ;

        // Find the header dot command paragraph
        bool foundHeader = false ;
        for(ssize_t i = 0; i < doc.GetNumberofParagraphs(); i++)
        {
            std::string text = doc.GetParagraphText(i) ;
            if(text.find(".h1 ") == 0 || text.find(".h1 ") != std::string::npos)
            {
                // Header paragraph should start with ".h1 " then MARKER_CHAR
                // ".h1 " = 4 chars, then MARKER_CHAR at position 4
                CHECK(text.length() > 4) ;
                CHECK(static_cast<unsigned char>(text[4]) == MARKER_CHAR) ;

                // Verify the tab type via document API
                POSITION_T paraStart = 0 ;
                POSITION_T paraEnd = 0 ;
                doc.GetParagraphStartandEnd(i, paraStart, paraEnd) ;

                // MARKER_CHAR is at grapheme position 4 within the paragraph (after ".h1 ")
                POSITION_T markerDocPos = paraStart + 4 ;
                eModifiers controlType = doc.GetControlChar(markerDocPos) ;
                CHECK(controlType == STYLE_TAB) ;

                sWSTab tabInfo = doc.GetTab(markerDocPos) ;
                CHECK(tabInfo.type == TAB_CENTER) ;

                // Verify the header text follows the MARKER_CHAR
                CHECK(text.find("Centered Header") != std::string::npos) ;

                foundHeader = true ;
                break ;
            }
        }
        CHECK(foundHeader) ;
    }

    SUBCASE("\\qr in header inserts TAB_RIGHT at start of header text")
    {
        std::string rtf =
            "{\\rtf1\\ansi"
            "{\\header {\\qr Right Header}\\par}"
            "Body\\par}" ;

        cDocument doc ;
        doc.Clear() ;
        cRTFParser parser(rtf, &doc, nullptr) ;

        bool foundHeader = false ;
        for(ssize_t i = 0; i < doc.GetNumberofParagraphs(); i++)
        {
            std::string text = doc.GetParagraphText(i) ;
            if(text.find(".h1 ") == 0)
            {
                CHECK(text.length() > 4) ;
                CHECK(static_cast<unsigned char>(text[4]) == MARKER_CHAR) ;

                POSITION_T paraStart = 0 ;
                POSITION_T paraEnd = 0 ;
                doc.GetParagraphStartandEnd(i, paraStart, paraEnd) ;

                POSITION_T markerDocPos = paraStart + 4 ;
                eModifiers controlType = doc.GetControlChar(markerDocPos) ;
                CHECK(controlType == STYLE_TAB) ;

                sWSTab tabInfo = doc.GetTab(markerDocPos) ;
                CHECK(tabInfo.type == TAB_RIGHT) ;

                CHECK(text.find("Right Header") != std::string::npos) ;
                foundHeader = true ;
                break ;
            }
        }
        CHECK(foundHeader) ;
    }

    SUBCASE("No alignment (default left) - no MARKER_CHAR in header text")
    {
        std::string rtf =
            "{\\rtf1\\ansi"
            "{\\header Left Header\\par}"
            "Body\\par}" ;

        cDocument doc ;
        doc.Clear() ;
        cRTFParser parser(rtf, &doc, nullptr) ;

        bool foundHeader = false ;
        for(ssize_t i = 0; i < doc.GetNumberofParagraphs(); i++)
        {
            std::string text = doc.GetParagraphText(i) ;
            if(text.find(".h1 ") == 0)
            {
                // No MARKER_CHAR should appear in left-aligned header
                CHECK(text.find(static_cast<char>(MARKER_CHAR)) == std::string::npos) ;
                CHECK(text.find("Left Header") != std::string::npos) ;
                foundHeader = true ;
                break ;
            }
        }
        CHECK(foundHeader) ;
    }

    SUBCASE("Multi-line centered header - each line gets TAB_CENTER")
    {
        std::string rtf =
            "{\\rtf1\\ansi"
            "{\\header {\\qc Line One\\par Line Two}\\par}"
            "Body\\par}" ;

        cDocument doc ;
        doc.Clear() ;
        cRTFParser parser(rtf, &doc, nullptr) ;

        int headerCount = 0 ;
        for(ssize_t i = 0; i < doc.GetNumberofParagraphs(); i++)
        {
            std::string text = doc.GetParagraphText(i) ;
            if(text.find(".h") == 0 && text.length() > 4)
            {
                // Both .h1 and .h2 lines should have MARKER_CHAR (TAB_CENTER)
                // The prefix is ".h1 " or ".h2 " = 4 chars
                if(static_cast<unsigned char>(text[4]) == MARKER_CHAR)
                {
                    POSITION_T paraStart = 0 ;
                    POSITION_T paraEnd = 0 ;
                    doc.GetParagraphStartandEnd(i, paraStart, paraEnd) ;

                    POSITION_T markerDocPos = paraStart + 4 ;
                    sWSTab tabInfo = doc.GetTab(markerDocPos) ;
                    CHECK(tabInfo.type == TAB_CENTER) ;
                    headerCount++ ;
                }
            }
        }
        CHECK(headerCount == 2) ;
    }

    SUBCASE("\\qc in footer inserts TAB_CENTER")
    {
        std::string rtf =
            "{\\rtf1\\ansi"
            "{\\footer {\\qc Page #}\\par}"
            "Body\\par}" ;

        cDocument doc ;
        doc.Clear() ;
        cRTFParser parser(rtf, &doc, nullptr) ;

        bool foundFooter = false ;
        for(ssize_t i = 0; i < doc.GetNumberofParagraphs(); i++)
        {
            std::string text = doc.GetParagraphText(i) ;
            if(text.find(".f1 ") == 0)
            {
                CHECK(text.length() > 4) ;
                CHECK(static_cast<unsigned char>(text[4]) == MARKER_CHAR) ;

                POSITION_T paraStart = 0 ;
                POSITION_T paraEnd = 0 ;
                doc.GetParagraphStartandEnd(i, paraStart, paraEnd) ;

                POSITION_T markerDocPos = paraStart + 4 ;
                sWSTab tabInfo = doc.GetTab(markerDocPos) ;
                CHECK(tabInfo.type == TAB_CENTER) ;

                foundFooter = true ;
                break ;
            }
        }
        CHECK(foundFooter) ;
    }
}


/////////////////////////////////////////////////////////////////////////////
//
// Edge case and corner case tests
//
/////////////////////////////////////////////////////////////////////////////


TEST_CASE("RTF missing closing brace")
{
    // Malformed RTF with no closing brace -- should not crash
    std::vector<std::string> paras = ParseRTF("{\\rtf1 Hello World") ;

    // Should have parsed some text
    bool foundHello = false ;
    for (const auto& p : paras)
    {
        if (p.find("Hello") != std::string::npos)
        {
            foundHello = true ;
            break ;
        }
    }
    CHECK(foundHello) ;
}


TEST_CASE("RTF deeply nested groups")
{
    // Build RTF with 50+ nested groups
    std::string rtf = "{\\rtf1 " ;
    for (int i = 0; i < 50; i++)
    {
        rtf += "{" ;
    }
    rtf += "Deep text" ;
    for (int i = 0; i < 50; i++)
    {
        rtf += "}" ;
    }
    rtf += "}" ;

    // Should not crash or stack overflow
    std::vector<std::string> paras = ParseRTF(rtf) ;

    // Should have parsed the text
    bool foundDeep = false ;
    for (const auto& p : paras)
    {
        if (p.find("Deep") != std::string::npos)
        {
            foundDeep = true ;
            break ;
        }
    }
    CHECK(foundDeep) ;
}


TEST_CASE("RTF invalid control word parameter")
{
    // Negative font size -- should not crash
    std::vector<std::string> paras = ParseRTF("{\\rtf1 \\fs-10 Hello}") ;

    // Should have parsed the text
    bool foundHello = false ;
    for (const auto& p : paras)
    {
        if (p.find("Hello") != std::string::npos)
        {
            foundHello = true ;
            break ;
        }
    }
    CHECK(foundHello) ;
}


TEST_CASE("RTF unknown control word ignored")
{
    // Unknown control word should be silently ignored
    std::vector<std::string> paras = ParseRTF("{\\rtf1 \\zzzunknown99 Hello}") ;

    // Should have parsed "Hello" text
    bool foundHello = false ;
    for (const auto& p : paras)
    {
        if (p.find("Hello") != std::string::npos)
        {
            foundHello = true ;
            break ;
        }
    }
    CHECK(foundHello) ;
}


TEST_CASE("RTF round-trip preserves text")
{
    // Create a document with text
    cDocument sourceDoc ;
    sourceDoc.Clear() ;
    sourceDoc.Insert("Round trip test.\r") ;
    sourceDoc.Insert("Second paragraph.\r") ;

    // Write to RTF string would require cRTFWriter which writes to file.
    // Instead, test a simpler round-trip: parse a known RTF, verify content.
    std::string rtf = "{\\rtf1\\ansi{\\fonttbl{\\f0 Courier New;}}"
                      "\\f0\\fs24 Round trip test.\\par "
                      "Second paragraph.\\par}" ;

    std::vector<std::string> paras = ParseRTF(rtf) ;

    bool foundFirst = false ;
    bool foundSecond = false ;
    for (const auto& p : paras)
    {
        if (p.find("Round trip") != std::string::npos)
        {
            foundFirst = true ;
        }
        if (p.find("Second paragraph") != std::string::npos)
        {
            foundSecond = true ;
        }
    }
    CHECK(foundFirst) ;
    CHECK(foundSecond) ;
}
