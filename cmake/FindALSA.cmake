# - Try to find ALSA
# Once done, this will define
#
#  ALSA_FOUND - system has ALSA (GL and GLU)
#  ALSA_INCLUDE_DIRS - the ALSA include directories
#  ALSA_LIBRARIES - link these to use ALSA
#  ALSA_GL_LIBRARY - only GL
#  ALSA_GLU_LIBRARY - only GLU
#
# See documentation on how to write CMake scripts at
# http://www.cmake.org/Wiki/CMake:How_To_Find_Libraries

INCLUDE(LibFindMacros)
INCLUDE(CheckIncludeFiles)

LIBFIND_PKG_CHECK_MODULES(ALSA_PKGCONF alsa)

FIND_PATH(ALSA_INCLUDE_DIR
  NAMES alsa/version.h
  PATHS ${ALSA_PKGCONF_INCLUDE_DIRS}
)
CHECK_INCLUDE_FILES(alsa/version.h HAVE_ALSA_H)

FIND_LIBRARY(ALSA_LIBRARY
  NAMES asound
  PATHS ${ALSA_PKGCONF_LIBRARY_DIRS}
)

# Extract the version number
IF(ALSA_INCLUDE_DIR)
FILE(READ "${ALSA_INCLUDE_DIR}/alsa/version.h" _ALSA_VERSION_H_CONTENTS)
STRING(REGEX REPLACE ".*#define SND_LIB_VERSION_STR[ \t]*\"([^\n]*)\".*" "\\1" ALSA_VERSION "${_ALSA_VERSION_H_CONTENTS}")
ENDIF(ALSA_INCLUDE_DIR)

SET(ALSA_PROCESS_INCLUDES ALSA_INCLUDE_DIR)
SET(ALSA_PROCESS_LIBS ALSA_LIBRARY)
LIBFIND_PROCESS(ALSA)

