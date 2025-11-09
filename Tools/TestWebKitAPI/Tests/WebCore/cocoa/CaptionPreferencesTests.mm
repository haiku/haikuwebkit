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

#import "PlatformUtilities.h"
#import "SoftLinkShim.h"
#import <CoreFoundation/CoreFoundation.h>
#import <WebCore/CaptionUserPreferencesMediaAF.h>
#import <WebCore/Color.h>
#import <WebCore/PageGroup.h>
#import <WebKit/_WKCaptionStyleMenuController.h>
#import <wtf/cocoa/TypeCastsCocoa.h>
#import <wtf/darwin/DispatchExtras.h>

#if PLATFORM(IOS_FAMILY)
#import <UIKit/UIAction.h>
#endif

#import <WebCore/MediaAccessibilitySoftLink.h>

#if PLATFORM(MAC)
@interface NSMenu (PrivateHighlightItem)
- (void)highlightItem:(NSMenuItem *)item;
@end
#elif USE(UICONTEXTMENU)
@interface UIContextMenuInteraction (PrivatePresentMenu)
- (void)_presentMenuAtLocation:(CGPoint)location;
@end
#endif

static bool s_captionStyleMenuWillOpenCalled = false;
static bool s_captionStyleMenuDidCloseCalled = false;

@interface CaptionPreferenceTestMenuControllerDelegate : NSObject<WKCaptionStyleMenuControllerDelegate>
@end

@implementation CaptionPreferenceTestMenuControllerDelegate
- (void)captionStyleMenuWillOpen:(PlatformMenu *)menu
{
    s_captionStyleMenuWillOpenCalled = true;
}

- (void)captionStyleMenuDidClose:(PlatformMenu *)menu
{
    s_captionStyleMenuDidCloseCalled = true;
}
@end

