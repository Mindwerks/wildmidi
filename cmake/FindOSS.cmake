# - Find Oss
# Find Oss headers and libraries.
#
#  OSS_INCLUDE_DIR  -  where to find soundcard.h, etc.
#  OSS_FOUND        - True if Oss found.

INCLUDE(CheckIncludeFiles)
INCLUDE(CheckCSourceCompiles)

SET(OSS_LIBRARY) # no lib to link
SET(OSS_INCLUDE_DIR) # system header must suffice
SET(OSS_FOUND)

MESSAGE(STATUS "Looking for OSS...")

#CHECK_INCLUDE_FILES(linux/soundcard.h HAVE_LINUX_SOUNDCARD_H) # Linux does provide <sys/soundcard.h>
CHECK_INCLUDE_FILES(sys/soundcard.h HAVE_SYS_SOUNDCARD_H)
CHECK_INCLUDE_FILES(machine/soundcard.h HAVE_MACHINE_SOUNDCARD_H)
CHECK_INCLUDE_FILES(soundcard.h HAVE_SOUNDCARD_H) # less common, but exists.

IF(HAVE_SYS_SOUNDCARD_H)
    CHECK_C_SOURCE_COMPILES("#include <sys/ioctl.h>
                             #include <sys/soundcard.h>
                             int main() {return SNDCTL_DSP_RESET;}" OSS_FOUND)
ELSEIF(HAVE_MACHINE_SOUNDCARD_H)
    CHECK_C_SOURCE_COMPILES("#include <sys/ioctl.h>
                             #include <machine/soundcard.h>
                             int main() {return SNDCTL_DSP_RESET;}" OSS_FOUND)
ELSEIF(HAVE_SOUNDCARD_H)
    CHECK_C_SOURCE_COMPILES("#include <sys/ioctl.h>
                             #include <soundcard.h>
                             int main() {return SNDCTL_DSP_RESET;}" OSS_FOUND)
ENDIF()

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
