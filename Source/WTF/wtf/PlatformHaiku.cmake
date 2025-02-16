LIST(APPEND WTF_SOURCES
    generic/WorkQueueGeneric.cpp
    generic/MainThreadGeneric.cpp

    unix/MemoryPressureHandlerUnix.cpp
    unix/UniStdExtrasUnix.cpp

posix/CPUTimePOSIX.cpp
    posix/FileSystemPOSIX.cpp
    posix/OSAllocatorPOSIX.cpp
    posix/ThreadingPOSIX.cpp

    haiku/RunLoopHaiku.cpp
    haiku/CurrentProcessMemoryStatus.cpp
    haiku/MemoryFootprintHaiku.cpp
    haiku/FileSystemHaiku.cpp

    unicode/icu/CollatorICU.cpp

    PlatformUserPreferredLanguagesHaiku.cpp

    text/unix/TextBreakIteratorInternalICUUnix.cpp

    unix/LoggingUnix.cpp
)

list(APPEND WTF_PUBLIC_HEADERS
    haiku/BeDC.h
)

LIST(APPEND WTF_LIBRARIES
    ${ZLIB_LIBRARIES}
    be execinfo
)

list(APPEND WTF_INCLUDE_DIRECTORIES
    /system/develop/headers/private/system/arch/$ENV{BE_HOST_CPU}/
    /system/develop/headers/private/system
)

add_definitions(-D_DEFAULT_SOURCE)
