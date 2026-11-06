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

#include "aboutdialog.h"

#include "src/core/utils/abouttext.h"

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

#include <string>

/////////////////////////////////////////////////////////////////////////////
///
/// @param  parent [in] parent widget
///
/// @brief
/// Builds the logo, the text view and the OK button. The logo is scaled
/// smoothly to kLogoWidth logical pixels at the screen's device pixel ratio.
///
/////////////////////////////////////////////////////////////////////////////
cAboutDialog::cAboutDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("About WordTsar") ;

    mLogo = new QLabel(this) ;
    mLogo->setObjectName("aboutLogo") ;
    mLogo->setAlignment(Qt::AlignTop | Qt::AlignHCenter) ;
    QPixmap source(":/gui/images/wordtsar-logo.png") ;
    if (source.isNull() == false)
    {
        qreal ratio = devicePixelRatioF() ;
        QPixmap logo = source.scaledToWidth(qRound(kLogoWidth * ratio), Qt::SmoothTransformation) ;
        logo.setDevicePixelRatio(ratio) ;
        mLogo->setPixmap(logo) ;
    }

    mText = new QPlainTextEdit(this) ;
    mText->setObjectName("aboutText") ;
    mText->setReadOnly(true) ;
    mText->setPlainText(Text()) ;
    mText->setMinimumWidth(380) ;

    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Ok, this) ;
    QPushButton *ok = buttons->button(QDialogButtonBox::Ok) ;
    ok->setDefault(true) ;
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept) ;

    QHBoxLayout *content = new QHBoxLayout ;
    content->addWidget(mLogo) ;
    content->addWidget(mText, 1) ;

    QVBoxLayout *layout = new QVBoxLayout(this) ;
    layout->addLayout(content, 1) ;
    layout->addWidget(buttons) ;

    ok->setFocus() ;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return abouttext::AboutText() followed by every abouttext::LegalLines()
///         line it does not already contain
///
/////////////////////////////////////////////////////////////////////////////
QString cAboutDialog::Text(void)
{
    std::string text = abouttext::AboutText() ;
    std::string missing ;
    for (const std::string &line : abouttext::LegalLines())
    {
        if (text.find(line) == std::string::npos)
        {
            missing += "\n" + line ;
        }
    }
    if (missing.empty() == false)
    {
        text += "\n" + missing ;
    }
    return QString::fromStdString(text) ;
}

QPlainTextEdit *cAboutDialog::TextView(void) const
{
    return mText ;
}

QLabel *cAboutDialog::LogoLabel(void) const
{
    return mLogo ;
}
