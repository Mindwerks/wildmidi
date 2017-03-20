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
#define PACKAGE_STRING "WildMidi 0.4.2"

/* Define to the one symbol short name of this package. */
#define PACKAGE_TARNAME "wildmidi"

/* Define to the version of this package. */
#define PACKAGE_VERSION "0.4.2"

/* Version number of package */
#define VERSION "0.4.2"

/* Define this to the location of the wildmidi config file */
/* #undef WILDMIDI_CFG "/etc/wildmidi/wildmidi.cfg" */

/* Define if the C compiler supports the `inline' keyword. */
#define HAVE_C_INLINE
/* Define if the C compiler supports the `__inline__' keyword. */
#define HAVE_C___INLINE__
/* Define if the C compiler supports the `__inline' keyword. */
#define HAVE_C___INLINE
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
#define HAVE___BUILTIN_EXPECT
#ifndef HAVE___BUILTIN_EXPECT
#define __builtin_expect(x,c) x
#endif

/* define this if you are running a bigendian system (motorola, sparc, etc) */
/* #undef WORDS_BIGENDIAN */

/* define this if building for AmigaOS variants */
/* #undef WILDMIDI_AMIGA */

/* Define if you have the <stdint.h> header file. */
#define HAVE_STDINT_H

/* Define if you have the <inttypes.h> header file. */
#define HAVE_INTTYPES_H

/* Define our audio drivers */
/* #undef HAVE_LINUX_SOUNDCARD_H */
/* #undef HAVE_SYS_SOUNDCARD_H */
/* #undef HAVE_MACHINE_SOUNDCARD_H */
/* #undef HAVE_SOUNDCARD_H */

/* #undef AUDIODRV_ALSA */
/* #undef AUDIODRV_OSS */
/* #undef AUDIODRV_OPENAL */
/* #undef AUDIODRV_AHI */
