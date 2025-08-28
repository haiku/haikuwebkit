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
#import "TestNavigationDelegate.h"
#import "TestScriptMessageHandler.h"
#import "TestWKWebView.h"
#import <WebKit/WKContentWorldPrivate.h>
#import <WebKit/_WKContentWorldConfiguration.h>
#import <WebKit/_WKFeature.h>
#import <WebKit/_WKNodeInfo.h>
#import <WebKit/_WKSerializedNode.h>

namespace TestWebKitAPI {

TEST(NodeInfo, Basic)
{
    HTTPServer server({
        { "/example"_s, { "<iframe id=onlyframe src='https://webkit.org/webkit'></iframe><div id=onlydiv></div>"_s } },
        { "/webkit"_s, { "hi"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    RetainPtr configuration = server.httpsProxyConfiguration();

    RetainPtr webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectZero configuration:configuration.get()]);
    RetainPtr navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [navigationDelegate allowAnyTLSCertificate];
    webView.get().navigationDelegate = navigationDelegate.get();
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];

    RetainPtr worldConfiguration = adoptNS([_WKContentWorldConfiguration new]);
    worldConfiguration.get().allowNodeInfo = YES;
    RetainPtr world = [WKContentWorld _worldWithConfiguration:worldConfiguration.get()];

    __block bool done { false };
    [webView evaluateJavaScript:@"window.webkit.createNodeInfo(onlyframe)" inFrame:nil inContentWorld:world.get() completionHandler:^(id result, NSError *error) {
        EXPECT_NULL(error);
        EXPECT_TRUE([result isKindOfClass:_WKNodeInfo.class]);
        [result contentFrameInfo:^(WKFrameInfo *info) {
            EXPECT_WK_STREQ(info.request.URL.absoluteString, "https://webkit.org/webkit");
            done = true;
        }];
    }];
    Util::run(&done);

    done = false;
    [webView evaluateJavaScript:@"window.webkit.createNodeInfo(onlydiv)" inFrame:nil inContentWorld:world.get() completionHandler:^(id result, NSError *error) {
        EXPECT_TRUE([result isKindOfClass:_WKNodeInfo.class]);
        EXPECT_NULL(error);
        [result contentFrameInfo:^(WKFrameInfo *info) {
            EXPECT_NULL(info);
            done = true;
        }];
    }];
    Util::run(&done);

    done = false;
    [webView evaluateJavaScript:@"window.webkit.createNodeInfo(5)" inFrame:nil inContentWorld:world.get() completionHandler:^(id result, NSError *error) {
        EXPECT_NULL(result);
        EXPECT_NOT_NULL(error);
        done = true;
    }];
    Util::run(&done);

    done = false;
    [webView evaluateJavaScript:@"window.webkit.createNodeInfo(document.createTextNode('hi'))" inFrame:nil inContentWorld:world.get() completionHandler:^(id result, NSError *error) {
        EXPECT_TRUE([result isKindOfClass:_WKNodeInfo.class]);
        EXPECT_NULL(error);
        [result contentFrameInfo:^(WKFrameInfo *info) {
            EXPECT_NULL(info);
            done = true;
        }];
    }];
    Util::run(&done);

    done = false;
    [webView evaluateJavaScript:@"window.WebKitNodeInfo" completionHandler:^(id result, NSError *error) {
        EXPECT_NULL(result);
        EXPECT_NULL(error);
        done = true;
    }];
    Util::run(&done);
}

TEST(SerializedNode, Basic)
{
    RetainPtr webView = adoptNS([WKWebView new]);
    [webView loadHTMLString:@"<div><div>test</div></div>" baseURL:[NSURL URLWithString:@"https://webkit.org/"]];

    RetainPtr worldConfiguration = adoptNS([_WKContentWorldConfiguration new]);
    worldConfiguration.get().allowNodeInfo = YES;
    RetainPtr world = [WKContentWorld _worldWithConfiguration:worldConfiguration.get()];

    auto verifyNodeSerialization = [world, webView] (const char* constructor, const char* accessor, const char* expected, const char* className) {
        __block bool done = false;
        [webView evaluateJavaScript:[NSString stringWithFormat:@"window.webkit.serializeNode(%s, { deep:true })", constructor] inFrame:nil inContentWorld:world.get() completionHandler:^(id serializedNode, NSError *error) {
            EXPECT_TRUE([serializedNode isKindOfClass:_WKSerializedNode.class]);
            EXPECT_NULL(error);
            RetainPtr other = adoptNS([TestWKWebView new]);

            id instanceof = [other objectByCallingAsyncFunction:@"return Object.getPrototypeOf(n).toString()" withArguments:@{ @"n" : serializedNode } error:nil];
            NSString *expectedClass = [NSString stringWithFormat:@"[object %s]", className];
            EXPECT_WK_STREQ(instanceof, expectedClass);

            id result = [other objectByCallingAsyncFunction:[NSString stringWithFormat:@"return %s", accessor] withArguments:@{ @"n" : serializedNode } error:nil];
            EXPECT_WK_STREQ(result, expected);
            done = true;
        }];
        Util::run(&done);
    };

    auto textAccessor = "n.wholeText";
    verifyNodeSerialization("document.createTextNode('text content')", textAccessor, "text content", "Text");

    auto attrAccessor = "n.namespaceURI + ',' + n.prefix + ',' + n.localName + ',' + n.name + ',' + n.value";
    verifyNodeSerialization("document.createAttributeNS('a', 'b')", attrAccessor, "a,null,b,b,", "Attr");
    verifyNodeSerialization("document.createAttribute('c')", attrAccessor, "null,null,c,c,", "Attr");

    verifyNodeSerialization("new Document().createCDATASection('test')", textAccessor, "test", "CDATASection");

    verifyNodeSerialization("document.implementation.createDocumentType('a', 'b', 'c')", "n.name + ',' + n.publicId + ',' + n.systemId", "a,b,c", "DocumentType");

    verifyNodeSerialization("document.createProcessingInstruction('a', 'b')", "n.target + ',' + n.data", "a,b", "ProcessingInstruction");

    auto documentAccessor = "n.URL + ',' + n.documentURI + ',' + new XMLSerializer().serializeToString(n)";
    auto documentExpected = "about:blank,about:blank,";
    verifyNodeSerialization("document.implementation.createDocument('http://www.w3.org/2000/svg', 'svg', null)", documentAccessor, documentExpected, "XMLDocument");
    verifyNodeSerialization("document.implementation.createHTMLDocument('test title')", documentAccessor, documentExpected, "HTMLDocument");
    verifyNodeSerialization("document", documentAccessor, "https://webkit.org/,https://webkit.org/,", "HTMLDocument");
}

}
