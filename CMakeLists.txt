# ########## Project setup ##########
PROJECT(wildmidi)
CMAKE_MINIMUM_REQUIRED(VERSION 2.8)

# Find Macros
set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)

# Compiler specific settings
if(CMAKE_COMPILER_IS_GNUCC)
    add_definitions( -std=c99 -D_POSIX_C_SOURCE=200809L -pedantic -Wall -W )
ENDIF(CMAKE_COMPILER_IS_GNUCC)

# UNIX-like environments
IF(NOT WIN32 AND NOT APPLE)

    # go looking for available sound packages
    find_package(ALSA)
    if(ALSA_FOUND)
        add_definitions( -DHAVE_ALSA=1 )
    else(ALSA_FOUND)
        find_package(OSS)
        if(OSS_FOUND)
            add_definitions( -DHAVE_OSS=1 )
        endif()
    endif()
    
    find_library(M_LIBRARY m)
    
    ## Debian and non debian Linux building
    IF (DPKG_PROGRAM)
        ## Debian specific
    ELSE ()
        ## Non debian specific
    ENDIF (DPKG_PROGRAM)
ENDIF(NOT WIN32 AND NOT APPLE)

#add_definitions( -static  )

# ######### General setup ##########
INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR} include ${ALSA_INCLUDE_DIR})

add_subdirectory(src)