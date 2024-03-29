# - Find OSS includes
#
#   OSS_FOUND        - True if OSS_INCLUDE_DIR is found
#   OSS_INCLUDE_DIRS - Set when OSS_INCLUDE_DIR is found
#   OSS_LIBRARIES    - Set when OSS_LIBRARY is found
#
#   OSS_INCLUDE_DIR  - where to find sys/soundcard.h
#   OSS_LIBRARY      - where to find libossaudio (for NetBSD)
#
message(STATUS "Looking for OSS...")

include(FindPackageHandleStandardArgs)

find_path(OSS_INCLUDE_DIR NAMES sys/soundcard.h)

# NetBSD uses ossaudio emulation layer:
if(CMAKE_SYSTEM_NAME MATCHES "kNetBSD.*|NetBSD.*")
  find_library(OSS_LIBRARY NAMES ossaudio)
  find_package_handle_standard_args(OSS REQUIRED_VARS OSS_INCLUDE_DIR OSS_LIBRARY)
else()
  unset(OSS_LIBRARY)
  find_package_handle_standard_args(OSS REQUIRED_VARS OSS_INCLUDE_DIR)
endif()

if(OSS_FOUND)
  set(OSS_INCLUDE_DIRS ${OSS_INCLUDE_DIR})
  if(OSS_LIBRARY)
    set(OSS_LIBRARIES ${OSS_LIBRARY})
  else()
    unset(OSS_LIBRARIES)
  endif()
endif()

mark_as_advanced(OSS_INCLUDE_DIR OSS_LIBRARY)
