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
 * @class QColorPickerButton
 *
 * @brief QPushButton subclass that displays and picks a color via QColorDialog.
 *
 * Implements the QColorPickerButton class, a flat push button whose background
 * reflects the currently selected QColor. Clicking the button opens a standard
 * QColorDialog; the chosen color is applied to the button background and
 * stored for retrieval.
 *
 * @section colorpicker_usage Usage Context
 * Used in the system preferences dialog for configuring editor display colors
 * (background, text, highlight, selection, control codes, etc.). Each color
 * preference has its own QColorPickerButton instance showing the current
 * color value as a visual swatch.
 *
 * @section colorpicker_behavior Interaction
 * - SetColor(): programmatically sets the displayed color and updates the
 *   button stylesheet to match
 * - GetColor(): returns the currently selected QColor
 * - Click handler: opens QColorDialog, applies the chosen color if accepted
 *
 * @author Gerald Brandt
 * @copyright GNU Affero General Public License v3.0
 *
 * @see QColorPickerButton Color picker button class
 */

#include <QColorDialog>

#include "qcolorpickerbutton.h"

#define UNUSED_ARGUMENT(x) (void)x          // also used for unused locals


QColorPickerButton::QColorPickerButton(QWidget *parent) : QPushButton(parent)
{
    setFlat(true) ;
}


void QColorPickerButton::setColor(QColor color)
{
    setFlat(true) ;

    QPalette pal = palette();
    pal.setColor(QPalette::Button, color);
    setAutoFillBackground(true);
    setPalette(pal);
    update();

    mColor = color ;
}

QColor QColorPickerButton::color(void)
{
    return mColor ;
}

void QColorPickerButton::mousePressEvent(QMouseEvent *event)
{
    UNUSED_ARGUMENT(event) ;
    QColor chosen = QColorDialog::getColor(mColor, this, tr("Select Color")) ;

    if(chosen.isValid() == true)
    {
        setColor(chosen) ;
    }
}
