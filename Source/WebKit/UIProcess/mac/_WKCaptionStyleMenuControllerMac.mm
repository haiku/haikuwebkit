/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "_WKCaptionStyleMenuController.h"

#if PLATFORM(MAC)

#import <WebCore/CaptionUserPreferencesMediaAF.h>
#import <WebCore/LocalizedStrings.h>
#import <wtf/Vector.h>
#import <wtf/text/WTFString.h>

using namespace WebCore;
using namespace WTF;

@interface WKCaptionStyleMenuController () <NSMenuDelegate> {
    Vector<String> _profileIDs;
    String _savedActiveProfileID;
    RetainPtr<NSMenu> _menu;
}
@end

@implementation WKCaptionStyleMenuController

- (instancetype)init
{
    if (!(self = [super init]))
        return nil;

    _menu = adoptNS([[NSMenu alloc] initWithTitle:@""]);
    [_menu setDelegate:self];
    _savedActiveProfileID = CaptionUserPreferencesMediaAF::platformActiveProfileID();

    [self rebuildMenu];

    return self;
}

- (void)rebuildMenu
{
    [_menu removeAllItems];
    auto profileIDs = CaptionUserPreferencesMediaAF::platformProfileIDs();

    // Add Caption Profile items
    for (const auto& profileID : profileIDs) {
        auto title = CaptionUserPreferencesMediaAF::nameForProfileID(profileID);
        RetainPtr item = adoptNS([[NSMenuItem alloc] initWithTitle:title.createNSString().get() action:@selector(profileMenuItemSelected:) keyEquivalent:@""]);
        [item setTarget:self];
        [item setRepresentedObject:profileID.createNSString().get()];

        // Add checkmark for currently selected item
        if (profileID == _savedActiveProfileID)
            [item setState:NSControlStateValueOn];

        [_menu addItem:item.get()];
    }

    if (!profileIDs.isEmpty())
        [_menu addItem:[NSMenuItem separatorItem]];

    // Add Deep-link to System Caption Settings
    RetainPtr<NSString> systemCaptionSettingsTitle = WEB_UI_NSSTRING_KEY(@"Manage Styles", @"Manage Styles (Caption Style Menu Title)", @"Subtitle Styles Submenu Link to System Preferences");
    RetainPtr systemCaptionSettingsItem = adoptNS([[NSMenuItem alloc] initWithTitle:systemCaptionSettingsTitle.get() action:@selector(systemCaptionStyleSettingsItemSelected:) keyEquivalent:@""]);
    [systemCaptionSettingsItem setTarget:self];
    [systemCaptionSettingsItem setImage:[NSImage imageWithSystemSymbolName:@"gear" accessibilityDescription:nil]];
    [_menu addItem:systemCaptionSettingsItem.get()];
}

- (BOOL)isAncestorOf:(NSMenu *)menu
{
    do {
        if (_menu == menu)
            return true;
        menu = menu.supermenu;
    } while (menu);

    return false;
}

#pragma mark - Properties

- (NSMenu *)captionStyleMenu
{
    return _menu.get();
}

#pragma mark - Actions

- (void)profileMenuItemSelected:(NSMenuItem *)item
{
    if (!item)
        return;

    NSString *nsProfileID = (NSString *)item.representedObject;
    if (![nsProfileID isKindOfClass:NSString.class])
        return;

    _savedActiveProfileID = nsProfileID;
    CaptionUserPreferencesMediaAF::setActiveProfileID(_savedActiveProfileID);
    [self rebuildMenu];
}

- (void)profileMenuItemHighlighted:(NSMenuItem *)item
{
    NSString *nsProfileID = (NSString *)item.representedObject;
    if ([nsProfileID isKindOfClass:NSString.class]) {
        CaptionUserPreferencesMediaAF::setActiveProfileID(nsProfileID);
        return;
    }

    if (!_savedActiveProfileID.isEmpty())
        CaptionUserPreferencesMediaAF::setActiveProfileID(_savedActiveProfileID);
}

- (void)systemCaptionStyleSettingsItemSelected:(NSMenuItem *)sender
{
    [NSWorkspace.sharedWorkspace openURL:[NSURL URLWithString:@"x-apple.systempreferences:com.apple.Accessibility?Captions"]];
}

#pragma mark - NSMenuDelegate

- (void)menuWillOpen:(NSMenu *)menu
{
    _savedActiveProfileID = CaptionUserPreferencesMediaAF::platformActiveProfileID();

    if (auto delegate = self.delegate)
        [delegate captionStyleMenuWillOpen:_menu.get()];
}

- (void)menu:(NSMenu *)menu willHighlightItem:(NSMenuItem *)item
{
    [self profileMenuItemHighlighted:item];
}

- (void)menuDidClose:(NSMenu *)menu
{
    if (!_savedActiveProfileID.isEmpty())
        CaptionUserPreferencesMediaAF::setActiveProfileID(_savedActiveProfileID);
    _savedActiveProfileID = nullString();

    if (auto delegate = self.delegate)
        [delegate captionStyleMenuDidClose:_menu.get()];
}

@end

#endif
