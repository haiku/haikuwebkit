set(DumpRenderTree_DIR "${TOOLS_DIR}/DumpRenderTree")
set(DumpRenderTree_BINDINGS_DIR "${TOOLS_DIR}/DumpRenderTree/Bindings")

add_definitions(-D_DEFAULT_SOURCE=1)

file(MAKE_DIRECTORY ${DumpRenderTree_DERIVED_SOURCES_DIR})

set(DumpRenderTree_SOURCES
    AccessibilityController.cpp
    AccessibilityTextMarker.cpp
    AccessibilityUIElement.cpp
    CyclicRedundancyCheck.cpp
    GCController.cpp
    JavaScriptThreading.cpp
    PixelDumpSupport.cpp
    TestOptions.cpp
    TestRunner.cpp
    WorkQueue.cpp
)

set(DumpRenderTree_LIBRARIES
    WebKit::TestRunnerShared
    WebKit::WebCoreTestSupport
)
set(DumpRenderTree_FRAMEWORKS
    JavaScriptCore
    PAL
    WTF
    WebCore
    WebKitLegacy
)
if (NOT USE_SYSTEM_MALLOC)
    list(APPEND WebKitTestRunner_FRAMEWORKS bmalloc)
endif ()

set(DumpRenderTree_PRIVATE_INCLUDE_DIRECTORIES
    ${CMAKE_BINARY_DIR}
    ${DumpRenderTree_DERIVED_SOURCES_DIR}
    ${DumpRenderTree_DIR}
)

set(DumpRenderTree_WEB_PREFERENCES_TEMPLATES
    ${DumpRenderTree_DIR}/Scripts/PreferencesTemplates/TestOptionsGeneratedWebKitLegacyKeyMapping.cpp.erb
    ${DumpRenderTree_DIR}/Scripts/PreferencesTemplates/TestOptionsGeneratedKeys.h.erb
)

set(DumpRenderTree_WEB_PREFERENCES
    ${WTF_SCRIPTS_DIR}/Preferences/UnifiedWebPreferences.yaml
)

set_source_files_properties(${DumpRenderTree_WEB_PREFERENCES} PROPERTIES GENERATED TRUE)

add_custom_command(
    OUTPUT ${DumpRenderTree_DERIVED_SOURCES_DIR}/TestOptionsGeneratedWebKitLegacyKeyMapping.cpp ${DumpRenderTree_DERIVED_SOURCES_DIR}/TestOptionsGeneratedKeys.h
    DEPENDS ${DumpRenderTree_WEB_PREFERENCES_TEMPLATES} ${DumpRenderTree_WEB_PREFERENCES} WTF_CopyPreferences
    COMMAND ${RUBY_EXECUTABLE} ${WTF_SCRIPTS_DIR}/GeneratePreferences.rb --frontend WebKitLegacy --outputDir "${DumpRenderTree_DERIVED_SOURCES_DIR}" --template ${DumpRenderTree_DIR}/Scripts/PreferencesTemplates/TestOptionsGeneratedWebKitLegacyKeyMapping.cpp.erb --template ${DumpRenderTree_DIR}/Scripts/PreferencesTemplates/TestOptionsGeneratedKeys.h.erb ${DumpRenderTree_WEB_PREFERENCES}
    VERBATIM)

list(APPEND DumpRenderTree_SOURCES
    ${DumpRenderTree_DERIVED_SOURCES_DIR}/TestOptionsGeneratedWebKitLegacyKeyMapping.cpp
    ${DumpRenderTree_DERIVED_SOURCES_DIR}/TestOptionsGeneratedKeys.h
)

WEBKIT_EXECUTABLE_DECLARE(DumpRenderTree)
WEBKIT_INCLUDE_CONFIG_FILES_IF_EXISTS()
WEBKIT_EXECUTABLE(DumpRenderTree)