namespace TestWebKitAPI {

using namespace WebCore;

#define SOFT_LINK_SHIM(framework, functionName, resultType, args...) \
    static resultType functionName##Result; \
    static resultType shimmed##functionName(args) { return functionName##Result; } \
    SoftLinkShim<resultType, args> functionName##Shim { &softLink##framework##functionName, shimmed##functionName }; \

#define SOFT_LINK_SHIM_CF_COPY_VOID_MAY_FAIL(framework, functionName, resultType) \
    static RetainPtr<resultType> functionName##Result; \
    static resultType shimmed##functionName() { return (resultType)CFRetain(functionName##Result.get()); } \
    SoftLinkShim<resultType> functionName##Shim { &softLink##framework##functionName, shimmed##functionName, canLoad_##framework##_##functionName }; \

#define SOFT_LINK_SHIM_CF_COPY(framework, functionName, resultType, args...) \
    static RetainPtr<resultType> functionName##Result; \
    static resultType shimmed##functionName(args) { return (resultType)CFRetain(functionName##Result.get()); } \
    SoftLinkShim<resultType, args> functionName##Shim { &softLink##framework##functionName, shimmed##functionName }; \

#define DOMAIN_AND_BEHAVIOR MACaptionAppearanceDomain, MACaptionAppearanceBehavior*

#define SOFT_LINK_SHIM_SET_RESULT(functionName, defaultValue) \
MediaAccessibilityShim::functionName##Result = defaultValue; \

#define SOFT_LINK_SHIM_DEFINE_RESULT(functionName, resultType) \
resultType MediaAccessibilityShim::functionName##Result; \

class MediaAccessibilityShim {
public:
    SOFT_LINK_SHIM(MediaAccessibility, MACaptionAppearanceGetDisplayType, MACaptionAppearanceDisplayType, MACaptionAppearanceDomain);
    SOFT_LINK_SHIM_CF_COPY(MediaAccessibility, MACaptionAppearanceCopyForegroundColor, CGColorRef, DOMAIN_AND_BEHAVIOR);
    SOFT_LINK_SHIM_CF_COPY(MediaAccessibility, MACaptionAppearanceCopyBackgroundColor, CGColorRef, DOMAIN_AND_BEHAVIOR);
    SOFT_LINK_SHIM_CF_COPY(MediaAccessibility, MACaptionAppearanceCopyWindowColor, CGColorRef, DOMAIN_AND_BEHAVIOR);
    SOFT_LINK_SHIM(MediaAccessibility, MACaptionAppearanceGetForegroundOpacity, CGFloat, DOMAIN_AND_BEHAVIOR);
    SOFT_LINK_SHIM(MediaAccessibility, MACaptionAppearanceGetBackgroundOpacity, CGFloat, DOMAIN_AND_BEHAVIOR);
    SOFT_LINK_SHIM(MediaAccessibility, MACaptionAppearanceGetWindowOpacity, CGFloat, DOMAIN_AND_BEHAVIOR);
    SOFT_LINK_SHIM(MediaAccessibility, MACaptionAppearanceGetWindowRoundedCornerRadius, CGFloat, DOMAIN_AND_BEHAVIOR);
    SOFT_LINK_SHIM(MediaAccessibility, MACaptionAppearanceGetRelativeCharacterSize, CGFloat, DOMAIN_AND_BEHAVIOR);
    SOFT_LINK_SHIM_CF_COPY(MediaAccessibility, MACaptionAppearanceCopyFontDescriptorForStyle, CTFontDescriptorRef, DOMAIN_AND_BEHAVIOR, MACaptionAppearanceFontStyle)
    SOFT_LINK_SHIM_CF_COPY(MediaAccessibility, MACaptionAppearanceCopySelectedLanguages, CFArrayRef, MACaptionAppearanceDomain);
    SOFT_LINK_SHIM_CF_COPY(MediaAccessibility, MACaptionAppearanceCopyPreferredCaptioningMediaCharacteristics, CFArrayRef, MACaptionAppearanceDomain);
    SOFT_LINK_SHIM_CF_COPY(MediaAccessibility, MACaptionAppearanceCopyFontDescriptorWithStrokeForStyle, CTFontDescriptorRef, DOMAIN_AND_BEHAVIOR, MACaptionAppearanceFontStyle, CFStringRef, CGFloat, CGFloat *);
    SOFT_LINK_SHIM(MediaAccessibility, MACaptionAppearanceGetTextEdgeStyle, MACaptionAppearanceTextEdgeStyle, DOMAIN_AND_BEHAVIOR);
    SOFT_LINK_SHIM_CF_COPY_VOID_MAY_FAIL(MediaAccessibility, MACaptionAppearanceCopyProfileIDs, CFArrayRef)
    SOFT_LINK_SHIM_CF_COPY_VOID_MAY_FAIL(MediaAccessibility, MACaptionAppearanceCopyActiveProfileID, CFStringRef)

    static void shimmedMACaptionAppearanceSetActiveProfileID(CFStringRef profileID) { MACaptionAppearanceCopyActiveProfileIDResult = profileID; }
    SoftLinkShim<void, CFStringRef> MACaptionAppearanceSetActiveProfileIDShim { &softLinkMediaAccessibilityMACaptionAppearanceSetActiveProfileID, shimmedMACaptionAppearanceSetActiveProfileID, canLoad_MediaAccessibility_MACaptionAppearanceSetActiveProfileID };

    static CFStringRef shimmedMACaptionAppearanceCopyProfileName(CFStringRef profileID) { return profileID; }
    SoftLinkShim<CFStringRef, CFStringRef> MACaptionAppearanceCopyProfileNameShim { &softLinkMediaAccessibilityMACaptionAppearanceCopyProfileName, shimmedMACaptionAppearanceCopyProfileName, canLoad_MediaAccessibility_MACaptionAppearanceCopyProfileName };

