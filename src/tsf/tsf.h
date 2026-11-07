/* TinySoundFont2 - v2.022 - SoundFont2 synthesizer - https://github.com/anzz1/TinySoundFont2
                                     no warranty implied; use at your own risk
   Do this:
      #define TSF_IMPLEMENTATION
   before you include this file in *one* C or C++ file to create the implementation.
   // i.e. it should look like this:
   #include ...
   #include ...
   #define TSF_IMPLEMENTATION
   #include "tsf.h"

   [OPTIONAL] #define TSF_NO_STDIO to remove stdio dependency
   [OPTIONAL] #define TSF_MALLOC, TSF_REALLOC, and TSF_FREE to avoid stdlib.h
   [OPTIONAL] #define TSF_MEMCPY, TSF_MEMSET to avoid string.h
   [OPTIONAL] #define TSF_EXP, TSF_EXPF, TSF_LOGF, TSF_SQRT, TSF_TAN to avoid math.h

   SUPPORT
     - 100% of General MIDI (GM1): channel messages, controllers, RPN 0/1/2, drum channel
     - 100% of SoundFont 2.01: all generators, modulators, default modulators and the NRPN real-time control
     - Linked modulators and the absolute value transform from SoundFont 2.04 (24-bit samples are not supported)
     - Built-in reverb and chorus, which use CPU only while a voice sends to them
     - EMU8000 compatibility (EMU8K_COMPAT=1)
     See TECHNICAL_DETAILS.md for how ambiguities in the specifications are resolved.

   NOT YET IMPLEMENTED
     - GM2 Mode (set by GM2 System On SysEx message)
     - GS Mode (set by GS Reset SysEx message)
     - XG Mode (set by XG Reset SysEx message)

   LICENSE (MIT)

   Copyright (C) 2017-2025 Bernhard Schelling (https://github.com/schellingb/TinySoundFont)
   Based on SFZero, Copyright (C) 2012 Steve Folta (https://github.com/stevefolta/SFZero)

   Permission is hereby granted, free of charge, to any person obtaining a copy of this
   software and associated documentation files (the "Software"), to deal in the Software
   without restriction, including without limitation the rights to use, copy, modify, merge,
   publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons
   to whom the Software is furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included in all
   copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
   INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
   PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
   LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
   TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
   USE OR OTHER DEALINGS IN THE SOFTWARE.

*/

#ifndef TSF_INCLUDE_TSF_INL
#define TSF_INCLUDE_TSF_INL

#ifdef __cplusplus
extern "C" {
#  define CPP_DEFAULT0 = 0
#else
#  define CPP_DEFAULT0
#endif

#if defined(__GNUC__)
//to avoid -Wunused warnings with TSF_STATIC :
#define TSF_UNUSEDFN __attribute__((__unused__))
#else
#define TSF_UNUSEDFN
#endif

//define this if you want the API functions to be static
#ifdef TSF_STATIC
#define TSFDEF TSF_UNUSEDFN static
#else
#define TSFDEF extern
#endif

// The load functions will return a pointer to a struct tsf which all functions
// thereafter take as the first parameter.
// On error the tsf_load* functions will return NULL most likely due to invalid
// data (or if the file did not exist in tsf_load_filename).
typedef struct tsf tsf;

#ifndef TSF_NO_STDIO
// Directly load a SoundFont from a .sf2 file path
TSFDEF tsf* tsf_load_filename(const char* filename);
#endif

// Load a SoundFont from a block of memory
TSFDEF tsf* tsf_load_memory(const void* buffer, int size);

// Stream structure for the generic loading
struct tsf_stream
{
	// Custom data given to the functions as the first parameter
	void* data;

	// Function pointer will be called to read 'size' bytes into ptr (returns number of read bytes)
	int (*read)(void* data, void* ptr, unsigned int size);

	// Function pointer will be called to skip ahead over 'count' bytes (returns 1 on success, 0 on error)
	int (*skip)(void* data, unsigned int count);
};

// Generic SoundFont loading method using the stream structure above
TSFDEF tsf* tsf_load(struct tsf_stream* stream);

// Copy a tsf instance from an existing one, use tsf_close to close it as well.
// All copied tsf instances and their original instance are linked, and share the underlying soundfont.
// This allows loading a soundfont only once, but using it for multiple independent playbacks.
// (This function isn't thread-safe without locking.)
TSFDEF tsf* tsf_copy(tsf* f);

// Free the memory related to this tsf instance
TSFDEF void tsf_close(tsf* f);

// Fade out all playing notes quickly (10 ms) and reset all channels to their initial state
TSFDEF void tsf_reset(tsf* f);

// Returns the preset index from a bank and preset number, or -1 if it does not exist in the loaded SoundFont
TSFDEF int tsf_get_presetindex(const tsf* f, int bank, int preset_number);

// Returns the number of presets in the loaded SoundFont
TSFDEF int tsf_get_presetcount(const tsf* f);

// Returns the name of a preset index >= 0 and < tsf_get_presetcount()
TSFDEF const char* tsf_get_presetname(const tsf* f, int preset_index);

// Returns the bank of a preset index >= 0 and < tsf_get_presetcount(), or -1 if out of range
TSFDEF int tsf_get_presetbank(const tsf* f, int preset_index);

// Returns the preset number of a preset index >= 0 and < tsf_get_presetcount(), or -1 if out of range
TSFDEF int tsf_get_presetnumber(const tsf* f, int preset_index);

// Returns the name of a preset by bank and preset number
TSFDEF const char* tsf_bank_get_presetname(const tsf* f, int bank, int preset_number);

// Supported output modes by the render methods
enum TSFOutputMode
{
	// Two channels with single left/right samples one after another
	TSF_STEREO_INTERLEAVED,
	// Two channels with all samples for the left channel first then right
	TSF_STEREO_UNWEAVED,
	// A single channel (stereo instruments are mixed into center)
	TSF_MONO
};

// Thread safety:
//
// 1. Rendering / voices:
//
// Your audio output which calls the tsf_render* functions will most likely
// run on a different thread than where the playback tsf_note* functions
// are called. In which case some sort of concurrency control like a
// mutex needs to be used so they are not called at the same time.
// Alternatively, you can pre-allocate a maximum number of voices that can
// play simultaneously by calling tsf_set_max_voices after loading.
// That way memory re-allocation will not happen during tsf_note_on and
// TSF should become mostly thread safe.
// There is a theoretical chance that ending notes would negatively influence
// a voice that is rendering at the time but it is hard to say.
// Also be aware, this has not been tested much.
//
// 2. Channels:
//
// Calls to tsf_channel_set_..., tsf_channel_midi_control and
// tsf_channel_note_on may allocate new channels if no channel with that
// number was previously used (channel numbers go up to TSF_MAX_CHANNELS - 1,
// by default 4096). A new channel starts at preset index 0 and the General
// MIDI controller defaults. Make sure to create all channels at the beginning
// as required if you call tsf_render* from a different thread.

// Setup the parameters for the voice render methods
//   outputmode: if mono or stereo and how stereo channel data is ordered
//   samplerate: the number of samples per second (output frequency), a different rate fades out the sounding notes
//   global_gain_db: volume gain in decibels (>0 means higher, <0 means lower), also applies to the sounding notes
TSFDEF void tsf_set_output(tsf* f, enum TSFOutputMode outputmode, int samplerate, float global_gain_db CPP_DEFAULT0);

// Set the global gain as a volume factor
//   global_gain: the desired volume where 1.0 is 100%, also applies to the sounding notes
TSFDEF void tsf_set_volume(tsf* f, float global_gain);

// Built-in effects driven by the SoundFont's chorus and reverb sends. They are enabled by default and use no CPU while no
// playing voice sends to them. Disabling both frees their memory. Parameters are normalized 0..1 except for the chorus rate in Hz.
//   room_size, damping, width: reverb character, wet: level of the reverb return
//   depth: chorus modulation depth, rate: chorus LFO speed in Hz (0..5), wet: level of the chorus return
TSFDEF void tsf_set_reverb(tsf* f, int enabled, float room_size, float damping, float width, float wet);
TSFDEF void tsf_set_chorus(tsf* f, int enabled, float depth, float rate, float wet);
// Clear the effects' internal buffers (silences reverb tails)
TSFDEF void tsf_reset_effects(tsf* f);

// Set the maximum number of voices to play simultaneously
// Depending on the soundfond, one note can cause many new voices to be started,
// so don't keep this number too low or otherwise sounds may not play.
// When the limit is reached a new note takes the released voice furthest into its release, else the oldest note held only by a
// pedal, else the oldest note.
//   max_voices: maximum number to pre-allocate and set the limit to (a number below the current one keeps the old number),
//               or 0 for dynamic polyphony (default, the pool grows as needed)
//   (tsf_set_max_voices returns 0 if max_voices is negative or allocation failed, otherwise 1)
TSFDEF int tsf_set_max_voices(tsf* f, int max_voices);

// Number of voice slots (valid voice_index values for tsf_get_voice_effects)
TSFDEF int tsf_get_voice_slot_count(const tsf* f);

// Read the chorus and reverb send levels (0..1) of a playing voice, returns 0 if the slot has no playing voice
TSFDEF int tsf_get_voice_effects(const tsf* f, int voice_index, float* chorus_send, float* reverb_send);

// Start playing a note (on no channel: the channel settings and controllers don't apply, the modulators see their defaults)
//   preset_index: preset index >= 0 and < tsf_get_presetcount()
//   key: note value between 0 and 127 (60 being middle C)
//   vel: velocity as a float between 0.0 (equal to note off) and 1.0 (full)
//   bank: instrument bank number (alternative to preset_index)
//   preset_number: preset number (alternative to preset_index)
//   (tsf_note_on returns 0 if the preset index or key is invalid or the allocation of a new voice failed, otherwise 1)
//   (tsf_bank_note_on returns 0 if preset does not exist, the key is invalid or allocation failed, otherwise 1)
TSFDEF int tsf_note_on(tsf* f, int preset_index, int key, float vel);
TSFDEF int tsf_bank_note_on(tsf* f, int bank, int preset_number, int key, float vel);

// Stop playing a note started with tsf_note_on/tsf_bank_note_on (no channel)
//   (bank_note_off returns 0 if preset does not exist or the key is invalid, otherwise 1)
TSFDEF void tsf_note_off(tsf* f, int preset_index, int key);
TSFDEF int  tsf_bank_note_off(tsf* f, int bank, int preset_number, int key);

// Stop playing all notes (end with sustain and release)
TSFDEF void tsf_note_off_all(tsf* f);

// Returns the number of active voices
TSFDEF int tsf_active_voice_count(tsf* f);

// Render output samples into a buffer
// You can either render as signed 16-bit values (tsf_render_short) or
// as 32-bit float values (tsf_render_float)
//   buffer: target buffer of size samples * output_channels * sizeof(type)
//   samples: number of samples to render
//   flag_mixing: if 0 clear the buffer first, otherwise mix into existing data
TSFDEF void tsf_render_short(tsf* f, short* buffer, int samples, int flag_mixing CPP_DEFAULT0);
TSFDEF void tsf_render_float(tsf* f, float* buffer, int samples, int flag_mixing CPP_DEFAULT0);

// Higher level channel based functions, set up channel parameters
//   channel: channel number
//   preset_index: preset index >= 0 and < tsf_get_presetcount()
//   preset_number: preset number (alternative to preset_index)
//   flag_mididrums: 0 for normal channels, otherwise apply MIDI drum channel rules
//   bank: instrument bank number (alternative to preset_index)
//   pan: stereo panning value from 0.0 (left) to 1.0 (right) (default 0.5 center), applied on top of the CC10 pan modulator
//   volume: linear volume scale factor (default 1.0 full), applied on top of the CC7 and CC11 volume modulators
//   pitch_wheel: pitch wheel position 0 to 16383 (default 8192 unpitched)
//   pitch_range: range of the pitch wheel in semitones (default 2.0, total +/- 2 semitones), the pitch wheel sensitivity source
//   tuning: tuning of all playing voices in semitones (default 0.0, standard (A440) tuning)
//   flag_sustain: 0 to end notes that were held sustained and disable holding sustain otherwise enable it (also sets CC64 to 127/0)
//   sostenuto: pedal flag, sounding notes are held until it is released (even if their keys are released) (also sets CC66 to 127/0)
//   pressure: channel pressure (aftertouch) from 0 to 127
//   (poly) pressure: polyphonic pressure from 0 to 127 for the playing notes of the given key
//   (tsf_set_preset_number and set_bank_preset return 0 if preset does not exist, otherwise 1)
//   (tsf_channel_set_... return 0 if a new channel needed allocation and that failed, or if the value is invalid, otherwise 1)
TSFDEF int tsf_channel_set_presetindex(tsf* f, int channel, int preset_index);
TSFDEF int tsf_channel_set_presetnumber(tsf* f, int channel, int preset_number, int flag_mididrums CPP_DEFAULT0);
TSFDEF int tsf_channel_set_bank(tsf* f, int channel, int bank);
TSFDEF int tsf_channel_set_bank_preset(tsf* f, int channel, int bank, int preset_number);
TSFDEF int tsf_channel_set_pan(tsf* f, int channel, float pan);
TSFDEF int tsf_channel_set_volume(tsf* f, int channel, float volume);
TSFDEF int tsf_channel_set_pitchwheel(tsf* f, int channel, int pitch_wheel);
TSFDEF int tsf_channel_set_pitchrange(tsf* f, int channel, float pitch_range);
TSFDEF int tsf_channel_set_tuning(tsf* f, int channel, float tuning);
TSFDEF int tsf_channel_set_sustain(tsf* f, int channel, int flag_sustain);
TSFDEF int tsf_channel_set_pressure(tsf* f, int channel, int pressure);
TSFDEF int tsf_channel_set_poly_pressure(tsf* f, int channel, int key, int pressure);
TSFDEF int tsf_channel_set_sostenuto(tsf* f, int channel, int flag_sostenuto);
#define tsf_channel_set_keypressure tsf_channel_set_poly_pressure // older name

// Start or stop playing notes on a channel (needs channel preset to be set)
//   channel: channel number
//   key: note value between 0 and 127 (60 being middle C)
//   vel: velocity as a float between 0.0 (equal to note off) and 1.0 (full)
//   (tsf_channel_note_on returns 0 for an invalid channel or key, or on allocation failure of new voice, otherwise 1)
TSFDEF int tsf_channel_note_on(tsf* f, int channel, int key, float vel);
TSFDEF void tsf_channel_note_off(tsf* f, int channel, int key);
TSFDEF void tsf_channel_note_off_all(tsf* f, int channel); //end with sustain and release (pedals do not keep the notes, CC123 does wait for them)
TSFDEF void tsf_channel_sounds_off_all(tsf* f, int channel); //end quickly (10 ms fade out)

// Apply a MIDI control change to the channel. Every controller is stored and available as modulator source
// (the SoundFont's modulators handle volume, expression, pan, modulation wheel, ...). Bank select, RPN/data entry,
// data increment/decrement, sustain, hold 2, sostenuto, soft pedal, legato footswitch, portamento and the channel mode controllers
// 120/121/123-127 are additionally handled directly.
//    (tsf_channel_midi_control returns 0 for an invalid channel or controller number, or on allocation failure of new channel, otherwise 1)
TSFDEF int tsf_channel_midi_control(tsf* f, int channel, int controller, int control_value);

// Get current values set on the channels
TSFDEF int tsf_channel_get_preset_index(tsf* f, int channel);
TSFDEF int tsf_channel_get_preset_bank(tsf* f, int channel);
TSFDEF int tsf_channel_get_preset_number(tsf* f, int channel);
TSFDEF float tsf_channel_get_pan(tsf* f, int channel);
TSFDEF float tsf_channel_get_volume(tsf* f, int channel);
TSFDEF int tsf_channel_get_pitchwheel(tsf* f, int channel);
TSFDEF float tsf_channel_get_pitchrange(tsf* f, int channel);
TSFDEF float tsf_channel_get_tuning(tsf* f, int channel);

#ifdef __cplusplus
#  undef CPP_DEFAULT0
}
#endif

// end header
// ---------------------------------------------------------------------------------------------------------
#endif //TSF_INCLUDE_TSF_INL

#ifdef TSF_IMPLEMENTATION
#undef TSF_IMPLEMENTATION

// The lower this block size is the more accurate the effects are.
// Increasing the value significantly lowers the CPU usage of the voice rendering.
// If LFO affects the low-pass filter it can be hearable even as low as 8.
#ifndef TSF_RENDER_EFFECTSAMPLEBLOCK
#define TSF_RENDER_EFFECTSAMPLEBLOCK 64
#endif

// When using tsf_render_short, to do the conversion a buffer of a fixed size is
// allocated on the stack. On low memory platforms this could be made smaller.
// Increasing this above 512 should not have a significant impact on performance.
// The value should be a multiple of 2 * TSF_RENDER_EFFECTSAMPLEBLOCK.
#ifndef TSF_RENDER_SHORTBUFFERBLOCK
#define TSF_RENDER_SHORTBUFFERBLOCK 512
#endif

// EMU8000 compatibility mode
// 0: the initialAttenuation generator counts 1:1 centibels as in the SoundFont 2.01 specification
// 1: scale initialAttenuation generator by 0.4 like FluidSynth and EMU8000 hardware do (many SoundFonts expect it)
#ifndef EMU8K_COMPAT
#define EMU8K_COMPAT 1
#endif

// Quick voice off fade and the minimum volume envelope release, in seconds (avoid clicking noise)
#define TSF_FASTRELEASETIME 0.01f

// Minimum note length (seconds, 0 = off): an earlier note off waits until then, so zero-length notes (drum hits) are heard
#ifndef TSF_MIN_NOTE_LENGTH
#define TSF_MIN_NOTE_LENGTH 0.01f
#endif

#include <stddef.h>

#if !defined(TSF_MALLOC) || !defined(TSF_FREE) || !defined(TSF_REALLOC)
#  include <stdlib.h>
#  define TSF_MALLOC  malloc
#  define TSF_FREE    free
#  define TSF_REALLOC realloc
#endif

#if !defined(TSF_MEMCPY) || !defined(TSF_MEMSET)
#  include <string.h>
#  ifndef TSF_MEMCPY
#  define TSF_MEMCPY  memcpy
#  endif
#  ifndef TSF_MEMSET
#  define TSF_MEMSET  memset
#  endif
#endif

#if !defined(TSF_EXP) || !defined(TSF_EXPF) || !defined(TSF_LOGF) || !defined(TSF_SQRT) || !defined(TSF_TAN)
#  include <math.h>
#  if !defined(__cplusplus) && !defined(NAN) && !defined(expf) && !defined(logf)
#    define expf (float)exp // deal with old math.h
#    define logf (float)log // files that come without
#  endif // expf and logf
#  ifndef TSF_EXP
#  define TSF_EXP     exp
#  endif
#  ifndef TSF_EXPF
#  define TSF_EXPF    expf
#  endif
#  ifndef TSF_LOGF
#  define TSF_LOGF    logf
#  endif
#  ifndef TSF_SQRT
#  define TSF_SQRT    sqrt
#  endif
#  ifndef TSF_TAN
#  define TSF_TAN     tan
#  endif
#endif

#ifndef TSF_NO_STDIO
#  include <stdio.h>
#endif

#define TSF_TRUE 1
#define TSF_FALSE 0
#define TSF_BOOL unsigned char
#define TSF_PI 3.14159265358979323846264338327950288
#define TSF_LN2_1200 0.000577622650466621 // ln(2) / 1200: 2^(cents / 1200) = exp(cents * TSF_LN2_1200)
#define TSF_NULL NULL

#if EMU8K_COMPAT
#define TSF_ATTENUATION_GEN_SCALE 0.4f // FluidSynth/EMU8000: the initialAttenuation generator counts 40%
#else
#define TSF_ATTENUATION_GEN_SCALE 1.0f
#endif

#if TSF_RENDER_EFFECTSAMPLEBLOCK < 1 || TSF_RENDER_EFFECTSAMPLEBLOCK > 256 || (256 % TSF_RENDER_EFFECTSAMPLEBLOCK) != 0
#error TSF_RENDER_EFFECTSAMPLEBLOCK must be a power of two and <= 256
#endif
#if TSF_RENDER_SHORTBUFFERBLOCK < 1 || (TSF_RENDER_SHORTBUFFERBLOCK % (2 * TSF_RENDER_EFFECTSAMPLEBLOCK)) != 0
#error TSF_RENDER_SHORTBUFFERBLOCK must be a multiple of 2 * TSF_RENDER_EFFECTSAMPLEBLOCK
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef char tsf_fourcc[4];
typedef signed char tsf_s8;
typedef unsigned char tsf_u8;
typedef unsigned short tsf_u16;
typedef signed short tsf_s16;
typedef unsigned int tsf_u32;
typedef unsigned long long tsf_u64;
typedef char tsf_char20[20];

#if !defined(WORDS_BIGENDIAN) && defined(__BYTE_ORDER__) && defined(__ORDER_BIG_ENDIAN__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define WORDS_BIGENDIAN
#endif
#ifdef WORDS_BIGENDIAN
#ifndef TSF_SWAP16LE
#define TSF_SWAP16LE tsf_swap16le
static tsf_u16 tsf_swap16le(tsf_u16 x) { return (tsf_u16) ((x << 8) | (x >> 8)); }
#endif
#ifndef TSF_SWAP32LE
#define TSF_SWAP32LE tsf_swap32le
static tsf_u32 tsf_swap32le(tsf_u32 x) { return (tsf_u32) ((x << 24) | ((x << 8) & 0x00FF0000) | ((x >>  8) & 0x0000FF00) | (x >> 24)); }
#endif
#else /* Little Endian : */
#ifndef TSF_SWAP16LE
#define TSF_SWAP16LE(x) x
#endif
#ifndef TSF_SWAP32LE
#define TSF_SWAP32LE(x) x
#endif
#endif /* WORDS_BIGENDIAN */

#define TSF_FourCCEquals(value1, value2) ((value1)[0] == (value2)[0] && (value1)[1] == (value2)[1] && (value1)[2] == (value2)[2] && (value1)[3] == (value2)[3])
#define TSF_NUMGENS 59 // number of generators

struct tsf_mod { tsf_u16 src, dest, amtSrc, trans; tsf_s16 amount; };
struct tsf
{
	struct tsf_preset* presets;
	short* fontSamples;
	struct tsf_voice* voices;
	struct tsf_channels* channels;
	struct tsf_fx* fx; // optional built-in effects, allocated when enabled
	struct tsf_mod* mods;
	tsf_u32 modCCs[4]; // bit set of the MIDI controllers used as source by any modulator in the font
	int modNum;

	int presetNum;
	int voiceNum;
	int maxVoiceNum;
	unsigned int voicePlayIndex;

	enum TSFOutputMode outputmode;
	float outSampleRate;
	float globalGainDB;
	int* refCount;
};

#ifndef TSF_NO_STDIO
static int tsf_stream_stdio_read(FILE* f, void* ptr, unsigned int size) { return (int)fread(ptr, 1, size, f); }
static int tsf_stream_stdio_skip(FILE* f, unsigned int count) // in steps that fit a 32-bit long (a larger count would seek backwards)
{
	for (; count > 0x7FFFFFFF; count -= 0x7FFFFFFF) if (fseek(f, 0x7FFFFFFF, SEEK_CUR)) return 0;
	return !fseek(f, (long)count, SEEK_CUR);
}
TSFDEF tsf* tsf_load_filename(const char* filename)
{
	tsf* res;
	struct tsf_stream stream = { TSF_NULL, (int(*)(void*,void*,unsigned int))&tsf_stream_stdio_read, (int(*)(void*,unsigned int))&tsf_stream_stdio_skip };
	#if __STDC_WANT_SECURE_LIB__
	FILE* f = TSF_NULL; fopen_s(&f, filename, "rb");
	#else
	FILE* f = fopen(filename, "rb");
	#endif
	if (!f)
	{
		//if (e) *e = TSF_FILENOTFOUND;
		return TSF_NULL;
	}
	stream.data = f;
	res = tsf_load(&stream);
	fclose(f);
	return res;
}
#endif

struct tsf_stream_memory { const char* buffer; unsigned int total, pos; };
static int tsf_stream_memory_read(struct tsf_stream_memory* m, void* ptr, unsigned int size) { if (size > m->total - m->pos) size = m->total - m->pos; TSF_MEMCPY(ptr, m->buffer+m->pos, size); m->pos += size; return size; }
static int tsf_stream_memory_skip(struct tsf_stream_memory* m, unsigned int count) { if (count > m->total - m->pos) return 0; m->pos += count; return 1; }
TSFDEF tsf* tsf_load_memory(const void* buffer, int size)
{
	struct tsf_stream stream = { TSF_NULL, (int(*)(void*,void*,unsigned int))&tsf_stream_memory_read, (int(*)(void*,unsigned int))&tsf_stream_memory_skip };
	struct tsf_stream_memory f = { TSF_NULL, 0, 0 };
	f.buffer = (const char*)buffer;
	f.total = (size > 0 ? (unsigned int)size : 0);
	stream.data = &f;
	return tsf_load(&stream);
}

enum { TSF_LOOPMODE_NONE, TSF_LOOPMODE_CONTINUOUS, TSF_LOOPMODE_SUSTAIN };

enum { TSF_SEGMENT_NONE, TSF_SEGMENT_DELAY, TSF_SEGMENT_ATTACK, TSF_SEGMENT_HOLD, TSF_SEGMENT_DECAY, TSF_SEGMENT_SUSTAIN, TSF_SEGMENT_RELEASE, TSF_SEGMENT_DONE };

struct tsf_hydra
{
	struct tsf_hydra_phdr *phdrs; struct tsf_hydra_pbag *pbags; struct tsf_hydra_pmod *pmods;
	struct tsf_hydra_pgen *pgens; struct tsf_hydra_inst *insts; struct tsf_hydra_ibag *ibags;
	struct tsf_hydra_imod *imods; struct tsf_hydra_igen *igens; struct tsf_hydra_shdr *shdrs;
	int phdrNum, pbagNum, pmodNum, pgenNum, instNum, ibagNum, imodNum, igenNum, shdrNum;
};

