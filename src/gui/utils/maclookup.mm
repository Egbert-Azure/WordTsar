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

#import <AppKit/AppKit.h>

#include "maclookup.h"

void ShowMacDefinitionPopover(void* nsViewPtr, const std::string& word, double x, double y)
{
    @autoreleasepool
    {
        if (!nsViewPtr || word.empty())
        {
            return;
        }

        NSView* view = (__bridge NSView*)nsViewPtr;
        NSString* nsWord = [NSString stringWithUTF8String:word.c_str()];
        if (!nsWord)
        {
            return;
        }

        NSAttributedString* attrString = [[NSAttributedString alloc] initWithString:nsWord];
        [view showDefinitionForAttributedString:attrString atPoint:NSMakePoint(x, y)];
    }
}
