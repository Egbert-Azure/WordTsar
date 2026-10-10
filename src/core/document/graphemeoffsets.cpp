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
 * (1 byte per grapheme instead of 8) plus the absolute offset of every
 * OFFSET_CHECKPOINT_INTERVAL-th grapheme, reconstructing an offset from the
 * nearest checkpoint.
 * This is a key memory optimization for cDocument's grapheme-based positioning.
 *
 * @section grapheme_modes Storage Modes
 * - MODE_EMPTY: no graphemes stored (freshly constructed or cleared)
 * - MODE_IDENTITY: pure ASCII paragraph where byte offset equals grapheme index,
 *   requiring zero additional storage and O(1) lookup
 * - MODE_DELTA: multi-byte paragraph storing per-grapheme byte lengths as uint8_t
 *   values plus a checkpoint offset every OFFSET_CHECKPOINT_INTERVAL (64)
 *   graphemes. Lookup sums at most 63 deltas from the nearest checkpoint
 *   (O(1) with respect to position); memory is 1 byte per grapheme plus
 *   8 bytes per 64 graphemes, vs. 8 bytes per grapheme for a full table
 * - MODE_FULL: used only when some grapheme is longer than 255 bytes (e.g.
 *   long combining-mark stacks), which a uint8_t delta cannot hold. Stores
 *   every grapheme's byte offset plus the buffer length, O(1) lookup.
 *
 * For typical English documents, ~90% of paragraphs use IDENTITY mode,
 * reducing memory from 8 bytes/grapheme (POSITION_T) to 0 bytes.
 * DELTA mode uses about 1.125 bytes/grapheme instead of 8.
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

#include <algorithm>
#include <cstddef>
#include <numeric>


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
/// Returns the byte offset for the given grapheme index (i == size()
/// returns the buffer length).
/// In IDENTITY mode (ASCII text), offset[i] = i, O(1).
/// In DELTA mode (multi-byte text), starts from the checkpoint at or before i
/// and adds at most OFFSET_CHECKPOINT_INTERVAL - 1 deltas.
/// In FULL mode (a grapheme longer than 255 bytes), reads the stored offset.
///
/////////////////////////////////////////////////////////////////////////////
POSITION_T cGraphemeOffsets::operator[](size_t i) const
{
    if (mMode == MODE_IDENTITY)
    {
        return static_cast<POSITION_T>(i) ;
    }

    if (mMode == MODE_FULL)
    {
        return mFull[i] ;
    }

    // DELTA mode: nearest checkpoint, then the deltas after it
    size_t block = i / OFFSET_CHECKPOINT_INTERVAL ;
    size_t first = block * OFFSET_CHECKPOINT_INTERVAL ;
    POSITION_T offset = 0 ;
    if (block > 0)
    {
        offset = mCheckpoints[block - 1] ;
    }
    return std::accumulate(mDeltas.begin() + static_cast<std::ptrdiff_t>(first),
                           mDeltas.begin() + static_cast<std::ptrdiff_t>(i), offset) ;
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
/// deltas in DELTA mode (1 byte per grapheme instead of 8), plus the byte
/// offset of every OFFSET_CHECKPOINT_INTERVAL-th grapheme (mCheckpoints).
/// If any grapheme is longer than 255 bytes, FULL mode stores every offset
/// plus the buffer length instead.
///
/////////////////////////////////////////////////////////////////////////////
void cGraphemeOffsets::Store(const std::string& buffer, const std::vector<POSITION_T>& offsets)
{
    mCount = offsets.size() ;

    mDeltas.clear() ;
    mCheckpoints.clear() ;
    mFull.clear() ;

    if (mCount == 0)
    {
        mMode = MODE_EMPTY ;
        return ;
    }

    // if every grapheme is exactly 1 byte, offset[i] == i
    if (buffer.size() == mCount)
    {
        mMode = MODE_IDENTITY ;
        return ;
    }

    // a grapheme too long for a uint8_t delta: keep the full offset table
    POSITION_T end = std::ssize(buffer) ;
    auto tooLong = [](POSITION_T from, POSITION_T to) { return (to - from) > 255 ; } ;
    if (std::adjacent_find(offsets.begin(), offsets.end(), tooLong) != offsets.end() ||
        tooLong(offsets.back(), end))
    {
        mMode = MODE_FULL ;
        mFull.assign(offsets.begin(), offsets.end()) ;
        mFull.push_back(end) ;
        return ;
    }

    // multi-byte graphemes: byte-length of each grapheme as uint8_t
    mMode = MODE_DELTA ;
    mDeltas.resize(mCount) ;
    std::transform(offsets.begin() + 1, offsets.end(), offsets.begin(), mDeltas.begin(),
                   [](POSITION_T next, POSITION_T current) { return static_cast<uint8_t>(next - current) ; }) ;
    mDeltas[mCount - 1] = static_cast<uint8_t>(end - offsets.back()) ;

    // checkpoints: running sum of the deltas at every interval boundary
    mCheckpoints.reserve(mCount / OFFSET_CHECKPOINT_INTERVAL) ;
    POSITION_T offset = 0 ;
    for (size_t first = 0 ; first + OFFSET_CHECKPOINT_INTERVAL <= mCount ; first += OFFSET_CHECKPOINT_INTERVAL)
    {
        auto blockStart = mDeltas.begin() + static_cast<std::ptrdiff_t>(first) ;
        offset = std::accumulate(blockStart, blockStart + static_cast<std::ptrdiff_t>(OFFSET_CHECKPOINT_INTERVAL), offset) ;
        mCheckpoints.push_back(offset) ;
    }
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
        std::iota(out.begin(), out.end(), POSITION_T(0)) ;
    }
    else if (mMode == MODE_DELTA)
    {
        // prefix sum of deltas
        std::exclusive_scan(mDeltas.begin(), mDeltas.end(), out.begin(), POSITION_T(0)) ;
    }
    else if (mMode == MODE_FULL)
    {
        std::copy_n(mFull.begin(), mCount, out.begin()) ;
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
    mCheckpoints.clear() ;
    mFull.clear() ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Releases excess allocated memory in the delta, checkpoint and full
/// offset vectors.
///
/////////////////////////////////////////////////////////////////////////////
void cGraphemeOffsets::shrink_to_fit(void)
{
    mDeltas.shrink_to_fit() ;
    mCheckpoints.shrink_to_fit() ;
    mFull.shrink_to_fit() ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return bytes currently in use
///
/// @brief
/// Returns the number of bytes actually used by stored data.
/// IDENTITY mode uses 0 bytes. DELTA mode uses 1 byte per grapheme plus
/// one POSITION_T per OFFSET_CHECKPOINT_INTERVAL graphemes. FULL mode uses
/// one POSITION_T per grapheme plus one for the buffer length.
///
/////////////////////////////////////////////////////////////////////////////
size_t cGraphemeOffsets::memoryUsed(void) const
{
    return mDeltas.size() * sizeof(uint8_t) + (mCheckpoints.size() + mFull.size()) * sizeof(POSITION_T) ;
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
    return mDeltas.capacity() * sizeof(uint8_t) + (mCheckpoints.capacity() + mFull.capacity()) * sizeof(POSITION_T) ;
}
