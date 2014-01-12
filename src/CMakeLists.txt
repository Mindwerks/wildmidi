# Setup up our config file
configure_file ("${CMAKE_SOURCE_DIR}/include/config.h.cmake" "${CMAKE_CURRENT_SOURCE_DIR}/config.h")

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

# actual target:
ADD_LIBRARY(wildmidi_static STATIC 
    ${wildmidi_library_SRCS}
    )

TARGET_LINK_LIBRARIES(wildmidi_static 
    ${ALSA_LIBRARY}
    ${OSS_LIBRARY}
    ${M_LIBRARY}
    )

SET_TARGET_PROPERTIES(wildmidi_static PROPERTIES OUTPUT_NAME wildmidi CLEAN_DIRECT_OUTPUT 1)

ADD_LIBRARY(wildmidi_dynamic SHARED
    ${wildmidi_library_SRCS}
    )

TARGET_LINK_LIBRARIES(wildmidi_dynamic 
    ${ALSA_LIBRARY}
    ${OSS_LIBRARY}
    ${M_LIBRARY}
    )

SET_TARGET_PROPERTIES(wildmidi_dynamic PROPERTIES OUTPUT_NAME wildmidi CLEAN_DIRECT_OUTPUT 1)


# Setup our wildmidi player
SET(wildmidi_executable_SRCS
    wildmidi.c
)

ADD_EXECUTABLE(wildmidi 
    ${wildmidi_executable_SRCS}
)

ADD_DEPENDENCIES(wildmidi wildmidi_static)

TARGET_LINK_LIBRARIES(wildmidi 
    ${CMAKE_CURRENT_BINARY_DIR}/libwildmidi.a 
    ${M_LIBRARY}
    ${ALSA_LIBRARY}
    )

# add install target:
INSTALL(TARGETS wildmidi DESTINATION bin)