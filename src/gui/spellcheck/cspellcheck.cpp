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
 * @class cSpellCheck
 *
 * @brief Interactive spell check dialog for document and word checking.
 *
 * Implements the cSpellCheck class, a QDialog that drives the spell checking
 * workflow. This is the active spell check UI for the GUI interface.
 *
 * @section spellcheck_modes Checking Modes
 * - Full-document (SPELLMODE_DOCUMENT): walks paragraphs from the current
 *   cursor position to the end of the document, checking each word
 * - Single-word (SPELLMODE_WORD): checks just the word under the cursor
 *
 * @section spellcheck_ui Dialog Interface
 * - Misspelled word display with context from the surrounding text
 * - Suggestion list populated by the cSpellChecker engine
 * - Action buttons: Replace, Replace All, Ignore, Ignore All, Add to Dictionary
 * - Manual replacement text field for custom corrections
 *
 * @section spellcheck_dictionary Custom Dictionary
 * Manages a user dictionary stored at ~/.Wordtsar_custom_dictionary. Words
 * added via "Add to Dictionary" are appended to this file and loaded at
 * spell check startup to supplement the system dictionary.
 *
 * @author Gerald Brandt
 * @copyright GNU Affero General Public License v3.0
 *
 * @see cSpellCheck Spell check dialog class
 * @see eSpellMode Spell check mode enumeration
 * @see cSpellChecker Core spell checking engine
 * @see cEditorCtrl Editor providing document access for spell checking
 */

#include <fstream>

#include <QMessageBox>
#include <QDir>
#include <QVBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QDialogButtonBox>

#include "src/core/include/utils.h"

#include "cspellcheck.h"


cSpellCheck::cSpellCheck(cEditorCtrl *parent, eSpellMode mode) : QDialog(parent)
{
    mEditor = parent ;
    mMode = mode ;
    mSpellCheckDotCommands = mEditor->GetSpellCheckDotCommands() ;

    // Set up the dialog UI for all modes
    spellcheck.setupUi(this) ;

    connect(spellcheck.ignorebutton, SIGNAL(clicked()), this, SLOT(ignorebuttonclicked())) ;
    connect(spellcheck.addbutton, SIGNAL(clicked()), this, SLOT(addbuttonclicked())) ;
    connect(spellcheck.enterbutton, SIGNAL(clicked()), this, SLOT(enterbuttonclicked())) ;
    connect(spellcheck.morebutton, SIGNAL(clicked()), this, SLOT(morebuttonclicked())) ;
    connect(spellcheck.cancelbutton, SIGNAL(clicked()), this, SLOT(cancelbuttonclicked())) ;
    connect(spellcheck.suggestions, SIGNAL(itemDoubleClicked(QListWidgetItem *)), this, SLOT(suggestiondoubleclicked(QListWidgetItem *)));

    // Get our local words list
    std::string homeDir = QDir::homePath().toStdString() ;
    std::string dict = homeDir + "/.Wordtsar_custom_dictionary" ;
    std::ifstream dictFile(dict);
    if (dictFile.is_open())
    {
        std::string line;
        while (std::getline(dictFile, line))
        {
            spelling.AddWord(line);
        }
        dictFile.close();
    }
}

cSpellCheck::~cSpellCheck(void)
{
}

