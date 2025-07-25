set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPERIMENTAL_CXX_MODULE_DYNDEP OFF)

add_definitions(-DBUILDING_WITH_CMAKE=1)
add_definitions(-DBUILDING_WEBKIT=1)
add_definitions(-DHAVE_CONFIG_H=1)
add_definitions(-DPAS_BMALLOC=1)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
define_property(TARGET PROPERTY FOLDER INHERITED BRIEF_DOCS "folder" FULL_DOCS "IDE folder name")

if (WTF_CPU_ARM)
    set(ARM_THUMB2_TEST_SOURCE
    "
    #if !defined(thumb2) && !defined(__thumb2__)
    #error \"Thumb2 instruction set isn't available\"
    #endif
    int main() {}
    ")

    if (COMPILER_IS_CLANG AND NOT (${CMAKE_SYSTEM_NAME} STREQUAL "Darwin"))
        set(CLANG_EXTRA_ARM_ARGS " -mthumb")
    endif ()

    set(CMAKE_REQUIRED_FLAGS "${CLANG_EXTRA_ARM_ARGS}")
    CHECK_CXX_SOURCE_COMPILES("${ARM_THUMB2_TEST_SOURCE}" ARM_THUMB2_DETECTED)
    unset(CMAKE_REQUIRED_FLAGS)

    if (ARM_THUMB2_DETECTED AND NOT (${CMAKE_SYSTEM_NAME} STREQUAL "Darwin"))
        string(APPEND CMAKE_C_FLAGS " ${CLANG_EXTRA_ARM_ARGS}")
        string(APPEND CMAKE_CXX_FLAGS " ${CLANG_EXTRA_ARM_ARGS}")
    endif ()
endif ()

# Use ld.lld when building with LTO, or for debug builds, if available.
# FIXME: With CMake 3.22+ full conditional syntax can be used in
#        cmake_dependent_option()
if (LTO_MODE OR DEVELOPER_MODE)
    set(TRY_USE_LD_LLD ON)
endif ()
CMAKE_DEPENDENT_OPTION(USE_LD_LLD "Use LLD linker" ON
                       "TRY_USE_LD_LLD;NOT WIN32" OFF)
if (USE_LD_LLD)
    execute_process(COMMAND ${CMAKE_C_COMPILER} -fuse-ld=lld -Wl,--version ERROR_QUIET OUTPUT_VARIABLE LD_VERSION)
    if (LD_VERSION MATCHES "(^|[ \t])LLD ")
        string(APPEND CMAKE_EXE_LINKER_FLAGS " -fuse-ld=lld")
        string(APPEND CMAKE_SHARED_LINKER_FLAGS " -fuse-ld=lld")
        string(APPEND CMAKE_MODULE_LINKER_FLAGS " -fuse-ld=lld")
    else ()
        set(USE_LD_LLD OFF)
    endif ()
endif ()

# Determine which linker is being used with the chosen linker flags.
separate_arguments(LD_VERSION_COMMAND UNIX_COMMAND
    "${CMAKE_C_COMPILER} ${CMAKE_EXE_LINKER_FLAGS} -Wl,--version"
)
execute_process(
    COMMAND ${LD_VERSION_COMMAND}
    OUTPUT_VARIABLE LD_VERSION
    ERROR_QUIET
)
unset(LD_VERSION_COMMAND)

separate_arguments(LD_USAGE_COMMAND UNIX_COMMAND
    "${CMAKE_C_COMPILER} ${CMAKE_EXE_LINKER_FLAGS} -Wl,--help"
)
execute_process(
  COMMAND ${LD_USAGE_COMMAND}
  OUTPUT_VARIABLE LD_USAGE
    ERROR_QUIET
)
unset(LD_USAGE_COMMAND)

set(LD_SUPPORTS_SPLIT_DEBUG TRUE)
set(LD_SUPPORTS_THIN_ARCHIVES TRUE)
if (LD_VERSION MATCHES "^LLD ")
    set(LD_VARIANT LLD)
elseif (LD_VERSION MATCHES "^mold ")
    set(LD_VARIANT MOLD)
elseif (LD_VERSION MATCHES "^GNU gold ")
    set(LD_VARIANT GOLD)
elseif (LD_VERSION MATCHES "^GNU ld ")
    set(LD_VARIANT BFD)
    set(LD_SUPPORTS_GDB_INDEX FALSE)
else ()
    set(LD_VARIANT UNKNOWN)
    set(LD_SUPPORTS_SPLIT_DEBUG FALSE)
    set(LD_SUPPORTS_THIN_ARCHIVES FALSE)
endif ()
unset(LD_VERSION)

set(LD_SUPPORTS_GDB_INDEX FALSE)
set(LD_SUPPORTS_DISABLE_NEW_DTAGS FALSE)
set(LD_SUPPORTS_GC_SECTIONS FALSE)
if (LD_USAGE MATCHES "--gdb-index")
    set(LD_SUPPORTS_GDB_INDEX TRUE)
endif ()
if (LD_USAGE MATCHES "--disable-new-dtags")
    set(LD_SUPPORTS_DISABLE_NEW_DTAGS TRUE)
endif ()
if (LD_USAGE MATCHES "--gc-sections")
    set(LD_SUPPORTS_GC_SECTIONS TRUE)
endif ()
unset(LD_USAGE)

message(STATUS "Linker variant in use: ${LD_VARIANT} ")
message(STATUS "  Linker supports thin archives - ${LD_SUPPORTS_THIN_ARCHIVES}")
message(STATUS "  Linker supports split debug info - ${LD_SUPPORTS_SPLIT_DEBUG}")
message(STATUS "  Linker supports --gdb-index - ${LD_SUPPORTS_GDB_INDEX}")
message(STATUS "  Linker supports --disable-new-dtags - ${LD_SUPPORTS_DISABLE_NEW_DTAGS}")
message(STATUS "  Linker supports --gc-sections - ${LD_SUPPORTS_GC_SECTIONS}")

# Determine whether the archiver in use supports thin archives.
separate_arguments(AR_VERSION_COMMAND UNIX_COMMAND "${CMAKE_AR} -V")
execute_process(
    COMMAND ${AR_VERSION_COMMAND}
    OUTPUT_VARIABLE AR_VERSION
    RESULT_VARIABLE AR_STATUS
    ERROR_QUIET
)
unset(AR_VERSION_COMMAND)

set(AR_SUPPORTS_THIN_ARCHIVES FALSE)
if (AR_STATUS EQUAL 0)
    if (AR_VERSION MATCHES "^GNU ar ")
        set(AR_VARIANT BFD)
        set(AR_SUPPORTS_THIN_ARCHIVES TRUE)
    elseif (AR_VERSION MATCHES "^LLVM ")
        set(AR_VARIANT LLVM)
        set(AR_SUPPORTS_THIN_ARCHIVES TRUE)
    else ()
        set(AR_VARIANT UNKNOWN)
    endif ()
endif ()
unset(AR_VERSION)
unset(AR_STATUS)
message(STATUS "Archiver variant in use: ${AR_VARIANT}")
message(STATUS "  Archiver supports thin archives - ${AR_SUPPORTS_THIN_ARCHIVES}")

# Remove unused sections to reduce the binary size when supported.
if (LD_SUPPORTS_GC_SECTIONS)
    WEBKIT_APPEND_GLOBAL_COMPILER_FLAGS(-ffunction-sections -fdata-sections)
    string(APPEND CMAKE_EXE_LINKER_FLAGS " -Wl,--gc-sections")
    string(APPEND CMAKE_SHARED_LINKER_FLAGS " -Wl,--gc-sections")
    string(APPEND CMAKE_MODULE_LINKER_FLAGS " -Wl,--gc-sections")
endif ()

# Use --disable-new-dtags to ensure that the rpath set by CMake when building
# will use a DT_RPATH entry in the ELF headers, to ensure that the build
# directory lib/ subdir is always used and developers do not accidentally run
# test programs from the build directory against system libraries. Without
# passing the option DT_RUNPATH is used, which can be overriden by the value
# of LD_LIBRARY_PATH set in the environment, resulting in unexpected behaviour
# for developers.
if (LD_SUPPORTS_DISABLE_NEW_DTAGS)
    string(APPEND CMAKE_EXE_LINKER_FLAGS " -Wl,--disable-new-dtags")
    string(APPEND CMAKE_SHARED_LINKER_FLAGS " -Wl,--disable-new-dtags")
    string(APPEND CMAKE_MODULE_LINKER_FLAGS " -Wl,--disable-new-dtags")
endif ()

# Prefer thin archives by default if they can be both created by the
# archiver and read back by the linker.
if (AR_SUPPORTS_THIN_ARCHIVES AND LD_SUPPORTS_THIN_ARCHIVES)
    set(USE_THIN_ARCHIVES_DEFAULT ON)
else ()
    set(USE_THIN_ARCHIVES_DEFAULT OFF)
endif ()
option(USE_THIN_ARCHIVES "Produce all static libraries as thin archives" ${USE_THIN_ARCHIVES_DEFAULT})

if (USE_THIN_ARCHIVES)
    set(CMAKE_CXX_ARCHIVE_CREATE "<CMAKE_AR> crT <TARGET> <LINK_FLAGS> <OBJECTS>")
    set(CMAKE_C_ARCHIVE_CREATE "<CMAKE_AR> crT <TARGET> <LINK_FLAGS> <OBJECTS>")
    set(CMAKE_CXX_ARCHIVE_APPEND "<CMAKE_AR> rT <TARGET> <LINK_FLAGS> <OBJECTS>")
    set(CMAKE_C_ARCHIVE_APPEND "<CMAKE_AR> rT <TARGET> <LINK_FLAGS> <OBJECTS>")
endif ()

set(ENABLE_DEBUG_FISSION_DEFAULT OFF)
if (ENABLE_DEVELOPER_MODE AND (CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo") AND NOT CMAKE_GENERATOR MATCHES "Visual Studio")
    WEBKIT_CHECK_COMPILER_FLAGS(CXX CXX_COMPILER_SUPPORTS_GSPLIT_DWARF -gsplit-dwarf)
    if (CXX_COMPILER_SUPPORTS_GSPLIT_DWARF AND LD_SUPPORTS_SPLIT_DEBUG)
        set(ENABLE_DEBUG_FISSION_DEFAULT ON)
    endif ()
endif ()

option(DEBUG_FISSION "Use Debug Fission support" ${ENABLE_DEBUG_FISSION_DEFAULT})

if (DEBUG_FISSION)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -gsplit-dwarf")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -gsplit-dwarf")
    if (LD_SUPPORTS_GDB_INDEX)
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--gdb-index")
        set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--gdb-index")
    endif ()
endif ()

set(GCC_OFFLINEASM_SOURCE_MAP_DEFAULT OFF)
if (CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    set(GCC_OFFLINEASM_SOURCE_MAP_DEFAULT ON)
endif ()

option(GCC_OFFLINEASM_SOURCE_MAP
  "Produce debug line information for offlineasm-generated code"
  ${GCC_OFFLINEASM_SOURCE_MAP_DEFAULT})

option(USE_APPLE_ICU "Use Apple's internal ICU" ${APPLE})

# Enable the usage of OpenMP.
#  - At this moment, OpenMP is only used as an alternative implementation
#    to native threads for the parallelization of the SVG filters.
if (USE_OPENMP)
    find_package(OpenMP REQUIRED)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
endif ()

# Detect the kind of C++ standard library being used, to enable assertions.
set(CXX_STDLIB_VARIANT "UNKNOWN")
set(CXX_STDLIB_ASSERTIONS_MACRO)
set(CXX_STDLIB_TEST_SOURCE "
    #if defined(__clang__)
    #include <utility>
    int main() { return _LIBCPP_VERSION; }
    #else
    #error Clang needed for libc++
    #endif
")
check_cxx_source_compiles("${CXX_STDLIB_TEST_SOURCE}" CXX_STDLIB_IS_LIBCPP)
if (CXX_STDLIB_IS_LIBCPP)
    set(CXX_STDLIB_TEST_SOURCE "
        #include <utility>
        #if _LIBCPP_VERSION >= 190000
        int main() { }
        #else
        #error libc++ is older than 19.x
        #endif
    ")
    check_cxx_source_compiles("${CXX_STDLIB_TEST_SOURCE}" CXX_STDLIB_IS_LIBCPP_19_OR_NEWER)
    if (CXX_STDLIB_IS_LIBCPP_19_OR_NEWER)
        set(CXX_STDLIB_VARIANT "LIBCPP 19+")
        set(CXX_STDLIB_ASSERTIONS_MACRO _LIBCPP_HARDENING_MODE=_LIBCPP_HARDENING_MODE_EXTENSIVE)
    else ()
        set(CXX_STDLIB_VARIANT "LIBCPP <19")
        set(CXX_STDLIB_ASSERTIONS_MACRO _LIBCPP_ENABLE_ASSERTIONS=1)
    endif ()
else ()
    set(CXX_STDLIB_TEST_SOURCE "
    #include <utility>
    int main() { return _GLIBCXX_RELEASE; }
    ")
    check_cxx_source_compiles("${CXX_STDLIB_TEST_SOURCE}" CXX_STDLIB_IS_GLIBCXX)
    if (CXX_STDLIB_IS_GLIBCXX)
        set(CXX_STDLIB_VARIANT "GLIBCXX")
        set(CXX_STDLIB_ASSERTIONS_MACRO _GLIBCXX_ASSERTIONS=1)
    endif ()
endif ()
message(STATUS "C++ standard library in use: ${CXX_STDLIB_VARIANT}")

if (CXX_STDLIB_ASSERTIONS_MACRO)
    set(USE_CXX_STDLIB_ASSERTIONS_DEFAULT ON)
else ()
    set(USE_CXX_STDLIB_ASSERTIONS_DEFAULT OFF)
endif ()
option(USE_CXX_STDLIB_ASSERTIONS
    "Enable lightweight run-time assertions in the C++ standard library"
    ${USE_CXX_STDLIB_ASSERTIONS_DEFAULT})

if (USE_CXX_STDLIB_ASSERTIONS)
    if (CXX_STDLIB_ASSERTIONS_MACRO)
        message(STATUS "  Assertions enabled, ${CXX_STDLIB_ASSERTIONS_MACRO}")
        add_compile_definitions("${CXX_STDLIB_ASSERTIONS_MACRO}")
    else ()
        message(STATUS "  Assertions disabled, CXX_STDLIB_ASSERTIONS_MACRO undefined")
    endif ()
else ()
    message(STATUS "  Assertions disabled, USE_CXX_STDLIB_ASSERTIONS=${USE_CXX_STDLIB_ASSERTIONS}")
endif ()

# GTK and WPE use the GNU installation directories as defaults.
if (NOT PORT STREQUAL "GTK" AND NOT PORT STREQUAL "WPE")
    IF(HAIKU)
        set(LIB_SUFFIX "/${CMAKE_HAIKU_SECONDARY_ARCH}" CACHE STRING
            "Define suffix of directory name (x86/x86_gcc2)")
    ELSE()
        set(LIB_SUFFIX "" CACHE STRING "Define suffix of directory name (32/64)")
    ENDIF()
    set(LIB_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/lib${LIB_SUFFIX}" CACHE PATH "Absolute path to library installation directory")
    set(EXEC_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/bin" CACHE PATH "Absolute path to executable installation directory")
    set(LIBEXEC_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/bin" CACHE PATH "Absolute path to install executables executed by the library")
endif ()

set(ENABLE_ASSERTS "AUTO" CACHE STRING "Enable or disable assertions regardless of build type")
set_property(CACHE ENABLE_ASSERTS PROPERTY STRINGS "AUTO" "ON" "OFF")

if (ENABLE_ASSERTS STREQUAL "AUTO")
    # The default value is handled by the NDEBUG define which is generally set by the toolchain module used by CMake.
elseif (ENABLE_ASSERTS)
    WEBKIT_PREPEND_GLOBAL_COMPILER_FLAGS(-DASSERT_ENABLED=1)
elseif (NOT ENABLE_ASSERTS)
    WEBKIT_PREPEND_GLOBAL_COMPILER_FLAGS(-DASSERT_ENABLED=0)
endif ()

# Check whether features.h header exists.
# Including glibc's one defines __GLIBC__, that is used in Platform.h
WEBKIT_CHECK_HAVE_INCLUDE(HAVE_FEATURES_H features.h)

# Check for headers
WEBKIT_CHECK_HAVE_INCLUDE(HAVE_ERRNO_H errno.h)
WEBKIT_CHECK_HAVE_INCLUDE(HAVE_LANGINFO_H langinfo.h)
WEBKIT_CHECK_HAVE_INCLUDE(HAVE_MMAP sys/mman.h)
WEBKIT_CHECK_HAVE_INCLUDE(HAVE_PTHREAD_NP_H pthread_np.h)
WEBKIT_CHECK_HAVE_INCLUDE(HAVE_SYS_PARAM_H sys/param.h)
WEBKIT_CHECK_HAVE_INCLUDE(HAVE_SYS_TIME_H sys/time.h)
WEBKIT_CHECK_HAVE_INCLUDE(HAVE_SYS_TIMEB_H sys/timeb.h)
WEBKIT_CHECK_HAVE_INCLUDE(HAVE_LINUX_MEMFD_H linux/memfd.h)

# Check for functions
# _GNU_SOURCE=1 is required to expose statx
list(APPEND CMAKE_REQUIRED_DEFINITIONS "-D_GNU_SOURCE=1")
WEBKIT_CHECK_HAVE_FUNCTION(HAVE_ALIGNED_MALLOC _aligned_malloc malloc.h)
WEBKIT_CHECK_HAVE_FUNCTION(HAVE_LOCALTIME_R localtime_r time.h)
WEBKIT_CHECK_HAVE_FUNCTION(HAVE_MALLOC_TRIM malloc_trim malloc.h)
WEBKIT_CHECK_HAVE_FUNCTION(HAVE_STATX statx sys/stat.h)
WEBKIT_CHECK_HAVE_FUNCTION(HAVE_TIMEGM timegm time.h)
WEBKIT_CHECK_HAVE_FUNCTION(HAVE_VASPRINTF vasprintf stdio.h)

# Check for symbols
WEBKIT_CHECK_HAVE_SYMBOL(HAVE_REGEX_H regexec regex.h)
if (NOT (${CMAKE_SYSTEM_NAME} STREQUAL "Darwin"))
    WEBKIT_CHECK_HAVE_SYMBOL(HAVE_PTHREAD_MAIN_NP pthread_main_np pthread_np.h)
endif ()
WEBKIT_CHECK_HAVE_SYMBOL(HAVE_MAP_ALIGNED MAP_ALIGNED sys/mman.h)
WEBKIT_CHECK_HAVE_SYMBOL(HAVE_SHM_ANON SHM_ANON sys/mman.h)
WEBKIT_CHECK_HAVE_SYMBOL(HAVE_TIMINGSAFE_BCMP timingsafe_bcmp string.h)
# Windows has signal.h but is missing symbols that are used in calls to signal.
WEBKIT_CHECK_HAVE_SYMBOL(HAVE_SIGNAL_H SIGTRAP signal.h)

# Check for struct members
WEBKIT_CHECK_HAVE_STRUCT(HAVE_STAT_BIRTHTIME "struct stat" st_birthtime sys/stat.h)
WEBKIT_CHECK_HAVE_STRUCT(HAVE_TM_GMTOFF "struct tm" tm_gmtoff time.h)
WEBKIT_CHECK_HAVE_STRUCT(HAVE_TM_ZONE "struct tm" tm_zone time.h)

# Check for int types
check_type_size("__int128_t" INT128_VALUE)

if (HAVE_INT128_VALUE)
  SET_AND_EXPOSE_TO_BUILD(HAVE_INT128_T INT128_VALUE)
endif ()

# Check which filesystem implementation is available if any
if (STD_FILESYSTEM_IS_AVAILABLE)
    SET_AND_EXPOSE_TO_BUILD(HAVE_STD_FILESYSTEM TRUE)
elseif (STD_EXPERIMENTAL_FILESYSTEM_IS_AVAILABLE)
    SET_AND_EXPOSE_TO_BUILD(HAVE_STD_EXPERIMENTAL_FILESYSTEM TRUE)
endif ()
