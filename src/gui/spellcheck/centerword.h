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
 *
#ifndef _CENTERWORD_H_
#define _CENTERWORD_H_



////@begin includes
////@end includes


////@begin forward declarations
////@end forward declarations


////@begin control identifiers
#define ID_CENTERWORD 10007
#define ID_TEXTCTRL1 10006
#define SYMBOL_CENTERWORD_STYLE wxCAPTION|wxRESIZE_BORDER|wxSYSTEM_MENU|wxCLOSE_BOX|wxTAB_TRAVERSAL
#define SYMBOL_CENTERWORD_TITLE _("Enter Word")
#define SYMBOL_CENTERWORD_IDNAME ID_CENTERWORD
#define SYMBOL_CENTERWORD_SIZE wxSize(400, 300)
#define SYMBOL_CENTERWORD_POSITION wxDefaultPosition
////@end control identifiers



class cEnterWord: public wxDialog
{    
    DECLARE_DYNAMIC_CLASS( cEnterWord )
    DECLARE_EVENT_TABLE()

public:
    /// Constructors
    cEnterWord(void);
    cEnterWord( wxWindow* parent, wxWindowID id = SYMBOL_CENTERWORD_IDNAME, const QString& caption = SYMBOL_CENTERWORD_TITLE, const wxPoint& pos = SYMBOL_CENTERWORD_POSITION, const wxSize& size = SYMBOL_CENTERWORD_SIZE, long style = SYMBOL_CENTERWORD_STYLE );

    /// Creation
    bool Create( wxWindow* parent, wxWindowID id = SYMBOL_CENTERWORD_IDNAME, const QString& caption = SYMBOL_CENTERWORD_TITLE, const wxPoint& pos = SYMBOL_CENTERWORD_POSITION, const wxSize& size = SYMBOL_CENTERWORD_SIZE, long style = SYMBOL_CENTERWORD_STYLE );

    /// Destructor
    ~cEnterWord(void);

    /// Initialises member variables
    void Init(void);

    /// Creates the controls and sizers
    void CreateControls(void);

////@begin cEnterWord event handler declarations

////@end cEnterWord event handler declarations

////@begin cEnterWord member function declarations

    /// Retrieves bitmap resources
    wxBitmap GetBitmapResource( const QString& name );

    /// Retrieves icon resources
    wxIcon GetIconResource( const QString& name );
////@end cEnterWord member function declarations

    /// Should we show tooltips?
    static bool ShowToolTips(void);

////@begin cEnterWord member variables
    wxTextCtrl* mWord;
////@end cEnterWord member variables
};

#endif
    // _CENTERWORD_H_
*/
