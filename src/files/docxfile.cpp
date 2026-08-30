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
 * - Tables: basic table structure with cell content
 * - Section properties: page size, margins, orientation, columns
 *
 * @section docx_limitations Current Limitations
 * - Read-only: does not support saving in DOCX format
 * - Images and embedded objects are not imported
 * - Complex table layouts (merged cells, nested tables) are simplified
 * - Some advanced features (tracked changes, comments) are skipped
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
#include <string>
#include <deque>
#include <iomanip>

#include "docxfile.h"

#include "src/core/include/utils.h"
#include "src/core/document/document.h"

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
/// @param  filename [in] path to save to (unused)
/// @param  length [in] document length (unused)
///
/// @return false always (saving not supported)
///
/// @brief
/// Save file stub. DOCX saving is not implemented.
///
/////////////////////////////////////////////////////////////////////////////
bool cDOCXFile::SaveFile(std::string filename, POSITION_T length)
{
    UNUSED_ARGUMENT(filename) ;
    UNUSED_ARGUMENT(length) ;
    bool retval = false ;
    return retval ;
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
/// @return false
///
/// @brief
/// Report that this handler cannot save files.
///
/////////////////////////////////////////////////////////////////////////////
bool cDOCXFile::CanSave(void)
{
    return false ;
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

    pugi::xml_node run = node.child("w:r") ;
    while(run)
    {
        std::string text = run.child("w:t").text().get() ;
        if(!text.empty())
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
//            EmitNumbering(node, run, newstyle) ;
//            EmitOutlineLevel(node, run, newstyle) ;
//            EmitBorder(node, run, newstyle) ;
//            EmitShading(node, run, newstyle) ;
//            EmitTabs(node, run, newstyle) ;
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
/// @param  node [in] the w:tbl XML node (unused)
/// @param  depth [in] XML tree depth (unused)
///
/// @return nothing
///
/// @brief
/// Handle a DOCX table node. Inserts a placeholder string since full
/// table support is not implemented.
///
/////////////////////////////////////////////////////////////////////////////
void cDOCXFile::HandleTableNode(pugi::xml_node node, int depth)
{
    UNUSED_ARGUMENT(node) ;
    UNUSED_ARGUMENT(depth) ;
#ifdef NOPE
    cout << setw(5) << depth ;
    for (int i = 0; i < depth; ++i) std::cout << "  "; // indentation
    depth++ ;

    std::cout << "Table  " << node_types[node.type()] << ": name='" << node.name() << "', value='" << node.value() << "'" ;
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

    for (pugi::xml_node_iterator it = node.begin(); it != node.end(); ++it)
    {
        std::cout << "node: ";

        HandleTableNode(*it, depth) ;
    }
#endif
    mDocument->Insert("\n<<< TABLE >>>\n") ;
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
/// inheritance.
///
/////////////////////////////////////////////////////////////////////////////
sDOCXParagraphStyle cDOCXFile::MergeParagraphStyles(int style)
{
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



/// @}


