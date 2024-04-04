# Project setup
CMAKE_MINIMUM_REQUIRED(VERSION 3.4...3.5)
PROJECT(wildmidi LANGUAGES C VERSION 0.4.6)

# WildMIDI Version
SET(WILDMIDI_VERSION ${PROJECT_VERSION})

# Lib Versions
SET(SOVERSION 2)
SET(VERSION 2.1.0)

# Find Macros
SET(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)

INCLUDE(CMakeDependentOption)
INCLUDE(CheckCCompilerFlag)
INCLUDE(CheckCSourceCompiles)
INCLUDE(CheckIncludeFile)
INCLUDE(GNUInstallDirs)
INCLUDE(TestBigEndian)

# Set our options
OPTION(BUILD_SHARED_LIBS "Build a dynamic wildmidi library" ON)
OPTION(WANT_PLAYER "Build WildMIDI player in addition to the libraries" ON)
OPTION(WANT_STATIC "Build static library in addition to dynamic library" OFF)
CMAKE_DEPENDENT_OPTION(WANT_PLAYERSTATIC "Build a statically linked WildMIDI player" ON "WANT_STATIC;WANT_PLAYER" OFF)

# Player options
IF (CMAKE_SYSTEM_NAME MATCHES ".*Linux")
    SET(ALSA_DEFAULT ON)
ELSE()
    SET(ALSA_DEFAULT OFF)
ENDIF()
IF (CMAKE_SYSTEM_NAME MATCHES "DragonFly.*|kFreeBSD.*|FreeBSD")
    SET(OSS_DEFAULT ON)
ELSE()
    SET(OSS_DEFAULT OFF)
ENDIF()
IF (CMAKE_SYSTEM_NAME MATCHES "kOpenBSD.*|OpenBSD.*")
    SET(SNDIO_DEFAULT ON)
    SET(SYS_OPENBSD TRUE)
ELSE()
    SET(SNDIO_DEFAULT OFF)
    SET(SYS_OPENBSD FALSE)
ENDIF()
IF(CMAKE_SYSTEM_NAME MATCHES "kNetBSD.*|NetBSD.*")
    SET(SYS_NETBSD TRUE)
ELSE()
    SET(SYS_NETBSD FALSE)
ENDIF()

CMAKE_DEPENDENT_OPTION(WANT_AMIGA_AHI "Include Amiga AHI support" ON "AMIGA" OFF)
CMAKE_DEPENDENT_OPTION(WANT_ALSA "Include ALSA (Advanced Linux Sound Architecture) support" ${ALSA_DEFAULT} "UNIX" OFF)
CMAKE_DEPENDENT_OPTION(WANT_COREAUDIO "Include macOS CoreAudio support" ON "APPLE" OFF)
CMAKE_DEPENDENT_OPTION(WANT_DOSSB "Include DOS SoundBlaster support" ON "DJGPP" OFF)
CMAKE_DEPENDENT_OPTION(WANT_OS2DART "Include OS/2 DART support" ON "OS2" OFF)
CMAKE_DEPENDENT_OPTION(WANT_NETBSD "Include NetBSD audio support" ON "SYS_NETBSD" OFF)
CMAKE_DEPENDENT_OPTION(WANT_OSS "Include OSS (Open Sound System) support" ${OSS_DEFAULT} "UNIX" OFF)
CMAKE_DEPENDENT_OPTION(WANT_SNDIO "Include OpenBSD sndio support" ${SNDIO_DEFAULT} "UNIX" OFF)
CMAKE_DEPENDENT_OPTION(WANT_WINMM "Include Windows WinMM audio support" ON "WIN32 OR CYGWIN" OFF)
OPTION(WANT_OPENAL "Include OpenAL (Cross Platform) support" OFF)

OPTION(WANT_DEVTEST "Build WildMIDI DevTest file to check files" OFF)
CMAKE_DEPENDENT_OPTION(WANT_OSX_DEPLOYMENT "OSX Deployment" OFF "APPLE" OFF)

IF (WIN32 AND MSVC)
    OPTION(WANT_MP_BUILD "Build with Multiple Processes (/MP)" OFF)
ENDIF()

IF (UNIX AND NOT APPLE AND NOT RISCOS)
    SET(WILDMIDI_CFG "/etc/wildmidi/wildmidi.cfg" CACHE STRING "default config location")
ELSE ()
    SET(WILDMIDI_CFG "wildmidi.cfg" CACHE STRING "default config location")
ENDIF()

IF ((NOT BUILD_SHARED_LIBS) AND (NOT WANT_STATIC))
    MESSAGE(FATAL_ERROR "Neither dynamic nor static library build is selected.")
ENDIF()

# Platform specific defines
IF (UNIX)
    # allow 'large' files in 32 bit builds
    ADD_DEFINITIONS(
            -D_LARGEFILE_SOURCE
            -D_FILE_OFFSET_BITS=64
            -D_LARGE_FILES
    )
ENDIF()