void cSpellCheck::CheckDocument(void)
{
    mEditor->GetDocument()->SaveBlocks() ;

    PARAGRAPH_T para = mEditor->GetDocument()->GetNumberofParagraphs() ;
    PARAGRAPH_T currentpara = mEditor->GetDocument()->GetParagraphFromPosition(mEditor->GetCaretDocumentPosition()) ;

    for(PARAGRAPH_T ploop = currentpara; ploop < para; ploop++)
    {
        bool quit = false ;
        std::string text ;
        std::vector<POSITION_T> wordstarts, modwordstarts ;
        POSITION_T pstart ;

        bool good = PrepareText(ploop, text, wordstarts, modwordstarts, pstart) ;
        if(good == false)
        {
            continue ;
        }

        POSITION_T startpos = modwordstarts[0] ;    // strt startpos in the last word in para index
        for(size_t wordloop = 1; wordloop < modwordstarts.size(); wordloop++)
        {
            POSITION_T wstart = modwordstarts[wordloop] ;   // start position of current word in para index
            std::string word = text.substr(startpos, wstart - startpos);  // wrong math here

            if(word.length() != 0)
            {
                int countspaces = trimandcheck(word) ;

                POSITION_T docwordstart = startpos + pstart + countspaces ;
                
                if(word.length() != 0)
                {
                    size_t index = 0 ;
                    if(spelling.CheckWord(word) == false)
                    {
                        std::vector<std::string> suggestions = spelling.suggestions(word) ;

                        mEditor->GetDocument()->SetPosition(docwordstart) ;
                        mEditor->GetDocument()->SetBeginBlock() ;
                        mEditor->GetDocument()->SetPosition(docwordstart + word.length() + 1) ;
                        mEditor->GetDocument()->SetEndBlock() ;
                        mEditor->ScrollIntoView() ;
                        mEditor->Repaint() ;

                        bool cont = false ;
                        do
                        {
                            cont = false ;

                            // put in 5 words at a time
                            for(size_t wordcount = index; wordcount < index + 5; wordcount++)
                            {
                                if(wordcount < suggestions.size())
                                {
                                    std::string entry ;
                                    entry = string_sprintf("%ld. %s", wordcount - index + 1, suggestions[wordcount].c_str()) ;
                                    spellcheck.suggestions->addItem(entry.c_str()) ;
                                }
                            }

                            spellcheck.badword->setText(word.c_str()) ;
                            restoreGeometry(mGeometry);
                            int ecode = exec() ;

                            spellcheck.suggestions->clear() ;

                            switch(ecode)
                            {
                                case 1 :
                                case 2 :
                                case 3 :
                                case 4 :
                                case 5 :
                                    if(ecode < (int)suggestions.size())
                                    {
                                        mEditor->DeleteBlock() ;
                                        mEditor->GetDocument()->SetPosition(docwordstart) ;
                                        mEditor->InsertWordStarString(suggestions[ecode - 1]) ;

                                        PrepareText(ploop, text, wordstarts, modwordstarts, pstart) ;
                                        wordloop-- ;
                                        wstart = modwordstarts[wordloop] ;
                                    }
                                    break ;

                                case QDialog::Rejected :
                                    quit = true ;
                                    break ;

                                case SPELLADD :
                                    AddWord(word) ;
                                    break ;

                                case SPELLIGNORE :
                                    // add the word to dictionary, withoy adding it to file
                                    spelling.AddWord(word) ;
                                    break ;

                                case SPELLMORE :
                                    index += 5 ;
                                    if(index >= suggestions.size())
                                    {
                                        index = 0 ;
                                    }
                                    cont = true ;
                                    break ;

                                case SPELLENTER :
                                    {
                                        std::string newword ;
                                        EnterWord(newword) ;

                                        if(newword.length() != 0)
                                        {
                                            mEditor->DeleteBlock() ;
                                            mEditor->GetDocument()->SetPosition(docwordstart) ;
                                            mEditor->InsertWordStarString(newword) ;

                                            PrepareText(ploop, text, wordstarts, modwordstarts, pstart) ;
                                            wordloop-- ;
                                            wstart = modwordstarts[wordloop] ;
                                        }
                                    }
                                    break ;

                            }
                        } while(cont == true) ;
                    }
                }
            }
            startpos = wstart ;

            if(quit)
            {
                break ;
            }
        }

        if(quit)
        {
            break ;
        }

    }

    mEditor->GetDocument()->RestoreBlocks() ;
}