#ifdef WORDS_BIGENDIAN
union tsf_hydra_genamount { struct { tsf_u8 hi, lo; } range; tsf_s16 shortAmount; tsf_u16 wordAmount; };
#else
union tsf_hydra_genamount { struct { tsf_u8 lo, hi; } range; tsf_s16 shortAmount; tsf_u16 wordAmount; };
#endif
struct tsf_hydra_phdr { tsf_char20 presetName; tsf_u16 preset, bank, presetBagNdx; tsf_u32 library, genre, morphology; };
struct tsf_hydra_pbag { tsf_u16 genNdx, modNdx; };
struct tsf_hydra_pmod { tsf_u16 modSrcOper, modDestOper; tsf_s16 modAmount; tsf_u16 modAmtSrcOper, modTransOper; };
struct tsf_hydra_pgen { tsf_u16 genOper; union tsf_hydra_genamount genAmount; };
struct tsf_hydra_inst { tsf_char20 instName; tsf_u16 instBagNdx; };
struct tsf_hydra_ibag { tsf_u16 instGenNdx, instModNdx; };
struct tsf_hydra_imod { tsf_u16 modSrcOper, modDestOper; tsf_s16 modAmount; tsf_u16 modAmtSrcOper, modTransOper; };
struct tsf_hydra_igen { tsf_u16 genOper; union tsf_hydra_genamount genAmount; };
struct tsf_hydra_shdr { tsf_char20 sampleName; tsf_u32 start, end, startLoop, endLoop, sampleRate; tsf_u8 originalPitch; tsf_s8 pitchCorrection; tsf_u16 sampleLink, sampleType; };

#define TSFR(FIELD) stream->read(stream->data, &i->FIELD, sizeof(i->FIELD));
#define TSFR_ARR(FIELD) stream->read(stream->data, i->FIELD, sizeof(i->FIELD));

#define TSFR16(x) {TSFR(x); i->x = TSF_SWAP16LE(i->x);};
#define TSFR32(x) {TSFR(x); i->x = TSF_SWAP32LE(i->x);};
#define TSFRGA(x) {TSFR(x); i->x.wordAmount = TSF_SWAP16LE(i->x.wordAmount); };

static void tsf_hydra_read_phdr(struct tsf_hydra_phdr* i, struct tsf_stream* stream) { TSFR_ARR(presetName) TSFR16(preset) TSFR16(bank) TSFR16(presetBagNdx) TSFR32(library) TSFR32(genre) TSFR32(morphology) }
static void tsf_hydra_read_pbag(struct tsf_hydra_pbag* i, struct tsf_stream* stream) { TSFR16(genNdx) TSFR16(modNdx) }
static void tsf_hydra_read_pmod(struct tsf_hydra_pmod* i, struct tsf_stream* stream) { TSFR16(modSrcOper) TSFR16(modDestOper) TSFR16(modAmount) TSFR16(modAmtSrcOper) TSFR16(modTransOper) }
static void tsf_hydra_read_pgen(struct tsf_hydra_pgen* i, struct tsf_stream* stream) { TSFR16(genOper) TSFRGA(genAmount) }
static void tsf_hydra_read_inst(struct tsf_hydra_inst* i, struct tsf_stream* stream) { TSFR_ARR(instName) TSFR16(instBagNdx) }
static void tsf_hydra_read_ibag(struct tsf_hydra_ibag* i, struct tsf_stream* stream) { TSFR16(instGenNdx) TSFR16(instModNdx) }
static void tsf_hydra_read_imod(struct tsf_hydra_imod* i, struct tsf_stream* stream) { TSFR16(modSrcOper) TSFR16(modDestOper) TSFR16(modAmount) TSFR16(modAmtSrcOper) TSFR16(modTransOper) }
static void tsf_hydra_read_igen(struct tsf_hydra_igen* i, struct tsf_stream* stream) { TSFR16(genOper) TSFRGA(genAmount) }
static void tsf_hydra_read_shdr(struct tsf_hydra_shdr* i, struct tsf_stream* stream) { TSFR_ARR(sampleName) TSFR32(start) TSFR32(end) TSFR32(startLoop) TSFR32(endLoop) TSFR32(sampleRate) TSFR(originalPitch) TSFR(pitchCorrection) TSFR16(sampleLink) TSFR16(sampleType) }

#undef TSFRGA
#undef TSFR32
#undef TSFR16
#undef TSFR
#undef TSFR_ARR

struct tsf_riffchunk { tsf_fourcc id; tsf_u32 size; };
struct tsf_envelope { float delay, attack, hold, decay, sustain, release, keynumToHold, keynumToDecay; };
struct tsf_voice_envelope { unsigned char segment, segmentIsExponential : 1, isAmpEnv : 1; short midiVelocity; float level, slope, slopeBlock; int samplesUntilNextSegment; struct tsf_envelope parameters; };
struct tsf_voice_lowpass { double QInv, gain, k1, p, q, ic1, ic2; TSF_BOOL active, resonant; };
struct tsf_voice_lfo { int samplesUntil; float level, delta; };

struct tsf_region
{
	int loop_mode;
	unsigned int sample_rate;
	unsigned char lokey, hikey, lovel, hivel;
	unsigned int group, offset, end, loop_start, loop_end; // sample positions (end is exclusive, loop_end inclusive)
	unsigned int sample_start, sample_end; // the sample's own data (the sample header's start and end), all positions stay inside it
	int addrOfs[8]; // address offset generators: start, end, loop start, loop end (fine), then the same in 32768 steps (coarse)
	int transpose, tune, pitch_keycenter, pitch_keytrack;
	int fixedKey, fixedVel; // keynum and velocity generators: the key/velocity the note is played as, -1 if not set (or illegal)
	float attenuation, pan;
	struct tsf_envelope ampenv, modenv;
	int initialFilterQ, initialFilterFc;
	int modEnvToPitch, modEnvToFilterFc, modLfoToFilterFc, modLfoToVolume;
	float delayModLFO;
	int freqModLFO, modLfoToPitch;
	float delayVibLFO;
	int freqVibLFO, vibLfoToPitch;
	float chorusSend, reverbSend; // 0..1000 (0.1%)
	unsigned int modIdx; // start of this region's modulator list in tsf.mods
};

struct tsf_preset
{
	tsf_char20 presetName;
	tsf_u16 preset, bank;
	struct tsf_region* regions;
	int regionNum;
};

struct tsf_voice
{
	int playingPreset, playingKey, playingChannel, heldSustain;
	struct tsf_region* region;
	double pitchInputTimecents, pitchOutputFactor;
	tsf_u64 sourceSamplePosition; // 32.32 fixed point
	float  noteGainDB, panFactorLeft, panFactorRight;
	unsigned int playIndex, loopStart, loopEnd;
	struct tsf_voice_envelope ampenv, modenv;
	struct tsf_voice_lowpass lowpass;
	struct tsf_voice_lfo modlfo, viblfo;
	unsigned char velocity, key, keyPressure; // modulator sources (the keynum/velocity generators can differ from the played key/velocity)
	short softFc; // cutoff reduction in cents from the soft pedal (CC67) at the time the note started
	unsigned char sostenuto; // held by the sostenuto pedal
	float portaCents, portaDelta; // remaining portamento glide (cents) and its change per sample
	float chorusSend, reverbSend; // 0..1 effect send levels
	unsigned char rampValid; // the gains below hold the end of the last rendered block (volume or pan changes ramp from there)
	unsigned char releasePending; // a note off came before the minimum note length
	int minLengthLeft; // samples left of the minimum note length
	float rampMono, rampLeft, rampRight;
	struct tsf_region mr; // the region with the modulator outputs applied
};

struct tsf_channel
{
	unsigned short presetIndex, bank, pitchWheel, midiRPN, midiData : 14, sustain : 1, sostenuto : 1, monoMode : 1, hold2 : 1;
	unsigned char portaKey; // CC84 portamento source key for the next note (255 = none)
	unsigned char lastKey; // the most recently played key, where portamento glides from (255 = none)
	float panOffset, gainDB, pitchRange, tuning;
	float modDepth; // RPN 5 modulation depth range relative to the default of 50 cents (scales the mod wheel pitch modulators)
	unsigned char pressure, cc[128]; // modulator sources
	unsigned char monoKeys[8], monoCount; // mono mode: the keys held down in the order they were pressed (the last one sounds)
	signed char tuneCoarse; float tuneFine; // RPN 2 and RPN 1 (semitones), their sum is the channel tuning
	unsigned char nrpnMSB; unsigned short nrpnSel, nrpnAcc; // SF2.01 NRPN (9.6): selected generator (0xFFFF none) and the 100/1000/10000 steps
	short nrpn[TSF_NUMGENS]; // SF2.01 NRPN data entry per generator (offset from 8192), added like a modulator output
	unsigned int aweSet; short awe[27]; // AWE32 NRPN (MSB 127): the parameters set (bits) and their values in generator units
};

struct tsf_channels
{
	int channelNum;
	float glideCents, glideDelta; // portamento handed to the voices of the note being started
	struct tsf_channel channels[1];
};

static double tsf_timecents2Secsd(double timecents) { return TSF_EXP(timecents * TSF_LN2_1200); }
static float tsf_cents2Hertz(float cents) { return 8.176f * TSF_EXPF(cents * (float)TSF_LN2_1200); }
static float tsf_decibelsToGain(float db) { return (db > -100.f ? TSF_EXPF(db * 0.11512925f) : 0); }
#define TSF_FINITE(x) ((x) - (x) == 0.0f) // false for infinity and NaN, which the API's float arguments ignore
static float tsf_gainToDecibels(float gain) { return (gain <= 1e-10f ? -200.f : 8.6858896f * TSF_LOGF(gain)); }

static TSF_BOOL tsf_riffchunk_read(struct tsf_riffchunk* parent, struct tsf_riffchunk* chunk, struct tsf_stream* stream)
{
	TSF_BOOL IsRiff, IsList;
	// A chunk of odd size is followed by a pad byte (some tools writing .sfo files leave it out). A zero byte can't start a chunk
	// id, so one there is taken as the pad, and one byte left at the end of a list is too.
	if (parent && parent->size == 1) { stream->skip(stream->data, 1); parent->size = 0; return TSF_FALSE; }
	if (parent && sizeof(tsf_fourcc) + sizeof(tsf_u32) > parent->size) return TSF_FALSE;
	if (!stream->read(stream->data, chunk->id, sizeof(tsf_fourcc))) return TSF_FALSE;
	if (!*chunk->id) { chunk->id[0] = chunk->id[1]; chunk->id[1] = chunk->id[2]; chunk->id[2] = chunk->id[3]; if (!stream->read(stream->data, &chunk->id[3], 1)) return TSF_FALSE; if (parent && parent->size) parent->size--; }
	if (*chunk->id <= ' ' || *chunk->id >= 'z') return TSF_FALSE;
	if (!stream->read(stream->data, &chunk->size, sizeof(tsf_u32))) return TSF_FALSE;
	chunk->size = TSF_SWAP32LE(chunk->size);
	if (parent && (parent->size < sizeof(tsf_fourcc) + sizeof(tsf_u32) || chunk->size > parent->size - (sizeof(tsf_fourcc) + sizeof(tsf_u32)))) return TSF_FALSE; // (no wraparound with a 32-bit size_t)
	if (parent) parent->size -= sizeof(tsf_fourcc) + sizeof(tsf_u32) + chunk->size;
	IsRiff = TSF_FourCCEquals(chunk->id, "RIFF"), IsList = TSF_FourCCEquals(chunk->id, "LIST");
	if (IsRiff && parent) return TSF_FALSE; //not allowed
	if (!IsRiff && !IsList) return TSF_TRUE; //custom type without sub type
	if (chunk->size < sizeof(tsf_fourcc)) return TSF_FALSE; //no room for the list type
	if (!stream->read(stream->data, chunk->id, sizeof(tsf_fourcc)) || *chunk->id <= ' ' || *chunk->id >= 'z') return TSF_FALSE;
	chunk->size -= sizeof(tsf_fourcc);
	return TSF_TRUE;
}

static void tsf_region_clear(struct tsf_region* i, TSF_BOOL for_relative)
{
	TSF_MEMSET(i, 0, sizeof(struct tsf_region));
	i->hikey = i->hivel = 127;
	i->pitch_keycenter = 60; // C4
	if (for_relative) return;

	i->pitch_keytrack = 100;
	i->fixedKey = i->fixedVel = -1;

	i->pitch_keycenter = -1;

	// SF2 defaults in timecents.
	i->ampenv.delay = i->ampenv.attack = i->ampenv.hold = i->ampenv.decay = i->ampenv.release = -12000.0f;
	i->modenv.delay = i->modenv.attack = i->modenv.hold = i->modenv.decay = i->modenv.release = -12000.0f;

	i->initialFilterFc = 13500;

	i->delayModLFO = -12000.0f;
	i->delayVibLFO = -12000.0f;
}

static void tsf_region_operator(struct tsf_region* region, tsf_u16 genOper, union tsf_hydra_genamount* amount, struct tsf_region* merge_region, float delta)
{
	enum
	{
		_GEN_TYPE_MASK       = 0x0F,
		GEN_FLOAT            = 0x01,
		GEN_INT              = 0x02,
		GEN_KEYRANGE         = 0x05,
		GEN_VELRANGE         = 0x06,
		GEN_LOOPMODE         = 0x07,
		GEN_GROUP            = 0x08,
		GEN_KEYCENTER        = 0x09,

		_GEN_LIMIT_MASK      = 0xF0,
		GEN_INT_LIMIT12K     = 0x10, //min -12000, max 12000
		GEN_INT_LIMITFC      = 0x20, //min 1500, max 13500
		GEN_INT_LIMITQ       = 0x30, //min 0, max 960
		GEN_INT_LIMIT960     = 0x40, //min -960, max 960
		GEN_INT_LIMIT16K4500 = 0x50, //min -16000, max 4500
		GEN_FLOAT_LIMIT12K5K = 0x60, //min -12000, max 5000
		GEN_FLOAT_LIMIT12K8K = 0x70, //min -12000, max 8000
		GEN_FLOAT_LIMIT1200  = 0x80, //min -1200, max 1200
		GEN_FLOAT_LIMITPAN   = 0x90, //min -500, max 500
		GEN_FLOAT_LIMITATTN  = 0xA0, //min 0, max 1440
		GEN_FLOAT_MAX1000    = 0xB0, //min 0, max 1000
		GEN_FLOAT_MAX1440    = 0xC0, //min 0, max 1440
		GEN_INT_MAX1200      = 0xD0, //min 0, max 1200
		GEN_INT_LIMIT120     = 0xE0, //min -120, max 120

		_GEN_MAX = TSF_NUMGENS
	};
	#if 0
	#define _TSFREGIONOFFSET(TYPE, FIELD) (unsigned char)(((TYPE*)&((struct tsf_region*)0)->FIELD) - (TYPE*)0)
	#define _TSFREGIONENVOFFSET(TYPE, ENV, FIELD) (unsigned char)(((TYPE*)&((&(((struct tsf_region*)0)->ENV))->FIELD)) - (TYPE*)0)
	#else
	#define _TSFREGIONOFFSET(TYPE, FIELD) (unsigned char)(offsetof(struct tsf_region,FIELD) / sizeof(TYPE))
	#define _TSFREGIONENVOFFSET(TYPE, ENV, FIELD) (unsigned char)((offsetof(struct tsf_region,ENV) + offsetof(struct tsf_envelope,FIELD)) / sizeof(TYPE))
	#endif
	#define _TSFREGIONADDROFFSET(I) (unsigned char)(offsetof(struct tsf_region,addrOfs) / sizeof(int) + (I)) // no subscript inside offsetof (not a constant for MSVC)
	static const struct { unsigned char mode, offset; } genMetas[_GEN_MAX] =
	{
		{ GEN_INT                          , _TSFREGIONADDROFFSET(0)                               }, // 0 StartAddrsOffset
		{ GEN_INT                          , _TSFREGIONADDROFFSET(1)                               }, // 1 EndAddrsOffset
		{ GEN_INT                          , _TSFREGIONADDROFFSET(2)                               }, // 2 StartloopAddrsOffset
		{ GEN_INT                          , _TSFREGIONADDROFFSET(3)                               }, // 3 EndloopAddrsOffset
		{ GEN_INT                          , _TSFREGIONADDROFFSET(4)                               }, // 4 StartAddrsCoarseOffset
		{ GEN_INT   | GEN_INT_LIMIT12K     , _TSFREGIONOFFSET(         int, modLfoToPitch        ) }, // 5 ModLfoToPitch
		{ GEN_INT   | GEN_INT_LIMIT12K     , _TSFREGIONOFFSET(         int, vibLfoToPitch        ) }, // 6 VibLfoToPitch
		{ GEN_INT   | GEN_INT_LIMIT12K     , _TSFREGIONOFFSET(         int, modEnvToPitch        ) }, // 7 ModEnvToPitch
		{ GEN_INT   | GEN_INT_LIMITFC      , _TSFREGIONOFFSET(         int, initialFilterFc      ) }, // 8 InitialFilterFc
		{ GEN_INT   | GEN_INT_LIMITQ       , _TSFREGIONOFFSET(         int, initialFilterQ       ) }, // 9 InitialFilterQ
		{ GEN_INT   | GEN_INT_LIMIT12K     , _TSFREGIONOFFSET(         int, modLfoToFilterFc     ) }, //10 ModLfoToFilterFc
		{ GEN_INT   | GEN_INT_LIMIT12K     , _TSFREGIONOFFSET(         int, modEnvToFilterFc     ) }, //11 ModEnvToFilterFc
		{ GEN_INT                          , _TSFREGIONADDROFFSET(5)                               }, //12 EndAddrsCoarseOffset
		{ GEN_INT   | GEN_INT_LIMIT960     , _TSFREGIONOFFSET(         int, modLfoToVolume       ) }, //13 ModLfoToVolume
		{ 0                                , (0                                                  ) }, //   Unused
		{ GEN_FLOAT | GEN_FLOAT_MAX1000    , _TSFREGIONOFFSET(       float, chorusSend           ) }, //15 ChorusEffectsSend
		{ GEN_FLOAT | GEN_FLOAT_MAX1000    , _TSFREGIONOFFSET(       float, reverbSend           ) }, //16 ReverbEffectsSend
		{ GEN_FLOAT | GEN_FLOAT_LIMITPAN   , _TSFREGIONOFFSET(       float, pan                  ) }, //17 Pan
		{ 0                                , (0                                                  ) }, //   Unused
		{ 0                                , (0                                                  ) }, //   Unused
		{ 0                                , (0                                                  ) }, //   Unused
		{ GEN_FLOAT | GEN_FLOAT_LIMIT12K5K , _TSFREGIONOFFSET(       float, delayModLFO          ) }, //21 DelayModLFO
		{ GEN_INT   | GEN_INT_LIMIT16K4500 , _TSFREGIONOFFSET(         int, freqModLFO           ) }, //22 FreqModLFO
		{ GEN_FLOAT | GEN_FLOAT_LIMIT12K5K , _TSFREGIONOFFSET(       float, delayVibLFO          ) }, //23 DelayVibLFO
		{ GEN_INT   | GEN_INT_LIMIT16K4500 , _TSFREGIONOFFSET(         int, freqVibLFO           ) }, //24 FreqVibLFO
		{ GEN_FLOAT | GEN_FLOAT_LIMIT12K5K , _TSFREGIONENVOFFSET(    float, modenv, delay        ) }, //25 DelayModEnv
		{ GEN_FLOAT | GEN_FLOAT_LIMIT12K8K , _TSFREGIONENVOFFSET(    float, modenv, attack       ) }, //26 AttackModEnv
		{ GEN_FLOAT | GEN_FLOAT_LIMIT12K5K , _TSFREGIONENVOFFSET(    float, modenv, hold         ) }, //27 HoldModEnv
		{ GEN_FLOAT | GEN_FLOAT_LIMIT12K8K , _TSFREGIONENVOFFSET(    float, modenv, decay        ) }, //28 DecayModEnv
		{ GEN_FLOAT | GEN_FLOAT_MAX1000    , _TSFREGIONENVOFFSET(    float, modenv, sustain      ) }, //29 SustainModEnv
		{ GEN_FLOAT | GEN_FLOAT_LIMIT12K8K , _TSFREGIONENVOFFSET(    float, modenv, release      ) }, //30 ReleaseModEnv
		{ GEN_FLOAT | GEN_FLOAT_LIMIT1200  , _TSFREGIONENVOFFSET(    float, modenv, keynumToHold ) }, //31 KeynumToModEnvHold
		{ GEN_FLOAT | GEN_FLOAT_LIMIT1200  , _TSFREGIONENVOFFSET(    float, modenv, keynumToDecay) }, //32 KeynumToModEnvDecay
		{ GEN_FLOAT | GEN_FLOAT_LIMIT12K5K , _TSFREGIONENVOFFSET(    float, ampenv, delay        ) }, //33 DelayVolEnv
		{ GEN_FLOAT | GEN_FLOAT_LIMIT12K8K , _TSFREGIONENVOFFSET(    float, ampenv, attack       ) }, //34 AttackVolEnv
		{ GEN_FLOAT | GEN_FLOAT_LIMIT12K5K , _TSFREGIONENVOFFSET(    float, ampenv, hold         ) }, //35 HoldVolEnv
		{ GEN_FLOAT | GEN_FLOAT_LIMIT12K8K , _TSFREGIONENVOFFSET(    float, ampenv, decay        ) }, //36 DecayVolEnv
		{ GEN_FLOAT | GEN_FLOAT_MAX1440    , _TSFREGIONENVOFFSET(    float, ampenv, sustain      ) }, //37 SustainVolEnv
		{ GEN_FLOAT | GEN_FLOAT_LIMIT12K8K , _TSFREGIONENVOFFSET(    float, ampenv, release      ) }, //38 ReleaseVolEnv
		{ GEN_FLOAT | GEN_FLOAT_LIMIT1200  , _TSFREGIONENVOFFSET(    float, ampenv, keynumToHold ) }, //39 KeynumToVolEnvHold
		{ GEN_FLOAT | GEN_FLOAT_LIMIT1200  , _TSFREGIONENVOFFSET(    float, ampenv, keynumToDecay) }, //40 KeynumToVolEnvDecay
		{ 0                                , (0                                                  ) }, //   Instrument (special)
		{ 0                                , (0                                                  ) }, //   Reserved
		{ GEN_KEYRANGE                     , (0                                                  ) }, //43 KeyRange
		{ GEN_VELRANGE                     , (0                                                  ) }, //44 VelRange
		{ GEN_INT                          , _TSFREGIONADDROFFSET(6)                               }, //45 StartloopAddrsCoarseOffset
		{ GEN_INT                          , _TSFREGIONOFFSET(         int, fixedKey             ) }, //46 Keynum
		{ GEN_INT                          , _TSFREGIONOFFSET(         int, fixedVel             ) }, //47 Velocity
		{ GEN_FLOAT | GEN_FLOAT_LIMITATTN  , _TSFREGIONOFFSET(       float, attenuation          ) }, //48 InitialAttenuation
		{ 0                                , (0                                                  ) }, //   Reserved
		{ GEN_INT                          , _TSFREGIONADDROFFSET(7)                               }, //50 EndloopAddrsCoarseOffset
		{ GEN_INT   | GEN_INT_LIMIT120     , _TSFREGIONOFFSET(         int, transpose            ) }, //51 CoarseTune
		{ GEN_INT                          , _TSFREGIONOFFSET(         int, tune                 ) }, //52 FineTune
		{ 0                                , (0                                                  ) }, //   SampleID (special)
		{ GEN_LOOPMODE                     , _TSFREGIONOFFSET(         int, loop_mode            ) }, //54 SampleModes
		{ 0                                , (0                                                  ) }, //   Reserved
		{ GEN_INT   | GEN_INT_MAX1200      , _TSFREGIONOFFSET(         int, pitch_keytrack       ) }, //56 ScaleTuning
		{ GEN_GROUP                        , _TSFREGIONOFFSET(unsigned int, group                ) }, //57 ExclusiveClass
		{ GEN_KEYCENTER                    , _TSFREGIONOFFSET(         int, pitch_keycenter      ) }, //58 OverridingRootKey
	};
	#undef _TSFREGIONOFFSET
	#undef _TSFREGIONENVOFFSET
	#undef _TSFREGIONADDROFFSET
	// clamping range per limit code (the high nibble of the generator mode)
	static const short limits[15][2] = { {0,0}, {-12000,12000}, {1500,13500}, {0,960}, {-960,960}, {-16000,4500}, {-12000,5000}, {-12000,8000}, {-1200,1200}, {-500,500}, {0,1440}, {0,1000}, {0,1440}, {0,1200}, {-120,120} };
	if (amount)
	{
		int offset;
		if (genOper >= _GEN_MAX) return;
		offset = genMetas[genOper].offset;
		switch (genMetas[genOper].mode & _GEN_TYPE_MASK)
		{
			case GEN_FLOAT:      ((       float*)region)[offset]  = amount->shortAmount;     return;
			case GEN_INT:        ((         int*)region)[offset]  = amount->shortAmount;     return;
			case GEN_KEYRANGE:   region->lokey = amount->range.lo; region->hikey = amount->range.hi; return;
			case GEN_VELRANGE:   region->lovel = amount->range.lo; region->hivel = amount->range.hi; return;
			case GEN_LOOPMODE:   region->loop_mode       = ((amount->wordAmount&3) == 3 ? TSF_LOOPMODE_SUSTAIN : ((amount->wordAmount&3) == 1 ? TSF_LOOPMODE_CONTINUOUS : TSF_LOOPMODE_NONE)); return;
			case GEN_GROUP:      region->group           = amount->wordAmount;  return;
			case GEN_KEYCENTER:  region->pitch_keycenter = amount->shortAmount; return;
		}
	}
	else if (merge_region) //add the preset level values (relative) to the instrument level values (absolute), clamping happens after modulation
	{
		for (genOper = 0; genOper != _GEN_MAX; genOper++)
		{
			int offset = genMetas[genOper].offset;
			switch (genMetas[genOper].mode & _GEN_TYPE_MASK)
			{
				case GEN_FLOAT: ((float*)region)[offset] += ((float*)merge_region)[offset]; continue;
				case GEN_INT:   ((int*)region)[offset]   += ((int*)merge_region)[offset];   continue;
			}
		}
	}
	else if (genOper < _GEN_MAX) //add an amount in generator units (modulator output) and clamp
	{
		unsigned char mode = genMetas[genOper].mode;
		float* fval = &((float*)region)[genMetas[genOper].offset], x;
		int* ival = &((int*)region)[genMetas[genOper].offset];
		if (!TSF_FINITE(delta)) return;
		if      ((mode & _GEN_TYPE_MASK) == GEN_FLOAT) x = *fval + delta;
		else if ((mode & _GEN_TYPE_MASK) == GEN_INT)   x = (float)*ival + delta;
		else return;
		if (x <= -32768.0f && genOper < 64 && (((tsf_u64)0xE0EA << 20) >> genOper & 1)) x = -32768.0f; // delays, attacks, holds: -32768 is zero time (8.1.2)
		else if (mode & _GEN_LIMIT_MASK)
		{
			float vmin = limits[mode >> 4][0], vmax = limits[mode >> 4][1];
			if      (x < vmin) x = vmin;
			else if (x > vmax) x = vmax;
		}
		if ((mode & _GEN_TYPE_MASK) == GEN_FLOAT) *fval = x;
		else *ival = (x < -2e9f ? -2000000000 : (x > 2e9f ? 2000000000 : (int)(x < 0.0f ? x - 0.5f : x + 0.5f)));
	}
}

