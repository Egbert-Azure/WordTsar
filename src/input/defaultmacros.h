#ifndef DEFAULTMACROS_H
#define DEFAULTMACROS_H

//////////////////////////////////////////////////////////////////////////////
//
// WordTsar - Wordstar clone for modern systems http://wordtsar.ca
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

#include <cstddef>

/// @ingroup Input
/// @{

/// One default macro file: name without ".txt" and the file bytes.
struct sDefaultMacro
{
    const char* name;
    const char* data;
    size_t size;
};

/// The default macros, generated at build time from templates/macros
/// (defaultmacros.cpp in the build folder, see cmake/sources.cmake).
extern const sDefaultMacro kDefaultMacros[];
extern const size_t kDefaultMacroCount;

/// @}

#endif // DEFAULTMACROS_H
