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

/**
 * @class cRTFWriter
 *
 * @brief RTF file writer that serializes WordTsar documents to RTF format.
 *
 * Implements the cRTFWriter class, which walks the document paragraph by
 * paragraph, emitting RTF control words for formatting, paragraph properties,
 * page setup, and text content. Produces RTF output compatible with
 * Microsoft Word and other RTF readers.
 *
 * @section rtfwriter_structure Document Structure Output
 * - RTF header: version, character set, font table, color table
 * - Document properties: page size, margins, orientation
 * - Headers/footers: stored as sRTFHeaderFooter structures and emitted
 *   as RTF header/footer groups
 * - Body content: paragraphs with inline formatting and text
 *
 * @section rtfwriter_formatting Formatting Output
 * - Character formatting: \b (bold), \i (italic), \ul (underline),
 *   \strike (strikethrough), \super/\sub (super/subscript), \f (font),
 *   \fs (size), \cf (color)
 * - Paragraph formatting: \ql/\qc/\qr/\qj (alignment), \li/\ri (indents),
 *   \sl (line spacing), \tx (tab stops), \sb/\sa (space before/after)
 * - Page setup: \paperw/\paperh, \margl/\margr/\margt/\margb
 *
 * @section rtfwriter_encoding Character Encoding
 * - UTF-8 to RTF Unicode escape: multi-byte UTF-8 sequences are converted
 *   to \uN? escapes with a fallback character for legacy readers
 * - Code page 1252 optimization: Latin-1 characters (0xA0-0xFF) are emitted
 *   directly as bytes rather than Unicode escapes for smaller output
 * - Symbol font PUA remapping: standard Unicode codepoints for Greek letters
 *   and math symbols are converted back to PUA for Symbol font compatibility
 *
 * @section rtfwriter_dotcommands Dot Command Handling
 * WordStar dot commands are converted to their RTF equivalents where
 * possible (margins, page breaks, headers/footers) or preserved as
 * comments for round-trip fidelity.
 *
 * @author Gerald Brandt
 * @copyright GNU Affero General Public License v3.0
 *
 * @see cRTFWriter RTF writer class
 * @see sRTFHeaderFooter Header/footer storage structure
 * @see cEditorBase Editor providing document and layout access
 * @see cLayoutBase Layout engine for page geometry queries
 * @see cDocument Document model being serialized
 * @see cRTFFile RTF file handler coordinating write operations
 */

#include <iostream>
#include <cmath>
#include <cstring>
#include <algorithm>
#include <cctype>

#include "rtfwriter.h"
#include "src/core/editor/editorbase.h"
#include "src/core/include/version.h"
#include "src/core/include/utils.h"

#include "src/core/layout/layoutbase.h"
#include "src/core/codepage/cp437.h"
#include "src/core/codepage/cp1252.h"
#include "src/files/rtf/symbolpua.h"


//extern sExtendedChars gCodePage437[] ;
//extern int gExtendedSize ;
extern sSeqRGBColor gBaseWSColors[] ;


const int ON = 1 ;
const int OFF = 0 ;


//-----------------------------------------------------------------------
// Local string helpers (replacing QString methods)
//-----------------------------------------------------------------------

/////////////////////////////////////////////////////////////////////////////
///
/// @param  str - [in] the string to convert
///
/// @return std::string - lowercase copy
///
/// @brief
/// Returns a lowercase copy of the input string (ASCII only).
/////////////////////////////////////////////////////////////////////////////
static std::string toLowerStr(const std::string &str)
{
    std::string result = str ;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return std::tolower(c) ; }) ;
    return result ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  str - [in] the string to trim