// Convert (modulated, clamped) envelope parameters from timecents to seconds and the sustain level to a 0..1 value.
// The key number scaling of hold and decay is applied in timecents first (8.1.2 #31/#32/#39/#40), and the result is kept in the
// generator's range (up to 1200 tc/key * 67 keys would otherwise give times of 2^70 seconds, which overflow the sample counts).
// exact (8.1.3, 8.5): the minimum -12000 timecents is 1 ms, -32768 (delays, attacks, holds) is 0
static float tsf_timecents2Secs(float tc) { return (tc <= -32768.0f ? 0.0f : TSF_EXPF(tc * (float)TSF_LN2_1200)); }
static float tsf_timecents_clamp(float tc, float max) { return (tc < -12000.0f ? -12000.0f : (tc > max ? max : tc)); }
static void tsf_envelope_tosecs(struct tsf_envelope* p, int key, TSF_BOOL isAmpEnv)
{
	p->delay   = tsf_timecents2Secs(p->delay);
	p->attack  = tsf_timecents2Secs(p->attack);
	p->hold    = p->hold + p->keynumToHold * (60 - key);
	p->hold    = (p->hold <= -32768.0f ? 0.0f : tsf_timecents2Secs(tsf_timecents_clamp(p->hold, 5000.0f)));
	p->decay   = tsf_timecents2Secs(tsf_timecents_clamp(p->decay + p->keynumToDecay * (60 - key), 8000.0f));
	p->release = tsf_timecents2Secs(p->release);
	p->sustain = (isAmpEnv ? tsf_decibelsToGain(-p->sustain * 0.1f) : 1.0f - p->sustain * 0.001f); // cB attenuation / 0.1% decrease
}

// ---------------------------------------------------------------------------------------------------------
// Modulators (SoundFont 2.01 section 8), load time part

#define TSF_MAXMODS 64 // modulators per list, any additional ones in a SoundFont are ignored
#define TSF_MODEND  0xFFFF
struct tsf_modlist { int num; struct tsf_mod mods[TSF_MAXMODS]; };

// Default modulators (8.4), with two deliberate deviations (see TECHNICAL_DETAILS.md): CC10 to pan uses 500 instead of 1000,
// and the velocity to filter cutoff modulator (broken in SF2.01, changed in SF2.04) is left out
static const struct tsf_mod tsf_defmods[] =
{
	{ 0x0502, 48, 0x0000, 0,    960 }, // note-on velocity (concave, negative) to initial attenuation
	{ 0x000D,  6, 0x0000, 0,     50 }, // channel pressure to vibrato LFO pitch depth
	{ 0x0081,  6, 0x0000, 0,     50 }, // CC1 (modulation wheel) to vibrato LFO pitch depth
	{ 0x0587, 48, 0x0000, 0,    960 }, // CC7 (volume) (concave, negative) to initial attenuation
	{ 0x028A, 17, 0x0000, 0,    500 }, // CC10 (pan) (linear, bipolar) to pan
	{ 0x058B, 48, 0x0000, 0,    960 }, // CC11 (expression) (concave, negative) to initial attenuation
	{ 0x00DB, 16, 0x0000, 0,    200 }, // CC91 (reverb send) to reverb effects send
	{ 0x00DD, 15, 0x0000, 0,    200 }, // CC93 (chorus send) to chorus effects send
	{ 0x020E, 52, 0x0010, 0,  12700 }, // pitch wheel (linear, bipolar) scaled by pitch wheel sensitivity to fine tune
};

static TSF_BOOL tsf_mod_srcvalid(unsigned s, TSF_BOOL isAmountSrc)
{
	unsigned idx = s & 127;
	if ((s >> 10) > 3) return TSF_FALSE; // unknown curve type
	if (s & 128) return (idx && idx != 6 && (idx < 32 || idx > 63) && (idx < 98 || idx > 101) && idx < 120); // MIDI controller (32-63 are LSBs, 8.2.1)
	return (idx == 0 || idx == 2 || idx == 3 || idx == 10 || idx == 13 || idx == 14 || idx == 16 || (idx == 127 && !isAmountSrc));
}

// Merge a zone's modulators into a list (9.5): an identical modulator (same sources, destination and transform) replaces the one
// in the list, or at the preset level (add) adds its amount to it. A linked modulator (SF2.04) is merged after its target so
// its destination can be compared as an index into the merged list; links to ignored modulators and cycles are dropped.
static void tsf_mods_merge(struct tsf_modlist* l, const struct tsf_mod* z, int count, TSF_BOOL add)
{
	int i, j, pos[TSF_MAXMODS]; // index in l, -1 ignored, -2 not handled yet
	TSF_BOOL progress;
	if (count > TSF_MAXMODS) count = TSF_MAXMODS;
	for (i = 0; i != count; i++) pos[i] = -2;
	do
	{
		for (progress = TSF_FALSE, i = 0; i != count; i++)
		{
			struct tsf_mod m = z[i];
			if (pos[i] != -2) continue;
			if (m.dest & 0x8000)
			{
				int t = m.dest & 0x7FFF;
				if (t < count && pos[t] == -2) continue; // its target comes first
				if (t >= count || pos[t] < 0) { pos[i] = -1; progress = TSF_TRUE; continue; }
				m.dest = (tsf_u16)(0x8000 | pos[t]);
			}
			progress = TSF_TRUE;
			pos[i] = -1;
			if (!tsf_mod_srcvalid(m.src, TSF_FALSE) || !tsf_mod_srcvalid(m.amtSrc, TSF_TRUE) || (m.trans != 0 && m.trans != 2)) continue;
			if (m.dest < 0x8000 && (m.dest >= TSF_NUMGENS || m.dest == 46 || m.dest == 47)) continue; // not a modulatable generator
			for (j = 0; j != l->num; j++)
				if (l->mods[j].src == m.src && l->mods[j].amtSrc == m.amtSrc && l->mods[j].trans == m.trans && l->mods[j].dest == m.dest) break;
			if (j == l->num)
			{
				if (j == TSF_MAXMODS) continue;
				l->mods[l->num++] = m;
			}
			else
			{
				int a = (add ? l->mods[j].amount + m.amount : m.amount);
				l->mods[j].amount = (tsf_s16)(a < -32768 ? -32768 : (a > 32767 ? 32767 : a));
			}
			pos[i] = j;
		}
	} while (progress);
}

// Merge a zone's modulators as read from the file's PMOD or IMOD chunk (both records have the same layout)
static void tsf_mods_zone(struct tsf_modlist* l, const struct tsf_hydra_pmod* h, int from, int to, int num, TSF_BOOL add)
{
	struct tsf_mod z[TSF_MAXMODS]; int i;
	if (to > num) to = num;
	if (to - from > TSF_MAXMODS) to = from + TSF_MAXMODS;
	for (i = 0; from + i < to; i++)
	{
		z[i].src = h[from + i].modSrcOper; z[i].dest = h[from + i].modDestOper; z[i].amtSrc = h[from + i].modAmtSrcOper;
		z[i].trans = h[from + i].modTransOper; z[i].amount = (tsf_s16)h[from + i].modAmount;
	}
	tsf_mods_merge(l, z, i, add);
}

static TSF_BOOL tsf_mods_equal(const struct tsf_mod* a, const struct tsf_mod* b, int n)
{
	for (; n; n--, a++, b++)
		if (a->src != b->src || a->dest != b->dest || a->amtSrc != b->amtSrc || a->trans != b->trans || a->amount != b->amount) return TSF_FALSE;
	return TSF_TRUE;
}

// Add a final list to the font's shared pool (reusing an identical list if there is one), returns its index or -1 if out of memory
static int tsf_mods_pool(tsf* f, struct tsf_modlist* l)
{
	int i, n, p, len, map[TSF_MAXMODS];
	struct tsf_mod* m;
	TSF_BOOL dropped;
	do // modulators with a zero amount have no effect, neither do those linked into them: drop them (fonts zero out default ones)
	{
		for (dropped = TSF_FALSE, i = 0; i != l->num; i++)
			if (l->mods[i].amount && (l->mods[i].dest & 0x8000) && !l->mods[(l->mods[i].dest & 0x7FFF) % l->num].amount) l->mods[i].amount = 0, dropped = TSF_TRUE;
	} while (dropped);
	for (i = n = 0; i != l->num; i++) { map[i] = n; if (l->mods[i].amount) l->mods[n++] = l->mods[i]; }
	for (i = 0; i != n; i++) if (l->mods[i].dest & 0x8000) l->mods[i].dest = (tsf_u16)(0x8000 | map[l->mods[i].dest & 0x7FFF]);
	l->num = n;
	for (p = 0; p < f->modNum; p += len + 1)
	{
		for (len = 0; f->mods[p + len].dest != TSF_MODEND; len++) {}
		if (len == n && tsf_mods_equal(f->mods + p, l->mods, n)) return p;
	}
	m = (struct tsf_mod*)TSF_REALLOC(f->mods, (f->modNum + n + 1) * sizeof(struct tsf_mod));
	if (!m) return -1;
	f->mods = m;
	TSF_MEMCPY(m + f->modNum, l->mods, n * sizeof(struct tsf_mod));
	m[f->modNum + n].dest = TSF_MODEND;
	for (i = 0; i != n; i++) // remember which MIDI controllers are of interest
	{
		if (l->mods[i].src    & 128) f->modCCs[(l->mods[i].src    & 127) >> 5] |= (1u << (l->mods[i].src    & 31));
		if (l->mods[i].amtSrc & 128) f->modCCs[(l->mods[i].amtSrc & 127) >> 5] |= (1u << (l->mods[i].amtSrc & 31));
	}
	p = f->modNum;
	f->modNum += n + 1;
	return p;
}

// The index ranges (start, end) in the bag and generator tables are taken from the file and clamped so that a damaged
// SoundFont cannot make the loader read outside of the tables (limit is the number of usable entries)
static int tsf_hydra_clamp_start(int start, int limit) { if (limit < 0) limit = 0; return (start < 0 ? 0 : (start > limit ? limit : start)); }
static int tsf_hydra_clamp_end(int start, int end, int limit)
{
	start = tsf_hydra_clamp_start(start, limit);
	if (limit < 0) limit = 0;
	return (end < start ? start : (end > limit ? limit : end));
}

static unsigned int tsf_addr_clamp(long long pos, unsigned int lo, unsigned int hi) { return (unsigned int)(pos < lo ? lo : (pos > hi ? hi : pos)); }

// Generators that are only allowed at the instrument level (sample generators, substitution generators, 8.5), ignored in presets
static TSF_BOOL tsf_gen_instrument_only(tsf_u16 g) { return (g <= 4 || g == 12 || g == 45 || g == 46 || g == 47 || g == 50 || g == 54 || g == 57 || g == 58); }

static int tsf_load_presets(tsf* res, struct tsf_hydra *hydra, unsigned int fontSampleCount)
{
	enum { GenInstrument = 41, GenKeyRange = 43, GenVelRange = 44, GenSampleID = 53 };
	// Read each preset.
	struct tsf_hydra_phdr *pphdr, *pphdrMax;
	// Every table needs its terminal record; phdr/inst/shdr need at least one real entry too
	if (hydra->phdrNum < 2 || hydra->pbagNum < 1 || hydra->pgenNum < 1 || hydra->pmodNum < 1 || hydra->instNum < 2 ||
		hydra->ibagNum < 1 || hydra->igenNum < 1 || hydra->imodNum < 1 || hydra->shdrNum < 2) return 0;
	res->presetNum = hydra->phdrNum - 1;
	res->presets = (struct tsf_preset*)TSF_MALLOC(res->presetNum * sizeof(struct tsf_preset));
	if (!res->presets) return 0;
	else { int i; for (i = 0; i != res->presetNum; i++) res->presets[i].regions = TSF_NULL; }
	res->mods = TSF_NULL; res->modNum = 0;
	for (pphdr = hydra->phdrs, pphdrMax = pphdr + hydra->phdrNum - 1; pphdr != pphdrMax; pphdr++)
	{
		int sortedIndex = 0, region_index = 0;
		unsigned int regionCount = 0; // a damaged font can reference up to 65535 sample zones 65535 times, more than fits an int
		struct tsf_hydra_phdr *otherphdr;
		struct tsf_preset* preset;
		struct tsf_hydra_pbag *ppbag, *ppbagEnd;
		struct tsf_region globalRegion;
		struct tsf_modlist globalMods;
		for (otherphdr = hydra->phdrs; otherphdr != pphdrMax; otherphdr++)
		{
			if (otherphdr == pphdr || otherphdr->bank > pphdr->bank) continue;
			else if (otherphdr->bank < pphdr->bank) sortedIndex++;
			else if (otherphdr->preset > pphdr->preset) continue;
			else if (otherphdr->preset < pphdr->preset) sortedIndex++;
			else if (otherphdr < pphdr) sortedIndex++;
		}

		preset = &res->presets[sortedIndex];
		TSF_MEMCPY(preset->presetName, pphdr->presetName, sizeof(preset->presetName));
		preset->presetName[sizeof(preset->presetName)-1] = '\0'; //should be zero terminated in source file but make sure
		preset->bank = pphdr->bank;
		preset->preset = pphdr->preset;
		preset->regionNum = 0;

		//count regions covered by this preset
		for (ppbag = hydra->pbags + tsf_hydra_clamp_start(pphdr->presetBagNdx, hydra->pbagNum - 1), ppbagEnd = hydra->pbags + tsf_hydra_clamp_end(pphdr->presetBagNdx, pphdr[1].presetBagNdx, hydra->pbagNum - 1); ppbag != ppbagEnd; ppbag++)
		{
			unsigned char plokey = 0, phikey = 127, plovel = 0, phivel = 127;
			struct tsf_hydra_pgen *ppgen, *ppgenEnd; struct tsf_hydra_inst *pinst; struct tsf_hydra_ibag *pibag, *pibagEnd; struct tsf_hydra_igen *pigen, *pigenEnd;
			for (ppgen = hydra->pgens + tsf_hydra_clamp_start(ppbag->genNdx, hydra->pgenNum), ppgenEnd = hydra->pgens + tsf_hydra_clamp_end(ppbag->genNdx, ppbag[1].genNdx, hydra->pgenNum); ppgen != ppgenEnd; ppgen++)
			{
				if (ppgen->genOper == GenKeyRange) { plokey = ppgen->genAmount.range.lo; phikey = ppgen->genAmount.range.hi; continue; }
				if (ppgen->genOper == GenVelRange) { plovel = ppgen->genAmount.range.lo; phivel = ppgen->genAmount.range.hi; continue; }
				if (ppgen->genOper != GenInstrument) continue;
				if (ppgen->genAmount.wordAmount >= hydra->instNum - 1) continue; // the last INST is the terminal record
				pinst = hydra->insts + ppgen->genAmount.wordAmount;
				for (pibag = hydra->ibags + tsf_hydra_clamp_start(pinst->instBagNdx, hydra->ibagNum - 1), pibagEnd = hydra->ibags + tsf_hydra_clamp_end(pinst->instBagNdx, pinst[1].instBagNdx, hydra->ibagNum - 1); pibag != pibagEnd; pibag++)
				{
					unsigned char ilokey = 0, ihikey = 127, ilovel = 0, ihivel = 127;
					for (pigen = hydra->igens + tsf_hydra_clamp_start(pibag->instGenNdx, hydra->igenNum), pigenEnd = hydra->igens + tsf_hydra_clamp_end(pibag->instGenNdx, pibag[1].instGenNdx, hydra->igenNum); pigen != pigenEnd; pigen++)
					{
						if (pigen->genOper == GenKeyRange) { ilokey = pigen->genAmount.range.lo; ihikey = pigen->genAmount.range.hi; continue; }
						if (pigen->genOper == GenVelRange) { ilovel = pigen->genAmount.range.lo; ihivel = pigen->genAmount.range.hi; continue; }
						if (pigen->genOper == GenSampleID && ihikey >= plokey && ilokey <= phikey && ihivel >= plovel && ilovel <= phivel) regionCount++;
					}
				}
			}
		}

		if (regionCount > 0x7FFFFFFF / sizeof(struct tsf_region)) goto out_of_memory; // over 2 GB (also keeps the allocation size from overflowing on 32-bit)
		preset->regionNum = (int)regionCount;
		preset->regions = (struct tsf_region*)TSF_MALLOC((preset->regionNum ? preset->regionNum : 1) * sizeof(struct tsf_region));
		if (!preset->regions) goto out_of_memory;
		tsf_region_clear(&globalRegion, TSF_TRUE);
		globalMods.num = 0;

		// Zones.
		for (ppbag = hydra->pbags + tsf_hydra_clamp_start(pphdr->presetBagNdx, hydra->pbagNum - 1), ppbagEnd = hydra->pbags + tsf_hydra_clamp_end(pphdr->presetBagNdx, pphdr[1].presetBagNdx, hydra->pbagNum - 1); ppbag != ppbagEnd; ppbag++)
		{
			struct tsf_hydra_pgen *ppgen, *ppgenEnd; struct tsf_hydra_inst *pinst; struct tsf_hydra_ibag *pibag, *pibagEnd; struct tsf_hydra_igen *pigen, *pigenEnd;
			struct tsf_region presetRegion = globalRegion;
			struct tsf_modlist presetMods = globalMods;
			int hadGenInstrument = 0;
			tsf_mods_zone(&presetMods, hydra->pmods, ppbag->modNdx, ppbag[1].modNdx, hydra->pmodNum, TSF_FALSE);

			// Generators.
			for (ppgen = hydra->pgens + tsf_hydra_clamp_start(ppbag->genNdx, hydra->pgenNum), ppgenEnd = hydra->pgens + tsf_hydra_clamp_end(ppbag->genNdx, ppbag[1].genNdx, hydra->pgenNum); ppgen != ppgenEnd; ppgen++)
			{
				// Instrument.
				if (ppgen->genOper == GenInstrument)
				{
					struct tsf_region instRegion;
					struct tsf_modlist instMods;
					tsf_u16 whichInst = ppgen->genAmount.wordAmount;
					hadGenInstrument = 1; // not the global zone, also if the instrument is invalid
					if (whichInst >= (unsigned int)(hydra->instNum - 1)) break; // the last INST is the terminal record

					tsf_region_clear(&instRegion, TSF_FALSE);
					TSF_MEMCPY(instMods.mods, tsf_defmods, sizeof(tsf_defmods));
					instMods.num = (int)(sizeof(tsf_defmods) / sizeof(tsf_defmods[0]));
					pinst = &hydra->insts[whichInst];
					for (pibag = hydra->ibags + tsf_hydra_clamp_start(pinst->instBagNdx, hydra->ibagNum - 1), pibagEnd = hydra->ibags + tsf_hydra_clamp_end(pinst->instBagNdx, pinst[1].instBagNdx, hydra->ibagNum - 1); pibag != pibagEnd; pibag++)
					{
						// Generators.
						struct tsf_region zoneRegion = instRegion;
						struct tsf_modlist zoneMods = instMods;
						int hadSampleID = 0;
						tsf_mods_zone(&zoneMods, (const struct tsf_hydra_pmod*)hydra->imods, pibag->instModNdx, pibag[1].instModNdx, hydra->imodNum, TSF_FALSE);
						for (pigen = hydra->igens + tsf_hydra_clamp_start(pibag->instGenNdx, hydra->igenNum), pigenEnd = hydra->igens + tsf_hydra_clamp_end(pibag->instGenNdx, pibag[1].instGenNdx, hydra->igenNum); pigen != pigenEnd; pigen++)
						{
							if (pigen->genOper == GenSampleID)
							{
								struct tsf_hydra_shdr* pshdr;
								struct tsf_modlist finalMods;
								int modIdx;

								hadSampleID = 1; // not the global zone, also if the sample is invalid or skipped
								if (pigen->genAmount.wordAmount >= (unsigned int)(hydra->shdrNum - 1)) break; // no such sample (the last SHDR is the terminal record)
								pshdr = &hydra->shdrs[pigen->genAmount.wordAmount];
								if (pshdr->start >= pshdr->end || pshdr->end > fontSampleCount || !pshdr->sampleRate) break;
								if (pshdr->sampleType & 0x8000) break; // ROM sample (7.10): its data is in a sound ROM, not in the smpl chunk

								//preset region key and vel ranges are a filter for the zone regions
								if (zoneRegion.hikey < presetRegion.lokey || zoneRegion.lokey > presetRegion.hikey) break;
								if (zoneRegion.hivel < presetRegion.lovel || zoneRegion.lovel > presetRegion.hivel) break;
								if (presetRegion.lokey > zoneRegion.lokey) zoneRegion.lokey = presetRegion.lokey;
								if (presetRegion.hikey < zoneRegion.hikey) zoneRegion.hikey = presetRegion.hikey;
								if (presetRegion.lovel > zoneRegion.lovel) zoneRegion.lovel = presetRegion.lovel;
								if (presetRegion.hivel < zoneRegion.hivel) zoneRegion.hivel = presetRegion.hivel;

								//sum regions
								tsf_region_operator(&zoneRegion, 0, TSF_NULL, &presetRegion, 0);

								// Modulators: instrument level ones replace the defaults, preset level ones add to those.
								// (EG and LFO times are converted from timecents to seconds when a note starts, after modulation.)
								finalMods = zoneMods;
								tsf_mods_merge(&finalMods, presetMods.mods, presetMods.num, TSF_TRUE);
								modIdx = tsf_mods_pool(res, &finalMods);
								if (modIdx < 0) goto out_of_memory;
								zoneRegion.modIdx = (unsigned int)modIdx;

								// Sample positions: the header's points plus the address offset generators (fine + 32768 * coarse), kept inside the
								// sample data. The end is exclusive (the first of the zero points after the sample), the loop end is made inclusive.
								{
									#define TSF_ADDR(base, i) tsf_addr_clamp((long long)(base) + zoneRegion.addrOfs[i] + 32768LL * zoneRegion.addrOfs[(i) + 4], pshdr->start, pshdr->end)
									zoneRegion.offset     = TSF_ADDR(pshdr->start, 0);
									zoneRegion.end        = TSF_ADDR(pshdr->end, 1);
									zoneRegion.loop_start = TSF_ADDR(pshdr->startLoop, 2);
									zoneRegion.loop_end   = TSF_ADDR(pshdr->endLoop, 3);
									#undef TSF_ADDR
								}
								if (zoneRegion.offset >= zoneRegion.end) break;
								zoneRegion.sample_start = pshdr->start; zoneRegion.sample_end = pshdr->end;
								// the loop may begin before the start point (playback runs into the loop and then loops), but must end inside the played part
								if (zoneRegion.loop_end > zoneRegion.end || zoneRegion.loop_end <= zoneRegion.loop_start + 1)
									zoneRegion.loop_mode = TSF_LOOPMODE_NONE, zoneRegion.loop_start = zoneRegion.loop_end = 0;
								else zoneRegion.loop_end--;
								if (zoneRegion.pitch_keycenter < 0 || zoneRegion.pitch_keycenter > 127) zoneRegion.pitch_keycenter = (pshdr->originalPitch <= 127 ? pshdr->originalPitch : 60); // 255 (unpitched) and the illegal 128-254 mean 60 (7.10)
								if (zoneRegion.fixedKey > 127) zoneRegion.fixedKey = -1; // an illegal keynum or velocity generator is ignored (10.4)
								if (zoneRegion.fixedVel < 1 || zoneRegion.fixedVel > 127) zoneRegion.fixedVel = -1;
								zoneRegion.tune += pshdr->pitchCorrection;
								zoneRegion.sample_rate = pshdr->sampleRate;

								preset->regions[region_index] = zoneRegion;
								region_index++;
								break; // the sampleID is the zone's last generator, any after it are ignored (7.9)
							}
							else tsf_region_operator(&zoneRegion, pigen->genOper, &pigen->genAmount, TSF_NULL, 0);
						}

						// Handle instrument's global zone.
						if (pibag == hydra->ibags + tsf_hydra_clamp_start(pinst->instBagNdx, hydra->ibagNum - 1) && !hadSampleID)
							instRegion = zoneRegion, instMods = zoneMods;
					}
					break; // the instrument is the zone's last generator, any after it are ignored (7.5)
				}
				else if (!tsf_gen_instrument_only(ppgen->genOper)) tsf_region_operator(&presetRegion, ppgen->genOper, &ppgen->genAmount, TSF_NULL, 0);
			}

			// Handle preset's global zone.
			if (ppbag == hydra->pbags + tsf_hydra_clamp_start(pphdr->presetBagNdx, hydra->pbagNum - 1) && !hadGenInstrument)
				globalRegion = presetRegion, globalMods = presetMods;
		}
		preset->regionNum = region_index; // zones rejected while materializing must not be exposed (count pass is an upper bound)
	}
	return 1;

out_of_memory:
	{ int i; for (i = 0; i != res->presetNum; i++) TSF_FREE(res->presets[i].regions); }
	TSF_FREE(res->presets);
	TSF_FREE(res->mods);
	return 0;
}

#ifdef STB_VORBIS_INCLUDE_STB_VORBIS_H
#define TSF_MAX_DECODED 0x3FFFFFFF // decoded samples (2 GB)
// The output grows by half, trimmed at the end
#define TSF_DECODE_GROW(res, oldres, resNum, resMax, resInitial, fail) \
	if (resNum > resMax) \
	{ \
		resMax = (resMax ? resMax + resMax / 2 : resInitial); if (resMax < resNum) resMax = resNum; if (resMax > TSF_MAX_DECODED) resMax = TSF_MAX_DECODED; \
		oldres = res; \
		if (!(res = (short*)TSF_REALLOC(res, resMax * sizeof(short)))) { TSF_FREE(oldres); fail } \
	}