    void resetToDefaultValues()
    {
        SOFT_LINK_SHIM_SET_RESULT(MACaptionAppearanceGetDisplayType, kMACaptionAppearanceDisplayTypeAutomatic);
        SOFT_LINK_SHIM_SET_RESULT(MACaptionAppearanceCopyForegroundColor, CGColorGetConstantColor(kCGColorWhite));
        SOFT_LINK_SHIM_SET_RESULT(MACaptionAppearanceCopyBackgroundColor, CGColorGetConstantColor(kCGColorBlack));
        SOFT_LINK_SHIM_SET_RESULT(MACaptionAppearanceCopyWindowColor, CGColorGetConstantColor(kCGColorClear));
        SOFT_LINK_SHIM_SET_RESULT(MACaptionAppearanceCopyFontDescriptorForStyle, adoptCF(CTFontDescriptorCreateWithNameAndSize(CFSTR(".AppleSystemUIFont"), 10)));
        SOFT_LINK_SHIM_SET_RESULT(MACaptionAppearanceGetForegroundOpacity, 1.);
        SOFT_LINK_SHIM_SET_RESULT(MACaptionAppearanceGetBackgroundOpacity, 1.);
        SOFT_LINK_SHIM_SET_RESULT(MACaptionAppearanceGetWindowOpacity, 1.);
        SOFT_LINK_SHIM_SET_RESULT(MACaptionAppearanceGetWindowRoundedCornerRadius, (CGFloat)0.f);
        SOFT_LINK_SHIM_SET_RESULT(MACaptionAppearanceCopySelectedLanguages, adoptCF((__bridge CFArrayRef)@[@"English"]));
        SOFT_LINK_SHIM_SET_RESULT(MACaptionAppearanceCopyPreferredCaptioningMediaCharacteristics, adoptCF((__bridge CFArrayRef)@[@"MAMediaCharacteristicDescribesMusicAndSoundForAccessibility"]));
        SOFT_LINK_SHIM_SET_RESULT(MACaptionAppearanceCopyFontDescriptorWithStrokeForStyle, adoptCF(CTFontDescriptorCreateWithNameAndSize(CFSTR(".AppleSystemUIFont"), 10)));
        SOFT_LINK_SHIM_SET_RESULT(MACaptionAppearanceGetRelativeCharacterSize, (CGFloat)1.f);
        SOFT_LINK_SHIM_SET_RESULT(MACaptionAppearanceGetTextEdgeStyle, kMACaptionAppearanceTextEdgeStyleNone);
        SOFT_LINK_SHIM_SET_RESULT(MACaptionAppearanceCopyProfileIDs, adoptCF((__bridge CFArrayRef)@[@"Profile 1", @"Profile 2", @"Profile 3"]));
        SOFT_LINK_SHIM_SET_RESULT(MACaptionAppearanceCopyActiveProfileID, CFSTR("Profile 1"));
    }
    MediaAccessibilityShim() { resetToDefaultValues(); }
    ~MediaAccessibilityShim() { resetToDefaultValues(); }
};

SOFT_LINK_SHIM_DEFINE_RESULT(MACaptionAppearanceGetDisplayType, MACaptionAppearanceDisplayType);
SOFT_LINK_SHIM_DEFINE_RESULT(MACaptionAppearanceCopyForegroundColor, RetainPtr<CGColorRef>);
SOFT_LINK_SHIM_DEFINE_RESULT(MACaptionAppearanceCopyBackgroundColor, RetainPtr<CGColorRef>);
SOFT_LINK_SHIM_DEFINE_RESULT(MACaptionAppearanceCopyWindowColor, RetainPtr<CGColorRef>);
SOFT_LINK_SHIM_DEFINE_RESULT(MACaptionAppearanceGetForegroundOpacity, CGFloat);
SOFT_LINK_SHIM_DEFINE_RESULT(MACaptionAppearanceGetBackgroundOpacity, CGFloat);
SOFT_LINK_SHIM_DEFINE_RESULT(MACaptionAppearanceGetWindowOpacity, CGFloat);
SOFT_LINK_SHIM_DEFINE_RESULT(MACaptionAppearanceGetWindowRoundedCornerRadius, CGFloat);
SOFT_LINK_SHIM_DEFINE_RESULT(MACaptionAppearanceGetRelativeCharacterSize, CGFloat);
SOFT_LINK_SHIM_DEFINE_RESULT(MACaptionAppearanceCopyFontDescriptorForStyle, RetainPtr<CTFontDescriptorRef>)
SOFT_LINK_SHIM_DEFINE_RESULT(MACaptionAppearanceCopySelectedLanguages, RetainPtr<CFArrayRef>);
SOFT_LINK_SHIM_DEFINE_RESULT(MACaptionAppearanceCopyPreferredCaptioningMediaCharacteristics, RetainPtr<CFArrayRef>);
SOFT_LINK_SHIM_DEFINE_RESULT(MACaptionAppearanceCopyFontDescriptorWithStrokeForStyle, RetainPtr<CTFontDescriptorRef>);
SOFT_LINK_SHIM_DEFINE_RESULT(MACaptionAppearanceGetTextEdgeStyle, MACaptionAppearanceTextEdgeStyle);
SOFT_LINK_SHIM_DEFINE_RESULT(MACaptionAppearanceCopyProfileIDs, RetainPtr<CFArrayRef>);
SOFT_LINK_SHIM_DEFINE_RESULT(MACaptionAppearanceCopyActiveProfileID, RetainPtr<CFStringRef>);

class CaptionPreferenceTests : public testing::Test {
public:
    WKCaptionStyleMenuController *ensureController()
    {
        if (!m_styleMenuController) {
            m_delegate = adoptNS([[CaptionPreferenceTestMenuControllerDelegate alloc] init]);
            m_styleMenuController = adoptNS([[WKCaptionStyleMenuController alloc] init]);
            [m_styleMenuController setDelegate:m_delegate.get()];
        }
        return m_styleMenuController.get();
    }