///
/// @return std::string - trimmed and whitespace-collapsed copy
///
/// @brief
/// Trims leading/trailing whitespace and collapses internal runs of
/// whitespace to a single space (equivalent to QString::simplified).
/////////////////////////////////////////////////////////////////////////////
static std::string simplifyStr(const std::string &str)
{
    std::string result ;
    bool inSpace = true ;  // treat leading whitespace as space run

    for (unsigned char c : str)
    {
        if (std::isspace(c))
        {
            if (!inSpace)
            {
                result += ' ' ;
                inSpace = true ;
            }
        }
        else
        {
            result += static_cast<char>(c) ;
            inSpace = false ;
        }
    }

    // trim trailing space
    if (!result.empty() && result.back() == ' ')
    {
        result.pop_back() ;
    }

    return result ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  str - [in] the string to trim
///
/// @return std::string - trimmed copy
///
/// @brief
/// Trims leading and trailing whitespace (equivalent to QString::trimmed).
/////////////////////////////////////////////////////////////////////////////
static std::string trimStr(const std::string &str)
{
    size_t start = 0 ;
    while (start < str.size() && std::isspace(static_cast<unsigned char>(str[start])))
    {
        start++ ;
    }

    size_t end = str.size() ;
    while (end > start && std::isspace(static_cast<unsigned char>(str[end - 1])))
    {
        end-- ;
    }

    return str.substr(start, end - start) ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  haystack - [in] the string to search in
/// @param  needle   - [in] the substring to find (case insensitive)
///
/// @return bool - true if needle is found (case insensitive)
///
/// @brief
/// Case-insensitive string search.
/////////////////////////////////////////////////////////////////////////////
static bool containsCI(const std::string &haystack, const std::string &needle)
{
    std::string lowerHay = toLowerStr(haystack) ;
    std::string lowerNeedle = toLowerStr(needle) ;
    return lowerHay.find(lowerNeedle) != std::string::npos ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  p         - [in] pointer to start of UTF-8 sequence
/// @param  remaining - [in] number of bytes remaining in buffer
/// @param  cp        - [out] decoded Unicode code point
///
/// @return size_t - number of bytes consumed (1-4), or 1 on error (replacement)
///
/// @brief
/// Decodes a single UTF-8 code point from a byte sequence.
/////////////////////////////////////////////////////////////////////////////
static size_t decodeUtf8CodePoint(const char *p, size_t remaining, uint32_t &cp)
{
    unsigned char b = static_cast<unsigned char>(p[0]) ;

    // Single byte (ASCII)
    if (b < 0x80)
    {
        cp = b ;
        return 1 ;
    }

    // Two bytes
    if ((b & 0xE0) == 0xC0 && remaining >= 2)
    {
        cp = (static_cast<uint32_t>(b & 0x1F) << 6)
           | (static_cast<uint32_t>(static_cast<unsigned char>(p[1])) & 0x3F) ;
        return 2 ;
    }

    // Three bytes
    if ((b & 0xF0) == 0xE0 && remaining >= 3)
    {
        cp = (static_cast<uint32_t>(b & 0x0F) << 12)
           | ((static_cast<uint32_t>(static_cast<unsigned char>(p[1])) & 0x3F) << 6)
           | (static_cast<uint32_t>(static_cast<unsigned char>(p[2])) & 0x3F) ;
        return 3 ;
    }

    // Four bytes
    if ((b & 0xF8) == 0xF0 && remaining >= 4)
    {
        cp = (static_cast<uint32_t>(b & 0x07) << 18)
           | ((static_cast<uint32_t>(static_cast<unsigned char>(p[1])) & 0x3F) << 12)
           | ((static_cast<uint32_t>(static_cast<unsigned char>(p[2])) & 0x3F) << 6)
           | (static_cast<uint32_t>(static_cast<unsigned char>(p[3])) & 0x3F) ;
        return 4 ;
    }

    // Invalid -- return replacement character
    cp = 0xFFFD ;
    return 1 ;
}


// wxWidgets compatibility functions

/////////////////////////////////////////////////////////////////////////////
///
/// @param  str - [in] the string to search
/// @param  ch  - [in] the char to look for
///
/// @return std::string - the string after the last ch or the whole string if ch is not found
///
/// @brief
/// Gets all the characters after the last occurrence of ch
/////////////////////////////////////////////////////////////////////////////
static std::string AfterLast(const std::string &str, char ch)
{
    size_t index = str.rfind(ch) ;
    if (index != std::string::npos)
    {
        return str.substr(index + 1) ;
    }

    return str ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  str  - [in] the string to search
/// @param  ch   - [in] the character to look for
///
/// @return std::string - the string after the first ch or empty string if ch is not found
///
/// @brief
/// Gets all the characters after the first occurrence of ch.
/////////////////////////////////////////////////////////////////////////////
static std::string AfterFirst(const std::string &str, char ch)
{
    size_t index = str.find(ch) ;
    if (index != std::string::npos)
    {
        return str.substr(index + 1) ;
    }

    return "" ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  str     - [in] the string to search
/// @param  ch      - [in] the character to look for
/// @param  rest    - [out] Filled with the part of the string following the first occurrence of ch or cleared if it was not found.
///
/// @return std::string - part of the string before the first occurrence of ch or whole string if ch not found
///
/// @brief
/// Gets all characters before the first occurrence of ch.
/////////////////////////////////////////////////////////////////////////////
static std::string BeforeFirst(const std::string &str, char ch, std::string &rest)
{
    size_t index = str.find(ch) ;
    if (index != std::string::npos)
    {
        rest = str.substr(index + 1) ;
        return str.substr(0, index) ;
    }
    else
    {
        rest.clear() ;
        return str ;
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  text    - [in] the string to search
/// @param  prefix  - [in] the prefix to look for
/// @param  rest    - [out] Filled with the part of the string following the prefix, or unmodified
///
/// @return bool - true if the string starts with the prefix, else false
///
/// @brief
/// Tests if the string starts with the specified prefix.
/// If it does, puts the rest of the string (after the prefix) into rest.
/////////////////////////////////////////////////////////////////////////////
static bool StartsWith(const std::string &text, const std::string &prefix, std::string *rest = nullptr)
{
    if (text.size() >= prefix.size() && text.compare(0, prefix.size(), prefix) == 0)
    {
        if (rest != nullptr)
        {
            *rest = text.substr(prefix.size()) ;
        }
        return true ;
    }
    else
    {
        return false ;
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  editor - [in] pointer to the editor control
///
/// @return nothing
///
/// @brief
/// Constructs the RTF writer with default margin and formatting values.
/////////////////////////////////////////////////////////////////////////////
cRTFWriter::cRTFWriter(cEditorBase *editor)
{
    mEditor = editor ;
    mDocument = mEditor->GetDocument() ;

    mCommentCount = 1 ;
    mPaperWidth = 12240 ;           // 8.5" default
    mPaperHeight = 15840 ;          // 11" default
    mCurrentLeftMargin = 0 ;
    mCurrentParagraphMargin = 0 ;
    mCurrentPageOffset = 1440 ;     // 1" default
    mCurrentRightMarginPos = 9360 ; // 6.5" default
    mCurrentMargr = 1440 ;          // 1" default
    mCurrentTopMargin = 1440 ;      // 1" default
    mCurrentBottomMargin = 1440 ;   // 1" default

    mGroupCount = 0 ;
    mNewLine = false ;

    mDoIndex = false ;
    mDonefacingp = false ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Destructor. Warns if RTF group braces are unbalanced.
/////////////////////////////////////////////////////////////////////////////
cRTFWriter::~cRTFWriter(void)
{
    if (mGroupCount != 0)
    {
        std::cerr << "Error: Unmatched braces in RTF file" << std::endl ;
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  infilename - [in] the filename to write RTF to
///
/// @return bool - true if the file was written successfully
///
/// @brief
/// Main entry point. Opens the output file, writes the RTF header, font
/// table, color table, style sheet, generator, document body, and close.
/////////////////////////////////////////////////////////////////////////////
bool cRTFWriter::Start(std::string &infilename)
{
    bool retval = false ;

    paragraph.character.bold = false ;
    paragraph.character.italics = false ;
    paragraph.character.underline = false ;
    paragraph.character.subscript = false ;
    paragraph.character.superscript = false ;
    paragraph.character.strikethrough = false ;
    paragraph.character.smallcaps = false ;

    mFile.open(infilename, std::ios::out) ;
    if (mFile.is_open())
    {
        CreateRTFHeader() ;
        CreateFontTable() ;
        CreateColorTable() ;
        CreateStyleSheet() ;

        CreateGenerator() ;

        CreateRTF() ;


        CreateRTFClose() ;

        mFile.close() ;

        retval = true ;
    }

    return retval ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Writes the opening RTF header group with version, character set,
/// default font, and Unicode fallback character count.
/////////////////////////////////////////////////////////////////////////////
void cRTFWriter::CreateRTFHeader(void)
{
    StartGroup() ;
    ControlWord("rtf", 1) ;                                 // RTF version number (always 1)
    ControlWord("pc") ;                                     // character set (pc == codepage 437)
    ControlWord("deff", 0) ;                                // default font number
    ControlWord("uc", 1) ;                                   // 1 fallback character after each \uN
    NewLine() ;
}



/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Writes the closing RTF group brace.
/////////////////////////////////////////////////////////////////////////////
void cRTFWriter::CreateRTFClose(void)
{
    EndGroup() ;
}



/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Creates the RTF font table from the document's internal font list.
/// Uses cFontClassifier to determine RTF font family (froman, fswiss, etc.)
/// without depending on Qt.
/////////////////////////////////////////////////////////////////////////////
void cRTFWriter::CreateFontTable(void)
{
    std::vector<sInternalFonts> fontlist ;
    mDocument->GetFontList(fontlist) ;

    mFinalfont.clear() ;

    // build a map of fonts used in document
    for (auto &fontiter : fontlist)
    {
        std::string fonttype ;

        // Classify font using keyword-based font classifier
        sFontProperties props = mFontClassifier.classify(fontiter.fontname) ;

        // Map font properties to RTF font family
        if (props.math || props.symbol)
        {
            fonttype = "ftech " ;
        }
        else if (!props.proportional)
        {
            fonttype = "fmodern " ;
        }
        else
        {
            switch (props.style)
            {
                case STYLE_SERIF :
                    fonttype = "froman " ;
                    break ;

                case STYLE_SANS :
                    fonttype = "fswiss " ;
                    break ;

                case STYLE_SCRIPT :
                    fonttype = "fscript " ;
                    break ;

                default :
                    fonttype = "fnil " ;
                    break ;
            }
        }

        // Use the real font name directly from the internal font struct
        mFinalfont[fontiter.fontname] = fonttype ;
    }

    StartGroup() ;
    ControlWord("fonttbl") ;                    // start the font table
    NewLine() ;

    // add default font
    StartGroup() ;
    ControlWord("f", 0) ;               // add a font
    ControlWord("froman ") ;
    ControlText("Times New Roman") ;
    ControlText(";") ;
    EndGroup() ;

    int count = 1 ;
    for (auto &fiter : mFinalfont)
    {
        StartGroup() ;
        ControlWord("f", count) ;               // add a font
        ControlWord(fiter.second) ;
        ControlText(fiter.first) ;
        ControlText(";") ;
        EndGroup() ;

        count++ ;
    }

    EndGroup() ;

    ControlWord("f", 0) ;                       // specify the default font
    ControlWord("fs", 24) ;                     // specifiy default font size
    paragraph.fontindex = 0 ;
    paragraph.fontsize = 24 ;
    NewLine() ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Dumps the WordStar color table as an RTF color table.
/////////////////////////////////////////////////////////////////////////////
// dumps the wordstar color table out
void cRTFWriter::CreateColorTable(void)
{
    StartGroup() ;
    ControlWord("colortbl") ;
    ControlText(";") ;

    for (int i = 0; i < 16; i++)
    {
        NewLine() ;
        ControlWord("red", gBaseWSColors[i].red) ;
        ControlWord("green", gBaseWSColors[i].green) ;
        ControlWord("blue", gBaseWSColors[i].blue) ;
        ControlText(";") ;
    }
    EndGroup() ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Writes an empty RTF style sheet group (WordTsar doesn't do styles yet).
/////////////////////////////////////////////////////////////////////////////
void cRTFWriter::CreateStyleSheet(void)
{
    StartGroup() ;
    ControlWord("stylesheet") ;
    NewLine() ;

    // WordTsar doesn't do styles yet

    EndGroup() ;
}



/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Writes the RTF generator group identifying WordTsar and the platform.
/////////////////////////////////////////////////////////////////////////////
void cRTFWriter::CreateGenerator(void)
{
    StartGroup() ;
    ControlWord("*") ;
    ControlWord("generator WordTsar/") ;
    ControlText(FULLVERSION_STRING) ;
    ControlText("/") ;

#if defined(_WIN32)
    ControlText("windows") ;
#elif defined(__APPLE__)
    ControlText("macos") ;
#else
    ControlText("linux") ;
#endif

    EndGroup() ;
}




/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Pre-scan the document's leading dot commands to find initial margin
/// values. This allows the RTF header to be emitted with correct margins
/// without needing to query the layout engine (which stores WordStar-model
/// values, not RTF-model values).
///
/////////////////////////////////////////////////////////////////////////////
void cRTFWriter::PreScanMargins(void)
{
    // Start with WordStar defaults
    mCurrentPageOffset = 1440 ;         // .po 1"
    mCurrentLeftMargin = 0 ;            // .lm 0
    mCurrentParagraphMargin = 0 ;       // .pm 0
    mCurrentRightMarginPos = 9360 ;     // .rm 6.5"
    mCurrentTopMargin = 1440 ;          // .mt 1"
    mCurrentBottomMargin = 1440 ;       // .mb 1"

    // Scan leading dot commands for margin overrides
    size_t paras = mDocument->GetNumberofParagraphs() ;
    for (size_t loop = 0 ; loop < paras ; loop++)
    {
        std::string paraText = mDocument->GetParagraphText(loop) ;
        if (paraText.empty() || paraText[0] != '.')
        {
            break ;  // stop at first non-dot-command paragraph
        }

        std::string lowtext = toLowerStr(paraText) ;
        bool incdec ;

        // Check for .poo/.poe before .po (longer prefix first)
        if (lowtext.starts_with(".poo") || lowtext.starts_with(".poe"))
        {
            // odd/even page offset -- skip the 'o' or 'e' suffix
            double value = mDocument->GetValue(paraText.substr(4), incdec) ;
            char type = mDocument->GetType(paraText.substr(4)) ;
            if (!incdec)
            {
                mCurrentPageOffset = static_cast<int>(mDocument->ConvertToTwips(value, type)) ;
            }
        }
        else if (lowtext.starts_with(".po"))
        {
            double value = mDocument->GetValue(paraText.substr(3), incdec) ;
            char type = mDocument->GetType(paraText.substr(3)) ;
            if (!incdec)
            {
                mCurrentPageOffset = static_cast<int>(mDocument->ConvertToTwips(value, type)) ;
            }
        }
        else if (lowtext.starts_with(".lm"))
        {
            double value = mDocument->GetValue(paraText.substr(3), incdec) ;
            char type = mDocument->GetType(paraText.substr(3)) ;
            if (!incdec)
            {
                mCurrentLeftMargin = static_cast<int>(mDocument->ConvertToTwips(value, type)) ;
            }
        }
        else if (lowtext.starts_with(".rm"))
        {
            double value = mDocument->GetValue(paraText.substr(3), incdec) ;
            char type = mDocument->GetType(paraText.substr(3)) ;
            if (!incdec)
            {
                mCurrentRightMarginPos = static_cast<int>(mDocument->ConvertToTwips(value, type)) ;
            }
        }
        else if (lowtext.starts_with(".pm"))
        {
            double value = mDocument->GetValue(paraText.substr(3), incdec) ;
            char type = mDocument->GetType(paraText.substr(3)) ;
            if (!incdec)
            {
                mCurrentParagraphMargin = static_cast<int>(mDocument->ConvertToTwips(value, type)) ;
            }
        }
        else if (lowtext.starts_with(".mt"))
        {
            double value = mDocument->GetValue(paraText.substr(3), incdec) ;
            char type = mDocument->GetType(paraText.substr(3)) ;
            if (!incdec)
            {
                mCurrentTopMargin = static_cast<int>(mDocument->ConvertToTwips(value, type)) ;
            }
        }
        else if (lowtext.starts_with(".mb"))
        {
            double value = mDocument->GetValue(paraText.substr(3), incdec) ;
            char type = mDocument->GetType(paraText.substr(3)) ;
            if (!incdec)
            {
                mCurrentBottomMargin = static_cast<int>(mDocument->ConvertToTwips(value, type)) ;
            }
        }
    }

    // Compute initial RTF \margr from WordStar values
    mCurrentMargr = mPaperWidth - mCurrentPageOffset - mCurrentRightMarginPos ;
    if (mCurrentMargr < 0)
    {
        mCurrentMargr = 0 ;
    }
}



/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Creates the RTF document body. Gets paper dimensions from layout,
/// pre-scans document for initial margins, then processes all paragraphs.
///
/////////////////////////////////////////////////////////////////////////////
void cRTFWriter::CreateRTF(void)
{
    // Get paper dimensions from layout (only layout dependency for margins)
    mPaperWidth = static_cast<int>(mEditor->GetLayout()->GetPaperWidth()) ;
    mPaperHeight = static_cast<int>(mEditor->GetLayout()->GetPaperHeight()) ;

    ControlWord("paperw", mPaperWidth) ;
    ControlWord("paperh", mPaperHeight) ;

    // Pre-scan document for initial margins
    PreScanMargins() ;

    // Emit correct initial RTF margins
    // .po maps to \margl (page offset from paper edge)
    // .rm maps to \margr (computed: paperw - .po - .rm)
    // .mt maps to \margt (top page margin)
    // .mb maps to \margb (bottom page margin)
    ControlWord("margl", mCurrentPageOffset) ;
    ControlWord("margr", mCurrentMargr) ;
    ControlWord("margt", mCurrentTopMargin) ;
    ControlWord("margb", mCurrentBottomMargin) ;
    NewLine() ;

//    PAGE_T oldpage = 0 ;

    // go through the text a paragraph at a time
    size_t paras = mDocument->GetNumberofParagraphs() ;
    for (size_t loop = 0; loop < paras; loop++)
    {
        POSITION_T start, end ;
        std::string text = mDocument->GetParagraphText(loop) ;
        mDocument->GetParagraphStartandEnd(loop, start, end) ;
        mCurrentPosition = start ;

        // make sure we are not a comment paragraph
        if (text[0] != '.')
        {
            CreateText(text) ;
        }
        else
        {
            CreateDot(text) ;
        }
    }
}



/////////////////////////////////////////////////////////////////////////////
///
/// @param  text - [in] the paragraph text to write as RTF
///
/// @return nothing
///
/// @brief
/// Writes a text paragraph to RTF, processing control codes, special
/// characters, and Unicode code points.
/////////////////////////////////////////////////////////////////////////////
void cRTFWriter::CreateText(std::string &text)
{
    // Win-1252 codepage for ANSI fallback characters (created once, not per-char)
    cCodePageWin1252 cp1252 ;

     // go through a character at a time (byte-level iteration with UTF-8 decoding)
    size_t pos = 0 ;
    while (pos < text.size())
    {
        unsigned char byte = static_cast<unsigned char>(text[pos]) ;

        if (byte == static_cast<unsigned char>(REPLACE_CHAR) ||
            byte == static_cast<unsigned char>(SAVE_CHAR))
        {
            pos++ ;
            mCurrentPosition++ ;
            continue ;
        }
//        if(text.at(pos) < STYLE_END_OF_STYLES)
        if (byte == static_cast<unsigned char>(MARKER_CHAR))
        {
            unsigned char ch = static_cast<unsigned char>(mDocument->GetControlChar(mCurrentPosition)) ;
            CreateModifiers(ch) ;
//            CreateModifiers(text.at(pos)) ;
            ControlSpace() ;
            pos++ ;
            mCurrentPosition++ ;
        }
        else if (byte == 0xC2 && pos + 1 < text.size() && static_cast<unsigned char>(text[pos + 1]) == 0xA0)
        {
            // Non-breaking space (Unicode U+00A0 encoded as UTF-8: C2 A0) emits RTF \~ control symbol
            mFile << "\\~" ;
            pos += 2 ;
            mCurrentPosition++ ;
        }
        else if (byte == '\\')
        {
            // Escape backslash -- RTF special character
            mFile << "\\\\" ;
            pos++ ;
            mCurrentPosition++ ;
        }
        else if (byte == '{')
        {
            // Escape open brace -- RTF special character
            mFile << "\\{" ;
            pos++ ;
            mCurrentPosition++ ;
        }
        else if (byte == '}')
        {
            // Escape close brace -- RTF special character
            mFile << "\\}" ;
            pos++ ;
            mCurrentPosition++ ;
        }
        else
        {
            if (pos == 0)
            {
                ControlSpace() ;
            }

            if (byte >= 32 && byte <= 126)
            {
                // Printable ASCII: write directly
                char w = static_cast<char>(byte) ;
                mFile.write(&w, 1) ;
                pos++ ;
                mCurrentPosition++ ;
            }
            else
            {
                // Decode UTF-8 to get the Unicode code point
                uint32_t code = 0 ;
                size_t byteCount = decodeUtf8CodePoint(text.data() + pos, text.size() - pos, code) ;

                // Emit \uN with signed 16-bit encoding per RTF spec
                // Values > 32767 are encoded as value - 65536
                long signedValue = static_cast<long>(code) ;
                if (signedValue > 32767)
                {
                    signedValue -= 65536 ;
                }
                ControlWord("u", signedValue) ;

                // Best ANSI (Win-1252) fallback for old readers
                unsigned char ansiByte = cp1252.toChar(static_cast<unsigned long>(code)) ;
                if (ansiByte != 0)
                {
                    // Win-1252 hex escape (e.g., \'95 for bullet, \'e9 for e-acute)
                    char buf[5] ;
                    snprintf(buf, sizeof(buf), "\\'%02x", ansiByte) ;
                    mFile.write(buf, 4) ;
                }
                else
                {
                    // No Win-1252 mapping (Greek, CJK, etc.): use ?
                    ControlText("?") ;
                }

                pos += byteCount ;
                mCurrentPosition++ ;
            }
        }
    }
    NewLine() ;
    ControlWord("par") ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  ch - [in] the control character code (style modifier)
///
/// @return nothing
///
/// @brief
/// Writes RTF control words for style modifiers (bold, italic, underline,
/// subscript, superscript, strikethrough, tab, font, color, variables).
/////////////////////////////////////////////////////////////////////////////
void cRTFWriter::CreateModifiers(unsigned char ch)
{
    switch (ch)
    {
        case STYLE_BOLD :
            if (paragraph.character.bold == false)
            {
                ControlWord("b", ON) ;
//                ControlSpace() ;
                paragraph.character.bold = true ;
            }
            else
            {
                ControlWord("b", OFF) ;
//                ControlSpace() ;
                paragraph.character.bold = false ;
            }
            break ;

        case STYLE_ITALICS :
            if (paragraph.character.italics == false)
            {
                ControlWord("i", ON) ;
//                ControlSpace() ;
                paragraph.character.italics = true ;
            }
            else
            {
                ControlWord("i", OFF) ;
//                ControlSpace() ;
                paragraph.character.italics = false ;
            }
            break ;

        case STYLE_UNDERLINE :
            if (paragraph.character.underline == false)
            {
                ControlWord("ul", ON) ;
//                ControlSpace() ;
                paragraph.character.underline = true ;
            }
            else
            {
                ControlWord("ul", OFF) ;
//                ControlSpace() ;
                paragraph.character.underline = false ;
            }
            break ;

        case STYLE_SUBSCRIPT :
            if (paragraph.character.subscript == false)
            {
                ControlWord("sub") ;
//                ControlSpace() ;
                paragraph.character.subscript = true ;
            }
            else
            {
                ControlWord("nosupersub") ;
//                ControlSpace() ;
                paragraph.character.subscript = false ;
            }
            break ;

        case STYLE_SUPERSCRIPT :
            if (paragraph.character.superscript == false)
            {
                ControlWord("super") ;
//                ControlSpace() ;
                paragraph.character.superscript = true ;
            }
            else
            {
                ControlWord("nosupersub") ;
//                ControlSpace() ;
                paragraph.character.superscript = false ;
            }
            break ;

        case STYLE_STRIKETHROUGH :
            if (paragraph.character.strikethrough == false)
            {
                ControlWord("strike", ON) ;
//                ControlSpace() ;
                paragraph.character.strikethrough = true ;
            }
            else
            {
                ControlWord("strike", OFF) ;
//                ControlSpace() ;
                paragraph.character.strikethrough = false ;
            }
            break ;

        case STYLE_TAB :
            ControlWord("tab") ;             /// @todo - style of TAB!
            break ;

        case STYLE_FONT1 :
            CreateFont1() ;
            break ;

        case STYLE_INTERNAL_COLOR :
            CreateColor() ;
            break ;

        case STYLE_VARIABLE :
        {
            // Emit RTF field code for document variables
            eVariableType varType = mDocument->GetVariable(mCurrentPosition) ;
            switch (varType)
            {
                case VAR_PAGE_NUMBER :
                {
                    StartGroup() ;
                    ControlWord("field") ;
                    StartGroup() ;
                    ControlWord("*") ;
                    ControlWord("fldinst PAGE") ;
                    EndGroup() ;
                    StartGroup() ;
                    ControlWord("fldrslt #") ;
                    EndGroup() ;
                    EndGroup() ;
                    break ;
                }
                case VAR_DATE :
                {
                    StartGroup() ;
                    ControlWord("field") ;
                    StartGroup() ;
                    ControlWord("*") ;
                    ControlWord("fldinst DATE") ;
                    EndGroup() ;
                    StartGroup() ;
                    ControlWord("fldrslt date") ;
                    EndGroup() ;
                    EndGroup() ;
                    break ;
                }
                case VAR_TIME :
                {
                    StartGroup() ;
                    ControlWord("field") ;
                    StartGroup() ;
                    ControlWord("*") ;
                    ControlWord("fldinst TIME") ;
                    EndGroup() ;
                    StartGroup() ;
                    ControlWord("fldrslt time") ;
                    EndGroup() ;
                    EndGroup() ;
                    break ;
                }
                case VAR_FILENAME :
                {
                    StartGroup() ;
                    ControlWord("field") ;
                    StartGroup() ;
                    ControlWord("*") ;
                    ControlWord("fldinst FILENAME") ;
                    EndGroup() ;
                    StartGroup() ;
                    ControlWord("fldrslt file") ;
                    EndGroup() ;
                    EndGroup() ;
                    break ;
                }
                default :
                {
                    break ;
                }
            }
            break ;
        }
    }
}



/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Writes an RTF font change control word by looking up the font index
/// in mFinalfont. Uses cFontClassifier for font name resolution.
/////////////////////////////////////////////////////////////////////////////
void cRTFWriter::CreateFont1(void)
{
    sInternalFonts internalfont ;
    if (mDocument->GetFont(mCurrentPosition, internalfont) == true)
    {
        // Use the real font name directly (no Qt font resolution needed)
        std::string resolvedName = internalfont.fontname ;

        // Check if this is the default font (index 0: Times New Roman)
        // Font 0 is hardcoded in CreateFontTable and not in mFinalfont,
        // so the loop below starting at fontnum=1 would never match it
        if (resolvedName == "Times New Roman")
        {
            ControlWord("f", 0) ;
            paragraph.fontindex = 0 ;
        }
        else
        {
            size_t fontnum = 1 ;
            for (auto &ffiter : mFinalfont)
            {
                if (resolvedName == ffiter.first)
                {
                    ControlWord("f", fontnum) ;
                    paragraph.fontindex = fontnum ;
                    break ;
                }
                fontnum++ ;
            }
        }

        int fontSize = static_cast<int>(internalfont.size) * 2 ;
        ControlWord("fs", fontSize) ;
        paragraph.fontsize = fontSize ;
    }
}



/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Writes an RTF color change control word for the current position.
/////////////////////////////////////////////////////////////////////////////
void cRTFWriter::CreateColor(void)
{
    sSeqRGBColor rgbColor ;

    mDocument->GetColor(mCurrentPosition, rgbColor) ;

    // Convert RGB to nearest WordStar palette index for RTF \cf reference
    int colorIndex = 0 ;
    if (!rgbColor.IsDefault())
    {
        int bestDistance = INT_MAX ;
        for (int i = 0; i < 16; ++i)
        {
            int dr = rgbColor.red - gBaseWSColors[i].red ;
            int dg = rgbColor.green - gBaseWSColors[i].green ;
            int db = rgbColor.blue - gBaseWSColors[i].blue ;
            int distance = dr * dr + dg * dg + db * db ;
            if (distance < bestDistance)
            {
                bestDistance = distance ;
                colorIndex = i ;
            }
        }
    }

    ControlWord("cf", colorIndex + 1) ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  index     - [in] header/footer index (1-based, decremented internally)
/// @param  which     - [in] which header/footer type (HEADER_BOTH, FOOTER_EVEN, etc.)
/// @param  rest      - [in/out] the text content of the header/footer
/// @param  prefixLen - [in] length of the dot command prefix for position calculation
///
/// @return nothing
///
/// @brief
/// Processes a header or footer definition dot command and emits the
/// appropriate RTF header/footer group.
/////////////////////////////////////////////////////////////////////////////
void cRTFWriter::CreateHeadersFooters(int index, enum eHeaderFooter which, std::string &rest, int prefixLen)
{
    // Note: removed unused 'sHeaderFooter current' variable (dead code from refactoring)
    index -- ;

    // Calculate document position where the text content starts (after the dot prefix)
    POSITION_T contentDocPos = mCurrentPosition + static_cast<POSITION_T>(prefixLen) ;

    // Strip trailing paragraph terminator (\n) from header text
    // GetParagraphText() includes \n which would be written as a raw byte in RTF
    if (!rest.empty() && rest.back() == '\n')
    {
        rest = rest.substr(0, rest.size() - 1) ;
    }

    // Detect alignment TAB at start of header/footer text (TAB_CENTER or TAB_RIGHT)
    // These are inserted by the RTF reader for \qc / \qr alignment
    if (!rest.empty() && static_cast<unsigned char>(rest[0]) == static_cast<unsigned char>(MARKER_CHAR))
    {
        eModifiers controlType = mDocument->GetControlChar(contentDocPos) ;
        if (controlType == STYLE_TAB)
        {
            sWSTab tabInfo = mDocument->GetTab(contentDocPos) ;
            if (tabInfo.type == TAB_CENTER)
            {
                paragraph.align = ALIGNCENTER ;
            }
            else if (tabInfo.type == TAB_RIGHT || tabInfo.type == TAB_RIGHT1)
            {
                paragraph.align = ALIGNRIGHT ;
            }
            // Strip alignment tab MARKER_CHAR only -- font/color markers must stay
            // for CreateText to process them into RTF control words
            rest = rest.substr(1) ;
            contentDocPos++ ;
        }
    }

    sRTFParaFormat para = paragraph ;

    switch (which)
    {
        case HEADER_BOTH :
            if (mHeaders[index].text != rest)
            {
                mHeaders[index].text = rest ;
                mHeaders[index].paraformat = paragraph ;
                mHeaders[index].docStartPos = contentDocPos ;

                StartGroup() ;
                ControlWord("header") ;

                for (int loop = 0 ; loop < MAX_HEADER_FOOTER; loop++)
                {
                    if (mHeaders[loop].text.length() != 0)
                    {
                        ControlWord("pard") ;
                        ControlWord("plain") ;
                        paragraph = mHeaders[loop].paraformat ;
                        ParagraphFormat() ;
                        POSITION_T savedPos = mCurrentPosition ;
                        mCurrentPosition = mHeaders[loop].docStartPos ;
                        CreateText(mHeaders[loop].text) ;
                        mCurrentPosition = savedPos ;
                    }
                }

                EndGroup() ;
            }
            break ;

        case HEADER_EVEN :
            if (mHeadersEven[index].text != rest)
            {
                mHeadersEven[index].text = rest ;
                mHeadersEven[index].paraformat = paragraph ;
                mHeadersEven[index].docStartPos = contentDocPos ;

                StartGroup() ;
                ControlWord("headerl") ;    // headerl = left = even pages

                for (int loop = 0 ; loop < MAX_HEADER_FOOTER; loop++)
                {
                    if (mHeadersEven[loop].text.length() != 0)
                    {
                        ControlWord("pard") ;
                        ControlWord("plain") ;
                        paragraph = mHeadersEven[loop].paraformat ;
                        ParagraphFormat() ;
                        POSITION_T savedPos = mCurrentPosition ;
                        mCurrentPosition = mHeadersEven[loop].docStartPos ;
                        CreateText(mHeadersEven[loop].text) ;
                        mCurrentPosition = savedPos ;
                    }
                }

                EndGroup() ;
            }
            break ;

        case HEADER_ODD :
            if (mHeadersOdd[index].text != rest)
            {
                mHeadersOdd[index].text = rest ;
                mHeadersOdd[index].paraformat = paragraph ;
                mHeadersOdd[index].docStartPos = contentDocPos ;

                StartGroup() ;
                ControlWord("headerr") ;    // headerr = right = odd pages

                for (int loop = 0 ; loop < MAX_HEADER_FOOTER; loop++)
                {
                    if (mHeadersOdd[loop].text.length() != 0)
                    {
                        ControlWord("pard") ;
                        ControlWord("plain") ;
                        paragraph = mHeadersOdd[loop].paraformat ;
                        ParagraphFormat() ;
                        POSITION_T savedPos = mCurrentPosition ;
                        mCurrentPosition = mHeadersOdd[loop].docStartPos ;
                        CreateText(mHeadersOdd[loop].text) ;
                        mCurrentPosition = savedPos ;
                    }
                }

                EndGroup() ;
            }
            break ;

        case FOOTER_BOTH :
            if (mFooters[index].text != rest)
            {
                mFooters[index].text = rest ;
                mFooters[index].paraformat = paragraph ;
                mFooters[index].docStartPos = contentDocPos ;

                StartGroup() ;
                ControlWord("footer") ;

                for (int loop = 0 ; loop < MAX_HEADER_FOOTER; loop++)
                {
                    if (mFooters[loop].text.length() != 0)
                    {
                        ControlWord("pard") ;
                        ControlWord("plain") ;
                        paragraph = mFooters[loop].paraformat ;
                        ParagraphFormat() ;
                        POSITION_T savedPos = mCurrentPosition ;
                        mCurrentPosition = mFooters[loop].docStartPos ;
                        CreateText(mFooters[loop].text) ;
                        mCurrentPosition = savedPos ;
                    }
                }

                EndGroup() ;
            }
            break ;

        case FOOTER_EVEN :
            if (mFootersEven[index].text != rest)
            {
                mFootersEven[index].text = rest ;
                mFootersEven[index].paraformat = paragraph ;
                mFootersEven[index].docStartPos = contentDocPos ;

                StartGroup() ;
                ControlWord("footerl") ;

                for (int loop = 0 ; loop < MAX_HEADER_FOOTER; loop++)
                {
                    if (mFootersEven[loop].text.length() != 0)
                    {
                        ControlWord("pard") ;
                        ControlWord("plain") ;
                        paragraph = mFootersEven[loop].paraformat ;
                        ParagraphFormat() ;
                        POSITION_T savedPos = mCurrentPosition ;
                        mCurrentPosition = mFootersEven[loop].docStartPos ;
                        CreateText(mFootersEven[loop].text) ;
                        mCurrentPosition = savedPos ;
                    }
                }

                EndGroup() ;
            }
            break ;

        case FOOTER_ODD :
            if (mFootersOdd[index].text != rest)
            {
                mFootersOdd[index].text = rest ;
                mFootersOdd[index].paraformat = paragraph ;
                mFootersOdd[index].docStartPos = contentDocPos ;

                StartGroup() ;
                ControlWord("footerr") ;

                for (int loop = 0 ; loop < MAX_HEADER_FOOTER; loop++)
                {
                    if (mFootersOdd[loop].text.length() != 0)
                    {
                        ControlWord("pard") ;
                        ControlWord("plain") ;
                        paragraph = mFootersOdd[loop].paraformat ;
                        ParagraphFormat() ;
                        POSITION_T savedPos = mCurrentPosition ;
                        mCurrentPosition = mFootersOdd[loop].docStartPos ;
                        CreateText(mFootersOdd[loop].text) ;
                        mCurrentPosition = savedPos ;
                    }
                }

                EndGroup() ;
            }
            break ;
    }

    paragraph = para ;
}




/////////////////////////////////////////////////////////////////////////////
///
/// @param  text - [in] the dot command paragraph text
///
/// @return nothing
///
/// @brief
/// Processes a dot command paragraph and emits the appropriate RTF
/// control words (margins, headers, footers, tabs, page breaks, etc.).
/////////////////////////////////////////////////////////////////////////////
void cRTFWriter::CreateDot(std::string &text)
{
    std::string rest ;
    std::string lowtext = toLowerStr(text) ;
    bool incdec ;

    char check = lowtext[1] ;

    // Track file position to detect unhandled dot commands
    std::streampos posBeforeSwitch = mFile.tellp() ;

    switch (check)
    {
        case 'c' :
            // columns
            if (StartsWith(lowtext, ".co", &rest)) //    lowtext.startsWith(".co"))
            {
                // we start column sections with a new section, so if columns start in middle of page, it works
                ControlWord("sect") ;
                ControlWord("sectd") ;
                ControlWord("sbknone") ;
                int value = static_cast<int>(mDocument->GetValue(text.substr(3), incdec));

                // Always emit \cols (including \cols 1 to explicitly reset)
                ControlWord("cols", value) ;

                if (value != 1)
                {
                    // Check for column spacing value after comma
                    std::string colrest = text.substr(3) ;
                    colrest = AfterLast(colrest, ' ') ;
                    colrest = AfterLast(colrest, ',') ;
                    double w = mDocument->GetValue(colrest, incdec) ;
                    char t = mDocument->GetType(colrest) ;

                    if (std::abs(w) >= 1e-6)
                    {
                        COORD_T twips = mDocument->ConvertToTwips(w, t) ;
                        ControlWord("colsx", static_cast<int>(twips)) ;
                    }
                    ControlWord("ri", 0) ;
                }
                NewLine() ;
            }
            break ;

        case 'f' :
            // footer
            if (StartsWith(lowtext, ".f1 ", &rest) || StartsWith(lowtext, ".fo ", &rest) ||
               StartsWith(lowtext, ".f2 ", &rest) || StartsWith(lowtext, ".f3 ", &rest) ||
               StartsWith(lowtext, ".f4 ", &rest) || StartsWith(lowtext, ".f5 ", &rest))
            {
                rest = AfterFirst(text, ' ') ;
                int index ;
                index = lowtext[2] - '0' ;
                if (index >= MAX_HEADER_FOOTER)
                {
                    index = 0 ;
                }
                CreateHeadersFooters(index, FOOTER_BOTH, rest, static_cast<int>(text.find(' ')) + 1) ;
            }
            else if (StartsWith(lowtext, ".f1e ", &rest) || StartsWith(lowtext, ".foe ", &rest) ||
               StartsWith(lowtext, ".f2e ", &rest) || StartsWith(lowtext, ".f3e ", &rest) ||
               StartsWith(lowtext, ".f4e ", &rest) || StartsWith(lowtext, ".f5e ", &rest))
            {
                rest = AfterFirst(text, ' ') ;
                int index ;
                index = lowtext[2] - '0' ;
                if (index >= MAX_HEADER_FOOTER)
                {
                    index = 0 ;
                }

                if (mDonefacingp == false)
                {
                    mDonefacingp = true ;
                    ControlWord("facingp") ;
                }
                CreateHeadersFooters(index, FOOTER_EVEN, rest, static_cast<int>(text.find(' ')) + 1) ;
            }
            else if (StartsWith(lowtext, ".f1o ", &rest) || StartsWith(lowtext, ".foo ", &rest) ||
               StartsWith(lowtext, ".f2o ", &rest) || StartsWith(lowtext, ".f3o ", &rest) ||
               StartsWith(lowtext, ".f4o ", &rest) || StartsWith(lowtext, ".f5o ", &rest))
            {
                rest = AfterFirst(text, ' ') ;
                int index ;
                index = lowtext[2] - '0' ;
                if (index >= MAX_HEADER_FOOTER)
                {
                    index = 0 ;
                }
                if (mDonefacingp == false)
                {
                    mDonefacingp = true ;
                    ControlWord("facingp") ;
                }
                CreateHeadersFooters(index, FOOTER_ODD, rest, static_cast<int>(text.find(' ')) + 1) ;
            }

            // footer margin
            else if (StartsWith(lowtext, ".fm", &rest))
            {
                double value = mDocument->GetValue(text.substr(3), incdec);
                char type = mDocument->GetType(text.substr(3));
                int twips = static_cast<int>(mDocument->ConvertToTwips(value, type)) ;
                ControlWord("footery", twips) ;
            }
            break ;

        case 'h' :
            // header
            if (StartsWith(lowtext, ".h1e ", &rest) || StartsWith(lowtext, ".hoe ", &rest) ||
               StartsWith(lowtext, ".h2e ", &rest) || StartsWith(lowtext, ".h3e ", &rest) ||
               StartsWith(lowtext, ".h4e ", &rest) || StartsWith(lowtext, ".h5e ", &rest))
            {
                rest = AfterFirst(text, ' ') ;
                int index ;
                index = lowtext[2] - '0' ;
                if (index >= MAX_HEADER_FOOTER)
                {
                    index = 0 ;
                }
                if (mDonefacingp == false)
                {
                    mDonefacingp = true ;
                    ControlWord("facingp") ;
                }
                CreateHeadersFooters(index, HEADER_EVEN, rest, static_cast<int>(text.find(' ')) + 1) ;
            }
            else if (StartsWith(lowtext, ".h1o ", &rest) || StartsWith(lowtext, ".hoo ", &rest) ||
               StartsWith(lowtext, ".h2o ", &rest) || StartsWith(lowtext, ".h3o ", &rest) ||
               StartsWith(lowtext, ".h4o ", &rest) || StartsWith(lowtext, ".h5o ", &rest))
            {
                rest = AfterFirst(text, ' ') ;
                int index ;
                index = lowtext[2] - '0' ;
                if (index >= MAX_HEADER_FOOTER)
                {
                    index = 0 ;
                }
                if (mDonefacingp == false)
                {
                    mDonefacingp = true ;
                    ControlWord("facingp") ;
                }
                CreateHeadersFooters(index, HEADER_ODD, rest, static_cast<int>(text.find(' ')) + 1) ;
            }
            else if (StartsWith(lowtext, ".h1 ", &rest) || StartsWith(lowtext, ".ho ", &rest) ||
               StartsWith(lowtext, ".h2 ", &rest) || StartsWith(lowtext, ".h3 ", &rest) ||
               StartsWith(lowtext, ".h4 ", &rest) || StartsWith(lowtext, ".h5 ", &rest))
            {
                rest = AfterFirst(text, ' ') ;
                int index ;
                index = lowtext[2] - '0' ;
                if (index >= MAX_HEADER_FOOTER)
                {
                    index = 0 ;
                }
                CreateHeadersFooters(index, HEADER_BOTH, rest, static_cast<int>(text.find(' ')) + 1) ;
            }

            // header margin
            else if (StartsWith(lowtext, ".hm", &rest))
            {
                double value = mDocument->GetValue(text.substr(3), incdec);
                char type = mDocument->GetType(text.substr(3));
                int twips = static_cast<int>(mDocument->ConvertToTwips(value, type)) ;
                ControlWord("headery", twips) ;
            }

            // hyphenate on/off
            else if (StartsWith(lowtext, ".hy", &rest))
            {
                if (rest.find("on") != std::string::npos)
                {
                    ControlWord("hyphauto", 1) ;
                }
                else
                {
                    ControlWord("hyphauto", 0) ;
                }
            }
            break ;

        case 'i' :
            // index
            if (StartsWith(lowtext, ".ix", &rest))
            {
                StartGroup() ;
//                ControlWord("xe") ;
//                ControlWord("v") ;
                IndexText(text) ;
                EndGroup() ;
                mDoIndex = true ;
            }
            break ;

        case 'k' :
            // kerning
            if (StartsWith(lowtext, ".kr", &rest))
            {
                if (rest.find("off") != std::string::npos)
                {
                    ControlWord("kerning", 0) ;
                }
                else
                {
                    ControlWord("kerning", 16) ;    // turn on kerning for font 8 points or large
                }
            }
            break ;

        case 'l' :
            // line numbering
            if (StartsWith(lowtext, ".l#", &rest))
            {
                ControlWord("linemod", 1) ;
                ControlWord("linex", 0) ;

                std::string lnrest = lowtext.substr(3) ;
                lnrest = simplifyStr(lnrest) ;
                if (!lnrest.empty() && lnrest[0] == 'p')
                {
                    ControlWord("lineppage") ;
                }
//                ControlWord("linecont") ;
            }

            // line height (.lh maps to \sl with negative value for exact height)
            else if (StartsWith(lowtext, ".lh", &rest))
            {
                double value = mDocument->GetValue(text.substr(3), incdec) ;
                char type = mDocument->GetType(text.substr(3)) ;
                int twips = static_cast<int>(mDocument->ConvertToTwips(value, type)) ;
                // Negative \sl = exact line height in twips
                ControlWord("sl", -twips) ;
            }

            // line spacing
            else if (StartsWith(lowtext, ".ls", &rest))
            {
                double space = 0.0 ;
                try
                {
                    space = std::stod(rest) ;
                }
                catch (...)
                {
                    space = 0.0 ;
                }

                if (std::abs(space) < 1e-6)
                {
                    ControlWord("sl", 0) ;
                }
                else
                {
                    long value = static_cast<long>(space * 240.0) ;
                    ControlWord("sl", value) ;
                    ControlWord("slmult", 1) ;
                }
                NewLine() ;
            }

            // left margin
            else if (StartsWith(lowtext, ".lm", &rest))
            {
                double value = mDocument->GetValue(text.substr(3), incdec);
                char type = mDocument->GetType(text.substr(3));

                int newlm;
                newlm = static_cast<int>(mDocument->ConvertToTwips(value, type));

                ControlWord("li", newlm) ;
                mCurrentLeftMargin = newlm ;

                // Re-emit \fi since it's relative to \li
                int fi = mCurrentParagraphMargin - mCurrentLeftMargin ;
                ControlWord("fi", fi) ;
            }
            break ;

        case 'm' :
            // bottom margin -- section-level property, needs section break
            if (StartsWith(lowtext, ".mb", &rest))
            {
                double value = mDocument->GetValue(text.substr(3), incdec);
                char type = mDocument->GetType(text.substr(3));

                mCurrentBottomMargin = mDocument->ConvertToTwips(value, type);

                // Section break with all four margins re-specified
                ControlWord("sect") ;
                ControlWord("sectd") ;
                ControlWord("sbknone") ;
                ControlWord("margl", mCurrentPageOffset) ;
                ControlWord("margr", mCurrentMargr) ;
                ControlWord("margt", mCurrentTopMargin) ;
                ControlWord("margb", mCurrentBottomMargin) ;
                NewLine() ;
            }

            // top margin -- section-level property, needs section break
            else if (StartsWith(lowtext, ".mt", &rest))
            {
                double value = mDocument->GetValue(text.substr(3), incdec);
                char type = mDocument->GetType(text.substr(3));

                mCurrentTopMargin = mDocument->ConvertToTwips(value, type);

                // Section break with all four margins re-specified
                ControlWord("sect") ;
                ControlWord("sectd") ;
                ControlWord("sbknone") ;
                ControlWord("margl", mCurrentPageOffset) ;
                ControlWord("margr", mCurrentMargr) ;
                ControlWord("margt", mCurrentTopMargin) ;
                ControlWord("margb", mCurrentBottomMargin) ;
                NewLine() ;
            }
            break ;

        case 'o' :
            // justification
            if (StartsWith(lowtext, ".oj", &rest))
            {
                if (StartsWith(rest, "c"))
                {
                    ControlWord("qc") ;
                    paragraph.align = ALIGNCENTER ;
                }
                else if (StartsWith(rest, "r"))
                {
                    ControlWord("qr") ;
                    paragraph.align = ALIGNRIGHT ;
                }
                else if (StartsWith(rest, "j"))
                {
                    ControlWord("qj") ;
                    paragraph.align = ALIGNJUSTIFY ;
                }
                else
                {
                    ControlWord("ql") ;
                    paragraph.align = ALIGNLEFT ;
                }
            }

            // center
            else if (StartsWith(lowtext, ".oc", &rest))
            {
                // get rid of extraneous from left and right
                rest = simplifyStr(rest) ;
                if (StartsWith(rest, "off"))
                {
                    ControlWord("ql") ;
                    paragraph.align = ALIGNLEFT ;
                }
                else
                {
                    ControlWord("qc") ;
                    paragraph.align = ALIGNCENTER ;
                }
            }
            break ;

        case 'p' :
            // page break
             if (StartsWith(lowtext, ".pa"))
            {
                ControlWord("page") ;
            }

            // paragraph margin
             else if (StartsWith(lowtext, ".pm", &rest))
            {
                double value = mDocument->GetValue(text.substr(3), incdec);
                char type = mDocument->GetType(text.substr(3));

                int newpm ;
                newpm = static_cast<int>(mDocument->ConvertToTwips(value, type));
                mCurrentParagraphMargin = newpm ;

                // RTF \fi is relative to \li. WordStar .pm is absolute from page offset.
                // So \fi = .pm - .lm (both in twips).
                int fi = mCurrentParagraphMargin - mCurrentLeftMargin ;
                ControlWord("fi", fi) ;
            }

            // paragraph numbering
             else if (StartsWith(lowtext, ".pn", &rest))
            {
                long number = 0 ;
                rest = simplifyStr(rest) ;
                try
                {
                    number = std::stol(rest) ;
                }
                catch (...)
                {
                    number = 0 ;
                }

                if (number != 0)
                {
                    // we start page numbering with a new section
                    ControlWord("sect") ;
                    ControlWord("sectd") ;
                    ControlWord("sbknone") ;
                    ControlWord("pgnstarts", number) ;
                    ControlWord("pgnrestart") ;
                }
            }

            // page offset -- .po maps to \margl (left page margin)
             else if (StartsWith(lowtext, ".po", &rest))
            {
                // .poo = odd page offset, .poe = even page offset (ignored), .po = both
                // RTF uses \margl for the left page margin.
                // After \sectd resets section properties, re-emit all margins to preserve state.
                if (!rest.empty() && rest[0] == 'o')              // .poo
                {
                    rest = rest.substr(1) ;

                    double value = mDocument->GetValue(rest, incdec);
                    char type = mDocument->GetType(rest);
                    size_t twips = mDocument->ConvertToTwips(value, type) ;

                    // start a new section so page offset changes mid-document work
                    ControlWord("sect") ;
                    ControlWord("sectd") ;
                    ControlWord("sbknone") ;
                    ControlWord("facingp") ;
                    mCurrentPageOffset = static_cast<int>(twips) ;

                    // recompute \margr when page offset changes
                    mCurrentMargr = mPaperWidth - mCurrentPageOffset - mCurrentRightMarginPos ;
                    if (mCurrentMargr < 0)
                    {
                        mCurrentMargr = 0 ;
                    }

                    ControlWord("margl", mCurrentPageOffset) ;
                    ControlWord("margr", mCurrentMargr) ;
                    ControlWord("margt", mCurrentTopMargin) ;
                    ControlWord("margb", mCurrentBottomMargin) ;
                }
                else                            // .po
                {
                    double value = mDocument->GetValue(rest, incdec);
                    char type = mDocument->GetType(rest);
                    size_t twips = mDocument->ConvertToTwips(value, type) ;

                    // start a new section so page offset changes mid-document work
                    ControlWord("sect") ;
                    ControlWord("sectd") ;
                    ControlWord("sbknone") ;
                    mCurrentPageOffset = static_cast<int>(twips) ;

                    // recompute \margr when page offset changes
                    mCurrentMargr = mPaperWidth - mCurrentPageOffset - mCurrentRightMarginPos ;
                    if (mCurrentMargr < 0)
                    {
                        mCurrentMargr = 0 ;
                    }

                    ControlWord("margl", mCurrentPageOffset) ;
                    ControlWord("margr", mCurrentMargr) ;
                    ControlWord("margt", mCurrentTopMargin) ;
                    ControlWord("margb", mCurrentBottomMargin) ;
                }
            }

            // printer options (.pr or=l for landscape)
            else if (StartsWith(lowtext, ".pr", &rest))
            {
                if (containsCI(rest, "or=l"))
                {
                    ControlWord("landscape") ;
                }
            }

            // paragraph space before (.psb maps to \sb)
            else if (StartsWith(lowtext, ".psb", &rest))
            {
                double value = mDocument->GetValue(text.substr(4), incdec) ;
                char type = mDocument->GetType(text.substr(4)) ;
                int twips = static_cast<int>(mDocument->ConvertToTwips(value, type)) ;
                ControlWord("sb", twips) ;
            }

            // paragraph space after (.psa maps to \sa)
            else if (StartsWith(lowtext, ".psa", &rest))
            {
                double value = mDocument->GetValue(text.substr(4), incdec) ;
                char type = mDocument->GetType(text.substr(4)) ;
                int twips = static_cast<int>(mDocument->ConvertToTwips(value, type)) ;
                ControlWord("sa", twips) ;
            }

            break ;

        case 'r' :
            // right margin  (really right indent)
            if (StartsWith(lowtext, ".rm", &rest))
            {
                double value = mDocument->GetValue(text.substr(3), incdec);
                char type = mDocument->GetType(text.substr(3));

                int newrm ;
                newrm = static_cast<int>(mDocument->ConvertToTwips(value, type));

                if (newrm == 0)
                {
                    ControlWord("ri", 0) ;
                }
                else
                {
                    // RTF \ri = distance from right page margin to paragraph right edge.
                    // WordStar .rm = absolute right boundary from page offset.
                    // text area width = paperwidth - \margl - \margr
                    // \ri = text area width - .rm
                    int textAreaWidth = mPaperWidth - mCurrentPageOffset - mCurrentMargr ;
                    int ri = textAreaWidth - newrm ;
                    if (ri < 0)
                    {
                        ri = 0 ;
                    }
                    ControlWord("ri", ri) ;
                }
                mCurrentRightMarginPos = newrm ;
            }
            break ;

        case 't' :
            // .tb with type prefixes: ^=center, >=right, #=decimal
            if (StartsWith(lowtext, ".tb", &rest))
            {
                std::string nums = rest ;

                do
                {
                    std::string tabrest ;
                    std::string item = BeforeFirst(nums, ',', tabrest) ;
                    nums = tabrest ;

                    item = trimStr(item) ;
                    if (item.empty())
                    {
                        continue ;
                    }

                    if (item[0] == '^')
                    {
                        ControlWord("tqc") ;
                        item = item.substr(1) ;
                    }
                    else if (item[0] == '>')
                    {
                        ControlWord("tqr") ;
                        item = item.substr(1) ;
                    }
                    else if (item[0] == '#')
                    {
                        ControlWord("tqdec") ;
                        item = item.substr(1) ;
                    }

                    double value = mDocument->GetValue(item, incdec);
                    char type = mDocument->GetType(item);
                    int newtab = static_cast<int>(mDocument->ConvertToTwips(value, type));

                    ControlWord("tx", newtab) ;
                } while (!nums.empty()) ;
            }
            break  ;

    }

    // Preserve unhandled dot commands as RTF comments for roundtrip fidelity
    std::streampos posAfterSwitch = mFile.tellp() ;
    if (posAfterSwitch == posBeforeSwitch &&
       !StartsWith(lowtext, "..") && !StartsWith(lowtext, ".ig"))
    {
        // No handler wrote any output -- emit as ignorable RTF destination
        StartGroup() ;
        ControlWord("*") ;
        ControlWord("wsdotcmd") ;
        ControlText(" ") ;
        ControlText(text) ;
        EndGroup() ;
    }

    // comment
    if (StartsWith(lowtext, "..", &rest) || StartsWith(lowtext, ".ig", &rest))
    {
        StartGroup() ;
        ControlWord("*") ;
        ControlWord("atrfstart", mCommentCount) ;
        EndGroup() ;

        mFile << "." ;              // we write out a single dot for the commented text. Reading RTF will parse this back out. (Word needs it)

        StartGroup() ;
        ControlWord("*") ;
        ControlWord("atrfend", mCommentCount) ;
        EndGroup() ;

        StartGroup() ;
        ControlWord("*") ;
        ControlWord("atnid", "wordstar") ;           // @TODO atnid
        EndGroup() ;

        StartGroup() ;
        ControlWord("*") ;
        ControlWord("atnauthor", "WordTsar") ;       // @TODO atnauthor
        EndGroup() ;

        NewLine() ;

        StartGroup() ;
        ControlWord("*") ;
        ControlWord("annotation") ;

        StartGroup() ;
        ControlWord("*") ;
        ControlWord("atnref", mCommentCount) ;
        EndGroup() ;

        ControlWord("pard") ;
        ControlWord("plain", rest) ;
        EndGroup() ;

        mCommentCount++ ;
    }
}



/////////////////////////////////////////////////////////////////////////////
///
/// @param  control - [in] the RTF control word name (without backslash)
///
/// @return nothing
///
/// @brief
/// Writes an RTF control word (backslash + name) to the output file.
/////////////////////////////////////////////////////////////////////////////
void cRTFWriter::ControlWord(const std::string &control)
{
    std::string out = string_sprintf("\\%s", control.c_str()) ;
    mFile << out ;
    mNewLine = false ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  control   - [in] the RTF control word name
/// @param  parameter - [in] the numeric parameter value
///
/// @return nothing
///
/// @brief
/// Writes an RTF control word with a numeric parameter.
/////////////////////////////////////////////////////////////////////////////
void cRTFWriter::ControlWord(const std::string &control, const long parameter)
{
    std::string out = string_sprintf("\\%s%ld", control.c_str(), parameter) ;
    mFile << out ;
    mNewLine = false ;
}



/////////////////////////////////////////////////////////////////////////////
///
/// @param  control - [in] the RTF control word name
/// @param  text    - [in] the text parameter value
///
/// @return nothing
///
/// @brief
/// Writes an RTF control word followed by a space and text value.
/////////////////////////////////////////////////////////////////////////////
void cRTFWriter::ControlWord(const std::string &control, const std::string &text)
{
    std::string out = string_sprintf("\\%s %s", control.c_str(), text.c_str()) ;
    mFile << out ;
    mNewLine = false ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  text - [in] the text to write directly
///
/// @return nothing
///
/// @brief
/// Writes raw text to the RTF output file (no backslash prefix).
/////////////////////////////////////////////////////////////////////////////
void cRTFWriter::ControlText(const std::string &text)
{
    mFile << text ;
    mNewLine = false ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Writes a space to the RTF output file (unless we just wrote a newline).
/////////////////////////////////////////////////////////////////////////////
void cRTFWriter::ControlSpace(void)
{
    if (mNewLine == false)
    {
        mFile << " " ;
    }
    mNewLine = false ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Writes an opening brace and increments the group counter.
/////////////////////////////////////////////////////////////////////////////
void cRTFWriter::StartGroup(void)
{
    mFile << "{" ;
    mGroupCount++ ;
    mNewLine = false ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Writes a closing brace with newline and decrements the group counter.
/////////////////////////////////////////////////////////////////////////////
void cRTFWriter::EndGroup(void)
{
    mFile << "}\n" ;
    mGroupCount-- ;
    mNewLine = true ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Writes a newline to the RTF output file.
/////////////////////////////////////////////////////////////////////////////
void cRTFWriter::NewLine(void)
{
    mFile << "\n" ;
    mNewLine = true ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Writes current paragraph formatting state as RTF control words
/// (bold, italic, underline, font, alignment, etc.).
/////////////////////////////////////////////////////////////////////////////
// returns a string with the paragraph formatting
void cRTFWriter::ParagraphFormat(void)
{
    if (paragraph.character.bold)
    {
        ControlWord("b", ON);
    }
    if (paragraph.character.italics)
    {
        ControlWord("i", ON);
    }
    if (paragraph.character.underline)
    {
        ControlWord("ul", ON);
    }
    if (paragraph.character.subscript)
    {
        ControlWord("sub");
    }
    if (paragraph.character.superscript)
    {
        ControlWord("super");
    }
    if (paragraph.character.strikethrough)
    {
        ControlWord("strike");
    }

    ControlWord("f", paragraph.fontindex) ;
    ControlWord("fs", paragraph.fontsize) ;

    switch (paragraph.align)
    {
        case ALIGNCENTER :
            ControlWord("qc") ;
            break ;

        case ALIGNJUSTIFY :
            ControlWord("qj") ;
            break ;

        case ALIGNLEFT :
            ControlWord("ql") ;
            break ;

        case ALIGNRIGHT :
            ControlWord("qr") ;
            break ;
    } ;

    mNewLine = false ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  text - [in] the .ix index entry text
///
/// @return nothing
///
/// @brief
/// Processes an index entry dot command and emits the RTF index entry.
/////////////////////////////////////////////////////////////////////////////
void cRTFWriter::IndexText(std::string text)
{
//    bool cross = false ;
//    bool sub = false ;
//    bool bold = false ;

    // strip off command
    text = text.substr(4) ;
    text = simplifyStr(text) ;

    // now see if this is a cross reference
    if (text[0] == '-')
    {
//        sub = true ;
        text = text.substr(1) ;
    }
    // or the page number must be in bold
    else if (text[0] == '+')
    {
//        bold = true ;
        text = text.substr(1) ;
    }

/* TODO QT
    // now check if this is a cross reference
    std::string crosstext = text ;
    int found = 0 ;
    while(found != wxNOT_FOUND)
    {
        found = crosstext.Find(',') ;
        if(found != wxNOT_FOUND)
        {
            if(crosstext[found - 1] != '\\')

            {
                break ;
            }
        }
    }


    std::string left, right ;
    if(found != wxNOT_FOUND)
    {
        if(sub != true)
        {
            cross = true ;
        }

        left = text.Left(found) ;
        right = text.right(text.length() - found - 1) ;
    }

    left.Replace("\\", "") ;
    right.Replace("\\", "") ;
    text.Replace("\\", "") ;

    ControlWord("xe") ;
    ControlWord("v ") ;

    StartGroup() ;

    if(!sub && !cross)
    {
        ControlText(text) ;
    }

    if(cross)
    {
        std::string ftext = left + "\\:" + right ;
        ControlText(ftext) ;
    }

    if(sub)
    {
        ControlText(left) ;
    }

    EndGroup() ;

    if(sub)
    {
        StartGroup() ;
        ControlWord("txe ") ;
        ControlText(right) ;
        EndGroup() ;
    }

    if(bold)
    {
        StartGroup() ;
        ControlWord("bxe") ;
        EndGroup() ;
    }
*/
}