// Decode an Ogg Vorbis stream to 16-bit samples appended to *pRes
static int tsf_decode_ogg(const tsf_u8 *pSmpl, const tsf_u8 *pSmplEnd, short** pRes, tsf_u32* pResNum, tsf_u32* pResMax, tsf_u32 resInitial)
{
	short *res = *pRes, *oldres, *out; tsf_u32 resNum = *pResNum; tsf_u32 resMax = *pResMax; stb_vorbis *v;

	// Use whatever stb_vorbis API that is available (either pull or push)
	#if !defined(STB_VORBIS_NO_PULLDATA_API) && !defined(STB_VORBIS_NO_FROMMEMORY)
	v = stb_vorbis_open_memory(pSmpl, (int)(pSmplEnd - pSmpl), TSF_NULL, TSF_NULL);
	#else
	{ int use, err; v = stb_vorbis_open_pushdata(pSmpl, (int)(pSmplEnd - pSmpl), &use, &err, TSF_NULL); pSmpl += use; }
	#endif
	if (v == TSF_NULL) return -1; // not a valid stream (0 is out of memory)

	for (;;)
	{
		float** outputs; const float* in; int n_samples;

		// Decode one frame of vorbis samples with whatever stb_vorbis API that is available
		#if !defined(STB_VORBIS_NO_PULLDATA_API) && !defined(STB_VORBIS_NO_FROMMEMORY)
		n_samples = stb_vorbis_get_frame_float(v, TSF_NULL, &outputs);
		if (!n_samples) break;
		#else
		if (pSmpl >= pSmplEnd) break;
		{ int use = stb_vorbis_decode_frame_pushdata(v, pSmpl, (int)(pSmplEnd - pSmpl), TSF_NULL, &outputs, &n_samples); pSmpl += use; if (!use) break; } // no complete page left
		if (!n_samples) continue;
		#endif

		// Expand the output buffer if necessary, then convert the frame to 16-bit, rounded and clipped (NaN as 0)
		if ((tsf_u32)n_samples > TSF_MAX_DECODED - resNum) { TSF_FREE(res); *pRes = TSF_NULL; stb_vorbis_close(v); return 0; }
		resNum += n_samples;
		TSF_DECODE_GROW(res, oldres, resNum, resMax, resInitial, *pRes = TSF_NULL; stb_vorbis_close(v); return 0;)
		for (in = outputs[0], out = res + resNum - n_samples; out != res + resNum; in++)
			{ float x = *in * 32768.0f; *(out++) = (short)(x > -32767.5f ? (x < 32766.5f ? (int)(x + (x < 0.0f ? -0.5f : 0.5f)) : 32767) : (x <= -32767.5f ? -32768 : 0)); }
	}
	stb_vorbis_close(v);
	*pRes = res; *pResNum = resNum; *pResMax = resMax;
	return 1;
}

static int tsf_decode_sf3_samples(const void* rawBuffer, short** pSampleBuffer, unsigned int* pSmplCount, struct tsf_hydra *hydra)
{
	const tsf_u8* smplBuffer = (const tsf_u8*)rawBuffer;
	tsf_u32 smplLength = *pSmplCount, smplShorts = smplLength / 2, resNum = 0, resMax = 0, resInitial = (smplLength > 0x100000 ? (smplLength & ~0xFFFFF) : 65536);
	short *res = TSF_NULL, *oldres;
	int i, shdrLast = hydra->shdrNum - 1, is_sf3 = 0;
	for (i = 0; i <= shdrLast; i++)
	{
		struct tsf_hydra_shdr *shdr = &hydra->shdrs[i];
		if (shdr->sampleType & 0x10) // compressed sample: start and end are byte offsets, the loop is relative
		{
			if (shdr->end <= smplLength && shdr->start < shdr->end && shdr->end - shdr->start >= 4)
			{
				tsf_u32 start = resNum;
				if (!TSF_FourCCEquals(smplBuffer + shdr->start, "OggS") || tsf_decode_ogg(smplBuffer + shdr->start, smplBuffer + shdr->end, &res, &resNum, &resMax, resInitial) <= 0)
				{
					TSF_FREE(res);
					return 0;
				}
				shdr->start = start; shdr->startLoop += start; shdr->endLoop += start; shdr->end = resNum; is_sf3 = 1;
			}
			else shdr->start = shdr->end = shdr->startLoop = shdr->endLoop = 0; // invalid sample header: the sample is left out
		}
		else // raw PCM sample (16-bit units; positions are kept as they are until a compressed sample moved them)
		{
			tsf_u32 from, to, oldResNum = resNum; short *out; const short *in;
			if (is_sf3 && i == shdrLast) continue; // the terminal record, the rest of the buffer is compressed data
			from = (is_sf3 ? shdr->start : resNum);
			to = (i == shdrLast ? smplShorts : (shdr->end >= shdr->endLoop ? shdr->end : shdr->endLoop));
			if (to > smplShorts) to = smplShorts;
			if (to <= from) { if (is_sf3) shdr->start = shdr->end = shdr->startLoop = shdr->endLoop = 0; continue; }
			if (is_sf3) // move the sample's points to where it lands in the output
			{
				tsf_u32 fix_offset = resNum - shdr->start;
				shdr->start = resNum;
				shdr->end += fix_offset;
				shdr->startLoop += fix_offset;
				shdr->endLoop += fix_offset;
			}

			// expand the output buffer if necessary, then copy the PCM data
			if (to - from > TSF_MAX_DECODED - resNum) { TSF_FREE(res); return 0; }
			resNum += to - from;
			TSF_DECODE_GROW(res, oldres, resNum, resMax, resInitial, return 0;)
			for (in = (const short*)smplBuffer + from, out = res + oldResNum; out != res + resNum;)
				*(out++) = (short)TSF_SWAP16LE(*(in++));
		}
	}

	// Trim the sample buffer down then return success (unless out of memory)
	if (!(*pSampleBuffer = (short*)TSF_REALLOC(res, (resNum + 1) * sizeof(short)))) { TSF_FREE(res); return 0; }
	(*pSampleBuffer)[resNum] = 0; // one extra zero sample for the interpolation at the very end
	*pSmplCount = resNum;
	return 1;
}
#endif

static int tsf_load_samples(void** pRawBuffer, short** pSampleBuffer, unsigned int* pSmplCount, struct tsf_riffchunk *chunkSmpl, struct tsf_stream* stream)
{
	#ifdef STB_VORBIS_INCLUDE_STB_VORBIS_H
	// With OGG Vorbis support we cannot pre-allocate the memory for tsf_decode_sf3_samples
	tsf_u32 resNum, resMax; short* oldres;
	*pSmplCount = chunkSmpl->size;
	if (*pSmplCount > 0x7FFFFFFF) return 0; // no wrap on 32-bit, and the int read result
	*pRawBuffer = (void*)TSF_MALLOC(*pSmplCount + sizeof(short)); // room for the extra zero sample if these are plain 16-bit samples
	if (!*pRawBuffer || !chunkSmpl->size || stream->read(stream->data, *pRawBuffer, chunkSmpl->size) != (int)chunkSmpl->size) return 0; // also a truncated file
	if (chunkSmpl->id[3] != 'o') return 1;

	// Decode custom .sfo 'smpo' format where all samples are in a single ogg stream
	resNum = resMax = 0;
	if (tsf_decode_ogg((tsf_u8*)*pRawBuffer, (tsf_u8*)*pRawBuffer + chunkSmpl->size, pSampleBuffer, &resNum, &resMax, 65536) != 1) return 0;
	oldres = *pSampleBuffer;
	if (!(*pSampleBuffer = (short*)TSF_REALLOC(*pSampleBuffer, (resNum + 1) * sizeof(short)))) { TSF_FREE(oldres); return 0; }
	(*pSampleBuffer)[resNum] = 0; // one extra zero sample for the interpolation at the very end
	*pSmplCount = resNum;
	return 1;
	#else
	// The samples stay 16-bit like in the file (byte-swapped in place on big-endian machines)
	short *out, *end;
	(void)pRawBuffer;
	*pSmplCount = chunkSmpl->size / (unsigned int)sizeof(short);
	if (chunkSmpl->size > 0x7FFFFFFF) return 0; // no wrap on 32-bit, and the int read result
	*pSampleBuffer = (short*)TSF_MALLOC((*pSmplCount + 1) * sizeof(short)); // with one extra zero sample for the interpolation at the very end
	if (!*pSampleBuffer || !chunkSmpl->size || stream->read(stream->data, *pSampleBuffer, chunkSmpl->size) != (int)chunkSmpl->size) return 0; // also a truncated file
	for (out = *pSampleBuffer, end = out + *pSmplCount; out != end; out++) *out = (short)TSF_SWAP16LE(*out);
	*out = 0;
	return 1;
	#endif
}

static int tsf_voice_envelope_release_samples(struct tsf_voice_envelope* e, float outSampleRate)
{
	float r = e->parameters.release;
	return (int)((r < TSF_FASTRELEASETIME && (e->isAmpEnv || r <= 0.0f) ? TSF_FASTRELEASETIME : r) * outSampleRate);
}

#define TSF_ENV_LN_100DB 11.512925f // ln(10^5), the amplitude ratio of 100 dB
#define TSF_ENV_SILENCE 0.00001f // 100 dB below full scale, the volume envelope is considered silent below this

// The modulation envelope's attack is convex (8.1.2 #26), the curve of the convex modulator source (8.2.4)
static float tsf_env_convex(float x) { float y = (x >= 1.0f ? 1.0f : (x <= 0.004f ? 0.0f : 1.0f + 0.18095603f * TSF_LOGF(x))); return (y > 0.0f ? y : 0.0f); }

static void tsf_voice_envelope_nextsegment(struct tsf_voice_envelope* e, short active_segment, float outSampleRate)
{
	switch (active_segment)
	{
		case TSF_SEGMENT_NONE:
			e->samplesUntilNextSegment = (int)(e->parameters.delay * outSampleRate);
			if (e->samplesUntilNextSegment > 0)
			{
				e->segment = TSF_SEGMENT_DELAY;
				e->segmentIsExponential = TSF_FALSE;
				e->level = 0.0;
				e->slope = 0.0;
				return;
			}
			/* fall through */
		case TSF_SEGMENT_DELAY:
			e->samplesUntilNextSegment = (int)(e->parameters.attack * outSampleRate);
			if (e->samplesUntilNextSegment > 0)
			{
				e->segment = TSF_SEGMENT_ATTACK;
				e->segmentIsExponential = TSF_FALSE;
				e->level = 0.0f;
				e->slope = 1.0f / e->samplesUntilNextSegment;
				return;
			}
			/* fall through */
		case TSF_SEGMENT_ATTACK:
			e->samplesUntilNextSegment = (int)(e->parameters.hold * outSampleRate);
			if (e->samplesUntilNextSegment > 0)
			{
				e->segment = TSF_SEGMENT_HOLD;
				e->segmentIsExponential = TSF_FALSE;
				e->level = 1.0f;
				e->slope = 0.0f;
				return;
			}
			/* fall through */
		case TSF_SEGMENT_HOLD:
			e->samplesUntilNextSegment = (int)(e->parameters.decay * outSampleRate);
			if (e->samplesUntilNextSegment > 0)
			{
				e->segment = TSF_SEGMENT_DECAY;
				e->level = 1.0f;
				if (e->isAmpEnv)
				{
					// Linear in dB, the decay time is the time for a 100 dB drop (8.1.2 #36), so it ends early at the sustain level
					float dbSlope = -TSF_ENV_LN_100DB / e->samplesUntilNextSegment;
					e->slope = dbSlope; // exponential segments keep the log of the factor per sample
					e->slopeBlock = TSF_EXPF(dbSlope * TSF_RENDER_EFFECTSAMPLEBLOCK); // the factor of a whole block, computed once
					e->segmentIsExponential = TSF_TRUE;
					if (e->parameters.sustain > 0.0f)
					{
						e->samplesUntilNextSegment = (int)(TSF_LOGF(e->parameters.sustain) / dbSlope);
					}
				}
				else
				{
					e->slope = -1.0f / e->samplesUntilNextSegment;
					e->samplesUntilNextSegment = (int)(e->parameters.decay * (1.0f - e->parameters.sustain) * outSampleRate);
					e->segmentIsExponential = TSF_FALSE;
				}
				return;
			}
			/* fall through */
		case TSF_SEGMENT_DECAY:
			if (e->isAmpEnv && e->parameters.sustain <= TSF_ENV_SILENCE) break; // nothing audible left to sustain (9.1.7): done
			e->segment = TSF_SEGMENT_SUSTAIN;
			e->level = e->parameters.sustain;
			e->slope = 0.0f;
			e->samplesUntilNextSegment = 0x7FFFFFFF;
			e->segmentIsExponential = TSF_FALSE;
			return;
		case TSF_SEGMENT_SUSTAIN:
			// Release starts from the current value (during the mod envelope's attack that is the convex curve's value)
			if (e->segment == TSF_SEGMENT_ATTACK && !e->isAmpEnv) e->level = tsf_env_convex(e->level);
			else if (e->segment == TSF_SEGMENT_DELAY || e->segment == TSF_SEGMENT_DONE) e->level = 0.0f;
			e->segment = TSF_SEGMENT_RELEASE;
			e->samplesUntilNextSegment = tsf_voice_envelope_release_samples(e, outSampleRate);
			if (e->isAmpEnv)
			{
				// Linear in dB, the release time is the time for a 100 dB drop (8.1.2 #38); done once 100 dB below full scale
				float dbSlope = -TSF_ENV_LN_100DB / e->samplesUntilNextSegment;
				e->slope = dbSlope;
				e->slopeBlock = TSF_EXPF(dbSlope * TSF_RENDER_EFFECTSAMPLEBLOCK);
				e->segmentIsExponential = TSF_TRUE;
				e->samplesUntilNextSegment = (e->level > TSF_ENV_SILENCE ? (int)(TSF_LOGF(TSF_ENV_SILENCE / e->level) / dbSlope) + 1 : 0);
			}
			else
			{
				// Linear from the current level at the rate of a full scale change in the release time (8.1.2 #30)
				e->samplesUntilNextSegment = (int)(e->level * e->samplesUntilNextSegment) + 1;
				e->slope = -e->level / e->samplesUntilNextSegment;
				e->segmentIsExponential = TSF_FALSE;
			}
			return;
		case TSF_SEGMENT_RELEASE:
		default:
			break;
	}
	e->segment = TSF_SEGMENT_DONE;
	e->segmentIsExponential = TSF_FALSE;
	e->level = e->slope = 0.0f;
	e->samplesUntilNextSegment = 0x7FFFFFFF;
}

// Take (modulated) envelope parameters in timecents; used at the start of a note and when modulators change them while it plays
static void tsf_voice_envelope_update_parameters(struct tsf_voice_envelope* e, const struct tsf_envelope* new_parameters, int midiNoteNumber)
{
	e->parameters = *new_parameters;
	tsf_envelope_tosecs(&e->parameters, midiNoteNumber, e->isAmpEnv);
}

// Modulated decay time, sustain level and release time (real-time generators) apply to sounding notes: a decay goes on from its
// level with the new slope to the new sustain level (if it is already at or below it, the sustain starts), a sustain moves to the
// new level, a release goes on from its level at the new rate. A quick release (sound off, exclusive class) stays quick.
static void tsf_voice_envelope_modulate(struct tsf_voice_envelope* e, const struct tsf_envelope* new_parameters, int midiNoteNumber, float outSampleRate)
{
	float samples, s, release = e->parameters.release;
	tsf_voice_envelope_update_parameters(e, new_parameters, midiNoteNumber);
	if (release <= 0.0f) e->parameters.release = 0.0f;
	else if (e->segment == TSF_SEGMENT_RELEASE && e->parameters.release != release) tsf_voice_envelope_nextsegment(e, TSF_SEGMENT_SUSTAIN, outSampleRate);
	s = e->parameters.sustain;
	if (e->segment == TSF_SEGMENT_SUSTAIN) e->level = s;
	if (e->segment != TSF_SEGMENT_DECAY) return;
	samples = e->parameters.decay * outSampleRate;
	if (samples < 1.0f || e->level <= s) e->samplesUntilNextSegment = 0;
	else if (e->isAmpEnv)
	{
		e->slope = -TSF_ENV_LN_100DB / samples;
		e->slopeBlock = TSF_EXPF(e->slope * TSF_RENDER_EFFECTSAMPLEBLOCK);
		e->samplesUntilNextSegment = (int)(TSF_LOGF((s > TSF_ENV_SILENCE ? s : TSF_ENV_SILENCE) / e->level) / e->slope);
	}
	else e->slope = -1.0f / samples, e->samplesUntilNextSegment = (int)((s - e->level) / e->slope);
}

static void tsf_voice_envelope_setup(struct tsf_voice_envelope* e, const struct tsf_envelope* new_parameters, int midiNoteNumber, short midiVelocity, TSF_BOOL isAmpEnv, float outSampleRate)
{
	e->isAmpEnv = isAmpEnv;
	tsf_voice_envelope_update_parameters(e, new_parameters, midiNoteNumber);
	e->midiVelocity = midiVelocity;
	tsf_voice_envelope_nextsegment(e, TSF_SEGMENT_NONE, outSampleRate);
}

static void tsf_voice_envelope_process(struct tsf_voice_envelope* e, int numSamples, float outSampleRate)
{
	// Segments that are shorter than the block (very fast envelopes) are all advanced, so none is stretched to a full block
	while (numSamples > 0 && e->segment != TSF_SEGMENT_DONE)
	{
		int step = e->samplesUntilNextSegment;
		if (step <= 0) { tsf_voice_envelope_nextsegment(e, e->segment, outSampleRate); continue; }
		if (step > numSamples) step = numSamples;
		if (e->slope)
		{
			if (e->segmentIsExponential) e->level *= (step == TSF_RENDER_EFFECTSAMPLEBLOCK ? e->slopeBlock : TSF_EXPF(e->slope * (float)step));
			else e->level += e->slope * (float)step;
		}
		e->samplesUntilNextSegment -= step; numSamples -= step;
		if (e->samplesUntilNextSegment <= 0) tsf_voice_envelope_nextsegment(e, e->segment, outSampleRate);
	}
}

// The low-pass is a trapezoidal integrated (TPT) state variable filter (A. Simper, "Linear Trapezoidal Integrated SVF")
static void tsf_voice_lowpass_setup(struct tsf_voice_lowpass* e, float Fc)
{
	double g = TSF_TAN(TSF_PI * Fc), a1 = 1.0 / (1.0 + g * (g + e->QInv)), a2 = g * a1;
	e->k1 = 2.0 * a1 - 1.0; e->p = 2.0 * a2; e->q = 2.0 * g * a2;
}

// Normalized filter frequency for a cutoff in cents (kept within 20 Hz (the generator's minimum) and 45% of the sample rate)
static float tsf_lowpass_fc(float cents, float outSampleRate)
{
	float hz = 8.176f * TSF_EXPF((cents < 1500.0f ? 1500.0f : cents) * (float)TSF_LN2_1200);
	return (hz < 0.45f * outSampleRate ? hz : 0.45f * outSampleRate) / outSampleRate;
}

// A filter turning on mid-note starts in the steady state for the current input x (zero state would step the output: a click)
static void tsf_voice_lowpass_start(struct tsf_voice_lowpass* e, double x)
{
	e->ic1 = 0.0; // the band pass part
	e->ic2 = x; // the low pass part (the DC gain is applied to the output)
}

static float tsf_voice_lowpass_process(struct tsf_voice_lowpass* e, double In)
{
	// v3 = In - ic2, v1 = a1 ic1 + a2 v3, v2 = ic2 + a2 ic1 + a3 v3, ic1 = 2 v1 - ic1, ic2 = 2 v2 - ic2, low pass v2 = (ic2 + new ic2) / 2
	double ic2 = e->ic2, v3 = In - ic2;
	e->ic2 = ic2 + e->p * e->ic1 + e->q * v3;
	e->ic1 = e->k1 * e->ic1 + e->p * v3;
	return (float)((ic2 + e->ic2) * e->gain);
}

static void tsf_voice_lfo_setup(struct tsf_voice_lfo* e, float delay, int freqCents, float outSampleRate)
{
	e->samplesUntil = (int)(delay * outSampleRate);
	e->delta = (4.0f * tsf_cents2Hertz((float)freqCents) / outSampleRate);
	e->level = 0;
}

static void tsf_voice_lfo_process(struct tsf_voice_lfo* e, int blockSamples)
{
	if (e->samplesUntil > 0)
	{
		if (e->samplesUntil >= blockSamples) { e->samplesUntil -= blockSamples; return; }
		blockSamples -= e->samplesUntil; e->samplesUntil = 0; // only the part after the delay advances the LFO
	}
	e->level += e->delta * blockSamples;
	while (e->level > 1.0f || e->level < -1.0f) // a fast LFO can pass both peaks within a large block
	{
		e->delta = -e->delta;
		e->level = (e->level > 1.0f ? 2.0f : -2.0f) - e->level;
	}
}

static void tsf_voice_kill(struct tsf_voice* v)
{
	v->playingPreset = -1;
}

#define TSF_VOICE_KEYDOWN(v) ((v)->ampenv.segment < TSF_SEGMENT_RELEASE && !(v)->releasePending)

static void tsf_voice_release(tsf* f, struct tsf_voice* v)
{
	// if maxVoiceNum is set, assume that voice rendering and note queuing are on separate threads
	// so to minimize the chance that voice rendering would advance the segment at the same time
	// we just do it twice here and hope that it sticks
	int repeats = (f->maxVoiceNum ? 2 : 1);
	while (repeats--)
	{
		tsf_voice_envelope_nextsegment(&v->ampenv, TSF_SEGMENT_SUSTAIN, f->outSampleRate);
		tsf_voice_envelope_nextsegment(&v->modenv, TSF_SEGMENT_SUSTAIN, f->outSampleRate);
		if (v->region->loop_mode == TSF_LOOPMODE_SUSTAIN)
		{
			// Continue playing, but stop looping.
			v->loopEnd = v->loopStart;
		}
	}
	v->releasePending = 0;
}

static void tsf_voice_end(tsf* f, struct tsf_voice* v)
{
	if (v->minLengthLeft > 0) v->releasePending = 1;
	else tsf_voice_release(f, v);
}

static void tsf_voice_endquick(tsf* f, struct tsf_voice* v)
{
	// if maxVoiceNum is set, assume that voice rendering and note queuing are on separate threads
	// so to minimize the chance that voice rendering would advance the segment at the same time
	// we just do it twice here and hope that it sticks
	int repeats = (f->maxVoiceNum ? 2 : 1);
	while (repeats--)
	{
		v->releasePending = 0;
		v->ampenv.parameters.release = 0.0f; tsf_voice_envelope_nextsegment(&v->ampenv, TSF_SEGMENT_SUSTAIN, f->outSampleRate);
		v->modenv.parameters.release = 0.0f; tsf_voice_envelope_nextsegment(&v->modenv, TSF_SEGMENT_SUSTAIN, f->outSampleRate);
	}
}

static void tsf_voice_calcpitchratio(struct tsf_voice* v, const struct tsf_region* r, float pitchShift)
{
	// Scale tuning only scales the distance of the key from the root key; coarse/fine tune (incl. the pitch wheel modulator) and the
	// channel tuning are added on top unscaled (8.1.2 #51, #52, #56)
	v->pitchInputTimecents = r->pitch_keycenter * 100.0 + (v->key - r->pitch_keycenter) * (double)r->pitch_keytrack + r->transpose * 100.0 + r->tune + pitchShift * 100.0;
}

static void tsf_voice_setpan(struct tsf_voice* v, float pan, float panOffset)
{
	// pan in 0.1% (-500 to 500); constant power law (RP-036): left = cos(pi/2 * p), right = sin(pi/2 * p) for p from 0 to 1,
	// evaluated with the half angle tangent (sin = 2t/(1+t^2), cos = (1-t^2)/(1+t^2)) so no extra math function is needed
	float newpan = pan * 0.001f + panOffset;
	if      (newpan <= -0.5f) { v->panFactorLeft = 1.0f; v->panFactorRight = 0.0f; }
	else if (newpan >=  0.5f) { v->panFactorLeft = 0.0f; v->panFactorRight = 1.0f; }
	else
	{
		double t = TSF_TAN((newpan + 0.5f) * (TSF_PI * 0.25)), n = 1.0 / (1.0 + t * t);
		v->panFactorLeft = (float)((1.0 - t * t) * n); v->panFactorRight = (float)(2.0 * t * n);
	}
}

static TSF_BOOL tsf_env_differs(const struct tsf_envelope* a, const struct tsf_envelope* b)
{
	const float *x = (const float*)a, *y = (const float*)b;
	unsigned i;
	for (i = 0; i != sizeof(struct tsf_envelope) / sizeof(float); i++) if (x[i] != y[i]) return TSF_TRUE;
	return TSF_FALSE;
}

static void tsf_voice_lowpass_update(struct tsf_voice_lowpass* l, int fcCents, int qCentibels, float outSampleRate, double x, TSF_BOOL dynamic)
{
	// Resonance = peak height above DC, with the DC gain lowered by half of it (8.1.2 #9). A 2-pole lowpass peaks at
	// Q / sqrt(1 - 1 / (4 Q^2)) above DC, solved for Q here (zero resonance = Butterworth). Bypassed at maximum cutoff without resonance.
	double peak = TSF_EXP(qCentibels * (2.302585092994046 / 200.0));
	TSF_BOOL wasActive = l->active;
	l->resonant = (qCentibels > 0);
	l->QInv = 1.0 / TSF_SQRT(peak * (peak + TSF_SQRT(peak * peak - 1.0)) * 0.5);
	l->gain = 0.5 * TSF_EXP(qCentibels * (2.302585092994046 / -400.0)); // 1 / sqrt(peak), halved for the low-pass output (old + new state)
	l->active = (l->resonant || fcCents < 13500 || (wasActive && dynamic)); // a modulated cutoff: the render loop decides per block
	if (!l->active) return;
	tsf_voice_lowpass_setup(l, tsf_lowpass_fc((float)fcCents, outSampleRate));
	if (!wasActive) tsf_voice_lowpass_start(l, x);
}

// ---------------------------------------------------------------------------------------------------------
// Modulators (SoundFont 2.01 section 8), run time part

// Concave (-40/96 * log10(1 - x)) and convex (1 + 40/96 * log10(x)) curve (8.2.1) for x in 0.0 to 1.0, the output is 0.0 to 1.0
static float tsf_mod_curve(unsigned type, float x)
{
	float y = (type == 1 ? (x >= 1.0f ? 1.0f : -0.18095603f * TSF_LOGF(1.0f - x)) : (x <= 0.0f ? 0.0f : 1.0f + 0.18095603f * TSF_LOGF(x)));
	return (y < 0.0f ? 0.0f : (y > 1.0f ? 1.0f : y));
}

// The value of a source operator s (index, CC flag, direction, polarity and curve type) normalized to 0.0 to 1.0 (bipolar -1.0 to 1.0).
// Without channels (or a channel for the voice) the controllers have their default values.
static float tsf_mod_source(const struct tsf_voice* v, const struct tsf_channel* c, unsigned s)
{
	unsigned idx = s & 127, type = s >> 10;
	float x, range = 128.0f;
	if (s & 128)
	{
		x = (c ? c->cc[idx] : (idx == 7 || idx == 11 ? 127.0f : (idx == 10 ? 64.0f : 0.0f)));
		if (c && idx - 1u < 31u && idx != 6) x += c->cc[idx + 32] * (1.0f / 128.0f); // 14-bit controller: CC 32-63 are the LSBs of CC 0-31 (not bank select/data entry)
	}
	else switch (idx)
	{
		case  2: x = v->velocity; break;
		case  3: x = (float)v->key; break;
		case 10: x = v->keyPressure; break;
		case 13: x = (c ? c->pressure : 0.0f); break;
		case 14: x = (c ? c->pitchWheel : 8192.0f); range = 16384.0f; break;
		case 16: x = (c ? c->pitchRange : 2.0f); range = 127.0f; break; // semitones / 127: the default modulator's 12700 cents bend by exactly the RPN 0 range
		default: return 1.0f; // no controller
	}
	x /= range;
	if (type == 3) // switch
	{
		float on = (float)((x >= 0.5f) != ((s & 256) != 0));
		return ((s & 512) ? 2.0f * on - 1.0f : on);
	}
	if (s & 256) x = 1.0f - x;
	if (s & 512) x = 2.0f * x - 1.0f;
	if (!type) return x;
	return (x < 0.0f ? -tsf_mod_curve(type, -x) : tsf_mod_curve(type, x));

}

