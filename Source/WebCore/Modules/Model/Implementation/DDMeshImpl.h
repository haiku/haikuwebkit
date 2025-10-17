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

#if HAVE(WEBGPU_IMPLEMENTATION)

#include "WebGPUPtr.h"
#include <WebCore/DDMesh.h>
#include <WebCore/DDMeshDescriptor.h>
#include <WebCore/WebGPUPredefinedColorSpace.h>
#include <WebGPU/WebGPU.h>
#include <WebGPU/WebGPUExt.h>
#include <wtf/TZoneMalloc.h>

namespace WebCore::DDModel {

class ConvertToBackingContext;

struct DDMeshDescriptor;

class DDMeshImpl final : public DDMesh {
    WTF_MAKE_TZONE_ALLOCATED(DDMeshImpl);
public:
    static Ref<DDMeshImpl> create(WebGPU::WebGPUPtr<WGPUDDMesh>&& ddMesh, ConvertToBackingContext& convertToBackingContext)
    {
        return adoptRef(*new DDMeshImpl(WTFMove(ddMesh), convertToBackingContext));
    }

    virtual ~DDMeshImpl();

    WGPUDDMesh backing() const { return m_backing.get(); };

private:
    friend class DowncastConvertToBackingContext;

    DDMeshImpl(WebGPU::WebGPUPtr<WGPUDDMesh>&&, ConvertToBackingContext&);

    DDMeshImpl(const DDMeshImpl&) = delete;
    DDMeshImpl(DDMeshImpl&&) = delete;
    DDMeshImpl& operator=(const DDMeshImpl&) = delete;
    DDMeshImpl& operator=(DDMeshImpl&&) = delete;

    void setLabelInternal(const String&) final;
#if PLATFORM(COCOA)
    void update(const DDUpdateMeshDescriptor&) final;
#endif

    const Ref<ConvertToBackingContext> m_convertToBackingContext;

    WebGPU::WebGPUPtr<WGPUDDMesh> m_backing;
};

}

#endif // HAVE(WEBGPU_IMPLEMENTATION)
