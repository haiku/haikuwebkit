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

#include "AudioVideoRenderer.h"
#include "PlatformDynamicRangeLimit.h"
#include "ProcessIdentity.h"
#include "WebAVSampleBufferListener.h"
#include <wtf/Forward.h>
#include <wtf/Function.h>
#include <wtf/HashMap.h>
#include <wtf/LoggerHelper.h>
#include <wtf/NativePromise.h>
#include <wtf/StdUnorderedMap.h>
#include <wtf/ThreadSafeWeakPtr.h>

OBJC_CLASS AVSampleBufferAudioRenderer;
OBJC_CLASS AVSampleBufferDisplayLayer;
OBJC_CLASS AVSampleBufferRenderSynchronizer;
OBJC_CLASS AVSampleBufferVideoRenderer;
OBJC_PROTOCOL(WebSampleBufferVideoRendering);

namespace WebCore {

class PixelBufferConformerCV;
class VideoLayerManagerObjC;
class VideoMediaSampleRenderer;

class AudioVideoRendererAVFObjC
    : public AudioVideoRenderer
    , public WebAVSampleBufferListenerClient
    , public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<AudioVideoRendererAVFObjC>
    , private LoggerHelper {
    WTF_MAKE_TZONE_ALLOCATED(AudioVideoRendererAVFObjC);
public:
    static Ref<AudioVideoRendererAVFObjC> create(const Logger& logger, uint64_t logIdentifier) { return adoptRef(*new AudioVideoRendererAVFObjC(logger, logIdentifier)); }

    ~AudioVideoRendererAVFObjC();
    WTF_ABSTRACT_THREAD_SAFE_REF_COUNTED_AND_CAN_MAKE_WEAK_PTR_IMPL;

    // TracksRendererInterface
    TrackIdentifier addTrack(TrackType) final;
    void removeTrack(TrackIdentifier) final;

    void enqueueSample(TrackIdentifier, Ref<MediaSample>&&, std::optional<MediaTime>) final;
    bool isReadyForMoreSamples(TrackIdentifier) final;
    void requestMediaDataWhenReady(TrackIdentifier, Function<void(TrackIdentifier)>&&) final;
    void stopRequestingMediaData(TrackIdentifier) final;

    bool timeIsProgressing() const final;
    MediaTime currentTime() const final;

    void flush() final;
    void flushTrack(TrackIdentifier) final;

    void notifyWhenErrorOccurs(Function<void(PlatformMediaError)>&&) final;

    // SynchronizerInterface
    void play() final;
    void pause() final;
    bool paused() const final;
    void setRate(double) final;
    double effectiveRate() const final;
    void setDuration(MediaTime) final;
    void notifyDurationReached(Function<void(const MediaTime&)>&&) final;
    void prepareToSeek() final;
    Ref<MediaTimePromise> seekTo(const MediaTime&) final;

    // AudioInterface
    void setVolume(float) final;
    void setMuted(bool) final;
    void setPreservesPitch(bool) final;
#if HAVE(AUDIO_OUTPUT_DEVICE_UNIQUE_ID)
    void setOutputDeviceId(const String&) final;
    void setOutputDeviceIdOnRenderer(AVSampleBufferAudioRenderer *);
#endif

    // VideoInterface
    void setIsVisible(bool);
    void setPresentationSize(const IntSize&) final;
    void setShouldMaintainAspectRatio(bool) final;
    void acceleratedRenderingStateChanged(bool) final;
    void contentBoxRectChanged(const LayoutRect&) final;
    void notifyFirstFrameAvailable(Function<void()>&&) final;
    void notifyWhenHasAvailableVideoFrame(Function<void(const MediaTime&, double)>&&) final;
    void notifyWhenRequiresFlushToResume(Function<void()>&&) final;
    void notifyRenderingModeChanged(Function<void()>&&) final;
    void setMinimumUpcomingPresentationTime(const MediaTime&) final;
    void setShouldDisableHDR(bool) final;
    void setPlatformDynamicRangeLimit(const PlatformDynamicRangeLimit&) final;
    void setResourceOwner(const ProcessIdentity& resourceOwner) final { m_resourceOwner = resourceOwner; }
    RefPtr<VideoFrame> currentVideoFrame() const final;
    std::optional<VideoPlaybackQualityMetrics> videoPlaybackQualityMetrics() final;
    PlatformLayerContainer platformVideoLayer() const final;

    // VideoFullscreenInterface
    void setVideoFullscreenLayer(PlatformLayer*, Function<void()>&&) final;
    void setVideoFullscreenFrame(const FloatRect&) final;
    void setTextTrackRepresentation(TextTrackRepresentation*) final;
    void syncTextTrackBounds() final;
    Ref<GenericPromise> setVideoTarget(const PlatformVideoTarget&) final;
    void isInFullscreenOrPictureInPictureChanged(bool) final;

private:
    AudioVideoRendererAVFObjC(const Logger&, uint64_t);

    bool seeking() const;
    MediaTime clampTimeToLastSeekTime(const MediaTime&) const;
    void maybeCompleteSeek();
    bool shouldBePlaying() const;

    std::optional<TrackType> typeOf(TrackIdentifier) const;

    void addAudioRenderer(TrackIdentifier);
    void removeAudioRenderer(TrackIdentifier);
    void destroyAudioRenderers();
    void destroyAudioRenderer(RetainPtr<AVSampleBufferAudioRenderer>);
    RetainPtr<AVSampleBufferAudioRenderer> audioRendererFor(TrackIdentifier) const;
    void applyOnAudioRenderers(NOESCAPE Function<void(AVSampleBufferAudioRenderer *)>&&) const;

    Ref<GenericPromise> updateDisplayLayerIfNeeded();
    bool shouldEnsureLayerOrVideoRenderer() const;
    WebSampleBufferVideoRendering *layerOrVideoRenderer() const;
    Ref<GenericPromise> ensureLayerOrVideoRenderer();
    void ensureLayer();
    void destroyLayer();
    void destroyVideoLayerIfNeeded();
    void ensureVideoRenderer();
    void destroyVideoRenderer();
    Ref<GenericPromise> setVideoRenderer(WebSampleBufferVideoRendering *);
    void configureLayerOrVideoRenderer(WebSampleBufferVideoRendering *);
    Ref<GenericPromise> stageVideoRenderer(WebSampleBufferVideoRendering *);

    enum class AcceleratedVideoMode: uint8_t {
        Layer = 0,
        VideoRenderer,
    };
    AcceleratedVideoMode acceleratedVideoMode() const;

    void notifyError(PlatformMediaError);
    // WebAVSampleBufferListenerClient
    void audioRendererDidReceiveError(AVSampleBufferAudioRenderer *, NSError *) final;

#if HAVE(SPATIAL_TRACKING_LABEL)
    void setSpatialTrackingInfo(bool prefersSpatialAudioExperience, SoundStageSize, const String& sceneIdentifier, const String& defaultLabel, const String& label) final;
    void updateSpatialTrackingLabel();
#endif

    bool isEnabledVideoTrackId(TrackIdentifier) const;
    bool hasSelectedVideo() const;
    void flushVideo();
    void flushAudio();
    void flushAudioTrack(TrackIdentifier);

    void cancelSeekingPromiseIfNeeded();

    RefPtr<VideoMediaSampleRenderer> protectedVideoRenderer() const;

    // Logger
    const Logger& logger() const final { return m_logger.get(); }
    Ref<const Logger> protectedLogger() const { return logger(); }
    ASCIILiteral logClassName() const final { return "AudioVideoRendererAVFObjC"_s; }
    uint64_t logIdentifier() const final { return m_logIdentifier; }
    WTFLogChannel& logChannel() const final;

    enum SeekState {
        Preparing,
        RequiresFlush,
        Seeking,
        WaitingForAvailableFame,
        SeekCompleted,
    };

    String toString(TrackIdentifier) const;
    String toString(SeekState) const;

    const Ref<const Logger> m_logger;
    const uint64_t m_logIdentifier;
    const UniqueRef<VideoLayerManagerObjC> m_videoLayerManager;
    const RetainPtr<AVSampleBufferRenderSynchronizer> m_synchronizer;
    const Ref<WebAVSampleBufferListener> m_listener;

    Function<void(PlatformMediaError)> m_errorCallback;
    Function<void(const MediaTime&)> m_durationReachedCallback;
    Function<void()> m_firstFrameAvailableCallback;
    Function<void(const MediaTime&, double)> m_hasAvailableVideoFrameCallback;
    Function<void()> m_notifyWhenRequiresFlushToResume;
    Function<void()> m_renderingModeChangedCallback;

    RetainPtr<id> m_durationObserver;
    bool m_isPlaying { false };
    double m_rate { 1 };

    float m_volume { 1 };
    bool m_muted { false };
    bool m_preservePitch { true };
#if HAVE(AUDIO_OUTPUT_DEVICE_UNIQUE_ID)
    String m_audioOutputDeviceId;
#endif

    // Seek Logic
    MediaTime m_lastSeekTime;
    SeekState m_seekState { SeekCompleted };
    std::optional<MediaTimePromise::Producer> m_seekPromise;
    RetainPtr<id> m_timeJumpedObserver;
    bool m_isSynchronizerSeeking { false };
    bool m_hasAvailableVideoFrame { false };

    HashMap<TrackIdentifier, TrackType> m_trackTypes;
    HashMap<TrackIdentifier, RetainPtr<AVSampleBufferAudioRenderer>> m_audioRenderers;
    bool m_readyToRequestVideoData { true };
    bool m_readyToRequestAudioData { true };
    RetainPtr<AVSampleBufferDisplayLayer> m_sampleBufferDisplayLayer;
    RetainPtr<AVSampleBufferVideoRenderer> m_sampleBufferVideoRenderer;
    RefPtr<VideoMediaSampleRenderer> m_videoRenderer;
    bool m_renderingCanBeAccelerated { false };
    bool m_visible { false };
    IntSize m_presentationSize;
    bool m_shouldMaintainAspectRatio { true };
    std::optional<TrackIdentifier> m_enabledVideoTrackId;
    bool m_shouldDisableHDR { false };
    PlatformDynamicRangeLimit m_dynamicRangeLimit { PlatformDynamicRangeLimit::initialValueForVideos() };
    ProcessIdentity m_resourceOwner;

    std::unique_ptr<PixelBufferConformerCV> m_rgbConformer;

#if HAVE(SPATIAL_TRACKING_LABEL)
    bool m_prefersSpatialAudioExperience { false };
    SoundStageSize m_soundStage { SoundStageSize::Auto };
    String m_sceneIdentifier;
    String m_defaultSpatialTrackingLabel;
    String m_spatialTrackingLabel;
#endif

    bool m_needsDestroyVideoLayer { false };
#if ENABLE(LINEAR_MEDIA_PLAYER)
    RetainPtr<FigVideoTargetRef> m_videoTarget;
#endif
};

}
