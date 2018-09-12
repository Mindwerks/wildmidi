#define WILDMIDI_CFG "wildmidi.cfg"

#define PACKAGE_URL "http://www.mindwerks.net/projects/wildmidi/"
#define PACKAGE_BUGREPORT "https://github.com/Mindwerks/wildmidi/issues"

#define PACKAGE_VERSION "0.4.3"

#define HAVE_C_INLINE

#if (__GNUC__ > 2) || (__GNUC__ == 2 && __GNUC_MINOR >= 96)
#define HAVE___BUILTIN_EXPECT
#endif
#ifndef HAVE___BUILTIN_EXPECT
#define __builtin_expect(x,c) x
#endif

#if defined(__POWERPC__) || defined(__ppc__) || defined(__BIG_ENDIAN__)
#define WORDS_BIGENDIAN 1
#endif

#define HAVE_STDINT_H 1
#define HAVE_INTTYPES_H 1
