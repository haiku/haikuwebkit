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

#pragma once

#include "RemoteMediaPlayerState.h"
#include <WebCore/PlatformMediaSessionTypes.h>

namespace WebKit {

class RemoteMediaSessionManagerProxy;

class RemoteMediaSessionClientProxy : final WebCore::PlatformMediaSessionClient {
    WTF_MAKE_TZONE_ALLOCATED(RemoteMediaSessionClientProxy);
public:
public:
    static Ref<RemoteMediaSessionClientProxy> create(RemoteMediaPlayerState& state, RemoteMediaSessionManagerProxy& manager)
    {
        return adoptRef(*new RemoteMediaSessionClientProxy(state, manager));
    }

    virtual ~RemoteMediaSessionClientProxy();

protected:
    RemoteMediaSessionClientProxy(RemoteMediaPlayerState&, RemoteMediaSessionManagerProxy&);

    RefPtr<MediaSessionManagerInterface> sessionManager() const final;

    PlatformMediaSessionMediaType mediaType() const final { return m_state.mediaType };
    PlatformMediaSessionMediaType presentationType() const final { return m_state.presentationType };
    PlatformMediaSessionDisplayType displayType() const final { return m_state.displayType };

    void resumeAutoplaying() final;
    void mayResumePlayback(bool shouldResume) final;
    void suspendPlayback() final;

    bool canReceiveRemoteControlCommands() const final { return m_state.canReceiveRemoteControlCommands };
    void didReceiveRemoteControlCommand(PlatformMediaSessionRemoteControlCommandType, const PlatformMediaSessionRemoteCommandArgument&) final;
    bool supportsSeeking() const final { return m_state.supportsSeeking };

    bool canProduceAudio() const final { return m_state.canProduceAudio };
    bool isSuspended() const final { return m_state.isSuspended };
    bool isPlaying() const final { return m_state.isPlaying };
    bool isAudible() const final { return m_state.isAudible };
    bool isEnded() const final { return m_state.isEnded };
    MediaTime mediaSessionDuration() const final { return m_state.mediaSessionDuration };

    bool shouldOverrideBackgroundPlaybackRestriction(PlatformMediaSessionInterruptionType) const final;
    bool shouldOverrideBackgroundLoadingRestriction() const final { return m_state.shouldOverrideBackgroundLoadingRestriction };

    bool isPlayingToWirelessPlaybackTarget() const final { return m_state.isPlayingToWirelessPlaybackTarget };

    bool isPlayingOnSecondScreen() const final { return m_state.isPlayingOnSecondScreen };

    std::optional<MediaSessionGroupIdentifier> mediaSessionGroupIdentifier() const final { return m_state.mediaSessionGroupIdentifier };

    bool hasMediaStreamSource() const final { return m_state.hasMediaStreamSource };

    bool shouldOverridePauseDuringRouteChange() const final { return m_state.shouldOverridePauseDuringRouteChange };

    bool isNowPlayingEligible() const final { return m_state.isNowPlayingEligible };
    std::optional<NowPlayingInfo> nowPlayingInfo() const final { return m_state.nowPlayingInfo };

    WeakPtr<PlatformMediaSessionInterface> selectBestMediaSession(const Vector<WeakPtr<PlatformMediaSessionInterface>>&, PlatformMediaSessionPlaybackControlsPurpose) { return nullptr; }

#if !RELEASE_LOG_DISABLED
    const Logger& logger() const = 0;
    Ref<const Logger> protectedLogger() const { return logger(); }
    uint64_t logIdentifier() const = 0;
#endif

    virtual ~RemoteMediaSessionClientProxy();

private:
    WeakPtr<RemoteMediaSessionManagerProxy> m_manager;
    RemoteMediaPlayerState m_state;
};

} // namespace WebKit
