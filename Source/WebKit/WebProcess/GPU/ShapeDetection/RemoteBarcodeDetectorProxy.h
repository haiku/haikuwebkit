/*
 * Copyright (C) 2023-2025 Apple Inc. All rights reserved.
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

#include "RenderingBackendIdentifier.h"
#include "ShapeDetectionIdentifier.h"
#include <WebCore/BarcodeDetectorInterface.h>
#include <wtf/CompletionHandler.h>
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/WeakPtr.h>

#if ENABLE(GPU_PROCESS)

namespace IPC {
class StreamClientConnection;
}

namespace WebCore::ShapeDetection {
struct BarcodeDetectorOptions;
enum class BarcodeFormat : uint8_t;
struct DetectedBarcode;
}

namespace WebKit::ShapeDetection {

class RemoteBarcodeDetectorProxy : public WebCore::ShapeDetection::BarcodeDetector {
    WTF_MAKE_TZONE_ALLOCATED(RemoteBarcodeDetectorProxy);
public:
    static Ref<RemoteBarcodeDetectorProxy> create(Ref<IPC::StreamClientConnection>&&, RenderingBackendIdentifier, ShapeDetectionIdentifier, const WebCore::ShapeDetection::BarcodeDetectorOptions&);

    virtual ~RemoteBarcodeDetectorProxy();

    static void getSupportedFormats(Ref<IPC::StreamClientConnection>&&, RenderingBackendIdentifier, CompletionHandler<void(Vector<WebCore::ShapeDetection::BarcodeFormat>&&)>&&);

private:
    RemoteBarcodeDetectorProxy(Ref<IPC::StreamClientConnection>&&, RenderingBackendIdentifier, ShapeDetectionIdentifier);

    RemoteBarcodeDetectorProxy(const RemoteBarcodeDetectorProxy&) = delete;
    RemoteBarcodeDetectorProxy(RemoteBarcodeDetectorProxy&&) = delete;
    RemoteBarcodeDetectorProxy& operator=(const RemoteBarcodeDetectorProxy&) = delete;
    RemoteBarcodeDetectorProxy& operator=(RemoteBarcodeDetectorProxy&&) = delete;

    ShapeDetectionIdentifier backing() const { return m_backing; }

    void detect(Ref<WebCore::ImageBuffer>&&, CompletionHandler<void(Vector<WebCore::ShapeDetection::DetectedBarcode>&&)>&&) final;

    ShapeDetectionIdentifier m_backing;
    const Ref<IPC::StreamClientConnection> m_streamClientConnection;
    RenderingBackendIdentifier m_renderingBackendIdentifier;
};

} // namespace WebKit::ShapeDetection

#endif // ENABLE(GPU_PROCESS)
