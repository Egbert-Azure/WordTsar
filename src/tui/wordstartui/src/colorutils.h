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

#ifndef WORDTSAR_TUI_COLORUTILS_H
#define WORDTSAR_TUI_COLORUTILS_H

#include "tuidefs.h"

namespace wordstartui
{

struct sHsv
{
    double h;
    double s;
    double v;
};

enum eColorLevel
{
    COLOR_LEVEL_16,
    COLOR_LEVEL_256,
    COLOR_LEVEL_TRUECOLOR
};

sHsv RgbToHsv(const sColor& color);
sColor HsvToRgb(const sHsv& hsv);
int RgbTo256(const sColor& color);
sColor Index256ToRgb(int index);
eColor RgbTo16(const sColor& color);
eColorLevel DetectColorLevel(const char* colorterm, const char* term, int tputColors);
std::string AnsiColorParams(const sColor& color, bool foreground, eColorLevel level);

}

#endif
