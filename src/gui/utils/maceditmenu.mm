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

#include "maceditmenu.h"

void RegisterMacEditMenuDefaults()
{
    @autoreleasepool
    {
        [[NSUserDefaults standardUserDefaults] registerDefaults:@{
            @"NSDisabledDictationMenuItem"        : @YES,
            @"NSDisabledCharacterPaletteMenuItem" : @YES,
        }];
    }
}

static bool IsWritingToolsOrAutoFill(NSMenuItem* item)
{
    NSString* ident = [item identifier];
    if (ident != nil && ([ident containsString:@"WritingTools"] || [ident containsString:@"AutoFill"]))
    {
        return true;
    }

    for (NSMenuItem* sub in [[item submenu] itemArray])
    {
        if ([sub action] == nil)
        {
            continue;
        }
        NSString* action = NSStringFromSelector([sub action]);
        if ([action isEqualToString:@"showWritingTools:"] || [action hasPrefix:@"_handleInsertFrom"])
        {
            return true;
        }
    }
    return false;
}

// Removes the item and the separator directly in front of it.
static void StripMenu(NSMenu* menu)
{
    for (NSInteger i = [menu numberOfItems] - 1; i >= 0; --i)
    {
        NSMenuItem* item = [menu itemAtIndex:i];
        if (![item isSeparatorItem] && IsWritingToolsOrAutoFill(item))
        {
            [menu removeItemAtIndex:i];
            if (i > 0 && [[menu itemAtIndex:i - 1] isSeparatorItem])
            {
                [menu removeItemAtIndex:i - 1];
                --i;
            }
        }
    }
}

static void StripMainMenu()
{
    for (NSMenuItem* top in [[NSApp mainMenu] itemArray])
    {
        if ([top submenu] != nil)
        {
            StripMenu([top submenu]);
        }
    }
}

void InstallMacEditMenuCleanup()
{
    NSNotificationCenter* center = [NSNotificationCenter defaultCenter];
    void (^strip)(NSNotification*) = ^(NSNotification*) { StripMainMenu(); };

    [center addObserverForName:NSApplicationDidFinishLaunchingNotification object:nil
                         queue:nil usingBlock:strip];
    [center addObserverForName:NSMenuDidBeginTrackingNotification object:nil
                         queue:nil usingBlock:strip];
}
