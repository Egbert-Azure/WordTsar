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

#include "src/tui/wordstartui/src/colorfield2d.h"
#include "src/tui/wordstartui/src/huebar.h"

using namespace wordstartui;

/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Tests for the color picker widgets cColorField2D (saturation/value square)
/// and cHueBar (hue slider): mouse and keyboard selection behavior.
///
/////////////////////////////////////////////////////////////////////////////

static sInputEvent MouseAt(int row, int col)
{
    sInputEvent event{};
    event.type = INPUT_TYPE_MOUSE;
    event.mouseButton = MOUSE_BUTTON_LEFT;
    event.mouseAction = MOUSE_ACTION_PRESS;
    event.mouseRow = row;
    event.mouseCol = col;
    return event;
}

static sInputEvent SpecialKey(eSpecialKey key)
{
    sInputEvent event{};
    event.type = INPUT_TYPE_SPECIAL;
    event.special = key;
    return event;
}

TEST_CASE("cColorField2D - mouse and keys move selection")
{
    sRect bounds;
    bounds.row = 0;
    bounds.col = 0;
    bounds.rows = 10;
    bounds.cols = 10;

    cColorField2D field(bounds);
    bool changed = false;
    field.SetOnChange([&changed](void) { changed = true; });

    // Top-right corner selects full saturation and full value.
    CHECK(field.HandleEvent(MouseAt(0, 9)) == true);
    CHECK(changed == true);
    CHECK(field.GetSaturation() == doctest::Approx(1.0));
    CHECK(field.GetValue() == doctest::Approx(1.0));

    // Bottom-left corner selects zero saturation and zero value.
    field.HandleEvent(MouseAt(9, 0));
    CHECK(field.GetSaturation() == doctest::Approx(0.0));
    CHECK(field.GetValue() == doctest::Approx(0.0));

    // Arrow keys nudge the selection.
    field.SetSaturationValue(0.5, 0.5);
    field.HandleEvent(SpecialKey(SPECIAL_KEY_ARROW_RIGHT));
    CHECK(field.GetSaturation() > 0.5);

    field.SetSaturationValue(0.5, 0.5);
    field.HandleEvent(SpecialKey(SPECIAL_KEY_ARROW_DOWN));
    CHECK(field.GetValue() < 0.5);
}

TEST_CASE("cColorField2D - selection clamps to range")
{
    sRect bounds;
    bounds.row = 0;
    bounds.col = 0;
    bounds.rows = 5;
    bounds.cols = 5;

    cColorField2D field(bounds);
    field.SetSaturationValue(0.0, 0.0);

    field.HandleEvent(SpecialKey(SPECIAL_KEY_ARROW_LEFT));
    CHECK(field.GetSaturation() == doctest::Approx(0.0));

    field.HandleEvent(SpecialKey(SPECIAL_KEY_ARROW_DOWN));
    CHECK(field.GetValue() == doctest::Approx(0.0));
}

TEST_CASE("cHueBar - mouse and keys change hue")
{
    sRect bounds;
    bounds.row = 0;
    bounds.col = 0;
    bounds.rows = 13;
    bounds.cols = 1;

    cHueBar bar(bounds);
    bool changed = false;
    bar.SetOnChange([&changed](void) { changed = true; });

    bar.HandleEvent(MouseAt(0, 0));
    CHECK(changed == true);
    CHECK(bar.GetHue() == doctest::Approx(0.0));

    bar.HandleEvent(MouseAt(12, 0));
    CHECK(bar.GetHue() == doctest::Approx(360.0));

    bar.SetHue(100.0);
    bar.HandleEvent(SpecialKey(SPECIAL_KEY_ARROW_UP));
    CHECK(bar.GetHue() < 100.0);
}
