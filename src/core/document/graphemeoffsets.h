#ifndef GRAPHEMEOFFSETS_H
#define GRAPHEMEOFFSETS_H

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

#include <vector>
#include <string>
#include <cstdint>

#include "src/core/include/config.h"


class cGraphemeOffsets
{
    // =================================================================
    // METHODS
    // =================================================================
public:
    cGraphemeOffsets(void) ;

    POSITION_T operator[](size_t i) const ;

    size_t size(void) const ;

    void Store(const std::string& buffer, const std::vector<POSITION_T>& offsets) ;

    void CopyTo(std::vector<POSITION_T>& out) const ;

    void clear(void) ;
    void shrink_to_fit(void) ;

    size_t memoryUsed(void) const ;

    size_t memoryAllocated(void) const ;

    // =================================================================
    // MEMBER VARIABLES
    // =================================================================
private:
    enum eMode { MODE_EMPTY, MODE_IDENTITY, MODE_DELTA } ;

    eMode mMode ;
    size_t mCount ;                     ///< number of graphemes
    std::vector<uint8_t> mDeltas ;      ///< byte-length of each grapheme (MODE_DELTA only)
} ;


#endif // GRAPHEMEOFFSETS_H
