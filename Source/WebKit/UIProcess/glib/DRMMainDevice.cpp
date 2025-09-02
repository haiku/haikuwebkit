/*
 * Copyright (C) 2024 Igalia S.L.
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
#include "DRMMainDevice.h"

#if USE(GBM)
#include <WebCore/DRMDevice.h>
#include <WebCore/GLContext.h>
#include <WebCore/PlatformDisplay.h>
#include <epoxy/egl.h>
#include <mutex>
#include <wtf/Function.h>
#include <wtf/NeverDestroyed.h>

#if PLATFORM(GTK)
#include "Display.h"
#endif

#if PLATFORM(WPE) && ENABLE(WPE_PLATFORM)
#include "WPEUtilities.h"
#include <wpe/wpe-platform.h>
#endif

#if USE(LIBDRM)
#include <xf86drm.h>
#endif

#ifndef EGL_DRM_RENDER_NODE_FILE_EXT
#define EGL_DRM_RENDER_NODE_FILE_EXT 0x3377
#endif

namespace WebKit {

#if USE(LIBDRM)
static void drmForeachDevice(Function<bool(drmDevice*)>&& functor)
{
    std::array<drmDevicePtr, 64> devices = { };

    int numDevices = drmGetDevices2(0, devices.data(), devices.size());
    if (numDevices <= 0)
        return;

    for (int i = 0; i < numDevices; ++i) {
        if (!functor(devices[i]))
            break;
    }
    drmFreeDevices(devices.data(), numDevices);
}
#endif

static std::optional<std::pair<CString, CString>> drmFirstDeviceWithRenderNode()
{
#if USE(LIBDRM)
    std::optional<std::pair<CString, CString>> device;
    drmForeachDevice([&](drmDevice* drmDevice) {
        if (!(drmDevice->available_nodes & (1 << DRM_NODE_RENDER)))
            return true;

        device = { CString(drmDevice->nodes[DRM_NODE_PRIMARY]), CString(drmDevice->nodes[DRM_NODE_RENDER]) };
        return false;
    });
    return device;
#else
    return std::nullopt;
#endif
}

static CString drmPrimaryNodeDeviceForRenderNodeDevice(const CString& renderNode)
{
#if USE(LIBDRM)
    CString primaryNode;
    drmForeachDevice([&](drmDevice* device) {
        if (!(device->available_nodes & (1 << DRM_NODE_RENDER)))
            return true;

        auto node = CString(device->nodes[DRM_NODE_RENDER]);
        if (node == renderNode) {
            primaryNode = CString(device->nodes[DRM_NODE_PRIMARY]);
            return false;
        }

        return true;
    });
    return primaryNode;
#else
    UNUSED_PARAM(renderNode);
    return { };
#endif
}

static EGLDisplay currentEGLDisplay()
{
#if PLATFORM(GTK)
    if (auto* glDisplay = Display::singleton().glDisplay())
        return glDisplay->eglDisplay();
#endif

    auto eglDisplay = eglGetCurrentDisplay();
    if (eglDisplay != EGL_NO_DISPLAY)
        return eglDisplay;

    return eglGetDisplay(EGL_DEFAULT_DISPLAY);
}

static EGLDeviceEXT eglDisplayDevice(EGLDisplay eglDisplay)
{
    if (!WebCore::GLContext::isExtensionSupported(eglQueryString(nullptr, EGL_EXTENSIONS), "EGL_EXT_device_query"))
        return nullptr;

    EGLDeviceEXT eglDevice;
    if (eglQueryDisplayAttribEXT(eglDisplay, EGL_DEVICE_EXT, reinterpret_cast<EGLAttrib*>(&eglDevice)))
        return eglDevice;

    return nullptr;
}

static CString drmPrimaryNodeDevice()
{
    auto eglDisplay = currentEGLDisplay();
    if (eglDisplay == EGL_NO_DISPLAY)
        return { };

    EGLDeviceEXT device = eglDisplayDevice(eglDisplay);
    if (!device)
        return { };

    if (!WebCore::GLContext::isExtensionSupported(eglQueryDeviceStringEXT(device, EGL_EXTENSIONS), "EGL_EXT_device_drm"))
        return { };

    return CString(eglQueryDeviceStringEXT(device, EGL_DRM_DEVICE_FILE_EXT));
}

static CString drmRenderNodeDevice()
{
    const char* envDeviceFile = getenv("WEBKIT_WEB_RENDER_DEVICE_FILE");
    if (envDeviceFile && *envDeviceFile)
        return CString(envDeviceFile);

    auto eglDisplay = currentEGLDisplay();
    if (eglDisplay == EGL_NO_DISPLAY)
        return { };

    EGLDeviceEXT device = eglDisplayDevice(eglDisplay);
    if (!device)
        return { };

    if (!WebCore::GLContext::isExtensionSupported(eglQueryDeviceStringEXT(device, EGL_EXTENSIONS), "EGL_EXT_device_drm_render_node"))
        return { };

    return CString(eglQueryDeviceStringEXT(device, EGL_DRM_RENDER_NODE_FILE_EXT));
}

const WebCore::DRMDevice& drmMainDevice()
{
    static LazyNeverDestroyed<WebCore::DRMDevice> mainDevice;
    static std::once_flag once;
    std::call_once(once, [] {
        mainDevice.construct();

#if PLATFORM(WPE) && ENABLE(WPE_PLATFORM)
        if (WKWPE::isUsingWPEPlatformAPI()) {
            if (auto* drmDevice = wpe_display_get_drm_device(wpe_display_get_primary())) {
                mainDevice->primaryNode = wpe_drm_device_get_primary_node(drmDevice);
                mainDevice->renderNode = wpe_drm_device_get_render_node(drmDevice);
            }
            return;
        }
#endif
        mainDevice->renderNode = drmRenderNodeDevice();
        if (mainDevice->renderNode.isNull()) {
            auto primaryNode = drmPrimaryNodeDevice();
            auto renderNode = drmPrimaryNodeDeviceForRenderNodeDevice(primaryNode);
            if (!renderNode.isNull()) {
                mainDevice->primaryNode = WTFMove(primaryNode);
                mainDevice->renderNode = WTFMove(renderNode);
            } else if (auto device = drmFirstDeviceWithRenderNode()) {
                mainDevice->primaryNode = WTFMove(device->first);
                mainDevice->renderNode = WTFMove(device->second);
            }
        } else
            mainDevice->primaryNode = drmPrimaryNodeDeviceForRenderNodeDevice(mainDevice->renderNode);
    });
    return mainDevice.get();
}

} // namespace WebKit

#endif // USE(GBM)
