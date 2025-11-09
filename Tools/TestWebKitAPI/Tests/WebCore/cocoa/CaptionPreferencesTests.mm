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

#import <CoreFoundation/CoreFoundation.h>
#import <WebCore/CaptionUserPreferencesMediaAF.h>
#import <WebCore/Color.h>
#import <WebCore/PageGroup.h>
#import "SoftLinkShim.h"

#import <WebCore/MediaAccessibilitySoftLink.h>

namespace TestWebKitAPI {

using namespace WebCore;

#define SOFT_LINK_SHIM(framework, functionName, resultType, args...) \
    static resultType functionName##Result; \
    static resultType shimmed##functionName(args) { return functionName##Result; } \
    SoftLinkShim<resultType, args> functionName##Shim { &softLink##framework##functionName, shimmed##functionName }; \

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

TEST(CaptionPreferenceTests, ShimTest)
{
    auto originalOpacity = MACaptionAppearanceGetForegroundOpacity(kMACaptionAppearanceDomainDefault, nullptr);
    {
        MediaAccessibilityShim shim;
        SOFT_LINK_SHIM_SET_RESULT(MACaptionAppearanceGetForegroundOpacity, 0.5f);

        EXPECT_EQ(0.5f, MACaptionAppearanceGetForegroundOpacity(kMACaptionAppearanceDomainDefault, nullptr));
    }

    EXPECT_EQ(originalOpacity, MACaptionAppearanceGetForegroundOpacity(kMACaptionAppearanceDomainDefault, nullptr));
}

TEST(CaptionPreferenceTests, FontFace)
{
    MediaAccessibilityShim shim;

    PageGroup group { "CaptionPreferenceTests"_s };
    auto preferences = CaptionUserPreferencesMediaAF::create(group);

    SOFT_LINK_SHIM_SET_RESULT(MACaptionAppearanceCopyFontDescriptorForStyle, adoptCF(CTFontDescriptorCreateWithNameAndSize(CFSTR(".AppleSystemUIFontMonospaced-Romulan"), 10)));
    EXPECT_STREQ(preferences->captionsDefaultFontCSS().utf8().data(), "font-family: \"ui-monospace\";");

    SOFT_LINK_SHIM_SET_RESULT(MACaptionAppearanceCopyFontDescriptorForStyle, adoptCF(CTFontDescriptorCreateWithNameAndSize(CFSTR(".AppleSystemUIFont-Klingon"), 10)));
    EXPECT_STREQ(preferences->captionsDefaultFontCSS().utf8().data(), "font-family: \"system-ui\";");

    SOFT_LINK_SHIM_SET_RESULT(MACaptionAppearanceCopyFontDescriptorForStyle, adoptCF(CTFontDescriptorCreateWithNameAndSize(CFSTR("WingDings"), 10)));
    EXPECT_STREQ(preferences->captionsDefaultFontCSS().utf8().data(), "font-family: \"WingDings\";");
}

TEST(CaptionPreferenceTests, FontSize)
{
    MediaAccessibilityShim shim;

    PageGroup group { "CaptionPreferenceTests"_s };
    auto preferences = CaptionUserPreferencesMediaAF::create(group);

    SOFT_LINK_SHIM_SET_RESULT(MACaptionAppearanceGetRelativeCharacterSize, 2.f);
    bool important = false;
    float fontScale = preferences->captionFontSizeScaleAndImportance(important);
    EXPECT_EQ(fontScale, 0.10f);

    EXPECT_STREQ(preferences->captionsFontSizeCSS().utf8().data(), "font-size: 10cqmin;");
}

TEST(CaptionPreferenceTests, Colors)
{
    MediaAccessibilityShim shim;

    PageGroup group { "CaptionPreferenceTests"_s };
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

TEST(CaptionPreferenceTests, BorderRadius)
{
    MediaAccessibilityShim shim;

    PageGroup group { "CaptionPreferenceTests"_s };
    auto preferences = CaptionUserPreferencesMediaAF::create(group);

    SOFT_LINK_SHIM_SET_RESULT(MACaptionAppearanceGetWindowRoundedCornerRadius, 8.f);
    EXPECT_STREQ(preferences->windowRoundedCornerRadiusCSS().utf8().data(), "border-radius:8px;padding:2px;");
}

TEST(CaptionPreferenceTests, TextEdge)
{
    MediaAccessibilityShim shim;

    PageGroup group { "CaptionPreferenceTests"_s };
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

}
