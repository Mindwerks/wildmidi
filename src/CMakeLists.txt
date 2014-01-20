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

SET_TARGET_PROPERTIES(wildmidi_dynamic PROPERTIES OUTPUT_NAME ${LIBRARY_DYN_NAME}-${WILDMIDI_VERSION} CLEAN_DIRECT_OUTPUT 1)

# Set our default and then look at the possible locations
SET(WILDMIDILIB "${CMAKE_BINARY_DIR}/lib${LIBRARY_DYN_NAME}.so")
IF(WILDMIDI_STATIC)
    SET(WILDMIDILIB "${CMAKE_BINARY_DIR}/lib${LIBRARY_STATIC_NAME}.a")
ENDIF(WILDMIDI_STATIC)

IF(MSVC)
    SET(GETOPT getopt.c getopt_long.c)
    SET(WILDMIDILIB "${CMAKE_BINARY_DIR}\\${CMAKE_BUILD_TYPE}\\${LIBRARY_DYN_NAME}.lib")
    IF(WILDMIDI_STATIC)
        SET(WILDMIDILIB "${CMAKE_BINARY_DIR}\\${CMAKE_BUILD_TYPE}\\${LIBRARY_STATIC_NAME}.lib")
    ENDIF(WILDMIDI_STATIC)
ELSEIF(CMAKE_GENERATOR STREQUAL "Xcode")
    SET(WILDMIDILIB "${CMAKE_BINARY_DIR}/${CMAKE_BUILD_TYPE}/libwildmidi.dylib") 
    IF(WILDMIDI_STATIC)
        SET(WILDMIDILIB "${CMAKE_BINARY_DIR}/${CMAKE_BUILD_TYPE}/libwildmidi.a") 
    ENDIF(WILDMIDI_STATIC)
ENDIF(MSVC)

# Setup our wildmidi player
SET(wildmidi_executable_SRCS
    ${GETOPT}
    wildmidi.c
)

# create our symlinks
SET(legacy_target "lib${LIBRARY_DYN_NAME}-${WILDMIDI_VERSION}.so")
SET(legacy_link "lib${LIBRARY_DYN_NAME}.so")
EXECUTE_PROCESS(COMMAND ln -sf ${legacy_target} ${legacy_link})
SET(legacy_link "lib${LIBRARY_DYN_NAME}.so.1")
EXECUTE_PROCESS(COMMAND ln -sf ${legacy_target} ${legacy_link})

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
IF (UNIX AND NOT APPLE)
INSTALL(TARGETS wildmidi_static DESTINATION lib)
INSTALL(TARGETS wildmidi_dynamic DESTINATION lib)
INSTALL(FILES ${CMAKE_BINARY_DIR}/libwildmidi.so DESTINATION lib)
INSTALL(FILES ${CMAKE_BINARY_DIR}/libwildmidi.so.1 DESTINATION lib)      
INSTALL(TARGETS wildmidi DESTINATION bin)
INSTALL(FILES ${CMAKE_SOURCE_DIR}/include/wildmidi_lib.h DESTINATION include)
INSTALL(DIRECTORY ${CMAKE_SOURCE_DIR}/docs/man/ DESTINATION share/man)
ENDIF (UNIX AND NOT APPLE)