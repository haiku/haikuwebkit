/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
#import "WKNavigationResponseInternal.h"

#import "WKFrameInfoInternal.h"
#import "WKNavigationInternal.h"
#import <WebCore/WebCoreObjCExtras.h>

@implementation WKNavigationResponse

WK_OBJECT_DISABLE_DISABLE_KVC_IVAR_ACCESS;

- (void)dealloc
{
    if (WebCoreObjCScheduleDeallocateOnMainRunLoop(WKNavigationResponse.class, self))
        return;

    RefPtr { _navigationResponse.get() }->~NavigationResponse();

    [super dealloc];
}

- (NSString *)description
{
    return [NSString stringWithFormat:@"<%@: %p; response = %@>", NSStringFromClass(self.class), self, self.response];
}

- (BOOL)isForMainFrame
{
    return _navigationResponse->frame().isMainFrame();
}

- (NSURLResponse *)response
{
    return _navigationResponse->response().nsURLResponse();
}

- (BOOL)canShowMIMEType
{
    return _navigationResponse->canShowMIMEType();
}

#pragma mark WKObject protocol implementation

- (API::Object&)_apiObject
{
    return *_navigationResponse;
}

@end

@implementation WKNavigationResponse (WKPrivate)

- (WKFrameInfo *)_frame
{
    // FIXME: This RefPtr should not be necessary. Remove it once clang static analyzer is fixed.
    return wrapper(RefPtr { _navigationResponse.get() }->protectedFrame().get());
}

- (WKFrameInfo *)_navigationInitiatingFrame
{
    return wrapper(RefPtr { _navigationResponse.get() }->navigationInitiatingFrame());
}

- (WKNavigation *)_navigation
{
    return wrapper(_navigationResponse->navigation());
}

- (NSURLRequest *)_request
{
    return _navigationResponse->request().nsURLRequest(WebCore::HTTPBodyUpdatePolicy::DoNotUpdateHTTPBody);
}

- (NSString *)_downloadAttribute
{
    const String& attribute = _navigationResponse->downloadAttribute();
    return attribute.isNull() ? nil : attribute.createNSString().autorelease();
}

- (BOOL)_wasPrivateRelayed
{
    return _navigationResponse->response().wasPrivateRelayed();
}

- (NSString *)_proxyName
{
    return _navigationResponse->response().proxyName().createNSString().autorelease();
}

- (BOOL)_isFromNetwork
{
    return _navigationResponse->response().source() == WebCore::ResourceResponseBase::Source::Network;
}
@end
