/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#pragma once

#include <wtf/Forward.h>

#if PLATFORM(IOS_FAMILY)
#define EXPERIMENTAL_FULLSCREEN_API_HIDDEN false
#else
#define EXPERIMENTAL_FULLSCREEN_API_HIDDEN true
#endif

namespace WebKit {

#if PLATFORM(IOS_FAMILY)
bool defaultAllowsInlineMediaPlayback();
bool defaultAllowsInlineMediaPlaybackAfterFullscreen();
bool defaultAllowsPictureInPictureMediaPlayback();
bool defaultInlineMediaPlaybackRequiresPlaysInlineAttribute();
bool defaultPassiveTouchListenersAsDefaultOnDocument();
bool defaultRequiresUserGestureToLoadVideo();
bool defaultShowModalDialogEnabled();
bool defaultWebSQLEnabled();
bool defaultAllowContentSecurityPolicySourceStarToMatchAnyProtocol();
#endif

#if PLATFORM(MAC)
bool defaultLoadDeferringEnabled();
bool defaultWindowFocusRestricted();
bool defaultUsePreHTML5ParserQuirks();
bool defaultNeedsAdobeFrameReloadingQuirk();
bool defaultScrollAnimatorEnabled();
bool defaultTreatsAnyTextCSSLinkAsStylesheet();
bool defaultNeedsFrameNameFallbackToIdQuirk();
bool defaultNeedsKeyboardEventDisambiguationQuirks();
#endif

bool defaultMutationEventsEnabled();
bool defaultAttachmentElementEnabled();
bool defaultShouldRestrictBaseURLSchemes();
bool defaultAllowDisplayOfInsecureContent();
bool defaultAllowRunningOfInsecureContent();
bool defaultShouldConvertInvalidURLsToBlank();
bool defaultPopoverAttributeEnabled();

#if PLATFORM(MAC)
bool defaultPassiveWheelListenersAsDefaultOnDocument();
bool defaultWheelEventGesturesBecomeNonBlocking();
#endif

#if ENABLE(MEDIA_SOURCE) && PLATFORM(IOS_FAMILY)
bool defaultMediaSourceEnabled();
#endif

} // namespace WebKit
