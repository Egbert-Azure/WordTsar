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

#ifndef COVERLAYWIDGET_H
#define COVERLAYWIDGET_H

#include <QtGui>
#include <QWidget>

class cOverlayWidget : public QWidget
{
    // =================================================================
    // METHODS
    // =================================================================
public:
    cOverlayWidget(QWidget * inparent = {}) ;

protected:
    //! Catches resize and child events from the parent widget
    bool eventFilter(QObject * obj, QEvent * ev) override ;

    //! Tracks parent widget changes
    bool event(QEvent* ev) override ;

private:
    void newParent(void) ;

    // =================================================================
    // MEMBER VARIABLES
    // =================================================================
protected:
    QWidget *parent ;
};

#endif // COVERLAYWIDGET_H
