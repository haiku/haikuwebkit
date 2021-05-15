/*
 * Copyright (C) 2014 Haiku, Inc.
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
#include "ShareableBitmap.h"

#include <WebCore/BitmapImage.h>
#include <WebCore/GraphicsContext.h>
#include <WebCore/GraphicsContextHaiku.h>
#include <WebCore/IntRect.h>
#include <WebCore/NotImplemented.h>

#include <Bitmap.h>
#include <View.h>

using namespace WebCore;

namespace WebKit {

void ShareableBitmap::validateConfiguration(Configuration&)
{
}

static inline PlatformImagePtr createSurfaceFromBitmap(BitmapRef* bitmap)
{
    PlatformImagePtr image(bitmap);
    return image;
}

BitmapRef* ShareableBitmap::bitmapObject() const
{
    if(isBackedBySharedMemory()) {
        fprintf(stderr, "FIXME ShareableBitmap backed by shared memory\n");
        // FIXME create bitmap with content in m_sharedMemory
        return NULL;
    }

    return NULL;
}

std::unique_ptr<GraphicsContext> ShareableBitmap::createGraphicsContext()
{
    PlatformImagePtr image = createPlatformImage();

    BView* surface = new BView(image->Bounds(),
        "Shareable", 0, 0);
    image->AddChild(surface);
    surface->LockLooper();

    return std::make_unique<GraphicsContextHaiku>(surface);
}

void ShareableBitmap::paint(GraphicsContext& context, const IntPoint& dstPoint, const IntRect& srcRect)
{
    BBitmap bitmap(BRect(B_ORIGIN,m_size),B_RGBA32,true);
    status_t result = bitmap.ImportBits(data(),m_size.width()*m_size.height()*4, 4*m_size.width(),0,B_RGBA32);
    
    BView* viewSurface = context.platformContext();
    
    viewSurface->LockLooper();
    viewSurface->DrawBitmap(&bitmap);
    viewSurface->Sync();
    viewSurface->UnlockLooper();
}

void ShareableBitmap::paint(GraphicsContext& context, float scaleFactor, const IntPoint& dstPoint, const IntRect& srcRect)
{
    FloatRect destRect(dstPoint, srcRect.size());
    FloatRect srcRectScaled(srcRect);
    srcRectScaled.scale(scaleFactor);
    notImplemented();
}

WebCore::PlatformImagePtr ShareableBitmap::createPlatformImage()
{
    WebCore::PlatformImagePtr image = createSurfaceFromBitmap(bitmapObject());

    ref(); // Balanced by deref in releaseSurfaceData.
    return image;
}

RefPtr<Image> ShareableBitmap::createImage()
{
    WebCore::PlatformImagePtr surface = createPlatformImage();
    if (!surface)
        return 0;

    return BitmapImage::create(std::move(surface));
}

Checked<unsigned, RecordOverflow> ShareableBitmap::calculateBytesPerRow(WebCore::IntSize size, const ShareableBitmap::Configuration& config)
{
    return calculateBytesPerPixel(config) * size.width();
}

Checked<unsigned int, RecordOverflow> ShareableBitmap::calculateBytesPerPixel(const Configuration&)
{
    return 4;
}
}
