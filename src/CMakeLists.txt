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
    ADD_LIBRARY(wildmidi_static STATIC 
        ${wildmidi_library_SRCS}
        )

    TARGET_LINK_LIBRARIES(wildmidi_static 
        ${M_LIBRARY}
        )

    SET_TARGET_PROPERTIES(wildmidi_static PROPERTIES 
        OUTPUT_NAME ${LIBRARY_STATIC_NAME} CLEAN_DIRECT_OUTPUT 1
        COMPILE_DEFINITIONS WILDMIDI_BUILD
        )
ENDIF(WANT_STATIC)

# always build our dynamic library
ADD_LIBRARY(wildmidi_dynamic SHARED
    ${wildmidi_library_SRCS}
    )

TARGET_LINK_LIBRARIES(wildmidi_dynamic 
    ${M_LIBRARY}
    )

SET_TARGET_PROPERTIES(wildmidi_dynamic PROPERTIES 
    SOVERSION ${SOVERSION}
    VERSION ${VERSION}
    OUTPUT_NAME ${LIBRARY_DYN_NAME} CLEAN_DIRECT_OUTPUT 1
    )

IF(WIN32)
    SET_TARGET_PROPERTIES(wildmidi_dynamic PROPERTIES 
        DEFINE_SYMBOL DLL_EXPORT
        COMPILE_DEFINITIONS WILDMIDI_BUILD
    )
ELSEIF(HAVE_VISIBILITY_HIDDEN AND HAVE_VISIBILITY_DEFAULT) # GCC, Clang
    SET_TARGET_PROPERTIES(wildmidi_dynamic PROPERTIES
        COMPILE_DEFINITIONS "WILDMIDI_BUILD;SYM_VISIBILITY"
        COMPILE_FLAGS "-fvisibility=hidden"
    )
#ELSEIF(HAVE_LDSCOPE_HIDDEN AND HAVE_VISIBILITY_DEFAULT) # SunPro (__SUNPRO_C >= 0x590)
#   SET_TARGET_PROPERTIES(wildmidi_dynamic PROPERTIES
#       COMPILE_DEFINITIONS "WILDMIDI_BUILD;SYM_VISIBILITY"
#       COMPILE_FLAGS "-xldscope=hidden"
#   )
ELSEIF(HAVE_LDSCOPE_HIDDEN AND HAVE_LDSCOPE_GLOBAL) # SunPro (__SUNPRO_C >= 0x550)
    SET_TARGET_PROPERTIES(wildmidi_dynamic PROPERTIES
        COMPILE_DEFINITIONS "WILDMIDI_BUILD;SYM_LDSCOPE"
        COMPILE_FLAGS "-xldscope=hidden"
    )
ELSE()
    SET_TARGET_PROPERTIES(wildmidi_dynamic PROPERTIES
        COMPILE_DEFINITIONS WILDMIDI_BUILD
    )
ENDIF()

# Set our default and then look at the possible locations
SET(WILDMIDILIB "${CMAKE_BINARY_DIR}/lib${LIBRARY_DYN_NAME}.so")

# MS Visual Studio
IF(MSVC)
    SET(GETOPT getopt_long.c)
    SET(WILDMIDILIB "${CMAKE_BINARY_DIR}\\${CMAKE_BUILD_TYPE}\\${LIBRARY_DYN_NAME}.lib")
ENDIF(MSVC)

# MinGW or MinGW-w64
IF(CMAKE_COMPILER_IS_MINGW)
    SET(WILDMIDILIB "${CMAKE_BINARY_DIR}\\lib${LIBRARY_DYN_NAME}.dll.a")
ENDIF(CMAKE_COMPILER_IS_MINGW)

# Apple's Xcode
IF(CMAKE_GENERATOR STREQUAL "Xcode")
    SET(WILDMIDILIB "${CMAKE_BINARY_DIR}/${CMAKE_BUILD_TYPE}/lib${LIBRARY_DYN_NAME}.dylib")
ELSEIF(APPLE) # Apple's CLI default
    SET(WILDMIDILIB "${CMAKE_BINARY_DIR}/lib${LIBRARY_DYN_NAME}.dylib")
ENDIF(CMAKE_GENERATOR STREQUAL "Xcode")

# do we want the wildmidi player?
IF(WANT_PLAYER)
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
        ${AUDIO_LIBRARY}
        )
ENDIF(WANT_PLAYER)

# add install target:
IF(UNIX AND NOT APPLE)
    # install our libraries
    IF(WANT_STATIC)
        INSTALL(TARGETS wildmidi_static DESTINATION "lib/${CMAKE_LIBRARY_ARCHITECTURE}")
    ENDIF(WANT_STATIC)
    INSTALL(TARGETS wildmidi_dynamic DESTINATION "lib/${CMAKE_LIBRARY_ARCHITECTURE}")

    # install our player if asked for
    IF(WANT_PLAYER)
        INSTALL(TARGETS wildmidi DESTINATION bin)
    ENDIF(WANT_PLAYER)

    # install supporting man pages and headers
    INSTALL(FILES ${CMAKE_SOURCE_DIR}/include/wildmidi_lib.h DESTINATION include)
    INSTALL(DIRECTORY ${CMAKE_SOURCE_DIR}/docs/man/ DESTINATION share/man)
ENDIF(UNIX AND NOT APPLE)
