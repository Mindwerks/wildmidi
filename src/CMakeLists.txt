# Setup our wildmidi library that we link to
SET(wildmidi_library_SRCS
    wm_error.c
    file_io.c
    lock.c
    wildmidi_lib.c
    reverb.c
    gus_pat.c
    internal_midi.c
    patches.c
    f_xmidi.c
    f_mus.c
    f_hmp.c
    f_midi.c
    f_hmi.c
    sample.c
    mus2mid.c
    xmi2mid.c
)

SET(wildmidi_library_HDRS
    ../include/wm_error.h
    ../include/file_io.h
    ../include/lock.h
    ../include/wildmidi_lib.h
    ../include/reverb.h
    ../include/gus_pat.h
    ../include/f_xmidi.h
    ../include/f_mus.h
    ../include/f_hmp.h
    ../include/f_midi.h
    ../include/f_hmi.h
    ../include/internal_midi.h
    ../include/patches.h
    ../include/sample.h
    ../include/common.h
    ../include/filenames.h
    ../include/mus2mid.h
    ../include/xmi2mid.h
)

IF(MSVC)
   SET(GETOPT getopt_long.c)
ELSE()
   SET(GETOPT)
ENDIF()
SET(wildmidi_executable_SRCS
    ${GETOPT}
    wm_tty.c
    wildmidi.c
)

SET(wildmidi_executable_HDRS
    ../include/wm_tty.h
    ../include/getopt_long.h
)

# set our target paths
SET(EXECUTABLE_OUTPUT_PATH "${CMAKE_BINARY_DIR}")
SET(LIBRARY_OUTPUT_PATH "${CMAKE_BINARY_DIR}")

# set our library names
IF(WIN32 AND NOT CMAKE_COMPILER_IS_MINGW) # windows uses *.lib for both static and dynamic, workaround
    SET(LIBRARY_DYN_NAME "wildmidi_dynamic")
    SET(LIBRARY_STATIC_NAME "wildmidi_static")
ELSE() # everyone else uses .a and .so
    SET(LIBRARY_DYN_NAME "WildMidi")
    SET(LIBRARY_STATIC_NAME "WildMidi")
ENDIF()

# do we want a static library?
IF(WANT_STATIC)
    ADD_LIBRARY(libwildmidi_static STATIC
        ${wildmidi_library_SRCS}
        ${wildmidi_library_HDRS}
        )

    TARGET_LINK_LIBRARIES(libwildmidi_static
        ${M_LIBRARY}
        )

    SET_TARGET_PROPERTIES(libwildmidi_static PROPERTIES
        OUTPUT_NAME ${LIBRARY_STATIC_NAME} CLEAN_DIRECT_OUTPUT 1
        COMPILE_DEFINITIONS WILDMIDI_BUILD
        )
ENDIF(WANT_STATIC)

# always build our dynamic library
ADD_LIBRARY(libwildmidi_dynamic SHARED
    ${wildmidi_library_SRCS}
    ${wildmidi_library_HDRS}
    )

TARGET_LINK_LIBRARIES(libwildmidi_dynamic
    ${M_LIBRARY}
    )

SET_TARGET_PROPERTIES(libwildmidi_dynamic PROPERTIES
    SOVERSION ${SOVERSION}
    VERSION ${VERSION}
    OUTPUT_NAME ${LIBRARY_DYN_NAME} CLEAN_DIRECT_OUTPUT 1
    )

IF(WIN32)
    SET_TARGET_PROPERTIES(libwildmidi_dynamic PROPERTIES
        DEFINE_SYMBOL DLL_EXPORT
        COMPILE_DEFINITIONS WILDMIDI_BUILD
    )
ELSEIF(HAVE_VISIBILITY_HIDDEN AND HAVE_VISIBILITY_DEFAULT) # GCC, Clang
    SET_TARGET_PROPERTIES(libwildmidi_dynamic PROPERTIES
        COMPILE_DEFINITIONS "WILDMIDI_BUILD;SYM_VISIBILITY"
        COMPILE_FLAGS "-fvisibility=hidden"
    )
#ELSEIF(HAVE_LDSCOPE_HIDDEN AND HAVE_VISIBILITY_DEFAULT) # SunPro (__SUNPRO_C >= 0x590)
#   SET_TARGET_PROPERTIES(libwildmidi_dynamic PROPERTIES
#       COMPILE_DEFINITIONS "WILDMIDI_BUILD;SYM_VISIBILITY"
#       COMPILE_FLAGS "-xldscope=hidden"
#   )
ELSEIF(HAVE_LDSCOPE_HIDDEN AND HAVE_LDSCOPE_GLOBAL) # SunPro (__SUNPRO_C >= 0x550)
    SET_TARGET_PROPERTIES(libwildmidi_dynamic PROPERTIES
        COMPILE_DEFINITIONS "WILDMIDI_BUILD;SYM_LDSCOPE"
        COMPILE_FLAGS "-xldscope=hidden"
    )
ELSE()
    SET_TARGET_PROPERTIES(libwildmidi_dynamic PROPERTIES
        COMPILE_DEFINITIONS WILDMIDI_BUILD
    )
ENDIF()

# Set our default and then look at the possible locations
SET(WILDMIDILIB "${CMAKE_BINARY_DIR}/lib${LIBRARY_DYN_NAME}.so")
SET(WILDMIDILIBSTATIC "${CMAKE_BINARY_DIR}/lib${LIBRARY_STATIC_NAME}.a")

