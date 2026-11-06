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
#include <QtGlobal>

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

// Number of non-overlapping occurrences of part in text.
int Count(const std::string &text, const std::string &part)
{
    int count = 0 ;
    for (size_t at = text.find(part) ; at != std::string::npos ; at = text.find(part, at + part.size()))
    {
        ++count ;
    }
    return count ;
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

TEST_CASE("About dialog shows the shared About text and each notice once")
{
    EnsureQApplication() ;
    cAboutDialog dialog ;

    CHECK(dialog.windowTitle() == QString("About WordTsar")) ;

    QPlainTextEdit *view = dialog.TextView() ;
    REQUIRE(view != nullptr) ;
    CHECK(dialog.findChild<QPlainTextEdit *>("aboutText") == view) ;
    CHECK(view->isReadOnly() == true) ;

    std::string shown = view->toPlainText().toStdString() ;
    std::string shared = abouttext::AboutText() ;
    CHECK(view->toPlainText() == cAboutDialog::Text()) ;
    CHECK(shown.rfind(shared, 0) == 0) ;
    for (const std::string &line : abouttext::LegalLines())
    {
        INFO(line) ;
        CHECK(Count(shown, line) == Count(shared, line)) ;
        CHECK(Count(shown, line) <= 1) ;
    }
    for (const std::string &line : abouttext::ProductLines())
    {
        INFO(line) ;
        CHECK(Count(shown, line) == 1) ;
    }
    CHECK(Count(shown, "This program comes with ABSOLUTELY NO WARRANTY.") == 1) ;
    CHECK(Count(shown, "GNU Affero General Public License v3.0") == 1) ;
    CHECK(Count(shown, "GNU AGPL") == 0) ;
    CHECK(Count(shown, "github.com/Egbert-Azure/WordTsar") == 1) ;
    CHECK(Count(shown, abouttext::TrademarkText()) == 1) ;
}

TEST_CASE("About dialog ends with the Qt runtime version")
{
    EnsureQApplication() ;
    cAboutDialog dialog ;

    std::string shown = dialog.TextView()->toPlainText().toStdString() ;
    std::string qt = std::string("Qt ") + qVersion() ;
    std::string last = "\n" + qt ;
    REQUIRE(shown.size() >= last.size()) ;
    CHECK(shown.compare(shown.size() - last.size(), last.size(), last) == 0) ;
    CHECK(Count(shown, qt) == 1) ;
    CHECK(Count(abouttext::AboutText(), "Qt ") == 0) ;
}

TEST_CASE("About dialog shows the WordTsar logo 140 pixels wide")
{
    EnsureQApplication() ;
    cAboutDialog dialog ;

    QLabel *logo = dialog.LogoLabel() ;
    REQUIRE(logo != nullptr) ;
    CHECK(dialog.findChild<QLabel *>("aboutLogo") == logo) ;

    QPixmap pixmap = logo->pixmap() ;
    REQUIRE(pixmap.isNull() == false) ;
    CHECK(pixmap.deviceIndependentSize().width() == doctest::Approx(140.0)) ;
    CHECK(pixmap.width() == qRound(140.0 * pixmap.devicePixelRatio())) ;
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
