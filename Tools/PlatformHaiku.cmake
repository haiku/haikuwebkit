add_subdirectory(TestRunnerShared)
add_subdirectory(ImageDiff)
add_subdirectory(MiniBrowser/haiku)

if(ENABLE_WEBKIT_LEGACY EQUAL ON)
	add_subdirectory(DumpRenderTree)
	add_subdirectory(HaikuLauncher)
endif()
