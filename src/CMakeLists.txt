# Setup up our config file
configure_file ("${CMAKE_SOURCE_DIR}/include/config.h.cmake" "${CMAKE_SOURCE_DIR}/include/config.h")

# Setup our wildmidi library that we link to
SET(wildmidi_library_SRCS
    wm_error.c
    file_io.c
    lock.c
    wildmidi_lib.c
    reverb.c
    gus_pat.c
)

# Headers:
SET(wildmidi_executable_HDRS)

# set our target paths
SET(EXECUTABLE_OUTPUT_PATH "${CMAKE_BINARY_DIR}")
SET(LIBRARY_OUTPUT_PATH "${CMAKE_BINARY_DIR}")

MESSAGE(STATUS "${CMAKE_BINARY_DIR}")

# actual target:
ADD_LIBRARY(wildmidi_static STATIC 
    ${wildmidi_library_SRCS}
    )

TARGET_LINK_LIBRARIES(wildmidi_static 
    ${ALSA_LIBRARY}
    ${OSS_LIBRARY}
    ${M_LIBRARY}
    ${COREAUDIO_LIBRARY}
    ${COREFOUNDATION_LIBRARY}
    )

IF(WIN32)
    SET(LIBRARY_DYN_NAME "wildmidi_dynamic")
    SET(LIBRARY_STATIC_NAME "wildmidi_static")
ELSE(WIN32)
    SET(LIBRARY_DYN_NAME "wildmidi")
    SET(LIBRARY_STATIC_NAME "wildmidi")
ENDIF(WIN32)

SET_TARGET_PROPERTIES(wildmidi_static PROPERTIES OUTPUT_NAME ${LIBRARY_STATIC_NAME} CLEAN_DIRECT_OUTPUT 1)

ADD_LIBRARY(wildmidi_dynamic SHARED
    ${wildmidi_library_SRCS}
    )

TARGET_LINK_LIBRARIES(wildmidi_dynamic 
    ${ALSA_LIBRARY}
    ${OSS_LIBRARY}
    ${M_LIBRARY}
    ${COREAUDIO_LIBRARY}
    ${COREFOUNDATION_LIBRARY}
    )

SET_TARGET_PROPERTIES(wildmidi_dynamic PROPERTIES OUTPUT_NAME ${LIBRARY_DYN_NAME} CLEAN_DIRECT_OUTPUT 1)


# Set our default and then look at the possible locations
SET(WILDMIDILIB "${CMAKE_BINARY_DIR}/libwildmidi.a")

IF(WIN32)
    SET(GETOPT getopt.c getopt_long.c)
    SET(WILDMIDILIB "${CMAKE_BINARY_DIR}\\${CMAKE_BUILD_TYPE}\\wildmidi_static.lib")
ELSEIF(APPLE AND CMAKE_GENERATOR STREQUAL "Xcode")
    SET(WILDMIDILIB "${CMAKE_BINARY_DIR}/${CMAKE_BUILD_TYPE}/libwildmidi.a")
ELSE(WIN32)
    
ENDIF(WIN32)

# Setup our wildmidi player
SET(wildmidi_executable_SRCS
    ${GETOPT}
    wildmidi.c
)

ADD_EXECUTABLE(wildmidi 
    ${wildmidi_executable_SRCS}
)

ADD_DEPENDENCIES(wildmidi wildmidi_static wildmidi_dynamic)

TARGET_LINK_LIBRARIES(wildmidi 
    ${WILDMIDILIB}
    ${M_LIBRARY}
    ${ALSA_LIBRARY}
    ${COREAUDIO_LIBRARY}
    ${COREFOUNDATION_LIBRARY}
    )

# add install target:
INSTALL(TARGETS wildmidi DESTINATION bin)