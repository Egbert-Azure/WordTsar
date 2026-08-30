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

#include "doctest.h"
#include "src/tui/layout/tuifontutils.h"
#include "src/gui/utils/fontutils.h"

#include <QFont>


TEST_CASE("TUIFontUtils - Descriptor round-trip")
{
    sTUIFontInfo original;
    original.name = "Courier New";
    original.size = 12;
    original.bold = true;
    original.italic = false;
    original.underline = true;

    std::string descriptor = TUIFontUtils::FontToDescriptor(original);

    // Should be "Courier New|12.0|1|0|1|0|0"
    CHECK(descriptor == "Courier New|12.0|1|0|1|0|0");

    sTUIFontInfo parsed = TUIFontUtils::FontInfoFromDescriptor(descriptor);
    CHECK(parsed.name == "Courier New");
    CHECK(parsed.size == 12);
    CHECK(parsed.bold == true);
    CHECK(parsed.italic == false);
    CHECK(parsed.underline == true);
}


TEST_CASE("TUIFontUtils - Empty descriptor returns defaults")
{
    sTUIFontInfo info = TUIFontUtils::FontInfoFromDescriptor("");
    CHECK(info.size == 12);     // Default from sTUIFontInfo constructor
    CHECK(info.bold == false);
    CHECK(info.italic == false);
    CHECK(info.underline == false);
}


TEST_CASE("TUIFontUtils - Convenience overload")
{
    // Verify convenience overload matches sTUIFontInfo overload
    std::string descriptor = TUIFontUtils::FontToDescriptor(
        "Times New Roman", 10.5, true, true, false);

    CHECK(descriptor == "Times New Roman|10.0|1|1|0|0|0");

    // Verify fields can be parsed back
    sTUIFontInfo info = TUIFontUtils::FontInfoFromDescriptor(descriptor);
    CHECK(info.name == "Times New Roman");
    CHECK(info.size == 10);
    CHECK(info.bold == true);
    CHECK(info.italic == true);
    CHECK(info.underline == false);
}


TEST_CASE("TUIFontUtils - GetFamilyFromDescriptor")
{
    CHECK(TUIFontUtils::GetFamilyFromDescriptor("Courier New|12.0|0|0|0|0|0") == "Courier New");
    CHECK(TUIFontUtils::GetFamilyFromDescriptor("Arial|10.0|1|0|0|0|0") == "Arial");
    CHECK(TUIFontUtils::GetFamilyFromDescriptor("") == "");
    CHECK(TUIFontUtils::GetFamilyFromDescriptor("NoDelimiters") == "NoDelimiters");
}


TEST_CASE("TUIFontUtils - GetSizeFromDescriptor")
{
    CHECK(TUIFontUtils::GetSizeFromDescriptor("Courier New|12.0|0|0|0|0|0") == doctest::Approx(12.0));
    CHECK(TUIFontUtils::GetSizeFromDescriptor("Arial|8.5|0|0|0|0|0") == doctest::Approx(8.5));
    CHECK(TUIFontUtils::GetSizeFromDescriptor("") == doctest::Approx(12.0));  // Default
}


TEST_CASE("TUIFontUtils - IsBoldInDescriptor")
{
    CHECK(TUIFontUtils::IsBoldInDescriptor("Courier New|12.0|1|0|0|0|0") == true);
    CHECK(TUIFontUtils::IsBoldInDescriptor("Courier New|12.0|0|0|0|0|0") == false);
    CHECK(TUIFontUtils::IsBoldInDescriptor("") == false);
}


TEST_CASE("TUIFontUtils - IsItalicInDescriptor")
{
    CHECK(TUIFontUtils::IsItalicInDescriptor("Courier New|12.0|0|1|0|0|0") == true);
    CHECK(TUIFontUtils::IsItalicInDescriptor("Courier New|12.0|0|0|0|0|0") == false);
    CHECK(TUIFontUtils::IsItalicInDescriptor("") == false);
}


TEST_CASE("TUIFontUtils - All style flags set")
{
    sTUIFontInfo font;
    font.name = "Liberation Serif";
    font.size = 14;
    font.bold = true;
    font.italic = true;
    font.underline = true;

    std::string descriptor = TUIFontUtils::FontToDescriptor(font);
    CHECK(descriptor == "Liberation Serif|14.0|1|1|1|0|0");

    sTUIFontInfo parsed = TUIFontUtils::FontInfoFromDescriptor(descriptor);
    CHECK(parsed.name == "Liberation Serif");
    CHECK(parsed.size == 14);
    CHECK(parsed.bold == true);
    CHECK(parsed.italic == true);
    CHECK(parsed.underline == true);
}


