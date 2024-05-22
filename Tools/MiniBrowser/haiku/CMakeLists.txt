set(MiniBrowser_SOURCES
    ${TOOLS_DIR}/MiniBrowser/haiku/BrowserApp.cpp
    ${TOOLS_DIR}/MiniBrowser/haiku/BrowserWindow.cpp
)

set(MiniBrowser_PRIVATE_INCLUDE_DIRECTORIES
    ${CMAKE_BINARY_DIR}
)

set(MiniBrowser_FRAMEWORKS
    WebKit
)

set(MiniBrowser_PRIVATE_LIBRARIES
    be
)

set(MiniBrowser_DEPENDENCIES
    NetworkProcess
    WebProcess
)

WEBKIT_EXECUTABLE_DECLARE(MiniBrowser)
WEBKIT_EXECUTABLE(MiniBrowser)
