/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
#import "CoreIPCNSShadow.h"

#if PLATFORM(COCOA)

#import "DoubleGeometry.h"
#import <WebCore/AttributedString.h>

#if USE(APPKIT)
#import <AppKit/NSShadow.h>
#endif
#if PLATFORM(IOS_FAMILY)
#import <UIKit/NSShadow.h>
#import <pal/ios/UIKitSoftLink.h>
#endif

namespace WebKit {

CoreIPCNSShadow::CoreIPCNSShadow(NSShadow *shadow)
    : m_shadowOffset(shadow.shadowOffset)
    , m_shadowBlurRadius(shadow.shadowBlurRadius)
    , m_shadowColor(shadow.shadowColor)
{
}

RetainPtr<id> CoreIPCNSShadow::toID() const
{
    RetainPtr<NSShadow> result = adoptNS([PlatformNSShadow new]);
    [result setShadowOffset:m_shadowOffset.toCG()];
    [result setShadowBlurRadius:m_shadowBlurRadius];
    [result setShadowColor:m_shadowColor.get()];
    return result;
}

} // namespace WebKit

#endif // PLATFORM(COCOA)
