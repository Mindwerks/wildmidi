#define WILDMIDI_CFG "wildmidi.cfg"

#define PACKAGE_URL "https://github.com/Mindwerks/wildmidi"
#define PACKAGE_BUGREPORT "https://github.com/Mindwerks/wildmidi/issues"

#define PACKAGE_VERSION "0.5.0"

#define HAVE_C_INLINE

#if (__GNUC__ > 2) || (__GNUC__ == 2 && __GNUC_MINOR >= 96)
#define HAVE___BUILTIN_EXPECT
#endif
#ifndef HAVE___BUILTIN_EXPECT
#define __builtin_expect(x,c) x
#endif

#ifdef __KLIBC__
#define HAVE_EXPF 1
#define HAVE_SQRTF 1
#define HAVE_POWF 1
#define HAVE_SINF 1
#endif

#define WILDMIDI_SF2 1

#define AUDIODRV_OS2DART 1