    PlatformMenu *ensureMenu()
    {
        return [ensureController() captionStyleMenu];
    }

    void showMenuAndThen(Function<void(PlatformMenu*)>&& function)
    {
#if PLATFORM(MAC)
        RetainPtr window = adoptNS([[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 300, 300) styleMask:NSBorderlessWindowMask backing:NSBackingStoreBuffered defer:NO]);

        bool done = false;
        RetainPtr menu = ensureMenu();
        RunLoop::mainSingleton().dispatch([&] {
            function(menu.get());
            [menu cancelTracking];
            done = true;
        });

        [menu popUpMenuPositioningItem:nil atLocation:NSMakePoint(0, 0) inView:[window contentView]];
        Util::run(&done);
#elif USE(UICONTEXTMENU)
        RetainPtr window = adoptNS([[UIWindow alloc] initWithFrame:NSMakeRect(0, 0, 300, 300)]);

        RetainPtr viewController = adoptNS([[UIViewController alloc] init]);
        RetainPtr testView = adoptNS([[UIView alloc] initWithFrame:CGRectMake(0, 0, 300, 300)]);
        [viewController setView:testView.get()];
        [window setRootViewController:viewController.get()];
        [window makeKeyAndVisible];
        [testView addInteraction:ensureController().contextMenuInteraction];

        [ensureController().contextMenuInteraction _presentMenuAtLocation:CGPointMake(0, 0)];

        function(ensureMenu());

        [ensureController().contextMenuInteraction dismissMenu];
#else
        function(nil);
#endif
    }

    void runAndWaitForCaptionStyleMenuWillOpenCalled(Function<void()>&& function)
    {
        s_captionStyleMenuWillOpenCalled = false;
        function();
        Util::run(&s_captionStyleMenuWillOpenCalled);
    }

    void runAndWaitForCaptionStyleMenuDidCloseCalled(Function<void()>&& function)
    {
        s_captionStyleMenuDidCloseCalled = false;
        function();
        Util::run(&s_captionStyleMenuDidCloseCalled);
    }

