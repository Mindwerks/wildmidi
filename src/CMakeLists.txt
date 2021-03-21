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

SET(wildmidi_executable_SRCS
        wm_tty.c
        wildmidi.c
        )

IF (MSVC)
    LIST(APPEND wildmidi_executable_SRCS getopt_long.c)
ENDIF ()

IF (AMIGA OR AROS)
    LIST(APPEND wildmidi_executable_SRCS amiga.c)
ENDIF ()

SET(wildmidi_executable_HDRS
        ../include/wm_tty.h
        ../include/getopt_long.h
        )

# set our target paths
SET(EXECUTABLE_OUTPUT_PATH "${PROJECT_BINARY_DIR}")
SET(LIBRARY_OUTPUT_PATH "${PROJECT_BINARY_DIR}")

# set our library names
IF (MSVC) # windows uses *.lib for both static and dynamic, workaround
    SET(LIBRARY_DYN_NAME "libWildMidi")
    SET(LIBRARY_STATIC_NAME "libWildMidi-static")
ELSE () # everyone else uses .a and .so
    SET(LIBRARY_DYN_NAME "WildMidi")
    SET(LIBRARY_STATIC_NAME "WildMidi")
ENDIF ()

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
    SET_TARGET_PROPERTIES(libwildmidi-static PROPERTIES
            EXCLUDE_FROM_ALL ON
            )
ENDIF (WANT_STATIC)

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
    #           COMPILE_DEFINITIONS "WILDMIDI_BUILD;SYM_VISIBILITY"
    #           COMPILE_FLAGS "-xldscope=hidden"
    #           )
    ELSEIF (HAVE_LDSCOPE_HIDDEN AND HAVE_LDSCOPE_GLOBAL) # SunPro (__SUNPRO_C >= 0x550)
        SET_TARGET_PROPERTIES(libwildmidi PROPERTIES
                COMPILE_DEFINITIONS "WILDMIDI_BUILD;SYM_LDSCOPE"
                COMPILE_FLAGS "-xldscope=hidden"
                )
    ELSE ()
        SET_TARGET_PROPERTIES(libwildmidi PROPERTIES
                COMPILE_DEFINITIONS WILDMIDI_BUILD
                )
    ENDIF ()
    LIST(APPEND wildmidi_lib_install libwildmidi)
ENDIF (BUILD_SHARED_LIBS)

# do we want the wildmidi player?
IF (WANT_PLAYER AND BUILD_SHARED_LIBS)
    ADD_EXECUTABLE(wildmidi
            ${wildmidi_executable_SRCS}
            ${wildmidi_executable_HDRS}
            )

    IF (AUDIODRV_OPENAL)
        TARGET_INCLUDE_DIRECTORIES(wildmidi PRIVATE
                ${OPENAL_INCLUDE_DIR}
                )
    ELSEIF (AUDIODRV_ALSA)
        TARGET_INCLUDE_DIRECTORIES(wildmidi PRIVATE
                ${ALSA_INCLUDE_DIR}
                )
    ELSEIF (AUDIODRV_OSS)
        # no special header paths
    ENDIF ()

    TARGET_LINK_LIBRARIES(wildmidi
            ${EXTRA_LDFLAGS}
            libwildmidi
            ${AUDIO_LIBRARY}
            ${M_LIBRARY}
            )
    IF (WIN32)
        TARGET_LINK_LIBRARIES(wildmidi winmm)
    ENDIF ()
    LIST(APPEND wildmidi_install wildmidi)
ENDIF ()

IF (WANT_PLAYERSTATIC)
    ADD_EXECUTABLE(wildmidi-static
            ${wildmidi_executable_SRCS}
            ${wildmidi_executable_HDRS}
            )

    SET_TARGET_PROPERTIES(wildmidi-static PROPERTIES
            COMPILE_DEFINITIONS WILDMIDI_STATIC
            )

    IF (AUDIODRV_OPENAL)
        TARGET_INCLUDE_DIRECTORIES(wildmidi-static PRIVATE
                ${OPENAL_INCLUDE_DIR}
                )
    ELSEIF (AUDIODRV_ALSA)
        TARGET_INCLUDE_DIRECTORIES(wildmidi-static PRIVATE
                ${ALSA_INCLUDE_DIR}
                )
    ELSEIF (AUDIODRV_OSS)
        # no special header paths
    ENDIF ()

    TARGET_LINK_LIBRARIES(wildmidi-static
            ${EXTRA_LDFLAGS}
            libwildmidi-static
            ${AUDIO_LIBRARY}
            ${M_LIBRARY}
            )
    IF (WIN32)
        TARGET_LINK_LIBRARIES(wildmidi-static winmm)
    ENDIF ()
    LIST(APPEND wildmidi_install wildmidi-static)
ENDIF (WANT_PLAYERSTATIC)

IF (WANT_DEVTEST)
    SET(wildmidi-devtest_executable_SRCS
            DevTest.c
            )
    IF (MSVC)
        LIST(APPEND wildmidi-devtest_executable_SRCS getopt_long.c)
    ENDIF ()
    ADD_EXECUTABLE(wildmidi-devtest
            ${wildmidi-devtest_executable_SRCS}
            )
    LIST(APPEND wildmidi_install wildmidi-devtest)
