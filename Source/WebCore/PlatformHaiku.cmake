include(platform/Haiku.cmake)
include(platform/ImageDecoders.cmake)
include(platform/OpenSSL.cmake)

add_definitions(-D_DEFAULT_SOURCE)

if(USE_CURL)
  include(platform/Curl.cmake)
  list(APPEND WebCore_SOURCES
    platform/network/haiku/CurlSSLHandleHaiku.cpp
    platform/network/curl/CurlCacheEntry.cpp
    platform/network/curl/CurlCacheManager.cpp
  )
else()
  list(APPEND WebCore_INCLUDE_DIRECTORIES
    "${WEBCORE_DIR}/platform/network/haiku"
  )

  list(APPEND WebCore_SOURCES
    platform/network/haiku/BUrlProtocolHandler.cpp
    platform/network/haiku/CertificateInfo.cpp
    platform/network/haiku/CookieJarHaiku.cpp
    platform/network/haiku/DNSHaiku.cpp
    platform/network/haiku/HaikuFormDataStream.cpp
    platform/network/haiku/ResourceHandleHaiku.cpp
    platform/network/haiku/ResourceRequestHaiku.cpp

    platform/network/haiku/CredentialStorageHaiku.cpp
    platform/network/haiku/NetworkStorageSessionHaiku.cpp
  )

  list(APPEND WebCore_LIBRARIES netservices shared)
  list(APPEND WebCore_PRIVATE_FRAMEWORK_HEADERS
    platform/network/haiku/AuthenticationChallenge.h
    platform/network/haiku/CertificateInfo.h
    platform/network/haiku/HaikuFormDataStream.h
    platform/network/haiku/ResourceError.h
    platform/network/haiku/ResourceRequest.h
    platform/network/haiku/ResourceResponse.h
  )

endif()

list(APPEND WebCore_PRIVATE_INCLUDE_DIRECTORIES
    ${WEBCORE_DIR}/platform/video-codecs
)

list(APPEND WebCore_INCLUDE_DIRECTORIES
  "${THIRDPARTY_DIR}/ANGLE/"
  "${THIRDPARTY_DIR}/ANGLE/include/KHR"
  "${WEBCORE_DIR}/page/scrolling/coordinatedgraphics"
  "${WEBCORE_DIR}/platform/haiku"
  "${WEBCORE_DIR}/platform/graphics/egl"
  "${WEBCORE_DIR}/platform/graphics/opengl"
  "${WEBCORE_DIR}/platform/graphics/opentype"
  "${WEBCORE_DIR}/platform/graphics/texmap/coordinated"
  "${WEBCORE_DIR}/platform/mediacapabilities"
  "${FORWARDING_HEADERS_DIR}/JavaScriptCore"
  "${CMAKE_SOURCE_DIR}/Source"
)

