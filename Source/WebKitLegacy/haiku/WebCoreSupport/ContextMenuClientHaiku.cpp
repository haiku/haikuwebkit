/*
 * Copyright (C) 2006 Zack Rusin <zack@kde.org>
 * Copyright (C) 2007 Ryan Leavengood <leavengood@gmail.com>
 * Copyright (C) 2010 Stephan Aßmus <superstippi@gmx.de>
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include "config.h"
#include "ContextMenuClientHaiku.h"

#include "WebCore/ContextMenu.h"
#include "WebCore/Document.h"
#include "WebCore/Editor.h"
#include "WebCore/Event.h"
#include "WebCore/Frame.h"
#include "WebCore/FrameInlines.h"
#include "WebCore/FrameLoader.h"
#include "WebCore/FrameLoadRequest.h"
#include "WebCore/HitTestResult.h"
#include "WebCore/LocalFrameInlines.h"
#include "WebCore/NotImplemented.h"
#include "WebCore/Page.h"
#include "WebCore/ResourceRequest.h"
#include "WebCore/UserGestureIndicator.h"
#include "wtf/URL.h"
#include "WebPage.h"


namespace WebCore {

ContextMenuClientHaiku::ContextMenuClientHaiku(BWebPage* webPage)
    : m_webPage(webPage)
{
}

void ContextMenuClientHaiku::downloadURL(const URL& url)
{
    URL urlCopy(url);
    ResourceRequest request(std::move(urlCopy));
    m_webPage->requestDownload(request);
}

void ContextMenuClientHaiku::searchWithGoogle(const LocalFrame* frame)
{
    String searchString = frame->editor().selectedText();
    String encoded = WTF::encodeWithURLEscapeSequences(searchString.trim(deprecatedIsSpaceOrNewline));
    encoded = makeStringByReplacingAll(encoded, "%20"_s, "+"_s);

    StringBuilder url;
    url.append("http://www.google.com/search?q="_s);
    url.append(encoded);

    if (Page* page = frame->page()) {
        UserGestureIndicator indicator(IsProcessingUserGesture);
        static_cast<WebCore::LocalFrame&>(page->mainFrame()).loader().changeLocation(URL({ }, url.toString()),
            ASCIILiteral::fromLiteralUnsafe("_blank"), 0, ReferrerPolicy::EmptyString,
            frame->document()->shouldOpenExternalURLsPolicyToPropagate());
    }
}

void ContextMenuClientHaiku::lookUpInDictionary(LocalFrame*)
{
    notImplemented();
}

void ContextMenuClientHaiku::speak(const String&)
{
    notImplemented();
}

bool ContextMenuClientHaiku::isSpeaking() const
{
    notImplemented();
    return false;
}

void ContextMenuClientHaiku::stopSpeaking()
{
    notImplemented();
}

} // namespace WebCore

