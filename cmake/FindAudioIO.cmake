# - Find AudioIO includes
#
#   AUDIOIO_FOUND        - True if AUDIOIO_INCLUDE_DIR is found
#   AUDIOIO_INCLUDE_DIRS - Set when AUDIOIO_INCLUDE_DIR is found
#
#   AUDIOIO_INCLUDE_DIR  - where to find sys/audioio.h

find_path(AUDIOIO_INCLUDE_DIR NAMES sys/audioio.h)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(AudioIO  REQUIRED_VARS AUDIOIO_INCLUDE_DIR)
if(AUDIOIO_FOUND)
    set(AUDIOIO_INCLUDE_DIRS ${AUDIOIO_INCLUDE_DIR})
endif()
mark_as_advanced(AUDIOIO_INCLUDE_DIR)
