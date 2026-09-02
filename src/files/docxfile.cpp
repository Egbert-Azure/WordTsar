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
// Parser based off of https://github.com/kschroeer/rtf-html-java (MIT License)
//
//////////////////////////////////////////////////////////////////////////////

/**
 * @class cDocxFile
 *
 * @brief DOCX file format loader for WordTsar.
 *
 * Implements the cDOCXFile class which provides partial DOCX import by
 * extracting word/document.xml and word/styles.xml from the ZIP archive
 * using miniz, then walking the XML tree with pugixml.
 *
 * @section docx_extraction ZIP Extraction
 * The DOCX format is a ZIP archive containing XML files. This implementation
 * extracts word/document.xml (body content) and word/styles.xml (style
 * definitions) into memory buffers using the miniz library.
 *
 * @section docx_formatting Supported Formatting
 * - Paragraph styles: heading levels, body text, list styles via sDOCXParagraphStyle
 * - Character formatting: bold, italic, underline, strikethrough, super/subscript
 *   via sCharacterProperties
 * - Font and color selection from style sheets and inline run properties
 * - Section properties: page size, margins, orientation, columns
 *
 * @section docx_limitations Current Limitations
 * - Images and embedded objects are not imported; a placeholder marker is
 *   inserted in their place instead
 * - Tables are imported as tab-delimited text (real w:tr/w:tc traversal,
 *   one row per line, columns aligned via a real .tb dot command) rather
 *   than a native table structure -- WordStar has no table/column concept
 *   to map DOCX's actual cell grid onto, so column widths are approximated
 *   as equal-width rather than mirroring the source document's own widths
 * - List numbering (w:numPr/w:numId/w:ilvl, resolved against
 *   word/numbering.xml's abstractNum definitions) is emitted as literal
 *   marker text ("1. ", "a) ", "-  " for bullets, etc.) inserted before the
 *   paragraph's own text, since WordStar has no live auto-numbering field
 *   to bind it to. Multi-level lists renumber correctly (a level-1 item
 *   resets deeper levels' counters), but the hanging indent a real bullet
 *   needs isn't applied -- left/right paragraph indent emission is a
 *   separate, pre-existing gap (EmitIndent only ever emits first-line
 *   indent), not something this pass changed
 * - Custom tab stops (w:pPr/w:tabs) are emitted as a real .tb dot command
 * - Paragraph borders and shading (w:pBdr, w:shd) are still not emitted --
 *   deliberately parked, not a gap: WordStar has no paragraph-border or
 *   shading concept at all, and approximating one with literal ASCII art
 *   wasn't part of what this pass was asked to cover
 * - Header/footer text content is not imported (only the page margin
 *   distances reserved for them are read from w:pgMar)
 * - Advanced features (tracked changes, comments) are skipped
 *
 * @author Gerald Brandt
 * @copyright GNU Affero General Public License v3.0
 *
 * @see cDOCXFile DOCX file handler class
 * @see cFile Base file handler class
 * @see sCharacterProperties Character formatting state during DOCX parsing
 * @see sDOCXParagraphStyle Paragraph style definition structure
 * @see sDOCXCharacterStyle Character style definition structure
 * @see cDocument Document model receiving imported content
 */


#include <cstdlib>
#include <stdlib.h>
#include <stdexcept>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <deque>
#include <iomanip>

#include "docxfile.h"

#include "src/core/include/utils.h"
#include "src/core/document/document.h"
#include "src/core/editor/editorbase.h"
#include "src/core/layout/layoutbase.h"

#include "zip.h"


/// @ingroup Editor
/// @{


//extern sExtendedChars gCodePage437[] ;
//extern int gExtendedSize ;
extern sSeqRGBColor gBaseWSColors[] ;