// SF2.01 NRPN (9.6.3) generator units per data entry step: the useful range (8.1.3) is halved until it is 8192 units or less,
// so the +-8192 steps reach every value of the range, 0 for generators that are not real-time parameters
static const unsigned char tsf_nrpn_scale[TSF_NUMGENS] =
{
	0, 0, 0, 0, 0, 4, 4, 4, 2, 1, 4, 4, 0, 1, 0, 1, 1, 1, 0, 0, // 0-19
	0, 4, 4, 4, 4, 4, 4, 4, 4, 1, 4, 1, 1, 4, 4, 4, 4, 1, 4, 1, // 20-39
	1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0     // 40-58
};

// AWE32 NRPN (MSB 127, LSB 0-26): the generator each sets (initial pitch adds to fine tune) and the value conversion:
// times and pitch use the 14-bit value - 8192, the others its low 8 or 7 bits; filter cutoff and Q follow the Audigy

static const unsigned char tsf_awe_gen[27] =
{
	21, 22, 23, 24,          // 0-3:   mod LFO delay and frequency, vibrato LFO delay and frequency
	25, 26, 27, 28, 29, 30,  // 4-9:   mod envelope delay, attack, hold, decay, sustain, release
	33, 34, 35, 36, 37, 38,  // 10-15: volume envelope delay, attack, hold, decay, sustain, release
	52, 5, 6, 7, 13,         // 16-20: initial pitch (added to the fine tune), mod LFO, vibrato LFO and mod envelope to pitch, mod LFO to volume
	8, 9, 10, 11, 15, 16     // 21-26: filter cutoff and resonance, mod LFO and mod envelope to cutoff, chorus, reverb
};
static short tsf_awe_round(float x) { return (short)(x < 0.0f ? x - 0.5f : x + 0.5f); }
static short tsf_awe_nrpn_value(int param, int data)
{
	int x = data - 8192, b = data & 0xFF, l = data & 0x7F, sb = (b ^ 0x80) - 0x80, max = 5940;
	switch (param)
	{
		case 0: case 2: case 4: case 10: max = 5900; /* fall through */ // delays: 0-5900 ms
		case 5: case 7: case 9: case 11: case 13: case 15:              // attacks, decays and releases: 0-5940 ms
		case 6: case 12:                                                // holds: 0-8191 ms
			if (param == 6 || param == 12) max = 8191;
			if (x > max) x = max;
			return (x <= 0 ? -12000 : tsf_awe_round(1731.234f * TSF_LOGF(x * 0.001f))); // timecents
		case 1: case 3: return (l ? tsf_awe_round(1731.234f * TSF_LOGF(l * (0.084f / 8.176f))) : -16000); // 0-127: 0.084 Hz steps
		case 8: return (short)((127 - (b > 127 ? 127 : b)) * 1000 / 127); // 0-127: the level, 127 the peak (sustain is a 0.1% decrease)
		case 14: return tsf_awe_round((127 - (b > 127 ? 127 : b)) * 7.5f); // 0-127: 0.75 dB steps below full level (sustain is in cB)
		case 16: return (short)x; // -8192-8191: cents
		case 17: case 18: case 19: return tsf_awe_round(sb * 9.375f); // -127-127: 9.375 cent steps
		case 20: return tsf_awe_round(l * 1.875f); // 0-127: 0.1875 dB steps
		case 21: return tsf_awe_round(4335.7f + l * (7646.0f / 128.0f)); // 0-127: 100 Hz up to 8000 Hz, logarithmic (absolute cents)
		case 22: return tsf_awe_round(l * (200.0f / 128.0f)); // 0-127: up to 19.84 dB (cB)
		case 23: return tsf_awe_round(((l ^ 0x40) - 0x40) * 56.25f); // -64-63: 56.25 cent steps
		case 24: return tsf_awe_round(sb * 56.25f); // -127-127: 56.25 cent steps
		default: return (short)(b * 1000 / 255); // 25, 26: 0-255 chorus and reverb send (0.1%)
	}
}

// The output of modulator p in list m (a linked source is the sum of the outputs of the modulators linked to p)
static float tsf_mod_out(const struct tsf_mod* m, const struct tsf_mod* p, const struct tsf_voice* v, const struct tsf_channel* c)
{
	float s, o;
	if ((p->src & 255) == 127)
	{
		const struct tsf_mod* q;
		for (s = 0.0f, q = m; q->dest != TSF_MODEND; q++)
			if (q->dest == (0x8000 | (p - m))) s += tsf_mod_out(m, q, v, c);
	}
	else s = tsf_mod_source(v, c, p->src);
	o = p->amount * s;
	if (c && p->src == 0x0081 && p->dest - 5u < 2u) o *= c->modDepth; // CC1 to modulation/vibrato LFO pitch depth
	if (p->amtSrc) o *= tsf_mod_source(v, c, p->amtSrc);
	return (p->trans == 2 && o < 0.0f ? -o : o);
}

// Re-evaluate the modulators of a voice and update everything they influenced. On init (note start) the voice's noteGainDB must
// hold the gain without the region's attenuation and the lowpass must be reset, otherwise only what changed gets updated.
static void tsf_voice_modulate(tsf* f, struct tsf_voice* v, TSF_BOOL init)
{
	const struct tsf_channel* c = (f->channels && v->playingChannel >= 0 && v->playingChannel < f->channels->channelNum ? &f->channels->channels[v->playingChannel] : TSF_NULL);
	const struct tsf_mod *m = f->mods + v->region->modIdx, *p;
	struct tsf_region n = *v->region, *o = &v->mr;
	float g[TSF_NUMGENS], attMod;
	int i;

	// Sum the outputs per destination generator (links are accounted for at their target) and apply to the region
	TSF_MEMSET(g, 0, sizeof(g));
	for (p = m; p->dest != TSF_MODEND; p++)
		if (!(p->dest & 0x8000)) g[p->dest] += tsf_mod_out(m, p, v, c);
	if (c) for (i = 0; i != TSF_NUMGENS; i++) g[i] += c->nrpn[i] * tsf_nrpn_scale[i]; // SF2.01 NRPN offsets act like modulators
	if (c && c->aweSet) // AWE32 NRPN: replace the generator value (the modulators still add to it), the initial pitch adds cents
		for (i = 0; i != 27; i++)
		{
			union tsf_hydra_genamount a;
			if (!(c->aweSet & (1u << i))) continue;
			if (i == 16) { g[52] += c->awe[16]; continue; }
			a.shortAmount = c->awe[i];
			tsf_region_operator(&n, tsf_awe_gen[i], &a, TSF_NULL, 0.0f);
		}
	// Every generator value (instrument + preset sum) gets the modulator outputs added and is then clamped to its range
	attMod = g[48]; // the attenuation generator is scaled, the modulators' share is not
	for (i = 0; i != TSF_NUMGENS; i++)
		if (i != 48) tsf_region_operator(&n, (tsf_u16)i, TSF_NULL, TSF_NULL, g[i]);
	n.attenuation = n.attenuation * TSF_ATTENUATION_GEN_SCALE + attMod;
	if (n.attenuation < 0.0f) n.attenuation = 0.0f; else if (n.attenuation > 1440.0f) n.attenuation = 1440.0f;
	n.initialFilterFc -= v->softFc;
	if (!init) n.end = o->end; // keep the end moved by the address offset modulators at note on

	if (init || n.attenuation != o->attenuation) v->noteGainDB += ((init ? 0.0f : o->attenuation) - n.attenuation) * 0.1f;
	if (init || n.pan != o->pan) tsf_voice_setpan(v, n.pan, (c ? c->panOffset : 0.0f));
	if (init || n.transpose != o->transpose || n.tune != o->tune || n.pitch_keytrack != o->pitch_keytrack) tsf_voice_calcpitchratio(v, &n, (c ? c->tuning : 0.0f));
	// the filter runs on the samples scaled by 2^15; a changed LFO or envelope depth too, as a depth of 0 hands the cutoff back from the render loop
	if (init || n.initialFilterFc != o->initialFilterFc || n.initialFilterQ != o->initialFilterQ || n.modLfoToFilterFc != o->modLfoToFilterFc || n.modEnvToFilterFc != o->modEnvToFilterFc)
		tsf_voice_lowpass_update(&v->lowpass, n.initialFilterFc, n.initialFilterQ, f->outSampleRate, (init ? 0.0 : f->fontSamples[(unsigned int)(v->sourceSamplePosition >> 32)] * 32768.0), (TSF_BOOL)(n.modLfoToFilterFc || n.modEnvToFilterFc));
	if (!init && n.freqModLFO != o->freqModLFO) v->modlfo.delta = (v->modlfo.delta < 0.0f ? -4.0f : 4.0f) * tsf_cents2Hertz((float)n.freqModLFO) / f->outSampleRate;
	if (!init && n.freqVibLFO != o->freqVibLFO) v->viblfo.delta = (v->viblfo.delta < 0.0f ? -4.0f : 4.0f) * tsf_cents2Hertz((float)n.freqVibLFO) / f->outSampleRate;
	if (!init && tsf_env_differs(&n.ampenv, &o->ampenv)) tsf_voice_envelope_modulate(&v->ampenv, &n.ampenv, v->key, f->outSampleRate);
	if (!init && tsf_env_differs(&n.modenv, &o->modenv)) tsf_voice_envelope_modulate(&v->modenv, &n.modenv, v->key, f->outSampleRate);
	v->chorusSend = n.chorusSend * 0.001f;
	v->reverbSend = n.reverbSend * 0.001f;
	*o = n;
}

// One block of samples of a voice: interpolation, low-pass filter, output (OUTPUT is the statement(s) writing val to the buffers)
// The gains ramp linearly over the block from the envelope/LFO state at its start to the state at its end (no zipper noise)
// Voices that send to the effects use a second copy of the loops so the others pay nothing for it
#define TSF_VOICE_SAMPLE(FILTER, OUTPUT, NEXTPOS) \
	{ \
		unsigned int pos = (unsigned int)(phase >> 32), nextPos = (NEXTPOS); \
		int x0 = input[pos], ival = x0 * 32768 + (input[nextPos] - x0) * (int)((unsigned int)phase >> 17); float val = (float)ival; /* linear interpolation in integers with a 15-bit fraction (the gains include 1 / 2^30) */ \
		FILTER \
		OUTPUT \
		gainLeft += stepLeft; gainRight += stepRight; gainMono += stepMono; \
		phase += phaseStep; \
	}
/* The samples that can't reach the loop end or the sample end are rendered as one run, without the end test and the loop wrap */
#define TSF_VOICE_LOOP_BODY(FILTER, OUTPUT) \
	while (blockSamples > 0 && phase < phaseEnd) \
	{ \
		int run = blockSamples; \
		if (phase + phaseStep * (tsf_u64)blockSamples > phaseSafe) run = (phase < phaseSafe && phaseStep ? (int)((phaseSafe - phase - 1) / phaseStep) + 1 : 0); \
		if (run) { blockSamples -= run; while (run--) TSF_VOICE_SAMPLE(FILTER, OUTPUT, pos + 1) } \
		else { blockSamples--; TSF_VOICE_SAMPLE(FILTER, OUTPUT, (pos >= tmpLoopEnd && isLooping ? tmpLoopStart : pos + 1)) } \
		if (phase >= phaseLoopEnd && isLooping) /* wrap into the loop, also when a step is longer than the loop */ \
			phase = (phase - phaseLoopEnd < phaseLoopLength ? phase - phaseLoopLength : phaseLoopEnd - phaseLoopLength + (phase - phaseLoopEnd) % phaseLoopLength); \
	}
#define TSF_VOICE_LOOP(OUTPUT) /* one loop with the filter, one without, so the test isn't made per sample */ \
	if (tmpLowpass.active) { TSF_VOICE_LOOP_BODY(val = tsf_voice_lowpass_process(&tmpLowpass, (double)ival);, OUTPUT) } \
	else { TSF_VOICE_LOOP_BODY(, OUTPUT) }
#define TSF_SEND(P, level, gl, gr) P[0] += val * (gl) * (level); P[1] += val * (gr) * (level); P += 2;

// Playback speed as a 32.32 fixed point step per output sample, at most 2^23 times faster (NaN plays at that rate too). With the
// position below 2^31 samples (< 2^63) the 256 steps of a block (< 2^63) can then be added to the position without overflowing.
static tsf_u64 tsf_phasestep(double pitchRatio) { return (pitchRatio < 8388608.0 ? (tsf_u64)(pitchRatio * 4294967296.0) : (tsf_u64)8388608 << 32); }

// The current output gain of a voice (volume envelope, attenuation and the modulation LFO's volume modulation)
static float tsf_voice_gain(const struct tsf_voice* v, float noteGain, float modLfoToVolumeDB)
{
	if (v->ampenv.segment == TSF_SEGMENT_DONE) return 0.0f;
	return v->ampenv.level * (modLfoToVolumeDB != 0.0f ? tsf_decibelsToGain(v->noteGainDB + v->modlfo.level * modLfoToVolumeDB) : noteGain);
}

// sendC and sendV: interleaved stereo pairs, NULL for an effect that doesn't run; the send a voice doesn't use is not touched
static void tsf_voice_render(tsf* f, struct tsf_voice* v, float* outputBuffer, int numSamples, int stride, float* sendC, float* sendV)
{
	struct tsf_region* region = &v->mr;
	const short* input = f->fontSamples;
	float* outL = outputBuffer;
	float* outR = (f->outputmode == TSF_STEREO_UNWEAVED ? outL + stride : TSF_NULL);
	float chorusSend = v->chorusSend, reverbSend = v->reverbSend;
	int sendKind = (sendV && reverbSend > 0.0f ? 1 : 0) | (sendC && chorusSend > 0.0f ? 2 : 0); // 1 reverb, 2 chorus, 3 both

	// Cache some values, to give them at least some chance of ending up in registers.
	// The mod envelope and LFOs run also at zero depth: modulators and NRPNs can set a depth mid-note
	TSF_BOOL isLooping    = (v->loopStart < v->loopEnd);
	unsigned int tmpLoopStart = v->loopStart, tmpLoopEnd = v->loopEnd;
	// The sample position is a 32.32 fixed point number while rendering
	tsf_u64 phase = v->sourceSamplePosition, phaseStep, phaseEnd = (tsf_u64)region->end << 32;
	tsf_u64 phaseLoopEnd = (tsf_u64)(tmpLoopEnd + 1) << 32, phaseLoopLength = (tsf_u64)(tmpLoopEnd + 1 - tmpLoopStart) << 32;
	tsf_u64 phaseSafe = (isLooping && ((tsf_u64)tmpLoopEnd << 32) < phaseEnd ? (tsf_u64)tmpLoopEnd << 32 : phaseEnd); // positions before this need no checks
	struct tsf_voice_lowpass tmpLowpass = v->lowpass;
	float tmpSampleRate = f->outSampleRate;

	TSF_BOOL dynamicLowpass = (region->modLfoToFilterFc || region->modEnvToFilterFc);
	float lastFres = -1.0f;
	float tmpInitialFilterFc = (float)region->initialFilterFc, tmpModLfoToFilterFc = (float)region->modLfoToFilterFc, tmpModEnvToFilterFc = (float)region->modEnvToFilterFc;

	TSF_BOOL dynamicPitchRatio = (region->modLfoToPitch || region->modEnvToPitch || region->vibLfoToPitch || v->portaCents != 0.0f);
	float tmpModLfoToPitch = (float)region->modLfoToPitch, tmpVibLfoToPitch = (float)region->vibLfoToPitch, tmpModEnvToPitch = (float)region->modEnvToPitch;
	double pitchRatio = tsf_timecents2Secsd(v->pitchInputTimecents) * v->pitchOutputFactor;

	float tmpModLfoToVolume = (float)region->modLfoToVolume * 0.1f;
	float noteGain = tsf_decibelsToGain(v->noteGainDB), gainNext = tsf_voice_gain(v, noteGain, tmpModLfoToVolume);
	float tmpLfoBoost = (tmpModLfoToVolume ? tsf_decibelsToGain(tmpModLfoToVolume < 0.0f ? -tmpModLfoToVolume : tmpModLfoToVolume) : 1.0f);
	// the gains at the start of the first block: where the last block ended, so a changed volume or pan ramps instead of jumping
	float startMono = (v->rampValid ? v->rampMono : gainNext);
	float startLeft = (v->rampValid ? v->rampLeft : gainNext * v->panFactorLeft), startRight = (v->rampValid ? v->rampRight : gainNext * v->panFactorRight);
	phaseStep = tsf_phasestep(pitchRatio);

	while (numSamples)
	{
		float gainMono, gainLeft, gainRight, stepMono, stepLeft, stepRight, modEnv;
		int blockSamples = (numSamples > TSF_RENDER_EFFECTSAMPLEBLOCK ? TSF_RENDER_EFFECTSAMPLEBLOCK : numSamples);
		numSamples -= blockSamples;

		// the mod envelope's attack is convex
		modEnv = (v->modenv.segment == TSF_SEGMENT_ATTACK ? tsf_env_convex(v->modenv.level) : v->modenv.level);

		if (dynamicLowpass)
		{
			float fres = tmpInitialFilterFc + v->modlfo.level * tmpModLfoToFilterFc + modEnv * tmpModEnvToFilterFc;
			TSF_BOOL wasActive = tmpLowpass.active;
			tmpLowpass.active = (tmpLowpass.resonant || fres < 13500.0f);
			if (tmpLowpass.active && (fres != lastFres || !wasActive)) tsf_voice_lowpass_setup(&tmpLowpass, tsf_lowpass_fc(fres, tmpSampleRate)), lastFres = fres; // e.g. not while the envelope sustains
			if (tmpLowpass.active && !wasActive) tsf_voice_lowpass_start(&tmpLowpass, input[(unsigned int)(phase >> 32)] * 32768.0); // no stale state from when it was bypassed
		}

		if (dynamicPitchRatio)
		{
			if (v->portaCents != 0.0f) // portamento: linear glide in cents, the last block lands exactly on the target pitch
			{
				v->portaCents += v->portaDelta * blockSamples;
				if ((v->portaDelta < 0.0f) == (v->portaCents <= 0.0f) || v->portaDelta == 0.0f) v->portaCents = v->portaDelta = 0.0f;
			}
			pitchRatio = tsf_timecents2Secsd(v->pitchInputTimecents + v->portaCents + (v->modlfo.level * tmpModLfoToPitch + v->viblfo.level * tmpVibLfoToPitch + modEnv * tmpModEnvToPitch)) * v->pitchOutputFactor;
			phaseStep = tsf_phasestep(pitchRatio);
		}

		if (v->minLengthLeft > 0) v->minLengthLeft -= blockSamples;
		if (v->releasePending && v->minLengthLeft <= 0) // every block: a note off from another thread can come late
		{
			tsf_voice_release(f, v);
			if (isLooping && v->loopStart >= v->loopEnd) isLooping = TSF_FALSE, phaseSafe = phaseEnd; // a sustain loop ends with the release
		}

		// Update EG and LFOs, the gain ramps from the value at the start of the block to the value at its end
		tsf_voice_envelope_process(&v->ampenv, blockSamples, tmpSampleRate);
		tsf_voice_envelope_process(&v->modenv, blockSamples, tmpSampleRate);
		tsf_voice_lfo_process(&v->modlfo, blockSamples);
		tsf_voice_lfo_process(&v->viblfo, blockSamples);
		gainNext = tsf_voice_gain(v, noteGain, tmpModLfoToVolume);
		// A released voice ends once its total gain (envelope and attenuation, at the LFO's loudest) is 100 dB down (9.1.7)
		if (v->ampenv.segment == TSF_SEGMENT_RELEASE && v->ampenv.level * noteGain * tmpLfoBoost < TSF_ENV_SILENCE) v->ampenv.segment = TSF_SEGMENT_DONE, gainNext = 0.0f;
		{
			float endLeft = gainNext * v->panFactorLeft, endRight = gainNext * v->panFactorRight, scale = (1.0f / 1073741824.0f) / blockSamples; // the interpolated samples are scaled by 2^30
			gainMono = startMono * (1.0f / 1073741824.0f), gainLeft = startLeft * (1.0f / 1073741824.0f), gainRight = startRight * (1.0f / 1073741824.0f);
			stepMono = (gainNext - startMono) * scale, stepLeft = (endLeft - startLeft) * scale, stepRight = (endRight - startRight) * scale;
			startMono = gainNext, startLeft = endLeft, startRight = endRight;
		}

		if (gainMono == 0.0f && stepMono == 0.0f && !tmpLowpass.active) // silent block (envelope delay, zero volume): only advance the position
		{
			phase += phaseStep * (tsf_u64)blockSamples;
			if (isLooping && phase >= phaseLoopEnd) phase = phaseLoopEnd - phaseLoopLength + (phase - phaseLoopEnd) % phaseLoopLength;
			if (outL) outL += blockSamples * (f->outputmode == TSF_STEREO_INTERLEAVED ? 2 : 1);
			if (outR) outR += blockSamples;
			if (sendKind & 1) sendV += blockSamples * 2;
			if (sendKind & 2) sendC += blockSamples * 2;
		}
		else
		{
		if (!sendKind)
		{
			switch (f->outputmode)
			{
				case TSF_STEREO_INTERLEAVED: TSF_VOICE_LOOP(*outL++ += val * gainLeft; *outL++ += val * gainRight;) break;
				case TSF_STEREO_UNWEAVED:    TSF_VOICE_LOOP(*outL++ += val * gainLeft; *outR++ += val * gainRight;) break;
				case TSF_MONO:               TSF_VOICE_LOOP(*outL++ += val * gainMono;) break;
			}
		}
		else if (sendKind == 1) // reverb only
		{
			switch (f->outputmode)
			{
				case TSF_STEREO_INTERLEAVED: TSF_VOICE_LOOP(*outL++ += val * gainLeft; *outL++ += val * gainRight; TSF_SEND(sendV, reverbSend, gainLeft, gainRight)) break;
				case TSF_STEREO_UNWEAVED:    TSF_VOICE_LOOP(*outL++ += val * gainLeft; *outR++ += val * gainRight; TSF_SEND(sendV, reverbSend, gainLeft, gainRight)) break;
				case TSF_MONO:               TSF_VOICE_LOOP(*outL++ += val * gainMono; TSF_SEND(sendV, reverbSend, gainMono, gainMono)) break;
			}
		}
		else if (sendKind == 2) // chorus only
		{
			switch (f->outputmode)
			{
				case TSF_STEREO_INTERLEAVED: TSF_VOICE_LOOP(*outL++ += val * gainLeft; *outL++ += val * gainRight; TSF_SEND(sendC, chorusSend, gainLeft, gainRight)) break;
				case TSF_STEREO_UNWEAVED:    TSF_VOICE_LOOP(*outL++ += val * gainLeft; *outR++ += val * gainRight; TSF_SEND(sendC, chorusSend, gainLeft, gainRight)) break;
				case TSF_MONO:               TSF_VOICE_LOOP(*outL++ += val * gainMono; TSF_SEND(sendC, chorusSend, gainMono, gainMono)) break;
			}
		}
		else // both
		{
			switch (f->outputmode)
			{
				case TSF_STEREO_INTERLEAVED: TSF_VOICE_LOOP(*outL++ += val * gainLeft; *outL++ += val * gainRight; TSF_SEND(sendV, reverbSend, gainLeft, gainRight) TSF_SEND(sendC, chorusSend, gainLeft, gainRight)) break;
				case TSF_STEREO_UNWEAVED:    TSF_VOICE_LOOP(*outL++ += val * gainLeft; *outR++ += val * gainRight; TSF_SEND(sendV, reverbSend, gainLeft, gainRight) TSF_SEND(sendC, chorusSend, gainLeft, gainRight)) break;
				case TSF_MONO:               TSF_VOICE_LOOP(*outL++ += val * gainMono; TSF_SEND(sendV, reverbSend, gainMono, gainMono) TSF_SEND(sendC, chorusSend, gainMono, gainMono)) break;
			}
		}
		}

		if (phase >= phaseEnd || v->ampenv.segment == TSF_SEGMENT_DONE)
		{
			tsf_voice_kill(v);
			return;
		}
	}

	v->sourceSamplePosition = phase;
	v->rampMono = startMono, v->rampLeft = startLeft, v->rampRight = startRight, v->rampValid = 1;
	if (tmpLowpass.active || dynamicLowpass) v->lowpass = tmpLowpass;
}
#undef TSF_VOICE_LOOP
#undef TSF_SEND

// ---------------------------------------------------------------------------------------------------------
// Optional built-in effects: a Freeverb style reverb and a stereo chorus, fed by the voices' send levels

#define TSF_FX_BLOCK 256 // samples per run of the effects
#define TSF_FX_QUIET_SECONDS 0.1f // the effects stop once nothing sends and their output stayed below TSF_FX_SILENCE this long (longer than any delay line)
#define TSF_FX_SILENCE 0.00001f // -100 dB
struct tsf_fx_line { float* data; unsigned int length, pos; };
struct tsf_fx
{
	float* mem;
	struct tsf_fx_line comb[8], allpass[4], chorus[2];
	float combStore[8], roomSize, damping, width, reverbWet, chorusDepth, chorusRate, chorusWet, chorusPhase;
	int reverbEnabled, chorusEnabled, idle;
};

static TSF_BOOL tsf_fx_alloc(struct tsf_fx* e, float rate)
{
	static const unsigned short lengths[12] = { 1116, 1188, 1277, 1356, 1139, 1211, 1300, 1379, 556, 441, 579, 464 }; // at 44100 Hz
	unsigned int i, len[14], total = 0;
	float *p, scale = rate / 44100.0f;
	for (i = 0; i != 14; i++)
	{
		len[i] = (i < 12 ? (unsigned int)(lengths[i] * scale + 0.5f) : (unsigned int)(rate * 0.05f) + 4); // the chorus delay lines hold 50 ms
		if (len[i] < 4) len[i] = 4;
		total += len[i];
	}
	e->mem = (float*)TSF_MALLOC(total * sizeof(float));
	if (!e->mem) return TSF_FALSE;
	TSF_MEMSET(e->mem, 0, total * sizeof(float));
	for (p = e->mem, i = 0; i != 14; p += len[i++])
	{
		struct tsf_fx_line* l = (i < 8 ? &e->comb[i] : (i < 12 ? &e->allpass[i - 8] : &e->chorus[i - 12]));
		l->data = p; l->length = len[i]; l->pos = 0;
	}
	return TSF_TRUE;
}

