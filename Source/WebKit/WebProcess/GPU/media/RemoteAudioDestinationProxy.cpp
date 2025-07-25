/*
 * Copyright (C) 2020-2025 Apple Inc. All rights reserved.
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
#include "RemoteAudioDestinationProxy.h"

#if ENABLE(GPU_PROCESS) && ENABLE(WEB_AUDIO)

#include "GPUConnectionToWebProcess.h"
#include "Logging.h"
#include "RemoteAudioDestinationManagerMessages.h"
#include "WebProcess.h"
#include <WebCore/AudioBus.h>
#include <WebCore/AudioUtilities.h>
#include <WebCore/RealtimeAudioThread.h>
#include <WebCore/SharedMemory.h>
#include <algorithm>
#include <wtf/StdLibExtras.h>

#if PLATFORM(COCOA)
#include <WebCore/AudioUtilitiesCocoa.h>
#include <WebCore/CARingBuffer.h>
#include <WebCore/SpanCoreAudio.h>
#include <WebCore/WebAudioBufferList.h>
#include <mach/mach_time.h>
#endif

namespace WebKit {

#if PLATFORM(COCOA)
// Allocate a ring buffer large enough to contain 2 seconds of audio.
constexpr size_t ringBufferSizeInSecond = 2;
constexpr unsigned maxAudioBufferListSampleCount = 4096;
#endif

using AudioIOCallback = WebCore::AudioIOCallback;

Ref<RemoteAudioDestinationProxy> RemoteAudioDestinationProxy::create(const CreationOptions& options)
{
    return adoptRef(*new RemoteAudioDestinationProxy(options));
}

RemoteAudioDestinationProxy::RemoteAudioDestinationProxy(const CreationOptions& options)
    : WebCore::AudioDestinationResampler(options, hardwareSampleRate())
    , m_inputDeviceId(options.inputDeviceId)
    , m_numberOfInputChannels(options.numberOfInputChannels)
    , m_remoteSampleRate(hardwareSampleRate())
#if PLATFORM(IOS_FAMILY)
    , m_sceneIdentifier(options.sceneIdentifier)
#endif
{
#if PLATFORM(MAC)
    // On macOS, we are seeing page load time improvements when eagerly creating the Audio destination in the GPU process. See rdar://124071843.
    RunLoop::currentSingleton().dispatch([protectedThis = Ref { *this }]() {
        protectedThis->connection();
    });
#endif
}

uint32_t RemoteAudioDestinationProxy::totalFrameCount() const
{
    RELEASE_ASSERT(m_frameCount->size() == sizeof(std::atomic<uint32_t>));
    return WTF::atomicLoad(&spanReinterpretCast<uint32_t>(m_frameCount->mutableSpan())[0]);
}

void RemoteAudioDestinationProxy::startRenderingThread()
{
    ASSERT(!m_renderThread);

    auto offThreadRendering = [this, protectedThis = Ref { *this }]() mutable {
        do {
            m_renderSemaphore.wait();
            if (m_shouldStopThread || !m_frameCount)
                break;

            uint32_t totalFrameCount = this->totalFrameCount();
            uint32_t frameCount = (totalFrameCount < m_lastFrameCount) ? (totalFrameCount + (std::numeric_limits<uint32_t>::max() - m_lastFrameCount)) : (totalFrameCount - m_lastFrameCount);

            m_lastFrameCount = totalFrameCount;
            renderAudio(frameCount);

        } while (!m_shouldStopThread);
    };

    // FIXME(263073): Coalesce compatible realtime threads together to render sequentially
    // rather than have separate realtime threads for each RemoteAudioDestinationProxy.
    m_renderThread = WebCore::createMaybeRealtimeAudioThread("RemoteAudioDestinationProxy render thread"_s, WTFMove(offThreadRendering), Seconds { 128 / m_remoteSampleRate });
}

void RemoteAudioDestinationProxy::stopRenderingThread()
{
    RefPtr renderThread = m_renderThread;
    if (!renderThread)
        return;

    m_shouldStopThread = true;
    m_renderSemaphore.signal();
    renderThread->waitForCompletion();
    m_renderThread = nullptr;
    m_shouldStopThread = false;
}

IPC::Connection* RemoteAudioDestinationProxy::connection()
{
    RefPtr gpuProcessConnection = m_gpuProcessConnection.get();
    if (!gpuProcessConnection) {
        gpuProcessConnection = WebProcess::singleton().ensureGPUProcessConnection();
        m_gpuProcessConnection = gpuProcessConnection;
        gpuProcessConnection->addClient(*this);
        m_destinationID = RemoteAudioDestinationIdentifier::generate();

        m_lastFrameCount = 0;
        std::optional<WebCore::SharedMemory::Handle> frameCountHandle;
        RefPtr frameCount = WebCore::SharedMemory::allocate(sizeof(std::atomic<uint32_t>));
        m_frameCount = frameCount;
        if (frameCount)
            frameCountHandle = frameCount->createHandle(WebCore::SharedMemory::Protection::ReadWrite);
        RELEASE_ASSERT(frameCountHandle.has_value());
        gpuProcessConnection->connection().sendWithAsyncReply(Messages::RemoteAudioDestinationManager::CreateAudioDestination(*m_destinationID, m_inputDeviceId, m_numberOfInputChannels, m_outputBus->numberOfChannels(), sampleRate(), m_remoteSampleRate, m_renderSemaphore, WTFMove(*frameCountHandle)), [protectedThis = Ref { *this }](size_t latency) {
            protectedThis->m_audioUnitLatency = latency;
        }, 0);

#if PLATFORM(COCOA)
        m_currentFrame = 0;
        auto streamFormat = audioStreamBasicDescriptionForAudioBus(m_outputBus);
        size_t numberOfFrames = m_remoteSampleRate * ringBufferSizeInSecond;
        auto result = ProducerSharedCARingBuffer::allocate(streamFormat, numberOfFrames);
        RELEASE_ASSERT(result); // FIXME(https://bugs.webkit.org/show_bug.cgi?id=262690): Handle allocation failure.
        auto [ringBuffer, handle] = WTFMove(*result);
        m_ringBuffer = WTFMove(ringBuffer);
        gpuProcessConnection->connection().send(Messages::RemoteAudioDestinationManager::AudioSamplesStorageChanged { *m_destinationID, WTFMove(handle) }, 0);
        m_audioBufferList = makeUnique<WebCore::WebAudioBufferList>(streamFormat);
        m_audioBufferList->setSampleCount(maxAudioBufferListSampleCount);
#endif

#if PLATFORM(IOS_FAMILY)
        gpuProcessConnection->connection().send(Messages::RemoteAudioDestinationManager::SetSceneIdentifier { *m_destinationID, m_sceneIdentifier }, 0);
#endif

        startRenderingThread();
    }
    return m_destinationID ? &gpuProcessConnection->connection() : nullptr;
}

IPC::Connection* RemoteAudioDestinationProxy::existingConnection()
{
    auto gpuProcessConnection = m_gpuProcessConnection.get();
    return gpuProcessConnection && m_destinationID ? &gpuProcessConnection->connection() : nullptr;
}

RemoteAudioDestinationProxy::~RemoteAudioDestinationProxy()
{
    if (auto gpuProcessConnection = m_gpuProcessConnection.get(); gpuProcessConnection && m_destinationID)
        gpuProcessConnection->connection().send(Messages::RemoteAudioDestinationManager::DeleteAudioDestination(*m_destinationID), 0);
    stopRenderingThread();
}

void RemoteAudioDestinationProxy::startRendering(CompletionHandler<void(bool)>&& completionHandler)
{
    RefPtr connection = this->connection();
    if (!connection) {
        RunLoop::currentSingleton().dispatch([protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)]() mutable {
            protectedThis->setIsPlaying(false);
            completionHandler(false);
        });
        return;
    }

    connection->sendWithAsyncReply(Messages::RemoteAudioDestinationManager::StartAudioDestination(*m_destinationID), [protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)](bool isPlaying, size_t latency) mutable {
        protectedThis->setIsPlaying(isPlaying);
        protectedThis->m_audioUnitLatency = latency;
        completionHandler(isPlaying);
    });
}

void RemoteAudioDestinationProxy::stopRendering(CompletionHandler<void(bool)>&& completionHandler)
{
    RefPtr connection = existingConnection();
    if (!connection) {
        RunLoop::currentSingleton().dispatch([protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)]() mutable {
            protectedThis->setIsPlaying(false);
            completionHandler(true);
        });
        return;
    }

    connection->sendWithAsyncReply(Messages::RemoteAudioDestinationManager::StopAudioDestination(*m_destinationID), [protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)](bool isPlaying) mutable {
        protectedThis->setIsPlaying(isPlaying);
        completionHandler(!isPlaying);
    });
}

MediaTime RemoteAudioDestinationProxy::outputLatency() const
{
    return (MediaTime { static_cast<int64_t>(m_audioUnitLatency), static_cast<uint32_t>(sampleRate()) }
#if USE(AUDIO_SESSION)
            + MediaTime { static_cast<int64_t>(AudioSession::protectedSharedSession()->outputLatency()), static_cast<uint32_t>(AudioSession::protectedSharedSession()->sampleRate()) }
#endif
            );
}

void RemoteAudioDestinationProxy::renderAudio(unsigned frameCount)
{
    ASSERT(!isMainRunLoop());

#if PLATFORM(COCOA)
    while (frameCount) {
        auto sampleTime = m_currentFrame / static_cast<double>(m_remoteSampleRate);
        auto hostTime =  MonotonicTime::fromMachAbsoluteTime(mach_absolute_time());
        size_t numberOfFrames = std::min(frameCount, maxAudioBufferListSampleCount);
        frameCount -= numberOfFrames;
        auto* ioData = m_audioBufferList->list();


        auto numberOfBuffers = std::min<UInt32>(ioData->mNumberBuffers, m_outputBus->numberOfChannels());
        auto buffers = unsafeMakeSpan(ioData->mBuffers, numberOfBuffers);

        // Associate the destination data array with the output bus then fill the FIFO.
        for (UInt32 i = 0; i < numberOfBuffers; ++i) {
            auto memory = mutableSpan<float>(buffers[i]);
            if (numberOfFrames < memory.size())
                memory = memory.first(numberOfFrames);
            m_outputBus->setChannelMemory(i, memory);
        }
        size_t framesToRender = pullRendered(numberOfFrames);
        m_ringBuffer->store(m_audioBufferList->list(), numberOfFrames, m_currentFrame);
        render(sampleTime, hostTime, framesToRender);
        m_currentFrame += numberOfFrames;
    }
#endif
}

#if PLATFORM(IOS_FAMILY)
void RemoteAudioDestinationProxy::setSceneIdentifier(const String& sceneIdentifier)
{
    if (sceneIdentifier == m_sceneIdentifier)
        return;
    m_sceneIdentifier = sceneIdentifier;

    if (auto gpuProcessConnection = m_gpuProcessConnection.get(); gpuProcessConnection && m_destinationID)
        gpuProcessConnection->connection().send(Messages::RemoteAudioDestinationManager::SetSceneIdentifier { *m_destinationID, m_sceneIdentifier }, 0);
}
#endif

void RemoteAudioDestinationProxy::gpuProcessConnectionDidClose(GPUProcessConnection& oldConnection)
{
    stopRenderingThread();
    m_gpuProcessConnection = nullptr;
    m_destinationID = std::nullopt;

    if (isPlaying())
        startRendering([](bool) { });
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS) && ENABLE(WEB_AUDIO)