list(APPEND WebCore_SOURCES
  accessibility/playstation/AXObjectCachePlayStation.cpp
  accessibility/playstation/AccessibilityObjectPlayStation.cpp

  editing/haiku/EditorHaiku.cpp

  platform/Cursor.cpp

  page/haiku/DragControllerHaiku.cpp
  page/haiku/EventHandlerHaiku.cpp

  page/PointerLockController.cpp

  platform/audio/FFTFrameStub.cpp

  platform/haiku/CursorHaiku.cpp
  platform/haiku/DragImageHaiku.cpp
  platform/haiku/DragDataHaiku.cpp
  platform/haiku/ErrorsHaiku.cpp
  platform/haiku/FileSystemHaiku.cpp
  platform/haiku/KeyedDecoderHaiku.cpp
  platform/haiku/KeyedEncoderHaiku.cpp
  platform/haiku/KURLHaiku.cpp
  platform/haiku/LoggingHaiku.cpp
  platform/haiku/MIMETypeRegistryHaiku.cpp
  platform/haiku/MainThreadSharedTimerHaiku.cpp
  platform/haiku/PasteboardHaiku.cpp
  platform/haiku/PlatformKeyboardEventHaiku.cpp
  platform/haiku/PlatformMouseEventHaiku.cpp
  platform/haiku/PlatformScreenHaiku.cpp
  platform/haiku/PopupMenuHaiku.cpp
  platform/haiku/ScrollbarThemeHaiku.cpp
  platform/haiku/SearchPopupMenuHaiku.cpp
  platform/haiku/SharedMemoryHaiku.cpp
  platform/haiku/SharedTimerHaiku.cpp
  platform/haiku/SoundHaiku.cpp
  platform/haiku/TemporaryLinkStubs.cpp
  platform/haiku/ThemeHaiku.cpp
  platform/haiku/UserAgentHaiku.cpp
  platform/haiku/WidgetHaiku.cpp

  platform/image-decoders/haiku/ImageDecoderHaiku.cpp

  platform/graphics/PlatformDisplay.cpp
  platform/graphics/WOFFFileFormat.cpp

  platform/graphics/haiku/AffineTransformHaiku.cpp
  platform/graphics/haiku/BitmapImageHaiku.cpp
  platform/graphics/haiku/ColorHaiku.cpp
  platform/graphics/haiku/ComplexTextControllerHaiku.cpp
  platform/graphics/haiku/FontCacheHaiku.cpp
  platform/graphics/haiku/FontCustomPlatformData.cpp
  platform/graphics/haiku/FontDescriptionHaiku.cpp
  platform/graphics/haiku/FontPlatformDataHaiku.cpp
  platform/graphics/haiku/FloatPointHaiku.cpp
  platform/graphics/haiku/FloatRectHaiku.cpp
  platform/graphics/haiku/FloatSizeHaiku.cpp
  platform/graphics/haiku/FontHaiku.cpp
  platform/graphics/haiku/FontPlatformDataHaiku.cpp
  platform/graphics/haiku/GlyphPageTreeNodeHaiku.cpp
  platform/graphics/haiku/GradientHaiku.cpp
  platform/graphics/haiku/GraphicsContextHaiku.cpp
  platform/graphics/haiku/IconHaiku.cpp
  platform/graphics/haiku/ImageBufferHaiku.cpp
  platform/graphics/haiku/ImageHaiku.cpp
  platform/graphics/haiku/IntPointHaiku.cpp
  platform/graphics/haiku/IntRectHaiku.cpp
  platform/graphics/haiku/IntSizeHaiku.cpp
  platform/graphics/haiku/MediaPlayerPrivateHaiku.cpp
  platform/graphics/haiku/NativeImageHaiku.cpp
  platform/graphics/haiku/PathHaiku.cpp
  platform/graphics/haiku/ShareableBitmapHaiku.cpp
  platform/graphics/haiku/SimpleFontDataHaiku.cpp
  platform/graphics/haiku/SystemFontDatabaseHaiku.cpp
  platform/graphics/haiku/TileHaiku.cpp
  platform/graphics/haiku/TiledBackingStoreHaiku.cpp
  platform/graphics/haiku/GraphicsLayerHaiku.cpp

  platform/network/haiku/NetworkStateNotifierHaiku.cpp

  platform/mock/GeolocationClientMock.cpp

  platform/text/Hyphenation.cpp
  platform/text/LocaleICU.cpp

  platform/text/haiku/StringHaiku.cpp

  platform/unix/LoggingUnix.cpp

  rendering/haiku/RenderThemeHaiku.cpp

  inspector/LegacyWebSocketInspectorInstrumentation.cpp
)

if (ENABLE_WEB_AUDIO)
    list(APPEND WebCore_SOURCES
		platform/audio/haiku/AudioDestinationHaiku.cpp
	)
endif ()

list(APPEND WebCore_USER_AGENT_STYLE_SHEETS
    ${WebCore_DERIVED_SOURCES_DIR}/ModernMediaControls.css
)

if (WTF_USE_COORDINATED_GRAPHICS)
    list(APPEND WebCore_SOURCES
        platform/graphics/texmap/coordinated/CoordinatedTile.cpp
    )
else()

endif()

set(WebCore_USER_AGENT_SCRIPTS
    ${WebCore_DERIVED_SOURCES_DIR}/ModernMediaControls.js
)

set(WebCore_USER_AGENT_SCRIPTS_DEPENDENCIES ${WEBCORE_DIR}/rendering/haiku/RenderThemeHaiku.cpp)

list(APPEND WebCore_LIBRARIES
  ${JPEG_LIBRARY}
  ${LIBGCRYPT_LIBRARIES}
  ${LIBTASN1_LIBRARIES}
  ${LIBXSLT_LIBRARIES}
  ${PNG_LIBRARY}
  psl
  ${WEBP_LIBRARIES}
  be bsd network bnetapi textencoding translation execinfo shared
)

list(APPEND WebCore_SYSTEM_INCLUDE_DIRECTORIES
  ${GNUTLS_INCLUDE_DIRS}
  ${ICU_INCLUDE_DIRS}
  ${LIBXML2_INCLUDE_DIR}
  ${LIBXSLT_INCLUDE_DIR}
  ${SQLITE_INCLUDE_DIR}
  ${WEBP_INCLUDE_DIRS}
  ${ZLIB_INCLUDE_DIRS}
  /system/develop/headers/private/netservices
)

if (ENABLE_WEB_AUDIO)
    #    list(APPEND WebCore_INCLUDE_DIRECTORIES
    #    "${WEBCORE_DIR}/platform/graphics/gstreamer"
    #)

    list(APPEND WebCore_LIBRARIES
        media avcodec
    )
endif ()

if (ENABLE_VIDEO)
    list(APPEND WebCore_LIBRARIES
        media
    )
endif ()

