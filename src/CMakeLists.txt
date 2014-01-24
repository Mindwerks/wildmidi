# Setup up our config file
CONFIGURE_FILE ("${CMAKE_SOURCE_DIR}/include/config.h.cmake" "${CMAKE_SOURCE_DIR}/include/config.h")

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

INSTALL(TARGETS wildmidi_static DESTINATION lib)

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

SET_TARGET_PROPERTIES(wildmidi_static PROPERTIES 
    OUTPUT_NAME ${LIBRARY_STATIC_NAME} CLEAN_DIRECT_OUTPUT 1
    )

ADD_LIBRARY(wildmidi_dynamic SHARED
    ${wildmidi_library_SRCS}
    )

INSTALL(TARGETS wildmidi_dynamic DESTINATION lib)
INSTALL(FILES ../include/wildmidi_lib.h DESTINATION include)

TARGET_LINK_LIBRARIES(wildmidi_dynamic 
    ${ALSA_LIBRARY}
    ${OSS_LIBRARY}
    ${M_LIBRARY}
    ${COREAUDIO_LIBRARY}
    ${COREFOUNDATION_LIBRARY}
    )

SET_TARGET_PROPERTIES(wildmidi_dynamic PROPERTIES 
    SOVERSION ${SOVERSION} 
    OUTPUT_NAME ${LIBRARY_DYN_NAME} CLEAN_DIRECT_OUTPUT 1
    )


# Set our default and then look at the possible locations
SET(WILDMIDILIB "${CMAKE_BINARY_DIR}/lib${LIBRARY_DYN_NAME}.so")
IF(WANT_STATIC)
    SET(WILDMIDILIB "${CMAKE_BINARY_DIR}/lib${LIBRARY_STATIC_NAME}.a")
ENDIF(WANT_STATIC)

IF(MSVC)
    SET(GETOPT getopt.c getopt_long.c)
    SET(WILDMIDILIB "${CMAKE_BINARY_DIR}\\${CMAKE_BUILD_TYPE}\\${LIBRARY_DYN_NAME}.lib")
    IF(WANT_STATIC)
        SET(WILDMIDILIB "${CMAKE_BINARY_DIR}\\${CMAKE_BUILD_TYPE}\\${LIBRARY_STATIC_NAME}.lib")
    ENDIF(WANT_STATIC)
ELSEIF(CMAKE_GENERATOR STREQUAL "Xcode")
    SET(WILDMIDILIB "${CMAKE_BINARY_DIR}/${CMAKE_BUILD_TYPE}/libwildmidi.dylib") 
    IF(WANT_STATIC)
        SET(WILDMIDILIB "${CMAKE_BINARY_DIR}/${CMAKE_BUILD_TYPE}/libwildmidi.a") 
    ENDIF(WANT_STATIC)
ENDIF(MSVC)

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
    ${ALSA_LIBRARY}
    ${COREAUDIO_LIBRARY}
    ${COREFOUNDATION_LIBRARY}
    )

# add install target:
IF (UNIX AND NOT APPLE)
    # set legacy soversion
    SET(legacy_target "lib${LIBRARY_DYN_NAME}.so.${SOVERSION}")
    SET(legacy_link "lib${LIBRARY_DYN_NAME}.so.1")
    EXECUTE_PROCESS(COMMAND ln -sf ${legacy_target} ${legacy_link})
    INSTALL(FILES ${CMAKE_BINARY_DIR}/${legacy_link} DESTINATION lib)
    
    # install our libraries
    INSTALL(TARGETS wildmidi_static DESTINATION "lib/${CMAKE_LIBRARY_ARCHITECTURE}")
    INSTALL(TARGETS wildmidi_dynamic DESTINATION "lib/${CMAKE_LIBRARY_ARCHITECTURE}")
    
    # install our player if asked for
    IF(WANT_PLAYER)
        INSTALL(TARGETS wildmidi DESTINATION bin)
    ENDIF(WANT_PLAYER)
    
    # install supporting man pages and headers
    INSTALL(FILES ${CMAKE_SOURCE_DIR}/include/wildmidi_lib.h DESTINATION include)
    INSTALL(DIRECTORY ${CMAKE_SOURCE_DIR}/docs/man/ DESTINATION share/man)
ENDIF (UNIX AND NOT APPLE)