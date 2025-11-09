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
#include "RemoteMediaSessionManager.h"

#if ENABLE(VIDEO) || ENABLE(WEB_AUDIO)

#include "Logging.h"
#include "RemoteMediaSessionManagerMessages.h"
#include "RemoteMediaSessionManagerProxyMessages.h"
#include "RemoteMediaSessionState.h"
#include "WebPage.h"
#include "WebPageProxyMessages.h"
#include "WebProcess.h"
#include <WebCore/PlatformMediaSession.h>
#include <algorithm>
#include <ranges>
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteMediaSessionManager);

static RemoteMediaSessionState platformSessionState(const WebCore::PlatformMediaSessionInterface& session)
{
    return {
        .sessionIdentifier = session.mediaSessionIdentifier(),
#if !RELEASE_LOG_DISABLED
        .logIdentifier = session.logIdentifier(),
#endif
        .mediaType = session.mediaType(),
        .presentationType = session.presentationType(),
        .displayType = session.displayType(),

        .duration = session.duration(),

        .groupIdentifier = session.mediaSessionGroupIdentifier(),
        .nowPlayingInfo = session.nowPlayingInfo(),

        .shouldOverrideBackgroundLoadingRestriction = session.shouldOverrideBackgroundLoadingRestriction(),
        .isPlayingToWirelessPlaybackTarget = session.isPlayingToWirelessPlaybackTarget(),
        .isPlayingOnSecondScreen = session.isPlayingOnSecondScreen(),
        .hasMediaStreamSource = session.hasMediaStreamSource(),
        .shouldOverridePauseDuringRouteChange = session.shouldOverridePauseDuringRouteChange(),
        .isNowPlayingEligible = session.isNowPlayingEligible(),
        .canProduceAudio = session.canProduceAudio(),
        .isSuspended = session.isSuspended(),
        .isPlaying = session.isPlaying(),
        .isAudible = session.isAudible(),
        .isEnded = session.isEnded(),
        .canReceiveRemoteControlCommands = session.canReceiveRemoteControlCommands(),
        .supportsSeeking = session.supportsSeeking(),
    };
}

RefPtr<RemoteMediaSessionManager> RemoteMediaSessionManager::create(WebPage& topPage, WebPage& localPage)
{
    return adoptRef(new RemoteMediaSessionManager(topPage, localPage));
}

RemoteMediaSessionManager::RemoteMediaSessionManager(WebPage& topPage, WebPage& localPage)
    : PlatformMediaSessionManager(localPage.identifier())
    , m_topPage(topPage)
    , m_localPage(localPage)
    , m_topPageID(topPage.identifier())
    , m_localPageID(localPage.identifier())
{
    WebProcess::singleton().addMessageReceiver(Messages::RemoteMediaSessionManager::messageReceiverName(), m_localPageID, *this);

    localPage.send(Messages::WebPageProxy::EnsureRemoteMediaSessionManagerProxy());
}

RemoteMediaSessionManager::~RemoteMediaSessionManager()
{
    WebProcess::singleton().removeMessageReceiver(Messages::RemoteMediaSessionManager::messageReceiverName(), m_localPageID);
}

void RemoteMediaSessionManager::addSession(WebCore::PlatformMediaSessionInterface& session)
{
    PlatformMediaSessionManager::addSession(session);
    send(Messages::RemoteMediaSessionManagerProxy::AddSession(platformSessionState(session)));
}

void RemoteMediaSessionManager::removeSession(WebCore::PlatformMediaSessionInterface& session)
{
    PlatformMediaSessionManager::removeSession(session);
    send(Messages::RemoteMediaSessionManagerProxy::RemoveSession(platformSessionState(session)));
}

void RemoteMediaSessionManager::setCurrentSession(WebCore::PlatformMediaSessionInterface& session)
{
    ALWAYS_LOG(LOGIDENTIFIER, session.logIdentifier(), ", size = ", sessions().computeSize());
    PlatformMediaSessionManager::setCurrentSession(session);
    send(Messages::RemoteMediaSessionManagerProxy::SetCurrentSession(platformSessionState(session)));
}

void RemoteMediaSessionManager::updateSessionState()
{
    send(Messages::RemoteMediaSessionManagerProxy::UpdateSessionState());
}

IPC::Connection* RemoteMediaSessionManager::messageSenderConnection() const
{
    return WebProcess::singleton().parentProcessConnection();
}

uint64_t RemoteMediaSessionManager::messageSenderDestinationID() const
{
    return m_topPageID.toUInt64();
}

} // namespace WebKit

#endif // ENABLE(VIDEO) || ENABLE(WEB_AUDIO)
