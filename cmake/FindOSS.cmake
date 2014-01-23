# - Find Oss
# Find Oss headers and libraries.
#
#  OSS_INCLUDE_DIR  -  where to find soundcard.h, etc.
#  OSS_FOUND        - True if Oss found.


INCLUDE(LibFindMacros)
INCLUDE(CheckIncludeFiles)

MESSAGE(STATUS "Looking for OSS...")
CHECK_INCLUDE_FILES(linux/soundcard.h HAVE_LINUX_SOUNDCARD_H)
CHECK_INCLUDE_FILES(sys/soundcard.h HAVE_SYS_SOUNDCARD_H)
CHECK_INCLUDE_FILES(machine/soundcard.h HAVE_MACHINE_SOUNDCARD_H)

FIND_PATH(LINUX_OSS_INCLUDE_DIR "linux/soundcard.h"
  "/usr/include" "/usr/local/include"
)

FIND_PATH(SYS_OSS_INCLUDE_DIR "sys/soundcard.h"
  "/usr/include" "/usr/local/include"
)

FIND_PATH(MACHINE_OSS_INCLUDE_DIR "machine/soundcard.h"
  "/usr/include" "/usr/local/include"
)

SET(OSS_FOUND FALSE)

IF(LINUX_OSS_INCLUDE_DIR)
	SET(OSS_FOUND TRUE)
	SET(OSS_INCLUDE_DIR ${LINUX_OSS_INCLUDE_DIR})
ENDIF()

IF(SYS_OSS_INCLUDE_DIR)
	SET(OSS_FOUND TRUE)
	SET(OSS_INCLUDE_DIR ${SYS_OSS_INCLUDE_DIR})
ENDIF()

IF(MACHINE_OSS_INCLUDE_DIR)
	SET(OSS_FOUND TRUE)
	SET(OSS_INCLUDE_DIR ${MACHINE_OSS_INCLUDE_DIR})
ENDIF()

MARK_AS_ADVANCED (
	OSS_FOUND
	OSS_INCLUDE_DIR
	LINUX_OSS_INCLUDE_DIR
	SYS_OSS_INCLUDE_DIR
	MACHINE_OSS_INCLUDE_DIR
) 

IF(OSS_FOUND)
    MESSAGE(STATUS "Found OSS headers.")
ELSE(OSS_FOUND)
    FATAL_ERROR(STATUS "Could not find OSS headers!")
ENDIF()