/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Check the word at the current cursor position. Uses
/// GetPrevWordPosition/GetNextWordPosition for proper Unicode word
/// boundary detection. If misspelled, shows the spell check dialog
/// with suggestions and all button options.
///
/////////////////////////////////////////////////////////////////////////////
void cSpellCheck::CheckWord(void)
{
    POSITION_T pos = mEditor->GetCaretDocumentPosition() ;

    // Find word boundaries using Unicode word boundary detection
    POSITION_T wordStart = mEditor->GetDocument()->GetPrevWordPosition(pos + 1) ;
    POSITION_T wordEnd = mEditor->GetDocument()->GetNextWordPosition(pos) ;

    if (wordStart >= wordEnd)
    {
        QMessageBox::information(mEditor, "Spell Check", "No word at cursor position.") ;
        return ;
    }

    // Extract the word text from the document
    PARAGRAPH_T para = mEditor->GetDocument()->GetParagraphFromPosition(wordStart) ;
    std::string paraText = mEditor->GetDocument()->GetParagraphText(para) ;
    POSITION_T paraStart = 0 ;
    POSITION_T paraEnd = 0 ;
    mEditor->GetDocument()->GetParagraphStartandEnd(para, paraStart, paraEnd) ;

    // Convert document positions to paragraph-relative byte offsets
    // using grapheme offsets for proper UTF-8 handling
    std::vector<POSITION_T> offsets ;
    mEditor->GetDocument()->GetParagraphGraphemeOffsets(para, offsets) ;

    POSITION_T relStart = wordStart - paraStart ;
    POSITION_T relEnd = wordEnd - paraStart ;

    // Bounds check
    if (relStart < 0 || relEnd > static_cast<POSITION_T>(offsets.size()))
    {
        QMessageBox::information(mEditor, "Spell Check", "No word at cursor position.") ;
        return ;
    }

    // Get byte offsets for the word
    size_t byteStart = offsets[static_cast<size_t>(relStart)] ;
    size_t byteEnd = 0 ;
    if (relEnd < static_cast<POSITION_T>(offsets.size()))
    {
        byteEnd = offsets[static_cast<size_t>(relEnd)] ;
    }
    else
    {
        byteEnd = paraText.length() ;
    }

    std::string word = paraText.substr(byteStart, byteEnd - byteStart) ;

    // Trim whitespace and check for digits
    trimandcheck(word) ;
    if (word.empty())
    {
        QMessageBox::information(mEditor, "Spell Check", "No word at cursor position.") ;
        return ;
    }

    // Check spelling
    if (spelling.CheckWord(word))
    {
        std::string msg = string_sprintf("\"%s\" is spelled correctly.", word.c_str()) ;
        QMessageBox::information(mEditor, "Spell Check", QString::fromStdString(msg)) ;
        return ;
    }

    // Word is misspelled -- show the dialog with suggestions
    ShowSingleWordDialog(word, wordStart, wordEnd) ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Prompt the user to enter a word, then check its spelling.
/// If misspelled, shows the spell check dialog with suggestions.
/// Since the word is not in the document, suggestion selection and
/// Enter button are disabled (no text to replace).
///
/////////////////////////////////////////////////////////////////////////////
void cSpellCheck::CheckEnteredWord(void)
{
    // Prompt user for a word
    std::string word ;
    EnterWord(word) ;

    if (word.empty())
    {
        return ;
    }

    // Check spelling
    if (spelling.CheckWord(word))
    {
        std::string msg = string_sprintf("\"%s\" is spelled correctly.", word.c_str()) ;
        QMessageBox::information(mEditor, "Spell Check", QString::fromStdString(msg)) ;
        return ;
    }

    // Word is misspelled -- show the dialog with suggestions
    // Pass -1 for document positions since there is no document word to replace
    ShowSingleWordDialog(word, -1, -1) ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  word          [in] the misspelled word
/// @param  docWordStart  [in] document position of word start (-1 if no document word)
/// @param  docWordEnd    [in] document position of word end (-1 if no document word)
///
/// @return nothing
///
/// @brief
/// Show the spell check dialog for a single misspelled word.
/// Displays suggestions 5 at a time with all button options:
/// 1-5 pick suggestion (replaces word in document if positions valid),
/// Ignore (add to session dictionary), Add (add to custom dictionary file),
/// Enter (type replacement word), More (next 5 suggestions), Cancel.
///
/////////////////////////////////////////////////////////////////////////////
void cSpellCheck::ShowSingleWordDialog(const std::string &word, POSITION_T docWordStart, POSITION_T docWordEnd)
{
    bool hasDocPosition = (docWordStart >= 0 && docWordEnd >= 0) ;
    std::vector<std::string> suggestions = spelling.suggestions(word) ;

    // If we have a document position, highlight the word with block selection
    if (hasDocPosition)
    {
        mEditor->GetDocument()->SaveBlocks() ;
        mEditor->GetDocument()->SetPosition(docWordStart) ;
        mEditor->GetDocument()->SetBeginBlock() ;
        mEditor->GetDocument()->SetPosition(docWordEnd + 1) ;
        mEditor->GetDocument()->SetEndBlock() ;
        mEditor->ScrollIntoView() ;
        mEditor->Repaint() ;
    }

    size_t index = 0 ;
    bool cont = false ;

    do
    {
        cont = false ;

        // Show 5 suggestions at a time
        for (size_t wordcount = index; wordcount < index + 5; wordcount++)
        {
            if (wordcount < suggestions.size())
            {
                std::string entry ;
                entry = string_sprintf("%ld. %s", wordcount - index + 1, suggestions[wordcount].c_str()) ;
                spellcheck.suggestions->addItem(entry.c_str()) ;
            }
        }

        spellcheck.badword->setText(word.c_str()) ;
        restoreGeometry(mGeometry) ;
        int ecode = exec() ;

        spellcheck.suggestions->clear() ;

        switch (ecode)
        {
            case 1 :
            case 2 :
            case 3 :
            case 4 :
            case 5 :
            {
                // Replace word in document with selected suggestion
                size_t suggIdx = index + static_cast<size_t>(ecode) - 1 ;
                if (hasDocPosition && suggIdx < suggestions.size())
                {
                    mEditor->DeleteBlock() ;
                    mEditor->GetDocument()->SetPosition(docWordStart) ;
                    mEditor->InsertWordStarString(suggestions[suggIdx]) ;
                }
                break ;
            }

            case QDialog::Rejected :
                // Cancel -- just exit
                break ;

            case SPELLADD :
                AddWord(word) ;
                break ;

            case SPELLIGNORE :
                // Add the word to session dictionary without saving to file
                spelling.AddWord(word) ;
                break ;

            case SPELLMORE :
                index += 5 ;
                if (index >= suggestions.size())
                {
                    index = 0 ;
                }
                cont = true ;
                break ;

            case SPELLENTER :
            {
                // Enter a replacement word
                std::string newword ;
                EnterWord(newword) ;

                if (!newword.empty() && hasDocPosition)
                {
                    mEditor->DeleteBlock() ;
                    mEditor->GetDocument()->SetPosition(docWordStart) ;
                    mEditor->InsertWordStarString(newword) ;
                }
                break ;
            }
        }
    } while (cont == true) ;

    // Restore block selection state
    if (hasDocPosition)
    {
        mEditor->GetDocument()->RestoreBlocks() ;
    }
}


void cSpellCheck::keyPressEvent(QKeyEvent *event)
{
    // Save the current geometry of the dialog
    mGeometry = saveGeometry();

    event->setModifiers(Qt::NoModifier);
    switch (event->key()) 
    {
        case Qt::Key_1 :
            done(1);
            break;

        case Qt::Key_2 :
            done(2);    
            break;

        case Qt::Key_3 :
            done(3);
            break;  

        case Qt::Key_4 :
            done(4);
            break;

        case Qt::Key_5 :
            done(5);
            break;

        case Qt::Key_I:
            done(SPELLIGNORE);
            break;

        case Qt::Key_A:
            done(SPELLADD);
            break;

        case Qt::Key_E:
            done(SPELLENTER);
            break;

        case Qt::Key_M:
            done(SPELLMORE);
            break;

        case Qt::Key_C:
        case Qt::Key_Escape:
            done(QDialog::Rejected) ;
            break;

        default:
            QDialog::keyPressEvent(event);
    }
}


void cSpellCheck::showEvent(QShowEvent *e)
{
    UNUSED_ARGUMENT(e) ;

    setFocusPolicy(Qt::StrongFocus);
    setFocus(Qt::PopupFocusReason);
}

void cSpellCheck::ignorebuttonclicked(void)
{
    done(SPELLIGNORE);
}

void cSpellCheck::addbuttonclicked(void)
{
    done(SPELLADD);
}

void cSpellCheck::enterbuttonclicked(void)
{
    done(SPELLENTER);
}

void cSpellCheck::morebuttonclicked(void)
{
    done(SPELLMORE);
}

void cSpellCheck::cancelbuttonclicked(void)
{
    done(QDialog::Rejected) ;
}

void cSpellCheck::suggestiondoubleclicked(QListWidgetItem *item)
{
    std::string text = item->text().toStdString() ;
    int suggestionIndex = atoi(text.substr(0, 1).c_str()) ;
    if (suggestionIndex > 0 && suggestionIndex <= spellcheck.suggestions->count()) 
    {
        done(suggestionIndex);
    }
}


bool cSpellCheck::PrepareText(PARAGRAPH_T para, std::string &text, std::vector<POSITION_T> &wordstarts, std::vector<POSITION_T> &modwordstarts, POSITION_T &pstart)
{
    wordstarts.clear() ;
    modwordstarts.clear() ;

    text = mEditor->GetDocument()->GetParagraphText(para) ;
    POSITION_T end ;
    mEditor->GetDocument()->GetParagraphStartandEnd(para, pstart, end) ;

    // strip punctuation and special characters
    std::string replace = "\\()[]{};:?+-*=/&~|\"<>!\r\n#$%^&\177" ;

    // Handle dot command and comment lines
    if (text.size() >= 2 && text[0] == '.')
    {
        if (!mSpellCheckDotCommands)
        {
            // Skip all dot-prefixed lines when option is off
            return false ;
        }

        // Spell checking enabled -- blank out the command portion
        if (text[1] == '.')
        {
            // Comment line (..) -- blank the two-dot prefix
            text[0] = ' ' ;
            text[1] = ' ' ;
        }
        else if (text.size() >= 3 && (text.substr(0, 3) == ".ig" || text.substr(0, 3) == ".IG"))
        {
            // Ignore/comment line (.ig) -- blank the .ig prefix
            text[0] = ' ' ;
            text[1] = ' ' ;
            text[2] = ' ' ;
        }
        else
        {
            // Dot command (.HE, .FO, .LM, etc.) -- blank everything up to first space
            size_t cmdEnd = text.find(' ') ;
            if (cmdEnd != std::string::npos)
            {
                for (size_t i = 0; i < cmdEnd; i++)
                {
                    text[i] = ' ' ;
                }
            }
            else
            {
                // No text after the command -- nothing to spell check
                return false ;
            }
        }
    }
    
    // this works since the ascii chars are part of UTF8
    // and now a loop for control charcaters and contractions
    for(size_t loop = 0; loop < text.length(); loop++)
    {
        // replace chars we can't match with space
        if (replace.find(text[loop]) != std::string::npos)
        {
            text[loop] = ' ';
        }

        // Hunspell doesn't like apostropies around words, but want's them in contractions, so run a special check
        if(text[loop] == '\'')
        {
            if(loop != 0)
            {
                // if the apostrophe is not surround by text (non white space), replace it
                if(text[loop - 1] == ' ' || text[loop + 1] == ' ')
                {
                    text[loop] = ' ' ;
                }
            }
        }

        // if a period or a comma is surround by numbers, then leave it, else replace with space
        if (text[loop] == '.' || text[loop] == ',') 
        {
            if (loop == 0 || loop == text.length() - 1 || !std::isdigit(text[loop - 1]) || !std::isdigit(text[loop + 1])) 
            {
            text[loop] = ' ';
            }
        }
    }

        mEditor->GetDocument()->GetWordPositions(text, wordstarts) ;
        if(wordstarts.empty())
        {
            return false ;
        }

        modwordstarts = wordstarts ;
        for (auto &pos : wordstarts)
        {
            pos += pstart;
        }

    return true ;
}

void cSpellCheck::AddWord(std::string word)
{
    std::string text = string_sprintf("Add \"%s\" to dictionery as correct spelling?", word.c_str()) ;

    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(mEditor, "Add to Dictionary", QString::fromStdString(text),
                                  QMessageBox::Yes | QMessageBox::No);
    if (reply == QMessageBox::Yes)
    {
        std::string homeDir = QDir::homePath().toStdString() ;
        std::string dict = homeDir + "/.Wordtsar_custom_dictionary" ;
        std::ofstream dictFile(dict, std::ios_base::app);
        if (dictFile.is_open())
        {
            dictFile << word << std::endl;
            dictFile.close();
        }
        else
        {
            QMessageBox::warning(mEditor, "Error", "Unable to open dictionary file for writing.");
        }

        spelling.AddWord(word);
    }
}


void cSpellCheck::EnterWord(std::string &word)
{
    QDialog dialog(mEditor);
    QVBoxLayout layout(&dialog);

    QLabel label("Enter Word:");
    layout.addWidget(&label);

    QLineEdit lineEdit;
    layout.addWidget(&lineEdit);

    QDialogButtonBox buttonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    layout.addWidget(&buttonBox);

    connect(&buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(&buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() == QDialog::Accepted) 
    {
        word = lineEdit.text().toStdString();
    }
}

///////////////////////////////////////////////////////////////////
///
/// @param   s  [in/out] - the string to be trimmed
///
/// @return count of leading spaces removed
///
/// @brief  Trim the string and check if it contains any digits
///
/// if It contains any digits, then return an empty string
///
///////////////////////////////////////////////////////////////////
int cSpellCheck::trimandcheck(std::string &s) {
    // Remove leading spaces and count them.
    size_t start = 0;
    while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start]))) {
        ++start;
    }
    int leadingCount = static_cast<int>(start);
    
    // Remove trailing spaces.
    size_t end = s.size();
    while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1]))) {
        --end;
    }
    
    // Extract the trimmed portion.
    std::string trimmed = s.substr(start, end - start);
    
    // Check if the trimmed string contains any digit.
    if (std::any_of(trimmed.begin(), trimmed.end(), [](unsigned char c) { return std::isdigit(c); })) {
        s.clear();
    } else {
        s = trimmed;
    }
    
    return leadingCount;
}