# - Find Oss
# Find Oss headers and libraries.
#
#  OSS_INCLUDE_DIR  -  where to find soundcard.h, etc.
#  OSS_FOUND        - True if Oss found.


INCLUDE(LibFindMacros)
INCLUDE(CheckIncludeFiles)

MESSAGE(STATUS "Looking for OSS...")
#CHECK_INCLUDE_FILES(linux/soundcard.h HAVE_LINUX_SOUNDCARD_H) # Linux does provide <sys/soundcard.h>
CHECK_INCLUDE_FILES(sys/soundcard.h HAVE_SYS_SOUNDCARD_H)
CHECK_INCLUDE_FILES(machine/soundcard.h HAVE_MACHINE_SOUNDCARD_H)
CHECK_INCLUDE_FILES(soundcard.h HAVE_SOUNDCARD_H) # less common, but exists.

FIND_PATH(SYS_OSS_INCLUDE_DIR "sys/soundcard.h"
  "/usr/include" "/usr/local/include"
)

FIND_PATH(MACHINE_OSS_INCLUDE_DIR "machine/soundcard.h"
  "/usr/include" "/usr/local/include"
)

FIND_PATH(BSD_OSS_INCLUDE_DIR "soundcard.h"
  "/usr/include" "/usr/local/include"
)

SET(OSS_FOUND FALSE)

IF(SYS_OSS_INCLUDE_DIR)
	SET(OSS_FOUND TRUE)
	SET(OSS_INCLUDE_DIR ${SYS_OSS_INCLUDE_DIR})
ENDIF()

IF(MACHINE_OSS_INCLUDE_DIR)
	SET(OSS_FOUND TRUE)
	SET(OSS_INCLUDE_DIR ${MACHINE_OSS_INCLUDE_DIR})
ENDIF()

IF(BSD_OSS_INCLUDE_DIR)
	SET(OSS_FOUND TRUE)
	SET(OSS_INCLUDE_DIR ${BSD_OSS_INCLUDE_DIR})
ENDIF()

SET(OSS_LIBRARY) # no lib to link..

MARK_AS_ADVANCED (
	OSS_FOUND
	OSS_INCLUDE_DIR
	BSD_OSS_INCLUDE_DIR
	SYS_OSS_INCLUDE_DIR
	MACHINE_OSS_INCLUDE_DIR
	OSS_LIBRARY
)

IF(OSS_FOUND)
    MESSAGE(STATUS "Found OSS headers.")
ELSE(OSS_FOUND)
    MESSAGE(STATUS "Could not find OSS headers.")
ENDIF()
