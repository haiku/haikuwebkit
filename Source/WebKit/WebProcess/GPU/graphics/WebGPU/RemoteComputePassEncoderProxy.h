/*
 * Copyright (C) 2021-2025 Apple Inc. All rights reserved.
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

#if ENABLE(GPU_PROCESS)

#include "RemoteCommandEncoderProxy.h"
#include "WebGPUIdentifier.h"
#include <WebCore/WebGPUComputePassEncoder.h>
#include <wtf/TZoneMalloc.h>

namespace WebKit::WebGPU {

class ConvertToBackingContext;

class RemoteComputePassEncoderProxy final : public WebCore::WebGPU::ComputePassEncoder {
    WTF_MAKE_TZONE_ALLOCATED(RemoteComputePassEncoderProxy);
public:
    static Ref<RemoteComputePassEncoderProxy> create(RemoteCommandEncoderProxy& parent, ConvertToBackingContext& convertToBackingContext, WebGPUIdentifier identifier)
    {
        return adoptRef(*new RemoteComputePassEncoderProxy(parent, convertToBackingContext, identifier));
    }

    virtual ~RemoteComputePassEncoderProxy();

    RemoteGPUProxy& root() const { return m_root; }

private:
    friend class DowncastConvertToBackingContext;

    RemoteComputePassEncoderProxy(RemoteCommandEncoderProxy&, ConvertToBackingContext&, WebGPUIdentifier);

    RemoteComputePassEncoderProxy(const RemoteComputePassEncoderProxy&) = delete;
    RemoteComputePassEncoderProxy(RemoteComputePassEncoderProxy&&) = delete;
    RemoteComputePassEncoderProxy& operator=(const RemoteComputePassEncoderProxy&) = delete;
    RemoteComputePassEncoderProxy& operator=(RemoteComputePassEncoderProxy&&) = delete;

    WebGPUIdentifier backing() const { return m_backing; }
    
    template<typename T>
    WARN_UNUSED_RETURN IPC::Error send(T&& message)
    {
        return root().protectedStreamClientConnection()->send(WTFMove(message), backing());
    }

    void setPipeline(const WebCore::WebGPU::ComputePipeline&) final;
    void dispatch(WebCore::WebGPU::Size32 workgroupCountX, WebCore::WebGPU::Size32 workgroupCountY = 1, WebCore::WebGPU::Size32 workgroupCountZ = 1) final;
    void dispatchIndirect(const WebCore::WebGPU::Buffer& indirectBuffer, WebCore::WebGPU::Size64 indirectOffset) final;

    void end() final;

    void setBindGroup(WebCore::WebGPU::Index32, const WebCore::WebGPU::BindGroup*,
        std::optional<Vector<WebCore::WebGPU::BufferDynamicOffset>>&&) final;

    void setBindGroup(WebCore::WebGPU::Index32, const WebCore::WebGPU::BindGroup*,
        std::span<const uint32_t> dynamicOffsetsArrayBuffer,
        WebCore::WebGPU::Size64 dynamicOffsetsDataStart,
        WebCore::WebGPU::Size32 dynamicOffsetsDataLength) final;

    void pushDebugGroup(String&& groupLabel) final;
    void popDebugGroup() final;
    void insertDebugMarker(String&& markerLabel) final;

    void setLabelInternal(const String&) final;

    WebGPUIdentifier m_backing;
    const Ref<ConvertToBackingContext> m_convertToBackingContext;
    const Ref<RemoteGPUProxy> m_root;
};

} // namespace WebKit::WebGPU

#endif // ENABLE(GPU_PROCESS)