    void selectProfileAtIndex(NSUInteger index)
    {
#if PLATFORM(MAC)
        [ensureMenu() performActionForItemAtIndex:index];
#elif PLATFORM(IOS_FAMILY)
        PlatformMenu *profileMenu = dynamic_objc_cast<PlatformMenu>(ensureMenu().children.firstObject);
        [dynamic_objc_cast<UIAction>(profileMenu.children[index]) performWithSender:nil target:nil];
#endif
    }
private:
    RetainPtr<WKCaptionStyleMenuController> m_styleMenuController;
    RetainPtr<CaptionPreferenceTestMenuControllerDelegate> m_delegate;
};

TEST_F(CaptionPreferenceTests, ShimTest)
{
    auto originalOpacity = MACaptionAppearanceGetForegroundOpacity(kMACaptionAppearanceDomainDefault, nullptr);
    {
        MediaAccessibilityShim shim;
        SOFT_LINK_SHIM_SET_RESULT(MACaptionAppearanceGetForegroundOpacity, 0.5f);

        EXPECT_EQ(0.5f, MACaptionAppearanceGetForegroundOpacity(kMACaptionAppearanceDomainDefault, nullptr));
    }

    EXPECT_EQ(originalOpacity, MACaptionAppearanceGetForegroundOpacity(kMACaptionAppearanceDomainDefault, nullptr));
}

TEST_F(CaptionPreferenceTests, FontFace)
{
    MediaAccessibilityShim shim;

    UniqueRef group = PageGroup::create("CaptionPreferenceTests"_s);
    auto preferences = CaptionUserPreferencesMediaAF::create(group);

    SOFT_LINK_SHIM_SET_RESULT(MACaptionAppearanceCopyFontDescriptorForStyle, adoptCF(CTFontDescriptorCreateWithNameAndSize(CFSTR(".AppleSystemUIFontMonospaced-Romulan"), 10)));
    EXPECT_STREQ(preferences->captionsDefaultFontCSS().utf8().data(), "font-family: \"ui-monospace\";");

    SOFT_LINK_SHIM_SET_RESULT(MACaptionAppearanceCopyFontDescriptorForStyle, adoptCF(CTFontDescriptorCreateWithNameAndSize(CFSTR(".AppleSystemUIFont-Klingon"), 10)));
    EXPECT_STREQ(preferences->captionsDefaultFontCSS().utf8().data(), "font-family: \"system-ui\";");

    SOFT_LINK_SHIM_SET_RESULT(MACaptionAppearanceCopyFontDescriptorForStyle, adoptCF(CTFontDescriptorCreateWithNameAndSize(CFSTR("WingDings"), 10)));
    EXPECT_STREQ(preferences->captionsDefaultFontCSS().utf8().data(), "font-family: \"WingDings\";");
}

TEST_F(CaptionPreferenceTests, FontSize)
{
    if (!canLoad_MediaAccessibility_MACaptionAppearanceIsCustomized())
        return;

    MediaAccessibilityShim shim;

    UniqueRef group = PageGroup::create("CaptionPreferenceTests"_s);
    auto preferences = CaptionUserPreferencesMediaAF::create(group);

    SOFT_LINK_SHIM_SET_RESULT(MACaptionAppearanceGetRelativeCharacterSize, 2.f);
    bool important = false;
    float fontScale = preferences->captionFontSizeScaleAndImportance(important);
    EXPECT_EQ(fontScale, 0.10f);

    EXPECT_STREQ(preferences->captionsFontSizeCSS().utf8().data(), "font-size: 10cqmin;");
}

TEST_F(CaptionPreferenceTests, Colors)
{
    MediaAccessibilityShim shim;

    UniqueRef group = PageGroup::create("CaptionPreferenceTests"_s);
    auto preferences = CaptionUserPreferencesMediaAF::create(group);

    SOFT_LINK_SHIM_SET_RESULT(MACaptionAppearanceCopyForegroundColor, cachedCGColor(Color::red));
    SOFT_LINK_SHIM_SET_RESULT(MACaptionAppearanceCopyBackgroundColor, cachedCGColor(Color::blue));
    SOFT_LINK_SHIM_SET_RESULT(MACaptionAppearanceCopyWindowColor, cachedCGColor(Color::green));

    EXPECT_STREQ(preferences->captionsTextColorCSS().utf8().data(), "color:#ff0000;");
    EXPECT_STREQ(preferences->captionsBackgroundCSS().utf8().data(), "background-color:#0000ff;");
    EXPECT_STREQ(preferences->captionsWindowCSS().utf8().data(), "background-color:#00ff00;");

    SOFT_LINK_SHIM_SET_RESULT(MACaptionAppearanceGetForegroundOpacity, 0.75);
    SOFT_LINK_SHIM_SET_RESULT(MACaptionAppearanceGetBackgroundOpacity, 0.5);
    SOFT_LINK_SHIM_SET_RESULT(MACaptionAppearanceGetWindowOpacity, 0.25);

    EXPECT_STREQ(preferences->captionsTextColorCSS().utf8().data(), "color:rgba(255, 0, 0, 0.75);");
    EXPECT_STREQ(preferences->captionsBackgroundCSS().utf8().data(), "background-color:rgba(0, 0, 255, 0.5);");
    EXPECT_STREQ(preferences->captionsWindowCSS().utf8().data(), "background-color:rgba(0, 255, 0, 0.25);");
}

TEST_F(CaptionPreferenceTests, BorderRadius)
{
    MediaAccessibilityShim shim;

    UniqueRef group = PageGroup::create("CaptionPreferenceTests"_s);
    auto preferences = CaptionUserPreferencesMediaAF::create(group);

    SOFT_LINK_SHIM_SET_RESULT(MACaptionAppearanceGetWindowRoundedCornerRadius, 8.f);
    EXPECT_STREQ(preferences->windowRoundedCornerRadiusCSS().utf8().data(), "border-radius:8px;padding:2px;");
}

TEST_F(CaptionPreferenceTests, TextEdge)
{
    MediaAccessibilityShim shim;

    UniqueRef group = PageGroup::create("CaptionPreferenceTests"_s);
    auto preferences = CaptionUserPreferencesMediaAF::create(group);

    EXPECT_STREQ(preferences->captionsTextEdgeCSS().utf8().data(), "");

    SOFT_LINK_SHIM_SET_RESULT(MACaptionAppearanceGetTextEdgeStyle, kMACaptionAppearanceTextEdgeStyleRaised);
    EXPECT_STREQ(preferences->captionsTextEdgeCSS().utf8().data(), "text-shadow:-.1em -.1em .16em black;");

    SOFT_LINK_SHIM_SET_RESULT(MACaptionAppearanceGetTextEdgeStyle, kMACaptionAppearanceTextEdgeStyleDepressed);
    EXPECT_STREQ(preferences->captionsTextEdgeCSS().utf8().data(), "text-shadow:.1em .1em .16em black;");

    SOFT_LINK_SHIM_SET_RESULT(MACaptionAppearanceGetTextEdgeStyle, kMACaptionAppearanceTextEdgeStyleDropShadow);
    EXPECT_STREQ(preferences->captionsTextEdgeCSS().utf8().data(), "text-shadow:0 .1em .16em black;stroke-color:black;paint-order:stroke;stroke-linejoin:round;stroke-linecap:round;");

    SOFT_LINK_SHIM_SET_RESULT(MACaptionAppearanceGetTextEdgeStyle, kMACaptionAppearanceTextEdgeStyleUniform);
    EXPECT_STREQ(preferences->captionsTextEdgeCSS().utf8().data(), "stroke-color:black;paint-order:stroke;stroke-linejoin:round;stroke-linecap:round;");
}

TEST_F(CaptionPreferenceTests, CaptionStyleMenu)
{
    if (!CaptionUserPreferencesMediaAF::canSetActiveProfileID())
        return;

    MediaAccessibilityShim shim;

    PlatformMenu *menu = ensureMenu();

#if PLATFORM(MAC)
    EXPECT_EQ(menu.numberOfItems, 5);
    EXPECT_WK_STREQ("Profile 1", [[menu itemAtIndex:0] title]);
    EXPECT_WK_STREQ("Profile 2", [[menu itemAtIndex:1] title]);
    EXPECT_WK_STREQ("Profile 3", [[menu itemAtIndex:2] title]);

    EXPECT_EQ(NSControlStateValueOn, [[menu itemAtIndex:0] state]);
    EXPECT_EQ(NSControlStateValueOff, [[menu itemAtIndex:1] state]);
    EXPECT_EQ(NSControlStateValueOff, [[menu itemAtIndex:2] state]);

    [menu performActionForItemAtIndex:1];
    EXPECT_WK_STREQ("Profile 2", CaptionUserPreferencesMediaAF::platformActiveProfileID());

    [menu performActionForItemAtIndex:2];
    EXPECT_WK_STREQ("Profile 3", CaptionUserPreferencesMediaAF::platformActiveProfileID());
#elif PLATFORM(IOS_FAMILY)
    EXPECT_EQ(menu.children.count, 2UL);
    PlatformMenu *profileMenu = dynamic_objc_cast<PlatformMenu>(menu.children.firstObject);

    EXPECT_EQ(profileMenu.children.count, 3UL);
    EXPECT_WK_STREQ("Profile 1", profileMenu.children[0].title);
    EXPECT_WK_STREQ("Profile 2", profileMenu.children[1].title);
    EXPECT_WK_STREQ("Profile 3", profileMenu.children[2].title);

    EXPECT_EQ(UIMenuElementStateOn, dynamic_objc_cast<UIAction>(profileMenu.children[0]).state);
    EXPECT_EQ(UIMenuElementStateOff, dynamic_objc_cast<UIAction>(profileMenu.children[1]).state);
    EXPECT_EQ(UIMenuElementStateOff, dynamic_objc_cast<UIAction>(profileMenu.children[2]).state);

    [dynamic_objc_cast<UIAction>(profileMenu.children[1]) performWithSender:nil target:nil];
    EXPECT_WK_STREQ("Profile 2", CaptionUserPreferencesMediaAF::platformActiveProfileID());

    [dynamic_objc_cast<UIAction>(profileMenu.children[2]) performWithSender:nil target:nil];
    EXPECT_WK_STREQ("Profile 3", CaptionUserPreferencesMediaAF::platformActiveProfileID());
#endif
}

TEST_F(CaptionPreferenceTests, CaptionStyleMenuHighlight)
{
    if (!CaptionUserPreferencesMediaAF::canSetActiveProfileID())
        return;

    MediaAccessibilityShim shim;

    showMenuAndThen([&] (PlatformMenu *menu) {
        // TODO: Menu highlighting is currently not supported on IOS_FAMILY
#if PLATFORM(MAC)
        [menu performSelector:@selector(highlightItem:) withObject:[menu itemAtIndex:1]];
        EXPECT_WK_STREQ("Profile 2", CaptionUserPreferencesMediaAF::platformActiveProfileID());

        [menu performSelector:@selector(highlightItem:) withObject:[menu itemAtIndex:2]];
        EXPECT_WK_STREQ("Profile 3", CaptionUserPreferencesMediaAF::platformActiveProfileID());
#endif
    });

    // Verify that cancelling the menu without making a selection will restore the active profile
    // to its previous state.
    EXPECT_WK_STREQ("Profile 1", CaptionUserPreferencesMediaAF::platformActiveProfileID());
}

TEST_F(CaptionPreferenceTests, CaptionStyleMenuSelect)
{
    if (!CaptionUserPreferencesMediaAF::canSetActiveProfileID())
        return;

    MediaAccessibilityShim shim;

    showMenuAndThen([&] (PlatformMenu *menu) {
        selectProfileAtIndex(1);
        EXPECT_WK_STREQ("Profile 2", CaptionUserPreferencesMediaAF::platformActiveProfileID());

        selectProfileAtIndex(2);
        EXPECT_WK_STREQ("Profile 3", CaptionUserPreferencesMediaAF::platformActiveProfileID());
    });

    EXPECT_WK_STREQ("Profile 3", CaptionUserPreferencesMediaAF::platformActiveProfileID());
}

TEST_F(CaptionPreferenceTests, CaptionStyleMenuDelegate)
{
    if (!CaptionUserPreferencesMediaAF::canSetActiveProfileID())
        return;

    MediaAccessibilityShim shim;

    runAndWaitForCaptionStyleMenuWillOpenCalled([&] {
        runAndWaitForCaptionStyleMenuDidCloseCalled([&] {
            showMenuAndThen([] (PlatformMenu *) {
            });
        });
    });
}

}
