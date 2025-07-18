/*
 * Copyright (C) 2017-2025 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted, provided that the following conditions
 * are required to be met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. AND ITS CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if USE(LIBWEBRTC)

#include "AudioSampleDataSource.h"
#include "RealtimeOutgoingAudioSource.h"
#include <wtf/CheckedRef.h>
#include <wtf/TZoneMalloc.h>

namespace webrtc {
class AudioTrackInterface;
class AudioTrackSinkInterface;
}

namespace WebCore {

class RealtimeOutgoingAudioSourceCocoa final : public RealtimeOutgoingAudioSource, public CanMakeCheckedPtr<RealtimeOutgoingAudioSourceCocoa> {
    WTF_MAKE_TZONE_ALLOCATED(RealtimeOutgoingAudioSourceCocoa);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(RealtimeOutgoingAudioSourceCocoa);
public:
    static Ref<RealtimeOutgoingAudioSourceCocoa> create(Ref<MediaStreamTrackPrivate>&& audioSource) { return adoptRef(*new RealtimeOutgoingAudioSourceCocoa(WTFMove(audioSource))); }

private:
    explicit RealtimeOutgoingAudioSourceCocoa(Ref<MediaStreamTrackPrivate>&&);
    ~RealtimeOutgoingAudioSourceCocoa();

    // CheckedPtr interface
    uint32_t checkedPtrCount() const final { return CanMakeCheckedPtr::checkedPtrCount(); }
    uint32_t checkedPtrCountWithoutThreadCheck() const final { return CanMakeCheckedPtr::checkedPtrCountWithoutThreadCheck(); }
    void incrementCheckedPtrCount() const final { CanMakeCheckedPtr::incrementCheckedPtrCount(); }
    void decrementCheckedPtrCount() const final { CanMakeCheckedPtr::decrementCheckedPtrCount(); }

    void audioSamplesAvailable(const MediaTime&, const PlatformAudioData&, const AudioStreamDescription&, size_t) final;

    bool isReachingBufferedAudioDataHighLimit() final;
    bool isReachingBufferedAudioDataLowLimit() final;
    bool hasBufferedEnoughData() final;
    void sourceUpdated() final;

    void updateSampleConverter(const AudioStreamDescription&);
    void pullAudioData();

    const Ref<AudioSampleDataSource> m_sampleConverter;
    std::optional<CAAudioStreamDescription> m_inputStreamDescription;
    std::optional<CAAudioStreamDescription> m_outputStreamDescription;

    Vector<uint8_t> m_audioBuffer;
    uint64_t m_readCount { 0 };
    uint64_t m_writeCount { 0 };
    bool m_skippingAudioData { false };
};

} // namespace WebCore

#endif // USE(LIBWEBRTC)
