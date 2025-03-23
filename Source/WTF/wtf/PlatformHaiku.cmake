LIST(APPEND WTF_SOURCES
    generic/WorkQueueGeneric.cpp
    generic/MainThreadGeneric.cpp

    haiku/RunLoopHaiku.cpp
    haiku/CurrentProcessMemoryStatus.cpp
    haiku/MemoryFootprintHaiku.cpp
    haiku/FileSystemHaiku.cpp

    posix/CPUTimePOSIX.cpp
    posix/FileHandlePOSIX.cpp
    posix/FileSystemPOSIX.cpp
    posix/OSAllocatorPOSIX.cpp
    posix/ThreadingPOSIX.cpp

    text/unix/TextBreakIteratorInternalICUUnix.cpp

    unicode/icu/CollatorICU.cpp

    unix/MemoryPressureHandlerUnix.cpp
    unix/UniStdExtrasUnix.cpp

    PlatformUserPreferredLanguagesHaiku.cpp
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
