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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(VIDEO) || ENABLE(WEB_AUDIO)

#include "MessageReceiver.h"
#include "MessageSender.h"
#include "WebPageProxyIdentifier.h"
#include <WebCore/PlatformMediaSessionManager.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/Vector.h>
#include <wtf/WeakListHashSet.h>

namespace WebKit {

class WebPage;

class RemoteMediaSessionManager
    : public WebCore::PlatformMediaSessionManager
    , public IPC::MessageReceiver
    , public IPC::MessageSender {
        WTF_MAKE_TZONE_ALLOCATED(RemoteMediaSessionManager);
    public:
        static RefPtr<RemoteMediaSessionManager> create(WebPage& topPage, WebPage& localPage);

        virtual ~RemoteMediaSessionManager();

        void ref() const final { ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr::ref(); }
        void deref() const final { ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr::deref(); }

    protected:
        RemoteMediaSessionManager(WebPage& topPage, WebPage& localPage);

        void addSession(WebCore::PlatformMediaSessionInterface&) final;
        void removeSession(WebCore::PlatformMediaSessionInterface&) final;
        void setCurrentSession(WebCore::PlatformMediaSessionInterface&) final;

        void didReceiveMessage(IPC::Connection&, IPC::Decoder&);

        // IPC::MessageSender.
        IPC::Connection* messageSenderConnection() const final;
        uint64_t messageSenderDestinationID() const final;

    private:
        void updateSessionState() final;

        WeakPtr<WebPage> m_topPage;
        WeakPtr<WebPage> m_localPage;
        WebCore::PageIdentifier m_topPageID;
        WebCore::PageIdentifier m_localPageID;
    };

} // namespace WebKit

#endif // ENABLE(VIDEO) || ENABLE(WEB_AUDIO)

