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
#import <WebKit/WKUserContentControllerPrivate.h>
#import <WebKit/_WKStringMatcher.h>

TEST(StringMatcher, MatcherData)
{
    constexpr size_t alphabetLength = 'z' - 'a' + 1;
    NSMutableDictionary<NSString *, NSNumber *> *dictionary = [NSMutableDictionary dictionaryWithCapacity:alphabetLength * alphabetLength * alphabetLength * ('d' - 'a' + 1)];
    for (char first = 'a'; first < 'z'; first++) {
        for (char second = 'a'; second < 'z'; second++) {
            for (char third = 'a'; third < 'z'; third++) {
                for (char fourth = 'a'; fourth < 'd'; fourth++)
                    [dictionary setObject:@0 forKey:[NSString stringWithFormat:@"%c%c%c%c", first, second, third, fourth]];
            }
        }
    }
    EXPECT_NULL([_WKStringMatcher matcherDataForStringsAndIdentifiers:dictionary]);

    EXPECT_NULL(adoptNS([[_WKStringMatcher alloc] initWithData:NSData.data]).get());

    NSData *data = [_WKStringMatcher matcherDataForStringsAndIdentifiers:@{ @"abc": @37 }];
    std::array<uint8_t, 40> expected {
        // header
        0, 0, 0, 0, // version
        5, 0, // state count
        3, 0, // transition count

        // states
        0, 0, 1, 0,
        1, 0, 2, 0,
        2, 0, 3, 0,
        37, 0, 0, 0, // sentinel indicating match of string with identifier 37
        3, 0, 3, 0,

        // transitions
        'a', 0, 1, 0,
        'b', 0, 2, 0,
        'c', 0, 3, 0,
    };
    EXPECT_TRUE([data isEqual:[NSData dataWithBytes:expected.data() length:expected.size()]]);
}

TEST(StringMatcher, MatchResults)
{
    RetainPtr matcher = adoptNS([[_WKStringMatcher alloc] initWithData:[_WKStringMatcher matcherDataForStringsAndIdentifiers:@{
        @"aba": @30,
        @"ba": @20,
        @"abcd": @40,
        @"e": @10
    }]]);
    RetainPtr configuration = adoptNS([WKWebViewConfiguration new]);
    [configuration.get().userContentController _addStringMatcher:matcher.get() contentWorld:WKContentWorld.pageWorld name:@"testMatcher"];
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero configuration:configuration.get()]);
    auto checkMatch = [=] (const char* input, NSArray *expected, const char* options = "", bool matchAll = true, bool searchReverse = false) {
        NSArray *actual = [webView objectByEvaluatingJavaScript:[NSString stringWithFormat:@"window.webkit.stringMatchers.testMatcher.match('%s'%s)", input, options]];
        EXPECT_TRUE([actual isEqual:expected]);

        size_t inputLength = strlen(input);
        constexpr size_t maxTestStringLength { 7 };
        std::array<unichar, maxTestStringLength> unichars;
        for (size_t i = 0; i < inputLength; i++)
            unichars[i] = input[i];
        NSArray *actualFromNativeInterface = [matcher resultsForMatchingCharacters:unichars.data() length:strlen(input) matchAll:matchAll searchReverse:searchReverse];
        EXPECT_TRUE([actualFromNativeInterface isEqual:expected]);
    };
    checkMatch("aabcdef", @[
        @{ @"substring": @"abcd", @"index": @1, @"identifier": @40 },
        @{ @"substring": @"e", @"index": @5, @"identifier": @10 }
    ]);
    checkMatch("aabcd", @[
        @{ @"substring": @"abcd", @"index": @1, @"identifier": @40 }
    ]);
    checkMatch("aba", @[
        @{ @"substring": @"aba", @"index": @0, @"identifier": @30 },
        @{ @"substring": @"ba", @"index": @1, @"identifier": @20 }
    ]);
    checkMatch("abaa", @[
        @{ @"substring": @"aba", @"index": @0, @"identifier": @30 },
        @{ @"substring": @"ba", @"index": @1, @"identifier": @20 }
    ]);
    checkMatch("ababcd", @[
        @{ @"substring": @"aba", @"index": @0, @"identifier": @30 },
        @{ @"substring": @"ba", @"index": @1, @"identifier": @20 },
        @{ @"substring": @"abcd", @"index": @2, @"identifier": @40 }
    ]);
    checkMatch("e", @[
        @{ @"substring": @"e", @"index": @0, @"identifier": @10 },
    ]);
    checkMatch("ee", @[
        @{ @"substring": @"e", @"index": @0, @"identifier": @10 },
        @{ @"substring": @"e", @"index": @1, @"identifier": @10 },
    ]);
    checkMatch("ababcd", @[ @{ @"substring": @"abcd", @"index": @2, @"identifier": @40 } ], ", { 'searchReverse': true, 'matchAll': false }", false, true);
    checkMatch("ababcd", @[ @{ @"substring": @"aba", @"index": @0, @"identifier": @30 } ], ", { 'matchAll': false }", false);
    checkMatch("ababcd", @[
        @{ @"substring": @"abcd", @"index": @2, @"identifier": @40 },
        @{ @"substring": @"ba", @"index": @1, @"identifier": @20 },
        @{ @"substring": @"aba", @"index": @0, @"identifier": @30 }
    ], ", { 'searchReverse': true }", true, true);
    checkMatch("a", @[ ]);
    checkMatch("a", @[ ], ", { 'searchReverse': true, 'matchAll': false }", false, true);
}

TEST(StringMatcher, IDLExposed)
{
    RetainPtr webView = adoptNS([TestWKWebView new]);
    EXPECT_FALSE([[webView objectByEvaluatingJavaScript:@"!!window.WebKitStringMatcher"] boolValue]);
    EXPECT_FALSE([[webView objectByEvaluatingJavaScript:@"!!window.WebKitStringMatcherOptions"] boolValue]);
    EXPECT_FALSE([[webView objectByEvaluatingJavaScript:@"!!window.WebKitStringMatchersNamespace"] boolValue]);
}

TEST(StringMatcher, CharacterEdgeCases)
{
    RetainPtr matcher = adoptNS([[_WKStringMatcher alloc] initWithData:[_WKStringMatcher matcherDataForStringsAndIdentifiers:@{
        @"\uffff": @1,
        @"\ufffe": @1,
        @"\u0000": @1,
        @"\u0001": @1
    }]]);
    RetainPtr configuration = adoptNS([WKWebViewConfiguration new]);
    [configuration.get().userContentController _addStringMatcher:matcher.get() contentWorld:WKContentWorld.pageWorld name:@"testMatcher"];
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero configuration:configuration.get()]);
    NSArray *actual = [webView objectByEvaluatingJavaScript:@"window.webkit.stringMatchers.testMatcher.match('\\u0000\\u0001\\ufffe\\uffff')"];
    NSArray *expected = @[
        @{ @"substring": @"\u0000", @"index": @0, @"identifier": @1 },
        @{ @"substring": @"\u0001", @"index": @1, @"identifier": @1 },
        @{ @"substring": @"\ufffe", @"index": @2, @"identifier": @1 },
        @{ @"substring": @"\uffff", @"index": @3, @"identifier": @1 }
    ];
    EXPECT_TRUE([actual isEqual:expected]);
}