# MS Visual Studio
IF(MSVC)
    SET(WILDMIDILIB "${CMAKE_BINARY_DIR}\\${CMAKE_BUILD_TYPE}\\${LIBRARY_DYN_NAME}.lib")
    SET(WILDMIDILIBSTATIC "${CMAKE_BINARY_DIR}\\${CMAKE_BUILD_TYPE}\\${LIBRARY_STATIC_NAME}.lib")
ENDIF(MSVC)

# MinGW or MinGW-w64
IF(CMAKE_COMPILER_IS_MINGW)
    SET(WILDMIDILIB "${CMAKE_BINARY_DIR}\\lib${LIBRARY_DYN_NAME}.dll.a")
    SET(WILDMIDILIBSTATIC "${CMAKE_BINARY_DIR}\\lib${LIBRARY_STATIC_NAME}.a")
ENDIF(CMAKE_COMPILER_IS_MINGW)

# Apple's Xcode
IF(CMAKE_GENERATOR STREQUAL "Xcode")
    SET(WILDMIDILIB "${CMAKE_BINARY_DIR}/${CMAKE_BUILD_TYPE}/lib${LIBRARY_DYN_NAME}.dylib")
    SET(WILDMIDILIBSTATIC "${CMAKE_BINARY_DIR}/${CMAKE_BUILD_TYPE}/lib${LIBRARY_STATIC_NAME}.a")
ELSEIF(APPLE) # Apple's CLI default
    SET(WILDMIDILIB "${CMAKE_BINARY_DIR}/lib${LIBRARY_DYN_NAME}.dylib")
    SET(WILDMIDILIBSTATIC "${CMAKE_BINARY_DIR}/lib${LIBRARY_STATIC_NAME}.a")
ENDIF(CMAKE_GENERATOR STREQUAL "Xcode")

# do we want the wildmidi player?
IF(WANT_PLAYER)
    ADD_EXECUTABLE(wildmidi
        ${wildmidi_executable_SRCS}
        ${wildmidi_executable_HDRS}
    )

    ADD_DEPENDENCIES(wildmidi libwildmidi_dynamic)

    IF(AUDIODRV_OPENAL)
      TARGET_INCLUDE_DIRECTORIES(wildmidi PRIVATE
        ${OPENAL_INCLUDE_DIR}
        )
    ELSEIF(AUDIODRV_ALSA)
      TARGET_INCLUDE_DIRECTORIES(wildmidi PRIVATE
        ${ALSA_INCLUDE_DIR}
        )
    ELSEIF(AUDIODRV_OSS)
     # no special header paths
    ENDIF()

    TARGET_LINK_LIBRARIES(wildmidi
        ${WILDMIDILIB}
        ${M_LIBRARY}
        ${AUDIO_LIBRARY}
        )
ENDIF(WANT_PLAYER)

IF(WANT_PLAYERSTATIC)
    ADD_EXECUTABLE(wildmidi-static
        ${wildmidi_executable_SRCS}
        ${wildmidi_executable_HDRS}
    )

    ADD_DEPENDENCIES(wildmidi-static libwildmidi_static)

    SET_TARGET_PROPERTIES(wildmidi-static PROPERTIES
        COMPILE_DEFINITIONS WILDMIDI_STATIC
    )

    IF(AUDIODRV_OPENAL)
      TARGET_INCLUDE_DIRECTORIES(wildmidi-static PRIVATE
        ${OPENAL_INCLUDE_DIR}
        )
    ELSEIF(AUDIODRV_ALSA)
      TARGET_INCLUDE_DIRECTORIES(wildmidi-static PRIVATE
        ${ALSA_INCLUDE_DIR}
        )
    ELSEIF(AUDIODRV_OSS)
     # no special header paths
    ENDIF()

    TARGET_LINK_LIBRARIES(wildmidi-static
        ${WILDMIDILIBSTATIC}
        ${M_LIBRARY}
        ${AUDIO_LIBRARY}
        )
ENDIF(WANT_PLAYERSTATIC)

IF(WANT_DEVTEST)
    SET(wildmidi-devtest_executable_SRCS
        DevTest.c
    )
    ADD_EXECUTABLE(wildmidi-devtest
        ${wildmidi-devtest_executable_SRCS}
    )
ENDIF(WANT_DEVTEST)

# add install target:
IF(UNIX AND NOT APPLE)
    # install our libraries
    IF(WANT_STATIC)
        INSTALL(TARGETS libwildmidi_static DESTINATION "lib${LIB_SUFFIX}/${CMAKE_LIBRARY_ARCHITECTURE}")
    ENDIF(WANT_STATIC)
    INSTALL(TARGETS libwildmidi_dynamic DESTINATION "lib${LIB_SUFFIX}/${CMAKE_LIBRARY_ARCHITECTURE}")

    # install our player if asked for
    IF(WANT_PLAYER)
        INSTALL(TARGETS wildmidi DESTINATION bin)
    ENDIF(WANT_PLAYER)
    IF(WANT_PLAYERSTATIC)
        INSTALL(TARGETS wildmidi-static DESTINATION bin)
    ENDIF(WANT_PLAYERSTATIC)

    # install our devtest if asked for
    IF(WANT_DEVTEST)
        INSTALL(TARGETS wildmidi-devtest DESTINATION bin)
    ENDIF(WANT_DEVTEST)

    # install supporting man pages and headers
    INSTALL(FILES ${CMAKE_SOURCE_DIR}/include/wildmidi_lib.h DESTINATION include)
    INSTALL(DIRECTORY ${CMAKE_SOURCE_DIR}/docs/man/ DESTINATION share/man)
ENDIF(UNIX AND NOT APPLE)

