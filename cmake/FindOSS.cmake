# - Find OSS
# Find OSS headers and libraries.
#
#  OSS_INCLUDE_DIR  - where to find soundcard.h, etc.
#  OSS_LIBRARY      - link library, if any, needed for OSS.
#  OSS_FOUND        - True if OSS found.

INCLUDE(CheckIncludeFiles)
INCLUDE(CheckCSourceCompiles)

SET(OSS_LIBRARY "")
SET(OSS_INCLUDE_DIR) # system header must suffice
SET(OSS_FOUND)

MESSAGE(STATUS "Looking for OSS...")

CHECK_INCLUDE_FILES(sys/soundcard.h HAVE_SYS_SOUNDCARD_H)

# NetBSD uses ossaudio emulation layer,
# otherwise no link library is needed.
IF(CMAKE_SYSTEM_NAME MATCHES "kNetBSD.*|NetBSD.*")  # AND HAVE_SOUNDCARD_H ???
  FIND_LIBRARY(OSSAUDIO_LIBRARIES "ossaudio")
  IF(OSSAUDIO_LIBRARIES STREQUAL "OSSAUDIO_LIBRARIES-NOTFOUND")
    SET(OSSAUDIO_LIBRARIES)
  ELSE()
    MESSAGE(STATUS "Found libossaudio: ${OSSAUDIO_LIBRARIES}")
    SET(OSS_LIBRARY ${OSSAUDIO_LIBRARIES})
  ENDIF()
ELSE()
  SET(OSSAUDIO_LIBRARIES)
ENDIF()

SET(OLD_REQUIRED_LIBRARIES "${CMAKE_REQUIRED_LIBRARIES}")
IF(OSSAUDIO_LIBRARIES)
  SET(CMAKE_REQUIRED_LIBRARIES ${OSSAUDIO_LIBRARIES})
ENDIF()

IF(HAVE_SYS_SOUNDCARD_H)
    CHECK_C_SOURCE_COMPILES("#include <sys/ioctl.h>
                             #include <sys/soundcard.h>
                             int main() {return SNDCTL_DSP_RESET;}" OSS_FOUND)
ENDIF()

SET(CMAKE_REQUIRED_LIBRARIES "${OLD_REQUIRED_LIBRARIES}")

MARK_AS_ADVANCED (
	OSS_FOUND
	OSS_INCLUDE_DIR
	OSS_LIBRARY
)

IF(OSS_FOUND)
    MESSAGE(STATUS "Found OSS.")
ELSE()
    MESSAGE(STATUS "Could not find OSS.")
ENDIF()
