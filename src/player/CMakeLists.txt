# WildMidi Player related stuff

SET(wildmidi_executable_SRCS
		amiga.c
		fileops.c
		out_ahi.c
		out_alsa.c
		out_dart.c
		out_dossb.c
		out_openal.c
		out_oss.c
		out_wave.c
		out_win32mm.c
		wm_tty.c
		wildmidi.c
)

IF (MSVC)
		LIST(APPEND wildmidi_executable_SRCS ../getopt_long.c)
ENDIF ()

# do we want the wildmidi player?
IF (WANT_PLAYER)
		ADD_EXECUTABLE(wildmidi	${wildmidi_executable_SRCS})
		TARGET_INCLUDE_DIRECTORIES(wildmidi PRIVATE ${PROJECT_SOURCE_DIR}/include/player)
		IF (AUDIODRV_OPENAL)
				TARGET_INCLUDE_DIRECTORIES(wildmidi PRIVATE	${OPENAL_INCLUDE_DIR})
		ENDIF()
		IF (AUDIODRV_ALSA)
				TARGET_INCLUDE_DIRECTORIES(wildmidi PRIVATE	${ALSA_INCLUDE_DIR})
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
		ADD_EXECUTABLE(wildmidi-static ${wildmidi_executable_SRCS})
		TARGET_INCLUDE_DIRECTORIES(wildmidi-static PRIVATE ${PROJECT_SOURCE_DIR}/include/player)
		SET_TARGET_PROPERTIES(wildmidi-static PROPERTIES COMPILE_DEFINITIONS WILDMIDI_STATIC)

		IF (AUDIODRV_OPENAL)
				TARGET_INCLUDE_DIRECTORIES(wildmidi-static PRIVATE ${OPENAL_INCLUDE_DIR})
		ENDIF ()
		IF (AUDIODRV_ALSA)
				TARGET_INCLUDE_DIRECTORIES(wildmidi-static PRIVATE ${ALSA_INCLUDE_DIR})
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

		IF (RISCOS)
				SET_TARGET_PROPERTIES(wildmidi-static PROPERTIES LINK_FLAGS "-static")
				ADD_CUSTOM_COMMAND(OUTPUT ${PROJECT_BINARY_DIR}/wildmidi,ff8
						COMMAND elf2aif ${PROJECT_BINARY_DIR}/wildmidi-static ${PROJECT_BINARY_DIR}/wildmidi,ff8
						DEPENDS wildmidi-static)
				ADD_CUSTOM_TARGET(wildmidi-aif ALL DEPENDS ${PROJECT_BINARY_DIR}/wildmidi,ff8)
		ENDIF ()
ENDIF (WANT_PLAYERSTATIC)

INSTALL(TARGETS ${wildmidi_install}
		RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

IF (WIN32 AND MSVC)
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
				# Release builds use the debug console
				SET_TARGET_PROPERTIES(wildmidi PROPERTIES LINK_FLAGS_RELEASE "/SUBSYSTEM:CONSOLE")
				SET_TARGET_PROPERTIES(wildmidi PROPERTIES LINK_FLAGS_MINSIZEREL "/SUBSYSTEM:CONSOLE")
				TARGET_COMPILE_DEFINITIONS(wildmidi PRIVATE $<$<CONFIG:Release>:_CONSOLE>)
				TARGET_COMPILE_DEFINITIONS(wildmidi PRIVATE $<$<CONFIG:MinSizeRel>:_CONSOLE>)
		ENDIF ()

		IF (WANT_PLAYERSTATIC)
				SET_TARGET_PROPERTIES(wildmidi-static PROPERTIES COMPILE_FLAGS "${WARNINGS} ${MT_BUILD}")
				# Release builds use the debug console
				SET_TARGET_PROPERTIES(wildmidi-static PROPERTIES LINK_FLAGS_RELEASE "/SUBSYSTEM:CONSOLE")
				SET_TARGET_PROPERTIES(wildmidi-static PROPERTIES LINK_FLAGS_MINSIZEREL "/SUBSYSTEM:CONSOLE")
				TARGET_COMPILE_DEFINITIONS(wildmidi-static PRIVATE $<$<CONFIG:Release>:_CONSOLE>)
				TARGET_COMPILE_DEFINITIONS(wildmidi-static PRIVATE $<$<CONFIG:MinSizeRel>:_CONSOLE>)
		ENDIF ()

ENDIF(WIN32 AND MSVC)