IF (SYS_OPENBSD)
    # Set RPATH for OpenBSD so WildMIDI can find libWildMidi.so
    # use, i.e. don't skip the full RPATH for the build tree
    SET(CMAKE_SKIP_BUILD_RPATH FALSE)

    # when building, don't use the install RPATH already
    # (but later on when installing)
    SET(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)
    SET(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")

    # add the automatically determined parts of the RPATH
    # which point to directories outside the build tree to the install RPATH
    SET(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

    # the RPATH to be used when installing, but only if it's not a system directory
    LIST(FIND CMAKE_PLATFORM_IMPLICIT_LINK_DIRECTORIES "${CMAKE_INSTALL_PREFIX}/lib" isSystemDir)
    IF ("${isSystemDir}" STREQUAL "-1")
        SET(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")
    ENDIF()
ENDIF()

IF (WIN32)
    ADD_DEFINITIONS(
        -DWIN32_LEAN_AND_MEAN
        -DNOMINMAX
        -D_CRT_SECURE_NO_WARNINGS
    )
ENDIF()

# Compiler specific settings
IF ((CMAKE_C_COMPILER_ID MATCHES "GNU") OR (CMAKE_C_COMPILER_ID MATCHES "Clang"))
    ADD_DEFINITIONS(
            -Wall -W
            -fno-common
    )

    IF (NOT WIN32 AND NOT CYGWIN)
        SET(OLD_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS}")
        SET(CMAKE_REQUIRED_FLAGS "${OLD_REQUIRED_FLAGS} -Werror")
        CHECK_C_SOURCE_COMPILES("int foo(void) __attribute__((visibility(\"default\")));
                                 int main(void) {return 0;}" HAVE_VISIBILITY_DEFAULT)
        IF (HAVE_VISIBILITY_DEFAULT)
            CHECK_C_COMPILER_FLAG(-fvisibility=hidden HAVE_VISIBILITY_HIDDEN)
        ENDIF()
        SET(CMAKE_REQUIRED_FLAGS "${OLD_REQUIRED_FLAGS}")
    ENDIF()

    IF (CMAKE_BUILD_TYPE STREQUAL "Debug")
        ADD_DEFINITIONS(-ggdb3 -O0)
    ELSEIF (CMAKE_BUILD_TYPE STREQUAL "Release")
        ADD_DEFINITIONS(-O3)
    ENDIF()
ENDIF()

IF (CMAKE_C_COMPILER_ID MATCHES "SunPro")
    SET(OLD_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS}")
    SET(CMAKE_REQUIRED_FLAGS "${OLD_REQUIRED_FLAGS} -xldscope=hidden")
    # __SUNPRO_C >= 0x590
    #   CHECK_C_SOURCE_COMPILES("int foo(void) __attribute__((visibility(\"default\")));
    #                            int main(void) {return 0;}" HAVE_VISIBILITY_DEFAULT)
    # __SUNPRO_C >= 0x550
    CHECK_C_SOURCE_COMPILES("__global int foo(void);
                             int main(void) {return 0;}" HAVE_LDSCOPE_GLOBAL)
    IF (HAVE_LDSCOPE_GLOBAL)# OR HAVE_VISIBILITY_DEFAULT
        SET(HAVE_LDSCOPE_HIDDEN 1)
    ENDIF()
    SET(CMAKE_REQUIRED_FLAGS "${OLD_REQUIRED_FLAGS}")
ENDIF()

CHECK_C_SOURCE_COMPILES("int main(void) {__builtin_expect(0,0); return 0;}" HAVE___BUILTIN_EXPECT)

CHECK_C_SOURCE_COMPILES("static inline int static_foo() {return 0;}
                         int main(void) {return 0;}" HAVE_C_INLINE)
CHECK_C_SOURCE_COMPILES("static __inline__ int static_foo() {return 0;}
                         int main(void) {return 0;}" HAVE_C___INLINE__)
CHECK_C_SOURCE_COMPILES("static __inline int static_foo() {return 0;}
                         int main(void) {return 0;}" HAVE_C___INLINE)

# we must not have any unresolved symbols:
if (APPLE)
    SET(EXTRA_LDFLAGS "-Wl,-undefined,error")
ELSEIF(SYS_OPENBSD)
    SET(EXTRA_LDFLAGS "")
ELSE()
    SET(OLD_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS}")
    SET(CMAKE_REQUIRED_FLAGS "${OLD_REQUIRED_FLAGS} -Wl,--no-undefined")
    CHECK_C_COMPILER_FLAG("" HAVE_NO_UNDEFINED)
    SET(CMAKE_REQUIRED_FLAGS "${OLD_REQUIRED_FLAGS}")
    IF (HAVE_NO_UNDEFINED)
        SET(EXTRA_LDFLAGS "-Wl,--no-undefined")
    ELSE()
        SET(EXTRA_LDFLAGS "")
    ENDIF()
ENDIF()

CHECK_INCLUDE_FILE(stdint.h HAVE_STDINT_H)
CHECK_INCLUDE_FILE(inttypes.h HAVE_INTTYPES_H)

TEST_BIG_ENDIAN(WORDS_BIGENDIAN)

# UNIX-like environments
IF (UNIX AND NOT APPLE)
    # find our math lib
    FIND_LIBRARY(M_LIBRARY m)
    MESSAGE(STATUS "M_LIBRARY: ${M_LIBRARY}")
    IF (NOT M_LIBRARY)
        SET(M_LIBRARY "")
    ENDIF()
ENDIF()

# ######### General setup ##########
INCLUDE_DIRECTORIES(BEFORE "${PROJECT_SOURCE_DIR}/include" "${PROJECT_BINARY_DIR}/include")
IF (NOT HAVE_STDINT_H) # AND NOT HAVE_INTTYPES_H
    INCLUDE_DIRECTORIES(BEFORE "${PROJECT_SOURCE_DIR}/include/stdint")
ENDIF()

IF (AMIGA OR AROS)
    SET(WILDMIDI_AMIGA 1)
ENDIF()

IF (APPLE)
    SET(APP_BUNDLE_NAME "${CMAKE_PROJECT_NAME}.app")
    SET(APP_BUNDLE_DIR "${wildmidi_BINARY_DIR}/${APP_BUNDLE_NAME}")
    IF (WANT_OSX_DEPLOYMENT)
        SET(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
    ENDIF()
ENDIF()

IF (APPLE)
    SET(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${APP_BUNDLE_DIR}/Contents/MacOS")
ENDIF()

IF(WANT_PLAYER OR WANT_PLAYERSTATIC)
    SET(ENABLED_OUTPUT "")

    IF (WANT_AMIGA_AHI)
        SET(AUDIODRV_AHI 1)
        STRING(APPEND ENABLED_OUTPUT " ahi")
    ENDIF()

    IF (WANT_ALSA)
        FIND_PACKAGE(ALSA REQUIRED)
        SET(AUDIODRV_ALSA 1)
        LIST(APPEND AUDIO_LIBRARY ${ALSA_LIBRARIES})
        STRING(APPEND ENABLED_OUTPUT " alsa")
    ENDIF()

    IF (WANT_COREAUDIO)
        SET(AUDIODRV_COREAUDIO 1)
        LIST(APPEND AUDIO_LIBRARY -Wl,-framework,AudioToolbox)
        LIST(APPEND AUDIO_LIBRARY -Wl,-framework,AudioUnit)
        LIST(APPEND AUDIO_LIBRARY -Wl,-framework,CoreServices)
        STRING(APPEND ENABLED_OUTPUT " coreaudio")
    ENDIF()

    IF (WANT_DOSSB)
        SET(AUDIODRV_DOSSB 1)
        STRING(APPEND ENABLED_OUTPUT " dossb")
    ENDIF()

    IF (WANT_OPENAL)
        FIND_PACKAGE(OpenAL REQUIRED)
        SET(AUDIODRV_OPENAL 1)
        LIST(APPEND AUDIO_LIBRARY ${OPENAL_LIBRARY})
        STRING(APPEND ENABLED_OUTPUT " openal")
    ENDIF()

    IF (WANT_OS2DART)
        SET(AUDIODRV_OS2DART 1)
        LIST(APPEND AUDIO_LIBRARY mmpm2)
        STRING(APPEND ENABLED_OUTPUT " dart")
    ENDIF()

    IF (WANT_OSS)
        FIND_PACKAGE(OSS REQUIRED)
        SET(AUDIODRV_OSS 1)
        IF(OSS_LIBRARIES)
           LIST(APPEND AUDIO_LIBRARY ${OSS_LIBRARIES})
        ENDIF()
        STRING(APPEND ENABLED_OUTPUT " oss")
    ENDIF()

    IF (WANT_NETBSD)
        FIND_PACKAGE(AudioIO REQUIRED)
        SET(AUDIODRV_NETBSD 1)
        STRING(APPEND ENABLED_OUTPUT " netbsd")
    ENDIF()

    IF (WANT_SNDIO)
        FIND_PACKAGE(SNDIO REQUIRED)
        SET(AUDIODRV_SNDIO 1)
        LIST(APPEND AUDIO_LIBRARY ${SNDIO_LIBRARIES})
        STRING(APPEND ENABLED_OUTPUT " sndio")
    ENDIF()

    IF (WANT_WINMM)
        SET(AUDIODRV_WINMM 1)
        LIST(APPEND AUDIO_LIBRARY winmm)
        STRING(APPEND ENABLED_OUTPUT " winmm")
    ENDIF()

    STRING(STRIP ${ENABLED_OUTPUT} ENABLED_OUTPUT)
    STRING(APPEND ENABLED_OUTPUT " wave")
    MESSAGE(STATUS "Enabled audio output backends: ${ENABLED_OUTPUT}")
ENDIF()

# Setup up our config file
CONFIGURE_FILE("${PROJECT_SOURCE_DIR}/include/config.h.cmake" "${PROJECT_BINARY_DIR}/include/config.h")

ADD_SUBDIRECTORY(src)