/////////////////////////////////////////////////////////////////////////////
///
/// @param  editor [in] pointer to the editor for document access
///
/// @return nothing
///
/// @brief
/// Constructor. Delegates to cFile base class.
///
/////////////////////////////////////////////////////////////////////////////
cDOCXFile::cDOCXFile(cEditorBase *editor)
    : cFile(editor)
{
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Destructor.
///
/////////////////////////////////////////////////////////////////////////////
cDOCXFile::~cDOCXFile(void)
{
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  filename [in] file path to check
///
/// @return true if filename has .docx extension, false otherwise
///
/// @brief
/// Check if a file is a DOCX file by examining its extension.
///
/////////////////////////////////////////////////////////////////////////////
bool cDOCXFile::CheckType(std::string filename)
{
    std::string ext;

    size_t found = filename.find_last_of(".") ;
    ext = filename.substr(found + 1) ;

    for(size_t loop = 0; loop < ext.size(); loop++)
    {
        ext[loop] = tolower(ext[loop]) ;
    }

    return (ext == "docx");
}

const char* node_types[] =
{
    "null", "document", "element", "pcdata", "cdata", "comment", "pi", "declaration"
};


cDOCXFile * gDOCXFile = nullptr ;

/////////////////////////////////////////////////////////////////////////////
///
/// @param  filename [in] path to the DOCX file to load
///
/// @return true on success, false on failure
///
/// @brief
/// Load a DOCX file. Opens the ZIP archive, extracts word/styles.xml
/// and word/document.xml, parses styles, processes section properties,
/// then walks the XML tree inserting paragraphs and tables into the
/// document.
///
/////////////////////////////////////////////////////////////////////////////
bool cDOCXFile::LoadFile(std::string filename)
{
    bool retval = false ;

    mFontName.clear() ;
    mFontSize = 24 ;
    mSpaceBefore = 0 ;
    mSpaceAfter = 0 ;
    mAlign = JUST_LEFT ;

    mBold = false ;
    mItalics = false ;
    mUnderline = false ;
    mStrikethrough = false ;
    mSuperscript = false ;
    mSubscript = false ;
    mSmallcaps = false ;
    mShadow = false ;

    void *stylebuf = nullptr ;
    void *docbuf = nullptr ;
    size_t bufsize = 0;

    pugi::xml_node parent;

    // Open file and load "xml" content to the document variable
    zip_t *zip = zip_open(filename.c_str(), ZIP_DEFAULT_COMPRESSION_LEVEL, 'r');
    if(zip != nullptr)
    {
        // open Style Sheets
        zip_entry_open(zip, "word/styles.xml");
        zip_entry_read(zip, &stylebuf, &bufsize);

        zip_entry_close(zip);

        styles.load_buffer(stylebuf, bufsize);

        parent = styles.child("w:styles").child("w:style") ;

        free(stylebuf);

        ParseStyles(parent) ;

        // Numbering definitions (word/numbering.xml) are optional -- a DOCX
        // with no lists at all won't have this part in the archive.
        if(zip_entry_open(zip, "word/numbering.xml") == 0)
        {
            void *numberingbuf = nullptr ;
            size_t numberingsize = 0 ;
            zip_entry_read(zip, &numberingbuf, &numberingsize) ;
            zip_entry_close(zip) ;

            pugi::xml_document numberingDoc ;
            numberingDoc.load_buffer(numberingbuf, numberingsize) ;
            ParseNumbering(numberingDoc.child("w:numbering")) ;

            free(numberingbuf) ;
        }

        // open document
        zip_entry_open(zip, "word/document.xml");
        zip_entry_read(zip, &docbuf, &bufsize);

        zip_entry_close(zip);
        zip_close(zip);

        document.load_buffer(docbuf, bufsize);

        parent = document.child("w:document").child("w:body") ;

        pugi::xml_node sect = document.child("w:document").child("w:body").child("w:sectPr") ;
        if(sect)
        {
            HandleSection(sect) ;
        }

        free(docbuf);

        gDOCXFile = this ;

        // walk the document
        struct simple_walker: pugi::xml_tree_walker
        {
            virtual bool for_each(pugi::xml_node& node)
            {
                if(depth() == 2)
                {
                    std::string name = node.name() ;
                    if(name == "w:p")
                    {
                        gDOCXFile->HandleParagraphNode(node, depth()) ;
                    }
                    else if(name == "w:tbl")
                    {
                        gDOCXFile->HandleTableNode(node, depth()) ;
                    }
                    // a section
                    if(name == "w:sectPr")
                    {
                        gDOCXFile->HandleSection(node) ;
                    }
                }

                return true; // continue traversal
            }
        };

        simple_walker walker ;
        document.traverse(walker) ;
    }

    return retval ;
}



/////////////////////////////////////////////////////////////////////////////
///
/// @param  filename [in] path to save to
/// @param  length [in] document length (unused -- walked via GetNumberofParagraphs)
///
/// @return true on success
///
/// @brief
/// Save the document as a DOCX file.
///
/////////////////////////////////////////////////////////////////////////////
bool cDOCXFile::SaveFile(std::string filename, POSITION_T length)
{
    UNUSED_ARGUMENT(length) ;
    return WriteDocx(filename) ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return true
///
/// @brief
/// Report that this handler can load files.
///
/////////////////////////////////////////////////////////////////////////////
bool cDOCXFile::CanLoad(void)
{
    return true ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return true
///
/// @brief
/// Report that this handler can save files.
///
/////////////////////////////////////////////////////////////////////////////
bool cDOCXFile::CanSave(void)
{
    return true ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return file filter string for DOCX files
///
/// @brief
/// Return the file extension filter string for open/save dialogs.
///
/////////////////////////////////////////////////////////////////////////////
std::string cDOCXFile::GetExtensions(void)
{
    return "DOCX Files (*.docx *.DOCX)" ;
}



/////////////////////////////////////////////////////////////////////////////
///
/// @param  style [in] first w:style XML node from styles.xml
///
/// @return nothing
///
/// @brief
/// Walk the w:styles XML tree extracting paragraph and character style
/// definitions into mParagraphStyles and mCharacterStyles vectors.
///
/////////////////////////////////////////////////////////////////////////////
void cDOCXFile::ParseStyles(pugi::xml_node style)
{
    // fill all styles
    while(!style.empty())
    {

        std::string type = style.attribute("w:type").value() ;

        if(type == "paragraph")
        {
            struct sDOCXParagraphStyle pstyle ;

            pstyle.id = style.attribute("w:styleId").value() ;
            pstyle.name = style.child("w:name").attribute("w:val").value() ;
            pstyle.basedon = style.child("w:basedOn").attribute("w:val").value() ;
            pstyle.rsid = style.child("w:rsid").attribute("w:val").value() ;

            pstyle.asciifont = style.child("w:rPr").child("w:rFonts").attribute("w:ascii").value() ;
            pstyle.ansifont = style.child("w:rPr").child("w:rFonts").attribute("w:hAnsi").value() ;
            pstyle.csfont = style.child("w:rPr").child("w:rFonts").attribute("w:cs").value() ;


            pstyle.before = style.child("w:pPr").child("w:spacing").attribute("w:before").value() ;
            pstyle.after = style.child("w:pPr").child("w:spacing").attribute("w:after").value() ;
            pstyle.linespace = style.child("w:pPr").child("w:spacing").attribute("w:line").value() ;
            pstyle.linetype = style.child("w:pPr").child("w:spacing").attribute("w:lineRule").value() ;

            pstyle.outlinelevel = style.child("w:pPr").child("w:outlineLvl").attribute("w:val").value() ;

            // 2006 spec says 'left' 2011 spec says 'start'
            pstyle.left = style.child("w:pPr").child("w:ind").attribute("w:left").value() ;
            if(pstyle.left.empty())
            {
                pstyle.left = style.child("w:pPr").child("w:ind").attribute("w:start").value() ;
            }
            // 2006 spec says 'right' 2011 spec says 'end'
            pstyle.right = style.child("w:pPr").child("w:ind").attribute("w:right").value() ;
            if(pstyle.right.empty())
            {
                pstyle.right = style.child("w:pPr").child("w:ind").attribute("w:end").value() ;
            }
            pstyle.hanging = style.child("w:pPr").child("w:ind").attribute("w:hanging").value() ;
            pstyle.firstline = style.child("w:pPr").child("w:ind").attribute("w:firstLine").value() ;

            pstyle.justify = style.child("w:pPr").child("w:jc").attribute("w:val").value() ;

            for(pugi::xml_node tab = style.child("w:pPr").child("w:tabs").child("w:tab") ; tab ; tab = tab.next_sibling("w:tab"))
            {
                sDOCXTabStop tabstop ;
                tabstop.val = tab.attribute("w:val").value() ;
                tabstop.pos = tab.attribute("w:pos").value() ;
                pstyle.tabs.push_back(tabstop) ;
            }

            GetCharacterStyle(style, pstyle.charprops) ;


            mParagraphStyles.push_back(pstyle) ;
        }
        else if(type == "character")
        {
            struct sDOCXCharacterStyle cstyle ;

            cstyle.id = style.attribute("w:styleId").value() ;
            cstyle.name = style.child("w:name").attribute("w:val").value() ;
            cstyle.basedon = style.child("w:basedOn").attribute("w:val").value() ;
            cstyle.rsid = style.child("w:rsid").attribute("w:val").value() ;

            GetCharacterStyle(style, cstyle.charprops) ;

            mCharacterStyles.push_back(cstyle) ;
        }

        style = style.next_sibling() ;
    }

    printf("done") ;
}



/////////////////////////////////////////////////////////////////////////////
///
/// @param  style [in] XML node containing w:rPr child
/// @param  cstyle [out] character properties structure to fill
///
/// @return nothing
///
/// @brief
/// Extract character formatting properties (font size, color, bold,
/// italic, underline, strikethrough, superscript, subscript, smallcaps,
/// shadow) from a w:rPr XML node.
///
/////////////////////////////////////////////////////////////////////////////
void cDOCXFile::GetCharacterStyle(pugi::xml_node &style, sCharacterProperties &cstyle)
{
    pugi::xml_node attr ;

    cstyle.size = style.child("w:rPr").child("w:sz").attribute("w:val").value() ;
    cstyle.cssize = style.child("w:rPr").child("w:szCs").attribute("w:val").value() ;

    cstyle.color = style.child("w:rPr").child("w:color").attribute("w:val").value() ;

    attr = style.child("w:rPr").child("w:b") ;
    if(attr)
    {
        cstyle.bold = true ;
    }
    else
    {
        cstyle.bold = false ;
    }

    attr = style.child("w:rPr").child("w:i") ;
    if(attr)
    {
        cstyle.italics = true ;
    }
    else
    {
        cstyle.italics = false ;
    }

    attr = style.child("w:rPr").child("w:u") ;
    if(attr)
    {
        cstyle.underline = true ;
    }
    else
    {
        cstyle.underline = false ;
    }

    std::string val = style.child("w:rPr").child("w:strike").attribute("w:val").value() ;
    if(val == "true")
    {
        cstyle.strikethrough = true ;
    }
    else
    {
        cstyle.strikethrough = false ;
    }

    val = style.child("w:rPr").child("w:vertAlign").attribute("w:val").value() ;
    if(val == "superscript")
    {
        cstyle.superscript = true ;
    }
    else
    {
        cstyle.superscript = false ;
    }

    if(val == "subscript")
    {
        cstyle.subscript = true ;
    }
    else
    {
        cstyle.subscript = false ;
    }

    val = style.child("w:rPr").child("w:smallCaps").attribute("w:val").value() ;
    if(val == "true")
    {
        cstyle.smallcaps = true ;
    }
    else
    {
        cstyle.smallcaps = false ;
    }

    val = style.child("w:rPr").child("w:shadow").attribute("w:val").value() ;
    if(val == "true")
    {
        cstyle.shadow = true ;
    }
    else
    {
        cstyle.shadow = false ;
    }
}





/////////////////////////////////////////////////////////////////////////////
///
/// @param  node [in] the w:p XML node to process
/// @param  depth [in] XML tree depth (unused)
///
/// @return nothing
///
/// @brief
/// Process a DOCX paragraph node. Resolves paragraph and run styles,
/// emits spacing, indent, justification, font, and attribute formatting,
/// then walks child nodes inserting text, tabs, and image placeholders.
/// Appends a hard return after each paragraph.
///
/////////////////////////////////////////////////////////////////////////////
void cDOCXFile::HandleParagraphNode(pugi::xml_node node, int depth)
{
    UNUSED_ARGUMENT(depth) ;
#ifdef NOPE
    cout << setw(5) << depth ;
    for (int i = 0; i < depth; ++i) std::cout << "  "; // indentation
    depth++ ;

    std::cout << "Paragraph  " << node_types[node.type()] << ": name='" << node.name() << "', value='" << node.value() << "'" ;
    if(node.first_attribute())
    {
        cout << "  " << node.first_attribute().name() << "=" << node.first_attribute().value() << " " ;
        pugi::xml_attribute node1 = node.first_attribute().next_attribute() ;
        while(node1)
        {
            cout << node1.name() << "=" << node1.value() << " " ;
            node1 = node1.next_attribute() ;
        }
    }
    cout << "\n" ;
#endif

    // see if we have a style associated with this paragraph
    std::string stylename = node.child("w:pPr").child("w:pStyle").attribute("w:val").value() ;
    int styleindex = 0 ;
    if(!stylename.empty())
    {
        styleindex = FindParagraphStyle(stylename) ;
    }

    // Tabs and numbering are paragraph-level concepts (not per-run), so they're
    // resolved and emitted once, before the first run's text -- unlike the
    // Emit* calls inside the loop below, which re-check per run because they
    // can carry inline run-level overrides.
    sDOCXParagraphStyle basestyle = MergeParagraphStyles(styleindex) ;
    EmitTabs(node, basestyle) ;
    EmitNumbering(node) ;

    pugi::xml_node run = node.child("w:r") ;
    while(run)
    {
        std::string text = run.child("w:t").text().get() ;
        // Hidden runs (w:vanish) are how comment lines get preserved on
        // export (SaveDotCommand) -- bringing them back as ordinary visible
        // text on import would surface hidden content instead of keeping it
        // hidden, so skip their text entirely.
        bool hidden = !run.child("w:rPr").child("w:vanish").empty() ;
        if(!text.empty() && !hidden)
        {
            // see if we have a style associated with this run
            std::string stylename = run.child("w:rPr").child("w:rStyle").attribute("w:val").value() ;
            int runstyle = -1 ;
            if(!stylename.empty())
            {
                runstyle = FindCharacterStyle(stylename) ;
            }

            // follow all the based ons we need
            sDOCXParagraphStyle parastyle = MergeParagraphStyles(styleindex) ;

            sDOCXParagraphStyle newstyle ;
            if(runstyle != -1)
            {
                sDOCXCharacterStyle charstyle = MergeCharacterStyles(runstyle) ;

                // now merge both styles for the final output
                newstyle = MergeStyles(parastyle, charstyle) ;
            }
            else
            {
                newstyle = parastyle ;
            }

            EmitParagraphSpace(node, run, newstyle) ;
//            EmitKeepLines(node, run, newstyle) ;
//            EmitKeepNext(node, run, newstyle) ;
            EmitIndent(node, run, newstyle) ;
//            EmitOutlineLevel(node, run, newstyle) ;
//            EmitBorder(node, run, newstyle) ;    -- parked: WordStar has no paragraph-border concept
//            EmitShading(node, run, newstyle) ;   -- parked: WordStar has no paragraph-shading concept
            EmitJustify(node, run, newstyle) ;
            EmitFont(node, run, newstyle) ;
            EmitAttributes(node, run, newstyle) ;

            // now we loop through the entire run looking for stuff to insert

            // walk the node
            struct simple_walker: pugi::xml_tree_walker
            {
                virtual bool for_each(pugi::xml_node& node)
                {
                    std::string name = node.name() ;
                    if(name == "w:tab")
                    {
                        sWSTab tab ;
                        tab.abstabsize = 0 ;
                        tab.size = 0 ;
                        tab.tabsize = 0 ;
                        tab.type = TAB_TAB ;

                        gDOCXFile->mDocument->InsertTab(tab) ;
                    }
                    else if(name == "w:drawing")
                    {
                        gDOCXFile->mDocument->Insert("<<< INLINE IMAGE >>>\n") ;
                    }
                    else if(name == "w:anchor>")
                    {
                        gDOCXFile->mDocument->Insert("<< FLOATING IMAGE >>") ;          // no cr, embeded in paragraph
                    }
                    else if(name == "w:t")
                    {
                        std::string text = node.text().get() ;
                        gDOCXFile->mDocument->Insert(text) ;
                    }

                    return true; // continue traversal
                }
            };

            simple_walker walker ;
            run.traverse(walker) ;
        }

        std::string page = run.child("w:br").attribute("w:type").value()  ;
        if(page == "page")
        {
            EmitPage() ;
        }        

        run = run.next_sibling("w:r") ;
    }

    mDocument->Insert(HARD_RETURN) ;

    /// @todo - really parse the section break
    std::string pagebreak = node.child("w:pPr").child("w:sectPr").child("w:type").attribute("w:val").value() ;
    if(!pagebreak.empty())
    {
        if(pagebreak == "nextPage")
        {
            EmitPage() ;
        }
    }

//    for (pugi::xml_node_iterator it = node.begin(); it != node.end(); ++it)
//    {
//        std::cout << "node: ";
//
//        HandleParagraphNode(*it, depth) ;
//    }
}



/////////////////////////////////////////////////////////////////////////////
///
/// @param  cellNode [in] a w:tc table-cell XML node
///
/// @return the cell's text content, one space between paragraphs, tabs
///         and newlines flattened to spaces
///
/// @brief
/// Concatenate the plain text of every run in every paragraph of a table
/// cell. Nested tables inside a cell are not descended into.
///
/////////////////////////////////////////////////////////////////////////////
std::string cDOCXFile::ExtractCellText(pugi::xml_node cellNode)
{
    std::string text ;
    bool firstParagraph = true ;

    for(pugi::xml_node para = cellNode.child("w:p") ; para ; para = para.next_sibling("w:p"))
    {
        if(!firstParagraph)
        {
            text += " " ;
        }
        firstParagraph = false ;

        for(pugi::xml_node run = para.child("w:r") ; run ; run = run.next_sibling("w:r"))
        {
            text += run.child("w:t").text().get() ;
        }
    }

    // A cell's own tab/newline would otherwise break the one-line-per-row
    // layout the caller builds around real .tb tab stops.
    for(char &c : text)
    {
        if(c == '\t' || c == '\n' || c == '\r')
        {
            c = ' ' ;
        }
    }

    return text ;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  node [in] the w:tbl XML node
/// @param  depth [in] XML tree depth (unused)
///
/// @return nothing
///
/// @brief
/// Handle a DOCX table node. Real w:tr/w:tc traversal: each row becomes
/// one line of tab-separated cell text, columns aligned with a real .tb
/// dot command. Column widths are approximated as equal-width across a
/// nominal 6-inch text width -- WordStar has no table/column-grid concept
/// to map the source document's own w:tblGrid widths onto.
///
/////////////////////////////////////////////////////////////////////////////
void cDOCXFile::HandleTableNode(pugi::xml_node node, int depth)
{
    UNUSED_ARGUMENT(depth) ;

    // Column count from the widest row, not just the first -- a first row
    // whose cells are wrapped in w:sdt (a common Word content-control
    // pattern) has zero direct w:tc children even though the table is
    // otherwise normal, and a ragged table can have more cells in a later
    // row than in its first.
    int columnCount = 0 ;
    for(pugi::xml_node row = node.child("w:tr") ; row ; row = row.next_sibling("w:tr"))
    {
        int rowCells = 0 ;
        for(pugi::xml_node cell = row.child("w:tc") ; cell ; cell = cell.next_sibling("w:tc"))
        {
            rowCells++ ;
        }
        if(rowCells > columnCount)
        {
            columnCount = rowCells ;
        }
    }
    if(columnCount == 0)
    {
        return ;
    }

    const double TABLE_WIDTH_INCHES = 6.0 ;
    double colwidth = TABLE_WIDTH_INCHES / columnCount ;

    std::string tabcmd = ".tb " ;
    for(int loop = 1 ; loop <= columnCount ; loop++)
    {
        tabcmd += string_sprintf("%.2fi ", colwidth * loop) ;
    }
    tabcmd += "\n" ;
    mDocument->Insert(tabcmd) ;

    for(pugi::xml_node row = node.child("w:tr") ; row ; row = row.next_sibling("w:tr"))
    {
        int col = 0 ;
        for(pugi::xml_node cell = row.child("w:tc") ; cell ; cell = cell.next_sibling("w:tc"))
        {
            if(col > 0)
            {
                sWSTab tab ;
                tab.abstabsize = 0 ;
                tab.size = 0 ;
                tab.tabsize = 0 ;
                tab.type = TAB_TAB ;

                mDocument->InsertTab(tab) ;
            }

            mDocument->Insert(ExtractCellText(cell)) ;

            col++ ;
        }
        mDocument->Insert(HARD_RETURN) ;
    }

    // The table's own .tb overrides whatever the surrounding paragraphs had
    // set; force the next real paragraph's EmitTabs() to re-assert its own
    // state rather than comparing against this table's stale tab stops.
    mCurrentTabs.clear() ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  style [in] style ID string to search for
///
/// @return index into mParagraphStyles, or -1 if not found
///
/// @brief
/// Search the paragraph styles vector for a style matching the given ID.
///
/////////////////////////////////////////////////////////////////////////////
int cDOCXFile::FindParagraphStyle(std::string &style)
{
    size_t loop ;
    for(loop = 0; loop < mParagraphStyles.size(); loop++)
    {
        if(style == mParagraphStyles[loop].id)
        {
            break ;
        }
    }

    if(loop == mParagraphStyles.size())
    {
        return -1 ;
    }
    else
    {
        return loop ;
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  style [in] style ID string to search for
///
/// @return index into mCharacterStyles, or -1 if not found
///
/// @brief
/// Search the character styles vector for a style matching the given ID.
///
/////////////////////////////////////////////////////////////////////////////
int cDOCXFile::FindCharacterStyle(std::string &style)
{
    if(mCharacterStyles.size() == 0)
    {
        return -1 ;
    }

    size_t loop ;
    for(loop = 0; loop < mCharacterStyles.size(); loop++)
    {
        if(style == mCharacterStyles[loop].id)
        {
            break ;
        }
    }

    if(loop == mCharacterStyles.size())
    {
        return -1 ;
    }
    else
    {
        return loop ;
    }
}



/////////////////////////////////////////////////////////////////////////////
///
/// @param  style [in] index into mParagraphStyles for the leaf style
///
/// @return merged paragraph style with all inherited properties resolved
///
/// @brief
/// Walk the basedOn chain for a paragraph style, collecting all ancestor
/// styles into a deque, then merge from root to leaf. String properties
/// override when non-empty. Boolean attributes use XOR to toggle
/// inheritance. Returns a default-constructed style for an out-of-range
/// index -- callers pass 0 when a paragraph has no explicit w:pStyle, and
/// FindParagraphStyle() returns -1 for an unresolved one, either of which
/// is a real, unbounded index into mParagraphStyles when styles.xml
/// defines no paragraph styles at all, or references one that's missing.
///
/////////////////////////////////////////////////////////////////////////////
sDOCXParagraphStyle cDOCXFile::MergeParagraphStyles(int style)
{
    if(style < 0 || static_cast<size_t>(style) >= mParagraphStyles.size())
    {
        return sDOCXParagraphStyle() ;
    }

    std::deque<int> styles ;    // our styles inheritance
    styles.push_back(style) ;

    while(mParagraphStyles[style].basedon.length() != 0)
    {
        style = FindParagraphStyle(mParagraphStyles[style].basedon) ;
        styles.push_front(style) ;
    } ;

    sDOCXParagraphStyle pstyle = mParagraphStyles[styles[0]] ;
    for(size_t loop = 1 ; loop < styles.size(); loop++)
    {
        // check for font overrides
        if(mParagraphStyles[styles[loop]].asciifont != "")
        {
            pstyle.asciifont = mParagraphStyles[styles[loop]].asciifont ;
        }
        if(mParagraphStyles[styles[loop]].ansifont != "")
        {
            pstyle.ansifont = mParagraphStyles[styles[loop]].ansifont ;
        }
        if(mParagraphStyles[styles[loop]].asciifont != "")
        {
            pstyle.csfont = mParagraphStyles[styles[loop]].csfont ;
        }

        // check for spacing overrides
        if(mParagraphStyles[styles[loop]].before != "")
        {
            pstyle.before = mParagraphStyles[styles[loop]].before ;
        }
        if(mParagraphStyles[styles[loop]].after != "")
        {
            pstyle.after = mParagraphStyles[styles[loop]].after ;
        }
        if(mParagraphStyles[styles[loop]].linespace != "")
        {
            pstyle.linespace = mParagraphStyles[styles[loop]].linespace ;
        }
        if(mParagraphStyles[styles[loop]].linetype != "")
        {
            pstyle.linetype = mParagraphStyles[styles[loop]].linetype ;
        }
        if(mParagraphStyles[styles[loop]].outlinelevel != "")
        {
            pstyle.outlinelevel = mParagraphStyles[styles[loop]].outlinelevel ;
        }

        // indents
        if(mParagraphStyles[styles[loop]].left != "")
        {
            pstyle.left = mParagraphStyles[styles[loop]].left ;
        }
        if(mParagraphStyles[styles[loop]].right != "")
        {
            pstyle.right = mParagraphStyles[styles[loop]].right ;
        }
        if(mParagraphStyles[styles[loop]].hanging != "")
        {
            pstyle.hanging = mParagraphStyles[styles[loop]].hanging ;
        }
        if(mParagraphStyles[styles[loop]].firstline != "")
        {
            pstyle.firstline = mParagraphStyles[styles[loop]].firstline ;
        }

        // justification
        if(mParagraphStyles[styles[loop]].justify != "")
        {
            pstyle.justify = mParagraphStyles[styles[loop]].justify ;
        }

        // custom tab stops -- a derived style's own tabs replace the
        // base style's wholesale, they don't merge stop-by-stop
        if(!mParagraphStyles[styles[loop]].tabs.empty())
        {
            pstyle.tabs = mParagraphStyles[styles[loop]].tabs ;
        }

        // character properties - font size
        if(mParagraphStyles[styles[loop]].charprops.size != "")
        {
            pstyle.charprops.size = mParagraphStyles[styles[loop]].charprops.size ;
        }
        if(mParagraphStyles[styles[loop]].charprops.cssize != "")
        {
            pstyle.charprops.cssize = mParagraphStyles[styles[loop]].charprops.cssize ;
        }

        // character properties - attributes (xor with previous value)
        pstyle.charprops.bold = mParagraphStyles[styles[loop]].charprops.bold ^ mParagraphStyles[styles[loop - 1]].charprops.bold ;
        pstyle.charprops.italics = mParagraphStyles[styles[loop]].charprops.italics ^ mParagraphStyles[styles[loop - 1]].charprops.italics ;
        pstyle.charprops.underline = mParagraphStyles[styles[loop]].charprops.underline ^ mParagraphStyles[styles[loop - 1]].charprops.underline ;
        pstyle.charprops.strikethrough = mParagraphStyles[styles[loop]].charprops.strikethrough ^ mParagraphStyles[styles[loop - 1]].charprops.strikethrough ;
        pstyle.charprops.superscript = mParagraphStyles[styles[loop]].charprops.superscript ^ mParagraphStyles[styles[loop - 1]].charprops.superscript ;
        pstyle.charprops.subscript = mParagraphStyles[styles[loop]].charprops.subscript ^ mParagraphStyles[styles[loop - 1]].charprops.subscript ;
        pstyle.charprops.smallcaps = mParagraphStyles[styles[loop]].charprops.smallcaps ^ mParagraphStyles[styles[loop - 1]].charprops.smallcaps ;
        pstyle.charprops.shadow = mParagraphStyles[styles[loop]].charprops.shadow ^ mParagraphStyles[styles[loop - 1]].charprops.shadow ;
    }

    return pstyle ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  style [in] index into mCharacterStyles for the leaf style
///
/// @return merged character style with all inherited properties resolved
///
/// @brief
/// Walk the basedOn chain for a character style, collecting all ancestor
/// styles, then merge from root to leaf. Font size overrides when
/// non-empty. Boolean attributes use XOR to toggle inheritance.
///
/////////////////////////////////////////////////////////////////////////////
sDOCXCharacterStyle cDOCXFile::MergeCharacterStyles(int style)
{
    std::deque<int> styles ;    // our styles inheritance
    styles.push_back(style) ;

    while(mCharacterStyles[style].basedon.length() != 0)
    {
        style = FindCharacterStyle(mCharacterStyles[style].basedon) ;
        styles.push_front(style) ;
    } ;

    sDOCXCharacterStyle cstyle = mCharacterStyles[styles[0]] ;
    for(size_t loop = 1 ; loop < styles.size(); loop++)
    {
        // character properties - font size
        if(mCharacterStyles[styles[loop]].charprops.size != "")
        {
            cstyle.charprops.size = mCharacterStyles[styles[loop]].charprops.size ;
        }
        if(mCharacterStyles[styles[loop]].charprops.cssize != "")
        {
            cstyle.charprops.cssize = mCharacterStyles[styles[loop]].charprops.cssize ;
        }

        // character properties - attributes (xor with previous value)
        cstyle.charprops.bold = mCharacterStyles[styles[loop]].charprops.bold ^ mCharacterStyles[styles[loop - 1]].charprops.bold ;
        cstyle.charprops.italics = mCharacterStyles[styles[loop]].charprops.italics ^ mCharacterStyles[styles[loop - 1]].charprops.italics ;
        cstyle.charprops.underline = mCharacterStyles[styles[loop]].charprops.underline ^ mCharacterStyles[styles[loop - 1]].charprops.underline ;
        cstyle.charprops.strikethrough = mCharacterStyles[styles[loop]].charprops.strikethrough ^ mCharacterStyles[styles[loop - 1]].charprops.strikethrough ;
        cstyle.charprops.superscript = mCharacterStyles[styles[loop]].charprops.superscript ^ mCharacterStyles[styles[loop - 1]].charprops.superscript ;
        cstyle.charprops.subscript = mCharacterStyles[styles[loop]].charprops.subscript ^ mCharacterStyles[styles[loop - 1]].charprops.subscript ;
        cstyle.charprops.smallcaps = mCharacterStyles[styles[loop]].charprops.smallcaps ^ mCharacterStyles[styles[loop - 1]].charprops.smallcaps ;
        cstyle.charprops.shadow = mCharacterStyles[styles[loop]].charprops.shadow ^ mCharacterStyles[styles[loop - 1]].charprops.shadow ;
    }

    return cstyle ;
}




/////////////////////////////////////////////////////////////////////////////
///
/// @param  pstyle [in] merged paragraph style
/// @param  cstyle [in] merged character style
///
/// @return final paragraph style with character style overrides applied
///
/// @brief
/// Merge a character style into a paragraph style. Font size overrides
/// when non-empty. Boolean attributes use XOR between the two styles.
///
/////////////////////////////////////////////////////////////////////////////
sDOCXParagraphStyle cDOCXFile::MergeStyles(sDOCXParagraphStyle &pstyle, sDOCXCharacterStyle &cstyle)
{
    sDOCXParagraphStyle finalstyle = pstyle ;

    // character properties - font size
    if(cstyle.charprops.size != "")
    {
        finalstyle.charprops.size = cstyle.charprops.size ;
    }
    if(cstyle.charprops.cssize != "")
    {
        finalstyle.charprops.cssize = cstyle.charprops.cssize ;
    }

    // character properties - attributes (xor with previous value)
    finalstyle.charprops.bold = cstyle.charprops.bold ^ pstyle.charprops.bold ;
    finalstyle.charprops.italics = cstyle.charprops.italics ^ pstyle.charprops.italics ;
    finalstyle.charprops.underline = cstyle.charprops.underline ^ pstyle.charprops.underline ;
    finalstyle.charprops.strikethrough = cstyle.charprops.strikethrough ^ pstyle.charprops.strikethrough ;
    finalstyle.charprops.superscript = cstyle.charprops.subscript ^ pstyle.charprops.superscript ;
    finalstyle.charprops.subscript = cstyle.charprops.subscript ^ pstyle.charprops.subscript ;
    finalstyle.charprops.smallcaps = cstyle.charprops.smallcaps ^ pstyle.charprops.smallcaps ;
    finalstyle.charprops.shadow = cstyle.charprops.shadow ^ pstyle.charprops.shadow ;

    return finalstyle ;
}



/////////////////////////////////////////////////////////////////////////////
///
/// @param  node [in] the w:sectPr XML node
///
/// @return nothing
///
/// @brief
/// Read section properties (page size, margins, header/footer margins)
/// from the w:sectPr XML node and emit corresponding WordStar dot
/// commands (.po, .rm, .mt, .mb, .hm, .fm).
///
/////////////////////////////////////////////////////////////////////////////
void cDOCXFile::HandleSection(pugi::xml_node node)
{
    // headers and footers here

    std::string temp ;
    int left = 0 ;
//    int height = 0 ;
    int width = 0 ;
    temp = node.child("w:pgSz").attribute("w:w").value() ;
    if(!temp.empty())
    {
        width = atoi(temp.c_str()) ;
    }

    temp = node.child("w:pgSz").attribute("w:h").value() ;
    {
//        height = atoi(temp.c_str()) ;
    }

    temp = node.child("w:pgMar").attribute("w:left").value() ;
    if(!temp.empty())
    {
        left = atoi(temp.c_str()) ;
        double in = atof(temp.c_str()) / TWIPSPERINCH ;
        std::string out ;
        out = string_sprintf(".po %.2fi\n", in) ;
        mDocument->Insert(out) ;
    }

    temp = node.child("w:pgMar").attribute("w:right").value() ;
    if(!temp.empty())
    {
        double in = width - left - atoi(temp.c_str()) ;
        in = in / TWIPSPERINCH ;
        std::string out ;
        out = string_sprintf(".rm %.2fi\n", in) ;
        mDocument->Insert(out) ;
    }

    temp = node.child("w:pgMar").attribute("w:top").value() ;
    if(!temp.empty())
    {
        double in = atof(temp.c_str()) / TWIPSPERINCH ;
        std::string out ;
        out = string_sprintf(".mt %.2fi\n", in) ;
        mDocument->Insert(out) ;
    }

    temp = node.child("w:pgMar").attribute("w:bottom").value() ;
    if(!temp.empty())
    {
        double in = atof(temp.c_str()) / TWIPSPERINCH ;
        std::string out ;
        out = string_sprintf(".mb %.2fi\n", in) ;
        mDocument->Insert(out) ;
    }

    temp = node.child("w:pgMar").attribute("w:header").value() ;
    if(!temp.empty())
    {
        double in = atof(temp.c_str()) / TWIPSPERINCH ;
        std::string out ;
        out = string_sprintf(".hm %.2fi\n", in) ;
        mDocument->Insert(out) ;
    }

    temp = node.child("w:pgMar").attribute("w:footer").value() ;
    if(!temp.empty())
    {
        double in = atof(temp.c_str()) / TWIPSPERINCH ;
        std::string out ;
        out = string_sprintf(".fm %.2fi\n", in) ;
        mDocument->Insert(out) ;
    }

    temp = node.child("w:pgMar").attribute("w:gutter").value() ;
    if(!temp.empty())
    {
//        double in = atof(temp.c_str()) / TWIPSPERINCH ;
//        std::string out ;
//        out = string_sprintf(".lm %.2fi\n", in) ;
//        mDocument->Insert(out) ;
    }


}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  node [in] the w:p XML node for inline overrides
/// @param  run [in] the current w:r run node (unused)
/// @param  style [in] resolved paragraph style
///
/// @return nothing
///
/// @brief
/// Emit paragraph spacing dot commands (.psb, .psa, .ls) when the
/// current paragraph spacing differs from the previous. Checks for
/// inline overrides in the paragraph's w:pPr node.
///
/////////////////////////////////////////////////////////////////////////////
void cDOCXFile::EmitParagraphSpace(pugi::xml_node &node, pugi::xml_node run, sDOCXParagraphStyle &style)
{
    UNUSED_ARGUMENT(run) ;

    // get the space before and after from the paragraph style
    std::string stylename = node.child("w:pPr").child("w:pStyle").attribute("w:val").value() ;

    int spacebefore = atoi(style.before.c_str()) ;
    int spaceafter = atoi(style.after.c_str()) ;
    int linespace = atoi(style.linespace.c_str()) ;
    std::string linetype = style.linetype ;


    // see if there are overrides in this paragraph
    std::string temp ;
    temp = node.child("w:pPr").child("w:spacing").attribute("w:before").value() ;
    if(!temp.empty())
    {
        spacebefore = atoi(temp.c_str()) ;
    }
    temp = node.child("w:pPr").child("w:spacing").attribute("w:after").value() ;
    if(!temp.empty())
    {
        spaceafter = atoi(temp.c_str()) ;
    }
    temp = node.child("w:pPr").child("w:spacing").attribute("w:line").value() ;
    if(!temp.empty())
    {
        linespace = atoi(temp.c_str()) ;
    }
    temp = node.child("w:pPr").child("w:spacing").attribute("w:lineRule").value() ;
    if(!temp.empty())
    {
        linetype = temp.c_str() ;
    }

    if((spacebefore != mSpaceBefore) && !style.before.empty())
    {
        double before = static_cast<double>(spacebefore) / TWIPSPERINCH ;
        std::string out ;
        out = string_sprintf(".psb %.2fi\n", before) ;

        mDocument->Insert(out) ;
        mSpaceBefore = spacebefore ;
    }

    if((spaceafter != mSpaceAfter) && !style.after.empty())
    {
        double after = static_cast<double>(spaceafter) / TWIPSPERINCH ;
        std::string out ;
        out = string_sprintf(".psa %.2fi\n", after) ;

        mDocument->Insert(out) ;
        mSpaceAfter = spaceafter ;
    }

    bool changels = false ;
    if((linespace != mLineSpace) && !style.linespace.empty())
    {
        changels = true ;
    }
    if((linetype != mLineType) && !style.linetype.empty())
    {
        changels = true ;
    }
    if(changels)
    {
        if(linetype == "auto")
        {
            double ls = linespace / 240.0 ;
            std::string out ;
            out = string_sprintf(".ls %.2fi\n", ls) ;

            mDocument->Insert(out) ;
            mLineSpace = linespace ;
            mLineType = linetype ;
        }
        else
        {
            /// @todo not quite sure what to do here, so duplicating the above
            double ls = linespace / 240.0 ;
            std::string out ;
            out = string_sprintf(".ls %.2fi\n", ls) ;

            mDocument->Insert(out) ;
            mLineSpace = linespace ;
            mLineType = linetype ;
        }
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  node [in] the w:p XML node (unused)
/// @param  run [in] the current w:r run node for inline font overrides
/// @param  style [in] resolved paragraph style for default font
///
/// @return nothing
///
/// @brief
/// Emit a font change if the current run's font name or size differs
/// from the active font. Checks inline run properties for w:rFonts and
/// w:sz overrides, then calls InsertFont() when changed.
///
/////////////////////////////////////////////////////////////////////////////
void cDOCXFile::EmitFont(pugi::xml_node &node, pugi::xml_node run, sDOCXParagraphStyle &style)
{
    UNUSED_ARGUMENT(node) ;
    std::string fname = style.asciifont ;
    double fsize = atof(style.charprops.size.c_str()) / 2.0 ;

    std::string temp ;
    temp = run.child("w:rPr").child("w:rFonts").attribute("w:ascii").value() ;
    if(!temp.empty())
    {
        fname = temp ;
    }
    temp = run.child("w:rPr").child("w:sz").attribute("w:val").value() ;
    if(!temp.empty())
    {
        fsize = atof(temp.c_str()) / 2.0 ;
    }

    bool newfont = false ;

    if(!fname.empty())
    {
        if(fname != mFontName)
        {
            mFontName = fname ;
            newfont = true ;
        }
    }

    if(fsize != 0 && !style.charprops.size.empty())
    {
        if(fsize != mFontSize)
        {
            mFontSize = fsize ;
            newfont = true ;
        }
    }

    if(newfont)
    {
        sInternalFonts font ;

        font.name = mFontName ;
        font.size = mFontSize ;
        font.haveWSFont = false ;

        mDocument->InsertFont(font) ;
    }
}



/////////////////////////////////////////////////////////////////////////////
///
/// @param  node [in] the w:p XML node (unused)
/// @param  run [in] the current w:r run node for inline attribute overrides
/// @param  style [in] resolved paragraph style for base attributes
///
/// @return nothing
///
/// @brief
/// Emit text attribute toggles (bold, italic, underline, strikethrough,
/// superscript, subscript, smallcaps, shadow). XORs the style attributes
/// with inline run properties, then toggles any attributes that differ
/// from the current state.
///
/////////////////////////////////////////////////////////////////////////////
void cDOCXFile::EmitAttributes(pugi::xml_node &node, pugi::xml_node run, sDOCXParagraphStyle &style)
{
    UNUSED_ARGUMENT(node) ;
    // get the styles attributes
    bool bold = style.charprops.bold ;
    bool italics = style.charprops.italics ;
    bool underline = style.charprops.underline ;
    bool strikethrough = style.charprops.strikethrough ;
    bool superscript = style.charprops.superscript ;
    bool subscript = style.charprops.subscript ;
    bool smallcaps = style.charprops.smallcaps ;
    bool shadow = style.charprops.shadow ;

    // and xor with the runs attributes
    pugi::xml_node attr ;

    attr = run.child("w:rPr").child("w:b") ;
    if(attr)
    {
        bold = true ^ bold ;
    }
    else
    {
        bold = false ^ bold ;
    }

    attr = run.child("w:rPr").child("w:i") ;
    if(attr)
    {
        italics = true ^ italics ;
    }
    else
    {
        italics = false ^ italics ;
    }

    attr = run.child("w:rPr").child("w:u") ;
    if(attr)
    {
        underline = true ^ underline ;
    }
    else
    {
        underline = false ^ underline ;
    }

    std::string val = run.child("w:rPr").child("w:strike").attribute("w:val").value() ;
    if(val == "true")
    {
        strikethrough = true ^ strikethrough ;
    }
    else
    {
        strikethrough = false ^ strikethrough ;
    }

    val = run.child("w:rPr").child("w:vertAlign").attribute("w:val").value() ;
    if(val == "superscript")
    {
        superscript = true ^ superscript;
    }
    else
    {
        superscript = false ^ superscript ;
    }

    if(val == "subscript")
    {
        subscript = true ^ subscript ;
    }
    else
    {
        subscript = false ^ subscript ;
    }

    val = run.child("w:rPr").child("w:smallCaps").attribute("w:val").value() ;
    if(val == "true")
    {
        smallcaps = true ^ smallcaps ;
    }
    else
    {
        smallcaps = false ^ smallcaps ;
    }

    val = run.child("w:rPr").child("w:shadow").attribute("w:val").value() ;
    if(val == "true")
    {
        shadow = true ^ shadow ;
    }
    else
    {
        shadow = false ^ shadow ;
    }

    // apply attributes
    if(mBold != bold)
    {
        mDocument->BeginBold() ;
        mBold = bold ;
    }

    if(mItalics != italics)
    {
        mDocument->BeginItalics() ;
        mItalics = italics ;
    }

    if(mUnderline != underline)
    {
        mDocument->BeginUnderline() ;
        mUnderline = underline ;
    }

    if(mStrikethrough != strikethrough)
    {
        mDocument->BeginStrikeThrough() ;
        mStrikethrough = strikethrough ;
    }

    if(mSuperscript != superscript)
    {
        mDocument->BeginSuperscript() ;
        mSuperscript = superscript ;
    }

    if(mSubscript != subscript)
    {
        mDocument->BeginSubscript() ;
        mSubscript = subscript ;
    }

    if(mSmallcaps != smallcaps)
    {
//        mDocument->BeginBold() ;
        mSmallcaps = smallcaps ;
    }

    if(mShadow != shadow)
    {
//        mDocument->Begin() ;
        mShadow = shadow ;
    }

}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  node [in] the w:p XML node for inline justification override
/// @param  run [in] the current w:r run node (unused)
/// @param  style [in] resolved paragraph style for default justification
///
/// @return nothing
///
/// @brief
/// Emit justification changes. Maps DOCX justification values (both,
/// right, center, left) to WordStar justification commands and emits
/// when changed.
///
/////////////////////////////////////////////////////////////////////////////
void cDOCXFile::EmitJustify(pugi::xml_node &node, pugi::xml_node run, sDOCXParagraphStyle &style)
{
    UNUSED_ARGUMENT(run) ;
    std::string tmp = node.child("w:pPr").child("w:jc").attribute("w:val").value() ;
    if(tmp.empty())
    {
        tmp = style.justify ;
    }

    eJustification align = JUST_LEFT ;

    if(tmp == "both")
    {
        align = JUST_JUST ;
    }
    else if(tmp == "right")
    {
        align = JUST_RIGHT ;
    }
    else if(tmp == "center")
    {
        align = JUST_CENTER ;
    }
    else
    {
        align = JUST_LEFT ;
    }

    if(align != mAlign)
    {
        switch(align)
        {
            case JUST_JUST :
                mDocument->BeginJustify() ;
                break ;

            case JUST_CENTER :
                mDocument->BeginCenter() ;
                break ;

            case JUST_RIGHT :
                mDocument->BeginRight() ;
                break ;

            case JUST_LEFT :
                mDocument->BeginLeft() ;
                break ;
        }

        mAlign = align ;
    }
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Insert a WordStar page break dot command (.pa) into the document.
///
/////////////////////////////////////////////////////////////////////////////
void cDOCXFile::EmitPage(void)
{
    mDocument->Insert(".pa\n") ;
}



/////////////////////////////////////////////////////////////////////////////
///
/// @param  node [in] the w:p XML node for inline indent overrides
/// @param  run [in] the current w:r run node (unused)
/// @param  style [in] resolved paragraph style for default indents
///
/// @return nothing
///
/// @brief
/// Emit first-line indent changes as a .pm dot command. Checks for
/// inline w:firstLine override and emits when the value differs from
/// the current state.
///
/////////////////////////////////////////////////////////////////////////////
void cDOCXFile::EmitIndent(pugi::xml_node &node, pugi::xml_node run, sDOCXParagraphStyle &style)
{
    UNUSED_ARGUMENT(run) ;
//    double left = atof(style.left.c_str()) ;
//    double right = atof(style.right.c_str()) ;
    double firstline = atof(style.firstline.c_str()) ;
/*
    // 2006 spec says 'left' 2011 spec says 'start'
    std::string tmp = node.child("w:pPr").child("w:ind").attribute("w:left").value() ;
    if(tmp.empty())
    {
        tmp = node.child("w:pPr").child("w:ind").attribute("w:start").value() ;
    }
    if(!tmp.empty())
    {
        left = atof(tmp.c_str()) ;
        space = space / TWIPSPERINCH ;

        std::string out = string_sprintf(".lm %.2fi\n", space) ;
        mDocument->Insert(out) ;
    }

    // 2006 spec says 'right' 2011 spec says 'end'
    tmp = node.child("w:pPr").child("w:ind").attribute("w:right").value() ;
    if(tmp.empty())
    {
        tmp = node.child("w:pPr").child("w:ind").attribute("w:end").value() ;
    }
    if(!tmp.empty())
    {
        right = atof(tmp.c_str()) ;
        space = space / TWIPSPERINCH ;

        std::string out = string_sprintf(".rm %.2fi\n", space) ;
        mDocument->Insert(out) ;
    }


    tmp = node.child("w:pPr").child("w:ind").attribute("w:hanging").value() ;
    if(!tmp.empty())
    {
        ///< @todo hanging
    }
*/
    std::string tmp = node.child("w:pPr").child("w:ind").attribute("w:firstLine").value() ;
    if(!tmp.empty())
    {
        firstline = atof(tmp.c_str()) ;
    }

    if((firstline != mFirstline) && !style.firstline.empty())
    {
        double space = firstline / TWIPSPERINCH ;

        std::string out = string_sprintf(".pm %.2fi\n", space) ;
        mDocument->Insert(out) ;
        mFirstline = firstline ;
    }
}



/////////////////////////////////////////////////////////////////////////////
///
/// @param  node [in] the w:p XML node for inline tab-stop overrides
/// @param  style [in] resolved paragraph style carrying any style-level tabs
///
/// @return nothing
///
/// @brief
/// Emit a .tb dot command for custom tab stops (w:pPr/w:tabs), when the
/// paragraph or its style define any and they differ from the last ones
/// emitted. A paragraph with no explicit tabs of its own, and no inherited
/// style tabs, is left alone rather than reverting to a default -- the
/// same simplification EmitIndent already makes for indents.
///
/////////////////////////////////////////////////////////////////////////////
void cDOCXFile::EmitTabs(pugi::xml_node &node, sDOCXParagraphStyle &style)
{
    std::vector<sDOCXTabStop> tabs = style.tabs ;

    pugi::xml_node inlineTabs = node.child("w:pPr").child("w:tabs") ;
    if(inlineTabs)
    {
        tabs.clear() ;
        for(pugi::xml_node tab = inlineTabs.child("w:tab") ; tab ; tab = tab.next_sibling("w:tab"))
        {
            sDOCXTabStop tabstop ;
            tabstop.val = tab.attribute("w:val").value() ;
            tabstop.pos = tab.attribute("w:pos").value() ;
            tabs.push_back(tabstop) ;
        }
    }

    if(tabs.empty())
    {
        return ;
    }

    bool same = (tabs.size() == mCurrentTabs.size()) ;
    if(same)
    {
        for(size_t loop = 0 ; loop < tabs.size() ; loop++)
        {
            if(tabs[loop].pos != mCurrentTabs[loop].pos || tabs[loop].val != mCurrentTabs[loop].val)
            {
                same = false ;
                break ;
            }
        }
    }
    if(same)
    {
        return ;
    }

    std::string out = ".tb " ;
    for(const sDOCXTabStop &tab : tabs)
    {
        if(tab.val == "clear")
        {
            continue ;
        }

        double in = atof(tab.pos.c_str()) / TWIPSPERINCH ;

        std::string prefix ;
        if(tab.val == "center")
        {
            prefix = "^" ;
        }
        else if(tab.val == "right")
        {
            prefix = ">" ;
        }
        else if(tab.val == "decimal")
        {
            prefix = "#" ;
        }

        out += string_sprintf("%s%.2fi ", prefix.c_str(), in) ;
    }
    out += "\n" ;

    mDocument->Insert(out) ;
    mCurrentTabs = tabs ;
}



/////////////////////////////////////////////////////////////////////////////
///
/// @param  count [in] the 1-based counter value for this list item
/// @param  format [in] the w:numFmt value (decimal, bullet, lowerLetter, ...)
///
/// @return the formatted counter text, e.g. "3", "c", "iii", "III"
///
/// @brief
/// Format a numbering counter per a DOCX w:numFmt. Unrecognized formats
/// fall back to plain decimal rather than emitting nothing.
///
/////////////////////////////////////////////////////////////////////////////
std::string cDOCXFile::FormatNumberingCounter(int count, const std::string &format)
{
    if(count < 1)
    {
        count = 1 ;
    }

    if(format == "lowerLetter" || format == "upperLetter")
    {
        // Word's own scheme: 1-26 = a..z, 27-52 = aa..zz, 53-78 = aaa..zzz, ...
        int cycle = (count - 1) / 26 + 1 ;
        char letter = static_cast<char>('a' + ((count - 1) % 26)) ;

        std::string result(static_cast<size_t>(cycle), letter) ;
        if(format == "upperLetter")
        {
            for(char &c : result)
            {
                c = static_cast<char>(toupper(c)) ;
            }
        }
        return result ;
    }

    if(format == "lowerRoman")
    {
        return ToRomanNumeralLower(count) ;
    }
    if(format == "upperRoman")
    {
        return ToRomanNumeralUpper(count) ;
    }

    if(format == "decimalZero")
    {
        std::string result = std::to_string(count) ;
        if(result.length() < 2)
        {
            result = "0" + result ;
        }
        return result ;
    }

    // "decimal" and anything unrecognized
    return std::to_string(count) ;
}



/////////////////////////////////////////////////////////////////////////////
///
/// @param  node [in] the w:p XML node
///
/// @return nothing
///
/// @brief
/// Emit a literal list marker (e.g. "1. ", "b) ", "-  ") before a numbered
/// or bulleted paragraph's own text, resolved from word/numbering.xml
/// against the paragraph's w:pPr/w:numPr. WordStar has no live
/// auto-numbering field, so the marker is plain inserted text -- correct
/// once, not automatically renumbered if the source list is edited later.
/// A shallower level advancing resets any deeper levels of the same list,
/// matching Word's own outline-numbering behavior.
///
/////////////////////////////////////////////////////////////////////////////
void cDOCXFile::EmitNumbering(pugi::xml_node &node)
{
    pugi::xml_node numPr = node.child("w:pPr").child("w:numPr") ;
    if(!numPr)
    {
        return ;
    }

    std::string numId = numPr.child("w:numId").attribute("w:val").value() ;
    if(numId.empty())
    {
        return ;
    }

    std::string ilvlstr = numPr.child("w:ilvl").attribute("w:val").value() ;
    int ilvl = ilvlstr.empty() ? 0 : atoi(ilvlstr.c_str()) ;

    auto numIt = mNumIdToAbstractId.find(numId) ;
    if(numIt == mNumIdToAbstractId.end())
    {
        return ;
    }
    auto defIt = mNumberingDefs.find(numIt->second) ;
    if(defIt == mNumberingDefs.end())
    {
        return ;
    }
    auto lvlIt = defIt->second.levels.find(ilvl) ;
    if(lvlIt == defIt->second.levels.end())
    {
        return ;
    }
    const sDOCXNumberingLevel &level = lvlIt->second ;

    if(level.format == "none")
    {
        return ;
    }

    // A shallower level advancing restarts any deeper levels of this list --
    // erase them rather than zero them, so the next use of that level applies
    // its own w:start instead of silently resuming from 0.
    for(auto it = mNumberingCounters.begin() ; it != mNumberingCounters.end() ; )
    {
        if(it->first.first == numId && it->first.second > ilvl)
        {
            it = mNumberingCounters.erase(it) ;
        }
        else
        {
            ++it ;
        }
    }

    std::pair<std::string, int> key(numId, ilvl) ;
    auto countIt = mNumberingCounters.find(key) ;
    int count = (countIt == mNumberingCounters.end()) ? level.start : countIt->second + 1 ;
    mNumberingCounters[key] = count ;

    std::string marker ;
    if(level.format == "bullet")
    {
        marker = "-  " ;    // a plain-text stand-in; the real lvlText is usually a symbol-font glyph
    }
    else
    {
        // A compound lvlText like "%1.%2." (common outline/legal numbering)
        // needs every ancestor level's own placeholder substituted with that
        // level's own current counter and its own number format, not just
        // this level's.
        std::string text = level.text ;
        bool foundOwnPlaceholder = false ;

        for(int ancestorLvl = 0 ; ancestorLvl <= ilvl ; ancestorLvl++)
        {
            int ancestorCount ;
            std::string ancestorFormat ;

            if(ancestorLvl == ilvl)
            {
                ancestorCount = count ;
                ancestorFormat = level.format ;
            }
            else
            {
                std::pair<std::string, int> ancestorKey(numId, ancestorLvl) ;
                auto ancestorCountIt = mNumberingCounters.find(ancestorKey) ;
                auto ancestorLvlIt = defIt->second.levels.find(ancestorLvl) ;
                if(ancestorCountIt == mNumberingCounters.end() || ancestorLvlIt == defIt->second.levels.end())
                {
                    continue ;    // that ancestor level was never actually used -- leave its placeholder as-is
                }
                ancestorCount = ancestorCountIt->second ;
                ancestorFormat = ancestorLvlIt->second.format ;
            }

            std::string placeholder = string_sprintf("%%%d", ancestorLvl + 1) ;
            size_t pos = text.find(placeholder) ;
            if(pos != std::string::npos)
            {
                text.replace(pos, placeholder.length(), FormatNumberingCounter(ancestorCount, ancestorFormat)) ;
                if(ancestorLvl == ilvl)
                {
                    foundOwnPlaceholder = true ;
                }
            }
        }

        marker = foundOwnPlaceholder ? (text + "  ") : (FormatNumberingCounter(count, level.format) + ".  ") ;
    }

    mDocument->Insert(marker) ;
}



/////////////////////////////////////////////////////////////////////////////
///
/// @param  numbering [in] the root w:numbering node from word/numbering.xml
///
/// @return nothing
///
/// @brief
/// Parse abstract numbering definitions (w:abstractNum, one per w:lvl) and
/// the w:num -> w:abstractNumId mapping that paragraphs' w:numId values
/// resolve through.
///
/////////////////////////////////////////////////////////////////////////////
void cDOCXFile::ParseNumbering(pugi::xml_node numbering)
{
    for(pugi::xml_node abstractNum = numbering.child("w:abstractNum") ; abstractNum ; abstractNum = abstractNum.next_sibling("w:abstractNum"))
    {
        std::string abstractId = abstractNum.attribute("w:abstractNumId").value() ;

        sDOCXNumberingDefinition def ;
        for(pugi::xml_node lvl = abstractNum.child("w:lvl") ; lvl ; lvl = lvl.next_sibling("w:lvl"))
        {
            int ilvl = atoi(lvl.attribute("w:ilvl").value()) ;

            sDOCXNumberingLevel level ;
            level.format = lvl.child("w:numFmt").attribute("w:val").value() ;
            level.text = lvl.child("w:lvlText").attribute("w:val").value() ;

            std::string startstr = lvl.child("w:start").attribute("w:val").value() ;
            level.start = startstr.empty() ? 1 : atoi(startstr.c_str()) ;

            def.levels[ilvl] = level ;
        }

        mNumberingDefs[abstractId] = def ;
    }

    for(pugi::xml_node num = numbering.child("w:num") ; num ; num = num.next_sibling("w:num"))
    {
        std::string numId = num.attribute("w:numId").value() ;
        std::string abstractId = num.child("w:abstractNumId").attribute("w:val").value() ;
        mNumIdToAbstractId[numId] = abstractId ;
    }
}



//=============================================================================
// DOCX writing (Save As Word)
//
// Walks the document exactly the way cRTFWriter does (see rtf/write/rtfwriter.cpp):
// one pass over GetNumberofParagraphs()/GetParagraphText(), dot-command paragraphs
// update running formatting state, text paragraphs are emitted as one <w:p> each
// with that state applied to w:pPr (OOXML has no persistent state between
// paragraphs the way RTF control words do, so it's re-applied every time).
// Character-level formatting is scanned the same way CreateText() does: MARKER_CHAR
// bytes trigger GetControlChar()/GetFont()/GetColor() lookups that flip the current
// run's attributes.
//
// First-cut scope (see WordTsar macOS roadmap): plain paragraphs/runs, character
// formatting (bold/italic/underline/strikethrough/super/subscript/font/color),
// paragraph justification/indent/spacing, page size/margins, page breaks.
// Tables, headers/footers, columns, indexing and other dot commands are not
// translated -- unrecognized dot commands are silently skipped rather than
// producing broken output.
//=============================================================================

/////////////////////////////////////////////////////////////////////////////
///
/// @param  color [in] RGB color to format
///
/// @return 6-digit uppercase hex string (RRGGBB), "000000" for the default sentinel
///
/// @brief
/// Formats a document color as a DOCX w:color hex value.
///
/////////////////////////////////////////////////////////////////////////////
std::string cDOCXFile::ColorToHex(sSeqRGBColor &color)
{
    if (color.IsDefault())
    {
        return "000000" ;
    }

    char buf[8] ;
    snprintf(buf, sizeof(buf), "%02X%02X%02X",
             static_cast<unsigned char>(color.red),
             static_cast<unsigned char>(color.green),
             static_cast<unsigned char>(color.blue)) ;
    return std::string(buf) ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  paragraph [in] the w:p node to append the run to
/// @param  buffer [in/out] accumulated run text; cleared after flushing
///
/// @return nothing
///
/// @brief
/// Emits the buffered text as a single w:r run with rPr reflecting the
/// current character-formatting state (mWBold, mWItalics, etc.), then
/// clears the buffer. No-op if the buffer is empty.
///
/////////////////////////////////////////////////////////////////////////////
void cDOCXFile::FlushRun(pugi::xml_node &paragraph, std::string &buffer)
{
    if (buffer.empty())
    {
        return ;
    }

    pugi::xml_node run = paragraph.append_child("w:r") ;
    pugi::xml_node rPr = run.append_child("w:rPr") ;

    if (mWBold)
    {
        rPr.append_child("w:b") ;
    }
    if (mWItalics)
    {
        rPr.append_child("w:i") ;
    }
    if (mWUnderline)
    {
        rPr.append_child("w:u").append_attribute("w:val") = "single" ;
    }
    if (mWStrikethrough)
    {
        rPr.append_child("w:strike") ;
    }
    if (mWSuperscript)
    {
        rPr.append_child("w:vertAlign").append_attribute("w:val") = "superscript" ;
    }
    else if (mWSubscript)
    {
        rPr.append_child("w:vertAlign").append_attribute("w:val") = "subscript" ;
    }
    if (!mWFontName.empty())
    {
        pugi::xml_node rFonts = rPr.append_child("w:rFonts") ;
        rFonts.append_attribute("w:ascii") = mWFontName.c_str() ;
        rFonts.append_attribute("w:hAnsi") = mWFontName.c_str() ;
    }
    if (mWFontSize > 0.0)
    {
        rPr.append_child("w:sz").append_attribute("w:val") = static_cast<int>(mWFontSize * 2.0) ;
    }
    if (!mWColor.IsDefault())
    {
        rPr.append_child("w:color").append_attribute("w:val") = ColorToHex(mWColor).c_str() ;
    }

    if (!rPr.first_child())
    {
        run.remove_child(rPr) ;
    }

    pugi::xml_node t = run.append_child("w:t") ;
    t.append_attribute("xml:space") = "preserve" ;
    t.text().set(buffer.c_str()) ;

    buffer.clear() ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  body [in] the w:body node to append the paragraph to
/// @param  text [in] the paragraph's text (including embedded MARKER_CHAR
///                    control bytes), trailing newline stripped in place
///
/// @return nothing
///
/// @brief
/// Emits one text paragraph as a w:p element: paragraph properties from the
/// current running state (justification/indent/spacing), then one or more
/// w:r runs built by scanning the text for MARKER_CHAR-coded style toggles,
/// tabs, font changes, and color changes -- the same control-byte protocol
/// cRTFWriter::CreateText() reads.
///
/////////////////////////////////////////////////////////////////////////////
void cDOCXFile::SaveParagraph(pugi::xml_node &body, std::string &text)
{
    // GetParagraphText() includes the trailing paragraph terminator; strip it
    // so it doesn't end up as a stray run of text.
    while (!text.empty() && (text.back() == '\n' || text.back() == static_cast<char>(HARD_RETURN)))
    {
        text.pop_back() ;
    }

    pugi::xml_node para = body.append_child("w:p") ;

    pugi::xml_node pPr = para.append_child("w:pPr") ;

    pugi::xml_node jc = pPr.append_child("w:jc") ;
    switch (mWAlign)
    {
        case JUST_CENTER :
            jc.append_attribute("w:val") = "center" ;
            break ;
        case JUST_RIGHT :
            jc.append_attribute("w:val") = "right" ;
            break ;
        case JUST_JUST :
            jc.append_attribute("w:val") = "both" ;
            break ;
        default :
            jc.append_attribute("w:val") = "left" ;
            break ;
    }

    if (mWLeftMargin != 0 || mWFirstLine != 0)
    {
        pugi::xml_node ind = pPr.append_child("w:ind") ;
        if (mWLeftMargin != 0)
        {
            ind.append_attribute("w:left") = mWLeftMargin ;
        }

        // .pm is absolute from the page offset, like RTF's \fi is relative to \li:
        // relative first-line offset = .pm - .lm.
        int firstlineRelative = mWFirstLine - mWLeftMargin ;
        if (firstlineRelative > 0)
        {
            ind.append_attribute("w:firstLine") = firstlineRelative ;
        }
        else if (firstlineRelative < 0)
        {
            ind.append_attribute("w:hanging") = -firstlineRelative ;
        }
    }

    if (mWSpaceBefore != 0 || mWSpaceAfter != 0 || mWLineSpaceMult > 0.0)
    {
        pugi::xml_node spacing = pPr.append_child("w:spacing") ;
        if (mWSpaceBefore != 0)
        {
            spacing.append_attribute("w:before") = mWSpaceBefore ;
        }
        if (mWSpaceAfter != 0)
        {
            spacing.append_attribute("w:after") = mWSpaceAfter ;
        }
        if (mWLineSpaceMult > 0.0)
        {
            spacing.append_attribute("w:line") = static_cast<int>(mWLineSpaceMult * 240.0) ;
            spacing.append_attribute("w:lineRule") = "auto" ;
        }
    }

    std::string buffer ;
    size_t pos = 0 ;
    while (pos < text.size())
    {
        unsigned char byte = static_cast<unsigned char>(text[pos]) ;

        if (byte == static_cast<unsigned char>(REPLACE_CHAR) ||
            byte == static_cast<unsigned char>(SAVE_CHAR))
        {
            pos++ ;
            mWCurrentPosition++ ;
            continue ;
        }

        if (byte == static_cast<unsigned char>(MARKER_CHAR))
        {
            unsigned char ch = static_cast<unsigned char>(mDocument->GetControlChar(mWCurrentPosition)) ;

            switch (ch)
            {
                case STYLE_BOLD :
                    FlushRun(para, buffer) ;
                    mWBold = !mWBold ;
                    break ;

                case STYLE_ITALICS :
                    FlushRun(para, buffer) ;
                    mWItalics = !mWItalics ;
                    break ;

                case STYLE_UNDERLINE :
                    FlushRun(para, buffer) ;
                    mWUnderline = !mWUnderline ;
                    break ;

                case STYLE_STRIKETHROUGH :
                    FlushRun(para, buffer) ;
                    mWStrikethrough = !mWStrikethrough ;
                    break ;

                case STYLE_SUPERSCRIPT :
                    FlushRun(para, buffer) ;
                    mWSuperscript = !mWSuperscript ;
                    if (mWSuperscript)
                    {
                        mWSubscript = false ;
                    }
                    break ;

                case STYLE_SUBSCRIPT :
                    FlushRun(para, buffer) ;
                    mWSubscript = !mWSubscript ;
                    if (mWSubscript)
                    {
                        mWSuperscript = false ;
                    }
                    break ;

                case STYLE_TAB :
                {
                    FlushRun(para, buffer) ;
                    pugi::xml_node tabRun = para.append_child("w:r") ;
                    tabRun.append_child("w:tab") ;
                    break ;
                }

                case STYLE_FONT1 :
                {
                    sInternalFonts font ;
                    if (mDocument->GetFont(mWCurrentPosition, font))
                    {
                        FlushRun(para, buffer) ;
                        mWFontName = font.fontname ;
                        mWFontSize = font.size ;
                    }
                    break ;
                }

                case STYLE_INTERNAL_COLOR :
                    FlushRun(para, buffer) ;
                    mDocument->GetColor(mWCurrentPosition, mWColor) ;
                    break ;

                default :
                    break ;
            }

            pos++ ;
            mWCurrentPosition++ ;
            continue ;
        }

        buffer += static_cast<char>(byte) ;
        pos++ ;
        mWCurrentPosition++ ;
    }

    FlushRun(para, buffer) ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  body [in] the w:body node (a page break is appended here directly)
/// @param  text [in] the dot command paragraph text
///
/// @return nothing
///
/// @brief
/// Updates running paragraph/page formatting state from a WordStar dot
/// command, mirroring the subset of cRTFWriter::CreateDot() this first cut
/// supports: margins (.po/.lm/.rm/.pm/.mt/.mb), justification (.oj/.oc),
/// spacing (.psb/.psa/.ls), page breaks (.pa, emitted immediately since
/// unlike the other commands it isn't paragraph state), and comment lines
/// (../.ig, emitted immediately as hidden text). Anything else is silently
/// ignored -- see the file-level comment above for what's left out.
///
/////////////////////////////////////////////////////////////////////////////
void cDOCXFile::SaveDotCommand(pugi::xml_node &body, std::string &text)
{
    std::string lowtext = text ;
    for (auto &c : lowtext)
    {
        c = static_cast<char>(tolower(static_cast<unsigned char>(c))) ;
    }

    bool incdec ;

    auto startsWith = [&](const char *prefix) -> bool
    {
        return lowtext.rfind(prefix, 0) == 0 ;
    } ;

    if (startsWith(".pa"))
    {
        pugi::xml_node para = body.append_child("w:p") ;
        pugi::xml_node run = para.append_child("w:r") ;
        run.append_child("w:br").append_attribute("w:type") = "page" ;
    }
    else if (startsWith(".oj"))
    {
        std::string rest = text.size() > 3 ? text.substr(3) : "" ;
        char c = rest.empty() ? 0 : static_cast<char>(tolower(static_cast<unsigned char>(rest[0]))) ;
        if (c == 'c')
        {
            mWAlign = JUST_CENTER ;
        }
        else if (c == 'r')
        {
            mWAlign = JUST_RIGHT ;
        }
        else if (c == 'j')
        {
            mWAlign = JUST_JUST ;
        }
        else
        {
            mWAlign = JUST_LEFT ;
        }
    }
    else if (startsWith(".oc"))
    {
        std::string rest = text.size() > 3 ? text.substr(3) : "" ;
        size_t start = rest.find_first_not_of(" \t") ;
        std::string trimmed = (start == std::string::npos) ? "" : rest.substr(start) ;
        for (auto &c : trimmed)
        {
            c = static_cast<char>(tolower(static_cast<unsigned char>(c))) ;
        }
        mWAlign = (trimmed.rfind("off", 0) == 0) ? JUST_LEFT : JUST_CENTER ;
    }
    else if (startsWith(".lm"))
    {
        double value = mDocument->GetValue(text.substr(3), incdec) ;
        char type = mDocument->GetType(text.substr(3)) ;
        mWLeftMargin = static_cast<int>(mDocument->ConvertToTwips(value, type)) ;
    }
    else if (startsWith(".rm"))
    {
        double value = mDocument->GetValue(text.substr(3), incdec) ;
        char type = mDocument->GetType(text.substr(3)) ;
        mWRightMarginPos = static_cast<int>(mDocument->ConvertToTwips(value, type)) ;
    }
    else if (startsWith(".pm"))
    {
        double value = mDocument->GetValue(text.substr(3), incdec) ;
        char type = mDocument->GetType(text.substr(3)) ;
        mWFirstLine = static_cast<int>(mDocument->ConvertToTwips(value, type)) ;
    }
    else if (startsWith(".po"))
    {
        std::string rest = text.substr(3) ;
        if (!rest.empty() && (rest[0] == 'o' || rest[0] == 'e'))
        {
            rest = rest.substr(1) ;   // .poo/.poe (odd/even) -- treated the same as .po here
        }
        double value = mDocument->GetValue(rest, incdec) ;
        char type = mDocument->GetType(rest) ;
        mWPageOffset = static_cast<int>(mDocument->ConvertToTwips(value, type)) ;
    }
    else if (startsWith(".mt"))
    {
        double value = mDocument->GetValue(text.substr(3), incdec) ;
        char type = mDocument->GetType(text.substr(3)) ;
        mWTopMargin = static_cast<int>(mDocument->ConvertToTwips(value, type)) ;
    }
    else if (startsWith(".mb"))
    {
        double value = mDocument->GetValue(text.substr(3), incdec) ;
        char type = mDocument->GetType(text.substr(3)) ;
        mWBottomMargin = static_cast<int>(mDocument->ConvertToTwips(value, type)) ;
    }
    else if (startsWith(".psb"))
    {
        double value = mDocument->GetValue(text.substr(4), incdec) ;
        char type = mDocument->GetType(text.substr(4)) ;
        mWSpaceBefore = static_cast<int>(mDocument->ConvertToTwips(value, type)) ;
    }
    else if (startsWith(".psa"))
    {
        double value = mDocument->GetValue(text.substr(4), incdec) ;
        char type = mDocument->GetType(text.substr(4)) ;
        mWSpaceAfter = static_cast<int>(mDocument->ConvertToTwips(value, type)) ;
    }
    else if (startsWith(".ls"))
    {
        std::string rest = text.size() > 3 ? text.substr(3) : "" ;
        try
        {
            mWLineSpaceMult = std::stod(rest) ;
        }
        catch (...)
        {
            mWLineSpaceMult = 0.0 ;
        }
    }
    else if (startsWith("..") || startsWith(".ig"))
    {
        // Preserve the comment text losslessly as hidden (non-printing) run
        // text rather than a real Word review comment -- that would need a
        // whole extra word/comments.xml part; w:vanish needs nothing but this
        // run. Visible only if the reader turns on Word's hidden-text display.
        bool isDotDot = startsWith("..") ;
        size_t prefixLen = isDotDot ? 2 : 3 ;

        // GetParagraphText() includes the trailing paragraph terminator, same
        // as SaveParagraph() strips before use.
        while (!text.empty() && (text.back() == '\n' || text.back() == static_cast<char>(HARD_RETURN)))
        {
            text.pop_back() ;
        }
        std::string rest = text.substr(prefixLen) ;

        // Drop (rather than interpret) any in-band control bytes -- style
        // toggles are resolved via mWCurrentPosition/GetControlChar(), which
        // this branch never advances, so passing them through raw would leak
        // literal control characters into the hidden run instead of styling.
        std::string clean ;
        clean.reserve(rest.size()) ;
        for (unsigned char byte : rest)
        {
            if (byte != static_cast<unsigned char>(MARKER_CHAR) &&
               byte != static_cast<unsigned char>(REPLACE_CHAR) &&
               byte != static_cast<unsigned char>(SAVE_CHAR))
            {
                clean += static_cast<char>(byte) ;
            }
        }

        pugi::xml_node para = body.append_child("w:p") ;
        // Hide the paragraph mark itself too, not just the run -- otherwise
        // the paragraph still occupies a blank line with hidden text off.
        para.append_child("w:pPr").append_child("w:rPr").append_child("w:vanish") ;
        pugi::xml_node run = para.append_child("w:r") ;
        run.append_child("w:rPr").append_child("w:vanish") ;
        pugi::xml_node t = run.append_child("w:t") ;
        t.append_attribute("xml:space") = "preserve" ;
        t.text().set(clean.c_str()) ;
    }
    // Anything else (headers/footers, columns, tabs, indexing, page numbering,
    // printer options, kerning, line numbering, ...) is intentionally not
    // translated in this first cut. Left out rather than guessed at.
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return the serialized word/document.xml content
///
/// @brief
/// Builds word/document.xml by walking the document paragraph by paragraph
/// (same traversal as cRTFWriter::CreateRTF()), then appends the final
/// w:sectPr with page size and margins.
///
/////////////////////////////////////////////////////////////////////////////
std::string cDOCXFile::BuildDocumentXml(void)
{
    pugi::xml_document doc ;

    pugi::xml_node decl = doc.append_child(pugi::node_declaration) ;
    decl.append_attribute("version") = "1.0" ;
    decl.append_attribute("encoding") = "UTF-8" ;
    decl.append_attribute("standalone") = "yes" ;

    pugi::xml_node root = doc.append_child("w:document") ;
    root.append_attribute("xmlns:w") = "http://schemas.openxmlformats.org/wordprocessingml/2006/main" ;

    pugi::xml_node body = root.append_child("w:body") ;

    size_t paras = mDocument->GetNumberofParagraphs() ;
    for (size_t loop = 0 ; loop < paras ; loop++)
    {
        std::string text = mDocument->GetParagraphText(loop) ;
        POSITION_T start, end ;
        mDocument->GetParagraphStartandEnd(loop, start, end) ;
        mWCurrentPosition = start ;

        if (text.empty())
        {
            continue ;
        }

        if (text[0] == '.')
        {
            SaveDotCommand(body, text) ;
        }
        else
        {
            SaveParagraph(body, text) ;
        }
    }

    pugi::xml_node sectPr = body.append_child("w:sectPr") ;

    pugi::xml_node pgSz = sectPr.append_child("w:pgSz") ;
    pgSz.append_attribute("w:w") = mWPaperWidth ;
    pgSz.append_attribute("w:h") = mWPaperHeight ;

    int marginRight = mWPaperWidth - mWPageOffset - mWRightMarginPos ;
    if (marginRight < 0)
    {
        marginRight = 0 ;
    }

    pugi::xml_node pgMar = sectPr.append_child("w:pgMar") ;
    pgMar.append_attribute("w:top") = mWTopMargin ;
    pgMar.append_attribute("w:right") = marginRight ;
    pgMar.append_attribute("w:bottom") = mWBottomMargin ;
    pgMar.append_attribute("w:left") = mWPageOffset ;
    pgMar.append_attribute("w:header") = 720 ;
    pgMar.append_attribute("w:footer") = 720 ;
    pgMar.append_attribute("w:gutter") = 0 ;

    std::ostringstream oss ;
    doc.save(oss) ;
    return oss.str() ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return the serialized [Content_Types].xml content
///
/// @brief
/// Package part declaring the content type of every part in the archive.
///
/////////////////////////////////////////////////////////////////////////////
std::string cDOCXFile::BuildContentTypesXml(void)
{
    return
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
        "<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">"
        "<Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/>"
        "<Default Extension=\"xml\" ContentType=\"application/xml\"/>"
        "<Override PartName=\"/word/document.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.wordprocessingml.document.main+xml\"/>"
        "<Override PartName=\"/word/styles.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.wordprocessingml.styles+xml\"/>"
        "<Override PartName=\"/docProps/core.xml\" ContentType=\"application/vnd.openxmlformats-package.core-properties+xml\"/>"
        "<Override PartName=\"/docProps/app.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.extended-properties+xml\"/>"
        "</Types>" ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return the serialized _rels/.rels content
///
/// @brief
/// Package-level relationships: points at the main document part and the
/// core/app property parts.
///
/////////////////////////////////////////////////////////////////////////////
std::string cDOCXFile::BuildRelsXml(void)
{
    return
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
        "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
        "<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument\" Target=\"word/document.xml\"/>"
        "<Relationship Id=\"rId2\" Type=\"http://schemas.openxmlformats.org/package/2006/relationships/metadata/core-properties\" Target=\"docProps/core.xml\"/>"
        "<Relationship Id=\"rId3\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/extended-properties\" Target=\"docProps/app.xml\"/>"
        "</Relationships>" ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return the serialized word/_rels/document.xml.rels content
///
/// @brief
/// Relationships local to the main document part: points at the styles part.
///
/////////////////////////////////////////////////////////////////////////////
std::string cDOCXFile::BuildDocumentRelsXml(void)
{
    return
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
        "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
        "<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/styles\" Target=\"styles.xml\"/>"
        "</Relationships>" ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return the serialized word/styles.xml content
///
/// @brief
/// Minimal style sheet: default run properties (Times New Roman 12pt,
/// matching WordTsar's own default) plus a Normal paragraph style. Full
/// style definitions aren't written -- SaveParagraph/FlushRun apply
/// formatting directly on each paragraph/run instead of referencing styles.
///
/////////////////////////////////////////////////////////////////////////////
std::string cDOCXFile::BuildStylesXml(void)
{
    return
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
        "<w:styles xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\">"
        "<w:docDefaults>"
        "<w:rPrDefault><w:rPr><w:rFonts w:ascii=\"Times New Roman\" w:hAnsi=\"Times New Roman\"/><w:sz w:val=\"24\"/></w:rPr></w:rPrDefault>"
        "</w:docDefaults>"
        "<w:style w:type=\"paragraph\" w:default=\"1\" w:styleId=\"Normal\"><w:name w:val=\"Normal\"/></w:style>"
        "</w:styles>" ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return the serialized docProps/core.xml content
///
/// @brief
/// Core document properties (Dublin Core creator metadata).
///
/////////////////////////////////////////////////////////////////////////////
std::string cDOCXFile::BuildCoreXml(void)
{
    return
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
        "<cp:coreProperties xmlns:cp=\"http://schemas.openxmlformats.org/package/2006/metadata/core-properties\" xmlns:dc=\"http://purl.org/dc/elements/1.1/\">"
        "<dc:creator>WordTsar</dc:creator>"
        "</cp:coreProperties>" ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return the serialized docProps/app.xml content
///
/// @brief
/// Extended (application) document properties.
///
/////////////////////////////////////////////////////////////////////////////
std::string cDOCXFile::BuildAppXml(void)
{
    return
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
        "<Properties xmlns=\"http://schemas.openxmlformats.org/officeDocument/2006/extended-properties\">"
        "<Application>WordTsar</Application>"
        "</Properties>" ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  filename [in] path to write the .docx to
///
/// @return true on success
///
/// @brief
/// Writes the document as a DOCX (OOXML) package: builds word/document.xml
/// from the document model, then zips it up with the fixed boilerplate
/// parts (content types, relationships, a minimal style sheet, core/app
/// properties) using the same kuba--/zip library cDOCXFile::LoadFile()
/// already reads DOCX files with, in write mode.
///
/////////////////////////////////////////////////////////////////////////////
bool cDOCXFile::WriteDocx(const std::string &filename)
{
    mWPageOffset = 1440 ;           // .po 1" default
    mWLeftMargin = 0 ;
    mWFirstLine = 0 ;
    mWRightMarginPos = 9360 ;       // .rm 6.5" default
    mWTopMargin = 1440 ;
    mWBottomMargin = 1440 ;
    mWSpaceBefore = 0 ;
    mWSpaceAfter = 0 ;
    mWLineSpaceMult = 0.0 ;
    mWAlign = JUST_LEFT ;

    mWBold = false ;
    mWItalics = false ;
    mWUnderline = false ;
    mWStrikethrough = false ;
    mWSuperscript = false ;
    mWSubscript = false ;
    mWFontName.clear() ;
    mWFontSize = 0.0 ;
    mWColor.red = -1 ;
    mWColor.green = -1 ;
    mWColor.blue = -1 ;
    mWColor.alpha = -1 ;

    mWPaperWidth = 12240 ;          // 8.5" fallback if no editor/layout available
    mWPaperHeight = 15840 ;         // 11" fallback
    if (mEditor != nullptr && mEditor->GetLayout() != nullptr)
    {
        mWPaperWidth = static_cast<int>(mEditor->GetLayout()->GetPaperWidth()) ;
        mWPaperHeight = static_cast<int>(mEditor->GetLayout()->GetPaperHeight()) ;
    }

    std::string documentXml = BuildDocumentXml() ;

    zip_t *zip = zip_open(filename.c_str(), ZIP_DEFAULT_COMPRESSION_LEVEL, 'w') ;
    if (zip == nullptr)
    {
        return false ;
    }

    auto writeEntry = [zip](const char *name, const std::string &content) -> bool
    {
        if (zip_entry_open(zip, name) != 0)
        {
            return false ;
        }
        bool ok = zip_entry_write(zip, content.data(), content.size()) == 0 ;
        zip_entry_close(zip) ;
        return ok ;
    } ;

    bool ok = true ;
    ok = ok && writeEntry("[Content_Types].xml", BuildContentTypesXml()) ;
    ok = ok && writeEntry("_rels/.rels", BuildRelsXml()) ;
    ok = ok && writeEntry("word/document.xml", documentXml) ;
    ok = ok && writeEntry("word/_rels/document.xml.rels", BuildDocumentRelsXml()) ;
    ok = ok && writeEntry("word/styles.xml", BuildStylesXml()) ;
    ok = ok && writeEntry("docProps/core.xml", BuildCoreXml()) ;
    ok = ok && writeEntry("docProps/app.xml", BuildAppXml()) ;

    zip_close(zip) ;

    return ok ;
}


/// @}


