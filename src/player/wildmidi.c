/*
 * wildmidi.c -- Midi Player using the WildMidi Midi Processing Library
 *
 * Copyright (C) WildMidi Developers 2001-2016
 *
 * This file is part of WildMIDI.
 *
 * WildMIDI is free software: you can redistribute and/or modify the player
 * under the terms of the GNU General Public License and you can redistribute
 * and/or modify the library under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation, either version 3 of
 * the licenses, or(at your option) any later version.
 *
 * WildMIDI is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License and
 * the GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License and the
 * GNU Lesser General Public License along with WildMIDI.  If not,  see
 * <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <stdint.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(_WIN32) || defined(__CYGWIN__) || defined(__DJGPP__) || \
    defined(__OS2__) || defined(__EMX__) || defined(WILDMIDI_AMIGA)
#include "getopt_long.h"
#else
#include <unistd.h>
#include <getopt.h>
#endif

#include "wildplay.h"
#include "filenames.h"
#include "wm_tty.h"
#include "wildmidi_lib.h"

/* available outputs */
static const audiodrv_info *available_outputs[] = {
    &audiodrv_none, /* always available */
    &audiodrv_wave, /* always available */
#ifdef AUDIODRV_COREAUDIO
    &audiodrv_coreaudio,
#endif
#ifdef AUDIODRV_ALSA
    &audiodrv_alsa,
#endif
#ifdef AUDIODRV_SNDIO
    &audiodrv_sndio,
#endif
#ifdef AUDIODRV_OSS
    &audiodrv_oss,
#endif
#ifdef AUDIODRV_AHI
    &audiodrv_ahi,
#endif
#ifdef AUDIODRV_WINMM
    &audiodrv_winmm,
#endif
#ifdef AUDIODRV_OS2DART
    &audiodrv_dart,
#endif
#ifdef AUDIODRV_OPENAL
    &audiodrv_openal,
#endif
    NULL /* nul terminate */
};

struct _midi_test {
    uint8_t *data;
    uint32_t size;
};

/* scale test from 0 to 127
 * test a
 * offset 18-21 (0x12-0x15) - track size
 * offset 25 (0x1A) = bank number
 * offset 28 (0x1D) = patch number
 */
