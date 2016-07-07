/* config.h -- generated from config.h.cmake  */

/* Name of package */
#define PACKAGE "wildmidi"

/* Define to the home page for this package. */
#define PACKAGE_URL "http://www.mindwerks.net/projects/wildmidi/"

/* Define to the address where bug reports for this package should be sent. */
#define PACKAGE_BUGREPORT "https://github.com/Mindwerks/wildmidi/issues"

/* Define to the full name of this package. */
#define PACKAGE_NAME "WildMidi"

/* Define to the full name and version of this package. */
#define PACKAGE_STRING "WildMidi @WILDMIDI_VERSION@"

/* Define to the one symbol short name of this package. */
#define PACKAGE_TARNAME "wildmidi"

/* Define to the version of this package. */
#define PACKAGE_VERSION "@WILDMIDI_VERSION@"

/* Version number of package */
#define VERSION "@WILDMIDI_VERSION@"

/* Define this to the location of the wildmidi config file */
#define WILDMIDI_CFG "@WILDMIDI_CFG@"

/* Define if the C compiler supports the `inline' keyword. */
#cmakedefine HAVE_C_INLINE
/* Define if the C compiler supports the `__inline__' keyword. */
#cmakedefine HAVE_C___INLINE__
/* Define if the C compiler supports the `__inline' keyword. */
#cmakedefine HAVE_C___INLINE
#if !defined(HAVE_C_INLINE) && !defined(__cplusplus)
# ifdef HAVE_C___INLINE__
#  define inline __inline__
# elif defined(HAVE_C___INLINE)
#  define inline __inline
# else
#  define inline
# endif
#endif

/* Define if the compiler has the `__builtin_expect' built-in function */
#cmakedefine HAVE___BUILTIN_EXPECT
#ifndef HAVE___BUILTIN_EXPECT
#define __builtin_expect(x,c) x
#endif

/* define this if you are running a bigendian system (motorola, sparc, etc) */
#cmakedefine WORDS_BIGENDIAN 1

/* define this if building for AmigaOS variants */
#cmakedefine WILDMIDI_AMIGA 1

/* Define if you have the <stdint.h> header file. */
#cmakedefine HAVE_STDINT_H

/* Define if you have the <inttypes.h> header file. */
#cmakedefine HAVE_INTTYPES_H

/* Define our audio drivers */
#cmakedefine HAVE_LINUX_SOUNDCARD_H
#cmakedefine HAVE_SYS_SOUNDCARD_H
#cmakedefine HAVE_MACHINE_SOUNDCARD_H
#cmakedefine HAVE_SOUNDCARD_H

#cmakedefine AUDIODRV_ALSA
#cmakedefine AUDIODRV_OSS
#cmakedefine AUDIODRV_OPENAL
#cmakedefine AUDIODRV_AHI
