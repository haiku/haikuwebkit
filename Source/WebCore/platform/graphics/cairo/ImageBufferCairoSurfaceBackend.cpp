/*
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2007 Holger Hans Peter Freyther <zecke@selfish.org>
 * Copyright (C) 2008, 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) 2010 Torch Mobile (Beijing) Co. Ltd. All rights reserved.
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#include "config.h"
#include "ImageBufferCairoSurfaceBackend.h"

#include "BitmapImage.h"
#include "CairoOperations.h"
#include "CairoUtilities.h"
#include "Color.h"
#include "GraphicsContext.h"
#include "ImageBufferUtilitiesCairo.h"
#include "PixelBuffer.h"
#include <cairo.h>

#if USE(CAIRO)

namespace WebCore {

ImageBufferCairoSurfaceBackend::ImageBufferCairoSurfaceBackend(const Parameters& parameters, RefPtr<cairo_surface_t>&& surface)
    : ImageBufferCairoBackend(parameters)
    , m_surface(WTFMove(surface))
    , m_context(m_surface.get())
{
    ASSERT(cairo_surface_status(m_surface.get()) == CAIRO_STATUS_SUCCESS);
    m_context.applyDeviceScaleFactor(parameters.resolutionScale);
}

GraphicsContext& ImageBufferCairoSurfaceBackend::context()
{
    return m_context;
}

unsigned ImageBufferCairoSurfaceBackend::bytesPerRow() const
{
    return cairo_image_surface_get_stride(m_surface.get());
}

RefPtr<NativeImage> ImageBufferCairoSurfaceBackend::copyNativeImage()
{
    auto copy = adoptRef(cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
    cairo_image_surface_get_width(m_surface.get()),
    cairo_image_surface_get_height(m_surface.get())));

    auto cr = adoptRef(cairo_create(copy.get()));
    cairo_set_operator(cr.get(), CAIRO_OPERATOR_SOURCE);
    cairo_set_source_surface(cr.get(), m_surface.get(), 0, 0);
    cairo_paint(cr.get());

    return NativeImage::create(WTFMove(copy));
}

RefPtr<NativeImage> ImageBufferCairoSurfaceBackend::createNativeImageReference()
{
    return NativeImage::create(RefPtr { m_surface.get() });
}

bool ImageBufferCairoSurfaceBackend::canMapBackingStore() const
{
    return true;
}

RefPtr<cairo_surface_t> ImageBufferCairoSurfaceBackend::createCairoSurface()
{
    return RefPtr { m_surface.get() };
}

RefPtr<NativeImage> ImageBufferCairoSurfaceBackend::cairoSurfaceCoerceToImage()
{
    if (cairo_surface_get_type(m_surface.get()) == CAIRO_SURFACE_TYPE_IMAGE && cairo_surface_get_content(m_surface.get()) == CAIRO_CONTENT_COLOR_ALPHA)
        return createNativeImageReference();
    return copyNativeImage();
}

void ImageBufferCairoSurfaceBackend::getPixelBuffer(const IntRect& srcRect, PixelBuffer& destination)
{
    ImageBufferBackend::getPixelBuffer(srcRect, span(m_surface.get()), destination);
}

void ImageBufferCairoSurfaceBackend::putPixelBuffer(const PixelBufferSourceView& pixelBuffer, const IntRect& srcRect, const IntPoint& destPoint, AlphaPremultiplication destFormat)
{
    ImageBufferBackend::putPixelBuffer(pixelBuffer, srcRect, destPoint, destFormat, mutableSpan(m_surface.get()));

    cairo_surface_mark_dirty_rectangle(m_surface.get(), destPoint.x(), destPoint.y(), srcRect.width(), srcRect.height());
}

String ImageBufferCairoSurfaceBackend::debugDescription() const
{
    TextStream stream;
    stream << "ImageBufferCairoSurfaceBackend " << this << " " << m_surface.get();
    return stream.release();
}

} // namespace WebCore

#endif // USE(CAIRO)
