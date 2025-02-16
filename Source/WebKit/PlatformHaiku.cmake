include(Headers.cmake)
include(Platform/Curl.cmake)

file(MAKE_DIRECTORY ${DERIVED_SOURCES_HAIKU_API_DIR})

configure_file(UIProcess/API/haiku/WebKitVersion.h.in ${DERIVED_SOURCES_HAIKU_API_DIR}/WebKitVersion.h)

list(APPEND WebKit_SOURCES
    NetworkProcess/haiku/NetworkProcessMainHaiku.cpp

    NetworkProcess/Cookies/curl/WebCookieManagerCurl.cpp

    NetworkProcess/cache/NetworkCacheDataCurl.cpp
    NetworkProcess/cache/NetworkCacheIOChannelCurl.cpp

    NetworkProcess/curl/NetworkDataTaskCurl.cpp
    NetworkProcess/NetworkDataTaskDataURL.cpp
    NetworkProcess/curl/NetworkProcessCurl.cpp
    #NetworkProcess/curl/NetworkProcessMainCurl.cpp
    NetworkProcess/curl/NetworkSessionCurl.cpp
    NetworkProcess/curl/WebSocketTaskCurl.cpp

    NetworkProcess/Classifier/WebResourceLoadStatisticsStore.cpp

    Platform/IPC/haiku/ConnectionHaiku.cpp
    Platform/IPC/haiku/IPCSemaphoreHaiku.cpp

    Platform/classifier/ResourceLoadStatisticsClassifier.cpp

    Platform/haiku/ModuleHaiku.cpp

    Platform/unix/LoggingUnix.cpp

    Shared/haiku/ProcessExecutablePathHaiku.cpp
    Shared/haiku/WebMemorySamplerHaiku.cpp
    Shared/unix/AuxiliaryProcessMain.cpp

    Shared/API/c/curl/WKCertificateInfoCurl.cpp

    UIProcess/API/C/curl/WKProtectionSpaceCurl.cpp
    UIProcess/API/C/curl/WKWebsiteDataStoreRefCurl.cpp

    UIProcess/API/C/haiku/WKView.cpp

    UIProcess/DefaultUndoController.cpp

    UIProcess/WebsiteData/curl/WebsiteDataStoreCurl.cpp
    UIProcess/WebsiteData/haiku/WebsiteDataStoreHaiku.cpp

    UIProcess/Launcher/haiku/ProcessLauncherHaiku.cpp
    UIProcess/LegacySessionStateCodingNone.cpp
    UIProcess/haiku/BackingStoreHaiku.cpp
    UIProcess/haiku/TextCheckerHaiku.cpp
    UIProcess/haiku/WebPageProxyHaiku.cpp
    UIProcess/haiku/WebProcessPoolHaiku.cpp
    UIProcess/API/haiku/WebViewBase.cpp
    UIProcess/API/haiku/WebView.cpp
    UIProcess/API/haiku/PageClientImplHaiku.cpp

    WebProcess/InjectedBundle/haiku/InjectedBundleHaiku.cpp
    WebProcess/InjectedBundle/haiku/InjectedBundleHaiku.cpp

    WebProcess/WebPage/haiku/WebInspectorHaiku.cpp
    WebProcess/WebPage/haiku/WebPageHaiku.cpp
    WebProcess/WebPage/AcceleratedSurface.cpp

    WebProcess/haiku/WebProcessHaiku.cpp
    WebProcess/haiku/WebProcessMainHaiku.cpp

    #WebProcess/WebCoreSupport/haiku/WebFrameNetworkingContext.cpp
    WebProcess/WebCoreSupport/curl/WebFrameNetworkingContext.cpp
)

if (USE_COORDINATED_GRAPHICS)
    list(APPEND WebKit_SOURCES
        WebProcess/WebPage/CoordinatedGraphics/CompositingRunLoop.cpp
        WebProcess/WebPage/CoordinatedGraphics/CoordinatedSceneState.cpp
        WebProcess/WebPage/CoordinatedGraphics/LayerTreeHost.cpp
        WebProcess/WebPage/CoordinatedGraphics/ThreadedCompositor.cpp
        WebProcess/WebPage/CoordinatedGraphics/ThreadedDisplayRefreshMonitor.cpp
    )
endif ()

if (USE_TEXTURE_MAPPER)
    list(APPEND WebKit_SOURCES
        UIProcess/CoordinatedGraphics/DrawingAreaProxyCoordinatedGraphics.cpp
        WebProcess/WebPage/CoordinatedGraphics/DrawingAreaCoordinatedGraphics.cpp
    )
endif ()

