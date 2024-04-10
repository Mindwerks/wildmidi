WildMIDI is a simple software midi player which has a core softsynth
library that can be used in other applications.

The WildMIDI library uses Gravis Ultrasound patch files to convert MIDI
files into audio which is then passed back to the calling application.
The library API is designed so that it is easy to include WildMIDI into
applications that wish to include MIDI file playback.

Version: 0.4.6
Licenses: GPLv3+ and LGPLv3
Website: https://github.com/Mindwerks/wildmidi

PLATFORMS:

* Linux: Arch, Debian, Fedora, Ubuntu (player: ALSA, OSS,
  or optionally OpenAL output.)
* Windows: x86 and x64
* OSX: x86, x64 and powerpc (player: CoreAudio output)
* FreeBSD, Debian kFreeBSD (player: OSS output)
* OpenBSD (player: sndio output.)
* NetBSD (player: netbsd output.)
* Hurd: Debian
* DOS (player: sound blaster or compatibles output.)
* OS/2 (player: Dart output.)
* AmigaOS & variants like MorphOS, AROS. (player: AHI output)

BUILD FROM SOURCE:

Requirements:
* cmake
* GCC or clang / Xcode / VisualStudio / MinGW or MinGW-w64
* DOS port: DJGPP / GNU make
* OS/2 port: OpenWatcom (tested with version 1.9 and newer)
* Nintendo 3DS port: devkitARM
* Nintendo Wii port: devkitPPC
* Nintendo Switch port: devkitA64
* PSVita port: Vitasdk

CHANGELOG

0.4.6
* A lot of player cleanup and refactoring, thanks to initial work
  by Azamat H. Hackimov, with addition of several safeguards and
  minor fixes.
* Ability to choose which audio output backends to include in the
  build system: see the cmake script for the relevant `WANT_???`
  options. Player's `--help` command line switch lists the available
  backends. Thanks to initial work by Azamat H. Hackimov.
* New native audio output backends for player: coreaudio for macOS,
  sndio for OpenBSD, netbsd (sunaudio) for NetBSD.
