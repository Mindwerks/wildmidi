# ########## Project setup ##########
PROJECT(wildmidi)
CMAKE_MINIMUM_REQUIRED(VERSION 2.8)

# WildMIDI Version
SET(VERSION_MAJOR 0)
SET(VERSION_MINOR 3)
SET(VERSION_RELEASE 3)
SET(WILDMIDI_VERSION "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_RELEASE}")

# Lib Versions
SET(SOVERSION 1)
SET(VERSION 1.0.2)

# Find Macros
SET(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)

# Set a default build type if none was specified
IF(NOT DEFINED CMAKE_BUILD_TYPE OR CMAKE_BUILD_TYPE STREQUAL "")
  MESSAGE(STATUS "Setting build type to 'Debug' as none was specified.")
  SET(CMAKE_BUILD_TYPE Debug CACHE STRING "Choose the type of build." FORCE)
  # Set the possible values of build type for cmake-gui
  SET_PROPERTY(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release"
    "MinSizeRel" "RelWithDebInfo")
ENDIF()
SET(CMAKE_CONFIGURATION_TYPES "${CMAKE_BUILD_TYPE}")
MESSAGE(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")

# Set our optoins
OPTION(WANT_PLAYER      "Build WildMIDI player in addition to the libraries" ON)
OPTION(WANT_STATIC      "Build static library in addition to dynamic library" OFF)
OPTION(WANT_ALSA        "Include ALSA (Advanced Linux Sound Architecture) support" OFF)
OPTION(WANT_OSS         "Include OSS (Open Sound System) support" OFF)
OPTION(WANT_COREAUDIO   "Include CoreAudio support (Driver support for Mac OS X" OFF)

# Compiler specific settings
IF(CMAKE_COMPILER_IS_GNUCC)
    ADD_DEFINITIONS( 
        -std=c99 
        -D_POSIX_C_SOURCE=200809L 
        -D_BSD_SOURCE=1
        -pedantic
        -fvisibility=hidden
        -Wall -Wextra -W
        )
    
    IF (CMAKE_BUILD_TYPE STREQUAL "Debug")
        ADD_DEFINITIONS( -ggdb3 -O0 )
    ELSEIF(CMAKE_BUILD_TYPE STREQUAL "Release")
        ADD_DEFINITIONS( -O3 )
    ENDIF(CMAKE_BUILD_TYPE STREQUAL "Debug")
ENDIF(CMAKE_COMPILER_IS_GNUCC)

# default config locaiton
SET(WILDMIDI_CFG "wildmidi.cfg")

# UNIX-like environments
IF(NOT WIN32 AND NOT APPLE)

    SET(AUDIO_LIBRARY "")
    # Go looking for available sound packages for WildMIDI player
    IF(WANT_PLAYER) # Set our preferred output
        IF(WANT_ALSA)
            FIND_PACKAGE(ALSA REQUIRED)
            SET(AUDIO_LIBRARY ${ALSA_LIBRARY})
        ELSEIF(WANT_OSS)
            FIND_PACKAGE(OSS REQUIRED)
        ELSE(WANT_ALSA) # Try to auto-detect
            FIND_PACKAGE(ALSA)
            IF (ALSA_FOUND)
                SET(AUDIO_LIBRARY ${ALSA_LIBRARY})
            ELSE(ALSA_FOUND)
                FIND_PACKAGE(OSS)
                IF (OSS_FOUND)
                    SET(AUDIO_LIBRARY ${OSS_LIBRARY})
                ELSE(OSS_FOUND)
                    MESSAGE(WARNING "Could not find an audio sub-system!")
                ENDIF(OSS_FOUND)
            ENDIF(ALSA_FOUND)
        ENDIF(WANT_ALSA)
    ENDIF(WANT_PLAYER)
    
    # find our math lib
    FIND_LIBRARY(M_LIBRARY m REQUIRED)
    
    # set our default linux config
    SET(WILDMIDI_CFG "/etc/wildmidi/wildmidi.cfg")
    
    ## Debian and non debian Linux building
    IF (DPKG_PROGRAM)
        ## Debian specific
    ELSE ()
        ## Non debian specific
    ENDIF (DPKG_PROGRAM)
ENDIF(NOT WIN32 AND NOT APPLE)

IF (APPLE)
   IF (WANT_PLAYER AND WANT_COREAUDIO)
        FIND_LIBRARY(COREAUDIO_LIBRARY CoreAudio REQUIRED)
        FIND_LIBRARY(COREFOUNDATION_LIBRARY CoreFoundation REQUIRED)
        SET(AUDIO_LIBRARY "${COREAUDIO_LIBRARY} ${COREFOUNDATION_LIBRARY}")
   ENDIF(WANT_PLAYER AND WANT_COREAUDIO)
ENDIF (APPLE)

IF (WIN32)
    IF (WANT_PLAYER)
		LINK_LIBRARIES(winmm)
    ENDIF(WANT_PLAYER)
ENDIF (WIN32)

# ######### General setup ##########
INCLUDE_DIRECTORIES(include) # TODO: temp until we move to another layout

IF (APPLE)
    SET(APP_BUNDLE_NAME "${CMAKE_PROJECT_NAME}.app")
    SET(APP_BUNDLE_DIR "${wildmidi_BINARY_DIR}/${APP_BUNDLE_NAME}")
ENDIF (APPLE)

IF (APPLE)
    SET(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${APP_BUNDLE_DIR}/Contents/MacOS")
ELSE (APPLE)
    SET(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${wildmidi_BINARY_DIR}")
ENDIF (APPLE)

# Setup up our config file
CONFIGURE_FILE ("${CMAKE_SOURCE_DIR}/include/config.h.cmake" "${CMAKE_SOURCE_DIR}/include/config.h")

ADD_SUBDIRECTORY(src)
