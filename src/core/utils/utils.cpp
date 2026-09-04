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

/**
 * @file utils.cpp
 *
 * @brief printf-style string formatting utility function.
 *
 * Implements string_sprintf(), a type-safe wrapper around vsnprintf that
 * returns a std::string. Uses a 256-byte stack buffer for short strings
 * to avoid heap allocation, and falls back to dynamic allocation for
 * longer results.
 *
 * @section utils_implementation Implementation Details
 * - First attempts formatting into a 256-byte stack-allocated buffer
 * - If the result exceeds 256 bytes, dynamically allocates a buffer of
 *   the exact required size and reformats
 * - C++17 path: writes directly into std::string's internal buffer via
 *   data() (guaranteed contiguous since C++17)
 * - C++11/C++14 fallback: uses an intermediate std::unique_ptr<char[]>
 *   buffer and constructs the string from it
 *
 * @section utils_usage Usage
 * Used throughout WordTsar for formatted string construction, particularly
 * in status bar updates, debug messages, and file format output where
 * printf-style formatting is more convenient than stream insertion.
 *
 * @author Gerald Brandt
 * @copyright GNU Affero General Public License v3.0
 *
 * @see string_sprintf() The formatting utility function
 */

#include <cstdio>
#include <cstdarg>

#include "src/core/include/utils.h"

#if __cplusplus < 201703L
#include <memory>
#endif

/////////////////////////////////////////////////////////////////////////////
///
/// @param  const char *fmt [in] printf-style format string
/// @param  ... [in] variadic arguments matching the format string
///
/// @return formatted std::string
///
/// @brief
/// Format a string using printf-style format specifiers.
/// Uses a small stack buffer for short strings, falls back to heap
/// allocation for longer results.
///
/////////////////////////////////////////////////////////////////////////////
std::string string_sprintf(const char *fmt, ...)
{
    char buf[256];

    va_list args;
    va_start(args, fmt);
    const auto r = std::vsnprintf(buf, sizeof buf, fmt, args);
    va_end(args);

    if (r < 0)
        // conversion failed
        return {};

    const size_t len = r;
    if (len < sizeof buf)
        // we fit in the buffer
        return { buf, len };

#if __cplusplus >= 201703L
    // C++17: Create a string and write to its underlying array
    std::string s(len, '\0');
    va_start(args, fmt);
    std::vsnprintf(s.data(), len+1, fmt, args);
    va_end(args);

    return s;
#else
    // C++11 or C++14: We need to allocate scratch memory
    auto vbuf = std::unique_ptr<char[]>(new char[len+1]);
    va_start(args, fmt);
    std::vsnprintf(vbuf.get(), len+1, fmt, args);
    va_end(args);

    return { vbuf.get(), len };
#endif
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  num [in] - Number to convert (1-3999)
///
/// @return Roman numeral string (lowercase)
///
/// @brief
/// Converts integer to lowercase Roman numerals.
/// Uses standard subtractive notation (iv, ix, xl, xc, cd, cm).
///
/////////////////////////////////////////////////////////////////////////////
std::string ToRomanNumeralLower(long num)
{
    if (num <= 0 || num > 3999)
    {
        return std::to_string(num);  // Fallback to Arabic for out of range
    }

    const int values[] = {1000, 900, 500, 400, 100, 90, 50, 40, 10, 9, 5, 4, 1};
    const char* numerals[] = {"m", "cm", "d", "cd", "c", "xc", "l", "xl", "x", "ix", "v", "iv", "i"};

    std::string result;
    for (int i = 0; i < 13; ++i)
    {
        while (num >= values[i])
        {
            result += numerals[i];
            num -= values[i];
        }
    }
    return result;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  num [in] - Number to convert (1-3999)
///
/// @return Roman numeral string (uppercase)
///
/// @brief
/// Converts integer to uppercase Roman numerals.
/// Uses standard subtractive notation (IV, IX, XL, XC, CD, CM).
///
/////////////////////////////////////////////////////////////////////////////
std::string ToRomanNumeralUpper(long num)
{
    if (num <= 0 || num > 3999)
    {
        return std::to_string(num);  // Fallback to Arabic for out of range
    }

    const int values[] = {1000, 900, 500, 400, 100, 90, 50, 40, 10, 9, 5, 4, 1};
    const char* numerals[] = {"M", "CM", "D", "CD", "C", "XC", "L", "XL", "X", "IX", "V", "IV", "I"};

    std::string result;
    for (int i = 0; i < 13; ++i)
    {
        while (num >= values[i])
        {
            result += numerals[i];
            num -= values[i];
        }
    }
    return result;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  fullPath  [in] - full file path to abbreviate for display
/// @param  maxWidth  [in] - maximum length of the returned string, in
///                          characters (0 = no limit, return fullPath as-is)
///
/// @return the abbreviated path
///
/// @brief
/// Recent-files style path shortening. The filename is what actually tells
/// two entries apart, so it's the last thing this ever cuts -- the directory
/// is elided from the front instead (".../Kapitel/New.ws"), falling back to
/// eliding the filename's own middle only if even that doesn't fit.
///
/////////////////////////////////////////////////////////////////////////////
std::string AbbreviatePathForDisplay(const std::string &fullPath, size_t maxWidth)
{
    if ((maxWidth == 0) || (fullPath.size() <= maxWidth))
    {
        return fullPath;
    }

    size_t lastSlash = fullPath.find_last_of('/');
    std::string filename = (lastSlash == std::string::npos) ? fullPath : fullPath.substr(lastSlash + 1);

    std::string parentName;
    if (lastSlash != std::string::npos && lastSlash != 0)
    {
        size_t prevSlash = fullPath.find_last_of('/', lastSlash - 1);
        size_t parentStart = (prevSlash == std::string::npos) ? 0 : (prevSlash + 1);
        parentName = fullPath.substr(parentStart, lastSlash - parentStart);
    }

    std::string withParent = parentName.empty() ? (".../" + filename) : (".../" + parentName + "/" + filename);
    if (withParent.size() <= maxWidth)
    {
        return withParent;
    }

    std::string bare = ".../" + filename;
    if (bare.size() <= maxWidth)
    {
        return bare;
    }

    // Even the bare filename doesn't fit -- elide its own middle, keeping
    // the head and the extension.
    if (maxWidth <= 3)
    {
        return filename.substr(0, maxWidth);
    }

    size_t keep = maxWidth - 3;
    size_t head = keep / 2;
    size_t tail = keep - head;
    if ((head + tail) >= filename.size())
    {
        return filename;
    }
    return filename.substr(0, head) + "..." + filename.substr(filename.size() - tail);
}

