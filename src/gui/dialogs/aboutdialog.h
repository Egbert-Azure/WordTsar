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

#ifndef ABOUTDIALOG_H
#define ABOUTDIALOG_H

#include <QDialog>
#include <QString>

class QLabel ;
class QPlainTextEdit ;

/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// About WordTsar box: the WordTsar logo on the left, a read-only
/// scrolling view of the shared About text on the right and an OK button.
/// Return and Escape close it.
///
/////////////////////////////////////////////////////////////////////////////
class cAboutDialog : public QDialog
{
public:
    // Logical width of the logo in pixels.
    static const int kLogoWidth = 140 ;

    explicit cAboutDialog(QWidget *parent = nullptr) ;

    // abouttext::AboutText() followed by "Qt " and the Qt runtime version.
    static QString Text(void) ;

    QPlainTextEdit *TextView(void) const ;
    QLabel *LogoLabel(void) const ;

private:
    QPlainTextEdit *mText = nullptr ;
    QLabel *mLogo = nullptr ;
} ;

#endif // ABOUTDIALOG_H