static uint8_t midi_test_c_scale[] = {
    0x4d, 0x54, 0x68, 0x64, 0x00, 0x00, 0x00, 0x06, /* 0x00    */
    0x00, 0x00, 0x00, 0x01, 0x00, 0x06, 0x4d, 0x54, /* 0x08    */
    0x72, 0x6b, 0x00, 0x00, 0x02, 0x63, 0x00, 0xb0, /* 0x10    */
    0x00, 0x00, 0x00, 0xC0, 0x00, 0x00, 0x90, 0x00, /* 0x18  C */
    0x64, 0x08, 0x80, 0x00, 0x00, 0x08, 0x90, 0x02, /* 0x20  D */
    0x64, 0x08, 0x80, 0x02, 0x00, 0x08, 0x90, 0x04, /* 0x28  E */
    0x64, 0x08, 0x80, 0x04, 0x00, 0x08, 0x90, 0x05, /* 0x30  F */
    0x64, 0x08, 0x80, 0x05, 0x00, 0x08, 0x90, 0x07, /* 0x38  G */
    0x64, 0x08, 0x80, 0x07, 0x00, 0x08, 0x90, 0x09, /* 0x40  A */
    0x64, 0x08, 0x80, 0x09, 0x00, 0x08, 0x90, 0x0b, /* 0x48  B */
    0x64, 0x08, 0x80, 0x0b, 0x00, 0x08, 0x90, 0x0c, /* 0x50  C */
    0x64, 0x08, 0x80, 0x0c, 0x00, 0x08, 0x90, 0x0e, /* 0x58  D */
    0x64, 0x08, 0x80, 0x0e, 0x00, 0x08, 0x90, 0x10, /* 0x60  E */
    0x64, 0x08, 0x80, 0x10, 0x00, 0x08, 0x90, 0x11, /* 0x68  F */
    0x64, 0x08, 0x80, 0x11, 0x00, 0x08, 0x90, 0x13, /* 0x70  G */
    0x64, 0x08, 0x80, 0x13, 0x00, 0x08, 0x90, 0x15, /* 0x78  A */
    0x64, 0x08, 0x80, 0x15, 0x00, 0x08, 0x90, 0x17, /* 0x80  B */
    0x64, 0x08, 0x80, 0x17, 0x00, 0x08, 0x90, 0x18, /* 0x88  C */
    0x64, 0x08, 0x80, 0x18, 0x00, 0x08, 0x90, 0x1a, /* 0x90  D */
    0x64, 0x08, 0x80, 0x1a, 0x00, 0x08, 0x90, 0x1c, /* 0x98  E */
    0x64, 0x08, 0x80, 0x1c, 0x00, 0x08, 0x90, 0x1d, /* 0xA0  F */
    0x64, 0x08, 0x80, 0x1d, 0x00, 0x08, 0x90, 0x1f, /* 0xA8  G */
    0x64, 0x08, 0x80, 0x1f, 0x00, 0x08, 0x90, 0x21, /* 0xB0  A */
    0x64, 0x08, 0x80, 0x21, 0x00, 0x08, 0x90, 0x23, /* 0xB8  B */
    0x64, 0x08, 0x80, 0x23, 0x00, 0x08, 0x90, 0x24, /* 0xC0  C */
    0x64, 0x08, 0x80, 0x24, 0x00, 0x08, 0x90, 0x26, /* 0xC8  D */
    0x64, 0x08, 0x80, 0x26, 0x00, 0x08, 0x90, 0x28, /* 0xD0  E */
    0x64, 0x08, 0x80, 0x28, 0x00, 0x08, 0x90, 0x29, /* 0xD8  F */
    0x64, 0x08, 0x80, 0x29, 0x00, 0x08, 0x90, 0x2b, /* 0xE0  G */
    0x64, 0x08, 0x80, 0x2b, 0x00, 0x08, 0x90, 0x2d, /* 0xE8  A */
    0x64, 0x08, 0x80, 0x2d, 0x00, 0x08, 0x90, 0x2f, /* 0xF0  B */
    0x64, 0x08, 0x80, 0x2f, 0x00, 0x08, 0x90, 0x30, /* 0xF8  C */
    0x64, 0x08, 0x80, 0x30, 0x00, 0x08, 0x90, 0x32, /* 0x100 D */
    0x64, 0x08, 0x80, 0x32, 0x00, 0x08, 0x90, 0x34, /* 0x108 E */
    0x64, 0x08, 0x80, 0x34, 0x00, 0x08, 0x90, 0x35, /* 0x110 F */
    0x64, 0x08, 0x80, 0x35, 0x00, 0x08, 0x90, 0x37, /* 0x118 G */
    0x64, 0x08, 0x80, 0x37, 0x00, 0x08, 0x90, 0x39, /* 0x120 A */
    0x64, 0x08, 0x80, 0x39, 0x00, 0x08, 0x90, 0x3b, /* 0X128 B */
    0x64, 0x08, 0x80, 0x3b, 0x00, 0x08, 0x90, 0x3c, /* 0x130 C */
    0x64, 0x08, 0x80, 0x3c, 0x00, 0x08, 0x90, 0x3e, /* 0x138 D */
    0x64, 0x08, 0x80, 0x3e, 0x00, 0x08, 0x90, 0x40, /* 0X140 E */
    0x64, 0x08, 0x80, 0x40, 0x00, 0x08, 0x90, 0x41, /* 0x148 F */
    0x64, 0x08, 0x80, 0x41, 0x00, 0x08, 0x90, 0x43, /* 0x150 G */
    0x64, 0x08, 0x80, 0x43, 0x00, 0x08, 0x90, 0x45, /* 0x158 A */
    0x64, 0x08, 0x80, 0x45, 0x00, 0x08, 0x90, 0x47, /* 0x160 B */
    0x64, 0x08, 0x80, 0x47, 0x00, 0x08, 0x90, 0x48, /* 0x168 C */
    0x64, 0x08, 0x80, 0x48, 0x00, 0x08, 0x90, 0x4a, /* 0x170 D */
    0x64, 0x08, 0x80, 0x4a, 0x00, 0x08, 0x90, 0x4c, /* 0x178 E */
    0x64, 0x08, 0x80, 0x4c, 0x00, 0x08, 0x90, 0x4d, /* 0x180 F */
    0x64, 0x08, 0x80, 0x4d, 0x00, 0x08, 0x90, 0x4f, /* 0x188 G */
    0x64, 0x08, 0x80, 0x4f, 0x00, 0x08, 0x90, 0x51, /* 0x190 A */
    0x64, 0x08, 0x80, 0x51, 0x00, 0x08, 0x90, 0x53, /* 0x198 B */
    0x64, 0x08, 0x80, 0x53, 0x00, 0x08, 0x90, 0x54, /* 0x1A0 C */
    0x64, 0x08, 0x80, 0x54, 0x00, 0x08, 0x90, 0x56, /* 0x1A8 D */
    0x64, 0x08, 0x80, 0x56, 0x00, 0x08, 0x90, 0x58, /* 0x1B0 E */
    0x64, 0x08, 0x80, 0x58, 0x00, 0x08, 0x90, 0x59, /* 0x1B8 F */
    0x64, 0x08, 0x80, 0x59, 0x00, 0x08, 0x90, 0x5b, /* 0x1C0 G */
    0x64, 0x08, 0x80, 0x5b, 0x00, 0x08, 0x90, 0x5d, /* 0x1C8 A */
    0x64, 0x08, 0x80, 0x5d, 0x00, 0x08, 0x90, 0x5f, /* 0x1D0 B */
    0x64, 0x08, 0x80, 0x5f, 0x00, 0x08, 0x90, 0x60, /* 0x1D8 C */
    0x64, 0x08, 0x80, 0x60, 0x00, 0x08, 0x90, 0x62, /* 0x1E0 D */
    0x64, 0x08, 0x80, 0x62, 0x00, 0x08, 0x90, 0x64, /* 0x1E8 E */
    0x64, 0x08, 0x80, 0x64, 0x00, 0x08, 0x90, 0x65, /* 0x1F0 F */
    0x64, 0x08, 0x80, 0x65, 0x00, 0x08, 0x90, 0x67, /* 0x1F8 G */
    0x64, 0x08, 0x80, 0x67, 0x00, 0x08, 0x90, 0x69, /* 0x200 A */
    0x64, 0x08, 0x80, 0x69, 0x00, 0x08, 0x90, 0x6b, /* 0x208 B */
    0x64, 0x08, 0x80, 0x6b, 0x00, 0x08, 0x90, 0x6c, /* 0x210 C */
    0x64, 0x08, 0x80, 0x6c, 0x00, 0x08, 0x90, 0x6e, /* 0x218 D */
    0x64, 0x08, 0x80, 0x6e, 0x00, 0x08, 0x90, 0x70, /* 0x220 E */
    0x64, 0x08, 0x80, 0x70, 0x00, 0x08, 0x90, 0x71, /* 0x228 F */
    0x64, 0x08, 0x80, 0x71, 0x00, 0x08, 0x90, 0x73, /* 0x230 G */
    0x64, 0x08, 0x80, 0x73, 0x00, 0x08, 0x90, 0x75, /* 0x238 A */
    0x64, 0x08, 0x80, 0x75, 0x00, 0x08, 0x90, 0x77, /* 0x240 B */
    0x64, 0x08, 0x80, 0x77, 0x00, 0x08, 0x90, 0x78, /* 0x248 C */
    0x64, 0x08, 0x80, 0x78, 0x00, 0x08, 0x90, 0x7a, /* 0x250 D */
    0x64, 0x08, 0x80, 0x7a, 0x00, 0x08, 0x90, 0x7c, /* 0x258 E */
    0x64, 0x08, 0x80, 0x7c, 0x00, 0x08, 0x90, 0x7d, /* 0x260 F */
    0x64, 0x08, 0x80, 0x7d, 0x00, 0x08, 0x90, 0x7f, /* 0x268 G */
    0x64, 0x08, 0x80, 0x7f, 0x00, 0x08, 0xff, 0x2f, /* 0x270   */
    0x00                                            /* 0x278   */
};

