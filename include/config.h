/* include/config.h.  Generated from config.h.cmake by configure.  */

/* Name of package */
#define PACKAGE "wildmidi"

/* Define to the home page for this package. */
#define PACKAGE_URL "http://www.mindwerks.net/projects/wildmidi/"

/* Define to the address where bug reports for this package should be sent. */
#define PACKAGE_BUGREPORT "https://github.com/psi29a/wildmidi/issues"

/* Define to the full name of this package. */
#define PACKAGE_NAME "WildMidi"

/* Define to the full name and version of this package. */
#define PACKAGE_STRING "WildMidi 0.4.dev-build"

/* Define to the one symbol short name of this package. */
#define PACKAGE_TARNAME "wildmidi"

/* Define to the version of this package. */
#define PACKAGE_VERSION "0.4.dev-build"

/* Version number of package */
#define VERSION "0.4.dev-build"

/* Define this to the location of the wildmidi config file */
#define WILDMIDI_CFG "/etc/wildmidi/wildmidi.cfg"

/* Set our global defines here */
#ifndef M_PI
#define M_PI           3.14159265358979323846
#endif

/* Define this if the GCC __builtin_expect keyword is available */
#ifndef __builtin_expect
#define __builtin_expect(x,c) x
#endif

#ifndef inline
#define inline __inline
#endif

/* Define our audio drivers */
#define HAVE_ALSA_H
/* #undef HAVE_LINUX_SOUNDCARD_H */
/* #undef HAVE_SYS_SOUNDCARD_H */
/* #undef HAVE_MACHINE_SOUNDCARD_H */
