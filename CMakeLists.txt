# ########## Project setup ##########
PROJECT(wildmidi)
CMAKE_MINIMUM_REQUIRED(VERSION 2.8)

# Version
set (WILDMIDI_VERSION_MAJOR 0)
set (WILDMIDI_VERSION_MINOR 3)
set (WILDMIDI_VERSION_RELEASE 0)
set (WILDMIDI_VERSION "${WILDMIDI_VERSION_MAJOR}.${WILDMIDI_VERSION_MINOR}.${WILDMIDI_VERSION_RELEASE}")

# Find Macros
set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)

# Compiler specific settings
if(CMAKE_COMPILER_IS_GNUCC)
    add_definitions( -std=c99 -D_POSIX_C_SOURCE=200809L -pedantic -Wall -W )
ENDIF(CMAKE_COMPILER_IS_GNUCC)

# default config locaiton
SET(WILDMIDI_CFG "wildmidi.cfg")

# UNIX-like environments
IF(NOT WIN32 AND NOT APPLE)

    # go looking for available sound packages
    find_package(ALSA)
    if(ALSA_FOUND)
        add_definitions( -DHAVE_ALSA=1 )
    else(ALSA_FOUND)
        find_package(OSS)
        if(OSS_FOUND)
            add_definitions( -DHAVE_LINUX_SOUNDCARD_H=1 )
        endif()
    endif()
    
    # find our math lib
    find_library(M_LIBRARY m)
    
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
   FIND_LIBRARY(COREAUDIO_LIBRARY CoreAudio)
   FIND_LIBRARY(COREFOUNDATION_LIBRARY CoreFoundation )
ENDIF (APPLE)

IF (WIN32)
    TARGET_LINK_LIBRARIES(winmm)
ENDIF (WIN32)

#add_definitions( -static  )

# ######### General setup ##########
INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR} include ${ALSA_INCLUDE_DIR})

add_subdirectory(src)