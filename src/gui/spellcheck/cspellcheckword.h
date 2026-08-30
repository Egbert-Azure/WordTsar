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

/*
#ifndef _CSPELLCHECKWORD_H_
#define _CSPELLCHECKWORD_H_




////@begin includes
////@end includes



////@begin forward declarations
////@end forward declarations



////@begin control identifiers
#define ID_CSPELLCHECKWORD 10008
#define ID_TEXTCTRL1 10006
#define SYMBOL_CSPELLCHECKWORD_STYLE wxCAPTION|wxRESIZE_BORDER|wxSYSTEM_MENU|wxCLOSE_BOX|wxTAB_TRAVERSAL
#define SYMBOL_CSPELLCHECKWORD_TITLE _("Spell Check Word")
#define SYMBOL_CSPELLCHECKWORD_IDNAME ID_CSPELLCHECKWORD
#define SYMBOL_CSPELLCHECKWORD_SIZE wxSize(400, 300)
#define SYMBOL_CSPELLCHECKWORD_POSITION wxDefaultPosition
////@end control identifiers




class cSpellCheckWord: public wxDialog
{    
    DECLARE_DYNAMIC_CLASS( cSpellCheckWord )
    DECLARE_EVENT_TABLE()

public:
    /// Constructors
    cSpellCheckWord(void);
    cSpellCheckWord( wxWindow* parent, wxWindowID id = SYMBOL_CSPELLCHECKWORD_IDNAME, const QString& caption = SYMBOL_CSPELLCHECKWORD_TITLE, const wxPoint& pos = SYMBOL_CSPELLCHECKWORD_POSITION, const wxSize& size = SYMBOL_CSPELLCHECKWORD_SIZE, long style = SYMBOL_CSPELLCHECKWORD_STYLE );

    /// Creation
    bool Create( wxWindow* parent, wxWindowID id = SYMBOL_CSPELLCHECKWORD_IDNAME, const QString& caption = SYMBOL_CSPELLCHECKWORD_TITLE, const wxPoint& pos = SYMBOL_CSPELLCHECKWORD_POSITION, const wxSize& size = SYMBOL_CSPELLCHECKWORD_SIZE, long style = SYMBOL_CSPELLCHECKWORD_STYLE );

    /// Destructor
    ~cSpellCheckWord(void);

    /// Initialises member variables
    void Init(void);

    /// Creates the controls and sizers
    void CreateControls(void);

////@begin cSpellCheckWord event handler declarations

////@end cSpellCheckWord event handler declarations

////@begin cSpellCheckWord member function declarations

    /// Retrieves bitmap resources
    wxBitmap GetBitmapResource( const QString& name );

    /// Retrieves icon resources
    wxIcon GetIconResource( const QString& name );
////@end cSpellCheckWord member function declarations

    /// Should we show tooltips?
    static bool ShowToolTips(void);

////@begin cSpellCheckWord member variables
    wxTextCtrl* mWord;
////@end cSpellCheckWord member variables
};

#endif
    // _CSPELLCHECKWORD_H_

    */
