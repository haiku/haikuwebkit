/*
 * Copyright (C) 2014-2025 Apple Inc. All rights reserved.
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

#if ENABLE(USER_MESSAGE_HANDLERS)

#include "FrameDestructionObserver.h"
#include "UserContentProvider.h"
#include "UserMessageHandler.h"
#include <wtf/HashMap.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/text/AtomString.h>
#include <wtf/text/AtomStringHash.h>

namespace WebCore {

class DOMWrapperWorld;
class LocalFrame;
class UserMessageHandler;

class UserMessageHandlersNamespace : public RefCounted<UserMessageHandlersNamespace>, public FrameDestructionObserver, public UserContentProviderInvalidationClient {
public:
    static Ref<UserMessageHandlersNamespace> create(LocalFrame& frame, UserContentProvider& userContentProvider)
    {
        return adoptRef(*new UserMessageHandlersNamespace(frame, userContentProvider));
    }

    virtual ~UserMessageHandlersNamespace();

    Vector<AtomString> supportedPropertyNames() const;
    UserMessageHandler* namedItem(DOMWrapperWorld&, const AtomString&);
    bool isSupportedPropertyName(const AtomString&);

private:
    explicit UserMessageHandlersNamespace(LocalFrame&, UserContentProvider&);

    // UserContentProviderInvalidationClient
    void didInvalidate(UserContentProvider&) override;

    const Ref<UserContentProvider> m_userContentProvider;
    HashMap<std::pair<AtomString, RefPtr<DOMWrapperWorld>>, RefPtr<UserMessageHandler>> m_messageHandlers;
};

} // namespace WebCore

#endif // ENABLE(USER_MESSAGE_HANDLERS)