// Clear delay lines from..to-1 (0-7 combs, 8-11 allpasses: the reverb; 12-13: the chorus)
static void tsf_fx_clear_lines(struct tsf_fx* e, unsigned int from, unsigned int to)
{
	unsigned int i;
	for (i = from; i != to; i++)
	{
		struct tsf_fx_line* l = (i < 8 ? &e->comb[i] : (i < 12 ? &e->allpass[i - 8] : &e->chorus[i - 12]));
		if (l->data) TSF_MEMSET(l->data, 0, l->length * sizeof(float));
		l->pos = 0;
	}
	if (from == 0) TSF_MEMSET(e->combStore, 0, sizeof(e->combStore));
	if (to > 12) e->chorusPhase = 0.0f; // the chorus LFO starts over too, as on a fresh instance
}

// Clear both effects: nothing left to play (they start again once a voice sends)
static void tsf_fx_clear(struct tsf_fx* e)
{
	tsf_fx_clear_lines(e, 0, 14);
	e->idle = 0x40000000;
}

// Get the effects of f, allocating them if they are about to be enabled
static struct tsf_fx* tsf_fx_get(tsf* f, int enabling)
{
	if (!f->fx && enabling)
	{
		f->fx = (struct tsf_fx*)TSF_MALLOC(sizeof(struct tsf_fx));
		if (!f->fx) return TSF_NULL;
		TSF_MEMSET(f->fx, 0, sizeof(struct tsf_fx));
		f->fx->roomSize = 0.78f; f->fx->damping = 0.25f; f->fx->width = 1.0f; f->fx->reverbWet = 0.5f;
		f->fx->chorusDepth = 0.5f; f->fx->chorusRate = 0.8f; f->fx->chorusWet = 0.4f;
		f->fx->idle = 0x40000000; // nothing has sent anything yet
		if (!tsf_fx_alloc(f->fx, f->outSampleRate)) { TSF_FREE(f->fx); f->fx = TSF_NULL; }
	}
	return f->fx;
}

// Free the effects' memory once both of them are disabled
static void tsf_fx_release_if_unused(tsf* f)
{
	if (f->fx && !f->fx->reverbEnabled && !f->fx->chorusEnabled)
	{
		TSF_FREE(f->fx->mem);
		TSF_FREE(f->fx);
		f->fx = TSF_NULL;
	}
}

// Process n samples of the chorus (ci) and reverb (ri) sends (interleaved stereo), adds the effect returns to the planar ol/or
static void tsf_fx_process(struct tsf_fx* e, const float* ci, const float* ri, float* ol, float* or_, int n, float rate)
{
	// Each filter and delay line runs over the whole block with its state in local variables (n <= TSF_FX_BLOCK)
	float feedback = 0.70f + 0.12f * e->roomSize, damp = e->damping, undamp = 1.0f - e->damping, chorusInc = e->chorusRate / rate;
	float rev[2][TSF_FX_BLOCK], cho[2][TSF_FX_BLOCK];
	TSF_BOOL doReverb = (e->reverbEnabled && e->reverbWet > 0.0f), doChorus = (e->chorusEnabled && e->chorusWet > 0.0f);
	int i, j;
	for (j = 0; j != 2; j++) for (i = 0; i != n; i++) rev[j][i] = cho[j][i] = 0.0f;
	if (doReverb)
	{
		for (j = 0; j != 2; j++) // parallel damped comb filters, 4 per side, run together so their feedback paths overlap
		{
			struct tsf_fx_line* b = &e->comb[j * 4];
			float *d0 = b[0].data, *d1 = b[1].data, *d2 = b[2].data, *d3 = b[3].data, *sum = rev[j];
			float s0 = e->combStore[j * 4], s1 = e->combStore[j * 4 + 1], s2 = e->combStore[j * 4 + 2], s3 = e->combStore[j * 4 + 3];
			unsigned int p0 = b[0].pos, p1 = b[1].pos, p2 = b[2].pos, p3 = b[3].pos;
			const float* in = ri + j;
			for (i = 0; i != n;)
			{
				unsigned int run = (unsigned int)(n - i), k;
				if (b[0].length - p0 < run) run = b[0].length - p0;
				if (b[1].length - p1 < run) run = b[1].length - p1;
				if (b[2].length - p2 < run) run = b[2].length - p2;
				if (b[3].length - p3 < run) run = b[3].length - p3;
				for (k = 0; k != run; k++, i++, p0++, p1++, p2++, p3++)
				{
					float x = in[i * 2], y0 = d0[p0], y1 = d1[p1], y2 = d2[p2], y3 = d3[p3];
					s0 = y0 * undamp + s0 * damp; d0[p0] = x + s0 * feedback;
					s1 = y1 * undamp + s1 * damp; d1[p1] = x + s1 * feedback;
					s2 = y2 * undamp + s2 * damp; d2[p2] = x + s2 * feedback;
					s3 = y3 * undamp + s3 * damp; d3[p3] = x + s3 * feedback;
					sum[i] = (((sum[i] + y0) + y1) + y2) + y3;
				}
				if (p0 == b[0].length) p0 = 0;
				if (p1 == b[1].length) p1 = 0;
				if (p2 == b[2].length) p2 = 0;
				if (p3 == b[3].length) p3 = 0;
			}
			e->combStore[j * 4] = s0; e->combStore[j * 4 + 1] = s1; e->combStore[j * 4 + 2] = s2; e->combStore[j * 4 + 3] = s3;
			b[0].pos = p0; b[1].pos = p1; b[2].pos = p2; b[3].pos = p3;
		}
		for (j = 0; j != 2; j++) for (i = 0; i != n; i++) rev[j][i] *= 0.25f;
		for (j = 0; j != 4; j++) // series allpass filters, 2 per side
		{
			struct tsf_fx_line* b = &e->allpass[j];
			float *data = b->data, *w = rev[j >> 1];
			unsigned int pos = b->pos, len = b->length;
			for (i = 0; i != n;)
			{
				unsigned int run = (len - pos < (unsigned int)(n - i) ? len - pos : (unsigned int)(n - i)), k;
				for (k = 0; k != run; k++, i++, pos++)
				{
					float x = w[i], y = data[pos];
					data[pos] = x + y * 0.5f;
					w[i] = y - x;
				}
				if (pos == len) pos = 0;
			}
			b->pos = pos;
		}
	}
	if (doChorus)
	{
		float startPhase = e->chorusPhase, p = startPhase, depth = e->chorusDepth * 0.008f;
		for (j = 0; j != 2; j++) // modulated delay line per side, the right side's LFO is a quarter cycle apart
		{
			struct tsf_fx_line* b = &e->chorus[j];
			float* data = b->data; unsigned int pos = b->pos, len = b->length;
			for (p = startPhase, i = 0; i != n; i++)
			{
				float phase = p + (j ? 0.25f : 0.0f), tri, rp, frac, a, d;
				unsigned int ix;
				if (phase >= 1.0f) phase -= 1.0f;
				tri = (phase < 0.5f ? phase * 4.0f - 1.0f : 3.0f - phase * 4.0f);
				rp = (float)pos - rate * (0.020f + depth * tri);
				while (rp < 0.0f) rp += (float)len;
				ix = (unsigned int)rp; frac = rp - (float)ix;
				if (ix >= len) ix -= len;
				a = data[ix]; d = data[ix + 1 == len ? 0 : ix + 1];
				cho[j][i] = a + (d - a) * frac;
				data[pos] = ci[i * 2 + j];
				if (++pos == len) pos = 0;
				p += chorusInc;
				if (p >= 1.0f) p -= 1.0f;
			}
			b->pos = pos;
		}
		e->chorusPhase = p;
	}
	{
		float width = e->width, reverbWet = e->reverbWet, chorusWet = e->chorusWet;
		for (i = 0; i != n; i++)
		{
			float mid = (rev[0][i] + rev[1][i]) * 0.5f, side = (rev[0][i] - rev[1][i]) * 0.5f * width;
			ol[i] += (mid + side) * reverbWet + cho[0][i] * chorusWet;
			or_[i] += (mid - side) * reverbWet + cho[1][i] * chorusWet;
		}
	}
}