ENDIF (WANT_DEVTEST)

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
    ENDIF ()

    foreach (OUTPUTCONFIG ${CMAKE_CONFIGURATION_TYPES})
        STRING(TOUPPER ${OUTPUTCONFIG} OUTPUTCONFIG)
        SET(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${OUTPUTCONFIG} "$(SolutionDir)$(Configuration)")
        SET(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${OUTPUTCONFIG} "$(ProjectDir)$(Configuration)")
    endforeach (OUTPUTCONFIG)

    IF (WANT_PLAYER)
        # Release builds use the debug console
        SET_TARGET_PROPERTIES(wildmidi PROPERTIES LINK_FLAGS_RELEASE "/SUBSYSTEM:CONSOLE")
        SET_TARGET_PROPERTIES(wildmidi PROPERTIES LINK_FLAGS_MINSIZEREL "/SUBSYSTEM:CONSOLE")
        TARGET_COMPILE_DEFINITIONS(wildmidi PRIVATE $<$<CONFIG:Release>:_CONSOLE>)
        TARGET_COMPILE_DEFINITIONS(wildmidi PRIVATE $<$<CONFIG:MinSizeRel>:_CONSOLE>)
    ENDIF ()

    IF (WANT_PLAYERSTATIC)
        # Release builds use the debug console
        SET_TARGET_PROPERTIES(wildmidi-static PROPERTIES LINK_FLAGS_RELEASE "/SUBSYSTEM:CONSOLE")
        SET_TARGET_PROPERTIES(wildmidi-static PROPERTIES LINK_FLAGS_MINSIZEREL "/SUBSYSTEM:CONSOLE")
        TARGET_COMPILE_DEFINITIONS(wildmidi-static PRIVATE $<$<CONFIG:Release>:_CONSOLE>)
        TARGET_COMPILE_DEFINITIONS(wildmidi-static PRIVATE $<$<CONFIG:MinSizeRel>:_CONSOLE>)
    ENDIF ()

    # Play a bit with the warning levels

    SET(WARNINGS "/Wall") # Since windows can only disable specific warnings, not enable them

    SET(WARNINGS_DISABLE
            # Warnings that aren't enabled normally and don't need to be enabled
            # They're unneeded and sometimes completely retarded warnings that /Wall enables
            # Not going to bother commenting them as they tend to warn on every standard library file
            4061 4263 4264 4266 4350 4371 4435 4514 4548 4571 4610 4619 4623 4625 4626 4628 4640 4668 4710 4711 4820 4826 4917 4946

            # Warnings that are thrown on standard libraries
            4347 # Non-template function with same name and parameter count as template function
            4365 # Variable signed/unsigned mismatch
            4510 4512 # Unable to generate copy constructor/assignment operator as it's not public in the base
            4706 # Assignment in conditional expression
            4738 # Storing 32-bit float result in memory, possible loss of performance
            4986 # Undocumented warning that occurs in the crtdbg.h file
            4987 # nonstandard extension used (triggered by setjmp.h)
            4996 # Function was declared deprecated

            # caused by boost
            4191 # 'type cast' : unsafe conversion (1.56, thread_primitives.hpp, normally off)

            # project specific warnings
            4099 # Type mismatch, declared class or struct is defined with other type
            4100 # Unreferenced formal parameter (-Wunused-parameter)
            4101 # Unreferenced local variable (-Wunused-variable)
            4127 # Conditional expression is constant
            4242 # Storing value in a variable of a smaller type, possible loss of data
            4244 # Storing value of one type in variable of another (size_t in int, for example)
            4245 # Signed/unsigned mismatch
            4267 # Conversion from 'size_t' to 'int', possible loss of data
            4305 # Truncating value (double to float, for example)
            4309 # Variable overflow, trying to store 128 in a signed char for example
            4351 # New behavior: elements of array 'array' will be default initialized (desired behavior)
            4355 # Using 'this' in member initialization list
            4505 # Unreferenced local function has been removed
            4701 # Potentially uninitialized local variable used
            4702 # Unreachable code
            4800 # Boolean optimization warning, e.g. myBool = (myInt != 0) instead of myBool = myInt
            )

    foreach (d ${WARNINGS_DISABLE})
        SET(WARNINGS "${WARNINGS} /wd${d}")
    endforeach (d)

    IF (WANT_PLAYER)
        SET_TARGET_PROPERTIES(wildmidi PROPERTIES COMPILE_FLAGS "${WARNINGS} ${MT_BUILD}")
    ENDIF ()

    IF (WANT_PLAYERSTATIC)
        SET_TARGET_PROPERTIES(wildmidi-static PROPERTIES COMPILE_FLAGS "${WARNINGS} ${MT_BUILD}")
    ENDIF ()

ENDIF ()

IF (RISCOS AND WANT_PLAYERSTATIC)
    SET_TARGET_PROPERTIES(wildmidi-static PROPERTIES LINK_FLAGS "-static")
    ADD_CUSTOM_COMMAND(OUTPUT ${EXECUTABLE_OUTPUT_PATH}/wildmidi,ff8
                       COMMAND elf2aif ${EXECUTABLE_OUTPUT_PATH}/wildmidi-static ${EXECUTABLE_OUTPUT_PATH}/wildmidi,ff8
                       DEPENDS wildmidi-static)
    ADD_CUSTOM_TARGET(wildmidi-aif ALL DEPENDS ${EXECUTABLE_OUTPUT_PATH}/wildmidi,ff8)
ENDIF ()
