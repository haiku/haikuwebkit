/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#if PLATFORM(MAC)

#import "PlatformWebView.h"
#import "TestController.h"
#import <WebKit/WKProcessPoolPrivate.h>

static void setDefaultsToConsistentValuesForTesting()
{
    NSDictionary *dict = @{
        @"AppleKeyboardUIMode": @1,
        // FIXME: This is likely insufficient, as tests change (and don't reset) these settings via Internals.
        @"WebAutomaticQuoteSubstitutionEnabled": @NO,
        @"WebAutomaticDashSubstitutionEnabled": @NO,
        @"NSFakeForceTouchDevice" : @YES,
        @"AppleEnableSwipeNavigateWithScrolls": @YES,
        @"com.apple.swipescrolldirection": @1,
        @"com.apple.trackpad.forceClick": @1,
        @"NSOverlayScrollersEnabled": @NO,
        @"NSScrollAnimationEnabled" : @NO,
        @"AppleShowScrollBars": @"Always",
#if ENABLE(REMOTE_LAYER_TREE_ON_MAC_BY_DEFAULT)
        @"WebKit2UseRemoteLayerTreeDrawingArea": @YES,
#else
        @"WebKit2UseRemoteLayerTreeDrawingArea": @NO,
#endif
    };

    [[NSUserDefaults standardUserDefaults] setValuesForKeysWithDictionary:dict];
}

static void disableAppNapInUIProcess()
{
    static NeverDestroyed<RetainPtr<id>> assertion;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        NSActivityOptions options = (NSActivityUserInitiatedAllowingIdleSystemSleep | NSActivityLatencyCritical) & ~(NSActivitySuddenTerminationDisabled | NSActivityAutomaticTerminationDisabled);
        assertion.get() = [[NSProcessInfo processInfo] beginActivityWithOptions:options reason:@"WebKitTestRunner should not be subject to process suppression"];
    });
    ASSERT_UNUSED(assertion, assertion.get());
}

int main(int argc, const char* argv[])
{
    @autoreleasepool {
        [NSApplication sharedApplication];
        setDefaultsToConsistentValuesForTesting();
        disableAppNapInUIProcess(); // For secondary processes, app nap is disabled using WKPreferencesSetPageVisibilityBasedProcessSuppressionEnabled().
        [WKProcessPool _setLinkedOnOrAfterEverythingForTesting];
        [WKProcessPool _crashOnMessageCheckFailureForTesting];
    }
    WTR::TestController controller(argc, argv);
    return 0;
}

#else

int main(int, const char*[])
{
    return 0;
}

#endif // PLATFORM(MAC)