TSFDEF tsf* tsf_load(struct tsf_stream* stream)
{
	tsf* res = TSF_NULL;
	struct tsf_riffchunk chunkHead;
	struct tsf_riffchunk chunkList;
	struct tsf_hydra hydra;
	void* rawBuffer = TSF_NULL;
	short* sampleBuffer = TSF_NULL; // the 16-bit samples (decoded if compressed)
	tsf_u32 smplCount = 0;

	if (!tsf_riffchunk_read(TSF_NULL, &chunkHead, stream) || !TSF_FourCCEquals(chunkHead.id, "sfbk"))
	{
		//if (e) *e = TSF_INVALID_NOSF2HEADER;
		return res;
	}

	// Read hydra and locate sample data.
	TSF_MEMSET(&hydra, 0, sizeof(hydra));
	while (tsf_riffchunk_read(&chunkHead, &chunkList, stream))
	{
		struct tsf_riffchunk chunk;
		if (TSF_FourCCEquals(chunkList.id, "pdta"))
		{
			while (tsf_riffchunk_read(&chunkList, &chunk, stream))
			{
				// one stream read per chunk (a truncated file is rejected), parsed from memory; a chunk over 2 GB would wrap num * sizeof on 32-bit
				#define HandleChunk(chunkName) (TSF_FourCCEquals(chunk.id, #chunkName) && !(chunk.size % chunkName##SizeInFile)) \
					{ \
						int num = (int)(chunk.size / chunkName##SizeInFile), i; \
						struct tsf_stream_memory mem = { TSF_NULL, 0, 0 }; \
						struct tsf_stream ms = { TSF_NULL, (int(*)(void*,void*,unsigned int))&tsf_stream_memory_read, (int(*)(void*,unsigned int))&tsf_stream_memory_skip }; \
						TSF_FREE(hydra.chunkName##s); /* a damaged font can repeat a chunk */ \
						hydra.chunkName##s = TSF_NULL; hydra.chunkName##Num = 0; \
						if (chunk.size > 0x7FFFFFFF) goto out_of_memory; \
						if (!(mem.buffer = (const char*)TSF_MALLOC(chunk.size ? chunk.size : 1))) goto out_of_memory; \
						if (stream->read(stream->data, (void*)mem.buffer, chunk.size) != (int)chunk.size || \
							!(hydra.chunkName##s = (struct tsf_hydra_##chunkName*)TSF_MALLOC((num ? num : 1) * sizeof(struct tsf_hydra_##chunkName)))) { TSF_FREE((void*)mem.buffer); goto out_of_memory; } \
						hydra.chunkName##Num = num; mem.total = chunk.size; ms.data = &mem; \
						for (i = 0; i < num; ++i) tsf_hydra_read_##chunkName(&hydra.chunkName##s[i], &ms); \
						TSF_FREE((void*)mem.buffer); \
					}
				enum
				{
					phdrSizeInFile = 38, pbagSizeInFile =  4, pmodSizeInFile = 10,
					pgenSizeInFile =  4, instSizeInFile = 22, ibagSizeInFile =  4,
					imodSizeInFile = 10, igenSizeInFile =  4, shdrSizeInFile = 46
				};
				if      HandleChunk(phdr) else if HandleChunk(pbag) else if HandleChunk(pmod)
				else if HandleChunk(pgen) else if HandleChunk(inst) else if HandleChunk(ibag)
				else if HandleChunk(imod) else if HandleChunk(igen) else if HandleChunk(shdr)
				else if (!stream->skip(stream->data, chunk.size)) goto out_of_memory;
				#undef HandleChunk
			}
		}
		else if (TSF_FourCCEquals(chunkList.id, "sdta"))
		{
			while (tsf_riffchunk_read(&chunkList, &chunk, stream))
			{
				if ((TSF_FourCCEquals(chunk.id, "smpl")
						#ifdef STB_VORBIS_INCLUDE_STB_VORBIS_H
						|| TSF_FourCCEquals(chunk.id, "smpo")
						#endif
					) && !rawBuffer && !sampleBuffer && chunk.size >= sizeof(short))
				{
					if (!tsf_load_samples(&rawBuffer, &sampleBuffer, &smplCount, &chunk, stream)) goto out_of_memory;
				}
				else if (!stream->skip(stream->data, chunk.size)) goto out_of_memory;
			}
		}
		else if (!stream->skip(stream->data, chunkList.size)) goto out_of_memory;
	}
	if (!hydra.phdrs || !hydra.pbags || !hydra.pmods || !hydra.pgens || !hydra.insts || !hydra.ibags || !hydra.imods || !hydra.igens || !hydra.shdrs)
	{
		//if (e) *e = TSF_INVALID_INCOMPLETE;
	}
	else if (!rawBuffer && !sampleBuffer)
	{
		//if (e) *e = TSF_INVALID_NOSAMPLEDATA;
	}
	else
	{
		#ifdef STB_VORBIS_INCLUDE_STB_VORBIS_H
		int j = 0; // with no compressed sample (a plain .sf2) the samples are used as they are, like without Vorbis support
		if (!sampleBuffer) while (j != hydra.shdrNum && !(hydra.shdrs[j].sampleType & 0x10)) j++;
		if (!sampleBuffer && j == hydra.shdrNum)
		{
			short *s = (short*)rawBuffer, *sEnd = s + (smplCount /= sizeof(short));
			for (; s != sEnd; s++) *s = (short)TSF_SWAP16LE(*s);
			*s = 0; // one extra zero sample for the interpolation at the very end
			sampleBuffer = (short*)rawBuffer, rawBuffer = TSF_NULL;
		}
		else if (!sampleBuffer && !tsf_decode_sf3_samples(rawBuffer, &sampleBuffer, &smplCount, &hydra)) goto out_of_memory;
		#else
		int j = 0; // a compressed sample (SF3) can't play without Vorbis support: reject the font
		while (j != hydra.shdrNum && !(hydra.shdrs[j].sampleType & 0x10)) j++;
		if (j != hydra.shdrNum) goto done;
		#endif
		res = (tsf*)TSF_MALLOC(sizeof(tsf));
		if (res) TSF_MEMSET(res, 0, sizeof(tsf));
		if (!res || !tsf_load_presets(res, &hydra, smplCount)) goto out_of_memory;
		res->outSampleRate = 44100.0f;
		res->fontSamples = sampleBuffer;
		{ struct tsf_fx* fx = tsf_fx_get(res, 1); if (fx) fx->reverbEnabled = fx->chorusEnabled = 1; } // built-in effects on by default (none if out of memory)
		sampleBuffer = TSF_NULL; // don't free below
	}
	goto done;
    out_of_memory:
	TSF_FREE(res);
	res = TSF_NULL;
	//if (e) *e = TSF_OUT_OF_MEMORY;
    done:
	TSF_FREE(hydra.phdrs); TSF_FREE(hydra.pbags); TSF_FREE(hydra.pmods);
	TSF_FREE(hydra.pgens); TSF_FREE(hydra.insts); TSF_FREE(hydra.ibags);
	TSF_FREE(hydra.imods); TSF_FREE(hydra.igens); TSF_FREE(hydra.shdrs);
	TSF_FREE(rawBuffer);   TSF_FREE(sampleBuffer);
	return res;
}

TSFDEF tsf* tsf_copy(tsf* f)
{
	tsf* res;
	if (!f) return TSF_NULL;
	if (!f->refCount)
	{
		f->refCount = (int*)TSF_MALLOC(sizeof(int));
		if (!f->refCount) return TSF_NULL;
		*f->refCount = 1;
	}
	res = (tsf*)TSF_MALLOC(sizeof(tsf));
	if (!res) return TSF_NULL;
	TSF_MEMCPY(res, f, sizeof(tsf));
	res->voices = TSF_NULL;
	res->voiceNum = res->maxVoiceNum = 0;
	res->channels = TSF_NULL;
	res->fx = TSF_NULL; // effects are not shared, the copy gets its own with the same settings
	if (f->fx)
	{
		struct tsf_fx* fx = tsf_fx_get(res, 1);
		if (fx)
		{
			fx->reverbEnabled = f->fx->reverbEnabled; fx->chorusEnabled = f->fx->chorusEnabled;
			fx->roomSize = f->fx->roomSize; fx->damping = f->fx->damping; fx->width = f->fx->width; fx->reverbWet = f->fx->reverbWet;
			fx->chorusDepth = f->fx->chorusDepth; fx->chorusRate = f->fx->chorusRate; fx->chorusWet = f->fx->chorusWet;
		}
	}
	if (f->maxVoiceNum && !tsf_set_max_voices(res, f->maxVoiceNum))
	{
		if (res->fx) TSF_FREE(res->fx->mem);
		TSF_FREE(res->fx);
		TSF_FREE(res);
		return TSF_NULL;
	}
	(*res->refCount)++;
	return res;
}

TSFDEF void tsf_close(tsf* f)
{
	if (!f) return;
	if (!f->refCount || !--(*f->refCount))
	{
		struct tsf_preset *preset = f->presets, *presetEnd = preset + f->presetNum;
		for (; preset != presetEnd; preset++) TSF_FREE(preset->regions);
		TSF_FREE(f->presets);
		TSF_FREE(f->mods);
		TSF_FREE(f->fontSamples);
		TSF_FREE(f->refCount);
	}
	if (f->fx) TSF_FREE(f->fx->mem);
	TSF_FREE(f->fx);
	TSF_FREE(f->channels);
	TSF_FREE(f->voices);
	TSF_FREE(f);
}

TSFDEF void tsf_reset(tsf* f)
{
	struct tsf_voice *v = f->voices, *vEnd = v ? v + f->voiceNum : TSF_NULL;
	for (; v != vEnd; v++)
		if (v->playingPreset != -1 && (v->ampenv.segment < TSF_SEGMENT_RELEASE || v->ampenv.parameters.release))
			tsf_voice_endquick(f, v);
	if (f->channels) { TSF_FREE(f->channels); f->channels = TSF_NULL; }
	if (f->fx) tsf_fx_clear(f->fx);
}

TSFDEF int tsf_get_presetindex(const tsf* f, int bank, int preset_number)
{
	const struct tsf_preset *presets;
	int i, iMax;
	for (presets = f->presets, i = 0, iMax = f->presetNum; i < iMax; i++)
		if (presets[i].preset == preset_number && presets[i].bank == bank)
			return i;
	return -1;
}

TSFDEF int tsf_get_presetcount(const tsf* f)
{
	return f->presetNum;
}

TSFDEF const char* tsf_get_presetname(const tsf* f, int preset)
{
	return (preset < 0 || preset >= f->presetNum ? TSF_NULL : f->presets[preset].presetName);
}

TSFDEF int tsf_get_presetbank(const tsf* f, int preset)
{
	return (preset < 0 || preset >= f->presetNum ? -1 : f->presets[preset].bank);
}

TSFDEF int tsf_get_presetnumber(const tsf* f, int preset)
{
	return (preset < 0 || preset >= f->presetNum ? -1 : f->presets[preset].preset);
}

TSFDEF const char* tsf_bank_get_presetname(const tsf* f, int bank, int preset_number)
{
	return tsf_get_presetname(f, tsf_get_presetindex(f, bank, preset_number));
}

// The global gain applies to the sounding notes as well as to new ones (e.g. a Master Volume SysEx)
static void tsf_set_global_gain(tsf* f, float gainDB)
{
	struct tsf_voice *v = f->voices, *vEnd = v ? v + f->voiceNum : TSF_NULL;
	float change;
	if (!TSF_FINITE(gainDB)) return;
	gainDB = (gainDB < -200.0f ? -200.0f : (gainDB > 100.0f ? 100.0f : gainDB)); // keeps the notes' gains exact when it is changed back
	change = gainDB - f->globalGainDB;
	if (change == 0.0f) return;
	for (; v != vEnd; v++) if (v->playingPreset != -1) v->noteGainDB += change;
	f->globalGainDB = gainDB;
}

TSFDEF void tsf_set_output(tsf* f, enum TSFOutputMode outputmode, int samplerate, float global_gain_db)
{
	float newRate = (float)(samplerate < 1 ? 44100 : (samplerate > 10000000 ? 10000000 : samplerate)); // up to 10 MHz, so the longest times (101 s) fit the int sample counts
	TSF_BOOL rateChange = (newRate != f->outSampleRate);
	if (outputmode < 0 || outputmode > TSF_MONO) return;
	if (f->fx && rateChange) // the effects' delay lines depend on the sample rate
	{
		TSF_FREE(f->fx->mem);
		f->fx->mem = TSF_NULL;
		if (!tsf_fx_alloc(f->fx, newRate)) { TSF_FREE(f->fx); f->fx = TSF_NULL; } // out of memory: no effects
		else tsf_fx_clear(f->fx); // also no filter state from the old rate
	}
	f->outputmode = outputmode;
	f->outSampleRate = newRate;
	if (rateChange) // sounding voices have their pitch, envelopes, LFOs and filter in samples at the old rate: fade them out
	{
		struct tsf_voice *v = f->voices, *vEnd = v ? v + f->voiceNum : TSF_NULL;
		for (; v != vEnd; v++) if (v->playingPreset != -1) tsf_voice_endquick(f, v);
	}
	tsf_set_global_gain(f, global_gain_db);
}

TSFDEF void tsf_set_volume(tsf* f, float global_volume)
{
	tsf_set_global_gain(f, tsf_gainToDecibels(global_volume)); // -200 dB (silent) at or below 1e-10
}

TSFDEF int tsf_set_max_voices(tsf* f, int max_voices)
{
	int i = f->voiceNum;
	int newVoiceNum = (f->voiceNum > max_voices ? f->voiceNum : max_voices);
	struct tsf_voice *newVoices;
	if (max_voices == 0) { f->maxVoiceNum = 0; return 1; } // dynamic polyphony (the voices already allocated stay)
	if (max_voices < 0 || newVoiceNum > 0x7FFFFFFF / (int)sizeof(struct tsf_voice)) return 0; // over 2 GB (the allocation size would wrap on 32-bit)
	newVoices = (struct tsf_voice*)TSF_REALLOC(f->voices, newVoiceNum * sizeof(struct tsf_voice));
	if (!newVoices) return 0;
	f->voices = newVoices;
	f->voiceNum = f->maxVoiceNum = newVoiceNum;
	for (; i < max_voices; i++)
		f->voices[i].playingPreset = -1;
	return 1;
}

// Start a note, for a channel (its controllers are the modulator sources) or for channel -1 (the preset API, default controllers).
// legatoIndex: NULL for a new note, else the play index of a legato note whose voices moved from fromKey to this key: only the zones
// that don't cover fromKey start (a layer), as part of that note
static int tsf_note_on_channel(tsf* f, int preset_index, int key, float vel, int channel, const unsigned int* legatoIndex, int fromKey)
{
	struct tsf_channel* c = (channel >= 0 ? &f->channels->channels[channel] : TSF_NULL);
	short midiVelocity;
	unsigned int voicePlayIndex;
	struct tsf_region *region, *regionEnd;

	if (key < 0 || key > 127 || preset_index < 0 || preset_index >= f->presetNum) return 0;
	if (!(vel > 0.0f)) { tsf_note_off(f, preset_index, key); return 1; } // also NaN
	midiVelocity = (short)(vel >= 1.0f ? 127 : (vel < 1.5f / 127 ? 1 : vel * 127 + 0.5f)); // a note on has a velocity of at least 1

	// Play all matching regions.
	voicePlayIndex = (legatoIndex ? *legatoIndex : f->voicePlayIndex++);
	for (region = f->presets[preset_index].regions, regionEnd = region + f->presets[preset_index].regionNum; region != regionEnd; region++)
	{
		struct tsf_voice *voice, *v, *vEnd; TSF_BOOL doLoop;
		if (key < region->lokey || key > region->hikey || midiVelocity < region->lovel || midiVelocity > region->hivel) continue;

		voice = TSF_NULL, v = f->voices, vEnd = v ? v + f->voiceNum : TSF_NULL;
		if (legatoIndex)
		{
			if (fromKey >= region->lokey && fromKey <= region->hikey) continue; // the zone played the previous key (or has ended): no new attack
			for (; v != vEnd; v++)
				if (v->playIndex == voicePlayIndex && v->region == region && v->playingPreset == preset_index && v->playingChannel == channel && v->playingKey == key && TSF_VOICE_KEYDOWN(v)) break;
			if (v != vEnd) continue; // the legato note plays this zone already
			v = f->voices;
		}
		if (region->group)
		{
			for (; v != vEnd; v++)
				if (v->playingPreset == preset_index && v->playingChannel == channel && v->region->group == region->group && v->playIndex != voicePlayIndex) tsf_voice_endquick(f, v); // not the voices of this very note (e.g. the other side of a stereo sample)
				else if (v->playingPreset == -1 && !voice) voice = v;
		}
		else for (; v != vEnd; v++) if (v->playingPreset == -1) { voice = v; break; }

		if (!voice)
		{
			if (f->maxVoiceNum)
			{
				// Voices have been pre-allocated and limited to a maximum: end the released voice furthest into its release (its
				// envelope level falls in dB, the volume counts as a released note can't come back), otherwise the oldest note kept
				// by a pedal after its key was let go, otherwise the oldest note.
				float bestLevel = 3.402823466e+38F;
				struct tsf_voice *vOldestPedal = TSF_NULL, *vOldest = TSF_NULL;
				for (v = f->voices; v != vEnd; v++)
				{
					if (v->ampenv.segment >= TSF_SEGMENT_RELEASE)
					{
						float level = v->ampenv.level * tsf_decibelsToGain(v->noteGainDB);
						if (level < bestLevel) { bestLevel = level; voice = v; }
					}
					else if (v->heldSustain && (!vOldestPedal || v->playIndex < vOldestPedal->playIndex)) vOldestPedal = v;
					if (!vOldest || v->playIndex < vOldest->playIndex) vOldest = v;
				}
				if (!voice) voice = (vOldestPedal ? vOldestPedal : vOldest);
				if (!voice) continue;
				tsf_voice_kill(voice);
			}
			else
			{
				// Allocate more voices so we don't need to kill one off.
				struct tsf_voice* newVoices;
				f->voiceNum += 4;
				newVoices = (struct tsf_voice*)TSF_REALLOC(f->voices, f->voiceNum * sizeof(struct tsf_voice));
				if (!newVoices) { f->voiceNum -= 4; return 0; }
				f->voices = newVoices;
				voice = &f->voices[f->voiceNum - 4];
				voice[1].playingPreset = voice[2].playingPreset = voice[3].playingPreset = -1;
			}
		}

		voice->region = region;
		voice->playingPreset = preset_index;
		voice->playingKey = key;
		voice->playingChannel = channel;
		voice->playIndex = voicePlayIndex;
		voice->heldSustain = voice->sostenuto = voice->rampValid = voice->releasePending = 0;
		voice->minLengthLeft = (int)(TSF_MIN_NOTE_LENGTH * f->outSampleRate);
		voice->portaCents = voice->portaDelta = 0.0f;
		if (c) voice->portaCents = f->channels->glideCents, voice->portaDelta = f->channels->glideDelta;
		voice->key = (unsigned char)(region->fixedKey >= 0 ? region->fixedKey : key);
		voice->velocity = (unsigned char)(region->fixedVel >= 0 ? region->fixedVel : midiVelocity);
		voice->keyPressure = 0;
		voice->noteGainDB = f->globalGainDB + (c ? c->gainDB : 0.0f);
		voice->lowpass.ic1 = voice->lowpass.ic2 = 0;
		voice->lowpass.active = TSF_FALSE;
		voice->softFc = 0;
		if (c && c->cc[67] >= 64) // soft pedal: quieter and darker, for notes started while pressed
		{
			float soft = (c->cc[67] - 63) * (1.0f / 64.0f);
			voice->noteGainDB -= 6.0f * soft, voice->softFc = (short)(360.0f * soft);
		}

		// Evaluate the modulators (sets gain, pan, pitch and lowpass filter in voice and the modulated region)
		tsf_voice_modulate(f, voice, TSF_TRUE);
		voice->pitchOutputFactor = region->sample_rate / (tsf_timecents2Secsd(region->pitch_keycenter * 100.0) * f->outSampleRate);

		// Offset/end/loop, moved by the address offset modulators (evaluated once at note on)
		{
			const int *a = voice->mr.addrOfs, *b = region->addrOfs;
			long long st = region->offset     + (a[0] - b[0]) + 32768LL * (a[4] - b[4]);
			long long en = region->end        + (a[1] - b[1]) + 32768LL * (a[5] - b[5]);
			long long ls = region->loop_start + (a[2] - b[2]) + 32768LL * (a[6] - b[6]);
			long long le = region->loop_end   + (a[3] - b[3]) + 32768LL * (a[7] - b[7]);
			if (st < region->sample_start) st = region->sample_start; else if (st >= region->sample_end) st = region->sample_end - 1;
			if (en > region->sample_end) en = region->sample_end; else if (en <= st) en = st + 1;
			if (ls < region->sample_start) ls = region->sample_start; // loop points are kept inside the sample like at load time
			if (le >= region->sample_end) le = region->sample_end - 1;
			voice->sourceSamplePosition = (tsf_u64)st << 32;
			voice->mr.end = (unsigned int)en;
			doLoop = (region->loop_mode != TSF_LOOPMODE_NONE && region->loop_start < region->loop_end && ls < le && le < en);
			voice->loopStart = (doLoop ? (unsigned int)ls : 0);
			voice->loopEnd = (doLoop ? (unsigned int)le : 0);
			if (doLoop && st > le) voice->sourceSamplePosition = (tsf_u64)(ls + (st - ls) % (le + 1 - ls)) << 32; // a start after the loop begins inside it
		}

		// Setup envelopes (from the modulated timecents).
		tsf_voice_envelope_setup(&voice->ampenv, &voice->mr.ampenv, voice->key, midiVelocity, TSF_TRUE, f->outSampleRate);
		tsf_voice_envelope_setup(&voice->modenv, &voice->mr.modenv, voice->key, midiVelocity, TSF_FALSE, f->outSampleRate);

		// Setup LFO filters.
		tsf_voice_lfo_setup(&voice->modlfo, tsf_timecents2Secs(voice->mr.delayModLFO), voice->mr.freqModLFO, f->outSampleRate);
		tsf_voice_lfo_setup(&voice->viblfo, tsf_timecents2Secs(voice->mr.delayVibLFO), voice->mr.freqVibLFO, f->outSampleRate);
	}
	return 1;
}

TSFDEF int tsf_note_on(tsf* f, int preset_index, int key, float vel)
{
	return tsf_note_on_channel(f, preset_index, key, vel, -1, TSF_NULL, 0); // not on any channel
}

TSFDEF int tsf_bank_note_on(tsf* f, int bank, int preset_number, int key, float vel)
{
	int preset_index = tsf_get_presetindex(f, bank, preset_number);
	if (preset_index == -1) return 0;
	return tsf_note_on(f, preset_index, key, vel);
}

TSFDEF void tsf_note_off(tsf* f, int preset_index, int key)
{
	struct tsf_voice *v = f->voices, *vEnd = v ? v + f->voiceNum : TSF_NULL, *vMatchFirst = TSF_NULL, *vMatchLast = TSF_NULL;
	for (; v != vEnd; v++)
	{
		//Find the first and last entry in the voices list with matching preset, key and look up the smallest play index
		if (v->playingPreset != preset_index || v->playingKey != key || v->playingChannel != -1 || !TSF_VOICE_KEYDOWN(v)) continue;
		else if (!vMatchFirst || v->playIndex < vMatchFirst->playIndex) vMatchFirst = vMatchLast = v;
		else if (v->playIndex == vMatchFirst->playIndex) vMatchLast = v;
	}
	if (!vMatchFirst) return;
	for (v = vMatchFirst; v <= vMatchLast; v++)
	{
		//Stop all voices with matching preset, key and the smallest play index which was enumerated above
		if (v != vMatchFirst && v != vMatchLast &&
			(v->playIndex != vMatchFirst->playIndex || v->playingPreset != preset_index || v->playingKey != key || v->playingChannel != -1 || !TSF_VOICE_KEYDOWN(v))) continue;
		tsf_voice_end(f, v);
	}
}

TSFDEF int tsf_bank_note_off(tsf* f, int bank, int preset_number, int key)
{
	int preset_index = tsf_get_presetindex(f, bank, preset_number);
	if (preset_index == -1 || key < 0 || key > 127) return 0;
	tsf_note_off(f, preset_index, key);
	return 1;
}

TSFDEF void tsf_note_off_all(tsf* f)
{
	struct tsf_voice *v = f->voices, *vEnd = v ? v + f->voiceNum : TSF_NULL;
	for (; v != vEnd; v++) if (v->playingPreset != -1 && TSF_VOICE_KEYDOWN(v))
		tsf_voice_end(f, v);
}

TSFDEF int tsf_active_voice_count(tsf* f)
{
	int count = 0;
	struct tsf_voice *v = f->voices, *vEnd = v ? v + f->voiceNum : TSF_NULL;
	for (; v != vEnd; v++) if (v->playingPreset != -1) count++;
	return count;
}

TSFDEF void tsf_render_short(tsf* f, short* buffer, int samples, int flag_mixing)
{
	float outputSamples[TSF_RENDER_SHORTBUFFERBLOCK];
	int channels = (f->outputmode == TSF_MONO ? 1 : 2), maxChannelSamples = TSF_RENDER_SHORTBUFFERBLOCK / channels;
	while (samples > 0)
	{
		int channelSamples = (samples > maxChannelSamples ? maxChannelSamples : samples);
		short* bufferEnd = buffer + channelSamples * channels;
		float *floatSamples = outputSamples;
		tsf_render_float(f, floatSamples, channelSamples, TSF_FALSE);
		samples -= channelSamples;

		// Rounded to the nearest step (truncating would drop everything below one step, a dead zone around zero) and saturated
		if (flag_mixing)
			while (buffer != bufferEnd)
			{
				float v = *buffer + *floatSamples++ * 32768.0f;
				*buffer++ = (short)(v > -32767.5f ? (v < 32766.5f ? (int)(v + (v < 0.0f ? -0.5f : 0.5f)) : 32767) : (v <= -32767.5f ? -32768 : 0)); // NaN as 0
			}
		else
			while (buffer != bufferEnd)
			{
				float v = *floatSamples++ * 32768.0f;
				*buffer++ = (short)(v > -32767.5f ? (v < 32766.5f ? (int)(v + (v < 0.0f ? -0.5f : 0.5f)) : 32767) : (v <= -32767.5f ? -32768 : 0));
			}
	}
}

TSFDEF void tsf_render_float(tsf* f, float* buffer, int samples, int flag_mixing)
{
	struct tsf_voice *v, *vEnd = (f->voices ? f->voices + f->voiceNum : TSF_NULL);
	struct tsf_fx* fx = f->fx;
	int channels = (f->outputmode == TSF_MONO ? 1 : 2), offset = 0, total = samples;
	int idleLimit = (fx ? (int)(f->outSampleRate * TSF_FX_QUIET_SECONDS) : 0);
	TSF_BOOL doReverb = (fx && fx->reverbEnabled && fx->reverbWet > 0.0f), doChorus = (fx && fx->chorusEnabled && fx->chorusWet > 0.0f); // as tsf_fx_process
	if (samples <= 0) return;
	if (!flag_mixing) TSF_MEMSET(buffer, 0, channels * sizeof(float) * samples);
	#define TSF_VOICE_SENDS(v) (v->playingPreset != -1 && ((doChorus && v->chorusSend > 0.0f) || (doReverb && v->reverbSend > 0.0f))) // to an effect that runs
	if ((doReverb || doChorus) && fx->idle >= idleLimit) // without a tail, check if anything sends at all
		for (v = f->voices; v != vEnd; v++)
			if (TSF_VOICE_SENDS(v)) { fx->idle = 0; break; }
	if (!(doReverb || doChorus) || fx->idle >= idleLimit)
	{
		for (v = f->voices; v != vEnd; v++)
			if (v->playingPreset != -1)
				tsf_voice_render(f, v, buffer, samples, samples, TSF_NULL, TSF_NULL);
		return;
	}
	while (samples > 0) // render in blocks and run the effects on the sends of each block
	{
		float chorusSend[TSF_FX_BLOCK * 2], reverbSend[TSF_FX_BLOCK * 2], wetL[TSF_FX_BLOCK], wetR[TSF_FX_BLOCK];
		int block = (samples > TSF_FX_BLOCK ? TSF_FX_BLOCK : samples), i;
		float* out = (f->outputmode == TSF_STEREO_UNWEAVED ? buffer + offset : buffer);
		TSF_BOOL sending = TSF_FALSE;
		TSF_MEMSET(chorusSend, 0, block * 2 * sizeof(float));
		TSF_MEMSET(reverbSend, 0, block * 2 * sizeof(float));
		for (v = f->voices; v != vEnd; v++)
			if (v->playingPreset != -1)
			{
				tsf_voice_render(f, v, out, block, total, (doChorus ? chorusSend : TSF_NULL), (doReverb ? reverbSend : TSF_NULL));
				if (TSF_VOICE_SENDS(v)) sending = TSF_TRUE;
			}
		if (fx->idle < idleLimit || sending)
		{
			float peak = 0.0f;
			TSF_MEMSET(wetL, 0, block * sizeof(float));
			TSF_MEMSET(wetR, 0, block * sizeof(float));
			tsf_fx_process(fx, chorusSend, reverbSend, wetL, wetR, block, f->outSampleRate);
			for (i = 0; i != block; i++)
			{
				float l = (wetL[i] < 0.0f ? -wetL[i] : wetL[i]), r = (wetR[i] < 0.0f ? -wetR[i] : wetR[i]);
				if (l > peak) peak = l;
				if (r > peak) peak = r;
			}
			if (f->outputmode == TSF_STEREO_INTERLEAVED) for (i = 0; i != block; i++) { out[i * 2] += wetL[i]; out[i * 2 + 1] += wetR[i]; }
			else if (f->outputmode == TSF_STEREO_UNWEAVED) for (i = 0; i != block; i++) { out[i] += wetL[i]; out[total + i] += wetR[i]; }
			else for (i = 0; i != block; i++) out[i] += (wetL[i] + wetR[i]) * 0.70710678f; // the stereo returns' power (mostly uncorrelated)
			if (sending || peak > TSF_FX_SILENCE) fx->idle = 0;
			else if ((fx->idle += block) >= idleLimit) tsf_fx_clear(fx); // the tail has died out, start from silence next time
		}
		if (f->outputmode != TSF_STEREO_UNWEAVED) buffer += block * channels;
		offset += block; samples -= block;
	}
	#undef TSF_VOICE_SENDS
}

#define TSF_FXCLAMP(x) (!((x) > 0.0f) ? 0.0f : ((x) > 1.0f ? 1.0f : (x))) // NaN as 0
TSFDEF void tsf_set_reverb(tsf* f, int enabled, float room_size, float damping, float width, float wet)
{
	struct tsf_fx* fx; TSF_BOOL wasRunning;
	if (!f) return;
	if (!(fx = tsf_fx_get(f, enabled))) return;
	wasRunning = (fx->reverbEnabled && fx->reverbWet > 0.0f);
	fx->reverbEnabled = (enabled != 0); fx->roomSize = TSF_FXCLAMP(room_size); fx->damping = TSF_FXCLAMP(damping); fx->width = TSF_FXCLAMP(width); fx->reverbWet = TSF_FXCLAMP(wet);
	if (!wasRunning && fx->reverbEnabled && fx->reverbWet > 0.0f) tsf_fx_clear_lines(fx, 0, 12);
	tsf_fx_release_if_unused(f);
}

TSFDEF void tsf_set_chorus(tsf* f, int enabled, float depth, float rate, float wet)
{
	struct tsf_fx* fx; TSF_BOOL wasRunning;
	if (!f) return;
	if (!(fx = tsf_fx_get(f, enabled))) return;
	wasRunning = (fx->chorusEnabled && fx->chorusWet > 0.0f);
	fx->chorusEnabled = (enabled != 0); fx->chorusDepth = TSF_FXCLAMP(depth); fx->chorusRate = (!(rate > 0.0f) ? 0.0f : (rate > 5.0f ? 5.0f : rate)); fx->chorusWet = TSF_FXCLAMP(wet);
	if (!wasRunning && fx->chorusEnabled && fx->chorusWet > 0.0f) tsf_fx_clear_lines(fx, 12, 14);
	tsf_fx_release_if_unused(f);
}
#undef TSF_FXCLAMP

TSFDEF void tsf_reset_effects(tsf* f)
{
	if (f && f->fx) tsf_fx_clear(f->fx);
}

TSFDEF int tsf_get_voice_slot_count(const tsf* f)
{
	return (f ? f->voiceNum : 0);
}

TSFDEF int tsf_get_voice_effects(const tsf* f, int voice_index, float* chorus_send, float* reverb_send)
{
	const struct tsf_voice* v;
	if (!f || voice_index < 0 || voice_index >= f->voiceNum || (v = f->voices + voice_index)->playingPreset == -1) return 0;
	if (chorus_send) *chorus_send = v->chorusSend;
	if (reverb_send) *reverb_send = v->reverbSend;
	return 1;
}

#ifndef TSF_MAX_CHANNELS
#define TSF_MAX_CHANNELS 4096 // 256 MIDI ports of 16 channels; also keeps the allocation size from overflowing on 32-bit systems
#endif
static struct tsf_channel* tsf_channel_init(tsf* f, int channel)
{
	int i;
	if (channel < 0 || channel >= TSF_MAX_CHANNELS) return TSF_NULL;
	if (f->channels && (unsigned)channel < (unsigned)f->channels->channelNum) return &f->channels->channels[channel];
	if (!f->channels)
	{
		f->channels = (struct tsf_channels*)TSF_MALLOC(sizeof(struct tsf_channels) + sizeof(struct tsf_channel) * channel);
		if (!f->channels) return TSF_NULL;
		f->channels->channelNum = 0;
		f->channels->glideCents = f->channels->glideDelta = 0.0f;
	}
	else
	{
		struct tsf_channels *newChannels = (struct tsf_channels*)TSF_REALLOC(f->channels, sizeof(struct tsf_channels) + sizeof(struct tsf_channel) * channel);
		if (!newChannels) return TSF_NULL;
		f->channels = newChannels;
	}
	i = f->channels->channelNum;
	f->channels->channelNum = channel + 1;
	for (; i <= channel; i++)
	{
		struct tsf_channel* c = &f->channels->channels[i];
		c->presetIndex = c->bank = 0;
		c->pitchWheel = 8192;
		c->midiRPN = 0xFFFF;
		c->midiData = c->sustain = c->sostenuto = c->monoMode = 0;
		c->portaKey = c->lastKey = 255;
		c->hold2 = 0;
		c->panOffset = 0.0f;
		c->gainDB = 0.0f;
		c->pitchRange = 2.0f;
		c->tuning = c->tuneFine = 0.0f;
		c->tuneCoarse = 0;
		c->modDepth = 1.0f;
		c->nrpnMSB = 0; c->nrpnSel = 0xFFFF; c->nrpnAcc = 0;
		TSF_MEMSET(c->nrpn, 0, sizeof(c->nrpn));
		c->aweSet = 0;
		c->monoCount = 0;
		c->pressure = 0;
		TSF_MEMSET(c->cc, 0, sizeof(c->cc));
		c->cc[7] = 100; c->cc[11] = 127; // General MIDI defaults for volume and expression
		c->cc[8] = c->cc[10] = 64; // balance and pan center
		c->cc[71] = c->cc[72] = c->cc[73] = c->cc[74] = c->cc[75] = c->cc[76] = c->cc[77] = c->cc[78] = 64; // GM2 sound controllers: 64 is no change
	}
	return &f->channels->channels[channel];
}

// Re-evaluate the modulators of the sounding voices of a channel after a controller changed
static void tsf_channel_modulate(tsf* f, int channel)
{
	struct tsf_voice *v, *vEnd;
	for (v = f->voices, vEnd = v ? v + f->voiceNum : TSF_NULL; v != vEnd; v++)
		if (v->playingPreset != -1 && v->playingChannel == channel)
			tsf_voice_modulate(f, v, TSF_FALSE);
}

static void tsf_channel_applypitch(tsf* f, int channel, struct tsf_channel* c)
{
	struct tsf_voice *v, *vEnd;
	for (v = f->voices, vEnd = v ? v + f->voiceNum : TSF_NULL; v != vEnd; v++)
		if (v->playingPreset != -1 && v->playingChannel == channel)
			tsf_voice_calcpitchratio(v, &v->mr, c->tuning);
}

TSFDEF int tsf_channel_set_presetindex(tsf* f, int channel, int preset_index)
{
	struct tsf_channel *c = tsf_channel_init(f, channel);
	if (!c || preset_index < 0 || preset_index >= f->presetNum) return 0;
	c->presetIndex = (unsigned short)preset_index;
	return 1;
}

/* Resolve a preset with MMA/GS bank compatibility.
   MIDI bank is stored as MMA (MSB<<7)|LSB when both CC0 and CC32 were set (bit 15 flags MSB without an LSB).
   GS soundfonts use bank = CC0 (MSB) only
   Fallback order:
     1) MMA: exact bank
     2) GS-style: CC0 (MSB) only (bank >> 7)
     3) bank 0 (GM default) */
TSFDEF int tsf_channel_set_presetnumber(tsf* f, int channel, int preset_number, int flag_mididrums)
{
	int preset_index;
	struct tsf_channel *c = tsf_channel_init(f, channel);
	if (!c) return 0;
	if (flag_mididrums)
	{
		preset_index = tsf_get_presetindex(f, 128 | (c->bank & 0x7FFF), preset_number);
		if (preset_index == -1) preset_index = tsf_get_presetindex(f, 128, preset_number);
		if (preset_index == -1) preset_index = tsf_get_presetindex(f, 128, 0);
		if (preset_index == -1) preset_index = tsf_get_presetindex(f, (c->bank & 0x7FFF), preset_number);
	}
	else preset_index = tsf_get_presetindex(f, (c->bank & 0x7FFF), preset_number);
	if (preset_index == -1 && ((c->bank & 0x7FFF) >> 7) != (c->bank & 0x7FFF)) preset_index = tsf_get_presetindex(f, ((c->bank & 0x7FFF) >> 7), preset_number);
	if (preset_index == -1) preset_index = tsf_get_presetindex(f, 0, preset_number);
	if (preset_index != -1)
	{
		c->presetIndex = (unsigned short)preset_index;
		return 1;
	}
	return 0;
}

TSFDEF int tsf_channel_set_bank(tsf* f, int channel, int bank)
{
	struct tsf_channel *c = (bank < 0 || bank > 16383 ? TSF_NULL : tsf_channel_init(f, channel));
	if (!c) return 0;
	c->bank = (unsigned short)bank;
	return 1;
}

TSFDEF int tsf_channel_set_bank_preset(tsf* f, int channel, int bank, int preset_number)
{
	int preset_index;
	struct tsf_channel *c = tsf_channel_init(f, channel);
	if (!c) return 0;
	preset_index = tsf_get_presetindex(f, bank, preset_number);
	if (preset_index == -1) return 0;
	c->presetIndex = (unsigned short)preset_index;
	c->bank = (unsigned short)bank;
	return 1;
}

TSFDEF int tsf_channel_set_pan(tsf* f, int channel, float pan)
{
	struct tsf_voice *v, *vEnd;
	struct tsf_channel *c = tsf_channel_init(f, channel);
	if (!c || !TSF_FINITE(pan)) return 0;
	for (v = f->voices, vEnd = v ? v + f->voiceNum : TSF_NULL; v != vEnd; v++)
		if (v->playingPreset != -1 && v->playingChannel == channel)
			tsf_voice_setpan(v, v->mr.pan, pan - 0.5f);
	c->panOffset = pan - 0.5f;
	return 1;
}

TSFDEF int tsf_channel_set_volume(tsf* f, int channel, float volume)
{
	float gainDB = tsf_gainToDecibels(volume), gainDBChange;
	struct tsf_voice *v, *vEnd;
	struct tsf_channel *c = tsf_channel_init(f, channel);
	if (!c || !TSF_FINITE(volume)) return 0;
	if (gainDB == c->gainDB) return 1;
	for (v = f->voices, vEnd = v ? v + f->voiceNum : TSF_NULL, gainDBChange = gainDB - c->gainDB; v != vEnd; v++)
		if (v->playingPreset != -1 && v->playingChannel == channel)
			v->noteGainDB += gainDBChange;
	c->gainDB = gainDB;
	return 1;
}

TSFDEF int tsf_channel_set_pitchwheel(tsf* f, int channel, int pitch_wheel)
{
	struct tsf_channel *c = tsf_channel_init(f, channel);
	if (!c) return 0;
	pitch_wheel = (pitch_wheel < 0 ? 0 : (pitch_wheel > 16383 ? 16383 : pitch_wheel));
	if (c->pitchWheel == pitch_wheel) return 1;
	c->pitchWheel = (unsigned short)pitch_wheel;
	tsf_channel_modulate(f, channel);
	return 1;
}

TSFDEF int tsf_channel_set_pitchrange(tsf* f, int channel, float pitch_range)
{
	struct tsf_channel *c = tsf_channel_init(f, channel);
	if (!c || !TSF_FINITE(pitch_range)) return 0;
	if (c->pitchRange == pitch_range) return 1;
	c->pitchRange = pitch_range;
	tsf_channel_modulate(f, channel);
	return 1;
}

TSFDEF int tsf_channel_set_tuning(tsf* f, int channel, float tuning)
{
	struct tsf_channel *c = tsf_channel_init(f, channel);
	if (!c || !TSF_FINITE(tuning)) return 0;
	if (c->tuning == tuning) return 1;
	c->tuning = tuning;
	tsf_channel_applypitch(f, channel, c);
	return 1;
}

// End the notes whose keys were released and that no pedal (sustain, hold 2, sostenuto) holds anymore
static void tsf_channel_pedals_released(tsf* f, int channel, const struct tsf_channel* c)
{
	struct tsf_voice *v, *vEnd;
	if (c->sustain || c->hold2) return;
	for (v = f->voices, vEnd = v ? v + f->voiceNum : TSF_NULL; v != vEnd; v++)
		if (v->playingPreset != -1 && v->playingChannel == channel && TSF_VOICE_KEYDOWN(v) && v->heldSustain && !v->sostenuto)
			tsf_voice_end(f, v);
}

// Pressing sostenuto captures the notes sounding with their key down, releasing it ends those whose key was released meanwhile
static void tsf_channel_sostenuto(tsf* f, int channel, struct tsf_channel* c, int flag_sostenuto)
{
	struct tsf_voice *v, *vEnd;
	if (!c->sostenuto == !flag_sostenuto) return;
	c->sostenuto = (unsigned short)(flag_sostenuto != 0);
	for (v = f->voices, vEnd = v ? v + f->voiceNum : TSF_NULL; v != vEnd; v++)
		if (v->playingPreset != -1 && v->playingChannel == channel && TSF_VOICE_KEYDOWN(v))
			v->sostenuto = (unsigned char)(flag_sostenuto && !v->heldSustain);
	tsf_channel_pedals_released(f, channel, c);
}

TSFDEF int tsf_channel_set_sustain(tsf* f, int channel, int flag_sustain)
{
	struct tsf_channel *c = tsf_channel_init(f, channel);
	if (!c) return 0;
	return (!c->sustain == !flag_sustain ? 1 : tsf_channel_midi_control(f, channel, 64, flag_sustain ? 127 : 0));
}

TSFDEF int tsf_channel_set_sostenuto(tsf* f, int channel, int flag_sostenuto)
{
	struct tsf_channel *c = tsf_channel_init(f, channel);
	if (!c) return 0;
	return (!c->sostenuto == !flag_sostenuto ? 1 : tsf_channel_midi_control(f, channel, 66, flag_sostenuto ? 127 : 0));
}

// Start a glide of cents (the pitch offset the note starts with, it moves to 0 linearly) for the voice being started or retargeted.
// The time is the 14 bit value of CC5 (MSB) and CC37 (LSB) in milliseconds.
static void tsf_channel_glide(tsf* f, const struct tsf_channel* c, float cents, float* outCents, float* outDelta)
{
	float seconds = (c->cc[5] * 128 + c->cc[37]) * 0.001f;
	if (cents != 0.0f && seconds > 0.0f) { *outCents = cents; *outDelta = -cents / (seconds * f->outSampleRate); }
	else *outCents = *outDelta = 0.0f;
}

//a note whose key is down, or that sustain or hold 2 keeps (like a held key); a note kept by sostenuto stays as it is
#define TSF_LEGATO_SOURCE(v) (v->playingPreset != -1 && v->playingChannel == channel && TSF_VOICE_KEYDOWN(v) && (!v->heldSustain || !v->sostenuto))
//a note sounding for the (new) key: its key is down and no pedal keeps it
#define TSF_KEY_NOTE(v) (v->playingPreset != -1 && v->playingChannel == channel && v->playingKey == key && TSF_VOICE_KEYDOWN(v) && !v->heldSustain)

// Continue oldKey's voices at the new key without re-attacking, gliding from the CC84 key or with portamento on (false: none could).
// A voice whose zone doesn't cover the new key (another sample) is released, and a zone that covers only the new key (a layer)
// starts as part of the note. *restartVel gets the note's velocity, for a new note when no voice could continue.
static TSF_BOOL tsf_channel_retarget(tsf* f, int channel, struct tsf_channel* c, int oldKey, int key, float* restartVel)
{
	struct tsf_voice *v, *vEnd = (f->voices ? f->voices + f->voiceNum : TSF_NULL);
	TSF_BOOL found = TSF_FALSE;
	unsigned int playIndex = 0;
	int preset = 0;
	float glideCents = 0.0f, glideDelta = 0.0f, keepCents, keepDelta;
	*restartVel = 0.0f;
	for (v = f->voices; v != vEnd; v++)
		if (TSF_LEGATO_SOURCE(v) && v->playingKey == oldKey)
		{
			if (v->ampenv.midiVelocity > *restartVel * 127.0f) *restartVel = v->ampenv.midiVelocity * (1.0f / 127.0f);
			if (key < v->region->lokey || key > v->region->hikey) tsf_voice_end(f, v);
			else found = TSF_TRUE;
		}
	if (!found) return TSF_FALSE;
	// oldKey's voices can be several notes (pedal-held, struck again) and the key its own: one key down is one note, else they would hang
	for (v = f->voices; v != vEnd; v++)
		if ((TSF_LEGATO_SOURCE(v) && v->playingKey == oldKey) || TSF_KEY_NOTE(v)) { if (v->playIndex > playIndex) playIndex = v->playIndex; }
	for (v = f->voices; v != vEnd; v++)
		if (TSF_LEGATO_SOURCE(v) && v->playingKey == oldKey)
		{
			float cents = (c->portaKey != 255 ? (c->portaKey - key) * 100.0f : (oldKey - key) * 100.0f + v->portaCents);
			v->heldSustain = 0; // the new key is down now
			if (c->portaKey != 255 || c->cc[65] >= 64) tsf_channel_glide(f, c, cents, &v->portaCents, &v->portaDelta);
			else v->portaCents = v->portaDelta = 0.0f;
			v->playingKey = key;
			if (v->region->fixedKey < 0) v->key = (unsigned char)key;
			tsf_voice_modulate(f, v, TSF_FALSE); //key number sources
			tsf_voice_calcpitchratio(v, &v->mr, c->tuning);
			preset = v->playingPreset, glideCents = v->portaCents, glideDelta = v->portaDelta;
		}
	for (v = f->voices; v != vEnd; v++) if (TSF_KEY_NOTE(v)) v->playIndex = playIndex;
	// the zones of the note's preset that cover the new key but not the old one start, gliding along with the note
	keepCents = f->channels->glideCents, keepDelta = f->channels->glideDelta;
	f->channels->glideCents = glideCents, f->channels->glideDelta = glideDelta;
	tsf_note_on_channel(f, preset, key, *restartVel, channel, &playIndex, oldKey); // (can reallocate the voices)
	f->channels->glideCents = keepCents, f->channels->glideDelta = keepDelta;
	c->portaKey = 255;
	return TSF_TRUE;
}

// Mono mode: a key pressed while another one is held continues that note's voices with the new pitch instead of starting new ones
static TSF_BOOL tsf_channel_legato(tsf* f, int channel, struct tsf_channel* c, int key, float* restartVel)
{
	struct tsf_voice *v, *vEnd = (f->voices ? f->voices + f->voiceNum : TSF_NULL), *last = TSF_NULL;
	//the newest note whose key is down (sounding as the top key), else the newest one a pedal keeps
	for (v = f->voices; v != vEnd; v++)
		if (TSF_LEGATO_SOURCE(v) && (v->playingKey != key || v->heldSustain) &&
			(!last || (last->heldSustain && !v->heldSustain) || (last->heldSustain == v->heldSustain && v->playIndex > last->playIndex))) last = v;
	*restartVel = 0.0f;
	return (last && tsf_channel_retarget(f, channel, c, last->playingKey, key, restartVel));
}
#undef TSF_LEGATO_SOURCE
#undef TSF_KEY_NOTE

// Remember a key held down in mono mode or with the legato footswitch (moved to the top of the held keys, the last one sounds)
static void tsf_channel_hold_key(struct tsf_channel* c, int key)
{
	unsigned i = 0;
	while (i != c->monoCount && c->monoKeys[i] != key) i++;
	if (i == c->monoCount) { if (i == sizeof(c->monoKeys)) i = 0; /* full: the oldest key is dropped */ else c->monoCount++; }
	for (; i + 1 < c->monoCount; i++) c->monoKeys[i] = c->monoKeys[i + 1];
	c->monoKeys[c->monoCount - 1] = (unsigned char)key;
}

// The legato footswitch pressed in poly mode: the keys already held down are remembered (oldest note first), so releasing a
// legato key returns to a key that was held before the pedal
static void tsf_channel_hold_down_keys(tsf* f, int channel, struct tsf_channel* c)
{
	struct tsf_voice *v, *vEnd = (f->voices ? f->voices + f->voiceNum : TSF_NULL), *next;
	unsigned int after = 0;
	TSF_BOOL any = TSF_FALSE;
	c->monoCount = 0;
	for (;;)
	{
		for (next = TSF_NULL, v = f->voices; v != vEnd; v++)
			if (v->playingPreset != -1 && v->playingChannel == channel && TSF_VOICE_KEYDOWN(v) && !v->heldSustain &&
				(!any || v->playIndex > after) && (!next || v->playIndex < next->playIndex)) next = v;
		if (!next) return;
		tsf_channel_hold_key(c, next->playingKey);
		after = next->playIndex, any = TSF_TRUE;
	}
}

TSFDEF int tsf_channel_note_on(tsf* f, int channel, int key, float vel)
{
	struct tsf_channels* cs;
	struct tsf_channel* c;
	int res;
	float restartVel;
	if (key < 0 || key > 127) return 0;
	if (!(vel > 0.0f)) // also NaN
	{
		tsf_channel_note_off(f, channel, key);
		return ((unsigned)channel < TSF_MAX_CHANNELS);
	}
	c = tsf_channel_init(f, channel); // a note on a channel that wasn't set up yet plays its default preset
	if (!c) return 0;
	cs = f->channels;
	if (c->monoMode || c->cc[68] >= 64) // mono mode or legato footswitch
	{
		tsf_channel_hold_key(c, key);
		if (tsf_channel_legato(f, channel, c, key, &restartVel)) { c->lastKey = (unsigned char)key; return 1; } // else a new note
		if (c->monoMode) // the same key pressed again restarts the note: one note per channel in mono mode
		{
			struct tsf_voice *v, *vEnd = (f->voices ? f->voices + f->voiceNum : TSF_NULL);
			for (v = f->voices; v != vEnd; v++)
				if (v->playingPreset != -1 && v->playingChannel == channel && v->playingKey == key && TSF_VOICE_KEYDOWN(v)) tsf_voice_end(f, v);
		}
	}
	//CC84 (MIDI 1.0): a sounding note on the source key glides to the new key, no new voice
	if (c->portaKey != 255 && c->portaKey != key && tsf_channel_retarget(f, channel, c, c->portaKey, key, &restartVel)) { c->lastKey = (unsigned char)key; return 1; }
	if (c->portaKey != 255 || c->cc[65] >= 64) //glide from the CC84 key (this note only, also with portamento off), or with portamento on from the previous note
	{
		int from = (c->portaKey != 255 ? c->portaKey : c->lastKey);
		if (from != 255) tsf_channel_glide(f, c, (from - key) * 100.0f, &cs->glideCents, &cs->glideDelta);
	}
	c->portaKey = 255;
	c->lastKey = (unsigned char)key;
	res = tsf_note_on_channel(f, c->presetIndex, key, vel, channel, TSF_NULL, 0);
	cs->glideCents = cs->glideDelta = 0.0f;
	return res;
}

TSFDEF void tsf_channel_note_off(tsf* f, int channel, int key)
{
	unsigned sustain;
	struct tsf_voice *v = f->voices, *vEnd = v ? v + f->voiceNum : TSF_NULL, *vMatchFirst = TSF_NULL, *vMatchLast = TSF_NULL;
	if (!f->channels || (unsigned)channel >= (unsigned)f->channels->channelNum) return;
	if (f->channels->channels[channel].monoMode || f->channels->channels[channel].cc[68] >= 64) // mono mode or legato footswitch
	{
		struct tsf_channel* c = &f->channels->channels[channel];
		unsigned i = 0, wasTop;
		while (i != c->monoCount && c->monoKeys[i] != key) i++;
		if (i != c->monoCount) // forget the key, if it was the sounding one and another is still held the note returns to that key
		{
			wasTop = (i + 1 == c->monoCount);
			for (c->monoCount--; i < c->monoCount; i++) c->monoKeys[i] = c->monoKeys[i + 1];
			if (wasTop && c->monoCount) // the released key's own note continues at the held key
			{
				int back = c->monoKeys[c->monoCount - 1];
				float restartVel;
				for (v = f->voices; v != vEnd; v++) // unless that key sounds its own note (held from before the legato footswitch)
					if (v->playingPreset != -1 && v->playingChannel == channel && v->playingKey == back && TSF_VOICE_KEYDOWN(v) && !v->heldSustain) break;
				if (v == vEnd)
				{
					unsigned char ptc = c->portaKey; // a CC84 key is for the next note on, not for this return
					TSF_BOOL moved;
					c->portaKey = 255;
					moved = tsf_channel_retarget(f, channel, c, key, back, &restartVel);
					if (moved) c->lastKey = (unsigned char)back; // the next note glides from the key that sounds
					else if (restartVel > 0.0f) tsf_channel_note_on(f, channel, back, restartVel); // the held key is in another zone: a new note
					c->portaKey = ptc;
					if (moved || restartVel > 0.0f) return;
				}
				v = f->voices;
			}
		}
	}
	for (; v != vEnd; v++)
	{
		//Find the first and last entry in the voices list with matching channel, key and look up the smallest play index
		if (v->playingPreset == -1 || v->playingChannel != channel || v->playingKey != key || !TSF_VOICE_KEYDOWN(v) || v->heldSustain) continue;
		else if (!vMatchFirst || v->playIndex < vMatchFirst->playIndex) vMatchFirst = vMatchLast = v;
		else if (v->playIndex == vMatchFirst->playIndex) vMatchLast = v;
	}
	if (!vMatchFirst) return;
	for (sustain = (f->channels->channels[channel].sustain || f->channels->channels[channel].hold2), v = vMatchFirst; v <= vMatchLast; v++)
	{
		//Stop all voices with matching channel, key and the smallest play index which was enumerated above
		if (v != vMatchFirst && v != vMatchLast &&
			(v->playIndex != vMatchFirst->playIndex || v->playingPreset == -1 || v->playingChannel != channel || v->playingKey != key || !TSF_VOICE_KEYDOWN(v))) continue;
		//Don't turn off if sustain or hold 2 is active or the note is held by sostenuto, just mark as held so we don't forget it
		if (sustain || v->sostenuto)
			v->heldSustain = 1;
		else
			tsf_voice_end(f, v);
	}
}

TSFDEF void tsf_channel_note_off_all(tsf* f, int channel)
{
	//Ignore sustain channel settings, note_off_all overrides
	struct tsf_voice *v = f->voices, *vEnd = v ? v + f->voiceNum : TSF_NULL;
	if (f->channels && (unsigned)channel < (unsigned)f->channels->channelNum) f->channels->channels[channel].monoCount = 0;
	for (; v != vEnd; v++)
		if (v->playingPreset != -1 && v->playingChannel == channel && TSF_VOICE_KEYDOWN(v))
			tsf_voice_end(f, v);
}

// Release all notes of a channel like note off messages would: pedals keep notes sounding (CC123, mode changes)
static void tsf_channel_release_all(tsf* f, int channel, struct tsf_channel* c)
{
	struct tsf_voice *v = f->voices, *vEnd = v ? v + f->voiceNum : TSF_NULL;
	c->monoCount = 0;
	for (; v != vEnd; v++)
		if (v->playingPreset != -1 && v->playingChannel == channel && TSF_VOICE_KEYDOWN(v) && !v->heldSustain)
		{
			if (c->sustain || c->hold2 || v->sostenuto) v->heldSustain = 1;
			else tsf_voice_end(f, v);
		}
}

TSFDEF void tsf_channel_sounds_off_all(tsf* f, int channel)
{
	struct tsf_voice *v = f->voices, *vEnd = v ? v + f->voiceNum : TSF_NULL;
	if (f->channels && (unsigned)channel < (unsigned)f->channels->channelNum) f->channels->channels[channel].monoCount = 0;
	for (; v != vEnd; v++)
		if (v->playingPreset != -1 && v->playingChannel == channel && (v->ampenv.segment < TSF_SEGMENT_RELEASE || v->ampenv.parameters.release))
			tsf_voice_endquick(f, v);
}

TSFDEF int tsf_channel_set_pressure(tsf* f, int channel, int pressure)
{
	struct tsf_channel *c = tsf_channel_init(f, channel);
	if (!c) return 0;
	pressure = (pressure < 0 ? 0 : (pressure > 127 ? 127 : pressure));
	if (c->pressure == pressure) return 1;
	c->pressure = (unsigned char)pressure;
	tsf_channel_modulate(f, channel);
	return 1;
}

TSFDEF int tsf_channel_set_poly_pressure(tsf* f, int channel, int key, int key_pressure)
{
	struct tsf_voice *v, *vEnd;
	struct tsf_channel *c = (key < 0 || key > 127 ? TSF_NULL : tsf_channel_init(f, channel));
	if (!c) return 0;
	key_pressure = (key_pressure < 0 ? 0 : (key_pressure > 127 ? 127 : key_pressure));
	for (v = f->voices, vEnd = v ? v + f->voiceNum : TSF_NULL; v != vEnd; v++)
		if (v->playingPreset != -1 && v->playingChannel == channel && v->playingKey == key && v->keyPressure != key_pressure)
		{
			v->keyPressure = (unsigned char)key_pressure;
			tsf_voice_modulate(f, v, TSF_FALSE);
		}
	return 1;
}

// The selected RPN's or SF2.01 NRPN's current 14-bit value, for data increment/decrement and a lone RPN LSB
static int tsf_channel_data_value(const struct tsf_channel* c)
{
	switch (c->midiRPN)
	{
		case 0: // pitch bend range: semitones (MSB) and cents (LSB)
		{
			float r = (c->pitchRange > 0.0f ? (c->pitchRange < 127.99f ? c->pitchRange : 127.99f) : 0.0f);
			int semi = (int)r, ct = (int)((r - semi) * 100.0f + 0.5f);
			return (semi << 7) | (ct > 99 ? 99 : ct);
		}
		case 1: { float x = c->tuneFine * 8192.0f + 8192.5f; return (x < 0.0f ? 0 : (x > 16383.0f ? 16383 : (int)x)); } // fine tuning
		case 2: return (c->tuneCoarse + 64) << 7; // coarse tuning
		case 5: // modulation depth range: semitones (MSB) and 100/128 cents (LSB)
		{
			float cents = c->modDepth * 50.0f; int semi = (int)(cents / 100.0f), lsb;
			if (semi > 127) return 16383;
			lsb = (int)((cents - semi * 100.0f) * 1.28f + 0.5f);
			return (semi << 7) | (lsb > 127 ? 127 : lsb);
		}
		case 0xFFFF: if (c->nrpnSel < TSF_NUMGENS && tsf_nrpn_scale[c->nrpnSel]) return c->nrpn[c->nrpnSel] + 8192; break; // SF2.01 NRPN
	}
	return c->midiData;
}

TSFDEF int tsf_channel_midi_control(tsf* f, int channel, int controller, int control_value)
{
	struct tsf_channel* c = ((unsigned)controller > 127 ? TSF_NULL : tsf_channel_init(f, channel));
	unsigned char prev;
	if (!c) return 0;
	if (control_value < 0) control_value = 0; else if (control_value > 127) control_value = 127; // MIDI data bytes are 7 bit
	prev = c->cc[controller];
	{
		TSF_BOOL changed = (c->cc[controller] != (unsigned char)control_value);
		if (controller - 1u < 31u && controller != 6 && c->cc[controller + 32]) c->cc[controller + 32] = 0, changed = TSF_TRUE; // MIDI 1.0: an MSB sets the LSB to zero
		if (changed)
		{
			int tracked = controller - (controller - 33u < 31u && controller != 38 ? 32 : 0); // an LSB change counts for its MSB controller
			c->cc[controller] = (unsigned char)control_value;
			if (f->modCCs[tracked >> 5] & (1u << (tracked & 31)) || f->modCCs[controller >> 5] & (1u << (controller & 31))) tsf_channel_modulate(f, channel);
		}
	}
	switch (controller)
	{
		case   6 /*DATA_ENTRY_MSB*/  : c->midiData       = (unsigned short)((c->midiRPN == 0xFFFF ? (c->midiData & 0x7F) : 0) | (control_value << 7)); goto TCMC_SET_DATA; // MSB zeroes LSB, except for the SF2.01 NRPN (9.6.3)
		case  38 /*DATA_ENTRY_LSB*/  : c->midiData       = (unsigned short)(((c->midiRPN == 0xFFFF ? c->midiData : tsf_channel_data_value(c)) & 0x3F80) | control_value); goto TCMC_SET_DATA; // an RPN's LSB completes its current value
		case   0 /*BANK_SELECT_MSB*/ : c->bank = (unsigned short)(0x8000 | control_value); return 1; //bank select MSB alone acts like LSB
		case  32 /*BANK_SELECT_LSB*/ :
			c->bank = (unsigned short)((c->bank & 0x8000 ? ((c->bank & 0x7F) << 7) : (c->bank & 0x3F80)) | control_value); //the MSB stays when only the LSB changes
			if (c->bank == 128) c->bank = 0x8000 | 1; // MSB 1 LSB 0 is not the SoundFont percussion bank 128 but MSB 1
			return 1;
		case 101 /*RPN_MSB*/         : c->midiRPN = (unsigned short)(((c->midiRPN == 0xFFFF ? 0 : c->midiRPN) & 0x7F  ) | (control_value << 7)); return 1;
		case 100 /*RPN_LSB*/         : c->midiRPN = (unsigned short)(((c->midiRPN == 0xFFFF ? 0 : c->midiRPN) & 0x3F80) |  control_value); return 1;
		case  99 /*NRPN_MSB*/        : c->midiRPN = 0xFFFF; c->nrpnMSB = (unsigned char)control_value; c->nrpnSel = 0xFFFF; c->nrpnAcc = 0; return 1;
		case  98 /*NRPN_LSB*/        : // SF2.01 NRPN (9.6) with MSB 120: LSB 100/101/102 add 100/1000/10000, LSB 0-99 completes the generator number
			c->midiRPN = 0xFFFF;
			if (c->nrpnMSB == 127) { c->nrpnSel = (unsigned short)(control_value <= 26 ? 0x8000 | control_value : 0xFFFF); return 1; } // AWE32 NRPN
			if (c->nrpnMSB != 120) return 1;
			if (control_value < 100) // a generator that is unknown or not a real-time parameter is ignored, the selection stays (9.6.2)
			{
				int gen = c->nrpnAcc + control_value;
				if (gen < TSF_NUMGENS && tsf_nrpn_scale[gen]) c->nrpnSel = (unsigned short)gen;
				c->nrpnAcc = 0;
			}
			else if (control_value <= 102) { c->nrpnAcc = (unsigned short)(c->nrpnAcc + (control_value == 100 ? 100 : (control_value == 101 ? 1000 : 10000))); }
			return 1;
		case  64 /*SUSTAIN*/         : c->sustain = (unsigned short)(control_value >= 64); tsf_channel_pedals_released(f, channel, c); return 1;
		case  69 /*HOLD2*/           : c->hold2 = (unsigned short)(control_value >= 64); tsf_channel_pedals_released(f, channel, c); return 1;
		case  68 /*LEGATO_SWITCH*/   : // poly mode: the keys are only tracked while the pedal is down
			if (c->monoMode) return 1;
			if (control_value < 64) c->monoCount = 0; // poly again: forget the held keys
			else if (prev < 64) tsf_channel_hold_down_keys(f, channel, c); // the keys already held, legato returns to them
			return 1;
		case  96 /*DATA_INCREMENT*/  : // RP-018: steps the LSB, the MSB for coarse tuning
		case  97 /*DATA_DECREMENT*/  :
		{
			int step = (c->midiRPN == 2 ? 128 : 1), d = tsf_channel_data_value(c); // from the selected parameter's current value
			if (c->midiRPN == 0) // pitch bend range: the LSB is cents (0-99), so step by one cent across the semitones
			{
				d = (d >> 7) * 100 + ((d & 0x7F) > 99 ? 99 : (d & 0x7F)) + (controller == 96 ? 1 : -1);
				d = (d < 0 ? 0 : (d > 127 * 100 + 99 ? 127 * 100 + 99 : d));
				c->midiData = (unsigned short)(((d / 100) << 7) | (d % 100));
			}
			else
			{
				d += (controller == 96 ? step : -step);
				c->midiData = (unsigned short)(d < 0 ? 0 : (d > 16383 ? 16383 : d));
			}
			goto TCMC_SET_DATA;
		}
		case 120 /*ALL_SOUND_OFF*/   : tsf_channel_sounds_off_all(f, channel); return 1;
		case  66 /*SOSTENUTO*/       : tsf_channel_sostenuto(f, channel, c, (int)(control_value >= 64)); return 1;
		case  84 /*PORTAMENTO_CTRL*/ : c->portaKey = (unsigned char)(control_value & 0x7F); return 1; // glide start key for the next note
		case 123 /*ALL_NOTES_OFF*/   : // mode messages 124 to 127 also act as All Notes Off (MIDI 1.0)
		case 124 /*OMNI_OFF*/        :
		case 125 /*OMNI_ON*/         : tsf_channel_release_all(f, channel, c); return 1;
		case 126 /*MONO_ON*/         : c->monoMode = 1; tsf_channel_release_all(f, channel, c); return 1;
		case 127 /*POLY_ON*/         : c->monoMode = 0; tsf_channel_release_all(f, channel, c); return 1;
		case 121 /*ALL_CTRL_OFF*/    : // RP-015: bank, program, volume, pan, effect depths, sound controllers and the RPN/NRPN data (pitch range, tuning, NRPN values) are kept
		{
			int i;
			c->hold2 = c->sustain = 0;
			if (c->sostenuto) tsf_channel_sostenuto(f, channel, c, 0);
			else tsf_channel_pedals_released(f, channel, c);
			for (i = 0; i != 120; i++)
				if (i != 0 && i != 32 && i != 7 && i != 39 && i != 10 && i != 42 && !(i >= 70 && i <= 79) && !(i >= 91 && i <= 95)) c->cc[i] = 0;
			c->cc[11] = 127; // expression
			c->cc[8] = 64; // balance
			if (!c->monoMode) c->monoCount = 0; // the legato footswitch is off now
			c->pressure = 0;
			{ struct tsf_voice *v, *vEnd; for (v = f->voices, vEnd = v ? v + f->voiceNum : TSF_NULL; v != vEnd; v++) if (v->playingChannel == channel) v->keyPressure = 0; }
			c->pitchWheel = 8192;
			c->midiRPN = 0xFFFF;
			c->midiData = 0;
			c->nrpnMSB = 0; c->nrpnSel = 0xFFFF; c->nrpnAcc = 0; // only the selection: the values set stay
			c->portaKey = 255;
			tsf_channel_modulate(f, channel);
			return 1;
		}
	}
	return 1;
TCMC_SET_DATA:
	if      (c->midiRPN == 0) tsf_channel_set_pitchrange(f, channel, (c->midiData >> 7) + 0.01f * (c->midiData & 0x7F));
	else if (c->midiRPN == 1) { c->tuneFine = ((float)c->midiData - 8192.0f) / 8192.0f; tsf_channel_set_tuning(f, channel, c->tuneCoarse + c->tuneFine); } //fine tune (100/8192 cents steps)
	else if (c->midiRPN == 2) { c->tuneCoarse = (signed char)((c->midiData >> 7) - 64); tsf_channel_set_tuning(f, channel, c->tuneCoarse + c->tuneFine); } //coarse tune (semitones, the MSB)
	else if (c->midiRPN == 5) { c->modDepth = ((c->midiData >> 7) * 100.0f + (c->midiData & 0x7F) * (100.0f / 128.0f)) * 0.02f; tsf_channel_modulate(f, channel); } //modulation depth range: semitones and 100/128 cents, default 50 cents
	else if (c->midiRPN == 0xFFFF && c->nrpnSel < TSF_NUMGENS && tsf_nrpn_scale[c->nrpnSel] && controller != 38) //SF2.01 NRPN: applied when the data MSB arrives
	{
		c->nrpn[c->nrpnSel] = (short)(c->midiData - 8192);
		tsf_channel_modulate(f, channel);
	}
	else if (c->midiRPN == 0xFFFF && c->nrpnSel >= 0x8000 && c->nrpnSel != 0xFFFF) //AWE32 NRPN: applied with either data entry byte
	{
		int param = c->nrpnSel & 0x7FFF;
		c->awe[param] = tsf_awe_nrpn_value(param, c->midiData);
		c->aweSet |= 1u << param;
		tsf_channel_modulate(f, channel);
	}
	return 1;
}

TSFDEF int tsf_channel_get_preset_index(tsf* f, int channel)
{
	return (f->channels && (unsigned)channel < (unsigned)f->channels->channelNum ? f->channels->channels[channel].presetIndex : 0);
}

TSFDEF int tsf_channel_get_preset_bank(tsf* f, int channel)
{
	return (f->channels && (unsigned)channel < (unsigned)f->channels->channelNum ? (f->channels->channels[channel].bank & 0x7FFF) : 0);
}

TSFDEF int tsf_channel_get_preset_number(tsf* f, int channel)
{
	return (f->channels && (unsigned)channel < (unsigned)f->channels->channelNum && f->channels->channels[channel].presetIndex < f->presetNum ? f->presets[f->channels->channels[channel].presetIndex].preset : 0);
}

TSFDEF float tsf_channel_get_pan(tsf* f, int channel)
{
	return (f->channels && (unsigned)channel < (unsigned)f->channels->channelNum ? f->channels->channels[channel].panOffset + 0.5f : 0.5f);
}

TSFDEF float tsf_channel_get_volume(tsf* f, int channel)
{
	return (f->channels && (unsigned)channel < (unsigned)f->channels->channelNum ? tsf_decibelsToGain(f->channels->channels[channel].gainDB) : 1.0f);
}

TSFDEF int tsf_channel_get_pitchwheel(tsf* f, int channel)
{
	return (f->channels && (unsigned)channel < (unsigned)f->channels->channelNum ? f->channels->channels[channel].pitchWheel : 8192);
}

TSFDEF float tsf_channel_get_pitchrange(tsf* f, int channel)
{
	return (f->channels && (unsigned)channel < (unsigned)f->channels->channelNum ? f->channels->channels[channel].pitchRange : 2.0f);
}

TSFDEF float tsf_channel_get_tuning(tsf* f, int channel)
{
	return (f->channels && (unsigned)channel < (unsigned)f->channels->channelNum ? f->channels->channels[channel].tuning : 0.0f);
}

#ifdef __cplusplus
}
#endif

#endif //TSF_IMPLEMENTATION

