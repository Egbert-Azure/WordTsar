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

#include "macwriteblock.h"

std::string ShowMacWriteBlockPanel(const std::string& startDir, bool& asciiFormat, bool asciiLocked)
{
    @autoreleasepool
    {
        NSSavePanel* panel = [NSSavePanel savePanel];
        [panel setTitle:@"Write Block"];
        [panel setCanCreateDirectories:YES];

        if (!startDir.empty())
        {
            NSString* dir = [NSString stringWithUTF8String:startDir.c_str()];
            if (dir)
            {
                [panel setDirectoryURL:[NSURL fileURLWithPath:dir isDirectory:YES]];
            }
        }

        NSButton* box = [NSButton checkboxWithTitle:@"Copy in ASCII Format" target:nil action:nil];
        [box setState:(asciiFormat || asciiLocked) ? NSControlStateValueOn : NSControlStateValueOff];
        [box setEnabled:asciiLocked ? NO : YES];
        [box sizeToFit];

        const CGFloat margin = 12.0;
        NSRect boxFrame = [box frame];
        NSView* accessory = [[[NSView alloc] initWithFrame:NSMakeRect(0, 0,
                                boxFrame.size.width + 2 * margin,
                                boxFrame.size.height + 2 * margin)] autorelease];
        [box setFrameOrigin:NSMakePoint(margin, margin)];
        [accessory addSubview:box];
        [panel setAccessoryView:accessory];

        if ([panel runModal] != NSModalResponseOK)
        {
            return "";
        }

        NSURL* url = [panel URL];
        if (!url || ![url isFileURL])
        {
            return "";
        }

        asciiFormat = asciiLocked || ([box state] == NSControlStateValueOn);
        return std::string([[url path] fileSystemRepresentation]);
    }
}
