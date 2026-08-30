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

#ifndef BUSY_H
#define BUSY_H

#include <QWidget>
#include <QLabel>
#include <QTimer>

class cBusyIndicator : public QLabel
{
    Q_OBJECT

    // =================================================================
    // METHODS
    // =================================================================
public:
    explicit cBusyIndicator(QWidget *parent = nullptr);

    void Start(void) ;
    void Stop(void) ;

protected:
    void paintEvent(QPaintEvent *event) override;

private slots:
    void updateIndicator(void);

    // =================================================================
    // MEMBER VARIABLES
    // =================================================================
private:
    QTimer *timer;
    int currentFrame;
    QStringList frames;

    bool started ;
};

#endif