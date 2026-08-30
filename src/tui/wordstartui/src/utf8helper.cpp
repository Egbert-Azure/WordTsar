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

#include "utf8helper.h"

#include <algorithm>
#include <clocale>
#include <cwchar>

namespace wordstartui
{

/////////////////////////////////////////////////////////////////////////////
///
/// @class cUtf8Helper
///
/// @brief
/// Provides small UTF-8 and grapheme helpers used by terminal input and cell
/// drawing.
///
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
///
/// @param  const std::string& text [in] UTF-8 text to decode
/// @param  size_t& index [in,out] byte offset into the string
/// @param  char32_t& codepoint [out] decoded Unicode scalar value
///
/// @return true when a valid or replacement codepoint was decoded
///
/// @brief
/// Decode the next UTF-8 sequence from a string.
///
/////////////////////////////////////////////////////////////////////////////
bool cUtf8Helper::DecodeNext(const std::string& text, size_t& index, char32_t& codepoint)
{
    if (index >= text.size())
    {
        return false;
    }

    const unsigned char byte0 = static_cast<unsigned char>(text[index]);

    if (byte0 < 0x80U)
    {
        codepoint = byte0;
        ++index;
        return true;
    }

    int count = 0;
    char32_t value = 0;

    if ((byte0 & 0xE0U) == 0xC0U)
    {
        count = 2;
        value = byte0 & 0x1FU;
    }
    else if ((byte0 & 0xF0U) == 0xE0U)
    {
        count = 3;
        value = byte0 & 0x0FU;
    }
    else if ((byte0 & 0xF8U) == 0xF0U)
    {
        count = 4;
        value = byte0 & 0x07U;
    }
    else
    {
        codepoint = 0xFFFDU;
        ++index;
        return true;
    }

    if ((index + static_cast<size_t>(count)) > text.size())
    {
        codepoint = 0xFFFDU;
        index = text.size();
        return true;
    }

    for (int i = 1; i < count; ++i)
    {
        const unsigned char nextByte = static_cast<unsigned char>(text[index + static_cast<size_t>(i)]);

        if ((nextByte & 0xC0U) != 0x80U)
        {
            codepoint = 0xFFFDU;
            ++index;
            return true;
        }

        value = static_cast<char32_t>((value << 6U) | (nextByte & 0x3FU));
    }

    codepoint = value;
    index += static_cast<size_t>(count);

    return true;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  char32_t codepoint [in] Unicode scalar value
///
/// @return UTF-8 encoded string
///
/// @brief
/// Encode a Unicode scalar value as UTF-8.
///
/////////////////////////////////////////////////////////////////////////////
std::string cUtf8Helper::Encode(char32_t codepoint)
{
    std::string text;

    if (codepoint <= 0x7FU)
    {
        text.push_back(static_cast<char>(codepoint));
    }
    else if (codepoint <= 0x7FFU)
    {
        text.push_back(static_cast<char>(0xC0U | ((codepoint >> 6U) & 0x1FU)));
        text.push_back(static_cast<char>(0x80U | (codepoint & 0x3FU)));
    }
    else if (codepoint <= 0xFFFFU)
    {
        text.push_back(static_cast<char>(0xE0U | ((codepoint >> 12U) & 0x0FU)));
        text.push_back(static_cast<char>(0x80U | ((codepoint >> 6U) & 0x3FU)));
        text.push_back(static_cast<char>(0x80U | (codepoint & 0x3FU)));
    }
    else
    {
        text.push_back(static_cast<char>(0xF0U | ((codepoint >> 18U) & 0x07U)));
        text.push_back(static_cast<char>(0x80U | ((codepoint >> 12U) & 0x3FU)));
        text.push_back(static_cast<char>(0x80U | ((codepoint >> 6U) & 0x3FU)));
        text.push_back(static_cast<char>(0x80U | (codepoint & 0x3FU)));
    }

    return text;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  const std::string& text [in] UTF-8 text to split
///
/// @return vector of UTF-8 grapheme clusters
///
/// @brief
/// Split UTF-8 text into simple grapheme clusters.
///
/// @note
/// This implements the common terminal cases: base characters followed by
/// combining marks, variation selectors, and zero-width joiner sequences. It
/// is intentionally small and does not replace ICU for document layout.
///
/////////////////////////////////////////////////////////////////////////////
std::vector<std::string> cUtf8Helper::SplitGraphemes(const std::string& text)
{
    std::vector<std::string> graphemes;
    std::string current;
    size_t index = 0;

    while (index < text.size())
    {
        const size_t start = index;
        char32_t codepoint = 0;

        if (DecodeNext(text, index, codepoint) == false)
        {
            break;
        }

        std::string encoded = text.substr(start, index - start);
        bool appendToCurrent = false;

        if (current.empty() == true)
        {
            appendToCurrent = true;
        }
        else if (IsCombining(codepoint) == true)
        {
            appendToCurrent = true;
        }
        else if ((codepoint >= 0xFE00U) && (codepoint <= 0xFE0FU))
        {
            appendToCurrent = true;
        }
        else if (codepoint == 0x200DU)
        {
            appendToCurrent = true;
        }
        else if (FirstCodepoint(current.substr(current.size() - 1U)) == 0x200DU)
        {
            appendToCurrent = true;
        }

        if (appendToCurrent == true)
        {
            current += encoded;
        }
        else
        {
            graphemes.push_back(current);
            current = encoded;
        }
    }

    if (current.empty() == false)
    {
        graphemes.push_back(current);
    }

    return graphemes;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  const std::string& text [in] UTF-8 grapheme cluster
///
/// @return terminal column width for the grapheme
///
/// @brief
/// Calculate the terminal cell width used by a grapheme cluster.
///
/////////////////////////////////////////////////////////////////////////////
int cUtf8Helper::GraphemeWidth(const std::string& text)
{
    int width = 0;
    size_t index = 0;

    while (index < text.size())
    {
        char32_t codepoint = 0;

        if (DecodeNext(text, index, codepoint) == false)
        {
            break;
        }

        width += CodepointWidth(codepoint);
    }

    if (width < 1)
    {
        width = 1;
    }

    if (width > 2)
    {
        width = 2;
    }

    return width;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  char32_t codepoint [in] Unicode scalar value to test
///
/// @return true when the codepoint is a combining mark
///
/// @brief
/// Identify common Unicode combining mark ranges.
///
/////////////////////////////////////////////////////////////////////////////
bool cUtf8Helper::IsCombining(char32_t codepoint)
{
    bool result = false;

    if ((codepoint >= 0x0300U) && (codepoint <= 0x036FU))
    {
        result = true;
    }
    else if ((codepoint >= 0x1AB0U) && (codepoint <= 0x1AFFU))
    {
        result = true;
    }
    else if ((codepoint >= 0x1DC0U) && (codepoint <= 0x1DFFU))
    {
        result = true;
    }
    else if ((codepoint >= 0x20D0U) && (codepoint <= 0x20FFU))
    {
        result = true;
    }
    else if ((codepoint >= 0xFE20U) && (codepoint <= 0xFE2FU))
    {
        result = true;
    }

    return result;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  char32_t codepoint [in] Unicode scalar value to test
///
/// @return true when the codepoint is a C0 or DEL control character
///
/// @brief
/// Check whether a codepoint should be handled as a control character.
///
/////////////////////////////////////////////////////////////////////////////
bool cUtf8Helper::IsControl(char32_t codepoint)
{
    bool result = false;

    if (codepoint < 0x20U)
    {
        result = true;
    }
    else if (codepoint == 0x7FU)
    {
        result = true;
    }

    return result;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  const std::string& text [in] UTF-8 text
///
/// @return first decoded codepoint or zero
///
/// @brief
/// Decode and return only the first codepoint in a string.
///
/////////////////////////////////////////////////////////////////////////////
char32_t cUtf8Helper::FirstCodepoint(const std::string& text)
{
    size_t index = 0;
    char32_t codepoint = 0;

    DecodeNext(text, index, codepoint);

    return codepoint;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  char32_t codepoint [in] Unicode scalar value
///
/// @return terminal column width for the codepoint
///
/// @brief
/// Calculate an approximate terminal width for one codepoint.
///
/////////////////////////////////////////////////////////////////////////////
int cUtf8Helper::CodepointWidth(char32_t codepoint)
{
    if (IsCombining(codepoint) == true)
    {
        return 0;
    }

    if ((codepoint >= 0xFE00U) && (codepoint <= 0xFE0FU))
    {
        return 0;
    }

    if (codepoint == 0x200DU)
    {
        return 0;
    }

    if (IsControl(codepoint) == true)
    {
        return 0;
    }

    if (IsWide(codepoint) == true)
    {
        return 2;
    }

    return 1;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  char32_t codepoint [in] Unicode scalar value
/// @return true when the codepoint is commonly displayed double-width
///
/// @brief
/// Identify common East Asian wide ranges used by terminals.
///
/////////////////////////////////////////////////////////////////////////////
bool cUtf8Helper::IsWide(char32_t codepoint)
{
    bool result = false;

    if ((codepoint >= 0x1100U) && (codepoint <= 0x115FU))
    {
        result = true;
    }
    else if ((codepoint >= 0x2E80U) && (codepoint <= 0xA4CFU))
    {
        result = true;
    }
    else if ((codepoint >= 0xAC00U) && (codepoint <= 0xD7A3U))
    {
        result = true;
    }
    else if ((codepoint >= 0xF900U) && (codepoint <= 0xFAFFU))
    {
        result = true;
    }
    else if ((codepoint >= 0xFE10U) && (codepoint <= 0xFE19U))
    {
        result = true;
    }
    else if ((codepoint >= 0xFE30U) && (codepoint <= 0xFE6FU))
    {
        result = true;
    }
    else if ((codepoint >= 0xFF00U) && (codepoint <= 0xFF60U))
    {
        result = true;
    }
    else if ((codepoint >= 0xFFE0U) && (codepoint <= 0xFFE6U))
    {
        result = true;
    }

    return result;
}

}
