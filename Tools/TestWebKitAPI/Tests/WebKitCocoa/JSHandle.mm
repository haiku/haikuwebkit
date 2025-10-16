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

#import "HTTPServer.h"
#import "PlatformUtilities.h"
#import "Test.h"
#import "TestNavigationDelegate.h"
#import "TestWKWebView.h"
#import <WebKit/WKContentWorldPrivate.h>
#import <WebKit/_WKContentWorldConfiguration.h>
#import <WebKit/_WKFeature.h>
#import <WebKit/_WKJSHandle.h>

namespace TestWebKitAPI {

static WKFrameInfo *getWindowFrameInfo(RetainPtr<_WKJSHandle> node)
{
    EXPECT_TRUE([node isKindOfClass:_WKJSHandle.class]);
    __block RetainPtr<WKFrameInfo> frame;
    __block bool done { false };
    [node windowFrameInfo:^(WKFrameInfo *info) {
        frame = info;
        done = true;
    }];
    Util::run(&done);
    return frame.autorelease();
}

TEST(JSHandle, Basic)
{
    HTTPServer server({
        { "/example"_s, { "<iframe id=onlyframe src='https://webkit.org/webkit'></iframe><div id=onlydiv></div>"_s } },
        { "/webkit"_s, { "hi"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    RetainPtr configuration = server.httpsProxyConfiguration();

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero configuration:configuration.get()]);
    RetainPtr navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [navigationDelegate allowAnyTLSCertificate];
    webView.get().navigationDelegate = navigationDelegate.get();
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];

    RetainPtr worldConfiguration = adoptNS([_WKContentWorldConfiguration new]);
    worldConfiguration.get().allowJSHandleCreation = YES;
    RetainPtr world = [WKContentWorld _worldWithConfiguration:worldConfiguration.get()];

    RetainPtr<id> result = [webView objectByEvaluatingJavaScript:@"window.webkit.createJSHandle(onlyframe.contentWindow)" inFrame:nil inContentWorld:world.get()];
    EXPECT_WK_STREQ(getWindowFrameInfo(result).request.URL.absoluteString, "https://webkit.org/webkit");
    RetainPtr<_WKJSHandle> iframeRef = [webView objectByEvaluatingJavaScript:@"window.webkit.createJSHandle(onlyframe)" inFrame:nil inContentWorld:world.get()];

    result = [webView objectByEvaluatingJavaScript:@"window.webkit.createJSHandle(onlydiv)" inFrame:nil inContentWorld:world.get()];
    EXPECT_NULL(getWindowFrameInfo(result));
    {
        __block bool done { false };
        [webView evaluateJavaScript:@"window.webkit.createJSHandle(5)" inFrame:nil inContentWorld:world.get() completionHandler:^(id result, NSError *error) {
            EXPECT_NULL(result);
            EXPECT_NOT_NULL(error);
            done = true;
        }];
        Util::run(&done);
    }
    result = [webView objectByEvaluatingJavaScript:@"window.webkit.createJSHandle(document.createTextNode('hi'))" inFrame:nil inContentWorld:world.get()];
    EXPECT_NULL(getWindowFrameInfo(result));
    result = [webView objectByEvaluatingJavaScript:@"window.WebKitJSHandle"];
    EXPECT_NULL(result);

    result = [webView objectByCallingAsyncFunction:@"return {'arg':window.webkit.createJSHandle(onlydiv)}" withArguments:nil inFrame:nil inContentWorld:world.get()];
    result = [webView objectByCallingAsyncFunction:@"return arg.outerHTML" withArguments:result.get()];
    EXPECT_WK_STREQ(result.get(), "<div id=\"onlydiv\"></div>");

    result = [webView objectByCallingAsyncFunction:@"return n === undefined" withArguments:@{ @"n" : iframeRef.get() } inFrame:[webView firstChildFrame] inContentWorld:WKContentWorld.pageWorld];
    EXPECT_EQ(result.get(), @YES);

    result = [webView objectByCallingAsyncFunction:@"return n.id" withArguments:@{ @"n" : iframeRef.get() }];
    EXPECT_WK_STREQ(result.get(), "onlyframe");

    result = [webView objectByEvaluatingJavaScript:@"function returnThirty() { return '30'; }; window.webkit.createJSHandle(returnThirty)" inFrame:nil inContentWorld:world.get()];
    EXPECT_WK_STREQ([webView objectByCallingAsyncFunction:@"return n()" withArguments:@{ @"n" : result.get() } inFrame:nil inContentWorld:world.get()], "30");

    result = [webView objectByEvaluatingJavaScript:@"window.webkit.createJSHandle(window.parent)" inFrame:[webView firstChildFrame] inContentWorld:world.get()];
    EXPECT_WK_STREQ(getWindowFrameInfo(result).request.URL.absoluteString, "https://example.com/example");
}

}
