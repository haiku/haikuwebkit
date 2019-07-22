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

std::unique_ptr<GraphicsContext> ShareableBitmap::createGraphicsContext()
{
    PlatformImagePtr image = createPlatformImage();

    BView* surface = new BView(image->Bounds(), "Shareable", 0, 0);
    image->AddChild(surface);
    surface->LockLooper();

    return std::make_unique<GraphicsContextHaiku>(surface);
}

void ShareableBitmap::paint(GraphicsContext& context, const IntPoint& dstPoint, const IntRect& srcRect)
{
    BBitmap bitmap(BRect(B_ORIGIN, size()), B_RGBA32, true);
    bitmap.ImportBits(data(), size().width() * size().height() * 4, 4 * size().width(), 0, B_RGBA32);

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

WebCore::PlatformImagePtr ShareableBitmap::createPlatformImage(WebCore::BackingStoreCopy, WebCore::ShouldInterpolate)
{
    return WebCore::PlatformImagePtr(new BitmapRef(m_sharedMemory->area(), 0, bounds(), 0, /*m_configuration.platformColorSpace()*/ B_RGBA32, bytesPerRow()));
}

RefPtr<Image> ShareableBitmap::createImage()
{
    WebCore::PlatformImagePtr surface = createPlatformImage();
    if (!surface)
        return nullptr;

    return BitmapImage::create(std::move(surface));
}

Checked<unsigned, RecordOverflow> ShareableBitmapConfiguration::calculateBytesPerRow(const WebCore::IntSize& size,
    const WebCore::DestinationColorSpace& config)
{
    return 4 * size.width();
}

Checked<unsigned int, RecordOverflow> ShareableBitmapConfiguration::calculateBytesPerPixel(const WebCore::DestinationColorSpace&)
{
    return 4;
}

std::optional<WebCore::DestinationColorSpace> WebKit::ShareableBitmapConfiguration::validateColorSpace(std::optional<WebCore::DestinationColorSpace> space)
{
	return space;
}

}
