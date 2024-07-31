SET(MiniBrowser_SOURCES
    ${TOOLS_DIR}/MiniBrowser/haiku/BrowserApp.cpp
    ${TOOLS_DIR}/MiniBrowser/haiku/BrowserWindow.cpp
)

set(MiniBrowser_PRIVATE_INCLUDE_DIRECTORIES
    ${CMAKE_BINARY_DIR}
    ${WEBKIT_DIR}/UIProcess/API/
    ${WEBKIT_DIR}/UIProcess/API/haiku
)

SET(MiniBrowser_PRIVATE_LIBRARIES
    WebKit::WebKit
    be
)

SET(MiniBrowser_DEPENDENCIES
    NetworkProcess
    WebProcess
)

WEBKIT_EXECUTABLE_DECLARE(MiniBrowser)
WEBKIT_EXECUTABLE(MiniBrowser)
