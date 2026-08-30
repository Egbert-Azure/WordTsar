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
 * @class cGraphemeOffsets
 *
 * @brief Compact grapheme-to-byte offset mapping for paragraph text buffers.
 *
 * Implements the cGraphemeOffsets class, which maps grapheme indices to byte
 * offsets within a paragraph's UTF-8 buffer using minimal memory. For pure
 * ASCII paragraphs, IDENTITY mode is used (zero storage, O(1) lookup since
 * byte offset equals grapheme index). For paragraphs containing multi-byte
 * characters, DELTA mode stores per-grapheme byte lengths as uint8_t values
 * (1 byte per grapheme instead of 8), reconstructing offsets via prefix sum.
 * This is a key memory optimization for cDocument's grapheme-based positioning.
 *
 * @section grapheme_modes Storage Modes
 * - MODE_EMPTY: no graphemes stored (freshly constructed or cleared)
 * - MODE_IDENTITY: pure ASCII paragraph where byte offset equals grapheme index,
 *   requiring zero additional storage and O(1) lookup
 * - MODE_DELTA: multi-byte paragraph storing per-grapheme byte lengths as uint8_t
 *   values, reconstructing absolute offsets via prefix sum (O(n) lookup but
 *   minimal memory: 1 byte per grapheme vs. 8 bytes for a full offset table)
 *
 * For typical English documents, ~90% of paragraphs use IDENTITY mode,
 * reducing memory from 8 bytes/grapheme (POSITION_T) to 0 bytes.
 * DELTA mode uses 1 byte/grapheme instead of 8.
 *
 * @section grapheme_usage Usage Pattern
 * cDocument creates and populates cGraphemeOffsets for each sParagraphData during
 * text insertion and deletion. The Build() method analyzes a UTF-8 buffer and
 * chooses the optimal mode automatically. ByteOffset() converts a grapheme index
 * to a byte offset for accessing the raw paragraph buffer.
 *
 * @author Gerald Brandt
 * @copyright GNU Affero General Public License v3.0
 *
 * @see cDocument Document model that owns these offset tables
 * @see sParagraphData Paragraph storage containing the UTF-8 buffer
 */

#include "graphemeoffsets.h"


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Default constructor. Starts in EMPTY mode with zero graphemes.
///
/////////////////////////////////////////////////////////////////////////////
cGraphemeOffsets::cGraphemeOffsets(void)
    : mMode(MODE_EMPTY)
    , mCount(0)
{
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  i [in] grapheme index (0-based)
///
/// @return byte offset of grapheme i within the paragraph's UTF-8 buffer
///
/// @brief
/// Returns the byte offset for the given grapheme index.
/// In IDENTITY mode (ASCII text), offset[i] = i, O(1).
/// In DELTA mode (multi-byte text), reconstructs via prefix sum, O(i).
///
/////////////////////////////////////////////////////////////////////////////
POSITION_T cGraphemeOffsets::operator[](size_t i) const
{
    if (mMode == MODE_IDENTITY)
    {
        return static_cast<POSITION_T>(i) ;
    }

    // DELTA mode: prefix sum of byte-lengths up to position i
    POSITION_T offset = 0 ;
    for (size_t j = 0 ; j < i ; ++j)
    {
        offset += mDeltas[j] ;
    }
    return offset ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return number of graphemes stored
///
/// @brief
/// Returns the grapheme count. O(1) in all modes.
///
/////////////////////////////////////////////////////////////////////////////
size_t cGraphemeOffsets::size(void) const
{
    return mCount ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  buffer [in] the paragraph's UTF-8 text buffer
/// @param  offsets [in] raw grapheme byte offsets from GraphemeCount()
///
/// @return nothing
///
/// @brief
/// Stores grapheme offsets in the most compact representation.
/// If buffer.size() == offsets.size(), all graphemes are single-byte
/// (ASCII/Latin-1) and we use IDENTITY mode (zero storage).
/// Otherwise, we compute per-grapheme byte-lengths and store as uint8_t
/// deltas in DELTA mode (1 byte per grapheme instead of 8).
///
/////////////////////////////////////////////////////////////////////////////
void cGraphemeOffsets::Store(const std::string& buffer, const std::vector<POSITION_T>& offsets)
{
    mCount = offsets.size() ;

    if (mCount == 0)
    {
        mMode = MODE_EMPTY ;
        mDeltas.clear() ;
        return ;
    }

    // if every grapheme is exactly 1 byte, offset[i] == i
    if (buffer.size() == mCount)
    {
        mMode = MODE_IDENTITY ;
        mDeltas.clear() ;
        return ;
    }

    // multi-byte graphemes: store byte-length of each grapheme as uint8_t
    mMode = MODE_DELTA ;
    mDeltas.resize(mCount) ;

    for (size_t i = 0 ; i < mCount - 1 ; ++i)
    {
        POSITION_T delta = offsets[i + 1] - offsets[i] ;
        // clamp to 255 for safety (should never exceed 4 for normal UTF-8,
        // but combining character sequences could theoretically be longer)
        mDeltas[i] = static_cast<uint8_t>(delta > 255 ? 255 : delta) ;
    }

    // last grapheme: byte-length from last offset to end of buffer
    POSITION_T lastDelta = static_cast<POSITION_T>(buffer.size()) - offsets[mCount - 1] ;
    mDeltas[mCount - 1] = static_cast<uint8_t>(lastDelta > 255 ? 255 : lastDelta) ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  out [out] vector to populate with byte offsets
///
/// @return nothing
///
/// @brief
/// Exports the stored offsets as a std::vector<POSITION_T>.
/// Used by GetParagraphGraphemeOffsets() and GetParagraphGraphemes()
/// for compatibility with existing code paths.
///
/////////////////////////////////////////////////////////////////////////////
void cGraphemeOffsets::CopyTo(std::vector<POSITION_T>& out) const
{
    out.resize(mCount) ;

    if (mMode == MODE_IDENTITY)
    {
        // offset[i] = i
        for (size_t i = 0 ; i < mCount ; ++i)
        {
            out[i] = static_cast<POSITION_T>(i) ;
        }
    }
    else if (mMode == MODE_DELTA)
    {
        // prefix sum of deltas
        POSITION_T offset = 0 ;
        for (size_t i = 0 ; i < mCount ; ++i)
        {
            out[i] = offset ;
            offset += mDeltas[i] ;
        }
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Clears all stored data. Sets mode to EMPTY with zero graphemes.
///
/////////////////////////////////////////////////////////////////////////////
void cGraphemeOffsets::clear(void)
{
    mMode = MODE_EMPTY ;
    mCount = 0 ;
    mDeltas.clear() ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Releases excess allocated memory in the delta vector.
///
/////////////////////////////////////////////////////////////////////////////
void cGraphemeOffsets::shrink_to_fit(void)
{
    mDeltas.shrink_to_fit() ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return bytes currently in use
///
/// @brief
/// Returns the number of bytes actually used by stored data.
/// IDENTITY mode uses 0 bytes. DELTA mode uses 1 byte per grapheme.
///
/////////////////////////////////////////////////////////////////////////////
size_t cGraphemeOffsets::memoryUsed(void) const
{
    return mDeltas.size() * sizeof(uint8_t) ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return bytes allocated (capacity)
///
/// @brief
/// Returns the number of bytes allocated for stored data.
/// Includes vector capacity overhead from growth strategy.
///
/////////////////////////////////////////////////////////////////////////////
size_t cGraphemeOffsets::memoryAllocated(void) const
{
    return mDeltas.capacity() * sizeof(uint8_t) ;
}
