#define WILDMIDI_CFG "wildmidi.cfg"

#define PACKAGE_URL "https://github.com/Mindwerks/wildmidi"
#define PACKAGE_BUGREPORT "https://github.com/Mindwerks/wildmidi/issues"

#define PACKAGE_VERSION "0.5.0"

/* #undef HAVE_C_INLINE */
#define HAVE_C___INLINE
#if !defined(__cplusplus)
#  define inline __inline
#endif

#define __builtin_expect(x,c) x

#if (_MSC_VER >= 1600)
#define HAVE_STDINT_H 1
#endif
#if (_MSC_VER >= 1800)
#define HAVE_INTTYPES_H 1
#endif

#if _MSC_VER >= 1400
#define HAVE_EXPF 1
#define HAVE_SQRTF 1
#define HAVE_POWF 1
#define HAVE_SINF 1
#endif

#define WILDMIDI_SF2 1

/* #undef AUDIODRV_OPENAL */
#define AUDIODRV_WINMM 1
