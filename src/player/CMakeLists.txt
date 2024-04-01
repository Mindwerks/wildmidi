# WildMidi Player related stuff

SET(wildmidi_executable_SRCS
		amiga.c
		msleep.c
		out_none.c
		out_ahi.c
		out_alsa.c
		out_coreaudio.c
		out_dart.c
		out_dossb.c
		out_netbsd.c
		out_openal.c
		out_oss.c
		out_sndio.c
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
		IF (AUDIODRV_OPENAL)
				TARGET_INCLUDE_DIRECTORIES(wildmidi PRIVATE ${OPENAL_INCLUDE_DIR})
		ENDIF()
		IF (AUDIODRV_ALSA)
				TARGET_INCLUDE_DIRECTORIES(wildmidi PRIVATE ${ALSA_INCLUDE_DIR})
		ENDIF()
		IF (AUDIODRV_SNDIO)
				TARGET_INCLUDE_DIRECTORIES(wildmidi PRIVATE ${SNDIO_INCLUDE_DIR})
		ENDIF()

		TARGET_LINK_LIBRARIES(wildmidi
				${EXTRA_LDFLAGS}
				libwildmidi
				${AUDIO_LIBRARY}
				${M_LIBRARY}
		)
		LIST(APPEND wildmidi_install wildmidi)
ENDIF ()

IF (WANT_PLAYERSTATIC)
		ADD_EXECUTABLE(wildmidi-static ${wildmidi_executable_SRCS})
		SET_TARGET_PROPERTIES(wildmidi-static PROPERTIES COMPILE_DEFINITIONS WILDMIDI_STATIC)

		IF (AUDIODRV_OPENAL)
			TARGET_INCLUDE_DIRECTORIES(wildmidi-static PRIVATE ${OPENAL_INCLUDE_DIR})
		ENDIF()
		IF (AUDIODRV_ALSA)
			TARGET_INCLUDE_DIRECTORIES(wildmidi-static PRIVATE ${ALSA_INCLUDE_DIR})
		ENDIF()
		IF (AUDIODRV_SNDIO)
			TARGET_INCLUDE_DIRECTORIES(wildmidi-static PRIVATE ${SNDIO_INCLUDE_DIR})
		ENDIF()

		TARGET_LINK_LIBRARIES(wildmidi-static
				${EXTRA_LDFLAGS}
				libwildmidi-static
				${AUDIO_LIBRARY}
				${M_LIBRARY}
		)
		LIST(APPEND wildmidi_install wildmidi-static)

		IF (RISCOS)
				SET_TARGET_PROPERTIES(wildmidi-static PROPERTIES LINK_FLAGS "-static")
				ADD_CUSTOM_COMMAND(OUTPUT ${PROJECT_BINARY_DIR}/wildmidi,ff8
						COMMAND elf2aif ${PROJECT_BINARY_DIR}/wildmidi-static ${PROJECT_BINARY_DIR}/wildmidi,ff8
						DEPENDS wildmidi-static)
				ADD_CUSTOM_TARGET(wildmidi-aif ALL DEPENDS ${PROJECT_BINARY_DIR}/wildmidi,ff8)
		ENDIF ()
ENDIF ()

INSTALL(TARGETS ${wildmidi_install}
		RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

IF (WIN32 AND MSVC)
		SET(WARNINGS "/W3")

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
ENDIF()
