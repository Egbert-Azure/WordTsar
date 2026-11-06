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

#include "doctest.h"

#include "src/gui/dialogs/aboutdialog.h"
#include "src/core/utils/abouttext.h"

#include <QApplication>
#include <QKeyEvent>
#include <QLabel>
#include <QPixmap>
#include <QPlainTextEdit>

#include <string>
#include <vector>

namespace
{

int argc = 0 ;
char *argv[] = {nullptr} ;

void EnsureQApplication(void)
{
    if (QApplication::instance() == nullptr)
    {
        new QApplication(argc, argv) ;
    }
}

// Sends a key press and release to widget.
void PressKey(QWidget *widget, int key)
{
    QKeyEvent press(QEvent::KeyPress, key, Qt::NoModifier) ;
    QApplication::sendEvent(widget, &press) ;
    QKeyEvent release(QEvent::KeyRelease, key, Qt::NoModifier) ;
    QApplication::sendEvent(widget, &release) ;
}

}

TEST_CASE("About dialog shows the shared About text and the legal lines")
{
    EnsureQApplication() ;
    cAboutDialog dialog ;

    CHECK(dialog.windowTitle() == QString("About WordTsar")) ;

    QPlainTextEdit *view = dialog.TextView() ;
    REQUIRE(view != nullptr) ;
    CHECK(dialog.findChild<QPlainTextEdit *>("aboutText") == view) ;
    CHECK(view->isReadOnly() == true) ;

    std::string shown = view->toPlainText().toStdString() ;
    CHECK(view->toPlainText() == cAboutDialog::Text()) ;
    CHECK(shown.rfind(abouttext::AboutText(), 0) == 0) ;
    for (const std::string &line : abouttext::LegalLines())
    {
        CHECK(shown.find(line) != std::string::npos) ;
    }
    for (const std::string &line : abouttext::ProductLines())
    {
        CHECK(shown.find(line) != std::string::npos) ;
    }
}

TEST_CASE("About dialog shows the WordTsar logo 160 pixels wide")
{
    EnsureQApplication() ;
    cAboutDialog dialog ;

    QLabel *logo = dialog.LogoLabel() ;
    REQUIRE(logo != nullptr) ;
    CHECK(dialog.findChild<QLabel *>("aboutLogo") == logo) ;

    QPixmap pixmap = logo->pixmap() ;
    REQUIRE(pixmap.isNull() == false) ;
    CHECK(pixmap.deviceIndependentSize().width() == doctest::Approx(160.0)) ;
    CHECK(pixmap.width() == qRound(160.0 * pixmap.devicePixelRatio())) ;
}

TEST_CASE("About dialog closes on Return and on Escape")
{
    EnsureQApplication() ;
    {
        cAboutDialog dialog ;
        dialog.show() ;
        REQUIRE(dialog.TextView() != nullptr) ;
        PressKey(dialog.TextView(), Qt::Key_Return) ;
        CHECK(dialog.isVisible() == false) ;
        CHECK(dialog.result() == QDialog::Accepted) ;
    }
    {
        cAboutDialog dialog ;
        dialog.show() ;
        REQUIRE(dialog.TextView() != nullptr) ;
        PressKey(dialog.TextView(), Qt::Key_Escape) ;
        CHECK(dialog.isVisible() == false) ;
        CHECK(dialog.result() == QDialog::Rejected) ;
    }
}
