if (ENABLE_WEBKIT)
    add_subdirectory(MiniBrowser/haiku)
endif ()

if (ENABLE_WEBKIT_LEGACY EQUAL ON)
    add_subdirectory(HaikuLauncher)
endif ()
