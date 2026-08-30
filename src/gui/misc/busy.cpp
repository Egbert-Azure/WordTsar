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
 * @class cBusyIndicator
 *
 * @brief Animated busy indicator widget displayed during long operations.
 *
 * Implements the cBusyIndicator class, a QLabel-based widget that cycles
 * through Unicode block characters to create a rising/falling bar animation.
 *
 * @section busy_animation Animation Details
 * - Uses Unicode block element characters (U+2581 through U+2588) to create
 *   a vertical bar that rises and falls
 * - QTimer drives the animation at 125 ms per frame (8 frames per second)
 * - Start(): begins the animation timer and shows the widget
 * - Stop(): halts the animation timer and clears the display
 *
 * @section busy_usage Usage Context
 * Used in the WordTsar status bar to signal background activity such as
 * file loading, layout recalculation, or spell checking. The indicator
 * provides visual feedback that the application is processing without
 * blocking the UI.
 *
 * @author Gerald Brandt
 * @copyright GNU Affero General Public License v3.0
 *
 * @see cBusyIndicator Busy indicator widget class
 * @see cWordTsar Main application window hosting the status bar
 */

#include <QApplication>
#include <QPainter>

#include "busy.h"

cBusyIndicator::cBusyIndicator(QWidget *parent)
    : QLabel(parent), currentFrame(0)
{
//    frames << "| " << "/ " << "- " << "\\ "; // Define the frames for the animation
    frames << "▁ " << "▂ " << "▃ " << "▄ " << "▅ " << "▆ " << "▇ " << "█ " << "▇ " << "▆ " << "▅ " << "▄ " << "▃ " << "▂ " << "▁ " ; // Define the frames for the animation
    timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &cBusyIndicator::updateIndicator);
//    timer->start(100); // Update every 200 milliseconds

    started = false ;
}


void cBusyIndicator::Start(void)
{
    if(started == false)
    {
        started = true ;
        timer->start(125);
    }
}

void cBusyIndicator::Stop(void)
{
    timer->stop();
    started = false ;
}


void cBusyIndicator::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
//    painter.setFont(QFont("Courier", 24));
    painter.drawText(rect(), Qt::AlignCenter, frames[currentFrame]);
}

void cBusyIndicator::updateIndicator(void)
{
    currentFrame = (currentFrame + 1) % frames.size();
    update(); // Trigger a repaint
    
    QApplication::processEvents() ;
}