if (WTF_USE_3D_GRAPHICS)
    list(APPEND WebCore_INCLUDE_DIRECTORIES
        "${WEBCORE_DIR}/platform/graphics/opengl"
        "${WEBCORE_DIR}/platform/graphics/surfaces"
        "${WEBCORE_DIR}/platform/graphics/surfaces/glx"
        "${WEBCORE_DIR}/platform/graphics/texmap"
    )

    if (WTF_USE_EGL)
        list(APPEND WebCore_INCLUDE_DIRECTORIES
            ${EGL_INCLUDE_DIR}
            "${WEBCORE_DIR}/platform/graphics/surfaces/egl"
    )
    endif ()

    list(APPEND WebCore_SOURCES
        platform/graphics/opengl/Extensions3DOpenGLCommon.cpp
        platform/graphics/opengl/GLPlatformContext.cpp
        platform/graphics/opengl/GLPlatformSurface.cpp
        platform/graphics/opengl/GraphicsContext3DOpenGLCommon.cpp
        platform/graphics/opengl/TemporaryOpenGLSetting.cpp

        platform/graphics/surfaces/GLTransportSurface.cpp
        platform/graphics/surfaces/GraphicsSurface.cpp
    )

    if (WTF_USE_EGL)
        list(APPEND WebCore_SOURCES
            platform/graphics/surfaces/egl/EGLConfigSelector.cpp
            platform/graphics/surfaces/egl/EGLContext.cpp
            platform/graphics/surfaces/egl/EGLHelper.cpp
            platform/graphics/surfaces/egl/EGLSurface.cpp
            platform/graphics/surfaces/egl/EGLXSurface.cpp
        )
    else ()
        list(APPEND WebCore_SOURCES
            platform/graphics/surfaces/glx/GLXContext.cpp
            platform/graphics/surfaces/glx/GLXSurface.cpp
        )
    endif ()

    if (WTF_USE_OPENGL_ES_2)
        list(APPEND WebCore_SOURCES
            platform/graphics/opengl/Extensions3DOpenGLES.cpp
            platform/graphics/opengl/GraphicsContext3DOpenGLES.cpp
        )
    else ()
        list(APPEND WebCore_SOURCES
            platform/graphics/opengl/Extensions3DOpenGL.cpp
            platform/graphics/opengl/GraphicsContext3DOpenGL.cpp
        )
    endif ()

    if (WTF_USE_EGL)
        list(APPEND WebCore_LIBRARIES
            ${EGL_LIBRARY}
        )
    endif ()
endif ()

if (ENABLE_WEB_AUDIO)
    #list(APPEND WebCore_INCLUDE_DIRECTORIES
    #    "${WEBCORE_DIR}/platform/audio/gstreamer"
    #)
    set(WEB_AUDIO_DIR ${CMAKE_INSTALL_PREFIX}/${DATA_INSTALL_DIR}/webaudio/resources)
    file(GLOB WEB_AUDIO_DATA "${WEBCORE_DIR}/platform/audio/resources/*.wav")
    install(FILES ${WEB_AUDIO_DATA} DESTINATION ${WEB_AUDIO_DIR})
    add_definitions(-DUNINSTALLED_AUDIO_RESOURCES_DIR="${WEBCORE_DIR}/platform/audio/resources")
endif ()

# Directory listing template for FTP/file/gopher directory listings.
install(FILES "${WEBCORE_DIR}/platform/haiku/resources/Directory Listing Template.html"
    DESTINATION ${CMAKE_INSTALL_PREFIX}/${DATA_INSTALL_DIR})

if (ENABLE_SPELLCHECK)
    list(APPEND WebCore_INCLUDE_DIRECTORIES
        ${ENCHANT_INCLUDE_DIRS}
    )
    list(APPEND WebCore_LIBRARIES
        ${ENCHANT_LIBRARIES}
    )
endif ()

if (ENABLE_ACCESSIBILITY)
    list(APPEND WebCore_INCLUDE_DIRECTORIES
        "${WEBCORE_DIR}/accessibility/atk"
        ${ATK_INCLUDE_DIRS}
    )
    list(APPEND WebCore_LIBRARIES
        ${ATK_LIBRARIES}
    )
endif ()

list(APPEND WebCore_PRIVATE_FRAMEWORK_HEADERS
    platform/haiku/PopupMenuHaiku.h
    platform/haiku/SearchPopupMenuHaiku.h

    html/InputTypeNames.h
    platform/DateTimeChooser.h
    platform/DateTimeChooserClient.h
    platform/DateTimeChooserParameters.h

    platform/graphics/haiku/ImageBufferDataHaiku.h
    platform/graphics/Image.h

    inspector/LegacyWebSocketInspectorInstrumentation.h
)

set(CSS_VALUE_PLATFORM_DEFINES "HAVE_OS_DARK_MODE_SUPPORT=1")
