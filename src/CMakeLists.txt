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
 ../include/wm_tty.h
 ../include/wildplay.h
)

# set our library names
IF (MSVC) # windows uses *.lib for both static and dynamic, workaround
    SET(LIBRARY_DYN_NAME "libWildMidi")
    SET(LIBRARY_STATIC_NAME "libWildMidi-static")
ELSE () # everyone else uses .a and .so
    SET(LIBRARY_DYN_NAME "WildMidi")
    SET(LIBRARY_STATIC_NAME "WildMidi")
ENDIF()

ADD_LIBRARY(libwildmidi-static STATIC
    ${wildmidi_library_SRCS}
    ${wildmidi_library_HDRS}
)

SET_TARGET_PROPERTIES(libwildmidi-static PROPERTIES
    OUTPUT_NAME ${LIBRARY_STATIC_NAME} CLEAN_DIRECT_OUTPUT 1
    COMPILE_DEFINITIONS WILDMIDI_BUILD
)
TARGET_COMPILE_DEFINITIONS(libwildmidi-static INTERFACE
    WILDMIDI_STATIC
)
TARGET_INCLUDE_DIRECTORIES(libwildmidi-static INTERFACE
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# If the static library was not requested, we do not add it to the "all" & "install" targets
IF (WANT_STATIC)
    LIST(APPEND wildmidi_lib_install libwildmidi-static)
ELSE ()
    SET_TARGET_PROPERTIES(libwildmidi-static PROPERTIES EXCLUDE_FROM_ALL ON)
ENDIF()

IF (BUILD_SHARED_LIBS)
    # dynamic library
    ADD_LIBRARY(libwildmidi SHARED
        ${wildmidi_library_SRCS}
        ${wildmidi_library_HDRS}
    )
    TARGET_LINK_LIBRARIES(libwildmidi
        ${EXTRA_LDFLAGS}
        ${M_LIBRARY}
    )
    SET_TARGET_PROPERTIES(libwildmidi PROPERTIES
        SOVERSION ${SOVERSION}
        VERSION ${VERSION}
        OUTPUT_NAME ${LIBRARY_DYN_NAME} CLEAN_DIRECT_OUTPUT 1
    )

    IF (WIN32)
        SET_TARGET_PROPERTIES(libwildmidi PROPERTIES
            DEFINE_SYMBOL DLL_EXPORT
            COMPILE_DEFINITIONS WILDMIDI_BUILD
        )
    ELSEIF (HAVE_VISIBILITY_HIDDEN AND HAVE_VISIBILITY_DEFAULT) # GCC, Clang
        SET_TARGET_PROPERTIES(libwildmidi PROPERTIES
            COMPILE_DEFINITIONS "WILDMIDI_BUILD;SYM_VISIBILITY"
            COMPILE_FLAGS "-fvisibility=hidden"
        )
    #ELSEIF (HAVE_LDSCOPE_HIDDEN AND HAVE_VISIBILITY_DEFAULT) # SunPro (__SUNPRO_C >= 0x590)
    #   SET_TARGET_PROPERTIES(libwildmidi PROPERTIES
    #       COMPILE_DEFINITIONS "WILDMIDI_BUILD;SYM_VISIBILITY"
    #       COMPILE_FLAGS "-xldscope=hidden"
    #   )
    ELSEIF (HAVE_LDSCOPE_HIDDEN AND HAVE_LDSCOPE_GLOBAL) # SunPro (__SUNPRO_C >= 0x550)
        SET_TARGET_PROPERTIES(libwildmidi PROPERTIES
            COMPILE_DEFINITIONS "WILDMIDI_BUILD;SYM_LDSCOPE"
            COMPILE_FLAGS "-xldscope=hidden"
        )
    ELSE ()
        SET_TARGET_PROPERTIES(libwildmidi PROPERTIES
            COMPILE_DEFINITIONS WILDMIDI_BUILD
        )
    ENDIF()
    LIST(APPEND wildmidi_lib_install libwildmidi)
ENDIF()

IF (WANT_DEVTEST)
    SET(wildmidi-devtest_executable_SRCS
        DevTest.c
    )
    IF (MSVC)
        LIST(APPEND wildmidi-devtest_executable_SRCS getopt_long.c)
    ENDIF()
    ADD_EXECUTABLE(wildmidi-devtest
        ${wildmidi-devtest_executable_SRCS}
    )
    LIST(APPEND wildmidi_install wildmidi-devtest)
ENDIF()

# prepare pkg-config file
CONFIGURE_FILE("wildmidi.pc.in" "${PROJECT_BINARY_DIR}/wildmidi.pc" @ONLY)

INSTALL(TARGETS ${wildmidi_lib_install}
    EXPORT WildMidi-export
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
INSTALL(TARGETS ${wildmidi_install}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

INSTALL(FILES ${PROJECT_BINARY_DIR}/wildmidi.pc DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig")
INSTALL(FILES ${PROJECT_SOURCE_DIR}/include/wildmidi_lib.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
INSTALL(DIRECTORY ${PROJECT_SOURCE_DIR}/docs/man/ DESTINATION ${CMAKE_INSTALL_MANDIR})
INSTALL(EXPORT WildMidi-export FILE WildMidiTargets.cmake
    NAMESPACE WildMidi:: DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/WildMidi
)

INCLUDE(CMakePackageConfigHelpers)
CONFIGURE_PACKAGE_CONFIG_FILE(
    "${PROJECT_SOURCE_DIR}/src/WildMidiConfig.cmake.in"
    "${PROJECT_BINARY_DIR}/WildMidiConfig.cmake"
    INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/WildMidi"
    NO_SET_AND_CHECK_MACRO
    NO_CHECK_REQUIRED_COMPONENTS_MACRO
)

WRITE_BASIC_PACKAGE_VERSION_FILE(
    "${PROJECT_BINARY_DIR}/WildMidiConfigVersion.cmake"
    VERSION "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}"
    COMPATIBILITY AnyNewerVersion
)

INSTALL(FILES
    "${PROJECT_BINARY_DIR}/WildMidiConfig.cmake"
    "${PROJECT_BINARY_DIR}/WildMidiConfigVersion.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/WildMidi"
)

IF (WIN32 AND MSVC)
    IF (WANT_MP_BUILD)
        SET(MT_BUILD "/MP")
    ENDIF()
ENDIF()

IF (WANT_PLAYER OR WANT_PLAYERSTATIC)
    ADD_SUBDIRECTORY(player)
ENDIF()
