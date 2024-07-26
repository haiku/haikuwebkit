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

#include "BitmapImage.h"
#include "GraphicsContext.h"
#include "GraphicsContextHaiku.h"
#include "IntRect.h"
#include "NotImplemented.h"

#include <Bitmap.h>
#include <View.h>

namespace WebCore {

std::unique_ptr<GraphicsContext> ShareableBitmap::createGraphicsContext()
{
    PlatformImagePtr bitmap = createPlatformImage(DontCopyBackingStore, WebCore::ShouldInterpolate::No, true);

    BView* surface = new BView(bitmap->Bounds(), "Shareable", 0, 0);
    bitmap->AddChild(surface);
    surface->LockLooper();

    return makeUnique<GraphicsContextHaiku>(surface, bitmap);
}

void ShareableBitmap::paint(GraphicsContext& context, const IntPoint& dstPoint, const IntRect& srcRect)
{
    PlatformImagePtr bitmap = createPlatformImage(DontCopyBackingStore);

    BView* viewSurface = context.platformContext();

    viewSurface->DrawBitmap(bitmap.get());
    viewSurface->Sync();
}

void ShareableBitmap::paint(GraphicsContext& context, float scaleFactor, const IntPoint& dstPoint, const IntRect& srcRect)
{
    FloatRect destRect(dstPoint, srcRect.size());
    FloatRect srcRectScaled(srcRect);
    srcRectScaled.scale(scaleFactor);
    notImplemented();
}

WebCore::PlatformImagePtr ShareableBitmap::createPlatformImage(WebCore::BackingStoreCopy, WebCore::ShouldInterpolate, bool allowAttachingViews)
{
    // Creates a BBitmap (actually BitmapRef) that reads image data from the
    // address given by data(). This address is in shared memory. The idea is
    // that multiple processes can point to the same underlying bitmap data.
    // One can draw, and the other can display.

    status_t status;

    // Get area id of shared memory
    const void* address = span().data();
    area_id area = area_for((void*)address);
    ASSERT(area >= B_OK);

    // Get offset in area to put our BBitmap
    area_info areaInfo;
    status = get_area_info(area, &areaInfo);
    ASSERT(status == B_OK);
    ptrdiff_t offset = (ptrdiff_t)address - (ptrdiff_t)areaInfo.address;

#if USE(UNIX_DOMAIN_SOCKETS)
    // We are on UNIX's implementation of shared memory. UNIX's shared memory
    // doesn't have B_CLONEABLE_AREA by default. We need it enabled so that the
    // app server can clone it and manipulate the bitmap.
    status = set_area_protection(area, B_READ_AREA | B_WRITE_AREA | B_CLONEABLE_AREA);
    ASSERT(status == B_OK);
#endif

    // Create the BBitmap
    WebCore::PlatformImagePtr image = adoptRef(new BitmapRef(
        area, offset, bounds(), allowAttachingViews ? B_BITMAP_ACCEPTS_VIEWS : 0,
        /*m_configuration.platformColorSpace()*/ B_RGBA32, bytesPerRow()));
    ASSERT(image->InitCheck() == B_OK);

    // The bitmap needs to hold a reference to our shared memory.
    ref();
    image->onDestroy = [this](){
        this->deref();
    };

    return image;
}

RefPtr<Image> ShareableBitmap::createImage()
{
    WebCore::PlatformImagePtr surface = createPlatformImage();
    if (!surface)
        return nullptr;

    return BitmapImage::create(std::move(surface));
}

CheckedUint32 ShareableBitmapConfiguration::calculateBitsPerComponent(const DestinationColorSpace& colorSpace)
{
    return (calculateBytesPerPixel(colorSpace) / 4) * 8;
}

CheckedUint32 ShareableBitmapConfiguration::calculateBytesPerRow(const WebCore::IntSize& size,
    const WebCore::DestinationColorSpace& config)
{
    return 4 * size.width();
}

CheckedUint32 ShareableBitmapConfiguration::calculateBytesPerPixel(const WebCore::DestinationColorSpace&)
{
    return 4;
}

std::optional<WebCore::DestinationColorSpace> ShareableBitmapConfiguration::validateColorSpace(std::optional<WebCore::DestinationColorSpace> space)
{
    return space;
}

void ShareableBitmap::setOwnershipOfMemory(const ProcessIdentity&)
{
}

}