TEST_CASE("TUIFontUtils - No style flags set")
{
    sTUIFontInfo font;
    font.name = "Arial";
    font.size = 10;
    font.bold = false;
    font.italic = false;
    font.underline = false;

    std::string descriptor = TUIFontUtils::FontToDescriptor(font);
    CHECK(descriptor == "Arial|10.0|0|0|0|0|0");
}


TEST_CASE("TUIFontUtils - GUI compatibility: TUI produces same format as GUI FontUtils")
{
    // Create a QFont with known properties
    QFont qfont("Courier New", 12);
    qfont.setBold(true);
    qfont.setItalic(false);
    qfont.setUnderline(true);

    // Get GUI descriptor
    std::string guiDescriptor = FontUtils::FontToDescriptor(qfont);

    // Create matching TUI descriptor
    sTUIFontInfo tuiFont;
    tuiFont.name = qfont.family().toStdString();
    tuiFont.size = static_cast<int>(qfont.pointSizeF());
    tuiFont.bold = (qfont.weight() >= QFont::Bold);
    tuiFont.italic = qfont.italic();
    tuiFont.underline = qfont.underline();
    std::string tuiDescriptor = TUIFontUtils::FontToDescriptor(tuiFont);

    // Both should produce the same descriptor string
    CHECK(guiDescriptor == tuiDescriptor);
}


TEST_CASE("TUIFontUtils - GUI compatibility: TUI parses GUI descriptors correctly")
{
    // Create a descriptor using the GUI
    QFont qfont("Times New Roman", 14);
    qfont.setBold(false);
    qfont.setItalic(true);
    qfont.setUnderline(false);
    std::string guiDescriptor = FontUtils::FontToDescriptor(qfont);

    // Parse it with the TUI
    sTUIFontInfo tuiParsed = TUIFontUtils::FontInfoFromDescriptor(guiDescriptor);

    // Values should match
    CHECK(tuiParsed.name == qfont.family().toStdString());
    CHECK(tuiParsed.size == static_cast<int>(qfont.pointSizeF()));
    CHECK(tuiParsed.bold == (qfont.weight() >= QFont::Bold));
    CHECK(tuiParsed.italic == qfont.italic());
    CHECK(tuiParsed.underline == qfont.underline());
}


TEST_CASE("TUIFontUtils - GUI compatibility: GUI parses TUI descriptors correctly")
{
    // Create a descriptor using the TUI
    sTUIFontInfo tuiFont;
    tuiFont.name = "Arial";
    tuiFont.size = 11;
    tuiFont.bold = true;
    tuiFont.italic = true;
    tuiFont.underline = false;
    std::string tuiDescriptor = TUIFontUtils::FontToDescriptor(tuiFont);

    // Parse it with the GUI
    QFont guiParsed = FontUtils::FontFromDescriptor(tuiDescriptor);

    // Values should match
    CHECK(guiParsed.family().toStdString() == tuiFont.name);
    CHECK(static_cast<int>(guiParsed.pointSizeF()) == tuiFont.size);
    CHECK((guiParsed.weight() >= QFont::Bold) == tuiFont.bold);
    CHECK(guiParsed.italic() == tuiFont.italic);
    CHECK(guiParsed.underline() == tuiFont.underline);
}


TEST_CASE("TUIFontUtils - Fractional font sizes")
{
    // The descriptor format preserves one decimal place
    sTUIFontInfo font;
    font.name = "Courier New";
    font.size = 8;    // int size, but descriptor shows 8.0
    font.bold = false;
    font.italic = false;
    font.underline = false;

    std::string descriptor = TUIFontUtils::FontToDescriptor(font);
    CHECK(descriptor == "Courier New|8.0|0|0|0|0|0");

    // Size is stored as int in sTUIFontInfo, so fractional part is lost on round-trip
    // This matches sTUIFontInfo::size being int, unlike QFont which has pointSizeF()
    double parsedSize = TUIFontUtils::GetSizeFromDescriptor(descriptor);
    CHECK(parsedSize == doctest::Approx(8.0));
}


TEST_CASE("TUIFontUtils - Malformed descriptor with too few fields")
{
    // Only 3 fields (need at least 5)
    sTUIFontInfo info = TUIFontUtils::FontInfoFromDescriptor("Arial|12.0|1");
    CHECK(info.size == 12);  // Default -- not enough fields to parse
    CHECK(info.bold == false);  // Default -- not parsed
}


TEST_CASE("TUIFontUtils - Descriptor with extra fields")
{
    // 8 fields instead of 7 -- should still work (extra field ignored)
    std::string descriptor = "Arial|12.0|1|0|1|0|0|extra";
    sTUIFontInfo info = TUIFontUtils::FontInfoFromDescriptor(descriptor);
    CHECK(info.name == "Arial");
    CHECK(info.size == 12);
    CHECK(info.bold == true);
    CHECK(info.italic == false);
    CHECK(info.underline == true);
}
