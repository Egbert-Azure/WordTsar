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
 * @class cSpellCheckWord
 *
 * @brief Single-word spell check dialog (legacy wxWidgets code).
 *
 * Implements the cSpellCheckWord class, a dialog for checking the spelling
 * of an individual word. Displays the misspelled word with suggestions and
 * allows the user to accept a suggestion, ignore the word, or type a
 * replacement.
 *
 * @section spellcheckword_status Current Status
 * This file contains commented-out wxWidgets code from the original
 * WordTsar implementation and is NOT currently compiled. The active
 * spell check dialog is cSpellCheck in cspellcheck.cpp (Qt-based,
 * supports both document and single-word modes).
 *
 * @author Gerald Brandt
 * @copyright GNU Affero General Public License v3.0
 *
 * @see cSpellCheckWord Legacy word spell check dialog class
 * @see cSpellCheck Active spell check dialog (Qt-based)
 */

/*
// For compilers that support precompilation, includes "wx/wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
#pragma hdrstop
#endif

#ifndef WX_PRECOMP
#include "wx/wx.h"
#endif

////@begin includes
////@end includes
#include <wx/accel.h>


#include "cspellcheckword.h"

////@begin XPM images
////@end XPM images




IMPLEMENT_DYNAMIC_CLASS( cSpellCheckWord, wxDialog )




BEGIN_EVENT_TABLE( cSpellCheckWord, wxDialog )

////@begin cSpellCheckWord event table entries
////@end cSpellCheckWord event table entries

END_EVENT_TABLE()




cSpellCheckWord::cSpellCheckWord(void)
{
    Init();
}

cSpellCheckWord::cSpellCheckWord( wxWindow* parent, wxWindowID id, const QString& caption, const wxPoint& pos, const wxSize& size, long style )
{
    Init();
    Create(parent, id, caption, pos, size, style);
}




bool cSpellCheckWord::Create( wxWindow* parent, wxWindowID id, const QString& caption, const wxPoint& pos, const wxSize& size, long style )
{
////@begin cSpellCheckWord creation
    SetExtraStyle(wxWS_EX_BLOCK_EVENTS);
    wxDialog::Create( parent, id, caption, pos, size, style );

    CreateControls();
    if (GetSizer())
    {
        GetSizer()->SetSizeHints(this);
    }
    Centre();
////@end cSpellCheckWord creation
    return true;
}




cSpellCheckWord::~cSpellCheckWord(void)
{
////@begin cSpellCheckWord destruction
////@end cSpellCheckWord destruction
}




void cSpellCheckWord::Init(void)
{
////@begin cSpellCheckWord member initialisation
    mWord = NULL;
////@end cSpellCheckWord member initialisation

    wxAcceleratorEntry entries[1];
    entries[0].Set(wxACCEL_NORMAL, WXK_RETURN, wxID_OK);
    wxAcceleratorTable accel(1, entries);
    SetAcceleratorTable(accel);

}




void cSpellCheckWord::CreateControls(void)
{
////@begin cSpellCheckWord content construction
    cSpellCheckWord* itemDialog1 = this;

    wxBoxSizer* itemBoxSizer2 = new wxBoxSizer(wxVERTICAL);
    itemDialog1->SetSizer(itemBoxSizer2);

    wxBoxSizer* itemBoxSizer3 = new wxBoxSizer(wxHORIZONTAL);
    itemBoxSizer2->Add(itemBoxSizer3, 0, wxGROW|wxALL, 5);

    wxStaticText* itemStaticText4 = new wxStaticText( itemDialog1, wxID_STATIC, _("Word to check: "), wxDefaultPosition, wxDefaultSize, 0 );
    itemBoxSizer3->Add(itemStaticText4, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);

    mWord = new wxTextCtrl( itemDialog1, ID_TEXTCTRL1, wxEmptyString, wxDefaultPosition, wxSize(200, -1), 0 );
    itemBoxSizer3->Add(mWord, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);

    wxBoxSizer* itemBoxSizer6 = new wxBoxSizer(wxVERTICAL);
    itemBoxSizer3->Add(itemBoxSizer6, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);

    wxButton* itemButton7 = new wxButton( itemDialog1, wxID_OK, _("OK"), wxDefaultPosition, wxDefaultSize, 0 );
    itemBoxSizer6->Add(itemButton7, 0, wxALIGN_CENTER_HORIZONTAL|wxALL, 5);

    wxButton* itemButton8 = new wxButton( itemDialog1, wxID_CANCEL, _("Cancel"), wxDefaultPosition, wxDefaultSize, 0 );
    itemBoxSizer6->Add(itemButton8, 0, wxALIGN_CENTER_HORIZONTAL|wxALL, 5);

////@end cSpellCheckWord content construction

    mWord->SetFocus() ;

}




bool cSpellCheckWord::ShowToolTips(void)
{
    return true;
}



wxBitmap cSpellCheckWord::GetBitmapResource( const QString& name )
{
    // Bitmap retrieval
////@begin cSpellCheckWord bitmap retrieval
    wxUnusedVar(name);
    return wxNullBitmap;
////@end cSpellCheckWord bitmap retrieval
}



wxIcon cSpellCheckWord::GetIconResource( const QString& name )
{
    // Icon retrieval
////@begin cSpellCheckWord icon retrieval
    wxUnusedVar(name);
    return wxNullIcon;
////@end cSpellCheckWord icon retrieval
}

*/
