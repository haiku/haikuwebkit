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

#pragma once

#if PLATFORM(COCOA) && ENABLE(GPU_PROCESS) && ENABLE(MEDIA_STREAM)

#include "Connection.h"
#include "LayerHostingContext.h"
#include "SampleBufferDisplayLayerIdentifier.h"
#include "SharedPreferencesForWebProcess.h"
#include "WorkQueueMessageReceiver.h"
#include <WebCore/FloatRect.h>
#include <WebCore/IntSize.h>
#include <wtf/HashMap.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/WeakRef.h>
#include <wtf/WorkQueue.h>

namespace IPC {
class Decoder;
}

namespace WebCore {
class IntSize;
}

namespace WebKit {

class GPUConnectionToWebProcess;
class RemoteSampleBufferDisplayLayer;

class RemoteSampleBufferDisplayLayerManager final : public IPC::WorkQueueMessageReceiver<WTF::DestructionThread::Any> {
    WTF_MAKE_TZONE_ALLOCATED(RemoteSampleBufferDisplayLayerManager);
public:
    static Ref<RemoteSampleBufferDisplayLayerManager> create(GPUConnectionToWebProcess& connection, SharedPreferencesForWebProcess& sharedPreferencesForWebProcess)
    {
        auto instance = adoptRef(*new RemoteSampleBufferDisplayLayerManager(connection, sharedPreferencesForWebProcess));
        instance->startListeningForIPC();
        return instance;
    }
    ~RemoteSampleBufferDisplayLayerManager();

    void ref() const final { IPC::WorkQueueMessageReceiver<WTF::DestructionThread::Any>::ref(); }
    void deref() const final { IPC::WorkQueueMessageReceiver<WTF::DestructionThread::Any>::deref(); }

    void close();

    bool allowsExitUnderMemoryPressure() const;
    void updateSampleBufferDisplayLayerBoundsAndPosition(SampleBufferDisplayLayerIdentifier, WebCore::FloatRect, std::optional<MachSendRightAnnotated>&&);
    std::optional<SharedPreferencesForWebProcess> sharedPreferencesForWebProcess() const { return m_sharedPreferencesForWebProcess; }
    void updateSharedPreferencesForWebProcess(SharedPreferencesForWebProcess);

private:
    explicit RemoteSampleBufferDisplayLayerManager(GPUConnectionToWebProcess&, SharedPreferencesForWebProcess&);
    void startListeningForIPC();

    // IPC::WorkQueueMessageReceiver overrides.
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;

    bool dispatchMessage(IPC::Connection&, IPC::Decoder&);

    using LayerCreationCallback = CompletionHandler<void(WebCore::HostingContext)>&&;
    void createLayer(SampleBufferDisplayLayerIdentifier, bool hideRootLayer, WebCore::IntSize, bool shouldMaintainAspectRatio, bool canShowWhileLocked, LayerCreationCallback);
    void releaseLayer(SampleBufferDisplayLayerIdentifier);

    ThreadSafeWeakPtr<GPUConnectionToWebProcess> m_connectionToWebProcess;
    const Ref<IPC::Connection> m_connection;
    SharedPreferencesForWebProcess m_sharedPreferencesForWebProcess;
    const Ref<WorkQueue> m_queue;
    mutable Lock m_layersLock;
    HashMap<SampleBufferDisplayLayerIdentifier, Ref<RemoteSampleBufferDisplayLayer>> m_layers WTF_GUARDED_BY_LOCK(m_layersLock);
};

}

#endif // PLATFORM(COCOA) && ENABLE(GPU_PROCESS) && ENABLE(MEDIA_STREAM)