* Improved pkg-config file generation in cmake script (bug #236).
* Workaround a link failure on AmigaOS4 with newer SDKs (bug #241).
* Other minor source clean-ups.
* CMake project clean-ups. Cmake v3.4 or newer is now required.

0.4.5
* Fixed MUS drum channels 9 and 15 being swapped if the same file
  is played twice from the same memory buffer (bug #234).
* Player: Fixed save midi reading wrong argv if there are no path
  seperators (bug #227).
* Other code and build system clean-ups.

0.4.4
* Fixed integer overflow in midi parser sample count calculation
 (bug #200).
* Fixed 8 bit ping pong GUS patch loaders (bug #207).
* Fixed wrong variable use in reverb code (bug #210).
* Reset block status of tty after playback (bug #211).
* Fixed broken file name handling for 'save as midi' command during
  playback.
* Clamp MUS volume commands (PR #226).
* CMake project improvements (bugs: #214, #216, #217, #218) - cmake
  version 3.1 or newer is now required.

0.4.3
* New API addition: WildMidi_InitVIO().  It is like WildMidi_Init(),
  but tells the library to use caller-provided functions for file IO.
  See wildmidi_lib.h or the man page WildMidi_InitVIO(3) for details.
  This was suggested and implemented by Christian Breitwieser.
* Fixed Visual Studio optimized builds (bug #192, function ptr issue.)
* Fixed a thinko in one of the buffer size checks added in v0.4.2.
* Fixed possible out of bounds reads in sysex commands (bug #190).
* Fixed invalid reads during config parse with short patch file names.
* Do not treat a missing end-of-track marker as an error for type-0
  midi files (bug #183).
* Fixed bad reading of high delta values in XMI converter (bug #199).
* Fixed a memory leak when freeing a midi (bug #204).
* Fixed slurred/echoy playback at quick tempos on looped instruments
  (bug #185).
* Fixed certain midis sounding different compared to timidity, as if
  instruments not turned off (bug #186).
* Fixed compilation on systems without libm.
* Support for RISC OS, Nintendo Switch and PS Vita.
* Several clean-ups.

0.4.2
* Fixed CVE-2017-11661, CVE-2017-11662, CVE-2017-11663, CVE-2017-11664
  (bug #175).
* Fixed CVE-2017-1000418 (bug #178).
* Fixed a buffer overflow during playback with malformed midi files
  (bug #180).
* GUS patch processing changes to meet users expectations (bug #132).
* Worked around a build failure with newer FreeBSD versions failing to
  retrieve the ONLCR constant (bug #171).
* Fixed a minor Windows unicode issue (PR #170).
* A few other fixes / clean-ups.

0.4.1
* Fixed bug in handling of the "source" directive in config files.
* Fixed a nasty bug in dBm_pan_volume. Other fixes and clean-ups.
* Build system updates. Install a pkg-config file on supported platforms
  such as Linux. New android ndk makefile.
* File i/o updates.
* Support for OS/2.
* Support for Nintendo 3DS
* Support for Nintendo Wii
* Support for AmigaOS and its variants like MorphOS and AROS.

0.4.0
* API change: The library now returns audio data in host-endian format,
  not little-endian.
* API change: WildMidi_GetVersion() added to the api, along with new
  numeric version macros in the wildmidi_lib.h header. the dso version
  is changed from 1 to 2.
* API change: All long or unsigned long type _WM_Info fields changed
  into strictly 32bit fields (int32_t or uint32_t.)
* API change: WildMidi_OpenBuffer() and WildMidi_GetOutput() changed
  to accept strictly 32bit size parameters, i.e. uint32_t, instead of
  unsigned long.
* API change: WildMidi_ConvertToMidi() and WildMidi_ConvertBufferToMidi() 
  added for MIDI-like files to be converted to MIDI.
* API change: WildMidi_SetCvtOption() added to support conversion options.
* API change: WildMidi_SongSeek() added to support Type 2 MIDI files.
* API change: WildMidi_GetLyric() added to support embedded text, 
  such as KAR files.
* API change: WildMidi_GetError() and WildMidi_ClearError() added to
  cleanly check for, retrieve and clear error messages. They no longer
  go to stderr.
* Support for loading XMI (XMIDI format) and XFM files, such as from Arena.
  Thanks Ryan Nunn for releasing his code under the LGPL.
* Support for loading MUS (MUS Id format) files, such as from Doom.
* Support for loading HMP/HMI files, such as from Daggerfall.
* Support for loading KAR (MIDI with Lyrics) and Type 2 MIDI files.
* Build requires cmake-2.8.11 or newer now.

0.3.9
* Library: Fixed a segmentation fault with bad midi files.

0.3.8
* Library: Fixed a seek-to-0 bug in order to cure an issue of truncated
  start (bug #100, gnome/gstreamer bug #694811.)
* Player, OpenAL: reduced buffers from 8 to 4 so as to cure some output
  delay issues (bug #85.)

0.3.7
* Plug a memory leak in case of broken midis.
* Properly reset global state upon library shutdown.
* Support for type-2 midi files.
* Fix a possible crash in WildMidi_SetOption.
* DOS port: Support for Sound Blaster output in player.
* Uglify the library's private global variable and function names.
* Build: Add option for a statically linked player.
* Build: Add headers to project files. Use -fno-common flag.
* Other small fixes/clean-ups.

0.3.6
* Fix some portability issues.
* Fix a double-free issue during library shutdown when several midis
  were alive.
* Fix the invalid option checking in WildMidi_Init().
* Fix the roundtempo option which had been broken since its invention
  in 0.2.3.5 (WM_MO_ROUNDTEMPO: was 0xA000 instead of 0x2000.)
* Fix cfg files without a newline at the end weren't parsed correctly.
* Handle cfg files with mac line-endings.
* Refuse loading suspiciously long files.

0.3.5
* Greatly reduced the heap usage (was a regression introduced in 0.2.3)
* OpenAL support: Fixed audio output on big-endian systems. Fixed audio
  skips at song start.
* OSS support: No longer uses mmap mode for better compatibility. This
  gains us NetBSD and OpenBSD support.
* Worked around an invalid memory read found by valgrind when playing
  Beethoven's Fur Elise.rmi at 44100 Hz using the old MIDIA patch-set
  from 1994.
* Build fixes for MSVC. Revised visibility attributes usage.

0.3.4
* OpenAL support: This gains us OSX and other platforms that OpenAL
  supports for sound output!
* DOS (DJGPP) support: This goes a long way to helping other DOS
  based applications.
* MinGW support: This gains us win32 and win64 support using this
  toolchain.
* OSS fixes.
* Add missing parts of the absolute paths fix in config parsing.
* New portable file and path-name system to handle cross-platform
  support.
* Support for Debian/kFreeBSD, Debian/Hurd and other Debian archs.
* Many bug fixes, code clean-ups and cosmetic fixes.

0.3.3
* default to hidden visibility and only export our API functions
* windows lean and mean to help compile times on Windows
* cli and xcode work now on OSX
* better FreeBSD support
* Supported platforms are Debian, FreeBSD, Windows and OSX (but only
  for WAV output)

0.3.1 - 0.3.2
* Cmake updates/fixes/cleanups.

0.3.0
* initial CMake support.
* process non-registered params. fix issue of notes ending before
  attack envelope completed. (sf.net svn r149/r151.)

