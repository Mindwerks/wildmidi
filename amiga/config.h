/* wildmidi config for amigaos variants */
#define WILDMIDI_AMIGA 1

#define WILDMIDI_CFG "wildmidi.cfg"

#define PACKAGE_URL "https://github.com/Mindwerks/wildmidi"
#define PACKAGE_BUGREPORT "https://github.com/Mindwerks/wildmidi/issues"

#define PACKAGE_VERSION "0.4.5"

#define HAVE_C_INLINE

#if defined(__GNUC__) && ((__GNUC__ > 2) || (__GNUC__ == 2 && __GNUC_MINOR >= 96))
#define HAVE___BUILTIN_EXPECT
#endif
#ifndef HAVE___BUILTIN_EXPECT
#define __builtin_expect(x,c) x
#endif

#define AUDIODRV_NONE 1
#define AUDIODRV_WAVE 1
#define AUDIODRV_ALSA 0
#define AUDIODRV_OSS 0
#define AUDIODRV_OPENAL 0
#define AUDIODRV_AHI 1
#define AUDIODRV_WINMM 0
#define AUDIODRV_OS2DART 0
#define AUDIODRV_DOSSB 0