static struct _midi_test midi_test[] = {
    { midi_test_c_scale, 663 },
    { NULL, 0 }
};

static int midi_test_max = 1;
unsigned int rate = 32072;

/*
 MIDI Output Functions
 */
static char midi_file[1024];

static void mk_midifile_name(const char *src) {
    char *p;
    int len;

    strncpy(midi_file, src, sizeof(midi_file) - 1);
    midi_file[sizeof(midi_file) - 1] = 0;

    p = strrchr(midi_file, '.');
    if (p && (len = strlen(p)) <= 4) {
        if (p - midi_file <= (int)sizeof(midi_file) - 5) {
            memcpy(p, ".mid", 5);
            return;
        }
    }

    len = strlen(midi_file);
    if (len > (int)sizeof(midi_file) - 5)
        len = (int)sizeof(midi_file) - 5;
    p = &midi_file[len];
    memcpy(p, ".mid", 5);
}

static int write_midi_output(void *output_data, size_t output_size) {
    FILE *outf;

    if (midi_file[0] == '\0')
        return (-1);

/*
 * Test if file already exists 
 */
    outf = fopen(midi_file, "rb");
    if (outf != NULL) {
        fprintf(stderr, "\rError: %s already exists\r\n", midi_file);
        return (-1);
    }

    outf = fopen(midi_file, "wb");
    if (!outf) {
        fprintf(stderr, "Error: unable to open file for writing (%s)\r\n", strerror(errno));
        return (-1);
    }

    if (fwrite(output_data, 1, output_size, outf) != output_size) {
        fprintf(stderr, "\nERROR: failed writing midi (%s)\r\n", strerror(errno));
        fclose(outf);
        return (-1);
    }

    fclose(outf);
    return (0);
}

