# ########## Project setup ##########
PROJECT(wildmidi)
CMAKE_MINIMUM_REQUIRED(VERSION 2.8)

# WildMIDI Version
SET(VERSION_MAJOR 0)
SET(VERSION_MINOR 4)
SET(VERSION_RELEASE dev)
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
OPTION(WANT_OPENAL      "Include OpenAL suport (Cross Platform) support" OFF)

# Compiler specific settings
IF(CMAKE_COMPILER_IS_GNUCC)
    ADD_DEFINITIONS( 
        -std=c99 
        -D_POSIX_C_SOURCE=200809L 
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
    SET(AUDIO_FOUND FALSE)
    SET(AUDIO_LIBRARY "")
    # Go looking for available sound packages for WildMIDI player
    IF(WANT_PLAYER) # Set our preferred output
    
        IF(WANT_ALSA)
            FIND_PACKAGE(ALSA REQUIRED)
            SET(AUDIO_LIBRARY ${ALSA_LIBRARY})
        ELSEIF(WANT_OSS)
            FIND_PACKAGE(OSS REQUIRED)
            SET(AUDIO_LIBRARY ${OSS_LIBRARY})
        ELSEIF(WANT_OPENAL)
            FIND_PACKAGE(OpenAL REQUIRED)
            SET(AUDIO_LIBRARY ${OPENAL_LIBRARY})
            
        ELSE(WANT_ALSA) # Try to auto-detect
        
            FIND_PACKAGE(ALSA)
            IF(ALSA_FOUND AND NOT AUDIO_FOUND)
                SET(AUDIO_LIBRARY ${ALSA_LIBRARY})
                SET(AUDIO_FOUND TRUE)
            ENDIF(ALSA_FOUND AND NOT AUDIO_FOUND)
            
            FIND_PACKAGE(OSS)
            IF(OSS_FOUND AND NOT AUDIO_FOUND)
                SET(AUDIO_LIBRARY ${OSS_LIBRARY})
                SET(AUDIO_FOUND TRUE)
            ENDIF(OSS_FOUND AND NOT AUDIO_FOUND)
            
            FIND_PACKAGE(OpenAL)
            IF(OPENAL_FOUND AND NOT AUDIO_FOUND)
                SET(AUDIO_LIBRARY ${OPENAL_LIBRARY})
                SET(AUDIO_FOUND TRUE)
            ENDIF(OPENAL_FOUND AND NOT AUDIO_FOUND)
            
            IF(NOT AUDIO_FOUND)
                MESSAGE(WARNING "Could not find an audio sub-system!")
            ENDIF(NOT AUDIO_FOUND)
            
        ENDIF(WANT_ALSA)
    ENDIF(WANT_PLAYER)
    
    # find our math lib
    FIND_LIBRARY(M_LIBRARY m REQUIRED)
    
    # set our default linux/*bsd config
    SET(WILDMIDI_CFG "/etc/wildmidi/wildmidi.cfg")
    
    ## Debian and non debian Linux building
    IF (DPKG_PROGRAM)
        ## Debian specific
    ELSE ()
        ## Non debian specific
    ENDIF (DPKG_PROGRAM)
ENDIF(NOT WIN32 AND NOT APPLE)

IF (APPLE AND WANT_PLAYER)
    IF(WANT_OPENAL)
        FIND_PACKAGE(OpenAL REQUIRED)
        SET(AUDIO_LIBRARY ${OPENAL_LIBRARY})
    ELSE(WANT_OPENAL)
        FIND_PACKAGE(OpenAL)
        SET(AUDIO_LIBRARY ${OPENAL_LIBRARY})    
    ENDIF(WANT_OPENAL)
ENDIF (APPLE AND WANT_PLAYER)

IF (WIN32)
    LINK_LIBRARIES(winmm)
ENDIF (WIN32)

# OpenAL support (multi-platform)
IF (WANT_PLAYER AND OPENAL_FOUND)
    SET(HAVE_OPENAL_H 1)
    INCLUDE_DIRECTORIES(${OPENAL_INCLUDE_DIR})
ENDIF (WANT_PLAYER AND OPENAL_FOUND)

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
