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

#import "TestWKWebView.h"
#import "Utilities.h"
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/_WKTextExtraction.h>

@interface WKWebView (TextExtractionTests)
- (NSString *)synchronouslyGetDebugText:(_WKTextExtractionConfiguration *)configuration;
@end

@implementation WKWebView (TextExtractionTests)

- (NSString *)synchronouslyGetDebugText:(_WKTextExtractionConfiguration *)configuration
{
    RetainPtr configurationToUse = configuration;
    if (!configurationToUse)
        configurationToUse = adoptNS([_WKTextExtractionConfiguration new]);

    __block bool done = false;
    __block RetainPtr<NSString> result;
    [self _debugTextWithConfiguration:configurationToUse.get() completionHandler:^(NSString *text) {
        result = text;
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    return result.autorelease();
}

@end

namespace TestWebKitAPI {

#if PLATFORM(MAC)

static NSString *extractNodeIdentifier(NSString *debugText, NSString *searchText)
{
    for (NSString *line in [debugText componentsSeparatedByString:@"\n"]) {
        if (![line containsString:searchText])
            continue;

        RetainPtr regex = [NSRegularExpression regularExpressionWithPattern:@"uid=(\\d+)" options:0 error:nil];
        RetainPtr match = [regex firstMatchInString:line options:0 range:NSMakeRange(0, line.length)];
        if (!match)
            continue;

        NSRange identifierRange = [match rangeAtIndex:1];
        return [line substringWithRange:identifierRange];
    }

    return nil;
}

TEST(TextExtractionTests, SelectPopupMenu)
{
    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [[configuration preferences] _setTextExtractionEnabled:YES];

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration.get()]);

    [webView synchronouslyLoadTestPageNamed:@"debug-text-extraction"];

    RetainPtr debugText = [webView synchronouslyGetDebugText:nil];
    RetainPtr click = adoptNS([[_WKTextExtractionInteraction alloc] initWithAction:_WKTextExtractionActionClick]);
    [click setNodeIdentifier:extractNodeIdentifier(debugText.get(), @"menu")];

    __block bool doneSelectingOption = false;
    __block RetainPtr<NSString> debugTextAfterClickingSelect;
    [webView _performInteraction:click.get() completionHandler:^(_WKTextExtractionInteractionResult *clickResult) {
        EXPECT_FALSE(clickResult.error);
        EXPECT_TRUE([[webView synchronouslyGetDebugText:nil] containsString:@"nativePopupMenu"]);

        RetainPtr selectOption = adoptNS([[_WKTextExtractionInteraction alloc] initWithAction:_WKTextExtractionActionSelectMenuItem]);
        [selectOption setText:@"Three"];
        [webView _performInteraction:selectOption.get() completionHandler:^(_WKTextExtractionInteractionResult *selectOptionResult) {
            EXPECT_FALSE(selectOptionResult.error);
            doneSelectingOption = true;
        }];
    }];

    Util::run(&doneSelectingOption);
    EXPECT_WK_STREQ("Three", [webView stringByEvaluatingJavaScript:@"select.value"]);
}

#endif // PLATFORM(MAC)

} // namespace TestWebKitAPI
