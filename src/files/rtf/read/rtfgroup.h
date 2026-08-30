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

#ifndef CRTFGROUP_H
#define CRTFGROUP_H

#include <vector>
#include <string>

#include "rtfelement.h"


class cRTFGroup : public cRTFElement
{
public:
    cRTFGroup(void);
    virtual ~cRTFGroup() {};

    std::string GetType(void) ;
    bool IsDestination(void) ;

    void dump(int level) ;

public :
    cRTFGroup *mParent ;
    std::vector<cRTFElement *> mChildren ;
};

#endif // CRTFGROUP_H