static struct option const long_options[] = {
    { "version", 0, 0, 'v' },
    { "help", 0, 0, 'h' },
    { "playback", 1, 0, 'P'},
    { "rate", 1, 0, 'r' },
    { "mastervol", 1, 0, 'm' },
    { "config", 1, 0, 'c' },
#if defined(AUDIODRV_OSS) || defined(AUDIODRV_ALSA)
    { "device", 1, 0, 'd' }, /* treated the same as --wavout, keeping here for compat. */
#endif
    { "wavout", 1, 0, 'o' },
    { "tomidi", 1, 0, 'x' },
    { "convert", 1, 0, 'g' },
    { "frequency", 1, 0, 'f' },
    { "log_vol", 0, 0, 'l' },
    { "reverb", 0, 0, 'b' },
    { "test_midi", 0, 0, 't' },
    { "test_bank", 1, 0, 'k' },
    { "test_patch", 1, 0, 'p' },
    { "enhanced", 0, 0, 'e' },
    { "roundtempo", 0, 0, 'n' },
    { "skipsilentstart", 0, 0, 's' },
    { "textaslyric", 0, 0, 'a' },
    { "playfrom", 1, 0, 'i'},
    { "playto", 1, 0, 'j'},
    { NULL, 0, NULL, 0 }
};

static int get_default_output(void) {
    /* Return first available output */
    if (available_outputs[2])
        return 2;
    return 0;  /* No enabled outputs */
}

static void do_help(void) {
    printf("  -v    --version     Display version info and exit\n");
    printf("  -h    --help        Display this help and exit\n");
    printf("MIDI Options:\n");
    printf("  -n    --roundtempo  Round tempo to nearest whole number\n");
    printf("  -s    --skipsilentstart Skips any silence at the start of playback\n");
    printf("  -t    --test_midi   Listen to test MIDI\n");
    printf("Non-MIDI Options:\n");
    printf("  -x    --tomidi      Convert file to midi and save to file\n");
    printf("  -f F  --frequency=F Use frequency F Hz for playback (MUS)\n");
    printf("  -g    --convert     Convert XMI: 0 - No Conversion (default)\n");
    printf("                                   1 - MT32 to GM\n");
    printf("                                   2 - MT32 to GS\n");
    printf("  -P P  --playback=P  Set 'P' as playback output (default: %s).\n",
           available_outputs[get_default_output()]->name);
    printf("  -o W  --wavout=W    Save output to W in 16bit stereo format wav file\n");
    printf("                     (implies '-P wave' )\n");
#if defined(AUDIODRV_OSS)|| defined(AUDIODRV_ALSA)
    printf("  -d D  --device=D    For ALSA or OSS output: use device 'D' for audio\n");
    printf("                      output instead of the default\n");
#endif
    printf("Software Wavetable Options:\n");
    printf("  -l    --log_vol     Use log volume adjustments\n");
    printf("  -r N  --rate=N      Set sample rate to N samples per second (Hz)\n");
    printf("  -c P  --config=P    Point to your wildmidi.cfg config file name/path\n");
    printf("                      defaults to: %s\n", WILDMIDI_CFG);
    printf("  -m V  --mastervol=V Set the master volume (0..127), default is 100\n");
    printf("  -b    --reverb      Enable final output reverb engine\n\n");
}

static void do_available_outputs(void) {
    int i;

    printf("Available playback outputs for option -P:\n");
    for (i = 0 ; available_outputs[i] ; ++i) {
         printf("  %-20s%s\n",
                 available_outputs[i]->name, available_outputs[i]->description);
    }
}

static void do_version(void) {
    printf("\nWildMidi %s Open Source Midi Sequencer\n", PACKAGE_VERSION);
    printf("Copyright (C) WildMIDI Developers 2001-2016\n\n");
    printf("WildMidi comes with ABSOLUTELY NO WARRANTY\n");
    printf("This is free software, and you are welcome to redistribute it under\n");
    printf("the terms and conditions of the GNU General Public License version 3.\n");
    printf("For more information see COPYING\n\n");
    printf("Report bugs to %s\n", PACKAGE_BUGREPORT);
    printf("WildMIDI homepage is at %s\n\n", PACKAGE_URL);
}

static void do_syntax(void) {
    printf("Usage: wildmidi [options] filename.mid\n\n");
}

static char config_file[1024];

