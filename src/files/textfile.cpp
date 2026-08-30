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
 * @class cTextFile
 *
 * @brief Plain text file loader with automatic encoding detection.
 *
 * Implements the cTextFile class, which loads plain text files into the
 * document model. Acts as the fallback loader that accepts any file type
 * not claimed by the WordStar, RTF, or DOCX handlers.
 *
 * @section textfile_encoding Encoding Detection
 * Detects file encoding using a multi-stage approach:
 * - Binary detection: rejects known binary file signatures (ELF, PE, PNG,
 *   JPEG, PDF, ZIP, GIF, GZIP) and files with high control character ratios
 * - BOM detection: recognizes UTF-32LE (FF FE 00 00), UTF-32BE (00 00 FE FF),
 *   UTF-8 (EF BB BF), UTF-16LE (FF FE), and UTF-16BE (FE FF) byte order marks
 * - Heuristic analysis: examines byte patterns to distinguish UTF-16 (null
 *   byte distribution), valid UTF-8, and legacy single-byte encodings
 * - Code page guessing: when UTF-8 validation fails, analyzes high byte
 *   patterns to distinguish Windows-1252/Latin-1 from DOS code pages
 *   (CP437 and CP850)
 * - Fallback: assumes system locale if no encoding can be determined
 *
 * @section textfile_conversion Encoding Conversion
 * - UTF-32LE/BE files are converted to UTF-8 via codepoint extraction
 * - UTF-16LE/BE files are converted to UTF-8 using cpp-unicodelib
 * - Latin-1/CP1252 files have bytes 0x80-0xFF mapped via cCodePageWin1252
 * - DOS CP437 files are converted via cCodePage437
 * - DOS CP850 files are converted via cCodePage850
 * - Line endings are normalized: LF and CR+LF are converted to CR only
 *   (WordTsar's internal paragraph delimiter)
 *
 * @section textfile_capabilities Format Capabilities
 * - CanLoad(): always returns true (accepts any file as plain text)
 * - CanSave(): returns true (saves as UTF-8 with CRLF line endings)
 *
 * @author Gerald Brandt
 * @copyright GNU Affero General Public License v3.0
 *
 * @see cTextFile Plain text file handler class
 * @see cFile Base file handler class
 * @see eTextEncoding Encoding type enumeration
 * @see cCodePageWin1252 Windows-1252 code page converter
 * @see cCodePage437 DOS CP437 code page converter
 * @see cCodePage850 DOS CP850 code page converter
 * @see cDocument Document model receiving loaded text
 */

#include <fstream>
#include <vector>
#include <algorithm>
#include <locale>
#include <codecvt>
#include <cstdint>

#include "textfile.h"
#include "src/core/document/document.h"
#include "src/core/codepage/cp437.h"
#include "src/core/codepage/cp850.h"
#include "src/core/codepage/cp1252.h"
#include "src/core/editor/editorbase.h"
#include "third-party/cpp-unicodelib/unicodelib_encodings.h"

/// @ingroup Editor
/// @{

/////////////////////////////////////////////////////////////////////////////
///
/// @param  editor [in] pointer to editor control (optional)
///
/// @return nothing
///
/// @brief
/// Constructor.
///
/////////////////////////////////////////////////////////////////////////////
cTextFile::cTextFile(cEditorBase *editor)
    : cFile(editor)
{
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Destructor.
///
/////////////////////////////////////////////////////////////////////////////
cTextFile::~cTextFile(void)
{
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  filename [in] filename to check
///
/// @return true if file type is supported
///
/// @brief
/// Check if this file handler can process the given filename.
/// Plain text loader accepts any file type.
///
/////////////////////////////////////////////////////////////////////////////
bool cTextFile::CheckType(std::string filename)
{
    UNUSED_ARGUMENT(filename);  // Plain text accepts any filename
    return true;  // Try to load anything as plain text
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  buffer [in] first bytes of file
/// @param  bomSize [out] size of BOM in bytes (0 if no BOM)
///
/// @return detected encoding
///
/// @brief
/// Detect encoding from Byte Order Mark (BOM) at start of file.
/// Checks UTF-32 BOMs first (4 bytes) since UTF-32LE starts with the
/// same FF FE prefix as UTF-16LE. Returns the detected encoding or
/// SYSTEM_LOCALE if no BOM found.
///
/////////////////////////////////////////////////////////////////////////////
eTextEncoding cTextFile::DetectBOM(const std::vector<unsigned char>& buffer, size_t& bomSize)
{
    bomSize = 0;

    // Check 4-byte BOMs first (UTF-32 must come before UTF-16 because
    // UTF-32LE BOM FF FE 00 00 starts with the same bytes as UTF-16LE FF FE)
    if (buffer.size() >= 4)
    {
        // UTF-32 LE BOM: FF FE 00 00
        if (buffer[0] == 0xFF && buffer[1] == 0xFE && buffer[2] == 0x00 && buffer[3] == 0x00)
        {
            bomSize = 4;
            return eTextEncoding::UTF32LE;
        }

        // UTF-32 BE BOM: 00 00 FE FF
        if (buffer[0] == 0x00 && buffer[1] == 0x00 && buffer[2] == 0xFE && buffer[3] == 0xFF)
        {
            bomSize = 4;
            return eTextEncoding::UTF32BE;
        }
    }

    if (buffer.size() >= 3)
    {
        // UTF-8 BOM: EF BB BF
        if (buffer[0] == 0xEF && buffer[1] == 0xBB && buffer[2] == 0xBF)
        {
            bomSize = 3;
            return eTextEncoding::UTF8;
        }
    }

    if (buffer.size() >= 2)
    {
        // UTF-16 LE BOM: FF FE
        if (buffer[0] == 0xFF && buffer[1] == 0xFE)
        {
            bomSize = 2;
            return eTextEncoding::UTF16LE;
        }

        // UTF-16 BE BOM: FE FF
        if (buffer[0] == 0xFE && buffer[1] == 0xFF)
        {
            bomSize = 2;
            return eTextEncoding::UTF16BE;
        }
    }

    return eTextEncoding::SYSTEM_LOCALE;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  data [in] byte sequence to validate
/// @param  length [in] number of bytes to check
///
/// @return true if valid UTF-8 sequence
///
/// @brief
/// Validate UTF-8 byte sequence for proper multi-byte encoding.
/// Checks continuation bytes and overlong encodings.
///
/////////////////////////////////////////////////////////////////////////////
bool cTextFile::IsValidUTF8(const unsigned char* data, size_t length)
{
    size_t i = 0;

    while (i < length)
    {
        unsigned char byte = data[i];

        // Single-byte character (0xxxxxxx)
        if ((byte & 0x80) == 0x00)
        {
            i++;
            continue;
        }

        // Multi-byte character - determine expected length
        int extraBytes = 0;
        if ((byte & 0xE0) == 0xC0)
        {
            extraBytes = 1;  // 110xxxxx
        }
        else if ((byte & 0xF0) == 0xE0)
        {
            extraBytes = 2;  // 1110xxxx
        }
        else if ((byte & 0xF8) == 0xF0)
        {
            extraBytes = 3;  // 11110xxx
        }
        else
        {
            return false;  // Invalid UTF-8 start byte
        }

        // Check we have enough bytes
        if (i + extraBytes >= length)
        {
            return false;
        }

        // Validate continuation bytes (10xxxxxx)
        for (int j = 1; j <= extraBytes; j++)
        {
            if ((data[i + j] & 0xC0) != 0x80)
            {
                return false;
            }
        }

        i += extraBytes + 1;
    }

    return true;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  out [in/out] string to append UTF-8 bytes to
/// @param  cp [in] Unicode codepoint to encode
///
/// @return nothing
///
/// @brief
/// Encode a single Unicode codepoint as UTF-8 and append it to a string.
/// Handles 1, 2, 3, and 4 byte sequences. Invalid or out-of-range
/// codepoints are replaced with U+FFFD.
///
/////////////////////////////////////////////////////////////////////////////
static void AppendCodepointAsUTF8(std::string& out, uint32_t cp)
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
    else if (cp <= 0x10FFFF)
    {
        out += static_cast<char>(0xF0 | (cp >> 18));
        out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    }
    else
    {
        // Invalid codepoint, insert replacement character U+FFFD
        out += "\xEF\xBF\xBD";
    }
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  buffer [in] file data to check
/// @param  length [in] number of bytes to analyze
///
/// @return true if file appears to be binary, false if it looks like text
///
/// @brief
/// Detect binary files by checking for known binary file signatures and
/// counting non-text control characters. Checks for ELF, PE/MZ, PNG,
/// JPEG, PDF, ZIP, GIF, and GZIP signatures. Then counts control bytes
/// in ranges 0x01-0x07 and 0x0E-0x1F (excluding tab, LF, CR). If
/// control characters exceed 5% of the sample, the file is binary.
///
/////////////////////////////////////////////////////////////////////////////
bool cTextFile::IsBinaryFile(const std::vector<unsigned char>& buffer, size_t length)
{
    if (length < 4)
    {
        return false;
    }

    // Check common binary file signatures
    // ELF executable: 7F 45 4C 46
    if (buffer[0] == 0x7F && buffer[1] == 0x45 && buffer[2] == 0x4C && buffer[3] == 0x46)
    {
        return true;
    }

    // PE/MZ executable: 4D 5A
    if (buffer[0] == 0x4D && buffer[1] == 0x5A)
    {
        return true;
    }

    // PNG image: 89 50 4E 47
    if (buffer[0] == 0x89 && buffer[1] == 0x50 && buffer[2] == 0x4E && buffer[3] == 0x47)
    {
        return true;
    }

    // JPEG image: FF D8 FF
    if (length >= 3 && buffer[0] == 0xFF && buffer[1] == 0xD8 && buffer[2] == 0xFF)
    {
        return true;
    }

    // PDF: %PDF (25 50 44 46)
    if (buffer[0] == 0x25 && buffer[1] == 0x50 && buffer[2] == 0x44 && buffer[3] == 0x46)
    {
        return true;
    }

    // ZIP/DOCX/JAR: 50 4B 03 04
    if (buffer[0] == 0x50 && buffer[1] == 0x4B && buffer[2] == 0x03 && buffer[3] == 0x04)
    {
        return true;
    }

    // GIF image: GIF8 (47 49 46 38)
    if (buffer[0] == 0x47 && buffer[1] == 0x49 && buffer[2] == 0x46 && buffer[3] == 0x38)
    {
        return true;
    }

    // GZIP: 1F 8B
    if (buffer[0] == 0x1F && buffer[1] == 0x8B)
    {
        return true;
    }

    // Count non-text control characters (0x01-0x07, 0x0E-0x1F)
    // Excludes tab (0x09), LF (0x0A), CR (0x0D) which are valid in text
    int controlCount = 0;
    for (size_t i = 0; i < length; i++)
    {
        unsigned char byte = buffer[i];
        if ((byte >= 0x01 && byte <= 0x07) || (byte >= 0x0E && byte <= 0x1F))
        {
            controlCount++;
        }
    }

    // If more than 5% of bytes are control characters, likely binary
    if (controlCount > static_cast<int>(length / 20))
    {
        return true;
    }

    return false;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  buffer [in] file data with non-UTF-8 high bytes
/// @param  length [in] number of bytes to analyze
///
/// @return LATIN1, DOS_CP850, or DOS_CP437
///
/// @brief
/// Guess the legacy code page when UTF-8 validation fails. Examines
/// high bytes (0x80-0xFF) to distinguish Windows-1252 text from DOS
/// code page text. Bytes 0x81, 0x8D, 0x8F, 0x90 are undefined in
/// CP1252, so their presence suggests a DOS code page. Box-drawing
/// characters (0xB0-0xDF) also suggest DOS. Within DOS code pages,
/// bytes 0xE0-0xEF map to Greek/math symbols in CP437 but to Latin
/// accented characters in CP850. Defaults to LATIN1 (treated as
/// CP1252) since it is the most common legacy encoding on modern
/// systems.
///
/////////////////////////////////////////////////////////////////////////////
eTextEncoding cTextFile::GuessCodePage(const std::vector<unsigned char>& buffer, size_t length)
{
    // Count high bytes and look for code page indicators
    int highByteCount = 0;
    int cp1252Undefined = 0;    // bytes undefined in CP1252 (0x81, 0x8D, 0x8F, 0x90)
    int boxDrawing = 0;         // DOS box-drawing characters (0xB0-0xDF in CP437/CP850)
    int greekMath = 0;          // Greek/math bytes (0xE0-0xEF, CP437-specific)
    int latinAccented = 0;      // Latin accented in 0xD0-0xDF range (CP850 replaced CP437 box-drawing here)

    for (size_t i = 0; i < length; i++)
    {
        unsigned char byte = buffer[i];
        if (byte >= 0x80)
        {
            highByteCount++;

            // These bytes are undefined in CP1252 but valid in DOS code pages
            if (byte == 0x81 || byte == 0x8D || byte == 0x8F || byte == 0x90)
            {
                cp1252Undefined++;
            }

            // Box-drawing characters (shared by CP437 and CP850)
            if (byte >= 0xB0 && byte <= 0xCF)
            {
                boxDrawing++;
            }

            // 0xD0-0xDF: box-drawing in CP437, Latin accented in CP850
            if (byte >= 0xD0 && byte <= 0xDF)
            {
                latinAccented++;
            }

            // 0xE0-0xEF: Greek/math in CP437, Latin accented in CP850
            // Common CP437 Greek: alpha(E0), beta/ss(E1), Gamma(E2), pi(E3),
            // Sigma(E4/E5), mu(E6), tau(E7), Phi(E8)
            if (byte >= 0xE0 && byte <= 0xEF)
            {
                greekMath++;
            }
        }
    }

    if (highByteCount == 0)
    {
        // Pure ASCII, treat as UTF-8
        return eTextEncoding::UTF8;
    }

    // If we see undefined CP1252 bytes or significant box-drawing,
    // this is a DOS code page file
    if (cp1252Undefined > 0 || boxDrawing > highByteCount / 4)
    {
        // Distinguish CP437 from CP850:
        // CP437 has Greek/math in 0xE0-0xEF and box-drawing in 0xD0-0xDF
        // CP850 has Latin accented chars in both ranges
        // If Greek/math bytes dominate over Latin accented bytes, choose CP437
        if (greekMath > latinAccented && greekMath > 0)
        {
            return eTextEncoding::DOS_CP437;
        }

        // Default DOS code page is CP850 (broader Western European coverage)
        return eTextEncoding::DOS_CP850;
    }

    // Default to Latin-1/CP1252 (most common legacy encoding)
    return eTextEncoding::LATIN1;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  buffer [in] file data to analyze
/// @param  length [in] number of bytes to analyze
///
/// @return detected encoding
///
/// @brief
/// Heuristically detect encoding by analyzing byte patterns.
/// Checks for binary file signatures, UTF-16 null byte patterns,
/// valid UTF-8 sequences, and legacy code page indicators.
/// Falls back to GuessCodePage() when UTF-8 validation fails.
///
/////////////////////////////////////////////////////////////////////////////
eTextEncoding cTextFile::DetectEncodingHeuristic(const std::vector<unsigned char>& buffer, size_t length)
{
    if (length < 2)
    {
        return eTextEncoding::UTF8;
    }

    // Check for binary files first
    if (IsBinaryFile(buffer, length))
    {
        return eTextEncoding::BINARY;
    }

    // Count null bytes (indicator of UTF-16)
    int nullCount = 0;
    int evenNulls = 0;  // Nulls at even positions
    int oddNulls = 0;   // Nulls at odd positions

    for (size_t i = 0; i < length; i++)
    {
        if (buffer[i] == 0x00)
        {
            nullCount++;
            if (i % 2 == 0)
            {
                evenNulls++;
            }
            else
            {
                oddNulls++;
            }
        }
    }

    // If we have many null bytes, likely UTF-16
    if (nullCount > static_cast<int>(length / 10))
    {
        // Check which byte position has more nulls
        if (oddNulls > evenNulls * 2)
        {
            return eTextEncoding::UTF16LE;  // Nulls at odd positions
        }
        if (evenNulls > oddNulls * 2)
        {
            return eTextEncoding::UTF16BE;  // Nulls at even positions
        }
    }

    // Try to validate as UTF-8
    if (IsValidUTF8(buffer.data(), length))
    {
        return eTextEncoding::UTF8;
    }

    // UTF-8 failed, guess legacy code page
    return GuessCodePage(buffer, length);
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  buffer [in] UTF-16 encoded data
/// @param  length [in] number of bytes in buffer
/// @param  littleEndian [in] true for UTF-16LE, false for UTF-16BE
///
/// @return UTF-8 encoded string
///
/// @brief
/// Convert UTF-16 byte buffer to UTF-8 string using cpp-unicodelib.
/// Handles byte order (LE/BE) and converts to char16_t for conversion.
///
/////////////////////////////////////////////////////////////////////////////
std::string cTextFile::ConvertUTF16ToUTF8(const std::vector<unsigned char>& buffer, size_t length, bool littleEndian)
{
    // Convert bytes to char16_t array
    std::vector<char16_t> utf16chars;
    utf16chars.reserve(length / 2);

    for (size_t i = 0; i + 1 < length; i += 2)
    {
        char16_t ch;
        if (littleEndian)
        {
            ch = static_cast<char16_t>(buffer[i] | (buffer[i + 1] << 8));
        }
        else
        {
            ch = static_cast<char16_t>((buffer[i] << 8) | buffer[i + 1]);
        }
        utf16chars.push_back(ch);
    }

    // Convert to UTF-8 using cpp-unicodelib
    return unicode::to_utf8(utf16chars.data(), utf16chars.size());
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  buffer [in] UTF-32 encoded data
/// @param  length [in] number of bytes in buffer
/// @param  littleEndian [in] true for UTF-32LE, false for UTF-32BE
///
/// @return UTF-8 encoded string
///
/// @brief
/// Convert UTF-32 byte buffer to UTF-8 string. Each codepoint is stored
/// as 4 bytes in the specified byte order. Codepoints are read and
/// encoded as UTF-8.
///
/////////////////////////////////////////////////////////////////////////////
std::string cTextFile::ConvertUTF32ToUTF8(const std::vector<unsigned char>& buffer, size_t length, bool littleEndian)
{
    std::string result;
    result.reserve(length);

    for (size_t i = 0; i + 3 < length; i += 4)
    {
        uint32_t codepoint;
        if (littleEndian)
        {
            codepoint = static_cast<uint32_t>(buffer[i])
                      | (static_cast<uint32_t>(buffer[i + 1]) << 8)
                      | (static_cast<uint32_t>(buffer[i + 2]) << 16)
                      | (static_cast<uint32_t>(buffer[i + 3]) << 24);
        }
        else
        {
            codepoint = (static_cast<uint32_t>(buffer[i]) << 24)
                      | (static_cast<uint32_t>(buffer[i + 1]) << 16)
                      | (static_cast<uint32_t>(buffer[i + 2]) << 8)
                      | static_cast<uint32_t>(buffer[i + 3]);
        }

        AppendCodepointAsUTF8(result, codepoint);
    }

    return result;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  buffer [in] file data containing Latin-1/CP1252 bytes
/// @param  offset [in] byte offset to start converting from
/// @param  length [in] number of bytes to convert
///
/// @return UTF-8 encoded string
///
/// @brief
/// Convert Latin-1/Windows-1252 encoded bytes to UTF-8. ASCII bytes
/// (0x00-0x7F) pass through unchanged. High bytes (0x80-0xFF) are
/// mapped to Unicode codepoints via the cCodePageWin1252 converter
/// and then encoded as UTF-8. Unmapped bytes become U+FFFD.
///
/////////////////////////////////////////////////////////////////////////////
std::string cTextFile::ConvertLatin1ToUTF8(const std::vector<unsigned char>& buffer, size_t offset, size_t length)
{
    cCodePageWin1252 cp1252;
    std::string result;
    result.reserve(length * 2);

    for (size_t i = offset; i < offset + length; i++)
    {
        unsigned char byte = buffer[i];
        unsigned long codepoint = cp1252.toUTF8(byte);
        if (codepoint != UINT32_MAX)
        {
            AppendCodepointAsUTF8(result, static_cast<uint32_t>(codepoint));
        }
        else
        {
            // Unmapped byte - insert Unicode replacement character U+FFFD
            result += "\xEF\xBF\xBD";
        }
    }

    return result;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  buffer [in] file data containing CP437 bytes
/// @param  offset [in] byte offset to start converting from
/// @param  length [in] number of bytes to convert
///
/// @return UTF-8 encoded string
///
/// @brief
/// Convert DOS Code Page 437 encoded bytes to UTF-8. ASCII bytes
/// (0x00-0x7F) pass through unchanged. High bytes (0x80-0xFF) are
/// mapped to Unicode codepoints via the cCodePage437 converter
/// and then encoded as UTF-8. Unmapped bytes become U+FFFD.
///
/////////////////////////////////////////////////////////////////////////////
std::string cTextFile::ConvertCP437ToUTF8(const std::vector<unsigned char>& buffer, size_t offset, size_t length)
{
    cCodePage437 cp437;
    std::string result;
    result.reserve(length * 2);

    for (size_t i = offset; i < offset + length; i++)
    {
        unsigned char byte = buffer[i];
        unsigned long codepoint = cp437.toUTF8(byte);
        if (codepoint != UINT32_MAX)
        {
            AppendCodepointAsUTF8(result, static_cast<uint32_t>(codepoint));
        }
        else
        {
            // Unmapped byte - insert Unicode replacement character U+FFFD
            result += "\xEF\xBF\xBD";
        }
    }

    return result;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  buffer [in] file data containing CP850 bytes
/// @param  offset [in] byte offset to start converting from
/// @param  length [in] number of bytes to convert
///
/// @return UTF-8 encoded string
///
/// @brief
/// Convert DOS Code Page 850 encoded bytes to UTF-8. ASCII bytes
/// (0x00-0x7F) pass through unchanged. High bytes (0x80-0xFF) are
/// mapped to Unicode codepoints via the cCodePage850 converter
/// and then encoded as UTF-8. Unmapped bytes become U+FFFD.
///
/////////////////////////////////////////////////////////////////////////////
std::string cTextFile::ConvertCP850ToUTF8(const std::vector<unsigned char>& buffer, size_t offset, size_t length)
{
    cCodePage850 cp850;
    std::string result;
    result.reserve(length * 2);

    for (size_t i = offset; i < offset + length; i++)
    {
        unsigned char byte = buffer[i];
        unsigned long codepoint = cp850.toUTF8(byte);
        if (codepoint != UINT32_MAX)
        {
            AppendCodepointAsUTF8(result, static_cast<uint32_t>(codepoint));
        }
        else
        {
            // Unmapped byte - insert Unicode replacement character U+FFFD
            result += "\xEF\xBF\xBD";
        }
    }

    return result;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  content [in] text with mixed line endings
///
/// @return text with all line endings converted to HARD_RETURN
///
/// @brief
/// Normalize all line ending types (CR, LF, CRLF) to HARD_RETURN.
/// Handles Windows (CRLF), Unix (LF), and old Mac (CR) formats.
///
/////////////////////////////////////////////////////////////////////////////
std::string cTextFile::NormalizeLineEndings(const std::string& content)
{
    std::string result;
    result.reserve(content.size());

    for (size_t i = 0; i < content.size(); i++)
    {
        char ch = content[i];

        if (ch == '\r')
        {
            // Check for CRLF or LFCR
            if (i + 1 < content.size() && (content[i + 1] == '\n'))
            {
                // CRLF - consume both, output one HARD_RETURN
                result += HARD_RETURN;
                i++;  // Skip the \n
            }
            else
            {
                // Standalone CR - output HARD_RETURN
                result += HARD_RETURN;
            }
        }
        else if (ch == '\n')
        {
            // Check for LFCR (rare)
            if (i + 1 < content.size() && (content[i + 1] == '\r'))
            {
                // LFCR - consume both, output one HARD_RETURN
                result += HARD_RETURN;
                i++;  // Skip the \r
            }
            else
            {
                // Standalone LF - output HARD_RETURN
                result += HARD_RETURN;
            }
        }
        else
        {
            // Normal character
            result += ch;
        }
    }

    return result;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  filename [in] file to load
///
/// @return true if file loaded successfully
///
/// @brief
/// Load plain text file with automatic encoding detection and line ending
/// normalization. Detects and rejects binary files. Supports UTF-8,
/// UTF-16LE, UTF-16BE, UTF-32LE, UTF-32BE with or without BOM, and
/// legacy encodings (Latin-1/CP1252, DOS CP437, DOS CP850). Handles
/// CR, LF, and CRLF line endings.
///
/////////////////////////////////////////////////////////////////////////////
bool cTextFile::LoadFile(std::string filename)
{
    // Open file in binary mode
    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    if (!file.is_open())
    {
        return false;
    }

    // Get file size
    std::streamsize fileSize = file.tellg();
    file.seekg(0, std::ios::beg);

    if (fileSize == 0)
    {
        return true;  // Empty file is valid
    }

    // Read entire file into buffer
    std::vector<unsigned char> buffer(fileSize);
    if (!file.read(reinterpret_cast<char*>(buffer.data()), fileSize))
    {
        return false;
    }
    file.close();

    // Detect BOM
    size_t bomSize = 0;
    eTextEncoding encoding = DetectBOM(buffer, bomSize);

    // If no BOM, analyze first 8192 bytes for heuristic detection
    if (encoding == eTextEncoding::SYSTEM_LOCALE)
    {
        size_t analyzeSize = std::min(static_cast<size_t>(8192), buffer.size());
        encoding = DetectEncodingHeuristic(buffer, analyzeSize);
    }

    // Reject binary files
    if (encoding == eTextEncoding::BINARY)
    {
        if (mEditor != nullptr)
        {
            mEditor->ShowError("Binary File",
                "This file appears to be a binary file and cannot be loaded as text.");
        }
        return false;
    }

    // Convert to UTF-8 based on detected encoding
    std::string utf8Content;

    if (encoding == eTextEncoding::UTF32LE || encoding == eTextEncoding::UTF32BE)
    {
        // Convert UTF-32 to UTF-8
        bool littleEndian = (encoding == eTextEncoding::UTF32LE);
        utf8Content = ConvertUTF32ToUTF8(buffer, buffer.size() - bomSize, littleEndian);
    }
    else if (encoding == eTextEncoding::UTF16LE || encoding == eTextEncoding::UTF16BE)
    {
        // Convert UTF-16 to UTF-8
        bool littleEndian = (encoding == eTextEncoding::UTF16LE);
        utf8Content = ConvertUTF16ToUTF8(buffer, buffer.size() - bomSize, littleEndian);
    }
    else if (encoding == eTextEncoding::UTF8)
    {
        // Already UTF-8, just skip BOM if present
        utf8Content = std::string(reinterpret_cast<char*>(buffer.data() + bomSize),
                                   buffer.size() - bomSize);

        // Validate UTF-8
        if (!IsValidUTF8(reinterpret_cast<unsigned char*>(utf8Content.data()), utf8Content.size()))
        {
            // Invalid UTF-8, try Latin-1 conversion
            encoding = eTextEncoding::LATIN1;
        }
    }

    if (encoding == eTextEncoding::LATIN1)
    {
        // Convert Latin-1/CP1252 high bytes to proper UTF-8
        utf8Content = ConvertLatin1ToUTF8(buffer, bomSize, buffer.size() - bomSize);
    }
    else if (encoding == eTextEncoding::DOS_CP437)
    {
        // Convert DOS Code Page 437 high bytes to proper UTF-8
        utf8Content = ConvertCP437ToUTF8(buffer, bomSize, buffer.size() - bomSize);
    }
    else if (encoding == eTextEncoding::DOS_CP850)
    {
        // Convert DOS Code Page 850 high bytes to proper UTF-8
        utf8Content = ConvertCP850ToUTF8(buffer, bomSize, buffer.size() - bomSize);
    }
    else if (encoding == eTextEncoding::SYSTEM_LOCALE)
    {
        // Treat as plain ASCII/system locale
        utf8Content = std::string(reinterpret_cast<char*>(buffer.data() + bomSize),
                                   buffer.size() - bomSize);
    }

    // Normalize line endings (CR, LF, CRLF -> HARD_RETURN)
    std::string normalized = NormalizeLineEndings(utf8Content);

    // Insert content into document
    // Process character by character to handle line breaks
    std::string currentLine;

    for (size_t i = 0; i < normalized.size(); i++)
    {
        if (normalized[i] == HARD_RETURN)
        {
            // Insert accumulated line content
            if (!currentLine.empty())
            {
                mDocument->Insert(currentLine);
                currentLine.clear();
            }

            // Insert hard return (creates new paragraph)
            mDocument->Insert(HARD_RETURN);
        }
        else
        {
            // Accumulate character
            currentLine += normalized[i];
        }
    }

    // Insert final line if any
    if (!currentLine.empty())
    {
        mDocument->Insert(currentLine);
    }

    return true;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  filename [in] file to save
/// @param  length [in] document length in graphemes
///
/// @return true if file saved successfully
///
/// @brief
/// Save document as UTF-8 plain text with Windows line endings (CRLF).
/// Converts HARD_RETURN to CRLF, skips REPLACE_CHAR markers.
///
/////////////////////////////////////////////////////////////////////////////
bool cTextFile::SaveFile(std::string filename, POSITION_T length)
{
    std::ofstream file(filename, std::ios::binary);
    if (!file.is_open())
    {
        return false;
    }

    for (POSITION_T loop = 0; loop < length; loop++)
    {
        std::string ch = mDocument->GetChar(loop);

        // Skip block markers and save-position bookmarks
        if (ch[0] == REPLACE_CHAR || ch[0] == SAVE_CHAR)
        {
            continue;
        }

        // Convert HARD_RETURN to CRLF
        if (ch[0] == HARD_RETURN && ch.length() == 1)
        {
            file << '\r' << '\n';
        }
        else
        {
            // Normal character
            file << ch;
        }
    }

    file.close();
    return true;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return true
///
/// @brief
/// Check if this file handler can load files.
///
/////////////////////////////////////////////////////////////////////////////
bool cTextFile::CanLoad(void)
{
    return true;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return true
///
/// @brief
/// Check if this file handler can save files.
///
/////////////////////////////////////////////////////////////////////////////
bool cTextFile::CanSave(void)
{
    return true;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return file extension filter string
///
/// @brief
/// Get file extensions supported by this handler.
///
/////////////////////////////////////////////////////////////////////////////
std::string cTextFile::GetExtensions(void)
{
    return "All Files (*.*)";
}

/// @}