# TODO: It seems not all of these headers should be public. Currently, if a
# program such as MiniBrowser links against WebKit, all of these directories
# will be added to the include path of that program.
# See also https://github.com/webkit/webkit/commit/8da564110578
list(APPEND WebKit_INCLUDE_DIRECTORIES
    "${DERIVED_SOURCES_HAIKU_API_DIR}"
    "${WEBKIT_DIR}/NetworkProcess/curl"
    "${WEBKIT_DIR}/Platform"
    "${WEBKIT_DIR}/Platform/classifier"
    "${WEBKIT_DIR}/Platform/generic"
    "${WEBKIT_DIR}/Platform/IPC"
    "${WEBKIT_DIR}/Platform/IPC/haiku"
    "${WEBKIT_DIR}/Platform/IPC/unix"
    "${WEBKIT_DIR}/Shared"
    "${WEBKIT_DIR}/Shared/API"
    "${WEBKIT_DIR}/Shared/API/c"
    "${WEBKIT_DIR}/Shared/API/c/haiku"
    "${WEBKIT_DIR}/Shared/CoordinatedGraphics"
    "${WEBKIT_DIR}/Shared/CoordinatedGraphics/threadedcompositor"
    "${WEBKIT_DIR}/Shared/unix"
    "${WEBKIT_DIR}/Shared/haiku"
    "${WEBKIT_DIR}/UIProcess/API/C"
    "${WEBKIT_DIR}/UIProcess/API/C/CoordinatedGraphics"
    "${WEBKIT_DIR}/UIProcess/API/C/curl"
    "${WEBKIT_DIR}/UIProcess/API/C/haiku"
    "${WEBKIT_DIR}/UIProcess/API/haiku"
    "${WEBKIT_DIR}/UIProcess/haiku"
    "${WEBKIT_DIR}/UIProcess/CoordinatedGraphics"
    "${WEBKIT_DIR}/WebProcess/InjectedBundle"
    "${WEBKIT_DIR}/WebProcess/InjectedBundle/API/c"
    "${WEBKIT_DIR}/WebProcess/unix"
    "${WEBKIT_DIR}/WebProcess/WebCoreSupport/curl"
    "${WEBKIT_DIR}/WebProcess/WebCoreSupport/haiku"
    "${WEBKIT_DIR}/WebProcess/WebPage/CoordinatedGraphics"
    "${WEBKIT_DIR}/NetworkProcess/haiku"
    ${LIBXML2_INCLUDE_DIR}
    ${LIBXSLT_INCLUDE_DIRS}
    ${SQLITE_INCLUDE_DIRS}
    "${DERIVED_SOURCES_WEBCORE_DIR}"
    "${DERIVED_SOURCES_WEBKIT_DIR}"
    "${WebKit_FRAMEWORK_HEADERS_DIR}"
)

list(APPEND WebKit_LIBRARIES
    ${CMAKE_DL_LIBS}
    ${FONTCONFIG_LIBRARIES}
    ${FREETYPE2_LIBRARIES}
    ${JPEG_LIBRARIES}
    ${LIBXML2_LIBRARIES}
    ${OPENGL_LIBRARIES}
    ${PNG_LIBRARIES}
    ${SQLITE_LIBRARIES}
    -Wl,--whole-archive WTF -Wl,--no-whole-archive
)

set(WebKitCommonIncludeDirectories ${WebKit_PRIVATE_INCLUDE_DIRECTORIES})
set(WebKitCommonSystemIncludeDirectories ${WebKit_SYSTEM_INCLUDE_DIRECTORIES})

list(APPEND WebProcess_SOURCES
    WebProcess/EntryPoint/unix/WebProcessMain.cpp
)

list(APPEND NetworkProcess_SOURCES
    NetworkProcess/EntryPoint/unix/NetworkProcessMain.cpp
)

list(APPEND WebProcess_LIBRARIES
    ${LIBXML2_LIBRARIES}
    ${LIBXSLT_LIBRARIES}
    ${OPENGL_LIBRARIES}
    ${SQLITE_LIBRARIES}
)

add_definitions(-DWEBKIT2_COMPILATION)

add_custom_target(forwarding-headerHaiku
    COMMAND ${PERL_EXECUTABLE} ${WEBKIT_DIR}/Scripts/generate-forwarding-headers.pl ${WEBKIT_DIR} ${DERIVED_SOURCES_WEBKIT_DIR}/include haiku
    COMMAND ${PERL_EXECUTABLE} ${WEBKIT_DIR}/Scripts/generate-forwarding-headers.pl ${WEBKIT_DIR} ${DERIVED_SOURCES_WEBKIT_DIR}/include CoordinatedGraphics
)

set(WEBKIT_EXTRA_DEPENDENCIES
    forwarding-headerHaiku
)

add_definitions(
    -DLIBEXECDIR=\"${EXEC_INSTALL_DIR}\"
    -DWEBPROCESSNAME=\"WebProcess\"
    -DPLUGINPROCESSNAME=\"PluginProcess\"
    -DNETWORKPROCESSNAME=\"NetworkProcess\"
)

set(WebKit_FORWARDING_HEADERS_DIRECTORIES
   Shared/API/c
   Shared/API/c/haiku

   UIProcess/API/C
   UIProcess/API/cpp

   Platform/IPC/unix
   WebProcess/InjectedBundle/API/c
)

list(APPEND WebKit_PUBLIC_FRAMEWORK_HEADERS
    Shared/API/c/haiku/WKBaseHaiku.h
)

list(APPEND WebKit_SERIALIZATION_IN_FILES
	Shared/haiku/WebCoreFontHaiku.serialization.in
)
