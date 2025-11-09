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

#include "config.h"
#include "RemoteMediaSessionManagerProxy.h"

#if ENABLE(VIDEO) || ENABLE(WEB_AUDIO)

#include "RemoteMediaSessionManagerProxyMessages.h"
#include "RemoteMediaSessionState.h"
#include "WebPage.h"
#include "WebProcessProxy.h"
#include <WebCore/NotImplemented.h>
#include <WebCore/PlatformMediaSessionManager.h>
#include <algorithm>
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteMediaSessionManagerProxy);

RefPtr<RemoteMediaSessionManagerProxy> RemoteMediaSessionManagerProxy::create(WebCore::PageIdentifier identifier, WebProcessProxy& process)
{
    return adoptRef(new RemoteMediaSessionManagerProxy(identifier, process));
}

RemoteMediaSessionManagerProxy::RemoteMediaSessionManagerProxy(WebCore::PageIdentifier identifier, WebProcessProxy& process)
    : m_process(process)
    , m_topPageID(identifier)
{
    process.addMessageReceiver(Messages::RemoteMediaSessionManagerProxy::messageReceiverName(), m_topPageID, *this);
}

RemoteMediaSessionManagerProxy::~RemoteMediaSessionManagerProxy()
{
    m_process->removeMessageReceiver(Messages::RemoteMediaSessionManagerProxy::messageReceiverName(), m_topPageID);
}

void RemoteMediaSessionManagerProxy::addSession(RemoteMediaSessionState&&)
{
    notImplemented();
}

void RemoteMediaSessionManagerProxy::removeSession(RemoteMediaSessionState&&)
{
    notImplemented();
}

void RemoteMediaSessionManagerProxy::setCurrentSession(RemoteMediaSessionState&&)
{
    notImplemented();
}

void RemoteMediaSessionManagerProxy::updateSessionState()
{
    notImplemented();
}

IPC::Connection* RemoteMediaSessionManagerProxy::messageSenderConnection() const
{
    return &m_process->connection();
}

uint64_t RemoteMediaSessionManagerProxy::messageSenderDestinationID() const
{
    return m_topPageID.toUInt64();
}

std::optional<SharedPreferencesForWebProcess> RemoteMediaSessionManagerProxy::sharedPreferencesForWebProcess() const
{
    return m_process->sharedPreferencesForWebProcess();
}

} // namespace WebKit

#endif // ENABLE(VIDEO) || ENABLE(WEB_AUDIO)
