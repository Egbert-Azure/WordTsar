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

#import <Foundation/Foundation.h>

#include "doctest.h"
#include "src/gui/utils/maceditmenu.h"

TEST_CASE("Edit menu - system items are switched off in the registration domain only")
{
    @autoreleasepool
    {
        NSArray<NSString*>* keys = @[ @"NSDisabledDictationMenuItem",
                                      @"NSDisabledCharacterPaletteMenuItem" ];

        RegisterMacEditMenuDefaults();

        NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
        NSDictionary* registered = [defaults volatileDomainForName:NSRegistrationDomain];
        NSString* bundleId = [[NSBundle mainBundle] bundleIdentifier];
        NSDictionary* persistent = bundleId ? [defaults persistentDomainForName:bundleId] : nil;

        for (NSString* key in keys)
        {
            INFO([key UTF8String]);
            CHECK([[registered objectForKey:key] boolValue]);
            CHECK([defaults boolForKey:key]);
            CHECK([persistent objectForKey:key] == nil);
        }
    }
}
