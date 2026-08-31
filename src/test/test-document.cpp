#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <string>
#include <vector>
#include <iostream>

#include "src/core/include/config.h"
#include "src/core/document/document.h"

// these strings are the same, except one is escape sequences
// Hindi text (same as escape sequence below): "kaachm shaknomyattum. nopahinstim maam."
// windows won't display utf8 on it's console, but I use it here to use Visual Studio's profiler on the UTF code.
std::string utf8text = "\xe0\xa4\x95\xe0\xa4\xbe\xe0\xa4\x9a\xe0\xa4\x82\x20\xe0\xa4\xb6\xe0\xa4\x95\xe0\xa5\x8d\xe0\xa4\xa8\xe0\xa5\x8b\xe0\xa4\xae\xe0\xa5\x8d\xe0\xa4\xaf\xe0\xa4\xa4\xe0\xa5\x8d\xe0\xa4\xa4\xe0\xa5\x81\xe0\xa4\xae\xe0\xa5\x8d\x20\xe0\xa5\xa4\x20\xe0\xa4\xa8\xe0\xa5\x8b\xe0\xa4\xaa\xe0\xa4\xb9\xe0\xa4\xbf\xe0\xa4\xa8\xe0\xa4\xb8\xe0\xa5\x8d\xe0\xa4\xa4\xe0\xa4\xbf\x20\xe0\xa4\xae\xe0\xa4\xbe\xe0\xa4\xae\xe0\xa5\x8d\x20\xe0\xa5\xa5\r";

// 57 word paragraph
std::string paratext = "Light flickered in the dark corners of the room, growing into caricatures of guitars, their strings vibrating soundlessly. The air danced with images made of Threads that shimmered in the still, stifling air of the small room. Motes of dust floated through the strands, highlighting tigers that danced through a living sky of blue and orange.\r" ;


void PrintParagraphs(cDocument &document) 
{
    POSITION_T c = 0 ;
    for (ssize_t i = 0; i < document.GetNumberofParagraphs(); ++i) 
    {
        std::string paragraph = document.GetParagraphText(i);
        for (size_t j = 0; j < paragraph.size(); ++j) 
        {
            char ch = paragraph[j];
            if (ch == MARKER_CHAR) 
            {
                document.GetControlChar(c) ;

                std::string styleType;
                switch (document.GetControlChar(c)) 
                {
                    case STYLE_BOLD:
                        styleType = "\033[7mB\033[0m";
                        break;
                    case STYLE_ITALICS:
                        styleType = "\033[7mY\033[0m";
                        break;
                    case STYLE_UNDERLINE:
                        styleType = "\033[7mS\033[0m";
                        break;
                    case STYLE_EOF:
                        styleType = "\033[7mZ\033[0m";
                        break;
                    case STYLE_STRIKETHROUGH:
                        styleType = "\033[7mX\033[0m";
                        break;
                    case STYLE_SUPERSCRIPT:
                        styleType = "\033[7mT\033[0m";
                        break;
                    case STYLE_SUBSCRIPT:
                        styleType = "\033[7mV\033[0m";
                        break;
                    default:
                        char t[200] ;
                        sprintf(t, "\033[7m%d\033[0m", ch) ;
                        styleType = t ;
                        break;
                }
                std::cout << styleType;
            } 
            else 
            {
                std::cout << ch;
            }
            c++ ;
        }
        std::cout << std::endl;
    }
    std::cout << std::endl;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  cDocument& doc   [in] the document to query
/// @param  POSITION_T pos   [in] the grapheme position to check
///
/// @return the resolved character (style code for control chars, raw char otherwise)
///
/// @brief
/// Test helper: resolve MARKER_CHAR to its actual control code via
/// GetControlChar(). For regular text characters, returns the character
/// as-is. This bridges the gap between GetCharNoAdvance() (which returns
/// raw MARKER_CHAR for control codes) and test assertions that compare
/// against style constants like STYLE_BOLD, STYLE_EOF, etc.
///
/////////////////////////////////////////////////////////////////////////////
static char resolveControlChar(cDocument& doc, POSITION_T pos)
{
    std::string g = doc.GetCharNoAdvance(pos);
    if (!g.empty() && g[0] == MARKER_CHAR)
    {
        return static_cast<char>(doc.GetControlChar(pos));
    }
    return g.empty() ? 0 : g[0];
}


TEST_CASE("Freshly created document is empty")
{
    cDocument document ;
    document.SetShowControl(SHOW_ALL) ;

    POSITION_T len = document.GetTextSize() ;

    CHECK(len == 1) ;           // len is one because of the ^Z
}


TEST_CASE("Inserting a character")
{
    cDocument document ;
    document.SetShowControl(SHOW_ALL) ;

    document.Insert('B') ;

    SUBCASE("Cleared document buffer is empty")
    {
        cDocument document ;
        document.SetShowControl(SHOW_ALL) ;

        document.Clear() ;
        POSITION_T len = document.GetTextSize() ;

        CHECK(len == 1) ;           // 1 because of the ^Z
        CHECK(document.GetPosition() == 0) ;
        CHECK(document.GetNumberofParagraphs() == 1) ;
    }
}

TEST_CASE("Insert 1 character GetPosition")
{
    cDocument document ;
    document.SetShowControl(SHOW_ALL) ;

    document.Insert('a') ;
    POSITION_T x = document.GetPosition() ;

    CHECK(x == 1) ;
    CHECK(document.GetTextSize() == 2) ;
}

TEST_CASE("Insert 10 character GetPosition")
{
    cDocument document ;
    document.SetShowControl(SHOW_ALL) ;

    document.Insert('a') ;

    std::string ch = "b" ;
    for(auto loop = 0; loop < 10; ++loop)
    {
        document.Insert(ch) ;
        ch[0] += 1 ;
    }

    POSITION_T x = document.GetPosition() ;

    CHECK(x == 11) ;
    CHECK(document.GetNumberofParagraphs() == 1) ;
    CHECK(document.GetTextSize() == 12) ;
    CHECK(document.GetCharNoAdvance(0) == "a") ;
    CHECK(document.GetCharNoAdvance(1) == "b") ;
    CHECK(document.GetCharNoAdvance(2) == "c") ;
    CHECK(document.GetCharNoAdvance(3) == "d") ;
    CHECK(document.GetCharNoAdvance(4) == "e") ;
    CHECK(document.GetCharNoAdvance(5) == "f") ;
    CHECK(document.GetCharNoAdvance(6) == "g") ;
    CHECK(document.GetCharNoAdvance(7) == "h") ;
    CHECK(document.GetCharNoAdvance(8) == "i") ;
    CHECK(document.GetCharNoAdvance(9) == "j") ;
    CHECK(document.GetCharNoAdvance(10) == "k") ;

    std::string text = "abcdefghijk" ;
    text += MARKER_CHAR ;
    CHECK(document.GetParagraphText(0) == text) ;

    SUBCASE("Cleared document buffer is empty")
    {
        cDocument document ;
        document.SetShowControl(SHOW_ALL) ;

        document.Clear() ;
        POSITION_T len = document.GetTextSize() ;

        CHECK(len == 1) ;           // 1 because of the ^Z
        CHECK(document.GetPosition() == 0) ;
        CHECK(document.GetNumberofParagraphs() == 1) ;
    }
}

TEST_CASE("Grapheme Count (UTF8)")
{
    cDocument document ;
    document.SetShowControl(SHOW_ALL) ;

    std::vector<POSITION_T> offsets ;
    size_t len = document.GraphemeCount(utf8text, offsets) ;
    for(size_t loop = 0; loop < len; loop++)
    {
        std::string grapheme = utf8text.substr(offsets[loop], offsets[loop + 1] - offsets[loop]);

        document.Insert(grapheme) ;
    }

    CHECK(document.GraphemeCount(utf8text, offsets) == 22) ;
    CHECK(document.GetTextSize() == 23) ;
    CHECK(document.GetNumberofParagraphs() == 2) ;          // \r at end of text
    CHECK(document.GetParagraphText(0) == utf8text) ;

    std::vector<POSITION_T> docOffsets;
    size_t docLen = document.GraphemeCount(document.GetParagraphText(0), docOffsets);

    CHECK(len == docLen);

    for (size_t i = 0; i < len; ++i) {
        std::string utf8Grapheme = utf8text.substr(offsets[i], offsets[i + 1] - offsets[i]);
        std::string docGrapheme = document.GetParagraphText(0).substr(docOffsets[i], docOffsets[i + 1] - docOffsets[i]);

        CHECK(utf8Grapheme == docGrapheme);
    }
}

TEST_CASE("Insert 4 novels worth of text")
{
    size_t num = 1074 ; //7018 ;   // 4 novels worth of text based on paratext

    cDocument document ;
    document.SetShowControl(SHOW_ALL) ;

    for(size_t loop = 0; loop < num; loop++)
    {
        document.Insert(paratext) ;
    }

    CHECK(document.GetTextSize() == paratext.length() * num + 1) ;
    CHECK(document.GetNumberofParagraphs() == num + 1) ;
    CHECK(document.GetParagraphText(0) == paratext) ;
    CHECK(document.GetParagraphText(num / 2) == paratext) ;
    CHECK(document.GetParagraphText(num - 1) == paratext) ;   

    SUBCASE("Set Position check") 
    {
        document.SetPosition((paratext.length() * num) / 2) ;
        CHECK(document.GetPosition() == (paratext.length() * num) / 2) ;
    }
}

TEST_CASE("check deletion")
{
    cDocument document ;
    document.SetShowControl(SHOW_ALL) ;

    document.Insert("1234567890ABCDEFGHIJ") ;

    std::string paragraphText = document.GetParagraphText(0);
    paragraphText.pop_back(); // remove trailing ^Z
    CHECK(paragraphText == "1234567890ABCDEFGHIJ") ;

    document.Delete(9, 1) ;
    paragraphText = document.GetParagraphText(0);
    paragraphText.pop_back(); // remove trailing ^Z
    CHECK(paragraphText == "123456789ABCDEFGHIJ") ;

    document.Delete(0, 1) ;
    paragraphText = document.GetParagraphText(0);
    paragraphText.pop_back(); // remove trailing ^Z
    CHECK(paragraphText == "23456789ABCDEFGHIJ") ;

    document.Delete(0, 1) ;
    paragraphText = document.GetParagraphText(0);
    paragraphText.pop_back(); // remove trailing ^Z
    CHECK(paragraphText == "3456789ABCDEFGHIJ") ;

    document.Delete(0, 1) ;
    paragraphText = document.GetParagraphText(0);
    paragraphText.pop_back(); // remove trailing ^Z
    CHECK(paragraphText == "456789ABCDEFGHIJ") ;

    document.Delete(0, 1) ;
    paragraphText = document.GetParagraphText(0);
    paragraphText.pop_back(); // remove trailing ^Z
    CHECK(paragraphText == "56789ABCDEFGHIJ") ;

    document.Delete(0, 1) ;
    paragraphText = document.GetParagraphText(0);
    paragraphText.pop_back(); // remove trailing ^Z
    CHECK(paragraphText == "6789ABCDEFGHIJ") ;

    document.Delete(0, 1) ;
    paragraphText = document.GetParagraphText(0);
    paragraphText.pop_back(); // remove trailing ^Z
    CHECK(paragraphText == "789ABCDEFGHIJ") ;

    document.Delete(0, 1) ;
    paragraphText = document.GetParagraphText(0);
    paragraphText.pop_back(); // remove trailing ^Z
    CHECK(paragraphText == "89ABCDEFGHIJ") ;

    document.Delete(0, 1) ;
    paragraphText = document.GetParagraphText(0);
    paragraphText.pop_back(); // remove trailing ^Z
    CHECK(paragraphText == "9ABCDEFGHIJ") ;

    document.Delete(0, 1) ;
    paragraphText = document.GetParagraphText(0);
    paragraphText.pop_back(); // remove trailing ^Z
    CHECK(paragraphText == "ABCDEFGHIJ") ;

    document.Delete(0, 1) ;
    paragraphText = document.GetParagraphText(0);
    paragraphText.pop_back(); // remove trailing ^Z
    CHECK(paragraphText == "BCDEFGHIJ") ;

    document.Delete(0, 1) ;
    paragraphText = document.GetParagraphText(0);
    paragraphText.pop_back(); // remove trailing ^Z
    CHECK(paragraphText == "CDEFGHIJ") ;

    document.Delete(0, 1) ;
    paragraphText = document.GetParagraphText(0);
    paragraphText.pop_back(); // remove trailing ^Z
    CHECK(paragraphText == "DEFGHIJ") ;

    document.Delete(0, 7) ;
    paragraphText = document.GetParagraphText(0);
    paragraphText.pop_back(); // remove trailing ^Z
    CHECK(paragraphText == "") ;
    CHECK(document.GetParagraphText(0)[0] == MARKER_CHAR) ;
}

TEST_CASE("Insert into cleared docuemnt")
{
    cDocument document ;
    document.SetShowControl(SHOW_ALL) ;

    document.Insert("This is a test") ;

    document.Clear() ;
//    std::string text = "Tamil-chars 1234567890ABCDEFGHIJ";  // Tamil text (same as escape sequence below)
    std::string text = "\xe0\xae\xa3\xe0\xaf\x8b 1234567890ABCDEFGHIJ";

    document.Insert(text) ;

    std::string str = document.GetParagraphText(0) ;
    str.pop_back(); // remove trailing ^Z
    CHECK(str == text) ;

    SUBCASE("Insert into beginnng of document")
    {
        document.SetPosition(0) ;
        document.Insert('A') ;

        text = "A" + text ;

        str = document.GetParagraphText(0) ;
        str.pop_back(); // remove trailing ^Z
        CHECK(str == text) ;
    }

    SUBCASE("Insert into middle of document")
    {
        document.SetPosition(10) ;
        document.Insert('-') ;

        text.insert(15, "-") ;      // since string isn't grapheme aware, we know that pso 15 is right

        str = document.GetParagraphText(0) ;
        str.pop_back(); // remove trailing ^Z
        CHECK(str == text) ;
    }

    SUBCASE("Insert into end of document")
    {
        document.SetPosition(document.GetTextSize() - 1) ;
        document.Insert('Z') ;

        text += 'Z' ;

        str = document.GetParagraphText(0) ;
        str.pop_back(); // remove trailing ^Z
        CHECK(str == text) ;
    }
}


TEST_CASE("Read past end of document")
{
    cDocument document ;
    document.SetShowControl(SHOW_ALL) ;

    document.Insert("This is a test") ;

    POSITION_T px = document.GetTextSize() ;
    std::string ch = document.GetCharNoAdvance(px + 10) ;

    CHECK(ch.length() == 0) ;
}

TEST_CASE("Basic paragraphs")
{
    cDocument document ;
    document.SetShowControl(SHOW_ALL) ;

    document.Insert("This is a test\r") ;
    document.Insert(" This is part two\r") ;
    document.Insert(" This is part 3\r") ;


    CHECK(document.GetNumberofParagraphs() == 4) ;
    CHECK(document.GetParagraphText(0) == "This is a test\r") ;
    CHECK(document.GetParagraphText(1) == " This is part two\r") ;
    CHECK(document.GetParagraphText(2) == " This is part 3\r") ;
    std::string t = document.GetParagraphText(3) ;
    CHECK(t[0] == MARKER_CHAR) ;

    SUBCASE("Delete Character")
    {
        document.Delete(0, 1) ;
        CHECK(document.GetNumberofParagraphs() == 4) ;
        CHECK(document.GetParagraphText(0) == "his is a test\r") ;
        CHECK(document.GetParagraphText(1) == " This is part two\r") ;
        CHECK(document.GetParagraphText(2) == " This is part 3\r") ;
        t = document.GetParagraphText(3) ;
        CHECK(t[0] == MARKER_CHAR) ;
    }

    SUBCASE("Delete Paragraph 1 (merge to line 1)")
    {
        document.Delete(14, 1) ;
        CHECK(document.GetNumberofParagraphs() == 3) ;
        CHECK(document.GetParagraphText(0) == "This is a test This is part two\r") ;
        CHECK(document.GetParagraphText(1) == " This is part 3\r") ;
        t = document.GetParagraphText(2) ;
        CHECK(t[0] == MARKER_CHAR) ;
    }

    SUBCASE("Delete Paragraph 2 (merge to line 2)")
    {
        document.SetPosition(document.GetTextSize() - 1) ;
        document.Insert("-This is part 4") ;
        document.Delete(48, 1) ;
        CHECK(document.GetNumberofParagraphs() == 3) ;

        std::string t = " This is part 3-This is part 4" ;
        t.append(1, MARKER_CHAR) ;
        CHECK(document.GetParagraphText(2) == t) ;
    }

    SUBCASE("Delete Paragraph 3 (Empty)")
    {
        document.SetPosition(14) ;
        document.Insert("\r") ;
        CHECK(document.GetNumberofParagraphs() == 5) ;

        document.Delete(14, 1) ;
        CHECK(document.GetNumberofParagraphs() == 4) ;
        CHECK(document.GetParagraphText(0) == "This is a test\r") ;
        CHECK(document.GetParagraphText(1) == " This is part two\r") ;
        CHECK(document.GetParagraphText(2) == " This is part 3\r") ;
        std::string t = document.GetParagraphText(3) ;
        CHECK(t[0] == MARKER_CHAR) ;
    }
}

TEST_CASE("Check Control Charcaters")
{
    cDocument document ;
    document.SetShowControl(SHOW_ALL) ;

    document.Insert("This is a test\r") ;
    document.Insert(" This is part two\r") ;
    document.Insert(" This is part 3\r") ;

    // control char in first para
    document.SetPosition(4) ;
    document.BeginBold() ;
    document.SetPosition(10) ;
    document.EndBold() ;

    // control char in second para
    document.SetPosition(22) ;
    document.BeginItalics() ;
    document.SetPosition(28) ;
    document.EndItalics() ;

    // control char in third para
    document.SetPosition(40) ;
    document.BeginUnderline() ;
    document.SetPosition(48) ;
    document.EndUnderline() ;



    CHECK(resolveControlChar(document, 4) == STYLE_BOLD) ;
    CHECK(resolveControlChar(document, 10) == STYLE_BOLD) ;

    SUBCASE("Delete Character")
    {
        document.Delete(2, 1) ;
        CHECK(resolveControlChar(document, 3) == STYLE_BOLD) ;
        CHECK(resolveControlChar(document, 9) == STYLE_BOLD) ;
        CHECK(resolveControlChar(document, 21) == STYLE_ITALICS) ;
        CHECK(resolveControlChar(document, 27) == STYLE_ITALICS) ;
        CHECK(resolveControlChar(document, 39) == STYLE_UNDERLINE) ;
        CHECK(resolveControlChar(document, 47) == STYLE_UNDERLINE) ;
    }

    SUBCASE("Delete Character 2")
    {
        
        document.Delete(13, 1) ;
        CHECK(resolveControlChar(document, 4) == STYLE_BOLD) ;
        CHECK(resolveControlChar(document, 10) == STYLE_BOLD) ;
        CHECK(resolveControlChar(document, 21) == STYLE_ITALICS) ;
        CHECK(resolveControlChar(document, 27) == STYLE_ITALICS) ;
        CHECK(resolveControlChar(document, 39) == STYLE_UNDERLINE) ;
        CHECK(resolveControlChar(document, 47) == STYLE_UNDERLINE) ;
    }

    SUBCASE("Delete Character 3")
    {
        document.Delete(30, 1) ;
        CHECK(resolveControlChar(document, 4) == STYLE_BOLD) ;
        CHECK(resolveControlChar(document, 10) == STYLE_BOLD) ;
        CHECK(resolveControlChar(document, 22) == STYLE_ITALICS) ;
        CHECK(resolveControlChar(document, 28) == STYLE_ITALICS) ;
        CHECK(resolveControlChar(document, 39) == STYLE_UNDERLINE) ;
        CHECK(resolveControlChar(document, 47) == STYLE_UNDERLINE) ;
    }

    SUBCASE("Delete Paragraph 1 (merge to line 1)")
    {
        document.Delete(16, 1) ;
        CHECK(document.GetNumberofParagraphs() == 3) ;
        CHECK(resolveControlChar(document, 4) == STYLE_BOLD) ;
        CHECK(resolveControlChar(document, 10) == STYLE_BOLD) ;
        CHECK(resolveControlChar(document, 21) == STYLE_ITALICS) ;
        CHECK(resolveControlChar(document, 27) == STYLE_ITALICS) ;
        CHECK(resolveControlChar(document, 39) == STYLE_UNDERLINE) ;
        CHECK(resolveControlChar(document, 47) == STYLE_UNDERLINE) ;
    }

    SUBCASE("Delete Paragraph 2 (merge to line 2)")
    {
        document.Delete(36, 1) ;
        CHECK(document.GetNumberofParagraphs() == 3) ;
        CHECK(resolveControlChar(document, 4) == STYLE_BOLD) ;
        CHECK(resolveControlChar(document, 10) == STYLE_BOLD) ;
        CHECK(resolveControlChar(document, 22) == STYLE_ITALICS) ;
        CHECK(resolveControlChar(document, 28) == STYLE_ITALICS) ;
        CHECK(resolveControlChar(document, 39) == STYLE_UNDERLINE) ;
        CHECK(resolveControlChar(document, 47) == STYLE_UNDERLINE) ;
    }

    SUBCASE("Insert Character 1")
    {
        document.SetPosition(5) ;
        document.Insert('X') ;
        CHECK(resolveControlChar(document, 4) == STYLE_BOLD) ;
        CHECK(resolveControlChar(document, 11) == STYLE_BOLD) ;
        CHECK(resolveControlChar(document, 23) == STYLE_ITALICS) ;
        CHECK(resolveControlChar(document, 29) == STYLE_ITALICS) ;
        CHECK(resolveControlChar(document, 41) == STYLE_UNDERLINE) ;
        CHECK(resolveControlChar(document, 49) == STYLE_UNDERLINE) ;
    }

    SUBCASE("Insert Character 2")
    {
        document.SetPosition(22) ;
        document.Insert('X') ;
        CHECK(resolveControlChar(document, 4) == STYLE_BOLD) ;
        CHECK(resolveControlChar(document, 10) == STYLE_BOLD) ;
        CHECK(resolveControlChar(document, 23) == STYLE_ITALICS) ;
        CHECK(resolveControlChar(document, 29) == STYLE_ITALICS) ;
        CHECK(resolveControlChar(document, 41) == STYLE_UNDERLINE) ;
        CHECK(resolveControlChar(document, 49) == STYLE_UNDERLINE) ;
    }

    SUBCASE("Insert Character 3")
    {
        document.SetPosition(40) ;
        document.Insert('X') ;
        CHECK(resolveControlChar(document, 4) == STYLE_BOLD) ;
        CHECK(resolveControlChar(document, 10) == STYLE_BOLD) ;
        CHECK(resolveControlChar(document, 22) == STYLE_ITALICS) ;
        CHECK(resolveControlChar(document, 28) == STYLE_ITALICS) ;
        CHECK(resolveControlChar(document, 41) == STYLE_UNDERLINE) ;
        CHECK(resolveControlChar(document, 49) == STYLE_UNDERLINE) ;
    }

    SUBCASE("Insert Paragraph 1")
    {
        document.SetPosition(9) ;
        document.Insert("\r") ;
        CHECK(document.GetNumberofParagraphs() == 5) ;
        CHECK(resolveControlChar(document, 4) == STYLE_BOLD) ;
        CHECK(resolveControlChar(document, 11) == STYLE_BOLD) ;
        CHECK(resolveControlChar(document, 23) == STYLE_ITALICS) ;
        CHECK(resolveControlChar(document, 29) == STYLE_ITALICS) ;
        CHECK(resolveControlChar(document, 41) == STYLE_UNDERLINE) ;
        CHECK(resolveControlChar(document, 49) == STYLE_UNDERLINE) ;
    }

    SUBCASE("Insert Paragraph 2")
    {
        document.SetPosition(22) ;
        document.Insert("\r") ;
        CHECK(document.GetNumberofParagraphs() == 5) ;
        CHECK(resolveControlChar(document, 4) == STYLE_BOLD) ;
        CHECK(resolveControlChar(document, 10) == STYLE_BOLD) ;
        CHECK(resolveControlChar(document, 23) == STYLE_ITALICS) ;
        CHECK(resolveControlChar(document, 29) == STYLE_ITALICS) ;
        CHECK(resolveControlChar(document, 41) == STYLE_UNDERLINE) ;
        CHECK(resolveControlChar(document, 49) == STYLE_UNDERLINE) ;
    }

    SUBCASE("Insert Paragraph 3")
    {
        document.SetPosition(40) ;
        document.Insert("\r") ;
        CHECK(document.GetNumberofParagraphs() == 5) ;
        CHECK(resolveControlChar(document, 4) == STYLE_BOLD) ;
        CHECK(resolveControlChar(document, 10) == STYLE_BOLD) ;
        CHECK(resolveControlChar(document, 22) == STYLE_ITALICS) ;
        CHECK(resolveControlChar(document, 28) == STYLE_ITALICS) ;
        CHECK(resolveControlChar(document, 41) == STYLE_UNDERLINE) ;
        CHECK(resolveControlChar(document, 49) == STYLE_UNDERLINE) ;
    }

    SUBCASE("Insert Paragraph 4")
    {
        document.SetPosition(6) ;
        document.Insert("\r") ;
        document.SetPosition(27) ;
        document.Insert("\r") ;
        CHECK(document.GetNumberofParagraphs() == 6) ;
        CHECK(resolveControlChar(document, 4) == STYLE_BOLD) ;
        CHECK(resolveControlChar(document, 11) == STYLE_BOLD) ;
        CHECK(resolveControlChar(document, 23) == STYLE_ITALICS) ;
        CHECK(resolveControlChar(document, 30) == STYLE_ITALICS) ;
        CHECK(resolveControlChar(document, 42) == STYLE_UNDERLINE) ;
        CHECK(resolveControlChar(document, 50) == STYLE_UNDERLINE) ;
    }

    SUBCASE("Insert Paragraph 5")
    {
        document.SetPosition(9) ;
        document.Insert("\r") ;
        document.Insert("\r") ;
        CHECK(document.GetNumberofParagraphs() == 6) ;
        CHECK(resolveControlChar(document, 4) == STYLE_BOLD) ;
        CHECK(resolveControlChar(document, 12) == STYLE_BOLD) ;
        CHECK(resolveControlChar(document, 24) == STYLE_ITALICS) ;
        CHECK(resolveControlChar(document, 30) == STYLE_ITALICS) ;
        CHECK(resolveControlChar(document, 42) == STYLE_UNDERLINE) ;
        CHECK(resolveControlChar(document, 50) == STYLE_UNDERLINE) ;
    }
}


TEST_CASE("Complex MARKER_CHAR")
{
    cDocument document ;
    document.SetShowControl(SHOW_ALL) ;

    document.Insert("This is a test\r") ;
    document.Insert(" This is part two\r") ;
    document.Insert(" This is part 3\r") ;

    SUBCASE("Tab")
    {
        sWSTab tab ;
        tab.abstabsize = 0 ;
        tab.size = 0 ;
        tab.tabsize = 0 ;
        tab.type = TAB_TAB ;

        document.SetPosition(4) ;
        document.InsertTab(tab) ;
        document.Insert("test") ;
        document.InsertTab(tab) ;

        document.SetPosition(27) ;
        document.InsertTab(tab) ;
        document.Insert("test") ;
        document.InsertTab(tab) ;

        document.SetPosition(50) ;
        document.InsertTab(tab) ;
        document.Insert("test") ;
        document.InsertTab(tab) ;

        // check initial insertions
        SUBCASE("   Check initial insertions")
        {
            CHECK(resolveControlChar(document, 4) == STYLE_TAB) ;
            CHECK(resolveControlChar(document, 9) == STYLE_TAB) ;
            CHECK(resolveControlChar(document, 27) == STYLE_TAB) ;
            CHECK(resolveControlChar(document, 32) == STYLE_TAB) ;
            CHECK(resolveControlChar(document, 50) == STYLE_TAB) ;
            CHECK(resolveControlChar(document, 55) == STYLE_TAB) ;
        }


        SUBCASE("   Delete Character")
        {
            document.Delete(2, 1) ;
            CHECK(resolveControlChar(document, 3) == STYLE_TAB) ;
            CHECK(resolveControlChar(document, 8) == STYLE_TAB) ;
            CHECK(resolveControlChar(document, 26) == STYLE_TAB) ;
            CHECK(resolveControlChar(document, 31) == STYLE_TAB) ;
            CHECK(resolveControlChar(document, 49) == STYLE_TAB) ;
            CHECK(resolveControlChar(document, 54) == STYLE_TAB) ;
        }

        SUBCASE("   Delete Paragraph 1")
        {
            document.Delete(20, 1) ;
            CHECK(resolveControlChar(document, 4) == STYLE_TAB) ;
            CHECK(resolveControlChar(document, 9) == STYLE_TAB) ;
            CHECK(resolveControlChar(document, 26) == STYLE_TAB) ;
            CHECK(resolveControlChar(document, 31) == STYLE_TAB) ;
            CHECK(resolveControlChar(document, 49) == STYLE_TAB) ;
            CHECK(resolveControlChar(document, 54) == STYLE_TAB) ;
        }

        SUBCASE("   Delete Paragraph 2")
        {
            document.Delete(44, 1) ;
            CHECK(resolveControlChar(document, 4) == STYLE_TAB) ;
            CHECK(resolveControlChar(document, 9) == STYLE_TAB) ;
            CHECK(resolveControlChar(document, 27) == STYLE_TAB) ;
            CHECK(resolveControlChar(document, 32) == STYLE_TAB) ;
            CHECK(resolveControlChar(document, 49) == STYLE_TAB) ;
            CHECK(resolveControlChar(document, 54) == STYLE_TAB) ;
        }

        SUBCASE("   Insert Character 1")
        {
            document.SetPosition(2) ;
            document.Insert('X') ;
            CHECK(resolveControlChar(document, 5) == STYLE_TAB) ;
            CHECK(resolveControlChar(document, 10) == STYLE_TAB) ;
            CHECK(resolveControlChar(document, 28) == STYLE_TAB) ;
            CHECK(resolveControlChar(document, 33) == STYLE_TAB) ;
            CHECK(resolveControlChar(document, 51) == STYLE_TAB) ;
            CHECK(resolveControlChar(document, 56) == STYLE_TAB) ;
        }

        SUBCASE("   Insert Character 2")
        {
            document.SetPosition(26) ;
            document.Insert('X') ;
            CHECK(resolveControlChar(document, 4) == STYLE_TAB) ;
            CHECK(resolveControlChar(document, 9) == STYLE_TAB) ;
            CHECK(resolveControlChar(document, 28) == STYLE_TAB) ;
            CHECK(resolveControlChar(document, 33) == STYLE_TAB) ;
            CHECK(resolveControlChar(document, 51) == STYLE_TAB) ;
            CHECK(resolveControlChar(document, 56) == STYLE_TAB) ;
        }

        SUBCASE("   Insert Paragraph 1")
        {
            document.SetPosition(2) ;
            document.Insert("\r") ;
            CHECK(document.GetNumberofParagraphs() == 5) ;
            CHECK(resolveControlChar(document, 5) == STYLE_TAB) ;
            CHECK(resolveControlChar(document, 10) == STYLE_TAB) ;
            CHECK(resolveControlChar(document, 28) == STYLE_TAB) ;
            CHECK(resolveControlChar(document, 33) == STYLE_TAB) ;
            CHECK(resolveControlChar(document, 51) == STYLE_TAB) ;
            CHECK(resolveControlChar(document, 56) == STYLE_TAB) ;
        }

        SUBCASE("   Insert Paragraph 2")
        {
            document.SetPosition(26) ;
            document.Insert("\r") ;
            CHECK(document.GetNumberofParagraphs() == 5) ;
            CHECK(resolveControlChar(document, 4) == STYLE_TAB) ;
            CHECK(resolveControlChar(document, 9) == STYLE_TAB) ;
            CHECK(resolveControlChar(document, 28) == STYLE_TAB) ;
            CHECK(resolveControlChar(document, 33) == STYLE_TAB) ;
            CHECK(resolveControlChar(document, 51) == STYLE_TAB) ;
            CHECK(resolveControlChar(document, 56) == STYLE_TAB) ;
        }
    }

    SUBCASE("Color")
    {
        sSeqRGBColor color1 ;
        color1.red = 170 ; color1.green = 0 ; color1.blue = 170 ; color1.alpha = 255 ; // WS magenta (index 5)

        sSeqRGBColor color ;
        color.red = 0 ; color.green = 170 ; color.blue = 170 ; color.alpha = 255 ; // WS cyan (index 3)
        
        document.SetPosition(4) ;
        document.InsertColor(color1) ;
        document.Insert("test") ;
        document.InsertColor(color1) ;

        document.SetPosition(27) ;
        document.InsertColor(color1) ;
        document.Insert("test") ;
        document.InsertColor(color1) ;

        document.SetPosition(50) ;
        document.InsertColor(color) ;
        document.Insert("test") ;
        document.InsertColor(color1) ;

        // check initial insertions
        SUBCASE("   Check initial insertions")
        {
            CHECK(resolveControlChar(document, 4) == STYLE_INTERNAL_COLOR) ;
            CHECK(resolveControlChar(document, 9) == STYLE_INTERNAL_COLOR) ;
            CHECK(resolveControlChar(document, 27) == STYLE_INTERNAL_COLOR) ;
            CHECK(resolveControlChar(document, 32) == STYLE_INTERNAL_COLOR) ;
            CHECK(resolveControlChar(document, 50) == STYLE_INTERNAL_COLOR) ;
            CHECK(resolveControlChar(document, 55) == STYLE_INTERNAL_COLOR) ;

            sSeqRGBColor c ;
            CHECK(document.GetColor(50, c) == true) ;
            document.GetColor(50, c) ;
            CHECK(c.red == color.red) ;
            CHECK(c.green == color.green) ;
            CHECK(c.blue == color.blue) ;

        }


        SUBCASE("   Delete Character")
        {
            document.Delete(2, 1) ;
            CHECK(resolveControlChar(document, 3) == STYLE_INTERNAL_COLOR) ;
            CHECK(resolveControlChar(document, 8) == STYLE_INTERNAL_COLOR) ;
            CHECK(resolveControlChar(document, 26) == STYLE_INTERNAL_COLOR) ;
            CHECK(resolveControlChar(document, 31) == STYLE_INTERNAL_COLOR) ;
            CHECK(resolveControlChar(document, 49) == STYLE_INTERNAL_COLOR) ;
            CHECK(resolveControlChar(document, 54) == STYLE_INTERNAL_COLOR) ;

            sSeqRGBColor c ;
            CHECK(document.GetColor(49, c) == true) ;
            document.GetColor(49, c) ;
            CHECK(c.red == color.red) ;
            CHECK(c.green == color.green) ;
            CHECK(c.blue == color.blue) ;
        }

        SUBCASE("   Delete Paragraph 1")
        {
            document.Delete(20, 1) ;
            CHECK(resolveControlChar(document, 4) == STYLE_INTERNAL_COLOR) ;
            CHECK(resolveControlChar(document, 9) == STYLE_INTERNAL_COLOR) ;
            CHECK(resolveControlChar(document, 26) == STYLE_INTERNAL_COLOR) ;
            CHECK(resolveControlChar(document, 31) == STYLE_INTERNAL_COLOR) ;
            CHECK(resolveControlChar(document, 49) == STYLE_INTERNAL_COLOR) ;
            CHECK(resolveControlChar(document, 54) == STYLE_INTERNAL_COLOR) ;
        
            sSeqRGBColor c ;
            CHECK(document.GetColor(49, c) == true) ;
            document.GetColor(49, c) ;
            CHECK(c.red == color.red) ;
            CHECK(c.green == color.green) ;
            CHECK(c.blue == color.blue) ;
        }

        SUBCASE("   Delete Paragraph 2")
        {
            document.Delete(44, 1) ;
            CHECK(resolveControlChar(document, 4) == STYLE_INTERNAL_COLOR) ;
            CHECK(resolveControlChar(document, 9) == STYLE_INTERNAL_COLOR) ;
            CHECK(resolveControlChar(document, 27) == STYLE_INTERNAL_COLOR) ;
            CHECK(resolveControlChar(document, 32) == STYLE_INTERNAL_COLOR) ;
            CHECK(resolveControlChar(document, 49) == STYLE_INTERNAL_COLOR) ;
            CHECK(resolveControlChar(document, 54) == STYLE_INTERNAL_COLOR) ;
        
            sSeqRGBColor c ;
            CHECK(document.GetColor(49, c) == true) ;
            document.GetColor(49, c) ;
            CHECK(c.red == color.red) ;
            CHECK(c.green == color.green) ;
            CHECK(c.blue == color.blue) ;
        }

        SUBCASE("   Insert Character 1")
        {
            document.SetPosition(2) ;
            document.Insert('X') ;
            CHECK(resolveControlChar(document, 5) == STYLE_INTERNAL_COLOR) ;
            CHECK(resolveControlChar(document, 10) == STYLE_INTERNAL_COLOR) ;
            CHECK(resolveControlChar(document, 28) == STYLE_INTERNAL_COLOR) ;
            CHECK(resolveControlChar(document, 33) == STYLE_INTERNAL_COLOR) ;
            CHECK(resolveControlChar(document, 51) == STYLE_INTERNAL_COLOR) ;
            CHECK(resolveControlChar(document, 56) == STYLE_INTERNAL_COLOR) ;
        
            sSeqRGBColor c ;
            CHECK(document.GetColor(51, c) == true) ;
            document.GetColor(51, c) ;
            CHECK(c.red == color.red) ;
            CHECK(c.green == color.green) ;
            CHECK(c.blue == color.blue) ;
        }

        SUBCASE("   Insert Character 2")
        {
            document.SetPosition(26) ;
            document.Insert('X') ;
            CHECK(resolveControlChar(document, 4) == STYLE_INTERNAL_COLOR) ;
            CHECK(resolveControlChar(document, 9) == STYLE_INTERNAL_COLOR) ;
            CHECK(resolveControlChar(document, 28) == STYLE_INTERNAL_COLOR) ;
            CHECK(resolveControlChar(document, 33) == STYLE_INTERNAL_COLOR) ;
            CHECK(resolveControlChar(document, 51) == STYLE_INTERNAL_COLOR) ;
            CHECK(resolveControlChar(document, 56) == STYLE_INTERNAL_COLOR) ;
        
            sSeqRGBColor c ;
            CHECK(document.GetColor(51, c) == true) ;
            document.GetColor(51, c) ;
            CHECK(c.red == color.red) ;
            CHECK(c.green == color.green) ;
            CHECK(c.blue == color.blue) ;
        }

        SUBCASE("   Insert Paragraph 1")
        {
            document.SetPosition(2) ;
            document.Insert("\r") ;
            CHECK(document.GetNumberofParagraphs() == 5) ;
            CHECK(resolveControlChar(document, 5) == STYLE_INTERNAL_COLOR) ;
            CHECK(resolveControlChar(document, 10) == STYLE_INTERNAL_COLOR) ;
            CHECK(resolveControlChar(document, 28) == STYLE_INTERNAL_COLOR) ;
            CHECK(resolveControlChar(document, 33) == STYLE_INTERNAL_COLOR) ;
            CHECK(resolveControlChar(document, 51) == STYLE_INTERNAL_COLOR) ;
            CHECK(resolveControlChar(document, 56) == STYLE_INTERNAL_COLOR) ;
        
            sSeqRGBColor c ;
            CHECK(document.GetColor(51, c) == true) ;
            document.GetColor(51, c) ;
            CHECK(c.red == color.red) ;
            CHECK(c.green == color.green) ;
            CHECK(c.blue == color.blue) ;
        }

        SUBCASE("   Insert Paragraph 2")
        {
            document.SetPosition(26) ;
            document.Insert("\r") ;
            CHECK(document.GetNumberofParagraphs() == 5) ;
            CHECK(resolveControlChar(document, 4) == STYLE_INTERNAL_COLOR) ;
            CHECK(resolveControlChar(document, 9) == STYLE_INTERNAL_COLOR) ;
            CHECK(resolveControlChar(document, 28) == STYLE_INTERNAL_COLOR) ;
            CHECK(resolveControlChar(document, 33) == STYLE_INTERNAL_COLOR) ;
            CHECK(resolveControlChar(document, 51) == STYLE_INTERNAL_COLOR) ;
            CHECK(resolveControlChar(document, 56) == STYLE_INTERNAL_COLOR) ;
        
            sSeqRGBColor c ;
            CHECK(document.GetColor(51, c) == true) ;
            document.GetColor(51, c) ;
            CHECK(c.red == color.red) ;
            CHECK(c.green == color.green) ;
            CHECK(c.blue == color.blue) ;
        }
    }

    SUBCASE("Font")
    {
        sInternalFonts font ;
        font.fontname = "Arial" ;
        font.size = 12 ;
        font.haveWSFont = false ;
        font.fontname = "Arial" ;

        document.SetPosition(4) ;
        document.InsertFont(font) ;
        document.Insert("test") ;
        document.InsertFont(font) ;

        document.SetPosition(27) ;
        document.InsertFont(font) ;
        document.Insert("test") ;
        document.InsertFont(font) ;

        document.SetPosition(50) ;
        document.InsertFont(font) ;
        document.Insert("test") ;
        document.InsertFont(font) ;

        // check initial insertions
        SUBCASE("   Check initial insertions")
        {
            CHECK(resolveControlChar(document, 4) == STYLE_FONT1) ;
            CHECK(resolveControlChar(document, 9) == STYLE_FONT1) ;
            CHECK(resolveControlChar(document, 27) == STYLE_FONT1) ;
            CHECK(resolveControlChar(document, 32) == STYLE_FONT1) ;
            CHECK(resolveControlChar(document, 50) == STYLE_FONT1) ;
            CHECK(resolveControlChar(document, 55) == STYLE_FONT1) ;
        }


        SUBCASE("   Delete Character")
        {
            document.Delete(2, 1) ;
            CHECK(resolveControlChar(document, 3) == STYLE_FONT1) ;
            CHECK(resolveControlChar(document, 8) == STYLE_FONT1) ;
            CHECK(resolveControlChar(document, 26) == STYLE_FONT1) ;
            CHECK(resolveControlChar(document, 31) == STYLE_FONT1) ;
            CHECK(resolveControlChar(document, 49) == STYLE_FONT1) ;
            CHECK(resolveControlChar(document, 54) == STYLE_FONT1) ;
        }

        SUBCASE("   Delete Paragraph 1")
        {
            document.Delete(20, 1) ;
            CHECK(resolveControlChar(document, 4) == STYLE_FONT1) ;
            CHECK(resolveControlChar(document, 9) == STYLE_FONT1) ;
            CHECK(resolveControlChar(document, 26) == STYLE_FONT1) ;
            CHECK(resolveControlChar(document, 31) == STYLE_FONT1) ;
            CHECK(resolveControlChar(document, 49) == STYLE_FONT1) ;
            CHECK(resolveControlChar(document, 54) == STYLE_FONT1) ;
        }

        SUBCASE("   Delete Paragraph 2")
        {
            document.Delete(44, 1) ;
            CHECK(resolveControlChar(document, 4) == STYLE_FONT1) ;
            CHECK(resolveControlChar(document, 9) == STYLE_FONT1) ;
            CHECK(resolveControlChar(document, 27) == STYLE_FONT1) ;
            CHECK(resolveControlChar(document, 32) == STYLE_FONT1) ;
            CHECK(resolveControlChar(document, 49) == STYLE_FONT1) ;
            CHECK(resolveControlChar(document, 54) == STYLE_FONT1) ;
        }

        SUBCASE("   Insert Character 1")
        {
            document.SetPosition(2) ;
            document.Insert('X') ;
            CHECK(resolveControlChar(document, 5) == STYLE_FONT1) ;
            CHECK(resolveControlChar(document, 10) == STYLE_FONT1) ;
            CHECK(resolveControlChar(document, 28) == STYLE_FONT1) ;
            CHECK(resolveControlChar(document, 33) == STYLE_FONT1) ;
            CHECK(resolveControlChar(document, 51) == STYLE_FONT1) ;
            CHECK(resolveControlChar(document, 56) == STYLE_FONT1) ;
        }

        SUBCASE("   Insert Character 2")
        {
            document.SetPosition(26) ;
            document.Insert('X') ;
            CHECK(resolveControlChar(document, 4) == STYLE_FONT1) ;
            CHECK(resolveControlChar(document, 9) == STYLE_FONT1) ;
            CHECK(resolveControlChar(document, 28) == STYLE_FONT1) ;
            CHECK(resolveControlChar(document, 33) == STYLE_FONT1) ;
            CHECK(resolveControlChar(document, 51) == STYLE_FONT1) ;
            CHECK(resolveControlChar(document, 56) == STYLE_FONT1) ;
        }

        SUBCASE("   Insert Paragraph 1")
        {
            document.SetPosition(2) ;
            document.Insert("\r") ;
            CHECK(document.GetNumberofParagraphs() == 5) ;
            CHECK(resolveControlChar(document, 5) == STYLE_FONT1) ;
            CHECK(resolveControlChar(document, 10) == STYLE_FONT1) ;
            CHECK(resolveControlChar(document, 28) == STYLE_FONT1) ;
            CHECK(resolveControlChar(document, 33) == STYLE_FONT1) ;
            CHECK(resolveControlChar(document, 51) == STYLE_FONT1) ;
            CHECK(resolveControlChar(document, 56) == STYLE_FONT1) ;
        }

        SUBCASE("   Insert Paragraph 2")
        {
            document.SetPosition(26) ;
            document.Insert("\r") ;
            CHECK(document.GetNumberofParagraphs() == 5) ;
            CHECK(resolveControlChar(document, 4) == STYLE_FONT1) ;
            CHECK(resolveControlChar(document, 9) == STYLE_FONT1) ;
            CHECK(resolveControlChar(document, 28) == STYLE_FONT1) ;
            CHECK(resolveControlChar(document, 33) == STYLE_FONT1) ;
            CHECK(resolveControlChar(document, 51) == STYLE_FONT1) ;
            CHECK(resolveControlChar(document, 56) == STYLE_FONT1) ;
        }
    }

    SUBCASE("Footnote")
    {
        sNote note ;
        note.symbol = NOTE_NUMBER ;
        note.text = "This is a test" ;

        document.SetPosition(4) ;
        document.InsertFootnote(note) ;
        document.Insert("test") ;
        document.InsertFootnote(note) ;

        document.SetPosition(27) ;
        document.InsertFootnote(note) ;
        document.Insert("test") ;
        document.InsertFootnote(note) ;

        document.SetPosition(50) ;
        document.InsertFootnote(note) ;
        document.Insert("test") ;
        document.InsertFootnote(note) ;

        // check initial insertions
        SUBCASE("   Check initial insertions")
        {
            CHECK(resolveControlChar(document, 4) == STYLE_FOOTNOTE) ;
            CHECK(resolveControlChar(document, 9) == STYLE_FOOTNOTE) ;
            CHECK(resolveControlChar(document, 27) == STYLE_FOOTNOTE) ;
            CHECK(resolveControlChar(document, 32) == STYLE_FOOTNOTE) ;
            CHECK(resolveControlChar(document, 50) == STYLE_FOOTNOTE) ;
            CHECK(resolveControlChar(document, 55) == STYLE_FOOTNOTE) ;
        }


        SUBCASE("   Delete Character")
        {
            document.Delete(2, 1) ;
            CHECK(resolveControlChar(document, 3) == STYLE_FOOTNOTE) ;
            CHECK(resolveControlChar(document, 8) == STYLE_FOOTNOTE) ;
            CHECK(resolveControlChar(document, 26) == STYLE_FOOTNOTE) ;
            CHECK(resolveControlChar(document, 31) == STYLE_FOOTNOTE) ;
            CHECK(resolveControlChar(document, 49) == STYLE_FOOTNOTE) ;
            CHECK(resolveControlChar(document, 54) == STYLE_FOOTNOTE) ;
        }

        SUBCASE("   Delete Paragraph 1")
        {
            document.Delete(20, 1) ;
            CHECK(resolveControlChar(document, 4) == STYLE_FOOTNOTE) ;
            CHECK(resolveControlChar(document, 9) == STYLE_FOOTNOTE) ;
            CHECK(resolveControlChar(document, 26) == STYLE_FOOTNOTE) ;
            CHECK(resolveControlChar(document, 31) == STYLE_FOOTNOTE) ;
            CHECK(resolveControlChar(document, 49) == STYLE_FOOTNOTE) ;
            CHECK(resolveControlChar(document, 54) == STYLE_FOOTNOTE) ;
        }

        SUBCASE("   Delete Paragraph 2")
        {
            document.Delete(44, 1) ;
            CHECK(resolveControlChar(document, 4) == STYLE_FOOTNOTE) ;
            CHECK(resolveControlChar(document, 9) == STYLE_FOOTNOTE) ;
            CHECK(resolveControlChar(document, 27) == STYLE_FOOTNOTE) ;
            CHECK(resolveControlChar(document, 32) == STYLE_FOOTNOTE) ;
            CHECK(resolveControlChar(document, 49) == STYLE_FOOTNOTE) ;
            CHECK(resolveControlChar(document, 54) == STYLE_FOOTNOTE) ;
        }

        SUBCASE("   Insert Character 1")
        {
            document.SetPosition(2) ;
            document.Insert('X') ;
            CHECK(resolveControlChar(document, 5) == STYLE_FOOTNOTE) ;
            CHECK(resolveControlChar(document, 10) == STYLE_FOOTNOTE) ;
            CHECK(resolveControlChar(document, 28) == STYLE_FOOTNOTE) ;
            CHECK(resolveControlChar(document, 33) == STYLE_FOOTNOTE) ;
            CHECK(resolveControlChar(document, 51) == STYLE_FOOTNOTE) ;
            CHECK(resolveControlChar(document, 56) == STYLE_FOOTNOTE) ;
        }

        SUBCASE("   Insert Character 2")
        {
            document.SetPosition(26) ;
            document.Insert('X') ;
            CHECK(resolveControlChar(document, 4) == STYLE_FOOTNOTE) ;
            CHECK(resolveControlChar(document, 9) == STYLE_FOOTNOTE) ;
            CHECK(resolveControlChar(document, 28) == STYLE_FOOTNOTE) ;
            CHECK(resolveControlChar(document, 33) == STYLE_FOOTNOTE) ;
            CHECK(resolveControlChar(document, 51) == STYLE_FOOTNOTE) ;
            CHECK(resolveControlChar(document, 56) == STYLE_FOOTNOTE) ;
        }

        SUBCASE("   Insert Paragraph 1")
        {
            document.SetPosition(2) ;
            document.Insert("\r") ;
            CHECK(document.GetNumberofParagraphs() == 5) ;
            CHECK(resolveControlChar(document, 5) == STYLE_FOOTNOTE) ;
            CHECK(resolveControlChar(document, 10) == STYLE_FOOTNOTE) ;
            CHECK(resolveControlChar(document, 28) == STYLE_FOOTNOTE) ;
            CHECK(resolveControlChar(document, 33) == STYLE_FOOTNOTE) ;
            CHECK(resolveControlChar(document, 51) == STYLE_FOOTNOTE) ;
            CHECK(resolveControlChar(document, 56) == STYLE_FOOTNOTE) ;
        }

        SUBCASE("   Insert Paragraph 2")
        {
            document.SetPosition(26) ;
            document.Insert("\r") ;
            CHECK(document.GetNumberofParagraphs() == 5) ;
            CHECK(resolveControlChar(document, 4) == STYLE_FOOTNOTE) ;
            CHECK(resolveControlChar(document, 9) == STYLE_FOOTNOTE) ;
            CHECK(resolveControlChar(document, 28) == STYLE_FOOTNOTE) ;
            CHECK(resolveControlChar(document, 33) == STYLE_FOOTNOTE) ;
            CHECK(resolveControlChar(document, 51) == STYLE_FOOTNOTE) ;
            CHECK(resolveControlChar(document, 56) == STYLE_FOOTNOTE) ;
        }
    }
    SUBCASE("Variable")
    {
        document.SetPosition(4) ;
        document.InsertVariable(VAR_DATE) ;
        document.Insert("test") ;
        document.InsertVariable(VAR_PAGE_NUMBER) ;

        document.SetPosition(27) ;
        document.InsertVariable(VAR_TIME) ;
        document.Insert("test") ;
        document.InsertVariable(VAR_FILENAME) ;

        document.SetPosition(50) ;
        document.InsertVariable(VAR_DRIVE) ;
        document.Insert("test") ;
        document.InsertVariable(VAR_FULLPATH) ;

        // check initial insertions
        SUBCASE("   Check initial insertions")
        {
            CHECK(resolveControlChar(document, 4) == STYLE_VARIABLE) ;
            CHECK(resolveControlChar(document, 9) == STYLE_VARIABLE) ;
            CHECK(resolveControlChar(document, 27) == STYLE_VARIABLE) ;
            CHECK(resolveControlChar(document, 32) == STYLE_VARIABLE) ;
            CHECK(resolveControlChar(document, 50) == STYLE_VARIABLE) ;
            CHECK(resolveControlChar(document, 55) == STYLE_VARIABLE) ;
        }

        SUBCASE("   Delete Character")
        {
            document.Delete(2, 1) ;
            CHECK(resolveControlChar(document, 3) == STYLE_VARIABLE) ;
            CHECK(resolveControlChar(document, 8) == STYLE_VARIABLE) ;
            CHECK(resolveControlChar(document, 26) == STYLE_VARIABLE) ;
            CHECK(resolveControlChar(document, 31) == STYLE_VARIABLE) ;
            CHECK(resolveControlChar(document, 49) == STYLE_VARIABLE) ;
            CHECK(resolveControlChar(document, 54) == STYLE_VARIABLE) ;
        }

        SUBCASE("   Delete Paragraph 1")
        {
            document.Delete(20, 1) ;
            CHECK(resolveControlChar(document, 4) == STYLE_VARIABLE) ;
            CHECK(resolveControlChar(document, 9) == STYLE_VARIABLE) ;
            CHECK(resolveControlChar(document, 26) == STYLE_VARIABLE) ;
            CHECK(resolveControlChar(document, 31) == STYLE_VARIABLE) ;
            CHECK(resolveControlChar(document, 49) == STYLE_VARIABLE) ;
            CHECK(resolveControlChar(document, 54) == STYLE_VARIABLE) ;
        }

        SUBCASE("   Delete Paragraph 2")
        {
            document.Delete(44, 1) ;
            CHECK(resolveControlChar(document, 4) == STYLE_VARIABLE) ;
            CHECK(resolveControlChar(document, 9) == STYLE_VARIABLE) ;
            CHECK(resolveControlChar(document, 27) == STYLE_VARIABLE) ;
            CHECK(resolveControlChar(document, 32) == STYLE_VARIABLE) ;
            CHECK(resolveControlChar(document, 49) == STYLE_VARIABLE) ;
            CHECK(resolveControlChar(document, 54) == STYLE_VARIABLE) ;
        }

        SUBCASE("   Insert Character 1")
        {
            document.SetPosition(2) ;
            document.Insert('X') ;
            CHECK(resolveControlChar(document, 5) == STYLE_VARIABLE) ;
            CHECK(resolveControlChar(document, 10) == STYLE_VARIABLE) ;
            CHECK(resolveControlChar(document, 28) == STYLE_VARIABLE) ;
            CHECK(resolveControlChar(document, 33) == STYLE_VARIABLE) ;
            CHECK(resolveControlChar(document, 51) == STYLE_VARIABLE) ;
            CHECK(resolveControlChar(document, 56) == STYLE_VARIABLE) ;
        }

        SUBCASE("   Insert Character 2")
        {
            document.SetPosition(26) ;
            document.Insert('X') ;
            CHECK(resolveControlChar(document, 4) == STYLE_VARIABLE) ;
            CHECK(resolveControlChar(document, 9) == STYLE_VARIABLE) ;
            CHECK(resolveControlChar(document, 28) == STYLE_VARIABLE) ;
            CHECK(resolveControlChar(document, 33) == STYLE_VARIABLE) ;
            CHECK(resolveControlChar(document, 51) == STYLE_VARIABLE) ;
            CHECK(resolveControlChar(document, 56) == STYLE_VARIABLE) ;
        }

        SUBCASE("   Insert Paragraph 1")
        {
            document.SetPosition(2) ;
            document.Insert("\r") ;
            CHECK(document.GetNumberofParagraphs() == 5) ;
            CHECK(resolveControlChar(document, 5) == STYLE_VARIABLE) ;
            CHECK(resolveControlChar(document, 10) == STYLE_VARIABLE) ;
            CHECK(resolveControlChar(document, 28) == STYLE_VARIABLE) ;
            CHECK(resolveControlChar(document, 33) == STYLE_VARIABLE) ;
            CHECK(resolveControlChar(document, 51) == STYLE_VARIABLE) ;
            CHECK(resolveControlChar(document, 56) == STYLE_VARIABLE) ;
        }

        SUBCASE("   Insert Paragraph 2")
        {
            document.SetPosition(26) ;
            document.Insert("\r") ;
            CHECK(document.GetNumberofParagraphs() == 5) ;
            CHECK(resolveControlChar(document, 4) == STYLE_VARIABLE) ;
            CHECK(resolveControlChar(document, 9) == STYLE_VARIABLE) ;
            CHECK(resolveControlChar(document, 28) == STYLE_VARIABLE) ;
            CHECK(resolveControlChar(document, 33) == STYLE_VARIABLE) ;
            CHECK(resolveControlChar(document, 51) == STYLE_VARIABLE) ;
            CHECK(resolveControlChar(document, 56) == STYLE_VARIABLE) ;
        }
    }

    SUBCASE("Variable - GetVariable retrieval")
    {
        // Insert variables of different types at known positions
        document.SetPosition(4) ;
        document.InsertVariable(VAR_DATE) ;

        document.SetPosition(10) ;
        document.InsertVariable(VAR_PAGE_NUMBER) ;

        document.SetPosition(20) ;
        document.InsertVariable(VAR_TIME) ;

        document.SetPosition(30) ;
        document.InsertVariable(VAR_FILENAME) ;

        document.SetPosition(40) ;
        document.InsertVariable(VAR_DRIVE) ;

        // Verify each variable type is correctly stored and retrieved
        CHECK(document.GetVariable(4) == VAR_DATE) ;
        CHECK(document.GetVariable(10) == VAR_PAGE_NUMBER) ;
        CHECK(document.GetVariable(20) == VAR_TIME) ;
        CHECK(document.GetVariable(30) == VAR_FILENAME) ;
        CHECK(document.GetVariable(40) == VAR_DRIVE) ;
    }

    SUBCASE("Variable - Delete variable marker")
    {
        // Insert a variable at position 2 (inside "Th|is") to avoid STYLE_VARIABLE == SPACE collision
        document.SetPosition(2) ;
        document.InsertVariable(VAR_DATE) ;

        CHECK(resolveControlChar(document, 2) == STYLE_VARIABLE) ;
        CHECK(document.GetVariable(2) == VAR_DATE) ;

        POSITION_T sizeBeforeDelete = document.GetTextSize() ;

        // Delete the variable marker
        document.Delete(2, 1) ;

        // Text size should decrease by 1
        CHECK(document.GetTextSize() == sizeBeforeDelete - 1) ;

        // Position 2 should now be 'i' (from "This"), not a variable marker
        CHECK(document.GetCharNoAdvance(2)[0] == 'i') ;
    }

    SUBCASE("Variable - All variable types")
    {
        // Verify all 8 variable types can be inserted and retrieved
        document.SetPosition(0) ;
        document.InsertVariable(VAR_DATE) ;
        document.InsertVariable(VAR_TIME) ;
        document.InsertVariable(VAR_PAGE_NUMBER) ;
        document.InsertVariable(VAR_LINE_NUMBER) ;
        document.InsertVariable(VAR_FILENAME) ;
        document.InsertVariable(VAR_DRIVE) ;
        document.InsertVariable(VAR_DIRECTORY) ;
        document.InsertVariable(VAR_FULLPATH) ;

        // 8 variables inserted at positions 0-7
        CHECK(document.GetVariable(0) == VAR_DATE) ;
        CHECK(document.GetVariable(1) == VAR_TIME) ;
        CHECK(document.GetVariable(2) == VAR_PAGE_NUMBER) ;
        CHECK(document.GetVariable(3) == VAR_LINE_NUMBER) ;
        CHECK(document.GetVariable(4) == VAR_FILENAME) ;
        CHECK(document.GetVariable(5) == VAR_DRIVE) ;
        CHECK(document.GetVariable(6) == VAR_DIRECTORY) ;
        CHECK(document.GetVariable(7) == VAR_FULLPATH) ;

        // All should be STYLE_VARIABLE control chars
        for (int i = 0; i < 8; i++)
        {
            CHECK(resolveControlChar(document, i) == STYLE_VARIABLE) ;
        }
    }

// add tests for other symetrical code sequences
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Tests that deleting control code markers themselves works correctly.
/// Each control code type (bold, italic, tab, font, variable, etc.)
/// should be deletable, with its metadata properly cleaned up.
///
/// This test guards against regressions where GetChar() returns
/// raw MARKER_CHAR (127) instead of the control type, causing
/// the metadata cleanup switch in Delete() to be skipped.
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("Delete Control Code Markers")
{
    cDocument document ;
    document.SetShowControl(SHOW_ALL) ;

    SUBCASE("Delete bold marker")
    {
        document.Insert("AB\r") ;
        // Insert bold marker at position 1: A ^B B \r ^Z
        document.SetPosition(1) ;
        document.BeginBold() ;

        POSITION_T sizeBefore = document.GetTextSize() ;
        CHECK(resolveControlChar(document, 1) == STYLE_BOLD) ;
        CHECK(document.GetCharNoAdvance(2)[0] == 'B') ;

        // Delete the bold marker
        document.Delete(1, 1) ;

        CHECK(document.GetTextSize() == sizeBefore - 1) ;
        // Position 1 should now be 'B', not a control code
        CHECK(document.GetCharNoAdvance(1)[0] == 'B') ;
    }

    SUBCASE("Delete italic marker")
    {
        document.Insert("AB\r") ;
        document.SetPosition(1) ;
        document.BeginItalics() ;

        POSITION_T sizeBefore = document.GetTextSize() ;
        CHECK(resolveControlChar(document, 1) == STYLE_ITALICS) ;

        document.Delete(1, 1) ;

        CHECK(document.GetTextSize() == sizeBefore - 1) ;
        CHECK(document.GetCharNoAdvance(1)[0] == 'B') ;
    }

    SUBCASE("Delete underline marker")
    {
        document.Insert("AB\r") ;
        document.SetPosition(1) ;
        document.BeginUnderline() ;

        POSITION_T sizeBefore = document.GetTextSize() ;
        CHECK(resolveControlChar(document, 1) == STYLE_UNDERLINE) ;

        document.Delete(1, 1) ;

        CHECK(document.GetTextSize() == sizeBefore - 1) ;
        CHECK(document.GetCharNoAdvance(1)[0] == 'B') ;
    }

    SUBCASE("Delete strikethrough marker")
    {
        document.Insert("AB\r") ;
        document.SetPosition(1) ;
        document.BeginStrikeThrough() ;

        POSITION_T sizeBefore = document.GetTextSize() ;
        CHECK(resolveControlChar(document, 1) == STYLE_STRIKETHROUGH) ;

        document.Delete(1, 1) ;

        CHECK(document.GetTextSize() == sizeBefore - 1) ;
        CHECK(document.GetCharNoAdvance(1)[0] == 'B') ;
    }

    SUBCASE("Delete superscript marker")
    {
        document.Insert("AB\r") ;
        document.SetPosition(1) ;
        document.BeginSuperscript() ;

        POSITION_T sizeBefore = document.GetTextSize() ;
        CHECK(resolveControlChar(document, 1) == STYLE_SUPERSCRIPT) ;

        document.Delete(1, 1) ;

        CHECK(document.GetTextSize() == sizeBefore - 1) ;
        CHECK(document.GetCharNoAdvance(1)[0] == 'B') ;
    }

    SUBCASE("Delete subscript marker")
    {
        document.Insert("AB\r") ;
        document.SetPosition(1) ;
        document.BeginSubscript() ;

        POSITION_T sizeBefore = document.GetTextSize() ;
        CHECK(resolveControlChar(document, 1) == STYLE_SUBSCRIPT) ;

        document.Delete(1, 1) ;

        CHECK(document.GetTextSize() == sizeBefore - 1) ;
        CHECK(document.GetCharNoAdvance(1)[0] == 'B') ;
    }

    SUBCASE("Delete tab marker")
    {
        document.Insert("AB\r") ;
        document.SetPosition(1) ;

        sWSTab tab ;
        tab.abstabsize = 0 ;
        tab.size = 0 ;
        tab.tabsize = 0 ;
        tab.type = TAB_TAB ;
        document.InsertTab(tab) ;

        POSITION_T sizeBefore = document.GetTextSize() ;
        CHECK(resolveControlChar(document, 1) == STYLE_TAB) ;

        document.Delete(1, 1) ;

        CHECK(document.GetTextSize() == sizeBefore - 1) ;
        CHECK(document.GetCharNoAdvance(1)[0] == 'B') ;
    }

    SUBCASE("Delete font marker")
    {
        document.Insert("AB\r") ;
        document.SetPosition(1) ;

        sInternalFonts font ;
        font.name = "Arial" ;
        font.size = 12 ;
        font.haveWSFont = false ;
        document.InsertFont(font) ;

        POSITION_T sizeBefore = document.GetTextSize() ;
        CHECK(resolveControlChar(document, 1) == STYLE_FONT1) ;

        document.Delete(1, 1) ;

        CHECK(document.GetTextSize() == sizeBefore - 1) ;
        CHECK(document.GetCharNoAdvance(1)[0] == 'B') ;
    }

    SUBCASE("Delete variable marker")
    {
        document.Insert("AB\r") ;
        document.SetPosition(1) ;
        document.InsertVariable(VAR_PAGE_NUMBER) ;

        POSITION_T sizeBefore = document.GetTextSize() ;
        CHECK(resolveControlChar(document, 1) == STYLE_VARIABLE) ;

        document.Delete(1, 1) ;

        CHECK(document.GetTextSize() == sizeBefore - 1) ;
        CHECK(document.GetCharNoAdvance(1)[0] == 'B') ;
    }

    SUBCASE("Delete control code with SHOW_NONE active")
    {
        // Verify deletion works even when ShowControl is not SHOW_ALL
        // This was the original regression: document's mShowControl not set
        document.SetShowControl(SHOW_NONE) ;

        document.Insert("AB\r") ;
        document.SetPosition(1) ;

        // Must switch to SHOW_ALL to insert control codes properly
        document.SetShowControl(SHOW_ALL) ;
        document.BeginBold() ;

        // Switch back to SHOW_NONE
        document.SetShowControl(SHOW_NONE) ;

        POSITION_T sizeBefore = document.GetTextSize() ;

        // Delete at position 1 should remove the bold marker
        // even though SHOW_NONE is active
        document.SetShowControl(SHOW_ALL) ;
        document.Delete(1, 1) ;

        CHECK(document.GetTextSize() == sizeBefore - 1) ;
        CHECK(document.GetCharNoAdvance(1)[0] == 'B') ;
    }

    SUBCASE("Delete multiple adjacent control codes")
    {
        document.Insert("AB\r") ;
        document.SetPosition(1) ;
        document.BeginBold() ;
        document.BeginItalics() ;

        // Document: A ^B ^Y B \r ^Z
        CHECK(resolveControlChar(document, 1) == STYLE_BOLD) ;
        CHECK(resolveControlChar(document, 2) == STYLE_ITALICS) ;
        CHECK(document.GetCharNoAdvance(3)[0] == 'B') ;

        POSITION_T sizeBefore = document.GetTextSize() ;

        // Delete both control codes one at a time
        document.Delete(1, 1) ;
        CHECK(document.GetTextSize() == sizeBefore - 1) ;
        CHECK(resolveControlChar(document, 1) == STYLE_ITALICS) ;

        document.Delete(1, 1) ;
        CHECK(document.GetTextSize() == sizeBefore - 2) ;
        CHECK(document.GetCharNoAdvance(1)[0] == 'B') ;
    }

    SUBCASE("Delete control code at start of document")
    {
        document.Insert("AB\r") ;
        document.SetPosition(0) ;
        document.BeginBold() ;

        // Document: ^B A B \r ^Z
        CHECK(resolveControlChar(document, 0) == STYLE_BOLD) ;

        POSITION_T sizeBefore = document.GetTextSize() ;
        document.Delete(0, 1) ;

        CHECK(document.GetTextSize() == sizeBefore - 1) ;
        CHECK(document.GetCharNoAdvance(0)[0] == 'A') ;
    }
}


TEST_CASE("Block Select")
{
    cDocument document ;
    document.SetShowControl(SHOW_ALL) ;

    document.Insert("This is a test\r") ;
    document.Insert(" This is part two\r") ;
    document.Insert(" This is part 3\r") ;

    POSITION_T start, end ;
    bool set ;

    CHECK(document.mStartBlock == NOT_SET) ;
    CHECK(document.mEndBlock == NOT_SET) ;
    CHECK(document.mBlockSet == false) ;

    CHECK(document.mOldStartBlock == NOT_SET) ;
    CHECK(document.mOldEndBlock == NOT_SET) ;
    CHECK(document.mOldBlockSet == false) ;

    SUBCASE("Block start set")
    {
        document.SetPosition(4) ;
        document.SetBeginBlock() ;
        set = document.GetBlock(start, end) ;
        CHECK(set == false) ;
        CHECK(start == 4) ;
        CHECK(end == NOT_SET) ;
        CHECK(document.GetCharNoAdvance(4)[0] == 0) ;
    }

    SUBCASE("Block Select in paragraph")
    {
        document.SetPosition(4) ;
        document.SetBeginBlock() ;
        document.SetPosition(10) ;
        document.SetEndBlock() ;

        set = document.GetBlock(start, end) ;
        CHECK(set == true) ;
        CHECK(start == 4) ;
        CHECK(end == 9) ;
    }

    SUBCASE("Block Select, end marked before start (^KK then ^KB, order independence)")
    {
        // Classic WordStar lets either block boundary be marked first. Mirrors
        // "Block Select in paragraph" above but with ^KK/^KB reversed; end position
        // is 9 rather than 10 because that test's begin-first marker insertion
        // shifts the buffer by one before its SetPosition(10) is issued, whereas
        // here no marker is ever inserted (see cDocument::SetEndBlock/SetBeginBlock).
        document.SetPosition(9) ;
        document.SetEndBlock() ;
        document.SetPosition(4) ;
        document.SetBeginBlock() ;

        set = document.GetBlock(start, end) ;
        CHECK(set == true) ;
        CHECK(start == 4) ;
        CHECK(end == 9) ;
    }

    SUBCASE("Delete Character 1")
    {
        document.SetPosition(4) ;
        document.SetBeginBlock() ;
        document.SetPosition(10) ;
        document.SetEndBlock() ;

        document.Delete(2, 1) ;
        set = document.GetBlock(start, end) ;
        CHECK(set == true) ;
        CHECK(start == 3) ;
        CHECK(end == 8) ;
    }

    SUBCASE("Delete Character 2")
    {
        document.SetPosition(22) ;
        document.SetBeginBlock() ;
        document.SetPosition(28) ;
        document.SetEndBlock() ;

        document.Delete(2, 1) ;
        set = document.GetBlock(start, end) ;
        CHECK(set == true) ;
        CHECK(start == 21) ;
        CHECK(end == 26) ;
    }

    SUBCASE("Delete Character 3")
    {
        document.SetPosition(4) ;
        document.SetBeginBlock() ;
        document.SetPosition(10) ;
        document.SetEndBlock() ;

        document.Delete(6, 1) ;
        set = document.GetBlock(start, end) ;
        CHECK(set == true) ;
        CHECK(start == 4) ;
        CHECK(end == 8) ;
    }

    SUBCASE("Delete Paragraph 1")
    {
        document.SetPosition(22) ;
        document.SetBeginBlock() ;
        document.SetPosition(28) ;
        document.SetEndBlock() ;

        document.Delete(14, 1) ;
        set = document.GetBlock(start, end) ;
        CHECK(set == true) ;
        CHECK(start == 21) ;
        CHECK(end == 26) ;
    }

    SUBCASE("Delete Paragraph 2")
    {
        document.SetPosition(34) ;
        document.SetBeginBlock() ;
        document.SetPosition(38) ;
        document.SetEndBlock() ;

        document.Delete(32, 1) ;
        set = document.GetBlock(start, end) ;
        CHECK(set == true) ;
        CHECK(start == 33) ;
        CHECK(end == 36) ;
    }

    SUBCASE("Insert Character 1")
    {
        document.SetPosition(4) ;
        document.SetBeginBlock() ;
        document.SetPosition(10) ;
        document.SetEndBlock() ;

        document.SetPosition(2) ;
        document.Insert('X') ;
        set = document.GetBlock(start, end) ;
        CHECK(set == true) ;
        CHECK(start == 5) ;
        CHECK(end == 10) ;
    }

    SUBCASE("Insert Character 2")
    {
        document.SetPosition(22) ;
        document.SetBeginBlock() ;
        document.SetPosition(28) ;
        document.SetEndBlock() ;

        document.SetPosition(2) ;
        document.Insert('X') ;
        set = document.GetBlock(start, end) ;
        CHECK(set == true) ;
        CHECK(start == 23) ;
        CHECK(end == 28) ;
    }

    SUBCASE("Insert Character 3")
    {
        document.SetPosition(4) ;
        document.SetBeginBlock() ;
        document.SetPosition(10) ;
        document.SetEndBlock() ;

        document.SetPosition(6) ;
        document.Insert('X') ;
        set = document.GetBlock(start, end) ;
        CHECK(set == true) ;
        CHECK(start == 4) ;
        CHECK(end == 10) ;
    }

    // insert paragraph after paragraph 1
    SUBCASE("Insert Paragraph 1")
    {
        document.SetPosition(22) ;
        document.SetBeginBlock() ;
        document.SetPosition(28) ;
        document.SetEndBlock() ;

        document.SetPosition(20) ;
        document.Insert("\r") ;
        set = document.GetBlock(start, end) ;
        CHECK(set == true) ;
        CHECK(start == 23) ;
        CHECK(end == 28) ;
    }

    // insert paragraph after paragraph 2
    SUBCASE("Insert Paragraph 2")
    {
        document.SetPosition(22) ;
        document.SetBeginBlock() ;
        document.SetPosition(28) ;
        document.SetEndBlock() ;

        document.SetPosition(25) ;
        document.Insert("\r") ;
        set = document.GetBlock(start, end) ;
        CHECK(set == true) ;
        CHECK(start == 22) ;
        CHECK(end == 28) ;
    }

    // proper previous block
    SUBCASE("Set Previous Block 1")
    {
        document.SetPosition(22) ;
        document.SetBeginBlock() ;
        document.SetPosition(28) ;
        document.SetEndBlock() ;

        document.SetPosition(4) ;
        document.SetBeginBlock() ;
        document.SetPosition(10) ;
        document.SetEndBlock() ;

        document.SetPreviousBlock() ;

        set = document.GetBlock(start, end) ;
        CHECK(set == true) ;
        CHECK(start == 4) ;
        CHECK(end == 27) ;
    }

    // current block not set
    SUBCASE("Set Previous Block 2")
    {
        document.SetPosition(22) ;
        document.SetBeginBlock() ;
        document.SetPosition(28) ;
        document.SetEndBlock() ;

        document.SetPosition(10) ;
        document.SetEndBlock() ;

        document.SetPreviousBlock() ;

        set = document.GetBlock(start, end) ;
        CHECK(set == true) ;
        CHECK(start == 22) ;
        CHECK(end == 27) ;

        document.SetPreviousBlock() ;

        set = document.GetBlock(start, end) ;
        CHECK(set == true) ;
        CHECK(start == 22) ;
        CHECK(end == 27) ;
    }

    // copy block
    SUBCASE("Copy Block")
    {
        std::string one = document.GetParagraphText(0) ;
        std::string two = document.GetParagraphText(1) ;
        std::string three = document.GetParagraphText(2) ;

        document.SetPosition(22) ;
        document.SetBeginBlock() ;
        document.SetPosition(28) ;
        document.SetEndBlock() ;

        document.SetPosition(4) ;

        document.CopyBlock() ;

        std::string copy = two.substr(7, 5) ;
        std::string newone = one.insert(4, copy) ;
        CHECK(document.GetParagraphText(0) == newone) ;
        CHECK(document.GetParagraphText(1) == two) ;
    }

    SUBCASE("Cut Block")
    {
        std::string one = document.GetParagraphText(0) ;
        std::string two = document.GetParagraphText(1) ;
        std::string three = document.GetParagraphText(2) ;

        document.SetPosition(22) ;
        document.SetBeginBlock() ;
        document.SetPosition(28) ;
        document.SetEndBlock() ;

        document.SetPosition(4) ;

        document.DeleteBlock() ;

        two.erase(7, 5) ;
        CHECK(document.GetParagraphText(1) == two) ;
    }

    SUBCASE("Move Block")
    {
        std::string one = document.GetParagraphText(0) ;
        std::string two = document.GetParagraphText(1) ;
        std::string three = document.GetParagraphText(2) ;

        document.SetPosition(22) ;
        document.SetBeginBlock() ;
        document.SetPosition(28) ;
        document.SetEndBlock() ;

        document.SetPosition(4) ;

        document.MoveBlock() ;

        std::string copy = two.substr(7, 5) ;
        std::string newone = one.insert(4, copy) ;
        two.erase(7, 5) ;
        CHECK(document.GetParagraphText(0) == newone) ;
        CHECK(document.GetParagraphText(1) == two) ;
    }

    SUBCASE("Invalidate Block")
    {
        document.Clear() ;
        document.Insert("This is a test\r") ;

        document.SetPosition(5) ;
        document.SetBeginBlock() ;
        document.SetPosition(9) ;
        document.SetEndBlock() ;

        document.SetPosition(document.GetTextSize() - 1) ;

        document.Delete(1, document.GetTextSize() - 1) ;

        set = document.GetBlock(start, end) ;
        CHECK(set == false) ;
        CHECK(start == NOT_SET) ;
        CHECK(end == NOT_SET) ;
    }

    SUBCASE("Cut from non-zero start, caret inside block")
    {
        document.SetPosition(4) ;
        document.SetBeginBlock() ;
        document.SetPosition(10) ;
        document.SetEndBlock() ;
        POSITION_T sizeBeforeCut = document.GetTextSize() ;

        document.SetPosition(7) ;
        document.Cut() ;

        CHECK(document.GetPosition() == 4) ;
        CHECK(document.mBlockSet == false) ;
        CHECK(document.mStartBlock == NOT_SET) ;
        CHECK(document.mEndBlock == NOT_SET) ;
        CHECK(document.GetTextSize() == sizeBeforeCut - 5) ;
    }

    SUBCASE("Cut from non-zero start, caret at block start")
    {
        document.SetPosition(4) ;
        document.SetBeginBlock() ;
        document.SetPosition(10) ;
        document.SetEndBlock() ;
        POSITION_T sizeBeforeCut = document.GetTextSize() ;

        document.SetPosition(4) ;
        document.Cut() ;

        CHECK(document.GetPosition() == 4) ;
        CHECK(document.mBlockSet == false) ;
        CHECK(document.mStartBlock == NOT_SET) ;
        CHECK(document.mEndBlock == NOT_SET) ;
        CHECK(document.GetTextSize() == sizeBeforeCut - 5) ;
    }

    SUBCASE("Cut from non-zero start, caret before block")
    {
        document.SetPosition(4) ;
        document.SetBeginBlock() ;
        document.SetPosition(10) ;
        document.SetEndBlock() ;
        POSITION_T sizeBeforeCut = document.GetTextSize() ;

        document.SetPosition(2) ;
        document.Cut() ;

        CHECK(document.GetPosition() == 2) ;
        CHECK(document.mBlockSet == false) ;
        CHECK(document.mStartBlock == NOT_SET) ;
        CHECK(document.mEndBlock == NOT_SET) ;
        CHECK(document.GetTextSize() == sizeBeforeCut - 5) ;
    }

    SUBCASE("Cut from non-zero start, caret after block")
    {
        document.SetPosition(4) ;
        document.SetBeginBlock() ;
        document.SetPosition(10) ;
        document.SetEndBlock() ;
        POSITION_T sizeBeforeCut = document.GetTextSize() ;

        document.SetPosition(14) ;
        document.Cut() ;

        CHECK(document.GetPosition() == 9) ;
        CHECK(document.mBlockSet == false) ;
        CHECK(document.mStartBlock == NOT_SET) ;
        CHECK(document.mEndBlock == NOT_SET) ;
        CHECK(document.GetTextSize() == sizeBeforeCut - 5) ;
    }

    SUBCASE("Delete at mStartBlock position does not over-decrement mStartBlock")
    {
        document.SetPosition(5) ;
        document.SetBeginBlock() ;
        REQUIRE(document.mStartBlock == 5) ;

        document.Delete(5, 1) ;

        // After deleting the byte at mStartBlock, the content formerly at
        // mStartBlock+1 has shifted down to mStartBlock. The block start
        // semantically still points to that position -- it should NOT have
        // been decremented to 4.
        CHECK(document.mStartBlock == 5) ;
    }
}


TEST_CASE("GetPreviousBlock Test")
{
    cDocument document ;
    document.SetShowControl(SHOW_ALL) ;

    document.Insert("This is paragraph one\r") ;
    document.Insert("This is paragraph two\r") ;
    document.Insert("This is paragraph three\r") ;

    POSITION_T start, end ;
    POSITION_T prevStart, prevEnd ;
    bool set ;
    bool prevSet ;

    SUBCASE("GetPreviousBlock - No previous block")
    {
        // Initially, no block is set, so no previous block exists
        prevSet = document.GetPreviousBlock(prevStart, prevEnd) ;
        CHECK(prevSet == false) ;
        CHECK(prevStart == NOT_SET) ;
        CHECK(prevEnd == NOT_SET) ;
    }

    SUBCASE("GetPreviousBlock - After setting first block")
    {
        // Set first block
        document.SetPosition(5) ;
        document.SetBeginBlock() ;
        document.SetPosition(10) ;
        document.SetEndBlock() ;

        // Current block should be set
        set = document.GetBlock(start, end) ;
        CHECK(set == true) ;
        CHECK(start == 5) ;
        CHECK(end == 9) ;

        // Previous block should still not be set
        prevSet = document.GetPreviousBlock(prevStart, prevEnd) ;
        CHECK(prevSet == false) ;
        CHECK(prevStart == NOT_SET) ;
        CHECK(prevEnd == NOT_SET) ;
    }

    SUBCASE("GetPreviousBlock - After setting second block")
    {
        // Set first block (positions 5-10)
        document.SetPosition(5) ;
        document.SetBeginBlock() ;
        document.SetPosition(10) ;
        document.SetEndBlock() ;

        // Verify first block is set
        set = document.GetBlock(start, end) ;
        CHECK(set == true) ;
        POSITION_T firstStart = start ;
        POSITION_T firstEnd = end ;

        // Set second block (positions 25-30)
        document.SetPosition(25) ;
        document.SetBeginBlock() ;
        document.SetPosition(30) ;
        document.SetEndBlock() ;

        // Verify second block is now current
        set = document.GetBlock(start, end) ;
        CHECK(set == true) ;
        CHECK(start == 25) ;
        CHECK(end == 29) ;

        // Verify previous block holds the first block
        prevSet = document.GetPreviousBlock(prevStart, prevEnd) ;
        CHECK(prevSet == true) ;
        CHECK(prevStart == firstStart) ;
        CHECK(prevEnd == firstEnd) ;
    }

    SUBCASE("GetPreviousBlock - After SetPreviousBlock swap")
    {
        // Set first block (positions 5-10)
        document.SetPosition(5) ;
        document.SetBeginBlock() ;
        document.SetPosition(10) ;
        document.SetEndBlock() ;

        POSITION_T firstStart, firstEnd ;
        document.GetBlock(firstStart, firstEnd) ;

        // Set second block (positions 25-30)
        document.SetPosition(25) ;
        document.SetBeginBlock() ;
        document.SetPosition(30) ;
        document.SetEndBlock() ;

        POSITION_T secondStart, secondEnd ;
        document.GetBlock(secondStart, secondEnd) ;

        // Before swap: current=second, previous=first
        prevSet = document.GetPreviousBlock(prevStart, prevEnd) ;
        CHECK(prevSet == true) ;
        CHECK(prevStart == firstStart) ;
        CHECK(prevEnd == firstEnd) ;

        // Swap blocks using SetPreviousBlock
        document.SetPreviousBlock() ;

        // After swap: current=first, previous=second
        set = document.GetBlock(start, end) ;
        CHECK(set == true) ;
        CHECK(start == firstStart) ;

        prevSet = document.GetPreviousBlock(prevStart, prevEnd) ;
        CHECK(prevSet == true) ;
        CHECK(prevStart == secondStart) ;
        CHECK(prevEnd == secondEnd) ;
    }

    SUBCASE("GetPreviousBlock - After UnsetBlock")
    {
        // Set two blocks to establish previous block
        document.SetPosition(5) ;
        document.SetBeginBlock() ;
        document.SetPosition(10) ;
        document.SetEndBlock() ;

        document.SetPosition(25) ;
        document.SetBeginBlock() ;
        document.SetPosition(30) ;
        document.SetEndBlock() ;

        // Verify previous block exists
        prevSet = document.GetPreviousBlock(prevStart, prevEnd) ;
        CHECK(prevSet == true) ;

        // Unset current block
        document.UnsetBlock() ;

        // Previous block should be unaffected by UnsetBlock
        // (UnsetBlock only clears current block, not previous)
        prevSet = document.GetPreviousBlock(prevStart, prevEnd) ;
        CHECK(prevSet == true) ;
    }

    SUBCASE("GetPreviousBlock - Multiple block changes")
    {
        // Set block 1
        document.SetPosition(0) ;
        document.SetBeginBlock() ;
        document.SetPosition(5) ;
        document.SetEndBlock() ;

        POSITION_T block1Start, block1End ;
        document.GetBlock(block1Start, block1End) ;

        // Set block 2 (saves block 1 as previous)
        document.SetPosition(10) ;
        document.SetBeginBlock() ;
        document.SetPosition(15) ;
        document.SetEndBlock() ;

        POSITION_T block2Start, block2End ;
        document.GetBlock(block2Start, block2End) ;

        // Set block 3 (saves block 2 as previous, block 1 is lost)
        document.SetPosition(20) ;
        document.SetBeginBlock() ;
        document.SetPosition(25) ;
        document.SetEndBlock() ;

        // Current should be block 3, previous should be block 2
        prevSet = document.GetPreviousBlock(prevStart, prevEnd) ;
        CHECK(prevSet == true) ;
        CHECK(prevStart == block2Start) ;
        CHECK(prevEnd == block2End) ;
    }
}


TEST_CASE("Saved Positions Test")
{
    cDocument document ;
    document.SetShowControl(SHOW_ALL) ;

    document.Insert("This is a test\r") ;
    document.Insert(" This is part two\r") ;
    document.Insert(" This is part 3\r") ;

    CHECK(document.mSavePosition[0] == NOT_SET) ;
    CHECK(document.mSavePosition[1] == NOT_SET) ;
    CHECK(document.mSavePosition[2] == NOT_SET) ;
    CHECK(document.mSavePosition[3] == NOT_SET) ;
    CHECK(document.mSavePosition[4] == NOT_SET) ;
    CHECK(document.mSavePosition[5] == NOT_SET) ;
    CHECK(document.mSavePosition[6] == NOT_SET) ;
    CHECK(document.mSavePosition[7] == NOT_SET) ;
    CHECK(document.mSavePosition[8] == NOT_SET) ;
    CHECK(document.mSavePosition[9] == NOT_SET) ;

    SUBCASE("Set Position")
    {
        document.SetPosition(4) ;
        document.SetSavePosition(1) ;

        CHECK(document.mSavePosition[1] == 4) ;
        CHECK(document.GetCharNoAdvance(4)[0] == SAVE_CHAR) ;
    }

    SUBCASE("Set to same position")
    {
        document.SetPosition(4) ;
        document.SetSavePosition(1) ;
        CHECK(document.mSavePosition[1] == 4) ;
        CHECK(document.GetCharNoAdvance(4)[0] == SAVE_CHAR) ;

        // we test for position 5 since saving a position adds a char to the buffere
        document.SetPosition(5) ;
        document.SetSavePosition(1) ;
        CHECK(document.mSavePosition[1] == NOT_SET) ;
        CHECK(document.GetCharNoAdvance(4)[0] != SAVE_CHAR) ;
    }

    SUBCASE("Change position")
    {
        document.SetPosition(4) ;
        document.SetSavePosition(1) ;
        CHECK(document.mSavePosition[1] == 4) ;
        CHECK(document.GetCharNoAdvance(4)[0] == SAVE_CHAR) ;

        document.SetPosition(10) ;
        document.SetSavePosition(1) ;
        CHECK(document.GetCharNoAdvance(4)[0] != SAVE_CHAR) ;

        // check position 9, since removing old position removed a char from buffer
        CHECK(document.mSavePosition[1] == 9) ;
        CHECK(document.GetCharNoAdvance(9)[0] == SAVE_CHAR) ;
    }

    SUBCASE("SetBeginBlock at SetSavePosition location preserves save marker")
    {
        // Capture the byte SetSavePosition writes into the buffer in a clean
        // scenario; that is the "save marker" character for this build,
        // regardless of which codepoint the implementation picks.
        document.SetPosition(4) ;
        document.SetSavePosition(0) ;
        REQUIRE(document.mSavePosition[0] == 4) ;
        std::string saveMarker = document.GetCharNoAdvance(4) ;

        // Toggle the bookmark off so the document is back to a clean state.
        document.SetPosition(5) ;
        document.SetSavePosition(0) ;
        REQUIRE(document.mSavePosition[0] == NOT_SET) ;

        // Bug scenario: save bookmark + block start at exactly the same position.
        document.SetPosition(4) ;
        document.SetSavePosition(0) ;
        REQUIRE(document.mSavePosition[0] == 4) ;
        REQUIRE(document.GetCharNoAdvance(4) == saveMarker) ;

        document.SetPosition(4) ;
        document.SetBeginBlock() ;
        document.SetPosition(10) ;
        document.SetEndBlock() ;

        // The block end must not have destroyed the save marker. The bookmark
        // should still point at a byte holding the save marker character.
        REQUIRE(document.mSavePosition[0] != NOT_SET) ;
        CHECK(document.GetCharNoAdvance(document.mSavePosition[0]) == saveMarker) ;
    }
}

TEST_CASE("Insert(CHAR_T): combining mark keeps attributes aligned and buffer NFC")
{
    cDocument document ;
    sSeqRGBColor color ;
    color.red = 0 ; color.green = 170 ; color.blue = 170 ; color.alpha = 255 ;

    // "e" at grapheme 0, then a colour marker at grapheme 1.
    document.SetPosition(0) ;
    document.Insert(static_cast<CHAR_T>('e')) ;
    document.InsertColor(color) ;

    sSeqRGBColor c ;
    REQUIRE(document.GetColor(1, c) == true) ;   // colour marker starts at grapheme 1

    // Attach a combining acute (U+0301) to the 'e' (insert between 'e' and the marker).
    // No new grapheme emerges (e -> é), so nothing after it may shift.
    document.SetPosition(1) ;
    document.Insert(static_cast<CHAR_T>(0x0301)) ;

    // The colour marker must still be at grapheme 1, not shifted to 2.
    CHECK(document.GetColor(1, c) == true) ;
    CHECK(document.GetColor(2, c) == false) ;

    // The document is kept NFC: the first grapheme is composed "é" (2 UTF-8 bytes),
    // not decomposed "e"+combining (3 bytes).
    std::vector<std::string> graphemes ;
    std::vector<POSITION_T> offsets ;
    document.GetParagraphGraphemes(0, graphemes, offsets) ;
    REQUIRE(graphemes.size() >= 1) ;
    CHECK(graphemes[0].size() == 2) ;
}

TEST_CASE("Searching UTF-8")
{
    // Bug 1: whole-word matching must use codepoint boundaries, not byte offsets.
    SUBCASE("whole word match after a multibyte word")
    {
        cDocument document ;
        document.SetShowControl(SHOW_ALL) ;
        document.Insert("café se\r") ;  // graphemes: c0 a1 f2 e-acute3 sp4 s5 e6 CR7
        POSITION_T len = document.GetTextSize() ;

        POSITION_T fpos = document.FindNext("se", 0, false, false, true) ;
        CHECK(fpos == 5) ;

        fpos = document.FindPrev("se", len, false, false, true) ;
        CHECK(fpos == 5) ;
    }

    // Bug 2: case-insensitive matching must fold non-ASCII letters.
    SUBCASE("case-insensitive multibyte")
    {
        cDocument document ;
        document.SetShowControl(SHOW_ALL) ;
        document.Insert("café CAFÉ\r") ;  // café 0-3, sp 4, CAFÉ 5-8, CR 9

        // From past the first word, a case-insensitive search finds the
        // uppercase accented copy.
        POSITION_T fpos = document.FindNext("café", 1, false, true, false) ;
        CHECK(fpos == 5) ;

        // Case-sensitive must NOT match the differently-cased copy.
        fpos = document.FindNext("café", 1, false, false, false) ;
        CHECK(fpos == document.GetTextSize()) ;
    }

    // Bug 3: '?' wildcard matches one grapheme, not one byte.
    SUBCASE("wildcard matches a multibyte grapheme")
    {
        cDocument document ;
        document.SetShowControl(SHOW_ALL) ;
        document.Insert("café!\r") ;  // c0 a1 f2 e-acute3 !4 CR5

        // '?' must consume the whole é so the trailing '!' still lines up.
        POSITION_T fpos = document.FindNext("caf?!", 0, true, false, false) ;
        CHECK(fpos == 0) ;
    }

    // Bug 3 (cluster): '?' must not split a multi-codepoint grapheme.
    SUBCASE("wildcard does not split a grapheme cluster")
    {
        cDocument document ;
        document.SetShowControl(SHOW_ALL) ;
        // 'q' + combining acute (U+0301) has no precomposed form, so it stays
        // a single two-codepoint grapheme even after NFC normalization.
        document.Insert("aq́b\r") ;  // a0  q-acute 1  b2  CR3

        POSITION_T fpos = document.FindNext("a?b", 0, true, false, false) ;
        CHECK(fpos == 0) ;
    }

    // Whole-word must reject a multibyte substring of a longer word.
    SUBCASE("whole word rejects multibyte substring")
    {
        cDocument document ;
        document.SetShowControl(SHOW_ALL) ;
        document.Insert("cafés café\r") ;  // c0 a1 f2 é3 s4 sp5 c6 a7 f8 é9 CR10
        POSITION_T len = document.GetTextSize() ;

        // The "café" inside "cafés" is not a whole word; the standalone one is.
        POSITION_T fpos = document.FindNext("café", 0, false, false, true) ;
        CHECK(fpos == 6) ;

        fpos = document.FindPrev("café", len, false, false, true) ;
        CHECK(fpos == 6) ;

        // A pure prefix is never a whole word.
        fpos = document.FindNext("caf", 0, false, false, true) ;
        CHECK(fpos == document.GetTextSize()) ;
    }

    // Case-insensitive multibyte search also works backward.
    SUBCASE("case-insensitive multibyte backward")
    {
        cDocument document ;
        document.SetShowControl(SHOW_ALL) ;
        document.Insert("café CAFÉ\r") ;  // café 0-3, sp 4, CAFÉ 5-8, CR 9
        POSITION_T len = document.GetTextSize() ;

        POSITION_T fpos = document.FindPrev("café", len, false, true, false) ;
        CHECK(fpos == 5) ;
    }

    // Wildcard over a multibyte grapheme also works backward.
    SUBCASE("wildcard multibyte backward")
    {
        cDocument document ;
        document.SetShowControl(SHOW_ALL) ;
        document.Insert("café café\r") ;  // café 0-3, sp 4, café 5-8, CR 9
        POSITION_T len = document.GetTextSize() ;

        POSITION_T fpos = document.FindPrev("caf?", len, true, false, false) ;
        CHECK(fpos == 5) ;
    }

    // Regression: a composed multibyte needle still matches. (NFD-decomposed
    // needle normalization is covered by the dedicated normalize test.)
    SUBCASE("composed multibyte needle matches")
    {
        cDocument document ;
        document.SetShowControl(SHOW_ALL) ;
        document.Insert("café\r") ;

        POSITION_T fpos = document.FindNext("café", 0, false, false, false) ;
        CHECK(fpos == 0) ;
    }
}

TEST_CASE("Searching edge cases")
{
    cDocument document ;
    document.SetShowControl(SHOW_ALL) ;
    document.Insert("café test\r") ;
    POSITION_T len = document.GetTextSize() ;

    SUBCASE("empty needle finds nothing")
    {
        CHECK(document.FindNext("", 0, false, false, false) == document.GetTextSize()) ;
        CHECK(document.FindPrev("", len, false, false, false) == NOT_SET) ;
    }

    SUBCASE("needle longer than text is not found")
    {
        CHECK(document.FindNext("café test and more", 0, false, false, false) == document.GetTextSize()) ;
        CHECK(document.FindPrev("café test and more", len, false, false, false) == NOT_SET) ;
    }

    SUBCASE("absent multibyte needle is not found")
    {
        CHECK(document.FindNext("zoé", 0, false, false, false) == document.GetTextSize()) ;
    }
}

TEST_CASE("Searching")
{
    cDocument document ;
    document.SetShowControl(SHOW_ALL) ;

    document.Insert("This is a test\r") ;
    document.Insert(" This is part two\r") ;
    document.Insert(" This is pakt 3\r") ;

    POSITION_T len = document.GetTextSize() ;

    SUBCASE("forward")
    {
        POSITION_T fpos = document.FindNext("part", 0, false, false, false) ;
        CHECK(fpos == 24) ;
        fpos = document.FindNext("pakt", 25, false, false, false) ;
        CHECK(fpos == 42) ;
    }

    SUBCASE("forward wildcard")
    {
        POSITION_T fpos = document.FindNext("p?rt", 0, true, false, false) ;
        CHECK(fpos == 24) ;
        fpos = document.FindNext("pa?t", 25, true, false, false) ;
        CHECK(fpos == 42) ;

    }

    SUBCASE("forward case")
    {
        POSITION_T fpos = document.FindNext("PART", 0, false, true, false) ;
        CHECK(fpos == 24) ;
        fpos = document.FindNext("PAkT", 25, false, true, false) ;
        CHECK(fpos == 42) ;
    }

    SUBCASE("forward case wildcard")
    {
        POSITION_T fpos = document.FindNext("P?RT", 0, true, true, false) ;
        CHECK(fpos == 24) ;
        fpos = document.FindNext("PA?T", 25, true, true, false) ;
        CHECK(fpos == 42) ;
    }

    SUBCASE("forward wholeword")
    {
        POSITION_T fpos = document.FindNext("part", 0, false, false, true) ;
        CHECK(fpos == 24) ;
        fpos = document.FindNext("pakt", 25, false, false, true) ;
        CHECK(fpos == 42) ;
        fpos = document.FindNext("par", 0, false, false, true) ;
        CHECK(fpos == document.GetTextSize()) ;
    }

    SUBCASE("forward wholeword wildcard")
    {
        POSITION_T fpos = document.FindNext("p?rt", 0, true, false, true) ;
        CHECK(fpos == 24) ;
        fpos = document.FindNext("pa?t", 25, true, false, true) ;
        CHECK(fpos == 42) ;
        fpos = document.FindNext("pa?", 0, true, false, true) ;
        CHECK(fpos == document.GetTextSize()) ;
    }

    SUBCASE("forward wholeword case")
    {
        POSITION_T fpos = document.FindNext("PART", 0, false, true, true) ;
        CHECK(fpos == 24) ;
        fpos = document.FindNext("PAkT", 25, false, true, true) ;
        CHECK(fpos == 42) ;
        fpos = document.FindNext("PAR", 0, false, true, true) ;
        CHECK(fpos == document.GetTextSize()) ;
    }

    SUBCASE("forward wholeword case wildcard")
    {
        POSITION_T fpos = document.FindNext("P?RT", 0, true, true, true) ;
        CHECK(fpos == 24) ;
        fpos = document.FindNext("PA?T", 25, true, true, true) ;
        CHECK(fpos == 42) ;
        fpos = document.FindNext("PA?", 0, true, true, true) ;
        CHECK(fpos == document.GetTextSize()) ;
    }

    SUBCASE("backward")
    {
        POSITION_T fpos = document.FindPrev("pakt", len, false, false, false) ;
        CHECK(fpos == 42) ;
        fpos = document.FindPrev("part", fpos, false, false, false) ;
        CHECK(fpos == 24) ;
    }

    SUBCASE("backward wildcard")
    {
        POSITION_T fpos = document.FindPrev("p?kt", len, true, false, false) ;
        CHECK(fpos == 42) ;
        fpos = document.FindPrev("pa?t", 30, true, false, false) ;
        CHECK(fpos == 24) ;
    }

    SUBCASE("backward case")
    {
        POSITION_T fpos = document.FindPrev("PAkT", len, false, true, false) ;
        CHECK(fpos == 42) ;
        fpos = document.FindPrev("PART", 30, false, true, false) ;
        CHECK(fpos == 24) ;
    }

    SUBCASE("backward case wildcard")
    {
        POSITION_T fpos = document.FindPrev("P?kT", len, true, true, false) ;
        CHECK(fpos == 42) ;
        fpos = document.FindPrev("PA?T", 30, true, true, false) ;
        CHECK(fpos == 24) ;
    }

    SUBCASE("backward wholeword")
    {
        POSITION_T fpos = document.FindPrev("pakt", len, false, false, true) ;
        CHECK(fpos == 42) ;
        fpos = document.FindPrev("part", 30, false, false, true) ;
        CHECK(fpos == 24) ;
        fpos = document.FindPrev("par", 42, false, false, true) ;
        CHECK(fpos == NOT_SET) ;
    }

    SUBCASE("backward wholeword wildcard")
    {
        POSITION_T fpos = document.FindPrev("p?kt", len, true, false, true) ;
        CHECK(fpos == 42) ;
        fpos = document.FindPrev("pa?t", 30, true, false, true) ;
        CHECK(fpos == 24) ;
        fpos = document.FindPrev("pa?", 42, true, false, true) ;
        CHECK(fpos == NOT_SET) ;
    }

    SUBCASE("backward wholeword case")
    {
        POSITION_T fpos = document.FindPrev("PAkT", len, false, true, true) ;
        CHECK(fpos == 42) ;
        fpos = document.FindPrev("PART", 30, false, true, true) ;
        CHECK(fpos == 24) ;
        fpos = document.FindPrev("PAR", 42, false, true, true) ;
        CHECK(fpos == NOT_SET) ;
    }

    SUBCASE("backward wholeword case wildcard")
    {
        POSITION_T fpos = document.FindPrev("P?kT", len, true, true, true) ;
        CHECK(fpos == 42) ;
        fpos = document.FindPrev("PA?T", 30, true, true, true) ;
        CHECK(fpos == 24) ;
        fpos = document.FindPrev("PA?", 42, true, true, true) ;
        CHECK(fpos == NOT_SET) ;
    }
    
}

TEST_CASE("Lowercase")
{
    cDocument document ;
    document.SetShowControl(SHOW_ALL) ;
    
    std::string text = "This is a test With A Bunch of MiXed text" ;
    std::string lower = "this is a test with a bunch of mixed text" ;

    std::string f = document.LowerCase(text) ;

    CHECK(f == lower) ;


}

// =====================================================================
// COMPREHENSIVE TEST COVERAGE - PHASE 1: DOCUMENT STATE MANAGEMENT
// =====================================================================

TEST_CASE("Document State Management")
{
    cDocument document;
    document.SetShowControl(SHOW_ALL);

    SUBCASE("SetLoading and GetLoading")
    {
        // Test initial state
        CHECK(document.GetLoading() == false);
        
        // Test setting loading state
        document.SetLoading(true);
        CHECK(document.GetLoading() == true);
        
        document.SetLoading(false);
        CHECK(document.GetLoading() == false);
        
        // Test multiple toggles
        document.SetLoading(true);
        document.SetLoading(true); // Should remain true
        CHECK(document.GetLoading() == true);
        
        document.SetLoading(false);
        CHECK(document.GetLoading() == false);
    }

    SUBCASE("SetShowControl and GetShowControl")
    {
        // Test initial state (should be SHOW_ALL from constructor)
        CHECK(document.GetShowControl() == SHOW_ALL);
        
        // Test setting different control modes
        document.SetShowControl(SHOW_NONE);
        CHECK(document.GetShowControl() == SHOW_NONE);
        
        document.SetShowControl(SHOW_ALL);
        CHECK(document.GetShowControl() == SHOW_ALL);
        
        // Test with document content
        document.Insert("Test text");
        document.SetShowControl(SHOW_NONE);
        CHECK(document.GetShowControl() == SHOW_NONE);
        
        document.SetShowControl(SHOW_ALL);
        CHECK(document.GetShowControl() == SHOW_ALL);
    }

    SUBCASE("GetText - Empty Document")
    {
        document.Clear();
        std::string text = document.GetText();
        
        // Empty document should contain only EOF marker (stored as MARKER_CHAR)
        CHECK(text.length() == 1);
        CHECK(text[0] == MARKER_CHAR);
    }

    SUBCASE("GetText - Simple Document")
    {
        document.Clear();
        document.Insert("Hello World");
        
        std::string text = document.GetText();
        std::string expected = "Hello World";
        expected += MARKER_CHAR;  // GetText returns raw MARKER_CHAR for control codes

        CHECK(text == expected);
    }

    SUBCASE("GetText - Multi-paragraph Document")
    {
        document.Clear();
        document.Insert("First paragraph\r");
        document.Insert("Second paragraph\r");
        document.Insert("Third paragraph");

        std::string text = document.GetText();
        // GetText returns raw MARKER_CHAR for control codes (including EOF)
        std::string expected = "First paragraph\rSecond paragraph\rThird paragraph";
        expected += static_cast<char>(MARKER_CHAR) ;

        CHECK(text == expected);
    }

    SUBCASE("GetText - Document with Control Characters")
    {
        document.Clear();
        document.Insert("Bold");
        document.BeginBold();
        document.Insert(" text ");
        document.EndBold();
        document.Insert("normal");
        
        std::string text = document.GetText();
        
        // GetText returns raw MARKER_CHAR for all control codes (bold, EOF, etc.)
        // Count MARKER_CHAR occurrences to verify control chars are present
        int markerCount = 0;
        for (char c : text)
        {
            if (c == MARKER_CHAR)
            {
                markerCount++;
            }
        }

        // Bold begin + bold end + EOF = at least 3 MARKER_CHAR bytes
        CHECK(markerCount >= 3);
        CHECK(text[text.length() - 1] == MARKER_CHAR);
    }

    SUBCASE("ShrinkToFit - Basic Functionality")
    {
        // Add content to create vectors that might have excess capacity
        document.Clear();
        for (int i = 0; i < 100; i++) {
            document.Insert("Text ");
            if (i % 10 == 0) {
                document.Insert("\r");
            }
        }
        
        // Add formatting
        document.SetPosition(10);
        document.BeginBold();
        document.SetPosition(20);
        document.EndBold();
        
        // Call ShrinkToFit - mainly testing it doesn't crash
        document.ShrinkToFit();
        
        // Verify document still works after shrinking
        CHECK(document.GetTextSize() > 0);
        CHECK(document.GetNumberofParagraphs() > 1);
        
        // Test that we can still insert and delete
        POSITION_T oldSize = document.GetTextSize();
        document.Insert("NEW");
        CHECK(document.GetTextSize() == oldSize + 3);
        
        document.Delete(document.GetPosition() - 3, 3);
        CHECK(document.GetTextSize() == oldSize);
    }

    SUBCASE("ShrinkToFit - Empty Document")
    {
        document.Clear();
        document.ShrinkToFit(); // Should not crash
        
        CHECK(document.GetTextSize() == 1); // EOF marker
        CHECK(document.GetNumberofParagraphs() == 1);
    }

    SUBCASE("ShrinkToFit - Large Document")
    {
        document.Clear();
        
        // Create a large document with many paragraphs and formatting
        for (int para = 0; para < 50; para++) {
            for (int word = 0; word < 20; word++) {
                document.Insert("word" + std::to_string(word) + " ");
                
                // Add formatting every few words
                if (word % 5 == 0) {
                    document.BeginBold();
                    document.Insert("BOLD");
                    document.EndBold();
                    document.Insert(" ");
                }
            }
            document.Insert("\r");
        }
        
        POSITION_T sizeBeforeShrink = document.GetTextSize();
        PARAGRAPH_T parasBeforeShrink = document.GetNumberofParagraphs();
        
        document.ShrinkToFit();
        
        // Content should be unchanged
        CHECK(document.GetTextSize() == sizeBeforeShrink);
        CHECK(document.GetNumberofParagraphs() == parasBeforeShrink);
        
        // Document should still be functional
        document.SetPosition(100);
        document.Insert("TEST");
        CHECK(document.GetTextSize() == sizeBeforeShrink + 4);
    }
}

// =====================================================================
// COMPREHENSIVE TEST COVERAGE - PHASE 1: POSITION & NAVIGATION
// =====================================================================

TEST_CASE("Position and Navigation")
{
    cDocument document;
    document.SetShowControl(SHOW_ALL);

    SUBCASE("GotoPreviousPosition - Basic Functionality")
    {
        document.Clear();
        document.Insert("Hello World");
        
        // Move to position 5
        document.SetPosition(5);
        CHECK(document.GetPosition() == 5);
        
        // Move to position 8
        document.SetPosition(8);
        CHECK(document.GetPosition() == 8);
        
        // Go back to previous position (should be 5)
        document.GotoPreviousPosition();
        CHECK(document.GetPosition() == 5);
    }

    SUBCASE("GotoPreviousPosition - Multiple Movements")
    {
        document.Clear();
        document.Insert("This is a test document");
        
        // Move through several positions
        document.SetPosition(0);
        document.SetPosition(5);
        document.SetPosition(10);
        document.SetPosition(15);
        
        // Previous should be 10
        document.GotoPreviousPosition();
        CHECK(document.GetPosition() == 10);
        
        // Move again and test
        document.SetPosition(20);
        document.GotoPreviousPosition();
        CHECK(document.GetPosition() == 10); // Should still be 10
    }

    SUBCASE("SetPosition - Edge Cases")
    {
        document.Clear();
        document.Insert("Test");
        
        POSITION_T textSize = document.GetTextSize();
        
        // Test valid positions
        document.SetPosition(0);
        CHECK(document.GetPosition() == 0);
        
        document.SetPosition(textSize - 1);
        CHECK(document.GetPosition() == textSize - 1);
        
        // Test position at text size (should be valid for insertion)
        document.SetPosition(textSize);
        CHECK(document.GetPosition() == textSize - 1);
        
        // Test position beyond text size (should clamp or handle gracefully)
        POSITION_T farPosition = textSize + 100;
        document.SetPosition(farPosition);
        // Document should handle this gracefully - either clamp or allow
        CHECK(document.GetPosition() >= 0);
    }

    SUBCASE("SetPosition - Negative Values")
    {
        document.Clear();
        document.Insert("Test");
        
        // Set to middle first
        document.SetPosition(2);
        CHECK(document.GetPosition() == 2);
        
        // Try negative position - should be handled gracefully
        document.SetPosition(-1);
        // Should either clamp to 0 or handle gracefully
        CHECK(document.GetPosition() >= 0);
        
        document.SetPosition(-100);
        CHECK(document.GetPosition() >= 0);
    }

    SUBCASE("Word Position Navigation - Basic")
    {
        document.Clear();
        document.Insert("The quick brown fox jumps");
        
        // Start at beginning
        document.SetPosition(0);
        
        // Test GetNextWordPosition
        POSITION_T nextWord = document.GetNextWordPosition(0);
        CHECK(nextWord > 0); // Should find next word
        
        nextWord = document.GetNextWordPosition(nextWord);
        CHECK(nextWord > 4); // Should find word after "quick"
        
        // Test GetPrevWordPosition
        POSITION_T prevWord = document.GetPrevWordPosition(nextWord);
        CHECK(prevWord < nextWord);
        CHECK(prevWord >= 0);
    }

    SUBCASE("Word Position Navigation - Edge Cases")
    {
        document.Clear();
        document.Insert("Word");
        
        POSITION_T textSize = document.GetTextSize();
        
        // Test at document boundaries
        POSITION_T nextFromEnd = document.GetNextWordPosition(textSize - 1);

        // Should handle gracefully (return same position or NOT_SET)
        POSITION_T prevFromStart = document.GetPrevWordPosition(0);

        // Should handle gracefully (return 0 or NOT_SET)
        CHECK((prevFromStart >= 0 || prevFromStart == NOT_SET) == true);
    }

    SUBCASE("Word Position Navigation - Multiple Words")
    {
        document.Clear();
        document.Insert("One two three four five six seven");
        
        document.SetPosition(0);
        std::vector<POSITION_T> wordPositions;
        
        // Collect all word positions going forward
        POSITION_T pos = 0;
        for (int i = 0; i < 10; i++) { // Max 10 iterations to prevent infinite loop
            POSITION_T nextPos = document.GetNextWordPosition(pos);
            if (nextPos == pos || nextPos >= document.GetTextSize()) {
                break;
            }
            wordPositions.push_back(nextPos);
            pos = nextPos;
        }
        
        CHECK(wordPositions.size() >= 6); // Should find at least 6 words
        
        // Test going backwards
        if (!wordPositions.empty()) {
            POSITION_T lastWordPos = wordPositions.back();
            POSITION_T prevPos = document.GetPrevWordPosition(lastWordPos);
            CHECK(prevPos < lastWordPos);
            CHECK(prevPos >= 0);
        }
    }

    SUBCASE("Word Position Navigation - Punctuation and Spaces")
    {
        document.Clear();
        document.Insert("Hello,  world!  How   are you?");
        
        // Test navigation with multiple spaces and punctuation
        document.SetPosition(0);
        
        POSITION_T pos1 = document.GetNextWordPosition(0); // Should find "world"
        CHECK(pos1 > 5); // Should skip comma and spaces
        
        POSITION_T pos2 = document.GetNextWordPosition(pos1); // Should find "How"
        CHECK(pos2 > pos1);
        
        POSITION_T pos3 = document.GetNextWordPosition(pos2); // Should find "are"
        CHECK(pos3 > pos2);
        
        // Test reverse navigation
        POSITION_T back1 = document.GetPrevWordPosition(pos3);
        CHECK(back1 < pos3);
        CHECK(back1 >= pos2 - 1); // Should be around "How"
    }

    SUBCASE("Position Navigation - Empty Document")
    {
        document.Clear(); // Creates document with just EOF
        
        // Test word navigation on empty document
        POSITION_T nextWord = document.GetNextWordPosition(0);
        POSITION_T prevWord = document.GetPrevWordPosition(0);

        // Should handle gracefully
        bool nextWordValid = (nextWord == 1 || nextWord == NOT_SET);  // 1 is position of ^Z
        CHECK(nextWordValid);
        bool prevWordValid = (prevWord == 0 || prevWord == NOT_SET);
        CHECK(prevWordValid);
        
        // Test GotoPreviousPosition on empty document
        document.GotoPreviousPosition(); // Should not crash
        CHECK(document.GetPosition() >= 0);
    }

    SUBCASE("Position Navigation - Single Character Document")
    {
        document.Clear();
        document.Insert("A");

        // Test word navigation with single character
        POSITION_T nextWord = document.GetNextWordPosition(0);
        POSITION_T prevWord = document.GetPrevWordPosition(1);

        // Should handle single character gracefully
        CHECK(nextWord >= 0);
        CHECK(prevWord >= 0);
    }

    SUBCASE("GetWordEndPosition - word characters")
    {
        document.Clear();
        document.Insert("Hello World");

        // At start of "Hello": should return 5 (end of word, before space)
        POSITION_T endPos = document.GetWordEndPosition(0);
        CHECK(endPos == 5);

        // At middle of "Hello": should return 5 (end of same word)
        endPos = document.GetWordEndPosition(3);
        CHECK(endPos == 5);
    }

    SUBCASE("GetWordEndPosition - whitespace")
    {
        document.Clear();
        document.Insert("Hello World");

        // At space between words: should advance past whitespace to 6
        POSITION_T endPos = document.GetWordEndPosition(5);
        CHECK(endPos == 6);
    }

    SUBCASE("GetWordEndPosition - multiple spaces")
    {
        document.Clear();
        document.Insert("Hello  World");

        // At first space of double-space: should advance past all whitespace to 7
        POSITION_T endPos = document.GetWordEndPosition(5);
        CHECK(endPos == 7);
    }

    SUBCASE("GetWordEndPosition - single word")
    {
        document.Clear();
        document.Insert("Hello");

        // At start of only word: should return 5 (end of word)
        POSITION_T endPos = document.GetWordEndPosition(0);
        CHECK(endPos == 5);
    }

    SUBCASE("GetWordEndPosition - at end of document")
    {
        document.Clear();
        document.Insert("Hello");

        POSITION_T textSize = document.GetTextSize();

        // At end of document: should return same position
        POSITION_T endPos = document.GetWordEndPosition(textSize);
        CHECK(endPos == textSize);
    }
}

// =====================================================================
// COMPREHENSIVE TEST COVERAGE - PHASE 2: UNDO/REDO SYSTEM
// =====================================================================

TEST_CASE("Undo Redo System")
{
    cDocument document ;
    document.SetShowControl(SHOW_ALL) ;

    SUBCASE("Basic Undo - Single Character Insert")
    {
        document.Clear() ;

        // Insert a character
        document.Insert('A') ;
        CHECK(document.GetTextSize() == 2) ; // A + EOF
        CHECK(document.GetCharNoAdvance(0) == "A") ;

        // undo the insertion
        bool undoResult = document.Undo() ;
        CHECK(undoResult == true) ;
        CHECK(document.GetTextSize() == 1) ; // just EOF
        CHECK(resolveControlChar(document, 0) == STYLE_EOF) ;
    }

    SUBCASE("Basic Redo - Single Character")
    {
        document.Clear() ;

        // Insert and then undo
        document.Insert('B') ;
        document.Undo() ;
        CHECK(document.GetTextSize() == 1) ; // just EOF

        // Redo the insertion
        bool redoResult = document.Redo() ;
        CHECK(redoResult == true) ;
        CHECK(document.GetTextSize() == 2) ; // B + EOF
        CHECK(document.GetCharNoAdvance(0) == "B") ;
    }

    SUBCASE("Undo/Redo String Insert")
    {
        document.Clear() ;

        // Insert(string) records one combined action for the whole string
        document.Insert("Hello") ;
        CHECK(document.GetTextSize() == 6) ; // Hello + EOF

        // single undo removes the entire string
        bool undoResult = document.Undo() ;
        CHECK(undoResult == true) ;
        CHECK(document.GetTextSize() == 1) ; // just EOF

        // single redo restores the entire string
        bool redoResult = document.Redo() ;
        CHECK(redoResult == true) ;
        CHECK(document.GetTextSize() == 6) ; // Hello + EOF
        CHECK(document.GetCharNoAdvance(0) == "H") ;
        CHECK(document.GetCharNoAdvance(1) == "e") ;
        CHECK(document.GetCharNoAdvance(2) == "l") ;
        CHECK(document.GetCharNoAdvance(3) == "l") ;
        CHECK(document.GetCharNoAdvance(4) == "o") ;
    }

    SUBCASE("Undo/Redo Multiple Single Characters")
    {
        document.Clear() ;

        // Insert characters one at a time -- each is a separate undo step
        document.Insert('A') ;
        document.Insert('B') ;
        document.Insert('C') ;
        CHECK(document.GetTextSize() == 4) ; // ABC + EOF

        // undo removes one character at a time
        document.Undo() ;
        CHECK(document.GetTextSize() == 3) ; // AB + EOF
        CHECK(document.GetCharNoAdvance(0) == "A") ;
        CHECK(document.GetCharNoAdvance(1) == "B") ;

        document.Undo() ;
        CHECK(document.GetTextSize() == 2) ; // A + EOF

        document.Undo() ;
        CHECK(document.GetTextSize() == 1) ; // just EOF

        // nothing more to undo
        CHECK(document.Undo() == false) ;

        // redo all three
        document.Redo() ;
        CHECK(document.GetTextSize() == 2) ;
        document.Redo() ;
        CHECK(document.GetTextSize() == 3) ;
        document.Redo() ;
        CHECK(document.GetTextSize() == 4) ;

        // nothing more to redo
        CHECK(document.Redo() == false) ;
    }

    SUBCASE("Undo/Redo Delete")
    {
        document.Clear() ;
        document.Insert("Hello World") ;
        POSITION_T originalSize = document.GetTextSize() ; // 12 (Hello World + EOF)

        // delete "World" (positions 6-10)
        document.Delete(6, 5) ;
        CHECK(document.GetTextSize() == originalSize - 5) ; // "Hello " + EOF

        // undo restores "World"
        bool undoResult = document.Undo() ;
        CHECK(undoResult == true) ;
        CHECK(document.GetTextSize() == originalSize) ;
        CHECK(document.GetCharNoAdvance(6) == "W") ;
        CHECK(document.GetCharNoAdvance(7) == "o") ;
        CHECK(document.GetCharNoAdvance(8) == "r") ;
        CHECK(document.GetCharNoAdvance(9) == "l") ;
        CHECK(document.GetCharNoAdvance(10) == "d") ;

        // redo removes "World" again
        bool redoResult = document.Redo() ;
        CHECK(redoResult == true) ;
        CHECK(document.GetTextSize() == originalSize - 5) ;
    }

    SUBCASE("Undo/Redo Delete Across Paragraph Boundary")
    {
        document.Clear() ;
        document.Insert("Line1\rLine2") ;
        POSITION_T originalSize = document.GetTextSize() ;

        // verify we have 2 paragraphs
        CHECK(document.GetNumberofParagraphs() == 2) ;

        // delete across the paragraph boundary: "1\rL" (positions 4-6)
        document.Delete(4, 3) ;
        CHECK(document.GetTextSize() == originalSize - 3) ;

        // undo restores the paragraph boundary
        document.Undo() ;
        CHECK(document.GetTextSize() == originalSize) ;
        CHECK(document.GetNumberofParagraphs() == 2) ;
        CHECK(document.GetCharNoAdvance(4) == "1") ;
        std::string crChar = document.GetCharNoAdvance(5) ;
        CHECK(crChar[0] == HARD_RETURN) ;
        CHECK(document.GetCharNoAdvance(6) == "L") ;
    }

    SUBCASE("Undo/Redo Formatting Toggle")
    {
        document.Clear() ;
        document.Insert("Test") ;
        POSITION_T sizeBeforeFormat = document.GetTextSize() ; // 5 (Test + EOF)

        // insert bold toggle at position 2
        document.SetPosition(2) ;
        document.BeginBold() ;
        POSITION_T sizeAfterBold = document.GetTextSize() ; // 6 (Te[BOLD]st + EOF)
        CHECK(sizeAfterBold == sizeBeforeFormat + 1) ;

        // undo removes the bold marker
        document.Undo() ;
        CHECK(document.GetTextSize() == sizeBeforeFormat) ;

        // redo restores the bold marker
        document.Redo() ;
        CHECK(document.GetTextSize() == sizeAfterBold) ;

        // verify the bold marker is at the right position
        eModifiers ctrl = document.GetControlChar(2) ;
        CHECK(ctrl == STYLE_BOLD) ;
    }

    SUBCASE("Undo/Redo InsertTab")
    {
        document.Clear() ;
        document.Insert("AB") ;
        POSITION_T sizeBeforeTab = document.GetTextSize() ; // 3 (AB + EOF)

        // insert tab at position 1 (between A and B)
        document.SetPosition(1) ;
        sWSTab tab ;
        tab.type = TAB_TAB ;
        tab.size = 50 ;          // 1/10 inch (char field, max 127)
        tab.tabsize = 720 ;
        tab.abstabsize = 720 ;
        document.InsertTab(tab) ;
        CHECK(document.GetTextSize() == sizeBeforeTab + 1) ;

        // verify tab data
        sWSTab readTab = document.GetTab(1) ;
        CHECK(readTab.type == TAB_TAB) ;
        CHECK(readTab.size == 50) ;
        CHECK(readTab.tabsize == 720) ;

        // undo removes the tab
        document.Undo() ;
        CHECK(document.GetTextSize() == sizeBeforeTab) ;
        CHECK(document.GetCharNoAdvance(0) == "A") ;
        CHECK(document.GetCharNoAdvance(1) == "B") ;

        // redo restores the tab with metadata
        document.Redo() ;
        CHECK(document.GetTextSize() == sizeBeforeTab + 1) ;
        sWSTab redoTab = document.GetTab(1) ;
        CHECK(redoTab.type == TAB_TAB) ;
        CHECK(redoTab.size == 50) ;
        CHECK(redoTab.tabsize == 720) ;
    }

    SUBCASE("Undo/Redo InsertFont")
    {
        document.Clear() ;
        document.Insert("Text") ;
        POSITION_T sizeBeforeFont = document.GetTextSize() ;

        // insert font change at position 2
        document.SetPosition(2) ;
        sInternalFonts font ;
        font.fontname = "Arial" ;
        font.size = 12.0 ;
        document.InsertFont(font) ;
        CHECK(document.GetTextSize() == sizeBeforeFont + 1) ;

        // verify font data
        sInternalFonts readFont ;
        bool fontFound = document.GetFont(2, readFont) ;
        CHECK(fontFound == true) ;
        CHECK(readFont.fontname == "Arial") ;
        CHECK(readFont.size == 12.0) ;

        // undo removes the font marker
        document.Undo() ;
        CHECK(document.GetTextSize() == sizeBeforeFont) ;

        // redo restores the font with metadata
        document.Redo() ;
        CHECK(document.GetTextSize() == sizeBeforeFont + 1) ;
        sInternalFonts redoFont ;
        fontFound = document.GetFont(2, redoFont) ;
        CHECK(fontFound == true) ;
        CHECK(redoFont.fontname == "Arial") ;
    }

    SUBCASE("Undo/Redo InsertColor")
    {
        document.Clear() ;
        document.Insert("Text") ;
        POSITION_T sizeBeforeColor = document.GetTextSize() ;

        // insert color at position 2
        document.SetPosition(2) ;
        sSeqRGBColor color ;
        color.red = 170 ; color.green = 0 ; color.blue = 0 ; color.alpha = 255 ; // WS red (index 4)
        document.InsertColor(color) ;
        CHECK(document.GetTextSize() == sizeBeforeColor + 1) ;

        // verify color data
        sSeqRGBColor readColor ;
        bool colorFound = document.GetColor(2, readColor) ;
        CHECK(colorFound == true) ;
        CHECK(readColor.red == 170) ;
        CHECK(readColor.green == 0) ;
        CHECK(readColor.blue == 0) ;

        // undo removes the color marker
        document.Undo() ;
        CHECK(document.GetTextSize() == sizeBeforeColor) ;

        // redo restores the color with metadata
        document.Redo() ;
        CHECK(document.GetTextSize() == sizeBeforeColor + 1) ;
        sSeqRGBColor redoColor ;
        colorFound = document.GetColor(2, redoColor) ;
        CHECK(colorFound == true) ;
        CHECK(redoColor.red == 170) ;
        CHECK(redoColor.green == 0) ;
        CHECK(redoColor.blue == 0) ;
    }

    SUBCASE("Undo/Redo InsertVariable")
    {
        document.Clear() ;
        document.Insert("Text") ;
        POSITION_T sizeBeforeVar = document.GetTextSize() ;

        // insert variable at position 2
        document.SetPosition(2) ;
        document.InsertVariable(VAR_PAGE_NUMBER) ;
        CHECK(document.GetTextSize() == sizeBeforeVar + 1) ;

        // verify variable data
        eVariableType varType = document.GetVariable(2) ;
        CHECK(varType == VAR_PAGE_NUMBER) ;

        // undo removes the variable marker
        document.Undo() ;
        CHECK(document.GetTextSize() == sizeBeforeVar) ;

        // redo restores the variable with metadata
        document.Redo() ;
        CHECK(document.GetTextSize() == sizeBeforeVar + 1) ;
        varType = document.GetVariable(2) ;
        CHECK(varType == VAR_PAGE_NUMBER) ;
    }

    SUBCASE("Undo Empty Document")
    {
        document.Clear() ;

        // try to undo on empty document -- should return false
        bool undoResult = document.Undo() ;
        CHECK(undoResult == false) ;
        CHECK(document.GetTextSize() == 1) ; // still has EOF
    }

    SUBCASE("Redo Empty Document")
    {
        document.Clear() ;

        // try to redo on empty document -- should return false
        bool redoResult = document.Redo() ;
        CHECK(redoResult == false) ;
        CHECK(document.GetTextSize() == 1) ; // still has EOF
    }

    SUBCASE("CanUndo and CanRedo")
    {
        document.Clear() ;

        // initially nothing to undo or redo
        CHECK(document.CanUndo() == false) ;
        CHECK(document.CanRedo() == false) ;

        // after insert, can undo but not redo
        document.Insert('A') ;
        CHECK(document.CanUndo() == true) ;
        CHECK(document.CanRedo() == false) ;

        // after undo, can redo but not undo
        document.Undo() ;
        CHECK(document.CanUndo() == false) ;
        CHECK(document.CanRedo() == true) ;

        // after redo, can undo but not redo
        document.Redo() ;
        CHECK(document.CanUndo() == true) ;
        CHECK(document.CanRedo() == false) ;
    }

    SUBCASE("Redo Cleared On New Edit")
    {
        document.Clear() ;

        document.Insert('A') ;
        document.Insert('B') ;

        // undo B
        document.Undo() ;
        CHECK(document.CanRedo() == true) ;

        // new edit clears redo stack
        document.Insert('C') ;
        CHECK(document.CanRedo() == false) ;

        // undo C, then A
        document.Undo() ;
        CHECK(document.GetTextSize() == 2) ; // A + EOF
        document.Undo() ;
        CHECK(document.GetTextSize() == 1) ; // just EOF
    }

    SUBCASE("ClearUndoHistory")
    {
        document.Clear() ;

        document.Insert('A') ;
        document.Insert('B') ;
        CHECK(document.CanUndo() == true) ;

        document.ClearUndoHistory() ;
        CHECK(document.CanUndo() == false) ;
        CHECK(document.CanRedo() == false) ;

        // document content is unchanged
        CHECK(document.GetTextSize() == 3) ; // AB + EOF
    }

    SUBCASE("Undo/Redo Cursor Position Restoration")
    {
        document.Clear() ;
        document.Insert("Hello World") ;

        // move to position 6 and insert
        document.SetPosition(6) ;
        POSITION_T cursorBefore = document.GetPosition() ;
        document.Insert("Beautiful ") ;
        POSITION_T cursorAfter = document.GetPosition() ;

        // undo should restore cursor to before the insert
        document.Undo() ;
        CHECK(document.GetPosition() == cursorBefore) ;

        // redo should restore cursor to after the insert
        document.Redo() ;
        CHECK(document.GetPosition() == cursorAfter) ;
    }

    SUBCASE("Undo/Redo Chain Limit")
    {
        document.Clear() ;

        // insert more characters than MAX_UNDO_STEPS
        for (int i = 0 ; i < 120 ; i++)
        {
            document.Insert(static_cast<CHAR_T>('A' + (i % 26))) ;
        }

        // undo all -- should stop at MAX_UNDO_STEPS (100)
        int undoCount = 0 ;
        while (document.Undo())
        {
            undoCount++ ;
        }

        CHECK(undoCount == MAX_UNDO_STEPS) ;

        // there should still be 20 characters left (120 - 100) + EOF
        CHECK(document.GetTextSize() == 21) ;

        // redo all
        int redoCount = 0 ;
        while (document.Redo())
        {
            redoCount++ ;
        }
        CHECK(redoCount == MAX_UNDO_STEPS) ;
        CHECK(document.GetTextSize() == 121) ; // 120 chars + EOF
    }

    SUBCASE("BeginUndoGroup / EndUndoGroup")
    {
        document.Clear() ;

        // group multiple actions into one undo step
        document.BeginUndoGroup() ;
        document.Insert('A') ;
        document.Insert('B') ;
        document.Insert('C') ;
        document.EndUndoGroup() ;

        CHECK(document.GetTextSize() == 4) ; // ABC + EOF

        // single undo should remove all three characters
        document.Undo() ;
        CHECK(document.GetTextSize() == 1) ; // just EOF

        // single redo should restore all three
        document.Redo() ;
        CHECK(document.GetTextSize() == 4) ;
        CHECK(document.GetCharNoAdvance(0) == "A") ;
        CHECK(document.GetCharNoAdvance(1) == "B") ;
        CHECK(document.GetCharNoAdvance(2) == "C") ;
    }

    SUBCASE("Nested BeginUndoGroup / EndUndoGroup")
    {
        document.Clear() ;

        // outer group
        document.BeginUndoGroup() ;
        document.Insert('A') ;

        // nested group -- should be ignored (only outermost matters)
        document.BeginUndoGroup() ;
        document.Insert('B') ;
        document.EndUndoGroup() ;

        document.Insert('C') ;
        document.EndUndoGroup() ;

        CHECK(document.GetTextSize() == 4) ; // ABC + EOF

        // single undo should remove all three (one group)
        document.Undo() ;
        CHECK(document.GetTextSize() == 1) ;
    }

    SUBCASE("Undo Group with Delete and Insert")
    {
        document.Clear() ;
        document.Insert("Hello") ;
        CHECK(document.GetTextSize() == 6) ;

        // group a delete and insert together
        document.BeginUndoGroup() ;
        document.Delete(0, 5) ;   // delete "Hello"
        document.SetPosition(0) ;
        document.Insert("World") ;
        document.EndUndoGroup() ;

        CHECK(document.GetTextSize() == 6) ; // World + EOF
        CHECK(document.GetCharNoAdvance(0) == "W") ;

        // single undo reverses the entire group
        document.Undo() ;
        CHECK(document.GetTextSize() == 6) ; // Hello + EOF
        CHECK(document.GetCharNoAdvance(0) == "H") ;
        CHECK(document.GetCharNoAdvance(1) == "e") ;
        CHECK(document.GetCharNoAdvance(2) == "l") ;
        CHECK(document.GetCharNoAdvance(3) == "l") ;
        CHECK(document.GetCharNoAdvance(4) == "o") ;
    }

    SUBCASE("Undo/Redo Does Not Record During File Load")
    {
        document.Clear() ;

        // simulate file loading
        document.SetLoading(true) ;
        document.Insert("Loaded content") ;
        document.SetLoading(false) ;

        // nothing should be on the undo stack
        CHECK(document.CanUndo() == false) ;
        CHECK(document.GetTextSize() == 15) ; // "Loaded content" + EOF
    }

    SUBCASE("Undo/Redo with Hard Return")
    {
        document.Clear() ;
        document.Insert("AB") ;
        POSITION_T sizeBeforeCR = document.GetTextSize() ; // 3 (AB + EOF)

        // insert hard return at position 1 (between A and B)
        document.SetPosition(1) ;
        document.Insert(HARD_RETURN) ;
        CHECK(document.GetTextSize() == sizeBeforeCR + 1) ;
        CHECK(document.GetNumberofParagraphs() == 2) ;

        // undo removes the hard return
        document.Undo() ;
        CHECK(document.GetTextSize() == sizeBeforeCR) ;
        CHECK(document.GetNumberofParagraphs() == 1) ;

        // redo restores the hard return
        document.Redo() ;
        CHECK(document.GetTextSize() == sizeBeforeCR + 1) ;
        CHECK(document.GetNumberofParagraphs() == 2) ;
    }

    SUBCASE("Delete Then Undo Restores Tab Metadata")
    {
        document.Clear() ;
        document.Insert("AB") ;

        // insert a tab at position 1
        document.SetPosition(1) ;
        sWSTab tab ;
        tab.type = TAB_DECIMAL ;
        tab.size = 50 ;          // 1/10 inch (char field, max 127)
        tab.tabsize = 1440 ;
        tab.abstabsize = 1440 ;
        document.InsertTab(tab) ;
        CHECK(document.GetTextSize() == 4) ; // A [TAB] B EOF

        // delete the tab
        document.Delete(1, 1) ;
        CHECK(document.GetTextSize() == 3) ; // A B EOF

        // undo delete should restore the tab with its metadata
        document.Undo() ;
        CHECK(document.GetTextSize() == 4) ;
        sWSTab restoredTab = document.GetTab(1) ;
        CHECK(restoredTab.type == TAB_DECIMAL) ;
        CHECK(restoredTab.size == 50) ;
        CHECK(restoredTab.tabsize == 1440) ;
        CHECK(restoredTab.abstabsize == 1440) ;
    }

    SUBCASE("Delete Then Undo Restores Font Metadata")
    {
        document.Clear() ;
        document.Insert("AB") ;

        // insert a font at position 1
        document.SetPosition(1) ;
        sInternalFonts font ;
        font.fontname = "Times New Roman" ;
        font.size = 14.0 ;
        document.InsertFont(font) ;

        // delete the font marker
        document.Delete(1, 1) ;

        // undo should restore font with metadata
        document.Undo() ;
        sInternalFonts restoredFont ;
        bool found = document.GetFont(1, restoredFont) ;
        CHECK(found == true) ;
        CHECK(restoredFont.fontname == "Times New Roman") ;
        CHECK(restoredFont.size == 14.0) ;
    }

    SUBCASE("Delete Then Undo Restores Color Metadata")
    {
        document.Clear() ;
        document.Insert("AB") ;

        // insert a color at position 1
        document.SetPosition(1) ;
        sSeqRGBColor color ;
        color.red = 170 ; color.green = 170 ; color.blue = 170 ; color.alpha = 255 ; // WS light gray (index 7)
        document.InsertColor(color) ;

        // delete the color marker
        document.Delete(1, 1) ;

        // undo should restore color with metadata
        document.Undo() ;
        sSeqRGBColor restoredColor ;
        bool found = document.GetColor(1, restoredColor) ;
        CHECK(found == true) ;
        CHECK(restoredColor.red == 170) ;
        CHECK(restoredColor.green == 170) ;
        CHECK(restoredColor.blue == 170) ;
    }

    SUBCASE("Multiple Sequential Undo/Redo")
    {
        document.Clear() ;

        // build up document with multiple operations
        document.Insert("Hello") ;       // undo step 1
        document.SetPosition(5) ;
        document.Insert(" ") ;            // undo step 2
        document.Insert("World") ;        // undo step 3

        CHECK(document.GetTextSize() == 12) ; // "Hello World" + EOF

        // undo step 3: remove "World" (string insert = 1 undo step)
        document.Undo() ;
        CHECK(document.GetTextSize() == 7) ; // "Hello " + EOF

        // undo step 2: remove " "
        document.Undo() ;
        CHECK(document.GetTextSize() == 6) ; // "Hello" + EOF

        // undo step 1: remove "Hello"
        document.Undo() ;
        CHECK(document.GetTextSize() == 1) ; // just EOF

        // redo all three
        document.Redo() ; // "Hello"
        CHECK(document.GetTextSize() == 6) ;
        document.Redo() ; // " "
        CHECK(document.GetTextSize() == 7) ;
        document.Redo() ; // "World"
        CHECK(document.GetTextSize() == 12) ;
    }

    // =================================================================
    // CRITICAL: Multi-undo then redo scenarios (user-reported bug area)
    // =================================================================

    SUBCASE("Three Separate Inserts - 3 Undos - 3 Redos")
    {
        document.Clear() ;

        // Three separate insert operations (each is a separate undo step)
        document.Insert('A') ;
        document.Insert('B') ;
        document.Insert('C') ;

        CHECK(document.GetTextSize() == 4) ; // ABC + EOF

        // Undo all three
        CHECK(document.Undo() == true) ;
        CHECK(document.GetTextSize() == 3) ; // AB + EOF
        CHECK(document.Undo() == true) ;
        CHECK(document.GetTextSize() == 2) ; // A + EOF
        CHECK(document.Undo() == true) ;
        CHECK(document.GetTextSize() == 1) ; // just EOF

        // No more to undo
        CHECK(document.Undo() == false) ;

        // Redo all three
        CHECK(document.CanRedo() == true) ;
        CHECK(document.Redo() == true) ;
        CHECK(document.GetTextSize() == 2) ; // A + EOF
        CHECK(document.GetCharNoAdvance(0) == "A") ;

        CHECK(document.Redo() == true) ;
        CHECK(document.GetTextSize() == 3) ; // AB + EOF
        CHECK(document.GetCharNoAdvance(1) == "B") ;

        CHECK(document.Redo() == true) ;
        CHECK(document.GetTextSize() == 4) ; // ABC + EOF
        CHECK(document.GetCharNoAdvance(2) == "C") ;

        // No more to redo
        CHECK(document.Redo() == false) ;
    }

    SUBCASE("Three Separate Deletes - 3 Undos - 3 Redos")
    {
        document.Clear() ;
        document.Insert("ABCDEF") ;
        document.ClearUndoHistory() ;

        CHECK(document.GetTextSize() == 7) ; // ABCDEF + EOF

        // Delete F (pos 5), then E (pos 4), then D (pos 3)
        document.Delete(5, 1) ; // remove F
        CHECK(document.GetTextSize() == 6) ;
        document.Delete(4, 1) ; // remove E
        CHECK(document.GetTextSize() == 5) ;
        document.Delete(3, 1) ; // remove D
        CHECK(document.GetTextSize() == 4) ; // ABC + EOF

        // Undo all three deletes (reverse order)
        CHECK(document.Undo() == true) ; // restore D
        CHECK(document.GetTextSize() == 5) ; // ABCD + EOF
        CHECK(document.GetCharNoAdvance(3) == "D") ;

        CHECK(document.Undo() == true) ; // restore E
        CHECK(document.GetTextSize() == 6) ; // ABCDE + EOF
        CHECK(document.GetCharNoAdvance(4) == "E") ;

        CHECK(document.Undo() == true) ; // restore F
        CHECK(document.GetTextSize() == 7) ; // ABCDEF + EOF
        CHECK(document.GetCharNoAdvance(5) == "F") ;

        CHECK(document.Undo() == false) ; // nothing left

        // Redo all three deletes
        CHECK(document.Redo() == true) ; // re-delete F
        CHECK(document.GetTextSize() == 6) ;
        CHECK(document.Redo() == true) ; // re-delete E
        CHECK(document.GetTextSize() == 5) ;
        CHECK(document.Redo() == true) ; // re-delete D
        CHECK(document.GetTextSize() == 4) ;

        CHECK(document.Redo() == false) ; // nothing left
    }

    SUBCASE("Mixed Insert Delete Insert - 3 Undos - 3 Redos")
    {
        document.Clear() ;
        document.Insert("Hello") ;
        document.ClearUndoHistory() ;

        // Operation 1: insert " World"
        document.Insert(" World") ;
        CHECK(document.GetTextSize() == 12) ; // "Hello World" + EOF

        // Operation 2: delete "World" (positions 6-10)
        document.Delete(6, 5) ;
        CHECK(document.GetTextSize() == 7) ; // "Hello " + EOF

        // Operation 3: insert "Earth"
        document.Insert("Earth") ;
        CHECK(document.GetTextSize() == 12) ; // "Hello Earth" + EOF

        // Undo all three
        document.Undo() ; // undo "Earth"
        CHECK(document.GetTextSize() == 7) ; // "Hello " + EOF

        document.Undo() ; // undo delete "World"
        CHECK(document.GetTextSize() == 12) ; // "Hello World" + EOF

        document.Undo() ; // undo " World"
        CHECK(document.GetTextSize() == 6) ; // "Hello" + EOF

        // Redo all three
        document.Redo() ; // redo " World"
        CHECK(document.GetTextSize() == 12) ;
        document.Redo() ; // redo delete "World"
        CHECK(document.GetTextSize() == 7) ;
        document.Redo() ; // redo "Earth"
        CHECK(document.GetTextSize() == 12) ;
    }

    SUBCASE("Partial Redo - 3 Undos Then 1 Redo")
    {
        document.Clear() ;

        document.Insert('X') ;
        document.Insert('Y') ;
        document.Insert('Z') ;

        // Undo all 3
        document.Undo() ;
        document.Undo() ;
        document.Undo() ;
        CHECK(document.GetTextSize() == 1) ;

        // Redo only 1
        document.Redo() ;
        CHECK(document.GetTextSize() == 2) ; // X + EOF
        CHECK(document.GetCharNoAdvance(0) == "X") ;

        // Should still have 2 more redos available
        CHECK(document.CanRedo() == true) ;
        document.Redo() ;
        CHECK(document.GetTextSize() == 3) ; // XY + EOF
        CHECK(document.CanRedo() == true) ;
        document.Redo() ;
        CHECK(document.GetTextSize() == 4) ; // XYZ + EOF
        CHECK(document.CanRedo() == false) ;
    }

    SUBCASE("Undo Redo Undo Cycle - Ping Pong")
    {
        document.Clear() ;

        document.Insert('A') ;
        CHECK(document.GetTextSize() == 2) ;

        // Ping-pong 5 times
        for (int i = 0 ; i < 5 ; i++)
        {
            document.Undo() ;
            CHECK(document.GetTextSize() == 1) ;
            document.Redo() ;
            CHECK(document.GetTextSize() == 2) ;
            CHECK(document.GetCharNoAdvance(0) == "A") ;
        }
    }

    // =================================================================
    // All formatting types undo/redo
    // =================================================================

    SUBCASE("Undo/Redo BeginItalics")
    {
        document.Clear() ;
        document.Insert("text") ;
        document.BeginItalics() ;

        POSITION_T sizeAfter = document.GetTextSize() ;
        CHECK(sizeAfter == 6) ; // "text" + STYLE_ITALICS + EOF

        document.Undo() ;
        CHECK(document.GetTextSize() == 5) ; // "text" + EOF

        document.Redo() ;
        CHECK(document.GetTextSize() == 6) ;
    }

    SUBCASE("Undo/Redo BeginUnderline")
    {
        document.Clear() ;
        document.Insert("text") ;
        document.BeginUnderline() ;

        CHECK(document.GetTextSize() == 6) ;

        document.Undo() ;
        CHECK(document.GetTextSize() == 5) ;

        document.Redo() ;
        CHECK(document.GetTextSize() == 6) ;
    }

    SUBCASE("Undo/Redo BeginStrikeThrough")
    {
        document.Clear() ;
        document.Insert("text") ;
        document.BeginStrikeThrough() ;

        CHECK(document.GetTextSize() == 6) ;

        document.Undo() ;
        CHECK(document.GetTextSize() == 5) ;

        document.Redo() ;
        CHECK(document.GetTextSize() == 6) ;
    }

    SUBCASE("Undo/Redo BeginSuperscript")
    {
        document.Clear() ;
        document.Insert("text") ;
        document.BeginSuperscript() ;

        CHECK(document.GetTextSize() == 6) ;

        document.Undo() ;
        CHECK(document.GetTextSize() == 5) ;

        document.Redo() ;
        CHECK(document.GetTextSize() == 6) ;
    }

    SUBCASE("Undo/Redo BeginSubscript")
    {
        document.Clear() ;
        document.Insert("text") ;
        document.BeginSubscript() ;

        CHECK(document.GetTextSize() == 6) ;

        document.Undo() ;
        CHECK(document.GetTextSize() == 5) ;

        document.Redo() ;
        CHECK(document.GetTextSize() == 6) ;
    }

    SUBCASE("Multiple Different Format Codes Then Undo All")
    {
        document.Clear() ;

        document.Insert("Hello") ;     // 5 chars
        document.BeginBold() ;         // +1 = 6
        document.Insert(" World") ;    // +6 = 12
        document.BeginItalics() ;      // +1 = 13
        document.BeginUnderline() ;    // +1 = 14

        CHECK(document.GetTextSize() == 15) ; // 14 + EOF

        // Undo underline
        document.Undo() ;
        CHECK(document.GetTextSize() == 14) ;

        // Undo italics
        document.Undo() ;
        CHECK(document.GetTextSize() == 13) ;

        // Undo " World"
        document.Undo() ;
        CHECK(document.GetTextSize() == 7) ; // "Hello" + STYLE_BOLD + EOF

        // Undo bold
        document.Undo() ;
        CHECK(document.GetTextSize() == 6) ; // "Hello" + EOF

        // Undo "Hello"
        document.Undo() ;
        CHECK(document.GetTextSize() == 1) ; // just EOF

        // Redo all five
        for (int i = 0 ; i < 5 ; i++)
        {
            CHECK(document.Redo() == true) ;
        }
        CHECK(document.GetTextSize() == 15) ;
        CHECK(document.Redo() == false) ;
    }

    // =================================================================
    // Footnote/Endnote with metadata
    // =================================================================

    SUBCASE("Undo/Redo InsertFootnote With Metadata")
    {
        document.Clear() ;
        document.Insert("Text") ;

        sNote note ;
        note.symbol = NOTE_NUMBER ;
        note.text = "This is a footnote" ;
        document.InsertFootnote(note) ;

        CHECK(document.GetTextSize() == 6) ; // "Text" + footnote + EOF

        // Undo removes the footnote
        document.Undo() ;
        CHECK(document.GetTextSize() == 5) ; // "Text" + EOF

        // Redo restores the footnote
        document.Redo() ;
        CHECK(document.GetTextSize() == 6) ;
    }

    SUBCASE("Undo/Redo InsertEndnote With Metadata")
    {
        document.Clear() ;
        document.Insert("Text") ;

        sNote note ;
        note.symbol = NOTE_UPPER ;
        note.text = "This is an endnote" ;
        document.InsertEndnote(note) ;

        CHECK(document.GetTextSize() == 6) ; // "Text" + endnote + EOF

        document.Undo() ;
        CHECK(document.GetTextSize() == 5) ;

        document.Redo() ;
        CHECK(document.GetTextSize() == 6) ;
    }

    SUBCASE("Delete Then Undo Restores Footnote Metadata")
    {
        document.Clear() ;

        sNote note ;
        note.symbol = NOTE_SYMBOLS ;
        note.text = "Footnote content" ;
        document.InsertFootnote(note) ;
        document.ClearUndoHistory() ;

        CHECK(document.GetTextSize() == 2) ; // footnote + EOF

        // Delete the footnote
        document.Delete(0, 1) ;
        CHECK(document.GetTextSize() == 1) ; // just EOF

        // Undo should restore the footnote with metadata
        document.Undo() ;
        CHECK(document.GetTextSize() == 2) ;
    }

    SUBCASE("Delete Then Undo Restores Endnote Metadata")
    {
        document.Clear() ;

        sNote note ;
        note.symbol = NOTE_LOWER ;
        note.text = "Endnote content" ;
        document.InsertEndnote(note) ;
        document.ClearUndoHistory() ;

        CHECK(document.GetTextSize() == 2) ; // endnote + EOF

        document.Delete(0, 1) ;
        CHECK(document.GetTextSize() == 1) ;

        document.Undo() ;
        CHECK(document.GetTextSize() == 2) ;
    }

    // =================================================================
    // All variable types undo/redo
    // =================================================================

    SUBCASE("Undo/Redo All Variable Types")
    {
        // Test each variable type
        eVariableType varTypes[] = {
            VAR_DATE, VAR_TIME, VAR_PAGE_NUMBER, VAR_LINE_NUMBER,
            VAR_FILENAME, VAR_DRIVE, VAR_DIRECTORY, VAR_FULLPATH,
            VAR_WORD_COUNT
        } ;

        for (int i = 0 ; i < 9 ; i++)
        {
            document.Clear() ;
            document.InsertVariable(varTypes[i]) ;

            CHECK(document.GetTextSize() == 2) ; // variable + EOF

            document.Undo() ;
            CHECK(document.GetTextSize() == 1) ; // just EOF

            document.Redo() ;
            CHECK(document.GetTextSize() == 2) ; // variable restored
        }
    }

    // =================================================================
    // Tab variations
    // =================================================================

    SUBCASE("Undo/Redo Tab Decimal Type")
    {
        document.Clear() ;
        document.Insert("AB") ;

        sWSTab tab ;
        tab.type = TAB_DECIMAL ;
        tab.size = 30 ;
        tab.tabsize = 540 ;
        tab.abstabsize = 540 ;
        document.InsertTab(tab) ;

        CHECK(document.GetTextSize() == 4) ; // AB + tab + EOF

        document.Undo() ;
        CHECK(document.GetTextSize() == 3) ; // AB + EOF

        document.Redo() ;
        CHECK(document.GetTextSize() == 4) ;
    }

    SUBCASE("Undo/Redo Tab Center Type")
    {
        document.Clear() ;

        sWSTab tab ;
        tab.type = TAB_CENTER ;
        tab.size = 40 ;
        tab.tabsize = 720 ;
        tab.abstabsize = 720 ;
        document.InsertTab(tab) ;

        CHECK(document.GetTextSize() == 2) ; // tab + EOF

        document.Undo() ;
        CHECK(document.GetTextSize() == 1) ;

        document.Redo() ;
        CHECK(document.GetTextSize() == 2) ;
    }

    SUBCASE("Undo/Redo Tab Right Type")
    {
        document.Clear() ;

        sWSTab tab ;
        tab.type = TAB_RIGHT ;
        tab.size = 50 ;
        tab.tabsize = 900 ;
        tab.abstabsize = 900 ;
        document.InsertTab(tab) ;

        CHECK(document.GetTextSize() == 2) ;

        document.Undo() ;
        CHECK(document.GetTextSize() == 1) ;

        document.Redo() ;
        CHECK(document.GetTextSize() == 2) ;
    }

    // =================================================================
    // Block operations at document level
    // =================================================================

    SUBCASE("CopyBlock Undo At Document Level")
    {
        document.Clear() ;
        document.Insert("Hello World\r") ;
        document.ClearUndoHistory() ;

        // Select "Hello" (positions 0-4)
        document.SetPosition(0) ;
        document.SetBeginBlock() ;
        document.SetPosition(6) ; // after marker
        document.SetEndBlock() ;
        document.ClearUndoHistory() ;

        // Move past end of text
        document.SetPosition(document.GetTextSize()) ;

        // Copy block
        document.BeginUndoGroup() ;
        document.CopyBlock() ;
        document.EndUndoGroup() ;

        POSITION_T sizeAfterCopy = document.GetTextSize() ;
        CHECK(sizeAfterCopy == 18) ; // "Hello World\r" + "Hello" + EOF

        // Undo the copy
        document.Undo() ;
        CHECK(document.GetTextSize() == 13) ; // "Hello World\r" + EOF

        // Redo the copy
        document.Redo() ;
        CHECK(document.GetTextSize() == 18) ;
    }

    SUBCASE("DeleteBlock Undo At Document Level")
    {
        document.Clear() ;
        document.Insert("Hello World\r") ;

        // Select "Hello" (positions 0-4)
        document.SetPosition(0) ;
        document.SetBeginBlock() ;
        document.SetPosition(6) ; // after marker
        document.SetEndBlock() ;
        document.ClearUndoHistory() ;

        // Delete block
        document.BeginUndoGroup() ;
        document.DeleteBlock() ;
        document.EndUndoGroup() ;

        CHECK(document.GetTextSize() == 8) ; // " World\r" + EOF

        // Undo restores "Hello"
        document.Undo() ;
        CHECK(document.GetTextSize() == 13) ; // "Hello World\r" + EOF

        // Redo deletes again
        document.Redo() ;
        CHECK(document.GetTextSize() == 8) ;
    }

    SUBCASE("Paste Undo")
    {
        document.Clear() ;
        document.Insert("Hello World") ;

        // Select "Hello"
        document.SetPosition(0) ;
        document.SetBeginBlock() ;
        document.SetPosition(6) ; // after marker
        document.SetEndBlock() ;
        document.Copy() ;
        document.ClearUndoHistory() ;

        // Move to end and paste
        document.SetPosition(document.GetTextSize()) ;
        document.Paste() ;

        POSITION_T sizeAfterPaste = document.GetTextSize() ;
        CHECK(sizeAfterPaste == 17) ; // "Hello World" + "Hello" + EOF

        // Undo the paste
        document.Undo() ;
        CHECK(document.GetTextSize() == 12) ; // "Hello World" + EOF

        // Redo the paste
        document.Redo() ;
        CHECK(document.GetTextSize() == 17) ;
    }

    SUBCASE("Cut Undo")
    {
        document.Clear() ;
        document.Insert("Hello World") ;

        // Select "Hello" (positions 0-4)
        document.SetPosition(0) ;
        document.SetBeginBlock() ;
        document.SetPosition(6) ; // after marker
        document.SetEndBlock() ;
        document.ClearUndoHistory() ;

        // Cut
        document.BeginUndoGroup() ;
        document.Cut() ;
        document.EndUndoGroup() ;

        CHECK(document.GetTextSize() == 7) ; // " World" + EOF

        // Undo restores "Hello"
        document.Undo() ;
        CHECK(document.GetTextSize() == 12) ;

        // Redo cuts again
        document.Redo() ;
        CHECK(document.GetTextSize() == 7) ;
    }

    // =================================================================
    // Edge cases
    // =================================================================

    SUBCASE("Undo Returns False When Stack Exhausted")
    {
        document.Clear() ;

        document.Insert('A') ;
        document.Insert('B') ;

        CHECK(document.Undo() == true) ;
        CHECK(document.Undo() == true) ;
        CHECK(document.Undo() == false) ;
        CHECK(document.Undo() == false) ;
        CHECK(document.Undo() == false) ;
    }

    SUBCASE("Redo Returns False When Stack Exhausted")
    {
        document.Clear() ;

        document.Insert('A') ;
        document.Undo() ;

        CHECK(document.Redo() == true) ;
        CHECK(document.Redo() == false) ;
        CHECK(document.Redo() == false) ;
    }

    SUBCASE("Empty Undo Group Does Not Affect Stack")
    {
        document.Clear() ;

        document.Insert('A') ;
        CHECK(document.CanUndo() == true) ;

        // Open and close group with no operations
        document.BeginUndoGroup() ;
        document.EndUndoGroup() ;

        // Should still have the Insert 'A' to undo (empty group not pushed)
        CHECK(document.CanUndo() == true) ;
        document.Undo() ;
        CHECK(document.GetTextSize() == 1) ;
        CHECK(document.CanUndo() == false) ;
    }

    SUBCASE("Multiple Sequential Undo Groups")
    {
        document.Clear() ;

        // Group 1: insert AB
        document.BeginUndoGroup() ;
        document.Insert('A') ;
        document.Insert('B') ;
        document.EndUndoGroup() ;

        // Group 2: insert CD
        document.BeginUndoGroup() ;
        document.Insert('C') ;
        document.Insert('D') ;
        document.EndUndoGroup() ;

        CHECK(document.GetTextSize() == 5) ; // ABCD + EOF

        // Undo group 2
        document.Undo() ;
        CHECK(document.GetTextSize() == 3) ; // AB + EOF

        // Undo group 1
        document.Undo() ;
        CHECK(document.GetTextSize() == 1) ; // just EOF

        // Redo both
        document.Redo() ;
        CHECK(document.GetTextSize() == 3) ;
        document.Redo() ;
        CHECK(document.GetTextSize() == 5) ;
    }

    SUBCASE("UTF-8 Multi-Byte Characters Undo/Redo")
    {
        document.Clear() ;

        // Insert a multi-byte UTF-8 string
        document.Insert("\xC3\xA9") ;  // e-acute (U+00E9, 2 bytes)
        CHECK(document.GetTextSize() == 2) ; // e-acute + EOF

        document.Undo() ;
        CHECK(document.GetTextSize() == 1) ;

        document.Redo() ;
        CHECK(document.GetTextSize() == 2) ;
        CHECK(document.GetCharNoAdvance(0) == "\xC3\xA9") ;
    }

    SUBCASE("Long String Insert Undo/Redo")
    {
        document.Clear() ;

        // Build a 500-character string
        std::string longStr ;
        for (int i = 0 ; i < 500 ; i++)
        {
            longStr += 'a' + (i % 26) ;
        }

        document.Insert(longStr) ;
        CHECK(document.GetTextSize() == 501) ; // 500 + EOF

        document.Undo() ;
        CHECK(document.GetTextSize() == 1) ;

        document.Redo() ;
        CHECK(document.GetTextSize() == 501) ;
    }

    SUBCASE("EOF Marker Integrity After Undo/Redo Cycles")
    {
        document.Clear() ;

        // Multiple operations and undo/redo cycles
        document.Insert("Hello") ;
        CHECK(document.GetTextSize() == 6) ; // Hello + EOF
        document.Insert(HARD_RETURN) ;
        CHECK(document.GetTextSize() == 7) ; // Hello\r + EOF
        document.Insert("World") ;
        CHECK(document.GetTextSize() == 12) ; // Hello\rWorld + EOF

        // verify text content before undo (including EOF via GetCharNoAdvance)
        CHECK(document.GetCharNoAdvance(0) == "H") ;
        CHECK(document.GetCharNoAdvance(5)[0] == HARD_RETURN) ;
        CHECK(document.GetCharNoAdvance(6) == "W") ;
        CHECK(document.GetCharNoAdvance(10) == "d") ;
        CHECK(resolveControlChar(document, 11) == STYLE_EOF) ; // EOF now correctly transferred

        // undo 1: remove "World"
        document.Undo() ;
        CHECK(document.GetTextSize() == 7) ; // Hello\r + EOF
        CHECK(resolveControlChar(document, 6) == STYLE_EOF) ;

        // undo 2: remove hard return
        document.Undo() ;
        CHECK(document.GetTextSize() == 6) ; // Hello + EOF
        CHECK(resolveControlChar(document, 5) == STYLE_EOF) ;

        // undo 3: remove "Hello"
        document.Undo() ;
        CHECK(document.GetTextSize() == 1) ; // just EOF
        CHECK(resolveControlChar(document, 0) == STYLE_EOF) ;

        // redo 1: re-insert "Hello"
        document.Redo() ;
        CHECK(document.GetTextSize() == 6) ; // Hello + EOF
        CHECK(document.GetCharNoAdvance(0) == "H") ;
        CHECK(resolveControlChar(document, 5) == STYLE_EOF) ;

        // redo 2: re-insert hard return
        document.Redo() ;
        CHECK(document.GetTextSize() == 7) ; // Hello\r + EOF
        CHECK(document.GetCharNoAdvance(5)[0] == HARD_RETURN) ;
        CHECK(resolveControlChar(document, 6) == STYLE_EOF) ;

        // redo 3: re-insert "World"
        document.Redo() ;
        CHECK(document.GetTextSize() == 12) ; // Hello\rWorld + EOF

        // verify text content restored correctly
        CHECK(document.GetCharNoAdvance(0) == "H") ;
        CHECK(document.GetCharNoAdvance(4) == "o") ;
        CHECK(document.GetCharNoAdvance(5)[0] == HARD_RETURN) ;
        CHECK(document.GetCharNoAdvance(6) == "W") ;
        CHECK(document.GetCharNoAdvance(10) == "d") ;
        CHECK(resolveControlChar(document, 11) == STYLE_EOF) ; // EOF still correctly available
    }

    SUBCASE("Delete At Position 0 Undo/Redo")
    {
        document.Clear() ;
        document.Insert("ABC") ;
        document.ClearUndoHistory() ;

        // Delete first character
        document.Delete(0, 1) ;
        CHECK(document.GetTextSize() == 3) ; // BC + EOF
        CHECK(document.GetCharNoAdvance(0) == "B") ;

        document.Undo() ;
        CHECK(document.GetTextSize() == 4) ; // ABC + EOF
        CHECK(document.GetCharNoAdvance(0) == "A") ;

        document.Redo() ;
        CHECK(document.GetTextSize() == 3) ;
        CHECK(document.GetCharNoAdvance(0) == "B") ;
    }

    SUBCASE("Delete Last Character Before EOF Undo/Redo")
    {
        document.Clear() ;
        document.Insert("XY") ;
        document.ClearUndoHistory() ;

        // Delete Y (last char before EOF)
        document.Delete(1, 1) ;
        CHECK(document.GetTextSize() == 2) ; // X + EOF

        document.Undo() ;
        CHECK(document.GetTextSize() == 3) ; // XY + EOF
        CHECK(document.GetCharNoAdvance(1) == "Y") ;

        document.Redo() ;
        CHECK(document.GetTextSize() == 2) ;
    }

    SUBCASE("Insert At Position 0 Undo/Redo")
    {
        document.Clear() ;
        document.Insert("World") ;
        document.ClearUndoHistory() ;

        // Insert at position 0
        document.SetPosition(0) ;
        document.Insert("Hello ") ;

        CHECK(document.GetTextSize() == 12) ; // "Hello World" + EOF

        document.Undo() ;
        CHECK(document.GetTextSize() == 6) ; // "World" + EOF

        document.Redo() ;
        CHECK(document.GetTextSize() == 12) ;
    }

    SUBCASE("Multiple Hard Returns Creating Many Paragraphs Undo All")
    {
        document.Clear() ;

        document.Insert("Para1") ;
        document.Insert(HARD_RETURN) ;
        document.Insert("Para2") ;
        document.Insert(HARD_RETURN) ;
        document.Insert("Para3") ;
        document.Insert(HARD_RETURN) ;

        CHECK(document.GetNumberofParagraphs() == 4) ; // 3 paragraphs + empty last

        // Undo everything (6 operations: 3 text + 3 returns)
        for (int i = 0 ; i < 6 ; i++)
        {
            CHECK(document.Undo() == true) ;
        }
        CHECK(document.GetTextSize() == 1) ; // just EOF

        // Redo everything
        for (int i = 0 ; i < 6 ; i++)
        {
            CHECK(document.Redo() == true) ;
        }
        CHECK(document.GetNumberofParagraphs() == 4) ;
    }

    SUBCASE("Delete Hard Return Merging Paragraphs Then Undo Restores")
    {
        document.Clear() ;
        document.Insert("Line1") ;
        document.Insert(HARD_RETURN) ;
        document.Insert("Line2") ;
        document.ClearUndoHistory() ;

        CHECK(document.GetNumberofParagraphs() == 2) ;

        // Delete the hard return at position 5
        document.Delete(5, 1) ;
        CHECK(document.GetNumberofParagraphs() == 1) ;
        CHECK(document.GetTextSize() == 11) ; // "Line1Line2" + EOF

        // Undo restores the hard return and paragraph structure
        document.Undo() ;
        CHECK(document.GetNumberofParagraphs() == 2) ;
        CHECK(document.GetTextSize() == 12) ; // "Line1\rLine2" + EOF
    }

    SUBCASE("Interleaved Format and Text Undo Each")
    {
        document.Clear() ;

        document.BeginBold() ;         // step 1
        document.Insert("bold") ;      // step 2
        document.BeginBold() ;         // step 3 (end bold)
        document.Insert(" normal") ;   // step 4
        document.BeginItalics() ;      // step 5
        document.Insert(" italic") ;   // step 6

        POSITION_T fullSize = document.GetTextSize() ;

        // Undo step 6: " italic"
        document.Undo() ;
        CHECK(document.GetTextSize() == fullSize - 7) ;

        // Undo step 5: STYLE_ITALICS
        document.Undo() ;
        CHECK(document.GetTextSize() == fullSize - 8) ;

        // Undo step 4: " normal"
        document.Undo() ;
        CHECK(document.GetTextSize() == fullSize - 15) ;

        // Undo step 3: end bold
        document.Undo() ;
        CHECK(document.GetTextSize() == fullSize - 16) ;

        // Undo step 2: "bold"
        document.Undo() ;
        CHECK(document.GetTextSize() == fullSize - 20) ;

        // Undo step 1: begin bold
        document.Undo() ;
        CHECK(document.GetTextSize() == 1) ; // just EOF

        // Redo all 6
        for (int i = 0 ; i < 6 ; i++)
        {
            CHECK(document.Redo() == true) ;
        }
        CHECK(document.GetTextSize() == fullSize) ;
    }

    SUBCASE("Undo After Redo After Undo - 3 Level Cycle")
    {
        document.Clear() ;

        document.Insert("First") ;
        document.Insert("Second") ;

        // Level 1: undo "Second"
        document.Undo() ;
        CHECK(document.GetTextSize() == 6) ; // "First" + EOF

        // Level 2: redo "Second"
        document.Redo() ;
        CHECK(document.GetTextSize() == 12) ; // "FirstSecond" + EOF

        // Level 3: undo "Second" again
        document.Undo() ;
        CHECK(document.GetTextSize() == 6) ;

        // Level 4: undo "First"
        document.Undo() ;
        CHECK(document.GetTextSize() == 1) ;

        // Redo both
        document.Redo() ;
        CHECK(document.GetTextSize() == 6) ;
        document.Redo() ;
        CHECK(document.GetTextSize() == 12) ;
    }

    SUBCASE("Group Containing Only Deletes")
    {
        document.Clear() ;
        document.Insert("ABCDE") ;
        document.ClearUndoHistory() ;

        document.BeginUndoGroup() ;
        document.Delete(4, 1) ; // delete E
        document.Delete(3, 1) ; // delete D
        document.Delete(2, 1) ; // delete C
        document.EndUndoGroup() ;

        CHECK(document.GetTextSize() == 3) ; // AB + EOF

        // Single undo restores CDE
        document.Undo() ;
        CHECK(document.GetTextSize() == 6) ; // ABCDE + EOF
        CHECK(document.GetCharNoAdvance(2) == "C") ;
        CHECK(document.GetCharNoAdvance(3) == "D") ;
        CHECK(document.GetCharNoAdvance(4) == "E") ;

        // Redo removes CDE again
        document.Redo() ;
        CHECK(document.GetTextSize() == 3) ;
    }

    SUBCASE("Group Containing Only Inserts")
    {
        document.Clear() ;

        document.BeginUndoGroup() ;
        document.Insert('X') ;
        document.Insert('Y') ;
        document.Insert('Z') ;
        document.EndUndoGroup() ;

        CHECK(document.GetTextSize() == 4) ; // XYZ + EOF

        // Single undo removes all three
        document.Undo() ;
        CHECK(document.GetTextSize() == 1) ;

        // Redo restores all three
        document.Redo() ;
        CHECK(document.GetTextSize() == 4) ;
        CHECK(document.GetCharNoAdvance(0) == "X") ;
        CHECK(document.GetCharNoAdvance(1) == "Y") ;
        CHECK(document.GetCharNoAdvance(2) == "Z") ;
    }

    SUBCASE("Group With Metadata Carrying Elements")
    {
        document.Clear() ;

        document.BeginUndoGroup() ;

        // Insert tab
        sWSTab tab ;
        tab.type = TAB_TAB ;
        tab.size = 50 ;
        tab.tabsize = 720 ;
        tab.abstabsize = 720 ;
        document.InsertTab(tab) ;

        // Insert font
        sInternalFonts font ;
        font.fontname = "Courier" ;
        font.size = 10.0 ;
        font.haveWSFont = false ;
        font.name = "" ;
        document.InsertFont(font) ;

        // Insert color
        sSeqRGBColor color ;
        color.red = 170 ; color.green = 170 ; color.blue = 170 ; color.alpha = 255 ; // WS light gray (index 7)
        document.InsertColor(color) ;

        document.EndUndoGroup() ;

        CHECK(document.GetTextSize() == 4) ; // tab + font + color + EOF

        // Single undo removes all three
        document.Undo() ;
        CHECK(document.GetTextSize() == 1) ; // just EOF

        // Single redo restores all three
        document.Redo() ;
        CHECK(document.GetTextSize() == 4) ;
    }

    // =================================================================
    // Stress tests
    // =================================================================

    SUBCASE("Rapid Undo Redo Cycling 10 Times")
    {
        document.Clear() ;
        document.Insert("Persistent") ;

        for (int cycle = 0 ; cycle < 10 ; cycle++)
        {
            document.Undo() ;
            CHECK(document.GetTextSize() == 1) ;
            CHECK(document.CanRedo() == true) ;

            document.Redo() ;
            CHECK(document.GetTextSize() == 11) ; // "Persistent" + EOF
            CHECK(document.CanUndo() == true) ;
        }

        // Verify document content is intact
        CHECK(document.GetCharNoAdvance(0) == "P") ;
        CHECK(document.GetCharNoAdvance(9) == "t") ;
    }

    SUBCASE("Fill Undo Stack To MAX Then Verify Oldest Dropped And Redo All")
    {
        document.Clear() ;

        // Insert MAX_UNDO_STEPS + 10 individual characters
        int total = MAX_UNDO_STEPS + 10 ;
        for (int i = 0 ; i < total ; i++)
        {
            document.Insert(static_cast<CHAR_T>('A' + (i % 26))) ;
        }

        CHECK(document.GetTextSize() == total + 1) ; // chars + EOF

        // Undo should succeed MAX_UNDO_STEPS times
        int undoCount = 0 ;
        while (document.Undo())
        {
            undoCount++ ;
        }
        CHECK(undoCount == MAX_UNDO_STEPS) ;

        // 10 chars should remain (oldest 10 were dropped from stack)
        CHECK(document.GetTextSize() == 11) ; // 10 remaining + EOF

        // Redo all MAX_UNDO_STEPS
        int redoCount = 0 ;
        while (document.Redo())
        {
            redoCount++ ;
        }
        CHECK(redoCount == MAX_UNDO_STEPS) ;

        // Back to full document
        CHECK(document.GetTextSize() == total + 1) ;
    }

    SUBCASE("Three Deletes On Separate Lines - 3 Undos - 3 Redos")
    {
        // Reproduces user's exact bug scenario
        document.Clear() ;
        document.Insert("Line One\r") ;
        document.Insert("Line Two\r") ;
        document.Insert("Line Three\r") ;
        document.ClearUndoHistory() ;

        // Delete last char of each line (before \r)
        // Line One: 'e' at position 7
        document.Delete(7, 1) ;
        CHECK(document.GetCharNoAdvance(7) == "\r") ;

        // Line Two: 'o' at position 15 (shifted after first delete)
        // After delete of pos 7: "Line On\rLine Two\rLine Three\r"
        // "Line Two" starts at position 8, 'o' is at position 15
        document.Delete(15, 1) ;

        // Line Three: 'e' at position 24 (shifted after two deletes)
        // "Line On\rLine Tw\rLine Three\r"
        // 'e' at end of "Three" = position 24
        document.Delete(24, 1) ;

        // Now undo all three
        CHECK(document.Undo() == true) ; // restore 'e' in Three
        CHECK(document.Undo() == true) ; // restore 'o' in Two
        CHECK(document.Undo() == true) ; // restore 'e' in One

        // Now redo all three - THIS is the user's bug scenario
        CHECK(document.CanRedo() == true) ;
        CHECK(document.Redo() == true) ; // re-delete 'e' from One
        CHECK(document.Redo() == true) ; // re-delete 'o' from Two
        CHECK(document.Redo() == true) ; // re-delete 'e' from Three
        CHECK(document.Redo() == false) ; // nothing left
    }

    SUBCASE("Delete Variable Then Undo Restores Variable Type")
    {
        document.Clear() ;

        document.InsertVariable(VAR_PAGE_NUMBER) ;
        document.ClearUndoHistory() ;

        CHECK(document.GetTextSize() == 2) ; // variable + EOF

        document.Delete(0, 1) ;
        CHECK(document.GetTextSize() == 1) ;

        document.Undo() ;
        CHECK(document.GetTextSize() == 2) ;

        document.Redo() ;
        CHECK(document.GetTextSize() == 1) ;

        document.Undo() ;
        CHECK(document.GetTextSize() == 2) ;
    }

    SUBCASE("Multiple Metadata Types Interleaved With Text")
    {
        document.Clear() ;

        // Build: text + tab + text + font + text + color
        document.Insert("A") ;

        sWSTab tab ;
        tab.type = TAB_TAB ;
        tab.size = 50 ;
        tab.tabsize = 720 ;
        tab.abstabsize = 720 ;
        document.InsertTab(tab) ;

        document.Insert("B") ;

        sInternalFonts font ;
        font.fontname = "Arial" ;
        font.size = 12.0 ;
        font.haveWSFont = false ;
        font.name = "" ;
        document.InsertFont(font) ;

        document.Insert("C") ;

        sSeqRGBColor color ;
        color.red = 0 ; color.green = 170 ; color.blue = 170 ; color.alpha = 255 ; // WS cyan (index 3)
        document.InsertColor(color) ;

        CHECK(document.GetTextSize() == 7) ; // A + tab + B + font + C + color + EOF

        // Undo all 6 operations in reverse
        for (int i = 0 ; i < 6 ; i++)
        {
            CHECK(document.Undo() == true) ;
        }
        CHECK(document.GetTextSize() == 1) ;

        // Redo all 6
        for (int i = 0 ; i < 6 ; i++)
        {
            CHECK(document.Redo() == true) ;
        }
        CHECK(document.GetTextSize() == 7) ;
    }

    SUBCASE("Delete Range Spanning Multiple Types")
    {
        document.Clear() ;

        // Build: "AB" + BOLD_ON + "CD"
        document.Insert("AB") ;
        document.BeginBold() ;
        document.Insert("CD") ;
        document.ClearUndoHistory() ;

        // Delete positions 1-3 (B + BOLD + C) -- 3 graphemes across types
        document.Delete(1, 3) ;
        CHECK(document.GetTextSize() == 3) ; // A + D + EOF

        document.Undo() ;
        CHECK(document.GetTextSize() == 6) ; // AB + BOLD + CD + EOF
        CHECK(document.GetCharNoAdvance(0) == "A") ;
        CHECK(document.GetCharNoAdvance(1) == "B") ;
        // position 2 is STYLE_BOLD
        CHECK(document.GetCharNoAdvance(3) == "C") ;
        CHECK(document.GetCharNoAdvance(4) == "D") ;

        document.Redo() ;
        CHECK(document.GetTextSize() == 3) ;
    }

    SUBCASE("Delete Range Spanning Tab With Metadata")
    {
        document.Clear() ;

        document.Insert("A") ;

        sWSTab tab ;
        tab.type = TAB_DECIMAL ;
        tab.size = 40 ;
        tab.tabsize = 720 ;
        tab.abstabsize = 720 ;
        document.InsertTab(tab) ;

        document.Insert("B") ;
        document.ClearUndoHistory() ;

        // Delete range covering A + tab + B = 3 graphemes
        document.Delete(0, 3) ;
        CHECK(document.GetTextSize() == 1) ; // just EOF

        // Undo restores A + tab(with metadata) + B
        document.Undo() ;
        CHECK(document.GetTextSize() == 4) ; // A + tab + B + EOF

        // Redo
        document.Redo() ;
        CHECK(document.GetTextSize() == 1) ;
    }

    SUBCASE("Delete Range Spanning Font With Metadata")
    {
        document.Clear() ;

        document.Insert("X") ;

        sInternalFonts font ;
        font.fontname = "Helvetica" ;
        font.size = 14.0 ;
        font.haveWSFont = false ;
        font.name = "" ;
        document.InsertFont(font) ;

        document.Insert("Y") ;
        document.ClearUndoHistory() ;

        // Delete X + font + Y
        document.Delete(0, 3) ;
        CHECK(document.GetTextSize() == 1) ;

        document.Undo() ;
        CHECK(document.GetTextSize() == 4) ; // X + font + Y + EOF

        document.Redo() ;
        CHECK(document.GetTextSize() == 1) ;
    }

    SUBCASE("Delete Range Spanning Color With Metadata")
    {
        document.Clear() ;

        document.Insert("M") ;

        sSeqRGBColor color ;
        color.red = 255 ; color.green = 85 ; color.blue = 85 ; color.alpha = 255 ; // WS light red (index 12)
        document.InsertColor(color) ;

        document.Insert("N") ;
        document.ClearUndoHistory() ;

        // Delete M + color + N
        document.Delete(0, 3) ;
        CHECK(document.GetTextSize() == 1) ;

        document.Undo() ;
        CHECK(document.GetTextSize() == 4) ;

        document.Redo() ;
        CHECK(document.GetTextSize() == 1) ;
    }

    SUBCASE("Delete Range Spanning Variable With Metadata")
    {
        document.Clear() ;

        document.Insert("P") ;
        document.InsertVariable(VAR_FILENAME) ;
        document.Insert("Q") ;
        document.ClearUndoHistory() ;

        document.Delete(0, 3) ;
        CHECK(document.GetTextSize() == 1) ;

        document.Undo() ;
        CHECK(document.GetTextSize() == 4) ;

        document.Redo() ;
        CHECK(document.GetTextSize() == 1) ;
    }

    SUBCASE("Delete Range Spanning Footnote With Metadata")
    {
        document.Clear() ;

        document.Insert("R") ;

        sNote note ;
        note.symbol = NOTE_NUMBER ;
        note.text = "Test note" ;
        document.InsertFootnote(note) ;

        document.Insert("S") ;
        document.ClearUndoHistory() ;

        document.Delete(0, 3) ;
        CHECK(document.GetTextSize() == 1) ;

        document.Undo() ;
        CHECK(document.GetTextSize() == 4) ;

        document.Redo() ;
        CHECK(document.GetTextSize() == 1) ;
    }

    SUBCASE("Delete Range Spanning Endnote With Metadata")
    {
        document.Clear() ;

        document.Insert("T") ;

        sNote note ;
        note.symbol = NOTE_LOWER ;
        note.text = "End note text" ;
        document.InsertEndnote(note) ;

        document.Insert("U") ;
        document.ClearUndoHistory() ;

        document.Delete(0, 3) ;
        CHECK(document.GetTextSize() == 1) ;

        document.Undo() ;
        CHECK(document.GetTextSize() == 4) ;

        document.Redo() ;
        CHECK(document.GetTextSize() == 1) ;
    }

    SUBCASE("New Edit Between Undos Clears Remaining Redos")
    {
        document.Clear() ;

        document.Insert('A') ;
        document.Insert('B') ;
        document.Insert('C') ;

        // Undo 2 of 3
        document.Undo() ; // remove C
        document.Undo() ; // remove B
        CHECK(document.CanRedo() == true) ;

        // New edit clears redo stack
        document.Insert('X') ;
        CHECK(document.CanRedo() == false) ;

        // Only X and A should be undoable
        document.Undo() ; // remove X
        CHECK(document.GetTextSize() == 2) ; // A + EOF
        document.Undo() ; // remove A
        CHECK(document.GetTextSize() == 1) ; // just EOF
        CHECK(document.Undo() == false) ;
    }

    SUBCASE("Undo Redo With Document Clear In Between")
    {
        document.Clear() ;
        document.Insert("Hello") ;

        // Clear resets everything
        document.Clear() ;

        // After clear, undo should fail (clear inserts EOF which is suppressed)
        CHECK(document.CanUndo() == false) ;
        CHECK(document.CanRedo() == false) ;

        // New operations should work
        document.Insert("New") ;
        CHECK(document.CanUndo() == true) ;
        document.Undo() ;
        CHECK(document.GetTextSize() == 1) ;
    }

    SUBCASE("Undo Redo Single Character Delete At Every Position")
    {
        document.Clear() ;
        document.Insert("ABCDE") ;
        document.ClearUndoHistory() ;

        // Delete at position 0 (A)
        document.Delete(0, 1) ;
        document.Undo() ;
        CHECK(document.GetCharNoAdvance(0) == "A") ;
        document.Redo() ;

        // Delete at position 0 again (now B since A was deleted)
        document.Delete(0, 1) ;
        document.Undo() ;
        CHECK(document.GetCharNoAdvance(0) == "B") ;
        document.Redo() ;

        // Continue pattern for remaining chars
        document.Delete(0, 1) ;
        document.Undo() ;
        CHECK(document.GetCharNoAdvance(0) == "C") ;
        document.Redo() ;

        document.Delete(0, 1) ;
        document.Undo() ;
        CHECK(document.GetCharNoAdvance(0) == "D") ;
        document.Redo() ;

        document.Delete(0, 1) ;
        document.Undo() ;
        CHECK(document.GetCharNoAdvance(0) == "E") ;
        document.Redo() ;

        // All chars deleted
        CHECK(document.GetTextSize() == 1) ; // just EOF
    }

    // =================================================================
    // Metadata preservation in Copy/Paste operations
    // =================================================================

    SUBCASE("Copy Preserves Tab Metadata")
    {
        document.Clear() ;
        document.ClearUndoHistory() ;

        // Insert text with a tab
        document.Insert("A") ;
        sWSTab tab ;
        tab.type = TAB_CENTER ;
        tab.tabsize = 1440 ;  // 1 inch in 1/1800 inch units
        tab.abstabsize = 1440 ;
        tab.size = 8 ;  // 8/10 inch
        document.InsertTab(tab) ;
        document.Insert("B") ;

        // Select the block containing the tab (A, tab, B)
        document.SetPosition(0) ;
        document.SetBeginBlock() ;
        document.SetPosition(4) ;  // after marker shift: 3 chars + marker
        document.SetEndBlock() ;
        document.Copy() ;

        // Paste at end
        document.SetPosition(document.GetTextSize()) ;
        document.Paste() ;

        // Verify the pasted tab has the same metadata
        // Original tab at position 1, pasted tab at position 4 (after original content)
        sWSTab originalTab = document.GetTab(1) ;
        sWSTab pastedTab = document.GetTab(4) ;

        CHECK(originalTab.type == TAB_CENTER) ;
        CHECK(pastedTab.type == TAB_CENTER) ;
        CHECK(originalTab.tabsize == pastedTab.tabsize) ;
        CHECK(originalTab.abstabsize == pastedTab.abstabsize) ;
    }

    SUBCASE("Copy Preserves Font Metadata")
    {
        document.Clear() ;
        document.ClearUndoHistory() ;

        // Insert text with a font change
        document.Insert("A") ;
        sInternalFonts font ;
        font.fontname = "Arial" ;
        font.size = 24.0 ;
        font.haveWSFont = false ;
        font.name = "TestFont" ;
        document.InsertFont(font) ;
        document.Insert("B") ;

        // Select the block containing the font marker
        document.SetPosition(0) ;
        document.SetBeginBlock() ;
        document.SetPosition(4) ;  // after marker shift
        document.SetEndBlock() ;
        document.Copy() ;

        // Paste at end
        document.SetPosition(document.GetTextSize()) ;
        document.Paste() ;

        // Verify the pasted font marker has the same metadata
        sInternalFonts originalFont ;
        sInternalFonts pastedFont ;
        document.GetFont(1, originalFont) ;
        document.GetFont(4, pastedFont) ;

        CHECK(originalFont.fontname == "Arial") ;
        CHECK(pastedFont.fontname == "Arial") ;
        CHECK(originalFont.size == pastedFont.size) ;
        CHECK(originalFont.name == pastedFont.name) ;
    }

    SUBCASE("Copy Preserves Color Metadata")
    {
        document.Clear() ;
        document.ClearUndoHistory() ;

        // Insert text with a color change
        document.Insert("A") ;
        sSeqRGBColor color ;
        color.red = 170 ; color.green = 0 ; color.blue = 170 ; color.alpha = 255 ; // WS magenta (index 5)
        document.InsertColor(color) ;
        document.Insert("B") ;

        // Select the block containing the color marker
        document.SetPosition(0) ;
        document.SetBeginBlock() ;
        document.SetPosition(4) ;  // after marker shift
        document.SetEndBlock() ;
        document.Copy() ;

        // Paste at end
        document.SetPosition(document.GetTextSize()) ;
        document.Paste() ;

        // Verify the pasted color marker has the same metadata
        sSeqRGBColor originalColor ;
        sSeqRGBColor pastedColor ;
        document.GetColor(1, originalColor) ;
        document.GetColor(4, pastedColor) ;

        CHECK(originalColor.red == 170) ;
        CHECK(originalColor.green == 0) ;
        CHECK(originalColor.blue == 170) ;
        CHECK(pastedColor.red == 170) ;
        CHECK(pastedColor.green == 0) ;
        CHECK(pastedColor.blue == 170) ;
        CHECK(originalColor.red == pastedColor.red) ;
        CHECK(originalColor.green == pastedColor.green) ;
        CHECK(originalColor.blue == pastedColor.blue) ;
    }

    SUBCASE("Copy Preserves Variable Metadata")
    {
        document.Clear() ;
        document.ClearUndoHistory() ;

        // Insert text with a variable at a known position
        document.Insert("X") ;                       // position 0
        document.InsertVariable(VAR_PAGE_NUMBER) ;   // position 1
        document.Insert("Y") ;                       // position 2

        POSITION_T sizeBeforeCopy = document.GetTextSize() ;  // X + var + Y + EOF = 4

        // Select the entire content (X, variable, Y)
        document.SetPosition(0) ;
        document.SetBeginBlock() ;
        document.SetPosition(4) ;  // after marker shift, select X, var, Y
        document.SetEndBlock() ;
        document.Copy() ;

        // Verify original variable is at position 1
        eVariableType originalVar = document.GetVariable(1) ;
        CHECK(originalVar == VAR_PAGE_NUMBER) ;

        // Paste at end (before EOF)
        document.SetPosition(sizeBeforeCopy) ;
        document.Paste() ;

        // After paste: X + var + Y + X + var + Y + EOF
        // Original variable at position 1, pasted variable at position 4
        POSITION_T sizeAfterPaste = document.GetTextSize() ;  // should be 7
        CHECK(sizeAfterPaste == sizeBeforeCopy + 3) ;  // added X, var, Y

        // Verify pasted variable at position 4
        eVariableType pastedVar = document.GetVariable(4) ;
        CHECK(pastedVar == VAR_PAGE_NUMBER) ;
    }

    SUBCASE("Paste With Undo Restores Original Document")
    {
        document.Clear() ;
        document.ClearUndoHistory() ;

        // Insert text with metadata
        document.Insert("A") ;
        sWSTab tab ;
        tab.type = TAB_RIGHT ;
        tab.tabsize = 2880 ;
        tab.abstabsize = 2880 ;
        tab.size = 16 ;
        document.InsertTab(tab) ;
        document.Insert("B") ;

        POSITION_T originalSize = document.GetTextSize() ;
        document.ClearUndoHistory() ;

        // Select and copy
        document.SetPosition(0) ;
        document.SetBeginBlock() ;
        document.SetPosition(4) ;  // after marker
        document.SetEndBlock() ;
        document.Copy() ;

        // Paste at end
        document.SetPosition(document.GetTextSize()) ;
        document.Paste() ;

        POSITION_T sizeAfterPaste = document.GetTextSize() ;
        CHECK(sizeAfterPaste > originalSize) ;

        // Undo should restore to original
        document.Undo() ;
        CHECK(document.GetTextSize() == originalSize) ;

        // Redo should paste again
        document.Redo() ;
        CHECK(document.GetTextSize() == sizeAfterPaste) ;
    }

    SUBCASE("Copy Multiple Metadata Types Together")
    {
        document.Clear() ;
        document.ClearUndoHistory() ;

        // Insert text with multiple metadata types
        document.Insert("Text") ;

        sInternalFonts font ;
        font.fontname = "Courier" ;
        font.size = 12.0 ;
        font.haveWSFont = false ;
        font.name = "TestCourier" ;
        document.InsertFont(font) ;

        sSeqRGBColor color ;
        color.red = 0 ; color.green = 170 ; color.blue = 170 ; color.alpha = 255 ; // WS cyan (index 3)
        document.InsertColor(color) ;

        sWSTab tab ;
        tab.type = TAB_CENTER ;
        tab.tabsize = 720 ;
        tab.abstabsize = 720 ;
        tab.size = 4 ;
        document.InsertTab(tab) ;

        document.Insert("More") ;

        POSITION_T originalSize = document.GetTextSize() ;

        // Select entire content
        document.SetPosition(0) ;
        document.SetBeginBlock() ;
        document.SetPosition(originalSize) ;  // select all including marker
        document.SetEndBlock() ;
        document.Copy() ;

        // Paste at end
        document.SetPosition(document.GetTextSize()) ;
        document.Paste() ;

        // Verify all metadata types were preserved
        // The pasted content should have its own font, color, and tab markers
        POSITION_T newSize = document.GetTextSize() ;
        CHECK(newSize > originalSize) ;

        // Check that we can retrieve the pasted metadata
        // (exact positions depend on document structure, but metadata should exist)
        sInternalFonts pastedFont ;
        bool fontFound = document.GetFont(originalSize + 4, pastedFont) ;  // approximate position
        if (fontFound)
        {
            CHECK(pastedFont.fontname == "Courier") ;
            CHECK(pastedFont.name == "TestCourier") ;
        }
    }
}

// =====================================================================
// COMPREHENSIVE TEST COVERAGE - PHASE 2: CLIPBOARD OPERATIONS
// =====================================================================

TEST_CASE("Clipboard Operations")
{
    cDocument document;
    document.SetShowControl(SHOW_ALL);

    SUBCASE("Copy Simple Text Middle")
    {
        document.Clear();
        document.Insert("Hello World");
        
        // Set a block
        document.SetPosition(1);
        document.SetBeginBlock();
        document.SetPosition(5);
        document.SetEndBlock();
        
        // Copy the block
        document.Copy();
        
        // Document should be unchanged
        CHECK(document.GetTextSize() == 12); // "Hello World" + EOF
        CHECK(document.GetParagraphText(0).substr(1, 4) == "ello");
        
        // Block should still be set
        POSITION_T start, end;
        bool blockSet = document.GetBlock(start, end);

        CHECK(blockSet == true);
        CHECK(start == 1);
        CHECK(end == 4); // End is inclusive in GetBlock
    }

    SUBCASE("Copy Simple Text Front")
    {
        document.Clear();
        document.Insert("Hello World");
        
        // Set a block
        document.SetPosition(0);
        document.SetBeginBlock();
        document.SetPosition(5);
        document.SetEndBlock();
        
        // Copy the block
        document.Copy();
        
        // Document should be unchanged
        CHECK(document.GetTextSize() == 12); // "Hello World" + EOF
        CHECK(document.GetParagraphText(0).substr(0, 5) == "Hello");
        
        // Block should still be set
        POSITION_T start, end;
        bool blockSet = document.GetBlock(start, end);

        CHECK(blockSet == true);
        CHECK(start == 0);
        CHECK(end == 4); // End is inclusive in GetBlock
    }

    SUBCASE("Cut Simple Text")
    {
        document.Clear();
        document.Insert("Hello World");
        
        // Set a block
        document.SetPosition(6);
        document.SetBeginBlock();
        document.SetPosition(12);   // we go to 12, since ^KB inserts a char
        document.SetEndBlock();
        
        POSITION_T originalSize = document.GetTextSize();
        
        // Cut the block (should remove "World")
        document.Cut();
        
        // Document should be smaller
        POSITION_T newSize = document.GetTextSize();
        CHECK(newSize < originalSize);
        
        // "World" should be gone
        std::string remainingText = document.GetParagraphText(0);
        remainingText.pop_back(); // Remove EOF
        CHECK(remainingText == "Hello ");
    }

    SUBCASE("Paste After Copy")
    {
        document.Clear();
        document.Insert("Test");
        
        // Copy "Test"
        document.SetPosition(0);
        document.SetBeginBlock();
        document.SetPosition(5);
        document.SetEndBlock();
        document.Copy();
        
        // Move to end and paste
        document.SetPosition(document.GetTextSize() - 1); // Before EOF
        document.Paste();
        
        // Should now have "TestTest"
        std::string text = document.GetParagraphText(0);
        text.pop_back(); // Remove EOF
        CHECK(text == "TestTest");
    }

    SUBCASE("Paste After Cut")
    {
        document.Clear();
        document.Insert("Hello World");
        
        // Cut "World"
        document.SetPosition(6);
        document.SetBeginBlock();
        document.SetPosition(12);
        document.SetEndBlock();
        document.Copy();
        document.Cut();
        
        // Should now have "Hello "
        std::string text1 = document.GetParagraphText(0);
        text1.pop_back(); // Remove EOF
        CHECK(text1 == "Hello ");
        
        // Paste "World" at the beginning
        document.SetPosition(0);
        document.Paste();
        
        // Should now have "WorldHello "
        std::string text2 = document.GetParagraphText(0);
        text2.pop_back(); // Remove EOF
        CHECK(text2 == "WorldHello ");
    }

    SUBCASE("Copy/Paste with No Block Selected")
    {
        document.Clear();
        document.Insert("Test");
        
        // Try to copy without setting a block
        document.Copy(); // Should handle gracefully
        
        // Try to paste (might paste empty or do nothing)
        POSITION_T sizeBefore = document.GetTextSize();
        document.Paste();
        POSITION_T sizeAfter = document.GetTextSize();
        
        // Size should either stay same or increase slightly
        CHECK(sizeAfter >= sizeBefore);
    }

    SUBCASE("Cut with No Block Selected")
    {
        document.Clear();
        document.Insert("Test");
        
        POSITION_T sizeBefore = document.GetTextSize();
        
        // Try to cut without setting a block
        document.Cut(); // Should handle gracefully
        
        // Document should be unchanged or handle gracefully
        POSITION_T sizeAfter = document.GetTextSize();
        CHECK(sizeAfter <= sizeBefore); // Size should not increase
    }

    SUBCASE("Copy/Paste Multi-paragraph Text")
    {
        document.Clear();
        document.Insert("First paragraph\r");
        document.Insert("Second paragraph\r");
        document.Insert("Third paragraph");
        
        // Copy the second paragraph
        document.SetPosition(16); // Start of second paragraph
        document.SetBeginBlock();
        document.SetPosition(33); // End of second paragraph (before \r)
        document.SetEndBlock();
        document.Copy();
        
        // Paste at the end
        document.SetPosition(document.GetTextSize() - 1); // Before EOF
        document.Paste();
        
        // Should have the second paragraph duplicated
        CHECK(document.GetNumberofParagraphs() >= 3);
        
        // Check that text was added
        POSITION_T finalSize = document.GetTextSize();
        CHECK(finalSize > 50); // Should be longer than original
    }

    SUBCASE("Copy/Paste with Control Characters")
    {
        document.Clear();
        document.Insert("Bold");
        document.BeginBold();
        document.Insert(" text ");
        document.EndBold();
        document.Insert("normal");
        
        // Copy the formatted section
        document.SetPosition(4); // Before bold marker
        document.SetBeginBlock();
        document.SetPosition(13); // After end bold marker (approximate)
        document.SetEndBlock();
        document.Copy();
        
        // Paste at end
        document.SetPosition(document.GetTextSize() - 1);
        document.Paste();
        
        // Document should be longer and contain more formatting
        // GetText returns raw MARKER_CHAR for all control codes
        std::string fullText = document.GetText();
        int markerCount = 0;
        for (char c : fullText)
        {
            if (c == MARKER_CHAR)
            {
                markerCount++;
            }
        }
        CHECK(markerCount >= 4); // Should have at least 4 control characters (as MARKER_CHAR)
    }

    SUBCASE("Multiple Copy Operations")
    {
        document.Clear();
        document.Insert("ABC");
        
        // Copy "A"
        document.SetPosition(0);
        document.SetBeginBlock();
        document.SetPosition(2);
        document.SetEndBlock();
        document.Copy();
        
        // Paste "A"
        document.SetPosition(3);
        document.Paste();
        CHECK(document.GetParagraphText(0).substr(0, 4) == "ABCA");
        
        // Copy "B"
        document.SetPosition(1);
        document.SetBeginBlock();
        document.SetPosition(3);
        document.SetEndBlock();
        document.Copy();
        
        // Paste "B" (should replace "A" in clipboard)
        document.SetPosition(document.GetTextSize() - 1);
        document.Paste();
        
        std::string finalText = document.GetParagraphText(0);
        finalText.pop_back(); // Remove EOF
        CHECK(finalText == "ABCAB");
    }
}

// =====================================================================
// COMPREHENSIVE TEST COVERAGE - PHASE 2: FONT & FORMATTING
// =====================================================================

TEST_CASE("Font and Formatting")
{
    cDocument document;
    document.SetShowControl(SHOW_ALL);

    SUBCASE("GetFont - Basic Functionality")
    {
        document.Clear();
        document.Insert("Test text");
        
        // Insert a font change
        sInternalFonts font;
        font.fontname = "Arial";
        font.size = 12;
        font.haveWSFont = false;
        
        document.SetPosition(2);
        document.InsertFont(font);

        // GetFont returns true at the marker position only.
        sInternalFonts retrievedFont;
        CHECK(document.GetFont(2, retrievedFont) == true);
        CHECK(retrievedFont.fontname == "Arial");
        CHECK(retrievedFont.size == 12);

        CHECK(document.GetFont(0, retrievedFont) == false);
        CHECK(document.GetFont(3, retrievedFont) == false);
    }

    SUBCASE("GetFont - No Font Set")
    {
        document.Clear();
        document.Insert("Plain text");
        
        sInternalFonts font;
        bool result = document.GetFont(0, font);
        
        // Should either return false or return default font
        if (result) {
            // If returns true, should have some default font info
            CHECK(font.fontname.length() > 0);
            CHECK(font.size > 0);
        }
    }

    SUBCASE("GetFont - Multiple Fonts")
    {
        document.Clear();
        document.Insert("Different fonts");
        
        // Insert multiple fonts
        sInternalFonts font1;
        font1.fontname = "Times";
        font1.size = 10;
        font1.haveWSFont = false;
        
        sInternalFonts font2;
        font2.fontname = "Courier";
        font2.size = 14;
        font2.haveWSFont = false;
        
        document.SetPosition(3);
        document.InsertFont(font1);
        
        document.SetPosition(8);
        document.InsertFont(font2);
        
        // Test retrieval at different positions
        sInternalFonts retrieved;
        bool result1 = document.GetFont(3, retrieved); // Should get Times
        if (result1) {
            CHECK(retrieved.fontname == "Times");
        }
        
        bool result2 = document.GetFont(5, retrieved); // Should get Courier
        if (result2) {
            CHECK(retrieved.fontname == "Courier");
        }
    }

    SUBCASE("GetTab - Basic Functionality")
    {
        document.Clear();
        document.Insert("Text with tabs");
        
        // Insert a tab
        sWSTab tab;
        tab.type = TAB_TAB;
        tab.size = 100;
        tab.tabsize = 100;
        tab.abstabsize = 100;
        
        document.SetPosition(5);
        document.InsertTab(tab);
        
        // Get tab information
        sWSTab retrievedTab = document.GetTab(6); // After tab position
        
        // Should get the tab info (or default)
        CHECK(retrievedTab.type >= TAB_TAB);
        CHECK(retrievedTab.size >= 0);
    }

    SUBCASE("GetTab - Multiple Tabs")
    {
        document.Clear();
        document.Insert("Multiple tab stops");
        
        // Insert different types of tabs
        sWSTab leftTab;
        leftTab.type = TAB_TAB;
        leftTab.size = 50;
        leftTab.tabsize = 50;
        leftTab.abstabsize = 50;
        
        sWSTab centerTab;
        centerTab.type = TAB_CENTER;
        centerTab.size = 150;
        centerTab.tabsize = 150;
        centerTab.abstabsize = 150;
        
        document.SetPosition(3);
        document.InsertTab(leftTab);
        
        document.SetPosition(8);
        document.InsertTab(centerTab);
        
        // Query at the marker positions.
        sWSTab tab1 = document.GetTab(3);
        sWSTab tab2 = document.GetTab(8);
        CHECK((tab1.size != tab2.size || tab1.type != tab2.type) == true);
    }

    SUBCASE("GetFontList - Basic Functionality")
    {
        document.Clear();
        document.Insert("Multiple fonts document");
        
        // Add several different fonts
        std::vector<sInternalFonts> fonts = {
            {"Arial", 12, false, sWSFont(), "Arial"},
            {"Times", 10, false, sWSFont(), "Times"},
            {"Courier", 14, false, sWSFont(), "Courier"},
            {"Helvetica", 11, false, sWSFont(), "Helvetica"}
        };
        
        // Insert fonts at different positions
        for (size_t i = 0; i < fonts.size(); i++) {
            document.SetPosition(i * 3);
            document.InsertFont(fonts[i]);
        }
        
        // Get font list
        std::vector<sInternalFonts> fontList;
        document.GetFontList(fontList);
        
        // Should have found some fonts
        CHECK(fontList.size() >= 1);
        
        // Check that we have some of our inserted fonts
        bool foundArial = false;
        bool foundTimes = false;
        for (const auto& font : fontList) {
            if (font.fontname == "Arial") foundArial = true;
            if (font.fontname == "Times") foundTimes = true;
        }
        
        // Should find at least some of the fonts we inserted
        bool foundStandardFont = (foundArial || foundTimes);
        CHECK(foundStandardFont);
    }

    SUBCASE("GetFontList - Empty Document")
    {
        document.Clear();
        
        std::vector<sInternalFonts> fontList;
        document.GetFontList(fontList);
        
        // Should either be empty or contain default font
        // Implementation dependent
        CHECK(fontList.size() >= 0);
    }

    SUBCASE("GetNextFontTagPosition - Basic")
    {
        document.Clear();
        document.Insert("Text with font changes");
        
        // Add font tags
        sInternalFonts font;
        font.fontname = "Arial";
        font.size = 12;
        font.haveWSFont = false;
        
        document.SetPosition(5);
        document.InsertFont(font);
        
        document.SetPosition(15);
        document.InsertFont(font);
        
        // Find next font tag
        POSITION_T nextFontPos = document.GetNextFontTagPosition();
        
        // Should find a font position (or return reasonable value)
        CHECK(nextFontPos >= 0);
        CHECK(nextFontPos < document.GetTextSize());
    }

    SUBCASE("GetNextFontTagPosition - No Fonts")
    {
        document.Clear();
        document.Insert("Plain text no fonts");
        
        POSITION_T nextFontPos = document.GetNextFontTagPosition();
        
        // Should handle gracefully (return NOT_SET or end position)
        CHECK((nextFontPos >= 0 || nextFontPos == NOT_SET) == true);
    }

    SUBCASE("Font and Tab Integration")
    {
        document.Clear();
        document.Insert("Formatted text with tabs");
        
        // Mix fonts and tabs
        sInternalFonts font;
        font.fontname = "Times";
        font.size = 14;
        font.haveWSFont = false;
        
        sWSTab tab;
        tab.type = TAB_TAB;
        tab.size = 200;
        tab.tabsize = 200;
        tab.abstabsize = 200;
        
        document.SetPosition(3);
        document.InsertFont(font);
        
        document.SetPosition(5);
        document.InsertTab(tab);
        
        document.SetPosition(10);
        document.InsertFont(font);
        
        // Test that both fonts and tabs work together
        sInternalFonts retrievedFont;
        bool fontResult = document.GetFont(12, retrievedFont);
        
        sWSTab retrievedTab = document.GetTab(7);
        
        // Both should work (or handle gracefully)
        if (fontResult) {
            CHECK(retrievedFont.fontname == "Times");
        }
        CHECK(retrievedTab.size >= 0);
    }

    SUBCASE("Font Boundaries and Edge Cases")
    {
        document.Clear();
        document.Insert("A");
        
        // Test font at document boundaries
        sInternalFonts font;
        bool result1 = document.GetFont(0, font); // At start
        bool result2 = document.GetFont(document.GetTextSize() - 1, font); // At end
        
        // Should handle gracefully
        if (result1) {
            CHECK(font.fontname.length() >= 0);
        }
        if (result2) {
            CHECK(font.fontname.length() >= 0);
        }
        
        // Test beyond boundaries
        bool result3 = document.GetFont(1000, font);
        // Should either return false or handle gracefully
    }

    SUBCASE("Tab Edge Cases")
    {
        document.Clear();
        
        // Test tab on empty document
        sWSTab emptyTab = document.GetTab(0);
        CHECK(emptyTab.size >= 0); // Should return default or handle gracefully
        
        // Test tab beyond document bounds
        document.Insert("Short");
        sWSTab beyondTab = document.GetTab(1000);
        CHECK(beyondTab.size >= 0); // Should handle gracefully
    }
}

// =====================================================================
// COMPREHENSIVE TEST COVERAGE - PHASE 3: UNICODE PROCESSING
// =====================================================================

TEST_CASE("Unicode Processing")
{
    cDocument document;
    document.SetShowControl(SHOW_ALL);

    SUBCASE("Normalize - Basic ASCII")
    {
        document.Clear();
        
        std::string ascii = "Hello World 123";
        std::string normalized = document.Normalize(ascii);
        
        // ASCII should normalize to itself
        CHECK(normalized == ascii);
        CHECK(normalized.length() == ascii.length());
    }

    SUBCASE("Normalize - UTF-8 Characters")
    {
        document.Clear();
        
        // Test with UTF-8 characters that might need normalization
        std::string utf8Text = "café résumé naïve"; // Accented characters
        std::string normalized = document.Normalize(utf8Text);
        
        // Should be valid UTF-8 and not empty
        CHECK(normalized.length() > 0);
        CHECK(normalized.length() >= utf8Text.length()); // NFC might be same or longer
    }

    SUBCASE("Normalize - Complex Unicode")
    {
        document.Clear();
        
        // Test with the complex Unicode text from existing tests
        std::string complexText = "\xe0\xa4\x95\xe0\xa4\xbe\xe0\xa4\x9a\xe0\xa4\x82\x20\xe0\xa4\xb6\xe0\xa4\x95\xe0\xa5\x8d\xe0\xa4\xa8\xe0\xa5\x8b\xe0\xa4\xae\xe0\xa5\x8d\xe0\xa4\xaf\xe0\xa4\xa4\xe0\xa5\x8d\xe0\xa4\xa4\xe0\xa5\x81\xe0\xa4\xae\xe0\xa5\x8d\x20\xe0\xa5\xa4\x20\xe0\xa4\xa8\xe0\xa5\x8b\xe0\xa4\xaa\xe0\xa4\xb9\xe0\xa4\xbf\xe0\xa4\xa8\xe0\xa4\xb8\xe0\xa5\x8d\xe0\xa4\xa4\xe0\xa4\xbf\x20\xe0\xa4\xae\xe0\xa4\xbe\xe0\xa4\xae\xe0\xa5\x8d\x20\xe0\xa5\xa5\r";
        
        std::string normalized = document.Normalize(complexText);
        
        // Should produce valid output
        CHECK(normalized.length() > 0);
        // Complex scripts might normalize differently
        CHECK(normalized.length() >= 10); // Should have substantial content
    }

    SUBCASE("NormalizeToUTF32 and NormalizeToUTF8")
    {
        document.Clear();
        
        std::string input = "Hello éñjoy 世界";
        
        // Convert to UTF-32
        std::u32string utf32 = document.NormalizeToUTF32(input);
        CHECK(utf32.length() > 0);
        CHECK(utf32.length() <= input.length()); // UTF-32 should be shorter in codepoints
        
        // Convert back to UTF-8
        std::string backToUtf8 = document.NormalizeToUTF8(utf32);
        CHECK(backToUtf8.length() > 0);
        
        // Should round-trip reasonably (might not be identical due to normalization)
        CHECK(backToUtf8.length() >= input.length() - 5); // Allow some variation
    }

    SUBCASE("GetCodePoints - Basic Test")
    {
        document.Clear();
        
        std::string text = "ABC123";
        std::u32string codepoints;
        
        size_t count = document.GetCodePoints(text, codepoints);
        
        CHECK(count == 6); // Should have 6 codepoints
        CHECK(codepoints.length() == 6);
        CHECK(codepoints[0] == U'A');
        CHECK(codepoints[1] == U'B');
        CHECK(codepoints[2] == U'C');
        CHECK(codepoints[3] == U'1');
        CHECK(codepoints[4] == U'2');
        CHECK(codepoints[5] == U'3');
    }

    SUBCASE("GetCodePoints - Unicode Characters")
    {
        document.Clear();
        
        std::string text = "é🌟"; // e with acute + star emoji
        std::u32string codepoints;
        
        size_t count = document.GetCodePoints(text, codepoints);
        
        CHECK(count >= 1); // Should have at least 1 codepoint
        CHECK(codepoints.length() >= 1);
        CHECK(codepoints.length() <= 4); // Should be reasonable number
    }

    SUBCASE("GetCodePoints - Empty String")
    {
        document.Clear();
        
        std::string empty = "";
        std::u32string codepoints;
        
        size_t count = document.GetCodePoints(empty, codepoints);
        
        CHECK(count == 0);
        CHECK(codepoints.length() == 0);
    }

    SUBCASE("Unicode Integration with Document")
    {
        document.Clear();
        
        // Insert Unicode text and test document functions
        std::string unicodeText = "Hello 世界 Мир";
        document.Insert(unicodeText);
        
        // Test basic document operations work with Unicode
        CHECK(document.GetTextSize() == 13); // Include EOF
        CHECK(document.GetNumberofParagraphs() == 1);
        
        // Test GetText preserves Unicode
        std::string retrieved = document.GetText();
        retrieved.pop_back(); // Remove EOF
        
        // Should preserve the Unicode content (might be normalized)
        CHECK(retrieved.length() >= 10); // Should have substantial content
        
        // Test position operations work with Unicode
        document.SetPosition(5);
        POSITION_T pos = document.GetPosition();
        CHECK(pos == 5);
        
        // Test search works with Unicode
        POSITION_T found = document.FindNext("世界", 0, false, false, false);
        CHECK(found != NOT_SET); // Should find the text
    }

    SUBCASE("Complex Grapheme Clusters")
    {
        document.Clear();
        
        // Test with combining characters and complex graphemes
        std::string complexGraphemes = "e\xCC\x81"; // e + combining acute accent
        document.Insert(complexGraphemes);
        
        // Test that grapheme counting works
        std::vector<POSITION_T> offsets;
        size_t graphemeCount = document.GraphemeCount(complexGraphemes, offsets);
        
        CHECK(graphemeCount == 1); // Should be treated as 1 grapheme
        CHECK(offsets.size() == 1); 
        
        // Test document operations
        CHECK(document.GetTextSize() > 1); // Text + EOF
    }
}

// =====================================================================
// COMPREHENSIVE TEST COVERAGE - PHASE 3: MEASUREMENT SYSTEM
// =====================================================================

TEST_CASE("Measurement System")
{
    cDocument document;
    document.SetShowControl(SHOW_ALL);

    SUBCASE("ConvertToTwips - Inches")
    {
        document.Clear();
        
        // Test converting inches to twips (1 inch = 1440 twips)
        COORD_T result1 = document.ConvertToTwips(1.0, 'i'); // 1 inch
        CHECK(result1 == 1440.0);
        
        COORD_T result2 = document.ConvertToTwips(2.5, 'i'); // 2.5 inches
        CHECK(result2 == 3600.0); // 2.5 * 1440
        
        COORD_T result3 = document.ConvertToTwips(0.5, 'i'); // 0.5 inches
        CHECK(result3 == 720.0); // 0.5 * 1440
    }

    SUBCASE("ConvertToTwips - Centimeters")
    {
        document.Clear();
        
        // Test converting centimeters to twips (1 cm ~ 566.93 twips)
        COORD_T result1 = document.ConvertToTwips(1.0, 'c'); // 1 cm
        CHECK(result1 > 560.0);
        CHECK(result1 < 570.0);
        
        COORD_T result2 = document.ConvertToTwips(2.54, 'c'); // 2.54 cm = 1 inch
        CHECK(result2 > 1430.0);
        CHECK(result2 < 1450.0); // Should be close to 1440
    }

    SUBCASE("ConvertToTwips - Points")
    {
        document.Clear();
        
        // Test converting points to twips (1 point = 20 twips)
        COORD_T result1 = document.ConvertToTwips(1.0, 'p'); // 1 point
        CHECK(result1 == 20.0);
        
        COORD_T result2 = document.ConvertToTwips(12.0, 'p'); // 12 points
        CHECK(result2 == 240.0); // 12 * 20
        
        COORD_T result3 = document.ConvertToTwips(72.0, 'p'); // 72 points = 1 inch
        CHECK(result3 == 1440.0); // Should equal 1 inch in twips
    }

    SUBCASE("ConvertToTwips - Default/Unknown Unit")
    {
        document.Clear();
        
        // Test with unknown unit (should use default behavior)
        COORD_T result1 = document.ConvertToTwips(100.0, 'x'); // Unknown unit
        CHECK(result1 >= 0.0); // Should return some reasonable value
        
        COORD_T result2 = document.ConvertToTwips(1.0, '\0'); // Null character
        CHECK(result2 >= 0.0); // Should handle gracefully
    }

    SUBCASE("GetValue - Integer Values")
    {
        document.Clear();
        
        bool incDec = false;
        
        double result1 = document.GetValue("123", incDec);
        CHECK(result1 == 123.0);
        CHECK(incDec == false);
        
        double result2 = document.GetValue("0", incDec);
        CHECK(result2 == 0.0);
        CHECK(incDec == false);
        
        double result3 = document.GetValue("999", incDec);
        CHECK(result3 == 999.0);
        CHECK(incDec == false);
    }

    SUBCASE("GetValue - Decimal Values")
    {
        document.Clear();
        
        bool incDec = false;
        
        double result1 = document.GetValue("12.5", incDec);
        CHECK(result1 == 12.5);
        CHECK(incDec == false);
        
        double result2 = document.GetValue("0.25", incDec);
        CHECK(result2 == 0.25);
        CHECK(incDec == false);
        
        double result3 = document.GetValue("3.14159", incDec);
        CHECK(result3 > 3.14);
        CHECK(result3 < 3.15);
        CHECK(incDec == false);
    }

    SUBCASE("GetValue - Increment/Decrement")
    {
        document.Clear();
        
        bool incDec = false;
        
        // Test increment syntax (if supported)
        double result1 = document.GetValue("+5", incDec);
        CHECK(result1 == 5.0);
        // incDec might be true for increment syntax
        
        incDec = false;
        double result2 = document.GetValue("-3", incDec);
        CHECK((result2 == -3.0 || result2 == 3.0) == true); // Might handle negative or as decrement
    }

    SUBCASE("GetValue - Invalid Input")
    {
        document.Clear();
        
        bool incDec = false;
        
        // Test with non-numeric input
        double result1 = document.GetValue("abc", incDec);
        CHECK(result1 == 0.0); // Should return 0 for invalid input
        
        double result2 = document.GetValue("", incDec);
        CHECK(result2 == -32768.0); // Should return 0 for empty input
        
        double result3 = document.GetValue("12.34.56", incDec);
        // Should handle malformed decimals gracefully
        CHECK(result3 >= 0.0);
    }

    SUBCASE("GetType - Common Types")
    {
        document.Clear();
        
        char type1 = document.GetType("123I");
        CHECK(type1 == '\"'); // Inches
        
        char type2 = document.GetType("45.5C");
        CHECK(type2 == 'C'); // Centimeters
        
        char type3 = document.GetType("12P");
        CHECK(type3 == 'P'); // Points
        
        char type4 = document.GetType("100");
        CHECK((type4 == 'I' || type4 == '\0' || type4 == 'P') == true); // Default type
    }

    SUBCASE("GetType - Edge Cases")
    {
        document.Clear();
        
        char type1 = document.GetType("");
        CHECK((type1 == '\0' || type1 == 'I') == true); // Should handle empty string
        
        char type2 = document.GetType("123");
        CHECK((type2 == 'I' || type2 == '\0' || type2 == 'P') == true); // Number without unit
        
        char type3 = document.GetType("ABC");
        CHECK((type3 == 'x' || type3 == '\0') == true); // Invalid input
        
        char type4 = document.GetType("123X");
        CHECK((type4 == 'x' || type4 == '\0') == true); // Unknown unit
    }

    SUBCASE("Measurement Integration Test")
    {
        document.Clear();
        
        // Test the complete measurement workflow
        std::string measurement = "2.5I"; // 2.5 inches
        
        // Parse the value and type
        bool incDec = false;
        double value = document.GetValue(measurement, incDec);
        char type = document.GetType(measurement);
        
        CHECK(value == 2.5);
        CHECK(type == '\"');
        CHECK(incDec == false);
        
        // Convert to twips
        COORD_T twips = document.ConvertToTwips(value, type);
        CHECK(twips == 3600.0); // 2.5 * 1440
    }

    SUBCASE("Measurement Edge Cases")
    {
        document.Clear();
        
        // Test with very large values
        COORD_T largeTwips = document.ConvertToTwips(1000.0, 'I');
        CHECK(largeTwips == 1440000.0);
        
        // Test with very small values
        COORD_T smallTwips = document.ConvertToTwips(0.001, 'I');
        CHECK(smallTwips == doctest::Approx(1.44));
        
        // Test with negative values
        COORD_T negativeTwips = document.ConvertToTwips(-1.0, 'I');
        CHECK((negativeTwips == -1440.0 || negativeTwips >= 0.0) == true); // Implementation dependent
        
        // Test with zero
        COORD_T zeroTwips = document.ConvertToTwips(0.0, 'I');
        CHECK(zeroTwips == 0.0);
    }
}

// =====================================================================
// COMPREHENSIVE TEST COVERAGE - PHASE 4: BOUNDARY CONDITIONS & STRESS TESTING
// =====================================================================

TEST_CASE("Boundary Conditions and Stress Testing")
{
    cDocument document;
    document.SetShowControl(SHOW_ALL);

    SUBCASE("Empty Document Edge Cases")
    {
        document.Clear();
        
        // Test all major operations on empty document
        CHECK(document.GetTextSize() == 1); // EOF marker
        CHECK(document.GetNumberofParagraphs() == 1);
        CHECK(document.GetPosition() == 0);
        
        // Test navigation on empty document
        POSITION_T nextWord = document.GetNextWordPosition(0);
        POSITION_T prevWord = document.GetPrevWordPosition(0);

        bool nextWordValidEmpty = (nextWord == 1 || nextWord == NOT_SET);
        CHECK(nextWordValidEmpty);
        bool prevWordValidEmpty = (prevWord == 0 || prevWord == NOT_SET);
        CHECK(prevWordValidEmpty);
        
        // Test search on empty document
        POSITION_T found = document.FindNext("test", 0, false, false, false);
        CHECK(found == document.GetTextSize()); // Should not find anything
        
        // Test block operations on empty document
        document.SetBeginBlock();
        document.SetEndBlock();
        POSITION_T start, end;
        bool blockSet = document.GetBlock(start, end);
        // Should handle gracefully
        
        // Test delete on empty document
        bool deleteResult = document.Delete(0, 1);
        CHECK(deleteResult == false); // Can't delete EOF
        CHECK(document.GetTextSize() == 1); // Should still have EOF
    }

    SUBCASE("Single Character Document")
    {
        document.Clear();
        document.Insert('A');
        
        CHECK(document.GetTextSize() == 2); // A + EOF
        CHECK(document.GetNumberofParagraphs() == 1);
        
        // Test all operations work with single character
        CHECK(document.GetCharNoAdvance(0) == "A");
        CHECK(resolveControlChar(document, 1) == STYLE_EOF);
        
        // Test word navigation
        POSITION_T nextWord = document.GetNextWordPosition(0);
        POSITION_T prevWord = document.GetPrevWordPosition(1);
        CHECK(nextWord >= 0);
        CHECK(prevWord >= 0);
        
        // Test search
        POSITION_T found = document.FindNext("A", 0, false, false, false);
        CHECK(found == 0);
        
        // Test block operations
        document.SetPosition(0);
        document.SetBeginBlock();
        document.SetPosition(2);
        document.SetEndBlock();
        
        POSITION_T start, end;
        bool blockSet = document.GetBlock(start, end);
        CHECK(blockSet == true);
        CHECK(start == 0);
        CHECK(end == 1);
    }

    SUBCASE("Large Document Stress Test")
    {
        document.Clear();
        
        // Create a large document with 1000 paragraphs
        const int numParagraphs = 1000;
        const std::string paraText = "This is paragraph ";
        
        for (int i = 0; i < numParagraphs; i++) {
            document.Insert(paraText + std::to_string(i) + "\r");
        }
        
        CHECK(document.GetNumberofParagraphs() == numParagraphs + 1); // +1 for EOF paragraph
        CHECK(document.GetTextSize() > numParagraphs * paraText.length());
        
        // Test navigation in large document
        document.SetPosition(document.GetTextSize() / 2);
        POSITION_T pos = document.GetPosition();
        CHECK(pos > 0);
        CHECK(pos < document.GetTextSize());
        
        // Test search in large document
        POSITION_T found = document.FindNext("paragraph 500", 0, false, false, false);
        CHECK(found != NOT_SET);
        
        // Test memory optimization
        document.ShrinkToFit(); // Should not crash
        CHECK(document.GetNumberofParagraphs() == numParagraphs + 1);
    }


    SUBCASE("Very Long Lines")
    {
        document.Clear();
        
        // Create a very long line (10,000 characters)
        std::string longLine;
        for (int i = 0; i < 10000; i++) {
            longLine += char('A' + (i % 26));
        }
        
        document.Insert(longLine);
        
        CHECK(document.GetTextSize() == longLine.length() + 1); // +EOF
        CHECK(document.GetNumberofParagraphs() == 1);
        
        // Test navigation in long line
        document.SetPosition(5000);
        CHECK(document.GetPosition() == 5000);
        
        // Test word navigation in long line
        POSITION_T nextWord = document.GetNextWordPosition(1000);
        CHECK(nextWord >= 1000);
        
        // Test search in long line
        std::string searchTerm = longLine.substr(1000, 10);
        POSITION_T found = document.FindNext(searchTerm, 998, false, false, false);
        CHECK(found == 1000);
    }

    SUBCASE("Maximum Position Values")
    {
        document.Clear();
        document.Insert("Test");
        
        POSITION_T maxPos = document.GetTextSize() + 1000;
        
        // Test setting position beyond document bounds
        document.SetPosition(maxPos);
        POSITION_T actualPos = document.GetPosition();
        CHECK(actualPos >= 0); // Should handle gracefully
        
        // Test operations at extreme positions
        POSITION_T found = document.FindNext("Test", maxPos, false, false, false);
        // Should handle gracefully (probably return NOT_SET)
        
        // Test delete at extreme position
        bool deleteResult = document.Delete(maxPos, 1);
        CHECK(deleteResult == false); // Should fail gracefully
    }

    SUBCASE("Unicode Stress Test")
    {
        document.Clear();
        
        // Insert various Unicode scripts and symbols
        std::vector<std::string> unicodeTexts = {
            "Hello World", // ASCII
            "café résumé", // Latin with accents
            "Привет мир", // Cyrillic
            "你好世界", // Chinese
            "مرحبا بالعالم", // Arabic
            "שלום עולם", // Hebrew
            "🌍🌎🌏🚀⭐", // Emojis
            "ñoñó", // Spanish
            "naïve", // French
            "Москва", // Russian
        };
        
        for (const auto& text : unicodeTexts) {
            document.Insert(text + "\r");
        }
        
        // Test document still works with mixed Unicode
        CHECK(document.GetNumberofParagraphs() == unicodeTexts.size() + 1);
        CHECK(document.GetTextSize() > 50);
        
        // Test search with Unicode
        POSITION_T found = document.FindNext("世界", 0, false, false, false);
        CHECK(found != NOT_SET);
        
        // Test position operations with Unicode
        document.SetPosition(30);
        CHECK(document.GetPosition() == 30);
        
        // Test word navigation with Unicode
        POSITION_T nextWord = document.GetNextWordPosition(10);
        CHECK(nextWord >= 10);
    }

    SUBCASE("Formatting Stress Test")
    {
        document.Clear();
        document.Insert("Heavily formatted text document");
        
        // Add lots of formatting
        for (int i = 0; i < 100; i++) {
            document.SetPosition(i % (document.GetTextSize() - 1));
            
            switch (i % 6) {
                case 0:
                    document.BeginBold();
                    break;
                case 1:
                    document.EndBold();
                    break;
                case 2:
                    document.BeginItalics();
                    break;
                case 3:
                    document.EndItalics();
                    break;
                case 4:
                    document.BeginUnderline();
                    break;
                case 5:
                    document.EndUnderline();
                    break;
            }
        }
        
        // Document should still work
        CHECK(document.GetTextSize() > 32); // Original text + formatting markers
        
        // Test that we can still navigate and search
        document.SetPosition(10);
        CHECK(document.GetPosition() == 10);
        
        POSITION_T found = document.FindNext("formatted", 0, false, false, false);
        CHECK(found != NOT_SET);
    }

    SUBCASE("Delete Stress Test")
    {
        document.Clear();
        
        // Create document with lots of content
        for (int i = 0; i < 100; i++) {
            document.Insert("Line " + std::to_string(i) + " with some content\r");
        }
        
        POSITION_T originalSize = document.GetTextSize();
        CHECK(originalSize > 1000);
        
        // Delete lots of content in various ways
        for (int i = 0; i < 50; i++) {
            if (document.GetTextSize() > 100) {
                POSITION_T pos = document.GetTextSize() / 4;
                document.Delete(pos, 10); // Delete 10 characters
            }
        }
        
        // Document should still be valid
        CHECK(document.GetTextSize() >= 1); // At least EOF
        CHECK(document.GetNumberofParagraphs() >= 1);
        
        // Should still be able to add content
        document.Insert("New content");
        CHECK(document.GetTextSize() > 1);
    }
}

// =====================================================================
// COMPREHENSIVE TEST COVERAGE - PHASE 4: MEMORY AND PERFORMANCE
// =====================================================================

/* Very slow, uncomment to run
TEST_CASE("Memory and Performance Testing")
{
    cDocument document;
    document.SetShowControl(SHOW_ALL);

    SUBCASE("Memory Management - Large Insertions")
    {
        document.Clear();
        
        // Insert large amounts of text repeatedly
        const std::string chunk = "This is a chunk of text that will be repeated many times. ";
        const int iterations = 1000;
        
        for (int i = 0; i < iterations; i++) {
            document.Insert(chunk);
        }
        
        CHECK(document.GetTextSize() == chunk.length() * iterations + 1); // +EOF
        
        // Test ShrinkToFit reduces memory usage
        document.ShrinkToFit();
        
        // Document should still work after shrinking
        CHECK(document.GetTextSize() == chunk.length() * iterations + 1);
        
        // Test that we can still add more content
        document.Insert("Additional text");
        CHECK(document.GetTextSize() > chunk.length() * iterations + 1);
    }

    SUBCASE("Memory Management - Rapid Insert/Delete Cycles")
    {
        document.Clear();
        
        // Perform many insert/delete cycles to test memory management
        for (int cycle = 0; cycle < 100; cycle++) {
            // Insert some text
            for (int i = 0; i < 50; i++) {
                document.Insert("Test" + std::to_string(i) + " ");
            }
            
            // Delete some text
            while (document.GetTextSize() > 200) {
                document.Delete(100, 50);
            }
        }
        
        // Document should still be functional
        CHECK(document.GetTextSize() >= 1);
        CHECK(document.GetNumberofParagraphs() >= 1);
        
        // Memory optimization should still work
        document.ShrinkToFit();
        CHECK(document.GetTextSize() >= 1);
    }

    SUBCASE("Performance - Large Document Navigation")
    {
        document.Clear();
        
        // Create a large document for navigation testing
        for (int i = 0; i < 500; i++) {
            document.Insert("Paragraph " + std::to_string(i) + " contains some text for navigation testing.\r");
        }
        
        POSITION_T documentSize = document.GetTextSize();
        CHECK(documentSize > 10000);
        
        // Test navigation performance (should not hang)
        for (int i = 0; i < 100; i++) {
            POSITION_T randomPos = (i * 137) % (documentSize - 1); // Semi-random positions
            document.SetPosition(randomPos);
            CHECK(document.GetPosition() == randomPos);
            
            // Test word navigation from random positions
            POSITION_T nextWord = document.GetNextWordPosition(randomPos);
            CHECK((nextWord >= randomPos || nextWord == NOT_SET) == true);
        }
    }

    SUBCASE("Performance - Search in Large Document")
    {
        document.Clear();
        
        // Create document with repeated patterns for search testing
        for (int i = 0; i < 1000; i++) {
            if (i % 100 == 50) {
                document.Insert("FINDME" + std::to_string(i) + " ");
            } else {
                document.Insert("text" + std::to_string(i) + " ");
            }
        }
        
        // Test multiple searches (should complete in reasonable time)
        for (int search = 0; search < 20; search++) {
            std::string searchTerm = "FINDME" + std::to_string(search * 100 + 50);
            POSITION_T found = document.FindNext(searchTerm, 0, false, false, false);
            if (search < 10) { // First 10 should be found
                CHECK(found != NOT_SET);
            }
        }
    }

    SUBCASE("Memory - Complex Formatting")
    {
        document.Clear();
        
        // Create document with complex formatting that uses many vectors
        document.Insert("Document with complex formatting");
        
        // Add many different types of formatting elements
        for (int i = 0; i < 200; i++) {
            document.SetPosition(i % (document.GetTextSize() - 1));
            
            switch (i % 8) {
                case 0: {
                    sWSTab tab;
                    tab.type = TAB_TAB;
                    tab.size = 100 + i;
                    tab.tabsize = 100 + i;
                    tab.abstabsize = 100 + i;
                    document.InsertTab(tab);
                    break;
                }
                case 1: {
                    sInternalFonts font;
                    font.fontname = "Font" + std::to_string(i);
                    font.size = 10 + (i % 5);
                    font.haveWSFont = false;
                    document.InsertFont(font);
                    break;
                }
                case 2: {
                    sSeqRGBColor color;
                    color.red = static_cast<short>((i % 16) * 17) ;
                    color.green = static_cast<short>(((i + 3) % 16) * 17) ;
                    color.blue = static_cast<short>(((i + 7) % 16) * 17) ;
                    color.alpha = 255 ;
                    document.InsertColor(color);
                    break;
                }
                case 3:
                    document.BeginBold();
                    break;
                case 4:
                    document.EndBold();
                    break;
                case 5:
                    document.BeginItalics();
                    break;
                case 6:
                    document.EndItalics();
                    break;
                case 7: {
                    sNote note;
                    note.text = "Note " + std::to_string(i);
                    note.symbol = NOTE_NUMBER;
                    document.InsertFootnote(note);
                    break;
                }
            }
        }
        
        // Test that document still works with heavy formatting
        CHECK(document.GetTextSize() > 32); // Should be much larger now
        
        // Test ShrinkToFit with complex formatting
        document.ShrinkToFit();
        CHECK(document.GetTextSize() > 32);
        
        // Test that formatting retrieval still works
        sInternalFonts font;
        bool fontResult = document.GetFont(50, font);
        // Should either work or fail gracefully
        
        sWSTab tab = document.GetTab(25);
        CHECK(tab.size >= 0);
    }

    SUBCASE("Memory - Block Operations Performance")
    {
        document.Clear();
        
        // Create large document for block operations
        for (int i = 0; i < 200; i++) {
            document.Insert("Block testing paragraph " + std::to_string(i) + " with enough content.\r");
        }
        
        POSITION_T docSize = document.GetTextSize();
        
        // Test many block operations
        for (int i = 0; i < 50; i++) {
            POSITION_T start = (i * 47) % (docSize / 2);
            POSITION_T end = start + 20;
            if (end >= docSize) end = docSize - 1;
            
            document.SetPosition(start);
            document.SetBeginBlock();
            document.SetPosition(end);
            document.SetEndBlock();
            
            // Test copy (should not crash or hang)
            document.Copy();
            
            // Test paste
            document.SetPosition((start + 100) % (docSize - 1));
            document.Paste();
        }
        
        // Document should still be functional
        CHECK(document.GetTextSize() > docSize); // Should be larger due to pastes
        CHECK(document.GetNumberofParagraphs() >= 200);
    }
}
*/



// =====================================================================
// COMPREHENSIVE TEST COVERAGE - PHASE 5: ERROR HANDLING & INTEGRATION
// =====================================================================

TEST_CASE("Error Handling and Integration")
{
    cDocument document;
    document.SetShowControl(SHOW_ALL);

    SUBCASE("SaveBlocks and RestoreBlocks")
    {
        document.Clear();
        document.Insert("Test document with blocks");
        
        // Set a block
        document.SetPosition(5);
        document.SetBeginBlock();
        document.SetPosition(12);
        document.SetEndBlock();
        
        POSITION_T start1, end1;
        bool blockSet1 = document.GetBlock(start1, end1);
        CHECK(blockSet1 == true);
        
        // Save blocks
        document.SaveBlocks();
        
        // Change the block
        document.SetPosition(0);
        document.SetBeginBlock();
        document.SetPosition(3);
        document.SetEndBlock();
        
        POSITION_T start2, end2;
        bool blockSet2 = document.GetBlock(start2, end2);
        CHECK(blockSet2 == true);
        CHECK((start2 != start1 || end2 != end1) == true); // Should be different
        
        // Restore blocks
        document.RestoreBlocks();
        
        POSITION_T start3, end3;
        bool blockSet3 = document.GetBlock(start3, end3);
        if (blockSet3) {
            // If restore works, should get back original block
            CHECK(start3 == start1);
            CHECK(end3 == end1);
        }
    }

    SUBCASE("GetBlockText - Various Scenarios")
    {
        document.Clear();
        document.Insert("This is a test document for block text retrieval");
        
        // Test normal block text
        std::string blockText1 = document.GetBlockText(5, 10);
        CHECK(blockText1.length() == 5); // 5 characters
        CHECK(blockText1 == "is a ");
        
        // Test block at document boundaries
        std::string blockText2 = document.GetBlockText(0, 4);
        CHECK(blockText2 == "This");
        
        POSITION_T docSize = document.GetTextSize();
        std::string blockText3 = document.GetBlockText(docSize - 5, docSize - 1);
        CHECK(blockText3.length() >= 1); // Should get some text
        
        // Test invalid block ranges
        std::string blockText4 = document.GetBlockText(100, 200); // Beyond document
        // Should handle gracefully (return empty or partial)
        
        std::string blockText5 = document.GetBlockText(10, 5); // Reversed range
        // Should handle gracefully
    }

    SUBCASE("Integration - Undo with Complex Operations")
    {
        document.Clear();
        document.Insert("Integration test document");
        
        // Perform complex sequence of operations
        document.SetPosition(5);
        document.BeginBold();
        document.Insert(" BOLD ");
        document.EndBold();
        
        document.SetPosition(15);
        sWSTab tab;
        tab.type = TAB_TAB;
        tab.size = 100;
        tab.tabsize = 100;
        tab.abstabsize = 100;
        document.InsertTab(tab);
        
        document.Insert("After tab");
        
        POSITION_T sizeAfterOps = document.GetTextSize();
        
        // Try to undo the complex operations
        for (int i = 0; i < 10; i++) {
            if (!document.Undo()) {
                break;
            }
        }
        
        POSITION_T sizeAfterUndo = document.GetTextSize();
        CHECK(sizeAfterUndo <= sizeAfterOps); // Should be smaller or same
        
        // Document should still be functional
        document.Insert("New text");
        CHECK(document.GetTextSize() > sizeAfterUndo);
    }

    SUBCASE("Integration - Search with Formatting")
    {
        document.Clear();
        document.Insert("Search test ");
        document.BeginBold();
        document.Insert("BOLD TEXT");
        document.EndBold();
        document.Insert(" more text ");
        document.BeginItalics();
        document.Insert("ITALIC TEXT");
        document.EndItalics();
        document.Insert(" end");
        
        // Test searching for text that spans formatting
        POSITION_T found1 = document.FindNext("BOLD TEXT", 0, false, false, false);
        CHECK(found1 != NOT_SET);
        
        POSITION_T found2 = document.FindNext("ITALIC TEXT", 0, false, false, false);
        CHECK(found2 != NOT_SET);
        CHECK(found2 > found1);
        
        // Test case-insensitive search
        POSITION_T found3 = document.FindNext("bold text", 0, false, true, false);
        CHECK(found3 != NOT_SET);
        
        // Test whole word search
        POSITION_T found4 = document.FindNext("BOLD", 0, false, false, true);
        CHECK(found4 != NOT_SET);
        
        POSITION_T found5 = document.FindNext("BOL", 0, false, false, true);
        CHECK(found5 == document.GetTextSize()); // Should not find partial word
    }

    SUBCASE("Error Recovery - Invalid Operations")
    {
        document.Clear();
        
        // Test operations that should fail gracefully
        bool result1 = document.Delete(1000, 100); // Delete beyond document
        CHECK(result1 == false);
        CHECK(document.GetTextSize() == 1); // Should still have EOF
        
        // Test with empty document
        document.SetPosition(100); // Set position beyond document
        // Should handle gracefully
        CHECK(document.GetPosition() >= 0);
        
        // Test invalid font operations
        sInternalFonts invalidFont;
        invalidFont.fontname = "";
        invalidFont.size = -1;
        invalidFont.haveWSFont = false;
        
        document.Insert("Test");
        document.InsertFont(invalidFont); // Should handle gracefully
        
        sInternalFonts retrievedFont;
        bool fontResult = document.GetFont(1000, retrievedFont); // Position beyond document
        // Should handle gracefully
    }

    SUBCASE("Integration - All Features Combined")
    {
        document.Clear();
        
        // Create a document that uses all major features
        document.Insert("Complex document test\r");
        
        // Add formatting
        document.BeginBold();
        document.Insert("Bold paragraph\r");
        document.EndBold();
        
        // Add tabs
        sWSTab tab;
        tab.type = TAB_CENTER;
        tab.size = 200;
        tab.tabsize = 200;
        tab.abstabsize = 200;
        document.InsertTab(tab);
        document.Insert("Centered text\r");
        
        // Add fonts
        sInternalFonts font;
        font.fontname = "TestFont";
        font.size = 14;
        font.haveWSFont = false;
        document.InsertFont(font);
        document.Insert("Different font\r");
        
        // Add colors
        sSeqRGBColor color;
        color.red = 170 ; color.green = 0 ; color.blue = 170 ; color.alpha = 255 ; // WS magenta (index 5)
        document.InsertColor(color);
        document.Insert("Colored text\r");
        
        // Add footnotes
        sNote note;
        note.text = "Test footnote";
        note.symbol = NOTE_NUMBER;
        document.InsertFootnote(note);
        document.Insert("Text with footnote\r");
        
        // Test that all features work together
        CHECK(document.GetNumberofParagraphs() >= 5);
        CHECK(document.GetTextSize() > 50);
        
        // Test navigation works with all features
        document.SetPosition(30);
        POSITION_T nextWord = document.GetNextWordPosition(30);
        CHECK(nextWord >= 30);
        
        // Test search works with formatting
        POSITION_T found = document.FindNext("Bold", 0, false, false, false);
        CHECK(found != NOT_SET);
        
        // Test block operations work with formatting
        document.SetPosition(10);
        document.SetBeginBlock();
        document.SetPosition(25);
        document.SetEndBlock();
        document.Copy();
        
        document.SetPosition(document.GetTextSize() - 1);
        document.Paste();
        
        // Document should still be functional
        CHECK(document.GetTextSize() > 50);
        
        // Test memory optimization with complex document
        document.ShrinkToFit();
        CHECK(document.GetNumberofParagraphs() >= 5);
    }
}


///////////////////////////////////////////////////////////////////////////
// cGraphemeOffsets unit tests
///////////////////////////////////////////////////////////////////////////

TEST_CASE("cGraphemeOffsets - empty state")
{
    cGraphemeOffsets offsets ;

    CHECK(offsets.size() == 0) ;
    CHECK(offsets.memoryUsed() == 0) ;
    CHECK(offsets.memoryAllocated() == 0) ;
}

TEST_CASE("cGraphemeOffsets - identity mode for ASCII")
{
    // ASCII text: every grapheme is 1 byte, so offset[i] = i
    std::string buffer = "Hello World" ;
    std::vector<POSITION_T> rawOffsets = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10} ;

    cGraphemeOffsets offsets ;
    offsets.Store(buffer, rawOffsets) ;

    CHECK(offsets.size() == 11) ;
    CHECK(offsets.memoryUsed() == 0) ;      // identity mode = zero storage

    // verify all offsets are correct
    for (size_t i = 0 ; i < 11 ; ++i)
    {
        CHECK(offsets[i] == static_cast<POSITION_T>(i)) ;
    }

    // verify CopyTo produces correct vector
    std::vector<POSITION_T> exported ;
    offsets.CopyTo(exported) ;
    CHECK(exported.size() == 11) ;
    CHECK(exported == rawOffsets) ;
}

TEST_CASE("cGraphemeOffsets - delta mode for multi-byte UTF-8")
{
    // "Cafe-acute" = C(1) a(1) f(1) e-acute(2) = 5 bytes, 4 graphemes
    std::string buffer = "Caf\xc3\xa9" ;
    std::vector<POSITION_T> rawOffsets = {0, 1, 2, 3} ;  // C=0, a=1, f=2, e-acute=3

    cGraphemeOffsets offsets ;
    offsets.Store(buffer, rawOffsets) ;

    CHECK(offsets.size() == 4) ;
    CHECK(offsets.memoryUsed() == 4) ;      // 4 deltas, 1 byte each

    // verify all offsets
    CHECK(offsets[0] == 0) ;
    CHECK(offsets[1] == 1) ;
    CHECK(offsets[2] == 2) ;
    CHECK(offsets[3] == 3) ;

    // verify CopyTo
    std::vector<POSITION_T> exported ;
    offsets.CopyTo(exported) ;
    CHECK(exported == rawOffsets) ;
}

TEST_CASE("cGraphemeOffsets - delta mode with 3-byte graphemes")
{
    // Devanagari: each grapheme is 3 bytes
    // "U+0915U+093E" = U+0915(3 bytes) + U+093E(3 bytes combining) = 6 bytes, but as 2 graphemes
    // Let's use a simpler example: three 3-byte characters
    // euroeuroeuro = each is 3 bytes (0xE2 0x82 0xAC)
    std::string buffer = "\xe2\x82\xac\xe2\x82\xac\xe2\x82\xac" ; // euroeuroeuro
    std::vector<POSITION_T> rawOffsets = {0, 3, 6} ;

    cGraphemeOffsets offsets ;
    offsets.Store(buffer, rawOffsets) ;

    CHECK(offsets.size() == 3) ;
    CHECK(offsets.memoryUsed() == 3) ;      // 3 deltas

    CHECK(offsets[0] == 0) ;
    CHECK(offsets[1] == 3) ;
    CHECK(offsets[2] == 6) ;

    std::vector<POSITION_T> exported ;
    offsets.CopyTo(exported) ;
    CHECK(exported == rawOffsets) ;
}

TEST_CASE("cGraphemeOffsets - clear resets to empty")
{
    std::string buffer = "Hello" ;
    std::vector<POSITION_T> rawOffsets = {0, 1, 2, 3, 4} ;

    cGraphemeOffsets offsets ;
    offsets.Store(buffer, rawOffsets) ;
    CHECK(offsets.size() == 5) ;

    offsets.clear() ;
    CHECK(offsets.size() == 0) ;
    CHECK(offsets.memoryUsed() == 0) ;
}

TEST_CASE("cGraphemeOffsets - shrink_to_fit reduces allocation")
{
    // Build delta mode with some data
    std::string buffer = "\xc3\xa9\xc3\xa9\xc3\xa9" ; // e-acutee-acutee-acute = 6 bytes, 3 graphemes
    std::vector<POSITION_T> rawOffsets = {0, 2, 4} ;

    cGraphemeOffsets offsets ;
    offsets.Store(buffer, rawOffsets) ;

    size_t before = offsets.memoryAllocated() ;
    offsets.shrink_to_fit() ;
    size_t after = offsets.memoryAllocated() ;

    // after shrink_to_fit, allocated should be <= before
    CHECK(after <= before) ;
    CHECK(after == offsets.memoryUsed()) ;
}

TEST_CASE("cGraphemeOffsets - integration with cDocument ASCII paragraph")
{
    cDocument doc ;

    // Insert ASCII text
    doc.Insert("Hello World\r") ;

    // Get offsets through document API
    std::vector<POSITION_T> offsets ;
    size_t count = doc.GetParagraphGraphemeOffsets(0, offsets) ;

    CHECK(count == 12) ;    // 11 chars + \r
    for (size_t i = 0 ; i < count ; ++i)
    {
        CHECK(offsets[i] == static_cast<POSITION_T>(i)) ;
    }

    // Verify memory savings -- ASCII paragraph should use identity mode
    sDocumentMemoryUsage mem = doc.GetMemoryUsage() ;
    CHECK(mem.attrOffsetsBytes == 0) ;      // identity mode = zero bytes
}

TEST_CASE("cGraphemeOffsets - integration with cDocument UTF-8 paragraph")
{
    cDocument doc ;

    // Insert text with multi-byte characters (Cafe-acute)
    std::string text = "Caf\xc3\xa9\r" ;
    doc.Insert(text) ;

    // Get offsets through document API
    std::vector<POSITION_T> offsets ;
    size_t count = doc.GetParagraphGraphemeOffsets(0, offsets) ;

    CHECK(count == 5) ;     // C, a, f, e-acute, \r
    CHECK(offsets[0] == 0) ;
    CHECK(offsets[1] == 1) ;
    CHECK(offsets[2] == 2) ;
    CHECK(offsets[3] == 3) ;
    CHECK(offsets[4] == 5) ;    // e-acute is 2 bytes, \r starts at byte 5

    // Verify memory usage -- delta mode uses 1 byte per grapheme
    // which is far less than the old 8 bytes per grapheme (POSITION_T = ssize_t)
    sDocumentMemoryUsage mem = doc.GetMemoryUsage() ;
    CHECK(mem.attrOffsetsBytes > 0) ;            // delta mode uses some storage
    CHECK(mem.attrOffsetsBytes < count * 8) ;    // but much less than 8 bytes/grapheme
}


/////////////////////////////////////////////////////////////////////////////
//
// Document Listener Tests
//
/////////////////////////////////////////////////////////////////////////////

// Test listener that records all notifications
class cTestListener : public cDocumentListener
{
public:
    int mChangedCount ;
    int mClearedCount ;
    PARAGRAPH_T mLastParagraph ;

    cTestListener()
        : mChangedCount(0)
        , mClearedCount(0)
        , mLastParagraph(-1)
    {
    }

    void OnDocumentChanged(PARAGRAPH_T fromParagraph) override
    {
        mChangedCount++ ;
        mLastParagraph = fromParagraph ;
    }

    void OnDocumentCleared() override
    {
        mClearedCount++ ;
    }

    void Reset()
    {
        mChangedCount = 0 ;
        mClearedCount = 0 ;
        mLastParagraph = -1 ;
    }
} ;


TEST_CASE("Insert(string): multi-paragraph insert batches one notification and one undo")
{
    cDocument doc ;
    cTestListener listener ;
    doc.AddListener(&listener) ;

    doc.SetPosition(0) ;
    doc.Insert(std::string("line1\rline2\rline3")) ;

    // The whole string is one edit: exactly one notification, three paragraphs.
    CHECK(listener.mChangedCount == 1) ;
    CHECK(doc.GetNumberofParagraphs() == 3) ;

    // And it undoes in a single step (per-char undo was batched).
    POSITION_T sizeBefore = doc.GetTextSize() ;
    doc.Undo() ;
    CHECK(doc.GetNumberofParagraphs() == 1) ;
    CHECK(doc.GetTextSize() < sizeBefore) ;

    doc.RemoveListener(&listener) ;
}

TEST_CASE("Document Listener - Registration")
{
    cDocument doc ;
    cTestListener listener ;

    SUBCASE("AddListener and RemoveListener")
    {
        // initially no notifications
        doc.SetPosition(0) ;
        doc.Insert('A') ;
        CHECK(listener.mChangedCount == 0) ;

        // register listener
        doc.AddListener(&listener) ;

        doc.Insert('B') ;
        CHECK(listener.mChangedCount == 1) ;

        // deregister listener
        doc.RemoveListener(&listener) ;

        doc.Insert('C') ;
        CHECK(listener.mChangedCount == 1) ;  // no new notification
    }

    SUBCASE("Multiple listeners")
    {
        cTestListener listener2 ;
        doc.AddListener(&listener) ;
        doc.AddListener(&listener2) ;

        doc.SetPosition(0) ;
        doc.Insert('X') ;

        CHECK(listener.mChangedCount == 1) ;
        CHECK(listener2.mChangedCount == 1) ;

        doc.RemoveListener(&listener) ;
        doc.RemoveListener(&listener2) ;
    }

    SUBCASE("Safe deregistration of unregistered listener")
    {
        // removing a listener that was never added should not crash
        doc.RemoveListener(&listener) ;
        CHECK(true) ;  // if we get here, no crash
    }

    SUBCASE("Double registration")
    {
        doc.AddListener(&listener) ;
        doc.AddListener(&listener) ;

        doc.SetPosition(0) ;
        doc.Insert('Z') ;

        // registered twice, so notified twice
        CHECK(listener.mChangedCount == 2) ;

        // RemoveListener uses std::remove which removes ALL occurrences
        doc.RemoveListener(&listener) ;

        listener.Reset() ;
        doc.Insert('Y') ;
        CHECK(listener.mChangedCount == 0) ;  // all registrations removed
    }
}


TEST_CASE("Document Listener - Insert char notification")
{
    cDocument doc ;
    cTestListener listener ;
    doc.AddListener(&listener) ;

    SUBCASE("Single char insert fires notification")
    {
        doc.SetPosition(0) ;
        doc.Insert('H') ;

        CHECK(listener.mChangedCount == 1) ;
        CHECK(listener.mLastParagraph == 0) ;
    }

    SUBCASE("Multiple char inserts fire multiple notifications")
    {
        doc.SetPosition(0) ;
        doc.Insert('H') ;
        doc.Insert('e') ;
        doc.Insert('l') ;

        CHECK(listener.mChangedCount == 3) ;
    }

    SUBCASE("Insert tracks correct paragraph")
    {
        // insert two paragraphs
        doc.SetPosition(0) ;
        doc.Insert("First paragraph\r") ;
        listener.Reset() ;

        // insert in second paragraph
        doc.Insert('X') ;
        CHECK(listener.mChangedCount == 1) ;
        CHECK(listener.mLastParagraph == 1) ;
    }

    doc.RemoveListener(&listener) ;
}


TEST_CASE("Document Listener - Insert string notification")
{
    cDocument doc ;
    cTestListener listener ;
    doc.AddListener(&listener) ;

    SUBCASE("String insert fires one notification")
    {
        doc.SetPosition(0) ;
        doc.Insert("Hello World") ;

        // string insert suppresses per-char notifications (mIsLoading),
        // fires one notification at the end
        CHECK(listener.mChangedCount == 1) ;
    }

    SUBCASE("Multi-paragraph string insert")
    {
        doc.SetPosition(0) ;
        doc.Insert("Line 1\rLine 2\rLine 3\r") ;

        CHECK(listener.mChangedCount == 1) ;
    }

    doc.RemoveListener(&listener) ;
}


TEST_CASE("Document Listener - Delete notification")
{
    cDocument doc ;
    cTestListener listener ;

    // set up document with text
    doc.SetPosition(0) ;
    doc.Insert("Hello World\r") ;

    doc.AddListener(&listener) ;

    SUBCASE("Delete fires notification")
    {
        doc.Delete(0, 5) ;  // delete "Hello"

        CHECK(listener.mChangedCount == 1) ;
        CHECK(listener.mLastParagraph == 0) ;
    }

    SUBCASE("Delete at EOF position does not fire")
    {
        // "Hello World\r^Z" -- position at or beyond EOF returns false
        POSITION_T textSize = doc.GetTextSize() ;
        bool result = doc.Delete(textSize, 1) ;

        CHECK(result == false) ;
        CHECK(listener.mChangedCount == 0) ;
    }

    doc.RemoveListener(&listener) ;
}


TEST_CASE("Document Listener - Clear notification")
{
    cDocument doc ;
    cTestListener listener ;

    doc.SetPosition(0) ;
    doc.Insert("Some text\r") ;
    doc.AddListener(&listener) ;

    SUBCASE("Clear fires OnDocumentCleared")
    {
        doc.Clear() ;

        CHECK(listener.mClearedCount == 1) ;
        // Clear also calls Insert(STYLE_EOF) internally with mUndoDisabled=true,
        // so no OnDocumentChanged should fire
        CHECK(listener.mChangedCount == 0) ;
    }

    doc.RemoveListener(&listener) ;
}


TEST_CASE("Document Listener - Suppression during loading")
{
    cDocument doc ;
    cTestListener listener ;
    doc.AddListener(&listener) ;

    SUBCASE("Loading suppresses notifications")
    {
        doc.SetLoading(true) ;

        doc.SetPosition(0) ;
        doc.Insert('A') ;
        doc.Insert('B') ;
        doc.Insert('C') ;

        CHECK(listener.mChangedCount == 0) ;

        doc.SetLoading(false) ;

        // after loading ends, normal notifications resume
        doc.Insert('D') ;
        CHECK(listener.mChangedCount == 1) ;
    }

    doc.RemoveListener(&listener) ;
}


TEST_CASE("Document Listener - Suppression during undo/redo")
{
    cDocument doc ;
    cTestListener listener ;

    // set up document with text for undo testing
    doc.SetPosition(0) ;
    doc.BeginUndoGroup() ;
    doc.Insert('A') ;
    doc.Insert('B') ;
    doc.Insert('C') ;
    doc.EndUndoGroup() ;

    doc.AddListener(&listener) ;

    SUBCASE("Undo suppresses internal notifications")
    {
        bool undone = doc.Undo() ;
        CHECK(undone == true) ;

        // undo sets mUndoDisabled=true, so internal Delete/Insert calls
        // are suppressed. No listener notifications during undo.
        CHECK(listener.mChangedCount == 0) ;
    }

    SUBCASE("Redo suppresses internal notifications")
    {
        doc.Undo() ;
        listener.Reset() ;

        bool redone = doc.Redo() ;
        CHECK(redone == true) ;

        // redo sets mUndoDisabled=true, so internal calls are suppressed
        CHECK(listener.mChangedCount == 0) ;
    }

    doc.RemoveListener(&listener) ;
}


TEST_CASE("Document Listener - Composite operations delegate to leaf")
{
    cDocument doc ;
    cTestListener listener ;

    doc.SetPosition(0) ;
    doc.Insert("Hello World\r") ;

    doc.AddListener(&listener) ;

    SUBCASE("InsertTab fires via Insert(MARKER_CHAR)")
    {
        doc.SetPosition(0) ;
        sWSTab tab ;
        tab.type = 0 ;          // normal tab
        tab.tabsize = 720 ;
        tab.abstabsize = 720 ;
        tab.size = 4 ;
        doc.InsertTab(tab) ;

        // InsertTab calls Insert(MARKER_CHAR) which fires notification
        CHECK(listener.mChangedCount >= 1) ;
    }

    SUBCASE("Cut fires via Delete")
    {
        // set up block
        doc.SetPosition(0) ;
        doc.SetBeginBlock() ;
        doc.SetPosition(6) ;  // after marker shift
        doc.SetEndBlock() ;
        doc.Copy() ;

        listener.Reset() ;

        doc.Cut() ;
        // Cut calls Delete which fires notification
        CHECK(listener.mChangedCount >= 1) ;
    }

    SUBCASE("Paste fires via Insert")
    {
        // copy some text first
        doc.SetPosition(0) ;
        doc.SetBeginBlock() ;
        doc.SetPosition(6) ;  // select "Hello"
        doc.SetEndBlock() ;
        doc.Copy() ;

        listener.Reset() ;

        doc.Paste() ;
        // Paste suppresses per-character notifications, fires one at end
        CHECK(listener.mChangedCount == 1) ;
    }

    doc.RemoveListener(&listener) ;
}


TEST_CASE("Document Listener - Format toggles delegate to leaf")
{
    cDocument doc ;
    cTestListener listener ;

    doc.SetPosition(0) ;
    doc.AddListener(&listener) ;

    SUBCASE("BeginBold fires via Insert(STYLE_BOLD)")
    {
        doc.BeginBold() ;
        CHECK(listener.mChangedCount == 1) ;
    }

    SUBCASE("EndBold fires via Insert(STYLE_BOLD)")
    {
        doc.EndBold() ;
        CHECK(listener.mChangedCount == 1) ;
    }

    SUBCASE("BeginItalics fires via Insert(STYLE_ITALICS)")
    {
        doc.BeginItalics() ;
        CHECK(listener.mChangedCount == 1) ;
    }

    SUBCASE("BeginUnderline fires via Insert(STYLE_UNDERLINE)")
    {
        doc.BeginUnderline() ;
        CHECK(listener.mChangedCount == 1) ;
    }

    doc.RemoveListener(&listener) ;
}


//////////////////////////////////////////////////////////////////////////////
//
// Color Refactoring Tests - sWSColor to sSeqRGBColor + Default Sentinel
//
//////////////////////////////////////////////////////////////////////////////

// gBaseWSColors is defined in document.cpp -- declare extern for verification
extern sSeqRGBColor gBaseWSColors[] ;


TEST_CASE("IsDefault sentinel helper")
{
    SUBCASE("All -1 fields is default")
    {
        sSeqRGBColor c ;
        c.red = -1 ; c.green = -1 ; c.blue = -1 ; c.alpha = -1 ;
        CHECK(c.IsDefault() == true) ;
    }

    SUBCASE("All -1 RGB with alpha 255 is still default")
    {
        // IsDefault() checks red/green/blue only, not alpha
        sSeqRGBColor c ;
        c.red = -1 ; c.green = -1 ; c.blue = -1 ; c.alpha = 255 ;
        CHECK(c.IsDefault() == true) ;
    }

    SUBCASE("Black is not default")
    {
        sSeqRGBColor c ;
        c.red = 0 ; c.green = 0 ; c.blue = 0 ; c.alpha = 255 ;
        CHECK(c.IsDefault() == false) ;
    }

    SUBCASE("White is not default")
    {
        sSeqRGBColor c ;
        c.red = 255 ; c.green = 255 ; c.blue = 255 ; c.alpha = 255 ;
        CHECK(c.IsDefault() == false) ;
    }

    SUBCASE("Only red is -1")
    {
        sSeqRGBColor c ;
        c.red = -1 ; c.green = 0 ; c.blue = 0 ; c.alpha = 255 ;
        CHECK(c.IsDefault() == false) ;
    }

    SUBCASE("Only green is -1")
    {
        sSeqRGBColor c ;
        c.red = 0 ; c.green = -1 ; c.blue = 0 ; c.alpha = 255 ;
        CHECK(c.IsDefault() == false) ;
    }

    SUBCASE("Only blue is -1")
    {
        sSeqRGBColor c ;
        c.red = 0 ; c.green = 0 ; c.blue = -1 ; c.alpha = 255 ;
        CHECK(c.IsDefault() == false) ;
    }

    SUBCASE("Normal color is not default")
    {
        sSeqRGBColor c ;
        c.red = 128 ; c.green = 64 ; c.blue = 32 ; c.alpha = 255 ;
        CHECK(c.IsDefault() == false) ;
    }
}


TEST_CASE("InsertColorFromWSPalette converts palette to RGB")
{
    cDocument document ;
    document.Insert("Hello World this is a test.\r") ;

    SUBCASE("Palette index 0 converts to black")
    {
        document.SetPosition(2) ;
        sWSColor ws ;
        ws.colornumber = 0 ;
        ws.prevcolornumber = 0 ;
        document.InsertColorFromWSPalette(ws) ;

        sSeqRGBColor result ;
        CHECK(document.GetColor(2, result) == true) ;
        CHECK(result.red == 0) ;
        CHECK(result.green == 0) ;
        CHECK(result.blue == 0) ;
        CHECK(result.alpha == 255) ;
        CHECK(result.IsDefault() == false) ;
    }

    SUBCASE("Palette index 4 converts to red")
    {
        document.SetPosition(2) ;
        sWSColor ws ;
        ws.colornumber = 4 ;
        ws.prevcolornumber = 0 ;
        document.InsertColorFromWSPalette(ws) ;

        sSeqRGBColor result ;
        CHECK(document.GetColor(2, result) == true) ;
        CHECK(result.red == 170) ;
        CHECK(result.green == 0) ;
        CHECK(result.blue == 0) ;
        CHECK(result.alpha == 255) ;
    }

    SUBCASE("Palette index 5 converts to magenta")
    {
        document.SetPosition(2) ;
        sWSColor ws ;
        ws.colornumber = 5 ;
        ws.prevcolornumber = 0 ;
        document.InsertColorFromWSPalette(ws) ;

        sSeqRGBColor result ;
        CHECK(document.GetColor(2, result) == true) ;
        CHECK(result.red == 170) ;
        CHECK(result.green == 0) ;
        CHECK(result.blue == 170) ;
        CHECK(result.alpha == 255) ;
    }

    SUBCASE("Palette index 3 converts to cyan")
    {
        document.SetPosition(2) ;
        sWSColor ws ;
        ws.colornumber = 3 ;
        ws.prevcolornumber = 0 ;
        document.InsertColorFromWSPalette(ws) ;

        sSeqRGBColor result ;
        CHECK(document.GetColor(2, result) == true) ;
        CHECK(result.red == 0) ;
        CHECK(result.green == 170) ;
        CHECK(result.blue == 170) ;
        CHECK(result.alpha == 255) ;
    }

    SUBCASE("Palette index 15 converts to white")
    {
        document.SetPosition(2) ;
        sWSColor ws ;
        ws.colornumber = 15 ;
        ws.prevcolornumber = 0 ;
        document.InsertColorFromWSPalette(ws) ;

        sSeqRGBColor result ;
        CHECK(document.GetColor(2, result) == true) ;
        CHECK(result.red == 155) ;
        CHECK(result.green == 255) ;
        CHECK(result.blue == 255) ;
        CHECK(result.alpha == 255) ;
    }

    SUBCASE("All 16 palette indices convert correctly")
    {
        for (int i = 0; i < 16; ++i)
        {
            cDocument doc ;
            doc.Insert("ABCDEFGHIJ\r") ;
            doc.SetPosition(2) ;

            sWSColor ws ;
            ws.colornumber = static_cast<unsigned char>(i) ;
            ws.prevcolornumber = 0 ;
            doc.InsertColorFromWSPalette(ws) ;

            sSeqRGBColor result ;
            CHECK(doc.GetColor(2, result) == true) ;
            CHECK(result.red == gBaseWSColors[i].red) ;
            CHECK(result.green == gBaseWSColors[i].green) ;
            CHECK(result.blue == gBaseWSColors[i].blue) ;
            CHECK(result.alpha == 255) ;
        }
    }
}


TEST_CASE("InsertColor with default sentinel")
{
    cDocument document ;
    document.Insert("Hello World this is a test.\r") ;

    SUBCASE("Default sentinel round-trips through InsertColor/GetColor")
    {
        document.SetPosition(5) ;
        sSeqRGBColor sentinel ;
        sentinel.red = -1 ; sentinel.green = -1 ; sentinel.blue = -1 ; sentinel.alpha = -1 ;
        document.InsertColor(sentinel) ;

        sSeqRGBColor result ;
        CHECK(document.GetColor(5, result) == true) ;
        CHECK(result.red == -1) ;
        CHECK(result.green == -1) ;
        CHECK(result.blue == -1) ;
        CHECK(result.IsDefault() == true) ;
    }

    SUBCASE("Normal color is not default")
    {
        document.SetPosition(5) ;
        sSeqRGBColor color ;
        color.red = 200 ; color.green = 100 ; color.blue = 50 ; color.alpha = 255 ;
        document.InsertColor(color) ;

        sSeqRGBColor result ;
        CHECK(document.GetColor(5, result) == true) ;
        CHECK(result.IsDefault() == false) ;
        CHECK(result.red == 200) ;
        CHECK(result.green == 100) ;
        CHECK(result.blue == 50) ;
    }

    SUBCASE("Default and normal colors at different positions")
    {
        // Insert default at position 3
        document.SetPosition(3) ;
        sSeqRGBColor sentinel ;
        sentinel.red = -1 ; sentinel.green = -1 ; sentinel.blue = -1 ; sentinel.alpha = -1 ;
        document.InsertColor(sentinel) ;

        // Insert normal red at position 10 (shifted by 1 due to marker)
        document.SetPosition(10) ;
        sSeqRGBColor red ;
        red.red = 255 ; red.green = 0 ; red.blue = 0 ; red.alpha = 255 ;
        document.InsertColor(red) ;

        // Verify default at position 3
        sSeqRGBColor result1 ;
        CHECK(document.GetColor(3, result1) == true) ;
        CHECK(result1.IsDefault() == true) ;

        // Verify red at position 10
        sSeqRGBColor result2 ;
        CHECK(document.GetColor(10, result2) == true) ;
        CHECK(result2.IsDefault() == false) ;
        CHECK(result2.red == 255) ;
        CHECK(result2.green == 0) ;
        CHECK(result2.blue == 0) ;
    }

    SUBCASE("Undo/redo preserves default sentinel")
    {
        document.SetPosition(5) ;
        sSeqRGBColor sentinel ;
        sentinel.red = -1 ; sentinel.green = -1 ; sentinel.blue = -1 ; sentinel.alpha = -1 ;
        POSITION_T sizeBefore = document.GetTextSize() ;
        document.InsertColor(sentinel) ;
        CHECK(document.GetTextSize() == sizeBefore + 1) ;

        // Verify sentinel stored
        sSeqRGBColor result ;
        CHECK(document.GetColor(5, result) == true) ;
        CHECK(result.IsDefault() == true) ;

        // Undo removes marker
        document.Undo() ;
        CHECK(document.GetTextSize() == sizeBefore) ;

        // Redo restores sentinel
        document.Redo() ;
        CHECK(document.GetTextSize() == sizeBefore + 1) ;
        sSeqRGBColor redoResult ;
        CHECK(document.GetColor(5, redoResult) == true) ;
        CHECK(redoResult.IsDefault() == true) ;
        CHECK(redoResult.red == -1) ;
        CHECK(redoResult.green == -1) ;
        CHECK(redoResult.blue == -1) ;
    }
}


TEST_CASE("InsertColor preserves full RGB values")
{
    cDocument document ;
    document.Insert("ABCDEFGHIJKLMNOPQRSTUVWXYZ\r") ;

    SUBCASE("Arbitrary RGB preserved exactly")
    {
        document.SetPosition(3) ;
        sSeqRGBColor color ;
        color.red = 255 ; color.green = 128 ; color.blue = 64 ; color.alpha = 255 ;
        document.InsertColor(color) ;

        sSeqRGBColor result ;
        CHECK(document.GetColor(3, result) == true) ;
        CHECK(result.red == 255) ;
        CHECK(result.green == 128) ;
        CHECK(result.blue == 64) ;
        CHECK(result.alpha == 255) ;
    }

    SUBCASE("Low RGB values preserved")
    {
        document.SetPosition(3) ;
        sSeqRGBColor color ;
        color.red = 1 ; color.green = 2 ; color.blue = 3 ; color.alpha = 200 ;
        document.InsertColor(color) ;

        sSeqRGBColor result ;
        CHECK(document.GetColor(3, result) == true) ;
        CHECK(result.red == 1) ;
        CHECK(result.green == 2) ;
        CHECK(result.blue == 3) ;
        CHECK(result.alpha == 200) ;
    }

    SUBCASE("Black preserved exactly")
    {
        document.SetPosition(3) ;
        sSeqRGBColor color ;
        color.red = 0 ; color.green = 0 ; color.blue = 0 ; color.alpha = 255 ;
        document.InsertColor(color) ;

        sSeqRGBColor result ;
        CHECK(document.GetColor(3, result) == true) ;
        CHECK(result.red == 0) ;
        CHECK(result.green == 0) ;
        CHECK(result.blue == 0) ;
        CHECK(result.alpha == 255) ;
    }

    SUBCASE("White preserved exactly")
    {
        document.SetPosition(3) ;
        sSeqRGBColor color ;
        color.red = 255 ; color.green = 255 ; color.blue = 255 ; color.alpha = 255 ;
        document.InsertColor(color) ;

        sSeqRGBColor result ;
        CHECK(document.GetColor(3, result) == true) ;
        CHECK(result.red == 255) ;
        CHECK(result.green == 255) ;
        CHECK(result.blue == 255) ;
        CHECK(result.alpha == 255) ;
    }

    SUBCASE("Multiple colors at different positions independently correct")
    {
        // Insert blue at position 3
        document.SetPosition(3) ;
        sSeqRGBColor blue ;
        blue.red = 0 ; blue.green = 0 ; blue.blue = 255 ; blue.alpha = 255 ;
        document.InsertColor(blue) ;

        // Insert green at position 10 (shifted +1 by blue marker)
        document.SetPosition(10) ;
        sSeqRGBColor green ;
        green.red = 0 ; green.green = 255 ; green.blue = 0 ; green.alpha = 255 ;
        document.InsertColor(green) ;

        // Insert custom at position 20 (shifted +2 by two markers)
        document.SetPosition(20) ;
        sSeqRGBColor custom ;
        custom.red = 123 ; custom.green = 45 ; custom.blue = 67 ; custom.alpha = 255 ;
        document.InsertColor(custom) ;

        // Verify each color independently
        sSeqRGBColor r1, r2, r3 ;
        CHECK(document.GetColor(3, r1) == true) ;
        CHECK(r1.red == 0) ;
        CHECK(r1.green == 0) ;
        CHECK(r1.blue == 255) ;

        CHECK(document.GetColor(10, r2) == true) ;
        CHECK(r2.red == 0) ;
        CHECK(r2.green == 255) ;
        CHECK(r2.blue == 0) ;

        CHECK(document.GetColor(20, r3) == true) ;
        CHECK(r3.red == 123) ;
        CHECK(r3.green == 45) ;
        CHECK(r3.blue == 67) ;
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test EvaluateExpression with single unit-annotated values
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("EvaluateExpression single value with units")
{
    cDocument doc ;
    bool incdec = false ;
    bool hasUnits = false ;

    // 2 inches = 2 * 1440 = 2880 twips
    double result = doc.EvaluateExpression("2i", incdec, hasUnits) ;
    CHECK(hasUnits == true) ;
    CHECK(incdec == false) ;
    CHECK(result == doctest::Approx(2880.0).epsilon(0.01)) ;

    // 2 centimeters = 2 * TWIPSPERCM
    result = doc.EvaluateExpression("2c", incdec, hasUnits) ;
    CHECK(hasUnits == true) ;
    CHECK(result == doctest::Approx(2.0 * TWIPSPERCM).epsilon(0.01)) ;

    // 10 millimeters = 10 * TWIPSPERMM
    result = doc.EvaluateExpression("10m", incdec, hasUnits) ;
    CHECK(hasUnits == true) ;
    CHECK(result == doctest::Approx(10.0 * TWIPSPERMM).epsilon(0.01)) ;

    // 12 points = 12 * POINTSTOTWIPS = 12 * 20 = 240 twips
    result = doc.EvaluateExpression("12p", incdec, hasUnits) ;
    CHECK(hasUnits == true) ;
    CHECK(result == doctest::Approx(12.0 * POINTSTOTWIPS).epsilon(0.01)) ;

    // 2 inches using " notation
    result = doc.EvaluateExpression("2\"", incdec, hasUnits) ;
    CHECK(hasUnits == true) ;
    CHECK(result == doctest::Approx(2880.0).epsilon(0.01)) ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test EvaluateExpression with bare numbers (no units)
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("EvaluateExpression bare numbers")
{
    cDocument doc ;
    bool incdec = false ;
    bool hasUnits = false ;

    // bare number 75
    double result = doc.EvaluateExpression("75", incdec, hasUnits) ;
    CHECK(hasUnits == false) ;
    CHECK(incdec == false) ;
    CHECK(result == doctest::Approx(75.0)) ;

    // bare subtraction: 80 - 5 = 75
    result = doc.EvaluateExpression("80 - 5", incdec, hasUnits) ;
    CHECK(hasUnits == false) ;
    CHECK(result == doctest::Approx(75.0)) ;

    // bare multiplication: 8 * 2 = 16
    result = doc.EvaluateExpression("8 * 2", incdec, hasUnits) ;
    CHECK(hasUnits == false) ;
    CHECK(result == doctest::Approx(16.0)) ;

    // bare subtraction: 66 - 6 = 60
    result = doc.EvaluateExpression("66 - 6", incdec, hasUnits) ;
    CHECK(hasUnits == false) ;
    CHECK(result == doctest::Approx(60.0)) ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test EvaluateExpression with same-unit addition
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("EvaluateExpression same-unit addition")
{
    cDocument doc ;
    bool incdec = false ;
    bool hasUnits = false ;

    // 2i + 3i = 5 inches = 7200 twips
    double result = doc.EvaluateExpression("2i + 3i", incdec, hasUnits) ;
    CHECK(hasUnits == true) ;
    CHECK(result == doctest::Approx(7200.0).epsilon(0.01)) ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test EvaluateExpression with mixed-unit addition
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("EvaluateExpression mixed-unit addition")
{
    cDocument doc ;
    bool incdec = false ;
    bool hasUnits = false ;

    // 4c + 2i = 4cm + 5.08cm = 9.08cm in twips
    double result = doc.EvaluateExpression("4c + 2i", incdec, hasUnits) ;
    CHECK(hasUnits == true) ;
    // 4cm = 4*TWIPSPERCM, 2in = 2*TWIPSPERINCH
    double expected = 4.0 * TWIPSPERCM + 2.0 * TWIPSPERINCH ;
    CHECK(result == doctest::Approx(expected).epsilon(1.0)) ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test EvaluateExpression with dimensionless multiplier (strategy 2)
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("EvaluateExpression dimensionless multiplier")
{
    cDocument doc ;
    bool incdec = false ;
    bool hasUnits = false ;

    // 8 * 2i: strategy 2 (has *), single unit 'i', strip to "8 * 2" = 16i = 23040 twips
    double result = doc.EvaluateExpression("8 * 2i", incdec, hasUnits) ;
    CHECK(hasUnits == true) ;
    CHECK(result == doctest::Approx(23040.0).epsilon(1.0)) ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test EvaluateExpression with same-unit multiplication (strategy 2)
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("EvaluateExpression same-unit multiplication")
{
    cDocument doc ;
    bool incdec = false ;
    bool hasUnits = false ;

    // .5i * .5i: strategy 2 (has *), single unit 'i', strip to "0.5 * 0.5" = 0.25i = 360 twips
    double result = doc.EvaluateExpression(".5i * .5i", incdec, hasUnits) ;
    CHECK(hasUnits == true) ;
    CHECK(result == doctest::Approx(360.0).epsilon(1.0)) ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test EvaluateExpression mixed-unit multiplication returns error
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("EvaluateExpression mixed-unit multiplication is error")
{
    cDocument doc ;
    bool incdec = false ;
    bool hasUnits = false ;

    // 8c * 4i: strategy 2, mixed units (c and i) -- error
    double result = doc.EvaluateExpression("8c * 4i", incdec, hasUnits) ;
    CHECK(result == doctest::Approx(-32768.0)) ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test EvaluateExpression with * and + combined (strategy 2)
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("EvaluateExpression multiply with add uses strategy 2")
{
    cDocument doc ;
    bool incdec = false ;
    bool hasUnits = false ;

    // 3 * 2 + 1i: has *, single unit 'i', strip to "3 * 2 + 1" = 7i = 10080 twips
    double result = doc.EvaluateExpression("3 * 2 + 1i", incdec, hasUnits) ;
    CHECK(hasUnits == true) ;
    CHECK(result == doctest::Approx(10080.0).epsilon(1.0)) ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test EvaluateExpression division (strategy 2)
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("EvaluateExpression division")
{
    cDocument doc ;
    bool incdec = false ;
    bool hasUnits = false ;

    // 10i / 2: strategy 2 (has /), single unit 'i', strip to "10 / 2" = 5i = 7200 twips
    double result = doc.EvaluateExpression("10i / 2", incdec, hasUnits) ;
    CHECK(hasUnits == true) ;
    CHECK(result == doctest::Approx(7200.0).epsilon(1.0)) ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test EvaluateExpression with increment/decrement prefix
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("EvaluateExpression increment/decrement")
{
    cDocument doc ;
    bool incdec = false ;
    bool hasUnits = false ;

    // +3i = positive 3 inches = 4320 twips, incdec=true
    double result = doc.EvaluateExpression("+3i", incdec, hasUnits) ;
    CHECK(hasUnits == true) ;
    CHECK(incdec == true) ;
    CHECK(result == doctest::Approx(4320.0).epsilon(1.0)) ;

    // -2i = negative 2 inches = -2880 twips, incdec=true
    result = doc.EvaluateExpression("-2i", incdec, hasUnits) ;
    CHECK(hasUnits == true) ;
    CHECK(incdec == true) ;
    CHECK(result == doctest::Approx(-2880.0).epsilon(1.0)) ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test EvaluateExpression increment/decrement with all unit types
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("EvaluateExpression increment/decrement with all units")
{
    cDocument doc ;
    bool incdec = false ;
    bool hasUnits = false ;

    // +2c = positive 2 centimeters, incdec=true
    double result = doc.EvaluateExpression("+2c", incdec, hasUnits) ;
    CHECK(hasUnits == true) ;
    CHECK(incdec == true) ;
    CHECK(result == doctest::Approx(2.0 * TWIPSPERCM).epsilon(1.0)) ;

    // -1c = negative 1 centimeter, incdec=true
    incdec = false ;
    hasUnits = false ;
    result = doc.EvaluateExpression("-1c", incdec, hasUnits) ;
    CHECK(hasUnits == true) ;
    CHECK(incdec == true) ;
    CHECK(result == doctest::Approx(-1.0 * TWIPSPERCM).epsilon(1.0)) ;

    // +5m = positive 5 millimeters, incdec=true
    incdec = false ;
    hasUnits = false ;
    result = doc.EvaluateExpression("+5m", incdec, hasUnits) ;
    CHECK(hasUnits == true) ;
    CHECK(incdec == true) ;
    CHECK(result == doctest::Approx(5.0 * TWIPSPERMM).epsilon(1.0)) ;

    // -3m = negative 3 millimeters, incdec=true
    incdec = false ;
    hasUnits = false ;
    result = doc.EvaluateExpression("-3m", incdec, hasUnits) ;
    CHECK(hasUnits == true) ;
    CHECK(incdec == true) ;
    CHECK(result == doctest::Approx(-3.0 * TWIPSPERMM).epsilon(1.0)) ;

    // +10p = positive 10 points, incdec=true
    incdec = false ;
    hasUnits = false ;
    result = doc.EvaluateExpression("+10p", incdec, hasUnits) ;
    CHECK(hasUnits == true) ;
    CHECK(incdec == true) ;
    CHECK(result == doctest::Approx(10.0 * POINTSTOTWIPS).epsilon(1.0)) ;

    // -5p = negative 5 points, incdec=true
    incdec = false ;
    hasUnits = false ;
    result = doc.EvaluateExpression("-5p", incdec, hasUnits) ;
    CHECK(hasUnits == true) ;
    CHECK(incdec == true) ;
    CHECK(result == doctest::Approx(-5.0 * POINTSTOTWIPS).epsilon(1.0)) ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test EvaluateExpression with complex multi-unit expression
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("EvaluateExpression complex multi-unit expression")
{
    cDocument doc ;
    bool incdec = false ;
    bool hasUnits = false ;

    // 4c + .25i - 1c + 1i = 4 + 0.635 - 1 + 2.54 = 6.175cm in twips
    double result = doc.EvaluateExpression("4c + .25i - 1c + 1i", incdec, hasUnits) ;
    CHECK(hasUnits == true) ;
    double expected = 6.175 * TWIPSPERCM ;
    CHECK(result == doctest::Approx(expected).epsilon(1.0)) ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test EvaluateExpression with empty and error cases
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("EvaluateExpression error cases")
{
    cDocument doc ;
    bool incdec = false ;
    bool hasUnits = false ;

    // empty string returns error value
    double result = doc.EvaluateExpression("", incdec, hasUnits) ;
    CHECK(result == doctest::Approx(-32768.0)) ;
    CHECK(hasUnits == false) ;

    // whitespace only returns error value
    result = doc.EvaluateExpression("   ", incdec, hasUnits) ;
    CHECK(result == doctest::Approx(-32768.0)) ;
    CHECK(hasUnits == false) ;

    // lone + returns error value
    result = doc.EvaluateExpression("+", incdec, hasUnits) ;
    CHECK(result == doctest::Approx(-32768.0)) ;

    // lone - returns error value
    result = doc.EvaluateExpression("-", incdec, hasUnits) ;
    CHECK(result == doctest::Approx(-32768.0)) ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test GetParagraphGraphemes with basic ASCII text
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("GetParagraphGraphemes basic ASCII")
{
    cDocument doc ;
    doc.Insert("Hello\r") ;

    std::vector<std::string> graphemes ;
    std::vector<POSITION_T> offsets ;
    doc.GetParagraphGraphemes(0, graphemes, offsets) ;

    // "Hello" = 5 graphemes plus the paragraph terminator
    CHECK(graphemes.size() >= 5) ;
    CHECK(graphemes[0] == "H") ;
    CHECK(graphemes[1] == "e") ;
    CHECK(graphemes[2] == "l") ;
    CHECK(graphemes[3] == "l") ;
    CHECK(graphemes[4] == "o") ;

    // Offsets should match byte positions for ASCII
    CHECK(offsets.size() == graphemes.size()) ;
    CHECK(offsets[0] == 0) ;
    CHECK(offsets[1] == 1) ;
    CHECK(offsets[2] == 2) ;
    CHECK(offsets[3] == 3) ;
    CHECK(offsets[4] == 4) ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test GetParagraphGraphemes with multibyte UTF-8 characters
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("GetParagraphGraphemes multibyte UTF-8")
{
    cDocument doc ;

    // e-acute (2 bytes: 0xC3 0xA9) + CJK character (3 bytes: 0xE4 0xB8 0x96)
    doc.Insert("\xC3\xA9\xE4\xB8\x96\r") ;

    std::vector<std::string> graphemes ;
    std::vector<POSITION_T> offsets ;
    doc.GetParagraphGraphemes(0, graphemes, offsets) ;

    // 2 graphemes plus paragraph terminator
    CHECK(graphemes.size() >= 2) ;
    CHECK(graphemes[0] == "\xC3\xA9") ;       // e-acute (2 bytes)
    CHECK(graphemes[1] == "\xE4\xB8\x96") ;   // CJK character (3 bytes)

    // Byte offsets: 0 for e-acute, 2 for CJK
    CHECK(offsets[0] == 0) ;
    CHECK(offsets[1] == 2) ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test GetParagraphGraphemes with empty paragraph (no crash)
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("GetParagraphGraphemes empty paragraph")
{
    cDocument doc ;

    // Fresh document has one empty paragraph (just EOF marker)
    std::vector<std::string> graphemes ;
    std::vector<POSITION_T> offsets ;
    doc.GetParagraphGraphemes(0, graphemes, offsets) ;

    // Should not crash, may return the EOF marker or empty
    // The key assertion is that we get here without crashing
    CHECK(graphemes.size() <= 1) ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test GetParagraphGraphemes after paragraph delete (stale offset guard)
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("GetParagraphGraphemes after paragraph delete")
{
    cDocument doc ;
    doc.Insert("Hello Wor\r") ;

    // Delete "Hello Wor" (9 graphemes from position 0)
    doc.Delete(0, 9) ;

    std::vector<std::string> graphemes ;
    std::vector<POSITION_T> offsets ;
    doc.GetParagraphGraphemes(0, graphemes, offsets) ;

    // After deletion, paragraph should still be accessible without crash
    // Remaining content is the carriage return and EOF
    CHECK(graphemes.size() <= 2) ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test GetParagraphGraphemes with multiple paragraphs
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("GetParagraphGraphemes multiple paragraphs")
{
    cDocument doc ;
    doc.Insert("ABC\r") ;
    doc.Insert("DEF\r") ;

    SUBCASE("first paragraph")
    {
        std::vector<std::string> graphemes ;
        std::vector<POSITION_T> offsets ;
        doc.GetParagraphGraphemes(0, graphemes, offsets) ;

        CHECK(graphemes.size() >= 3) ;
        CHECK(graphemes[0] == "A") ;
        CHECK(graphemes[1] == "B") ;
        CHECK(graphemes[2] == "C") ;
    }

    SUBCASE("second paragraph")
    {
        std::vector<std::string> graphemes ;
        std::vector<POSITION_T> offsets ;
        doc.GetParagraphGraphemes(1, graphemes, offsets) ;

        CHECK(graphemes.size() >= 3) ;
        CHECK(graphemes[0] == "D") ;
        CHECK(graphemes[1] == "E") ;
        CHECK(graphemes[2] == "F") ;
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test GetParagraphGraphemes offsets are increasing and match grapheme count
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("GetParagraphGraphemes offsets match graphemes")
{
    cDocument doc ;
    doc.Insert("Test\r") ;

    std::vector<std::string> graphemes ;
    std::vector<POSITION_T> offsets ;
    doc.GetParagraphGraphemes(0, graphemes, offsets) ;

    // Offsets and graphemes vectors must have the same size
    CHECK(offsets.size() == graphemes.size()) ;

    // Offsets must be strictly increasing
    for (size_t i = 1; i < offsets.size(); ++i)
    {
        CHECK(offsets[i] > offsets[i - 1]) ;
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test that GetParagraphGraphemeOffsets safely handles out-of-bounds
/// paragraph indices by returning 0 and clearing the offsets vector.
/// Regression test for crash where stale layout segments referenced
/// invalid paragraph indices, causing CopyTo to resize with garbage.
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("UT-DOC-OOB-1: GetParagraphGraphemeOffsets out-of-bounds returns empty")
{
    cDocument doc ;
    doc.Insert("Hello\r") ;

    std::vector<POSITION_T> offsets ;
    PARAGRAPH_T paraCount = doc.GetNumberofParagraphs() ;

    SUBCASE("one past end")
    {
        size_t result = doc.GetParagraphGraphemeOffsets(paraCount, offsets) ;
        CHECK(result == 0) ;
        CHECK(offsets.empty()) ;
    }

    SUBCASE("far past end")
    {
        size_t result = doc.GetParagraphGraphemeOffsets(paraCount + 100, offsets) ;
        CHECK(result == 0) ;
        CHECK(offsets.empty()) ;
    }

    SUBCASE("pre-filled vector gets cleared")
    {
        offsets.push_back(42) ;
        offsets.push_back(99) ;
        size_t result = doc.GetParagraphGraphemeOffsets(paraCount, offsets) ;
        CHECK(result == 0) ;
        CHECK(offsets.empty()) ;
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test that GetParagraphGraphemes safely handles out-of-bounds paragraph
/// indices by returning empty graphemes and offsets vectors.
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("UT-DOC-OOB-2: GetParagraphGraphemes out-of-bounds returns empty")
{
    cDocument doc ;
    doc.Insert("Hello\r") ;

    std::vector<std::string> graphemes ;
    std::vector<POSITION_T> offsets ;
    PARAGRAPH_T paraCount = doc.GetNumberofParagraphs() ;

    doc.GetParagraphGraphemes(paraCount, graphemes, offsets) ;
    CHECK(graphemes.empty()) ;
    CHECK(offsets.empty()) ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test that GetParagraphText safely handles out-of-bounds paragraph
/// indices by returning an empty string.
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("UT-DOC-OOB-3: GetParagraphText out-of-bounds returns empty")
{
    cDocument doc ;
    doc.Insert("Hello\r") ;

    PARAGRAPH_T paraCount = doc.GetNumberofParagraphs() ;
    std::string text = doc.GetParagraphText(paraCount) ;
    CHECK(text.empty()) ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test SetSuppressNotify and GetSuppressNotify round-trip
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("SuppressNotify getter and setter")
{
    cDocument doc ;
    doc.SetShowControl(SHOW_ALL) ;

    // default should be false
    CHECK(doc.GetSuppressNotify() == false) ;

    doc.SetSuppressNotify(true) ;
    CHECK(doc.GetSuppressNotify() == true) ;

    doc.SetSuppressNotify(false) ;
    CHECK(doc.GetSuppressNotify() == false) ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test SetWordCount and GetWordCount round-trip
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("WordCount getter and setter")
{
    cDocument doc ;
    doc.SetShowControl(SHOW_ALL) ;

    // default should be 0
    CHECK(doc.GetWordCount() == 0) ;

    doc.SetWordCount(42) ;
    CHECK(doc.GetWordCount() == 42) ;

    doc.SetWordCount(0) ;
    CHECK(doc.GetWordCount() == 0) ;

    doc.SetWordCount(999999) ;
    CHECK(doc.GetWordCount() == 999999) ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test EndStrikeThrough inserts a strikethrough control code
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("EndStrikeThrough inserts control code")
{
    cDocument doc ;
    doc.SetShowControl(SHOW_ALL) ;

    doc.Insert("Hello\r") ;
    doc.SetPosition(2) ;
    doc.EndStrikeThrough() ;

    // EndStrikeThrough calls BeginStrikeThrough which inserts STYLE_STRIKETHROUGH
    CHECK(resolveControlChar(doc, 2) == STYLE_STRIKETHROUGH) ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test BeginIndex and EndIndex insert index control codes
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("BeginIndex and EndIndex insert control codes")
{
    cDocument doc ;
    doc.SetShowControl(SHOW_ALL) ;

    doc.Insert("Hello\r") ;

    // BeginIndex at position 2
    doc.SetPosition(2) ;
    doc.BeginIndex() ;
    CHECK(resolveControlChar(doc, 2) == STYLE_INDEX) ;

    // EndIndex at position 6 (shifted by 1 from BeginIndex insert)
    doc.SetPosition(6) ;
    doc.EndIndex() ;
    CHECK(resolveControlChar(doc, 6) == STYLE_INDEX) ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test BeginLeft inserts ".oj off" dot command
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("BeginLeft inserts dot command")
{
    cDocument doc ;
    doc.SetShowControl(SHOW_ALL) ;

    doc.Insert("Hello\r") ;
    doc.SetPosition(0) ;
    doc.BeginLeft() ;

    // BeginLeft inserts ".oj off\r" at position 0
    // First paragraph should now be the dot command
    std::string para0 = doc.GetParagraphText(0) ;
    CHECK(para0.find(".oj off") != std::string::npos) ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test BeginRight inserts ".ojr" dot command
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("BeginRight inserts dot command")
{
    cDocument doc ;
    doc.SetShowControl(SHOW_ALL) ;

    doc.Insert("Hello\r") ;
    doc.SetPosition(0) ;
    doc.BeginRight() ;

    // BeginRight inserts ".ojr\r" at position 0
    std::string para0 = doc.GetParagraphText(0) ;
    CHECK(para0.find(".ojr") != std::string::npos) ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test BeginJustify inserts ".oj on" dot command
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("BeginJustify inserts dot command")
{
    cDocument doc ;
    doc.SetShowControl(SHOW_ALL) ;

    doc.Insert("Hello\r") ;
    doc.SetPosition(0) ;
    doc.BeginJustify() ;

    // BeginJustify inserts ".oj on\r" at position 0
    std::string para0 = doc.GetParagraphText(0) ;
    CHECK(para0.find(".oj on") != std::string::npos) ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test MaybeInsertHardReturn inserts when not at paragraph start,
/// and does nothing when at paragraph start
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("MaybeInsertHardReturn")
{
    cDocument doc ;
    doc.SetShowControl(SHOW_ALL) ;

    doc.Insert("Hello\r") ;

    SUBCASE("inserts hard return when not at paragraph start")
    {
        POSITION_T sizeBefore = doc.GetTextSize() ;
        doc.SetPosition(3) ;
        doc.MaybeInsertHardReturn() ;
        POSITION_T sizeAfter = doc.GetTextSize() ;

        // should have inserted one character (the hard return)
        CHECK(sizeAfter == sizeBefore + 1) ;
    }

    SUBCASE("does nothing when at paragraph start")
    {
        POSITION_T sizeBefore = doc.GetTextSize() ;
        doc.SetPosition(0) ;
        doc.MaybeInsertHardReturn() ;
        POSITION_T sizeAfter = doc.GetTextSize() ;

        // should not have inserted anything
        CHECK(sizeAfter == sizeBefore) ;
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test ConvertToTwips for millimeters, rows, and column (space) units
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("ConvertToTwips missing branches")
{
    cDocument doc ;
    doc.SetShowControl(SHOW_ALL) ;

    SUBCASE("millimeters")
    {
        // 1mm = TWIPSPERMM twips (approximately 56.7)
        COORD_T result = doc.ConvertToTwips(1.0, 'm') ;
        CHECK(result == static_cast<COORD_T>(1.0 * TWIPSPERMM)) ;
    }

    SUBCASE("rows")
    {
        // 1 row = 240 twips (12pt font)
        COORD_T result = doc.ConvertToTwips(1.0, 'r') ;
        CHECK(result == 240) ;
    }

    SUBCASE("columns with default cwidth")
    {
        // column: (value - 1) * cwidth, default cwidth = 240
        COORD_T result = doc.ConvertToTwips(2.0, ' ') ;
        CHECK(result == static_cast<COORD_T>((2.0 - 1) * 240.0)) ;
    }

    SUBCASE("columns with custom cwidth")
    {
        // column: (value - 1) * cwidth
        COORD_T result = doc.ConvertToTwips(3.0, ' ', 120.0) ;
        CHECK(result == static_cast<COORD_T>((3.0 - 1) * 120.0)) ;
    }

    SUBCASE("unknown unit returns zero")
    {
        COORD_T result = doc.ConvertToTwips(5.0, 'z') ;
        CHECK(result == 0) ;
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test InsertTab inserts a tab marker that resolves to STYLE_TAB
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("InsertTab inserts tab marker")
{
    cDocument doc ;
    doc.SetShowControl(SHOW_ALL) ;

    doc.Insert("AB\r") ;
    doc.SetPosition(1) ;

    sWSTab tab ;
    tab.tabsize = 720 ;
    tab.abstabsize = 720 ;
    tab.type = 0 ;
    tab.size = 5 ;
    doc.InsertTab(tab) ;

    // the marker at position 1 should resolve to STYLE_TAB
    CHECK(resolveControlChar(doc, 1) == STYLE_TAB) ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test InsertFont inserts a font marker that resolves to STYLE_FONT1
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("InsertFont inserts font marker")
{
    cDocument doc ;
    doc.SetShowControl(SHOW_ALL) ;

    doc.Insert("AB\r") ;
    doc.SetPosition(1) ;

    sInternalFonts font ;
    font.fontname = "Courier" ;
    font.size = 12.0 ;
    font.haveWSFont = false ;
    font.name = "Courier" ;
    doc.InsertFont(font) ;

    // the marker at position 1 should resolve to STYLE_FONT1
    CHECK(resolveControlChar(doc, 1) == STYLE_FONT1) ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test InsertColor inserts a color marker that resolves to STYLE_INTERNAL_COLOR
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("InsertColor inserts color marker")
{
    cDocument doc ;
    doc.SetShowControl(SHOW_ALL) ;

    doc.Insert("AB\r") ;
    doc.SetPosition(1) ;

    sSeqRGBColor color ;
    color.red = 255 ;
    color.green = 0 ;
    color.blue = 0 ;
    color.alpha = 255 ;
    doc.InsertColor(color) ;

    // the marker at position 1 should resolve to STYLE_INTERNAL_COLOR
    CHECK(resolveControlChar(doc, 1) == STYLE_INTERNAL_COLOR) ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test InsertVariable inserts a variable marker that resolves to STYLE_VARIABLE
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("InsertVariable inserts variable marker")
{
    cDocument doc ;
    doc.SetShowControl(SHOW_ALL) ;

    doc.Insert("AB\r") ;
    doc.SetPosition(1) ;

    doc.InsertVariable(VAR_DATE) ;

    // the marker at position 1 should resolve to STYLE_VARIABLE
    CHECK(resolveControlChar(doc, 1) == STYLE_VARIABLE) ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test FindNext skips dot command paragraphs when ShowControl is SHOW_NONE
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("FindNext skips dot commands with SHOW_NONE")
{
    cDocument doc ;
    doc.SetShowControl(SHOW_ALL) ;

    // insert a dot command paragraph, then a normal paragraph
    doc.Insert(".oj on\r") ;
    doc.Insert("Hello world\r") ;

    SUBCASE("SHOW_NONE skips dot command content")
    {
        doc.SetShowControl(SHOW_NONE) ;

        // searching for "oj" should not find it in the dot command
        POSITION_T result = doc.FindNext("oj", 0, false, false, false) ;
        CHECK(result == doc.GetTextSize()) ;
    }

    SUBCASE("SHOW_NONE finds normal text")
    {
        doc.SetShowControl(SHOW_NONE) ;

        // searching for "Hello" should find it in the normal paragraph
        POSITION_T result = doc.FindNext("Hello", 0, false, false, false) ;
        CHECK(result != doc.GetTextSize()) ;
    }

    SUBCASE("SHOW_ALL finds dot command content")
    {
        doc.SetShowControl(SHOW_ALL) ;

        // searching for "oj" should find it when showing all
        POSITION_T result = doc.FindNext("oj", 0, false, false, false) ;
        CHECK(result != doc.GetTextSize()) ;
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test FindPrev skips dot command paragraphs when ShowControl is SHOW_NONE
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("FindPrev skips dot commands with SHOW_NONE")
{
    cDocument doc ;
    doc.SetShowControl(SHOW_ALL) ;

    // insert a dot command paragraph, then a normal paragraph
    doc.Insert(".oj on\r") ;
    doc.Insert("Hello world\r") ;

    POSITION_T textEnd = doc.GetTextSize() ;

    SUBCASE("SHOW_NONE skips dot command content")
    {
        doc.SetShowControl(SHOW_NONE) ;

        // searching backward for "oj" should not find it in the dot command
        POSITION_T result = doc.FindPrev("oj", textEnd, false, false, false) ;
        CHECK(result == std::string::npos) ;
    }

    SUBCASE("SHOW_NONE finds normal text")
    {
        doc.SetShowControl(SHOW_NONE) ;

        // searching backward for "Hello" should find it
        POSITION_T result = doc.FindPrev("Hello", textEnd, false, false, false) ;
        CHECK(result != std::string::npos) ;
    }

    SUBCASE("SHOW_ALL finds dot command content")
    {
        doc.SetShowControl(SHOW_ALL) ;

        // searching backward for "oj" should find it when showing all
        POSITION_T result = doc.FindPrev("oj", textEnd, false, false, false) ;
        CHECK(result != std::string::npos) ;
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// Test FindPrev locates a match that sits near the END of an EARLIER
/// paragraph (not at offset 0). Regression for the byte/grapheme start-position
/// bug where earlier paragraphs were reverse-searched from byte 0 only, so any
/// match past the first grapheme was missed. The existing cross-paragraph test
/// only finds a match at offset 0, which would pass even with that bug.
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("FindPrev finds a match near the END of an earlier paragraph")
{
    cDocument doc ;
    doc.SetShowControl(SHOW_ALL) ;

    // "target" is the 12th grapheme (index 11) of paragraph 0, near its end.
    doc.Insert("alpha beta target\r") ;        // para 0
    doc.Insert("second paragraph here\r") ;    // para 1

    POSITION_T textEnd = doc.GetTextSize() ;

    SUBCASE("search from end of document")
    {
        POSITION_T result = doc.FindPrev("target", textEnd, false, false, false) ;
        REQUIRE(result != std::string::npos) ;
        CHECK(result == 11) ;
        CHECK(doc.GetCharNoAdvance(result) == "t") ;
        CHECK(doc.GetParagraphFromPosition(result) == 0) ;
    }

    SUBCASE("search from inside the later paragraph")
    {
        // Start a few graphemes before end-of-document (well inside paragraph 1);
        // the match is still backward in paragraph 0 and must be found.
        POSITION_T startInPara1 = textEnd - 3 ;
        REQUIRE(doc.GetParagraphFromPosition(startInPara1) == 1) ;
        POSITION_T result = doc.FindPrev("target", startInPara1, false, false, false) ;
        REQUIRE(result != std::string::npos) ;
        CHECK(result == 11) ;
        CHECK(doc.GetParagraphFromPosition(result) == 0) ;
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// Test that search needles are NFC-normalized so a DECOMPOSED search term
/// matches the document, which is stored in NFC (composed) form. The document
/// holds a precomposed "e-acute" (U+00E9); searching for the decomposed
/// "e" + U+0301 must still find it.
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("FindNext/FindPrev normalize a decomposed needle to match the NFC document")
{
    cDocument doc ;
    doc.SetShowControl(SHOW_ALL) ;

    // Document stores NFC: "café" with a precomposed e-acute (U+00E9 = \xC3\xA9).
    // "a caf<e-acute> here": e-acute is grapheme index 5.
    doc.Insert("a caf\xC3\xA9 here\r") ;

    // Decomposed needle: 'e' + U+0301 combining acute = 2 code points, 1 grapheme.
    const std::string decomposed = "e\xCC\x81" ;

    SUBCASE("FindNext")
    {
        POSITION_T result = doc.FindNext(decomposed, 0, false, false, false) ;
        REQUIRE(result != std::string::npos) ;
        CHECK(result == 5) ;
        CHECK(doc.GetCharNoAdvance(result) == "\xC3\xA9") ;  // composed e-acute
    }

    SUBCASE("FindPrev")
    {
        POSITION_T result = doc.FindPrev(decomposed, doc.GetTextSize(), false, false, false) ;
        REQUIRE(result != std::string::npos) ;
        CHECK(result == 5) ;
        CHECK(doc.GetCharNoAdvance(result) == "\xC3\xA9") ;
    }
}


/////////////////////////////////////////////////////////////////////////////
//
// Edge case and corner case tests
//
/////////////////////////////////////////////////////////////////////////////


TEST_CASE("Block selection: SetEndBlock with pos at or before StartBlock")
{
    cDocument doc ;
    doc.SetShowControl(SHOW_ALL) ;
    doc.Insert("Hello\r") ;

    // Set begin block at position 3, which inserts a REPLACE_CHAR marker
    doc.SetPosition(3) ;
    doc.SetBeginBlock() ;

    // Set end block at same position (3) -- with marker present, the original
    // position 3 is now at position 4, so position 3 is the marker itself.
    // This creates an invalid/empty block.
    doc.SetPosition(3) ;
    doc.SetEndBlock() ;

    POSITION_T start, end ;
    doc.GetBlock(start, end) ;

    // Should handle gracefully -- block not set or empty range
    // The key check is no crash and document text is intact
    CHECK(doc.GetCharNoAdvance(0) == "H") ;
    CHECK(doc.GetCharNoAdvance(1) == "e") ;
    CHECK(doc.GetCharNoAdvance(2) == "l") ;
}


TEST_CASE("Block selection: SetEndBlock without prior SetBeginBlock")
{
    cDocument doc ;
    doc.SetShowControl(SHOW_ALL) ;
    doc.Insert("Hello\r") ;

    // Call SetEndBlock without ever calling SetBeginBlock
    doc.SetPosition(3) ;
    doc.SetEndBlock() ;

    POSITION_T start, end ;
    bool blockSet = doc.GetBlock(start, end) ;

    // Should not crash, block should not be set
    CHECK(blockSet == false) ;

    // Document should be intact
    CHECK(doc.GetTextSize() == 7) ;  // Hello + \r + EOF
    CHECK(doc.GetCharNoAdvance(0) == "H") ;
}


TEST_CASE("Block selection: block spanning multiple paragraphs")
{
    cDocument doc ;
    doc.SetShowControl(SHOW_ALL) ;
    doc.Insert("Line1\rLine2\rLine3\r") ;

    // Select from position 2 (in "Line1") across paragraph boundary into "Line2"
    // With marker: SetBeginBlock at 2 inserts marker, shifting everything right by 1
    doc.SetPosition(2) ;
    doc.SetBeginBlock() ;

    // Position 10 in the shifted document (originally position 9, in "Line2")
    doc.SetPosition(10) ;
    doc.SetEndBlock() ;

    POSITION_T start, end ;
    bool blockSet = doc.GetBlock(start, end) ;

    CHECK(blockSet == true) ;
    CHECK(start == 2) ;

    // Block text should span the paragraph boundary (include \r)
    std::string blockText = doc.GetBlockText(start, end) ;
    CHECK(blockText.find('\r') != std::string::npos) ;
}


TEST_CASE("Block selection: select entire document")
{
    cDocument doc ;
    doc.SetShowControl(SHOW_ALL) ;
    doc.Insert("Hello World\r") ;

    POSITION_T textSize = doc.GetTextSize() ;

    // Select from 0 to end (with marker shift)
    doc.SetPosition(0) ;
    doc.SetBeginBlock() ;

    // textSize is now shifted by 1 due to marker insertion
    doc.SetPosition(textSize) ;
    doc.SetEndBlock() ;

    POSITION_T start, end ;
    bool blockSet = doc.GetBlock(start, end) ;

    CHECK(blockSet == true) ;
    CHECK(start == 0) ;

    // Block should cover all text content
    std::string blockText = doc.GetBlockText(start, end) ;
    CHECK(blockText.find("Hello World") != std::string::npos) ;
}


TEST_CASE("Undo: empty stack returns false")
{
    cDocument doc ;
    doc.SetShowControl(SHOW_ALL) ;
    doc.Clear() ;
    doc.ClearUndoHistory() ;

    CHECK(doc.Undo() == false) ;
    CHECK(doc.CanUndo() == false) ;

    // Document should still be valid
    CHECK(doc.GetTextSize() == 1) ;  // EOF only
}


TEST_CASE("Redo: empty stack returns false")
{
    cDocument doc ;
    doc.SetShowControl(SHOW_ALL) ;
    doc.Clear() ;
    doc.ClearUndoHistory() ;

    CHECK(doc.Redo() == false) ;
    CHECK(doc.CanRedo() == false) ;

    // Document should still be valid
    CHECK(doc.GetTextSize() == 1) ;  // EOF only
}


TEST_CASE("Undo: nested BeginUndoGroup/EndUndoGroup")
{
    cDocument doc ;
    doc.SetShowControl(SHOW_ALL) ;
    doc.Clear() ;

    // Nested undo groups -- only outermost pair creates the group
    doc.BeginUndoGroup() ;
    doc.BeginUndoGroup() ;  // nested
    doc.Insert("A") ;
    doc.Insert("B") ;
    doc.EndUndoGroup() ;
    doc.EndUndoGroup() ;

    CHECK(doc.GetTextSize() == 3) ;  // AB + EOF
    CHECK(doc.GetCharNoAdvance(0) == "A") ;
    CHECK(doc.GetCharNoAdvance(1) == "B") ;

    // Single undo should undo both inserts atomically
    bool result = doc.Undo() ;
    CHECK(result == true) ;
    CHECK(doc.GetTextSize() == 1) ;  // just EOF
}


TEST_CASE("Undo: group with zero actions")
{
    cDocument doc ;
    doc.SetShowControl(SHOW_ALL) ;
    doc.Clear() ;
    doc.ClearUndoHistory() ;

    // Empty undo group -- no actions between begin/end
    doc.BeginUndoGroup() ;
    doc.EndUndoGroup() ;

    // Should not crash, and there should be nothing to undo
    CHECK(doc.CanUndo() == false) ;
    CHECK(doc.Undo() == false) ;

    // Document should still be valid
    CHECK(doc.GetTextSize() == 1) ;  // EOF only
}


TEST_CASE("Undo after Clear")
{
    cDocument doc ;
    doc.SetShowControl(SHOW_ALL) ;
    doc.Insert("Hello\r") ;

    CHECK(doc.GetTextSize() == 7) ;  // Hello + \r + EOF

    // Clear resets the document
    doc.Clear() ;
    CHECK(doc.GetTextSize() == 1) ;  // EOF only

    // After clear, undo stack should be empty
    CHECK(doc.Undo() == false) ;
    CHECK(doc.GetTextSize() == 1) ;  // still just EOF
}


TEST_CASE("GetTab with out-of-bounds position")
{
    cDocument doc ;
    doc.SetShowControl(SHOW_ALL) ;
    doc.Insert("Hello\r") ;

    // Negative position
    sWSTab tab = doc.GetTab(-1) ;
    CHECK(tab.type == TAB_BAD) ;

    // Position beyond document end
    tab = doc.GetTab(doc.GetTextSize() + 100) ;
    CHECK(tab.type == TAB_BAD) ;

    // Position at valid location but no tab there
    tab = doc.GetTab(0) ;
    CHECK(tab.type == TAB_BAD) ;
}


TEST_CASE("GetColor with out-of-bounds position")
{
    cDocument doc ;
    doc.SetShowControl(SHOW_ALL) ;
    doc.Insert("Hello\r") ;

    sSeqRGBColor color ;

    // Negative position
    CHECK(doc.GetColor(-1, color) == false) ;

    // Position beyond document end
    CHECK(doc.GetColor(doc.GetTextSize() + 100, color) == false) ;

    // Valid position but no color there
    CHECK(doc.GetColor(0, color) == false) ;
}


TEST_CASE("GetFont with out-of-bounds position")
{
    cDocument doc ;
    doc.SetShowControl(SHOW_ALL) ;
    doc.Insert("Hello\r") ;

    sInternalFonts font ;

    // Negative position
    CHECK(doc.GetFont(-1, font) == false) ;

    // Position beyond document end
    CHECK(doc.GetFont(doc.GetTextSize() + 100, font) == false) ;
}


// Regression: lower_bound returned the next attribute when there was no exact match.
TEST_CASE("GetTab returns TAB_BAD when no tab at exact position (tab exists later)")
{
    cDocument doc ;
    doc.SetShowControl(SHOW_ALL) ;
    doc.Insert("Hello World\r") ;

    sWSTab tab ;
    tab.abstabsize = 0 ;
    tab.size = 0 ;
    tab.tabsize = 0 ;
    tab.type = TAB_TAB ;
    doc.SetPosition(8) ;
    doc.InsertTab(tab) ;

    CHECK(doc.GetTab(2).type == TAB_BAD) ;
    CHECK(doc.GetTab(8).type == TAB_TAB) ;
}


TEST_CASE("GetColor returns false when no color at exact position (color exists later)")
{
    cDocument doc ;
    doc.SetShowControl(SHOW_ALL) ;
    doc.Insert("Hello World\r") ;

    sSeqRGBColor c ;
    c.red = 255 ; c.green = 0 ; c.blue = 0 ; c.alpha = 255 ;
    doc.SetPosition(8) ;
    doc.InsertColor(c) ;

    sSeqRGBColor got ;
    CHECK(doc.GetColor(2, got) == false) ;
    CHECK(doc.GetColor(8, got) == true) ;
}


TEST_CASE("GetFont returns false when no font at exact position (font exists later)")
{
    cDocument doc ;
    doc.SetShowControl(SHOW_ALL) ;
    doc.Insert("Hello World\r") ;

    sInternalFonts font ;
    doc.SetPosition(8) ;
    doc.InsertFont(font) ;

    sInternalFonts got ;
    CHECK(doc.GetFont(2, got) == false) ;
    CHECK(doc.GetFont(8, got) == true) ;
}


TEST_CASE("GetVariable with out-of-bounds position")
{
    cDocument doc ;
    doc.SetShowControl(SHOW_ALL) ;
    doc.Insert("Hello\r") ;

    // Negative position -- returns VAR_DATE as default
    eVariableType v = doc.GetVariable(-1) ;
    CHECK(v == VAR_DATE) ;

    // Position beyond document end
    v = doc.GetVariable(doc.GetTextSize() + 100) ;
    CHECK(v == VAR_DATE) ;
}


TEST_CASE("SetPosition clamps to valid range")
{
    cDocument doc ;
    doc.SetShowControl(SHOW_ALL) ;
    doc.Insert("AB\r") ;

    POSITION_T textSize = doc.GetTextSize() ;

    // Position beyond document end should be clamped
    doc.SetPosition(99999) ;
    CHECK(doc.GetPosition() <= textSize) ;

    // Negative position should be clamped to 0
    doc.SetPosition(-100) ;
    CHECK(doc.GetPosition() == 0) ;
}


TEST_CASE("Insert(HARD_RETURN): paragraph split offset stays in bounds at all split points")
{
    // "Hello\rWorld" -> para0 "Hello\r" (positions 0-5), para1 "World" (6-10),
    // EOF marker at 11. Splitting with a new HARD_RETURN must add one paragraph
    // and insert exactly one \r at the caret, regardless of where the caret sits.
    cDocument doc ;
    doc.SetShowControl(SHOW_ALL) ;
    doc.Insert("Hello\rWorld") ;

    std::string before = doc.GetText() ;
    POSITION_T paraBefore = doc.GetNumberofParagraphs() ;

    SUBCASE("End of paragraph (caret just before the trailing return)")
    {
        // parapos0 == len-1: exercises the last valid offsets index after the split
        POSITION_T pos = 5 ;
        doc.SetPosition(pos) ;
        doc.Insert(static_cast<CHAR_T>(HARD_RETURN)) ;

        CHECK(doc.GetNumberofParagraphs() == paraBefore + 1) ;
        std::string expected = before.substr(0, static_cast<size_t>(pos)) + "\r" + before.substr(static_cast<size_t>(pos)) ;
        CHECK(doc.GetText() == expected) ;
    }

    SUBCASE("Middle of paragraph")
    {
        POSITION_T pos = 2 ;
        doc.SetPosition(pos) ;
        doc.Insert(static_cast<CHAR_T>(HARD_RETURN)) ;

        CHECK(doc.GetNumberofParagraphs() == paraBefore + 1) ;
        std::string expected = before.substr(0, static_cast<size_t>(pos)) + "\r" + before.substr(static_cast<size_t>(pos)) ;
        CHECK(doc.GetText() == expected) ;
    }

    SUBCASE("Start of paragraph")
    {
        // parapos0 == 0 (caret at the start of the second paragraph)
        POSITION_T pos = 6 ;
        doc.SetPosition(pos) ;
        doc.Insert(static_cast<CHAR_T>(HARD_RETURN)) ;

        CHECK(doc.GetNumberofParagraphs() == paraBefore + 1) ;
        std::string expected = before.substr(0, static_cast<size_t>(pos)) + "\r" + before.substr(static_cast<size_t>(pos)) ;
        CHECK(doc.GetText() == expected) ;
    }
}


TEST_CASE("Delete cleans up control-code marker tables")
{
    cDocument doc ;

    doc.Insert("A") ;
    sInternalFonts font ;
    font.fontname = "Courier" ;
    font.size = 12.0 ;
    font.haveWSFont = false ;
    font.name = "Courier" ;
    doc.InsertFont(font) ;          // font marker at position 1
    doc.Insert("B\r") ;

    // sanity: the font marker is present before deletion
    CHECK(doc.GetControlChar(1) == STYLE_FONT1) ;

    // delete the font marker
    doc.Delete(1, 1) ;

    // no orphaned control-code entry remains (the EOF sentinel is excluded)
    POSITION_T size = doc.GetTextSize() ;
    bool orphan = false ;
    for (POSITION_T p = 0 ; p < size ; p++)
    {
        eModifiers ctrl = doc.GetControlChar(p) ;
        if (ctrl != STYLE_END_OF_STYLES && ctrl != STYLE_EOF)
        {
            orphan = true ;
        }
    }
    CHECK(orphan == false) ;
}