int main(int argc, char **argv) {
    char output[1024];
    struct _WM_Info *wm_info;
    int i, res;
    int playback_id;
    int option_index = 0;
    uint16_t mixer_options = 0;
    void *midi_ptr;
    uint8_t master_volume = 100;
    int8_t *output_buffer;
    uint32_t perc_play;
    uint32_t pro_mins;
    uint32_t pro_secs;
    uint32_t apr_mins;
    uint32_t apr_secs;
    char modes[5];
    uint8_t ch;
    int test_midi = 0;
    int test_count = 0;
    uint8_t *test_data;
    uint8_t test_bank = 0;
    uint8_t test_patch = 0;
    static char spinner[] = "|/-\\";
    static int spinpoint = 0;
    unsigned long int seek_to_sample;
    uint32_t samples = 0;
    int inpause = 0;
    char * ret_err = NULL;
    long libraryver;
    char * lyric = NULL;
    char *last_lyric = NULL;
    size_t last_lyric_length = 0;
    int8_t kareoke = 0;
#define MAX_LYRIC_CHAR 128
    char lyrics[MAX_LYRIC_CHAR + 1];
#define MAX_DISPLAY_LYRICS 29
    char display_lyrics[MAX_DISPLAY_LYRICS + 1];

    unsigned long int play_from = 0;
    unsigned long int play_to = 0;

    memset(lyrics,' ',MAX_LYRIC_CHAR);
    memset(display_lyrics,' ',MAX_DISPLAY_LYRICS);

    playback_id = get_default_output();
    config_file[0] = 0;
    output[0] = 0;
    midi_file[0] = 0;

    do_version();
    while (1) {
        i = getopt_long(argc, argv, "0vho:tx:g:P:f:lr:c:m:btak:p:ensi:j:", long_options,
                &option_index);
        if (i == -1)
            break;
        switch (i) {
        case 'v': /* Version */
            return (0);
        case 'h': /* help */
            do_syntax();
            do_help();
            do_available_outputs();
            return (0);
        case 'P': /* Playback */
            if (!*optarg) {
                fprintf(stderr, "Error: empty playback name.\n");
                return (1);
            } else {
                for (i = 0; available_outputs[i]; ++i) {
                    if (strcmp(available_outputs[i]->name, optarg) == 0) {
                        playback_id = i;
                        break;
                    }
                }
            }
            if (!available_outputs[i]) {
                fprintf(stderr, "Error: chosen playback %s is not available.\n", optarg);
                return (1);
            }
            break;
        case 'r': /* Sample Rate */
            res = atoi(optarg);
            if (res < 0 || res > 65535) {
                fprintf(stderr, "Error: bad rate %i.\n", res);
                return (1);
            }
            rate = (uint32_t) res;
            break;
        case 'b': /* Reverb */
            mixer_options |= WM_MO_REVERB;
            break;
        case 'm': /* Master Volume */
            master_volume = (uint8_t) atoi(optarg);
            break;
        case 'o': /* Wav Output    */
            playback_id = 1;
            /* FALL THROUGH */
        #if defined(AUDIODRV_OSS) || defined(AUDIODRV_ALSA)
        case 'd': /* Device Output */
        #endif
            if (!*optarg) {
                fprintf(stderr, "Error: empty file/device name.\n");
                return (1);
            }
            strncpy(output, optarg, sizeof(output) - 1);
            output[sizeof(output) - 1] = 0;
            break;
        case 'g': /* XMIDI Conversion */
            WildMidi_SetCvtOption(WM_CO_XMI_TYPE, (uint16_t) atoi(optarg));
            break;
        case 'f': /* MIDI-like Conversion */
            WildMidi_SetCvtOption(WM_CO_FREQUENCY, (uint16_t) atoi(optarg));
            break;
        case 'x': /* MIDI Output */
            if (!*optarg) {
                fprintf(stderr, "Error: empty midi name.\n");
                return (1);
            }
            strncpy(midi_file, optarg, sizeof(midi_file));
            midi_file[sizeof(midi_file) - 1] = 0;
            break;
        case 'c': /* Config File */
            if (!*optarg) {
                fprintf(stderr, "Error: empty config name.\n");
                return (1);
            }
            strncpy(config_file, optarg, sizeof(config_file));
            config_file[sizeof(config_file) - 1] = 0;
            break;
        case 'e': /* Enhanced Resampling */
            mixer_options |= WM_MO_ENHANCED_RESAMPLING;
            break;
        case 'l': /* log volume */
            mixer_options |= WM_MO_LOG_VOLUME;
            break;
        case 't': /* play test midis */
            test_midi = 1;
            break;
        case 'k': /* set test bank */
            test_bank = (uint8_t) atoi(optarg);
            break;
        case 'p': /* set test patch */
            test_patch = (uint8_t) atoi(optarg);
            break;
        case 'n': /* whole number tempo */
            mixer_options |= WM_MO_ROUNDTEMPO;
            break;
        case 'a':
            /* Some files have the lyrics in the text meta event.
             * This option reads lyrics from there instead.  */
            mixer_options |= WM_MO_TEXTASLYRIC;
            break;
        case 's': /* strip silence at start */
            mixer_options |= WM_MO_STRIPSILENCE;
            break;
        case '0': /* treat as type 2 midi when writing to file */
            mixer_options |= WM_MO_SAVEASTYPE0;
            break;
        case 'i':
            play_from = (unsigned long int)(atof(optarg) * (double)rate);
            break;
        case 'j':
            play_to = (unsigned long int)(atof(optarg) * (double)rate);
            break;
        default:
            do_syntax();
            return (1);
        }
    }

    if (optind >= argc && !test_midi) {
        fprintf(stderr, "ERROR: No midi file given\r\n");
        do_syntax();
        return (1);
    }

    if (test_midi) {
        if (midi_file[0] != '\0') {
            fprintf(stderr, "--test_midi and --convert cannot be used together.\n");
            return (1);
        }
    }

    /* check if we only need to convert a file to midi */
    if (midi_file[0] != '\0') {
        const char *real_file = FIND_LAST_DIRSEP(argv[optind]);
        uint32_t size;
        uint8_t *data;

        if (!real_file) real_file = argv[optind];
        else real_file++;

        printf("Converting %s\r\n", real_file);
        if (WildMidi_ConvertToMidi(argv[optind], &data, &size) < 0) {
            fprintf(stderr, "Conversion failed: %s.\r\n", WildMidi_GetError());
            WildMidi_ClearError();
            return (1);
        }

        printf("Writing %s: %u bytes.\r\n", midi_file, size);
        write_midi_output(data, size);
        free(data);
        return (0);
    }

    if (!config_file[0]) {
        strncpy(config_file, WILDMIDI_CFG, sizeof(config_file));
        config_file[sizeof(config_file) - 1] = 0;
    }

    printf("Initializing Sound System (%s)\n", available_outputs[playback_id]->name);

    if (available_outputs[playback_id]->open_out(output) == -1) {
        return (1);
    }

    libraryver = WildMidi_GetVersion();
    printf("Initializing libWildMidi %ld.%ld.%ld\n\n",
                        (libraryver>>16) & 255,
                        (libraryver>> 8) & 255,
                        (libraryver    ) & 255);
    if (WildMidi_Init(config_file, rate, mixer_options) == -1) {
        fprintf(stderr, "%s\r\n", WildMidi_GetError());
        WildMidi_ClearError();
        return (1);
    }

    printf(" +  Volume up        e  Better resampling    n  Next Midi\n");
    printf(" -  Volume down      l  Log volume           q  Quit\n");
    printf(" ,  1sec Seek Back   r  Reverb               .  1sec Seek Forward\n");
    printf(" m  save as midi     p  Pause On/Off\n\n");

    output_buffer = (int8_t *) malloc(16384);
    if (output_buffer == NULL) {
        fprintf(stderr, "Not enough memory, exiting\n");
        WildMidi_Shutdown();
        return (1);
    }

    wm_inittty();
#ifdef WILDMIDI_AMIGA
    amiga_sysinit();
#endif

    WildMidi_MasterVolume(master_volume);

    while (optind < argc || test_midi) {
        WildMidi_ClearError();
        if (!test_midi) {
            const char *real_file = FIND_LAST_DIRSEP(argv[optind]);

            if (!real_file) real_file = argv[optind];
            else real_file++;
            printf("\rPlaying %s ", real_file);

            midi_ptr = WildMidi_Open(argv[optind]);
            optind++;
            if (midi_ptr == NULL) {
                ret_err = WildMidi_GetError();
                printf(" Skipping: %s\r\n",ret_err);
                continue;
            }
        } else {
            if (test_count == midi_test_max) {
                break;
            }
            test_data = (uint8_t *) malloc(midi_test[test_count].size);
            memcpy(test_data, midi_test[test_count].data,
                    midi_test[test_count].size);
            test_data[25] = test_bank;
            test_data[28] = test_patch;
            midi_ptr = WildMidi_OpenBuffer(test_data, 633);
            test_count++;
            if (midi_ptr == NULL) {
                fprintf(stderr, "\rFailed loading test midi no. %i\r\n", test_count);
                continue;
            }
            printf("\rPlaying test midi no. %i ", test_count);
        }

        wm_info = WildMidi_GetInfo(midi_ptr);

        apr_mins = wm_info->approx_total_samples / (rate * 60);
        apr_secs = (wm_info->approx_total_samples % (rate * 60)) / rate;
        mixer_options = wm_info->mixer_options;
        modes[0] = (mixer_options & WM_MO_LOG_VOLUME)? 'l' : ' ';
        modes[1] = (mixer_options & WM_MO_REVERB)? 'r' : ' ';
        modes[2] = (mixer_options & WM_MO_ENHANCED_RESAMPLING)? 'e' : ' ';
        modes[3] = ' ';
        modes[4] = '\0';

        printf("\r\n[Approx %2um %2us Total]\r\n", apr_mins, apr_secs);
        fprintf(stderr, "\r");

        memset(lyrics,' ',MAX_LYRIC_CHAR);
        memset(display_lyrics,' ',MAX_DISPLAY_LYRICS);

        if (play_from != 0) {
            WildMidi_FastSeek(midi_ptr, &play_from);
            if (play_to < play_from) {
                /* Ignore --playto if set less than --playfrom */
                play_to = 0;
            }
        }

        while (1) {
            wm_getch (&ch);
            if (ch) {
                switch (ch) {
                case 'l':
                    WildMidi_SetOption(midi_ptr, WM_MO_LOG_VOLUME,
                                       ((mixer_options & WM_MO_LOG_VOLUME) ^ WM_MO_LOG_VOLUME));
                    mixer_options ^= WM_MO_LOG_VOLUME;
                    modes[0] = (mixer_options & WM_MO_LOG_VOLUME)? 'l' : ' ';
                    break;
                case 'r':
                    WildMidi_SetOption(midi_ptr, WM_MO_REVERB,
                                       ((mixer_options & WM_MO_REVERB) ^ WM_MO_REVERB));
                    mixer_options ^= WM_MO_REVERB;
                    modes[1] = (mixer_options & WM_MO_REVERB)? 'r' : ' ';
                    break;
                case 'e':
                    WildMidi_SetOption(midi_ptr, WM_MO_ENHANCED_RESAMPLING,
                                       ((mixer_options & WM_MO_ENHANCED_RESAMPLING) ^ WM_MO_ENHANCED_RESAMPLING));
                    mixer_options ^= WM_MO_ENHANCED_RESAMPLING;
                    modes[2] = (mixer_options & WM_MO_ENHANCED_RESAMPLING)? 'e' : ' ';
                    break;
                case 'a':
                    WildMidi_SetOption(midi_ptr, WM_MO_TEXTASLYRIC,
                                       ((mixer_options & WM_MO_TEXTASLYRIC) ^ WM_MO_TEXTASLYRIC));
                    mixer_options ^= WM_MO_TEXTASLYRIC;
                    break;
                case 'n':
                    goto NEXTMIDI;
                case 'p':
                    if (inpause) {
                        inpause = 0;
                        fprintf(stderr, "       \r");
                        available_outputs[playback_id]->resume_out();
                    } else {
                        inpause = 1;
                        fprintf(stderr, "Paused \r");
                        available_outputs[playback_id]->pause_out();
                        continue;
                    }
                    break;
                case 'q':
                    printf("\r\n");
                    if (inpause) goto end2;
                    goto end1;
                case '-':
                    if (master_volume > 0) {
                        master_volume--;
                        WildMidi_MasterVolume(master_volume);
                    }
                    break;
                case '+':
                    if (master_volume < 127) {
                        master_volume++;
                        WildMidi_MasterVolume(master_volume);
                    }
                    break;
                case ',': /* fast seek backwards */
                    if (wm_info->current_sample < rate) {
                        seek_to_sample = 0;
                    } else {
                        seek_to_sample = wm_info->current_sample - rate;
                    }
                    WildMidi_FastSeek(midi_ptr, &seek_to_sample);
                    break;
                case '.': /* fast seek forwards */
                    if ((wm_info->approx_total_samples
                            - wm_info->current_sample) < rate) {
                        seek_to_sample = wm_info->approx_total_samples;
                    } else {
                        seek_to_sample = wm_info->current_sample + rate;
                    }
                    WildMidi_FastSeek(midi_ptr, &seek_to_sample);
                    break;
                case '<':
                    WildMidi_SongSeek (midi_ptr, -1);
                    break;
                case '>':
                    WildMidi_SongSeek (midi_ptr, 1);
                    break;
                case '/':
                    WildMidi_SongSeek (midi_ptr, 0);
                    break;
                case 'm': /* save as midi */ {
                    int8_t *getmidibuffer = NULL;
                    uint32_t getmidisize = 0;
                    int32_t getmidiret = 0;

                    getmidiret = WildMidi_GetMidiOutput(midi_ptr, &getmidibuffer, &getmidisize);
                    if (getmidiret == -1) {
                        fprintf(stderr, "\r\n\nFAILED to convert events to midi\r\n");
                        ret_err = WildMidi_GetError();
                        fprintf(stderr, "%s\r\n",ret_err);
                        WildMidi_ClearError();
                    } else {
                        char *real_file = FIND_LAST_DIRSEP(argv[optind-1]);
                        if (!real_file) real_file = argv[optind-1];
                        else real_file++;
                        mk_midifile_name(real_file);
                        printf("\rWriting %s: %u bytes.\r\n", midi_file, getmidisize);
                        write_midi_output(getmidibuffer,getmidisize);
                        free(getmidibuffer);
                    }
                  } break;
                case 'k': /* Kareoke */
                          /* Enables/Disables the display of lyrics */
                    kareoke ^= 1;
                    break;
                default:
                    break;
                }
            }

            if (inpause) {
                wm_info = WildMidi_GetInfo(midi_ptr);
                perc_play = (wm_info->current_sample * 100)
                            / wm_info->approx_total_samples;
                pro_mins = wm_info->current_sample / (rate * 60);
                pro_secs = (wm_info->current_sample % (rate * 60)) / rate;
                fprintf(stderr,
                        "%s [%s] [%3i] [%2um %2us Processed] [%2u%%] P  \r",
                        display_lyrics, modes, (int)master_volume, pro_mins,
                        pro_secs, perc_play);
                msleep(5);
                continue;
            }

            if (play_to != 0) {
                if ((wm_info->current_sample + 4096) <= play_to) {
                    samples = 16384;
                } else {
                    samples = (play_to - wm_info->current_sample) << 2;
                    if (!samples) {
                        /* We are at or past where we wanted to play to */
                        break;
                    }
                }
            }
            else {
                samples = 16384;
            }
            res = WildMidi_GetOutput(midi_ptr, output_buffer, samples);

            if (res <= 0)
                break;

            wm_info = WildMidi_GetInfo(midi_ptr);
            lyric = WildMidi_GetLyric(midi_ptr);

            memmove(lyrics, &lyrics[1], MAX_LYRIC_CHAR - 1);
            lyrics[MAX_LYRIC_CHAR - 1] = ' ';

            if ((lyric != NULL) && (lyric != last_lyric) && (kareoke)) {
                last_lyric = lyric;
                if (last_lyric_length != 0) {
                    memcpy(lyrics, &lyrics[last_lyric_length], MAX_LYRIC_CHAR - last_lyric_length);
                }
                memcpy(&lyrics[MAX_DISPLAY_LYRICS], lyric, strlen(lyric));
                last_lyric_length = strlen(lyric);
            } else {
                if (last_lyric_length != 0) last_lyric_length--;
            }

            memcpy(display_lyrics,lyrics,MAX_DISPLAY_LYRICS);
            display_lyrics[MAX_DISPLAY_LYRICS] = '\0';

            perc_play = (wm_info->current_sample * 100)
                        / wm_info->approx_total_samples;
            pro_mins = wm_info->current_sample / (rate * 60);
            pro_secs = (wm_info->current_sample % (rate * 60)) / rate;
            fprintf(stderr,
                "%s [%s] [%3i] [%2um %2us Processed] [%2u%%] %c  \r",
                display_lyrics, modes, (int)master_volume, pro_mins,
                pro_secs, perc_play, spinner[spinpoint++ % 4]);

            if (available_outputs[playback_id]->send_out(output_buffer, res) < 0) {
            /* driver prints an error message already. */
                printf("\r");
                goto end2;
            }
        }
        NEXTMIDI: fprintf(stderr, "\r\n");
        if (WildMidi_Close(midi_ptr) == -1) {
            ret_err = WildMidi_GetError();
            fprintf(stderr, "OOPS: failed closing midi handle!\r\n%s\r\n",ret_err);
        }
        memset(output_buffer, 0, 16384);
        available_outputs[playback_id]->send_out(output_buffer, 16384);
    }
end1:
    memset(output_buffer, 0, 16384);
    available_outputs[playback_id]->send_out(output_buffer, 16384);
    msleep(5);
end2:
    available_outputs[playback_id]->close_out();
    free(output_buffer);
    if (WildMidi_Shutdown() == -1) {
        ret_err = WildMidi_GetError();
        fprintf(stderr, "OOPS: failure shutting down libWildMidi\r\n%s\r\n", ret_err);
        WildMidi_ClearError();
    }
    wm_resetty();

    printf("\r\n");
    return (0);
}
