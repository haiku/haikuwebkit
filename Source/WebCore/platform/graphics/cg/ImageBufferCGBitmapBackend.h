/*
 * Copyright (C) 2020-2024 Apple Inc. All rights reserved.
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

#if USE(CG)

#include "ImageBuffer.h"
#include "ImageBufferCGBackend.h"
#include <wtf/TZoneMalloc.h>

namespace WebCore {

class ImageBufferCGBitmapBackend final : public ImageBufferCGBackend {
    WTF_MAKE_TZONE_OR_ISO_ALLOCATED(ImageBufferCGBitmapBackend);
    WTF_MAKE_NONCOPYABLE(ImageBufferCGBitmapBackend);
public:
    ~ImageBufferCGBitmapBackend();

    static size_t calculateMemoryCost(const Parameters&);

    static std::unique_ptr<ImageBufferCGBitmapBackend> create(const Parameters&, const ImageBufferCreationContext&);
    bool canMapBackingStore() const final;
    GraphicsContext& context() final;

private:
    ImageBufferCGBitmapBackend(const Parameters&, std::span<uint8_t> data, RetainPtr<CGDataProviderRef>&&, std::unique_ptr<GraphicsContextCG>&&);

    unsigned bytesPerRow() const final;

    RefPtr<NativeImage> copyNativeImage() final;
    RefPtr<NativeImage> createNativeImageReference() final;

    void getPixelBuffer(const IntRect&, PixelBuffer&) final;
    void putPixelBuffer(const PixelBufferSourceView&, const IntRect& srcRect, const IntPoint& destPoint, AlphaPremultiplication destFormat) final;

    std::span<uint8_t> m_data;
    RetainPtr<CGDataProviderRef> m_dataProvider;
};

} // namespace WebCore

#endif // USE(CG)
