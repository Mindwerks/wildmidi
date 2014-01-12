/* include/config.h.  Generated from config.h.cmake by configure.  */

/* Define this if the GCC __builtin_expect keyword is available */
#define HAVE___BUILTIN_EXPECT 1
#ifndef HAVE___BUILTIN_EXPECT
# define __builtin_expect(x,c) x
#endif

/* Name of package */
#define PACKAGE "wildmidi"

/* Define to the address where bug reports for this package should be sent. */
#define PACKAGE_BUGREPORT "https://github.com/psi29a/wildmidi/issues"

/* Define to the full name of this package. */
#define PACKAGE_NAME "WildMidi"

/* Define to the full name and version of this package. */
#define PACKAGE_STRING "WildMidi 0.2.3.5"

/* Define to the one symbol short name of this package. */
#define PACKAGE_TARNAME "wildmidi"

/* Define to the home page for this package. */
#define PACKAGE_URL "https://github.com/psi29a/wildmidi"

/* Define to the version of this package. */
#define PACKAGE_VERSION "0.2.3.5"

/* Version number of package */
#define VERSION "0.2.3.5"

/* Define this to the location of the wildmidi config file */
#define WILDMIDI_CFG "wildmidi.cfg"

/* Define WORDS_BIGENDIAN to 1 if your processor stores words with the most
 significant byte first (like Motorola and SPARC, unlike Intel). */
#if defined AC_APPLE_UNIVERSAL_BUILD
# if defined __BIG_ENDIAN__
#  define WORDS_BIGENDIAN 1
# endif
#else
# ifndef WORDS_BIGENDIAN
/* #  undef WORDS_BIGENDIAN */
# endif
#endif
