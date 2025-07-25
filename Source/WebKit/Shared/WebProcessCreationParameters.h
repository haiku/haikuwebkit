/*
 * Copyright (C) 2010-2020 Apple Inc. All rights reserved.
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

#include "APIData.h"
#include "AccessibilityPreferences.h"
#include "AuxiliaryProcessCreationParameters.h"
#include "CacheModel.h"
#include "SandboxExtension.h"
#include "ScriptTrackingPrivacyFilter.h"
#include "TextCheckerState.h"
#include "UserData.h"

#include "WebProcessDataStoreParameters.h"
#include <WebCore/CrossOriginMode.h>
#include <wtf/HashMap.h>
#include <wtf/Markable.h>
#include <wtf/OptionSet.h>
#include <wtf/ProcessID.h>
#include <wtf/RetainPtr.h>
#include <wtf/Vector.h>
#include <wtf/text/StringHash.h>
#include <wtf/text/WTFString.h>

#if PLATFORM(COCOA) || PLATFORM(GTK) || (PLATFORM(WPE) && ENABLE(WPE_PLATFORM))
#include <WebCore/ScreenProperties.h>
#endif

#if PLATFORM(COCOA)
#include <WebCore/PlatformScreen.h>
#include <wtf/MachSendRight.h>
#endif

#if PLATFORM(IOS_FAMILY)
#include <WebCore/RenderThemeIOS.h>
#include <pal/system/ios/UserInterfaceIdiom.h>
#endif

#if PLATFORM(GTK) || PLATFORM(WPE)
#include "AvailableInputDevices.h"
#include "RendererBufferTransportMode.h"
#include <WebCore/SystemSettings.h>
#include <wtf/MemoryPressureHandler.h>
#endif

namespace API {
class Data;
}

namespace WebKit {

struct WebProcessCreationParameters {
    AuxiliaryProcessCreationParameters auxiliaryProcessParameters;
    String injectedBundlePath;
    SandboxExtension::Handle injectedBundlePathExtensionHandle;
    Vector<SandboxExtension::Handle> additionalSandboxExtensionHandles;

    UserData initializationUserData;

#if PLATFORM(COCOA) && ENABLE(REMOTE_INSPECTOR)
    Vector<SandboxExtension::Handle> enableRemoteWebInspectorExtensionHandles;
#endif

    Vector<String> urlSchemesRegisteredAsEmptyDocument;
    Vector<String> urlSchemesRegisteredAsSecure;
    Vector<String> urlSchemesRegisteredAsBypassingContentSecurityPolicy;
    Vector<String> urlSchemesForWhichDomainRelaxationIsForbidden;
    Vector<String> urlSchemesRegisteredAsLocal;
#if ENABLE(ALL_LEGACY_REGISTERED_SPECIAL_URL_SCHEMES)
    Vector<String> urlSchemesRegisteredAsNoAccess;
#endif
    Vector<String> urlSchemesRegisteredAsDisplayIsolated;
    Vector<String> urlSchemesRegisteredAsCORSEnabled;
    Vector<String> urlSchemesRegisteredAsAlwaysRevalidated;
    Vector<String> urlSchemesRegisteredAsCachePartitioned;
    Vector<String> urlSchemesRegisteredAsCanDisplayOnlyIfCanRequest;

#if ENABLE(WK_WEB_EXTENSIONS)
    Vector<String> urlSchemesRegisteredAsWebExtensions;
#endif

    Vector<String> fontAllowList;
    Vector<String> overrideLanguages;
#if USE(GSTREAMER)
    Vector<String> gstreamerOptions;
#endif

    CacheModel cacheModel;

    Markable<double> defaultRequestTimeoutInterval;
    unsigned backForwardCacheCapacity { 0 };

    bool shouldAlwaysUseComplexTextCodePath { false };
    bool shouldEnableMemoryPressureReliefLogging { false };
    bool shouldSuppressMemoryPressureHandler { false };
    bool disableFontSubpixelAntialiasingForTesting { false };
    bool fullKeyboardAccessEnabled { false };
#if HAVE(MOUSE_DEVICE_OBSERVATION)
    bool hasMouseDevice { false };
#endif
#if HAVE(STYLUS_DEVICE_OBSERVATION)
    bool hasStylusDevice { false };
#endif
    bool memoryCacheDisabled { false };
    bool attrStyleEnabled { false };
    bool shouldThrowExceptionForGlobalConstantRedeclaration { true };
    WebCore::CrossOriginMode crossOriginMode { WebCore::CrossOriginMode::Shared }; // Cross-origin isolation via COOP+COEP headers.

#if ENABLE(SERVICE_CONTROLS)
    bool hasImageServices { false };
    bool hasSelectionServices { false };
    bool hasRichContentServices { false };
#endif

    OptionSet<TextCheckerState> textCheckerState;

#if PLATFORM(COCOA)
    String uiProcessBundleIdentifier;
    int latencyQOS { 0 };
    int throughputQOS { 0 };
#endif

    ProcessID presentingApplicationPID { 0 };

#if PLATFORM(COCOA)
    String uiProcessBundleResourcePath;
    SandboxExtension::Handle uiProcessBundleResourcePathExtensionHandle;

    bool shouldEnableJIT { false };
    bool shouldEnableFTLJIT { false };
    bool accessibilityEnhancedUserInterfaceEnabled { false };
    
    RefPtr<API::Data> bundleParameterData;
#endif // PLATFORM(COCOA)

#if ENABLE(NOTIFICATIONS)
    HashMap<String, bool> notificationPermissions;
#endif

#if PLATFORM(COCOA)
    RetainPtr<CFDataRef> networkATSContext;
#endif

#if PLATFORM(WAYLAND)
    String waylandCompositorDisplayName;
#endif

#if PLATFORM(COCOA)
    Vector<String> mediaMIMETypes;
#endif

#if PLATFORM(COCOA) || PLATFORM(GTK) || (PLATFORM(WPE) && ENABLE(WPE_PLATFORM))
    WebCore::ScreenProperties screenProperties;
#endif

#if !RELEASE_LOG_DISABLED
    bool shouldLogUserInteraction { false };
#endif

#if PLATFORM(MAC)
    bool useOverlayScrollbars { true };
#endif

#if USE(WPE_RENDERER)
    bool isServiceWorkerProcess { false };
    UnixFileDescriptor hostClientFileDescriptor;
    CString implementationLibraryName;
#endif

    std::optional<WebProcessDataStoreParameters> websiteDataStoreParameters;

    std::optional<SandboxExtension::Handle> mobileGestaltExtensionHandle;
    std::optional<SandboxExtension::Handle> launchServicesExtensionHandle;
#if HAVE(VIDEO_RESTRICTED_DECODING)
#if PLATFORM(MAC) || PLATFORM(MACCATALYST)
    SandboxExtension::Handle trustdExtensionHandle;
#endif
    bool enableDecodingHEIC { false };
    bool enableDecodingAVIF { false };
#endif

#if PLATFORM(VISION)
    // FIXME: Remove when GPU Process is fully enabled.
    Vector<SandboxExtension::Handle> metalCacheDirectoryExtensionHandles;
#endif

#if PLATFORM(COCOA)
    bool systemHasBattery { false };
    bool systemHasAC { false };
#endif

#if PLATFORM(IOS_FAMILY)
    PAL::UserInterfaceIdiom currentUserInterfaceIdiom { PAL::UserInterfaceIdiom::Default };
    bool supportsPictureInPicture { false };
    WebCore::RenderThemeIOS::CSSValueToSystemColorMap cssValueToSystemColorMap;
    WebCore::Color focusRingColor;
    String localizedDeviceModel;
    String contentSizeCategory;
#endif

#if USE(GBM)
    String renderDeviceFile;
#endif

#if PLATFORM(GTK) || PLATFORM(WPE)
    OptionSet<RendererBufferTransportMode> rendererBufferTransportMode;
    WebCore::SystemSettings::State systemSettings;
    std::optional<MemoryPressureHandler::Configuration> memoryPressureHandlerConfiguration;
    bool disableFontHintingForTesting { false };
    OptionSet<AvailableInputDevices> availableInputDevices;
#endif

#if PLATFORM(GTK)
    bool useSystemAppearanceForScrollbars { false };
#endif

#if HAVE(CATALYST_USER_INTERFACE_IDIOM_AND_SCALE_FACTOR)
    std::pair<int64_t, double> overrideUserInterfaceIdiomAndScale;
#endif

#if HAVE(IOSURFACE)
    WebCore::IntSize maximumIOSurfaceSize;
    uint64_t bytesPerRowIOSurfaceAlignment;
#endif
    
    AccessibilityPreferences accessibilityPreferences;
#if PLATFORM(IOS_FAMILY)
    bool applicationAccessibilityEnabled { false };
#endif

#if USE(GLIB)
    String applicationID;
    String applicationName;
#if ENABLE(REMOTE_INSPECTOR)
    CString inspectorServerAddress;
#endif
#endif

#if USE(ATSPI)
    String accessibilityBusAddress;
    String accessibilityBusName;
#endif

    String timeZoneOverride;

    HashMap<WebCore::RegistrableDomain, String> storageAccessUserAgentStringQuirksData;
    HashSet<WebCore::RegistrableDomain> storageAccessPromptQuirksDomains;
    ScriptTrackingPrivacyRules scriptTrackingPrivacyRules;

    Seconds memoryFootprintPollIntervalForTesting;
    Vector<uint64_t> memoryFootprintNotificationThresholds;

#if ENABLE(NOTIFY_BLOCKING)
    Vector<std::pair<String, uint64_t>> notifyState;
#endif

#if ENABLE(INITIALIZE_ACCESSIBILITY_ON_DEMAND)
    bool shouldInitializeAccessibility { false };
#endif

#if HAVE(LIQUID_GLASS)
    bool isLiquidGlassEnabled { false };
#endif
};

} // namespace WebKit
