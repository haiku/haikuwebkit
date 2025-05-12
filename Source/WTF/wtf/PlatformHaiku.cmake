LIST(APPEND WTF_SOURCES
    generic/WorkQueueGeneric.cpp
    generic/MainThreadGeneric.cpp

    haiku/CurrentProcessMemoryStatus.cpp
    haiku/FileSystemHaiku.cpp
    haiku/LanguageHaiku.cpp
    haiku/MemoryFootprintHaiku.cpp
    haiku/RunLoopHaiku.cpp

    posix/CPUTimePOSIX.cpp
    posix/FileHandlePOSIX.cpp
    posix/FileSystemPOSIX.cpp
    posix/MappedFileDataPOSIX.cpp
    posix/OSAllocatorPOSIX.cpp
    posix/ThreadingPOSIX.cpp

    text/unix/TextBreakIteratorInternalICUUnix.cpp

    unicode/icu/CollatorICU.cpp

    unix/MemoryPressureHandlerUnix.cpp
    unix/UniStdExtrasUnix.cpp
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
