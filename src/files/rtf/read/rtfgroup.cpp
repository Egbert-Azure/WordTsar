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
 * @class cRTFGroup
 *
 * @brief RTF group parse tree node with child element management.
 *
 * Implements the cRTFGroup class, which represents a brace-delimited
 * group ({...}) in the RTF parse tree. Manages a vector of child
 * elements and provides GetType() to identify the group's destination
 * by inspecting the first child control word.
 *
 * @section rtfgroup_identification Group Type Identification
 * GetType() examines the first child element to determine the group's purpose:
 * - If the first child is a cRTFControlWord, the group type is that word
 *   (e.g., "fonttbl", "colortbl", "stylesheet", "header", "footer")
 * - If the first child is a cRTFControlSymbol with '*', the group is an
 *   ignorable destination and the type comes from the second child
 * - If neither applies, the group type is empty (anonymous group)
 *
 * @section rtfgroup_children Child Management
 * Child elements are stored in a vector and accessed by index. The parser
 * appends children during tree construction. Each group tracks its parent
 * pointer for upward tree traversal during scope management.
 *
 * @author Gerald Brandt
 * @copyright GNU Affero General Public License v3.0
 *
 * @see cRTFGroup RTF group node class
 * @see cRTFElement Base parse tree element class
 * @see cRTFControlWord Control word used for type identification
 * @see cRTFControlSymbol Control symbol for ignorable destinations
 * @see cRTFParser Parser building the group tree
 */

#include "rtfcontrolword.h"
#include "rtfcontrolsymbol.h"
#include "rtfgroup.h"
#include "rtfparser.h"
cRTFGroup::cRTFGroup(void)
{
    mParent = nullptr ;

    mType = eRTFTypeGroup ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return  std::string - control word of first child or empty std::string
///
/// @brief
/// reads the rtf file and returns when end of file reached
///
/////////////////////////////////////////////////////////////////////////////
std::string cRTFGroup::GetType(void)
{
    std::string retval ;

    // if there's no children, then group type is null
    if(!mChildren.empty())
    {
        cRTFElement *child = mChildren[0] ;
        if(child->mType == eRTFTypeControlWord)
        {
            // if the first child is a control word then the group type is the control word
            retval = static_cast<cRTFControlWord *>(child)->mWord ;
        }
        else if(child->mType == eRTFTypeControlSymbol)
        {
            // if first child is \* (ignorable destination), the type is the second child
            if(static_cast<cRTFControlSymbol *>(child)->mSymbol == '*' && mChildren.size() > 1)
            {
                cRTFElement *second = mChildren[1] ;
                if(second->mType == eRTFTypeControlWord)
                {
                    retval = static_cast<cRTFControlWord *>(second)->mWord ;
                }
            }
        }
    }

    return retval ;
}


bool cRTFGroup::IsDestination(void)
{
    bool retval = false ;

    if(mChildren.size() != 0)
    {
        cRTFElement *child = mChildren[0] ;
        if(child->mType == eRTFTypeControlSymbol)
        {
            if(static_cast<cRTFControlSymbol *>(child)->mSymbol == '*')
            {
                retval = true ;
            }
        }
    }

    return retval ;
}

void cRTFGroup::dump(int level)
{
    indent(level) ;

    for(size_t loop = 0; loop < mChildren.size(); loop++)
    {
        cRTFElement *child = mChildren[loop] ;
        if(child->mType == eRTFTypeGroup)
        {
/*
            cRTFGroup *group = static_cast<cRTFGroup *>(child) ;
            if(group->GetType() == "fonttbl")
            {
                continue ;
            }
            if(group->GetType() == "colortbl")
            {
                continue ;
            }
            if(group->GetType() == "stylesheet")
            {
                continue ;
            }
            if(group->GetType() == "info")
            {
                continue ;
            }

            if(group->GetType().length() >= 4 && group->GetType().substr(0, 4) == "pict")
            {
                continue ;
            }
            if(group->IsDestination())
            {
                continue ;
            }
*/
        }

        child->dump(level + 1) ;
    }
}
