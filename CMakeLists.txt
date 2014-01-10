# ########## Project setup ##########
PROJECT(wildmidi)
CMAKE_MINIMUM_REQUIRED(VERSION 2.8)

# if linux
find_package(ALSA REQUIRED)
add_definitions( -DHAVE_ALSA=1 )
add_definitions( -std=c99 -D_POSIX_C_SOURCE=200809L -pedantic -Wall -W )
add_definitions( -static  )

# ######### General setup ##########
INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR} include ${ALSA_INCLUDE_DIR})

add_subdirectory(src)
