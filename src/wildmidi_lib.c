/*
 * wildmidi_lib.c -- Midi Wavetable Processing library
 *
 * Copyright (C) WildMIDI Developers 2001-2016
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

#define _WILDMIDI_LIB_C

#include "config.h"

#include <stdint.h>
#include <errno.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "wm_error.h"
#include "file_io.h"
#include "lock.h"
#include "reverb.h"
#include "gus_pat.h"
#include "common.h"
#include "wildmidi_lib.h"
#include "filenames.h"
#include "internal_midi.h"
#include "f_hmi.h"
#include "f_hmp.h"
#include "f_midi.h"
#include "f_mus.h"
#include "f_xmidi.h"
#include "patches.h"
#include "sample.h"
#include "mus2mid.h"
#include "xmi2mid.h"

/*
 * =========================
 * Global Data and Data Structs
 * =========================
 */

static int WM_Initialized = 0;
uint16_t _WM_MixerOptions = 0;

uint16_t _WM_SampleRate;
int16_t _WM_MasterVolume;

/* when converting files to midi */
typedef struct _cvt_options {
    int lock;
    uint16_t xmi_convert_type;
    uint16_t frequency;
} _cvt_options;

static _cvt_options WM_ConvertOptions = {0, 0, 0};


float _WM_reverb_room_width = 16.875f;
float _WM_reverb_room_length = 22.5f;

float _WM_reverb_listen_posx = 8.4375f;
float _WM_reverb_listen_posy = 16.875f;

int _WM_fix_release = 0;
int _WM_auto_amp = 0;
int _WM_auto_amp_with_amp = 0;

struct _miditrack {
    uint32_t length;
    uint32_t ptr;
    uint32_t delta;
    uint8_t running_event;
    uint8_t EOT;
};

struct _mdi_patches {
    struct _patch *patch;
    struct _mdi_patch *next;
};

#define FPBITS 10
#define FPMASK ((1L<<FPBITS)-1L)


/* Gauss Interpolation code adapted from code supplied by Eric. A. Welsh */
static double newt_coeffs[58][58];  /* for start/end of samples */
#define MAX_GAUSS_ORDER 34          /* 34 is as high as we can go before errors crop up */
static double *gauss_table = NULL;  /* *gauss_table[1<<FPBITS] */
static int gauss_n = MAX_GAUSS_ORDER;
static int gauss_lock;

static void init_gauss(void) {
    /* init gauss table */
    int n = gauss_n;
    int m, i, k, n_half = (n >> 1);
    int j;
    int sign;
    double ck;
    double x, x_inc, xz;
    double z[35];
    double *gptr, *t;

    _WM_Lock(&gauss_lock);
    if (gauss_table) {
        _WM_Unlock(&gauss_lock);
        return;
    }

    newt_coeffs[0][0] = 1;
    for (i = 0; i <= n; i++) {
        newt_coeffs[i][0] = 1;
        newt_coeffs[i][i] = 1;

        if (i > 1) {
            newt_coeffs[i][0] = newt_coeffs[i - 1][0] / i;
            newt_coeffs[i][i] = newt_coeffs[i - 1][0] / i;
        }

        for (j = 1; j < i; j++) {
            newt_coeffs[i][j] = newt_coeffs[i - 1][j - 1]
                    + newt_coeffs[i - 1][j];
            if (i > 1)
                newt_coeffs[i][j] /= i;
        }
        z[i] = i / (4 * M_PI);
    }

    for (i = 0; i <= n; i++)
        for (j = 0, sign = (int) pow(-1, i); j <= i; j++, sign *= -1)
            newt_coeffs[i][j] *= sign;

    t = malloc((1<<FPBITS) * (n + 1) * sizeof(double));
    x_inc = 1.0 / (1<<FPBITS);
    for (m = 0, x = 0.0; m < (1<<FPBITS); m++, x += x_inc) {
        xz = (x + n_half) / (4 * M_PI);
        gptr = &t[m * (n + 1)];

        for (k = 0; k <= n; k++) {
            ck = 1.0;

            for (i = 0; i <= n; i++) {
                if (i == k)
                    continue;

                ck *= (sin(xz - z[i])) / (sin(z[k] - z[i]));
            }
            *gptr++ = ck;
        }
    }

    gauss_table = t;
    _WM_Unlock(&gauss_lock);
}

static void free_gauss(void) {
    _WM_Lock(&gauss_lock);
    free(gauss_table);
    gauss_table = NULL;
    _WM_Unlock(&gauss_lock);
}

struct _hndl {
    void * handle;
    struct _hndl *next;
    struct _hndl *prev;
};

static struct _hndl * first_handle = NULL;

#define MAX_AUTO_AMP 2.0

/*
 * =========================
 * Internal Functions
 * =========================
 */

void _cvt_reset_options (void) {
    _WM_Lock(&WM_ConvertOptions.lock);
    WM_ConvertOptions.xmi_convert_type = 0;
    WM_ConvertOptions.frequency = 0;
    _WM_Unlock(&WM_ConvertOptions.lock);
}

uint16_t _cvt_get_option (uint16_t tag) {
    uint16_t r = 0;
    _WM_Lock(&WM_ConvertOptions.lock);
    switch (tag) {
    case WM_CO_XMI_TYPE: r = WM_ConvertOptions.xmi_convert_type; break;
    case WM_CO_FREQUENCY: r = WM_ConvertOptions.frequency; break;
    }
    _WM_Unlock(&WM_ConvertOptions.lock);
    return r;
}

static void WM_InitPatches(void) {
    int i;
    for (i = 0; i < 128; i++) {
        _WM_patch[i] = NULL;
    }
}

static void WM_FreePatches(void) {
    int i;
    struct _patch * tmp_patch;
    struct _sample * tmp_sample;

    _WM_Lock(&_WM_patch_lock);
    for (i = 0; i < 128; i++) {
        while (_WM_patch[i]) {
            while (_WM_patch[i]->first_sample) {
                tmp_sample = _WM_patch[i]->first_sample->next;
                free(_WM_patch[i]->first_sample->data);
                free(_WM_patch[i]->first_sample);
                _WM_patch[i]->first_sample = tmp_sample;
            }
            free(_WM_patch[i]->filename);
            tmp_patch = _WM_patch[i]->next;
            free(_WM_patch[i]);
            _WM_patch[i] = tmp_patch;
        }
    }
    _WM_Unlock(&_WM_patch_lock);
}

/* wm_strdup -- adds extra space for appending up to 4 chars */
static char *wm_strdup (const char *str) {
    size_t l = strlen(str) + 5;
    char *d = (char *) malloc(l * sizeof(char));
    if (d) {
        strcpy(d, str);
        return (d);
    }
    return (NULL);
}

static inline int wm_isdigit(int c) {
    return (c >= '0' && c <= '9');
}
static inline int wm_isupper(int c) {
    return (c >= 'A' && c <= 'Z');
}
static inline int wm_tolower(int c) {
    return ((wm_isupper(c)) ? (c | ('a' - 'A')) : c);
}
#if 0 /* clang whines that these aren't used. */
static inline int wm_islower(int c) {
    return (c >= 'a' && c <= 'z');
}
static inline int wm_toupper(int c) {
    return ((wm_islower(c)) ? (c & ~('a' - 'A')) : c);
}
#endif

static int wm_strcasecmp(const char *s1, const char * s2) {
    const char * p1 = s1;
    const char * p2 = s2;
    char c1, c2;

    if (p1 == p2) return 0;
    do {
        c1 = wm_tolower (*p1++);
        c2 = wm_tolower (*p2++);
        if (c1 == '\0') break;
    } while (c1 == c2);
    return (int)(c1 - c2);
}

static int wm_strncasecmp(const char *s1, const char *s2, size_t n) {
    const char * p1 = s1;
    const char * p2 = s2;
    char c1, c2;

    if (p1 == p2 || n == 0) return 0;
    do {
        c1 = wm_tolower (*p1++);
        c2 = wm_tolower (*p2++);
        if (c1 == '\0' || c1 != c2) break;
    } while (--n > 0);
    return (int)(c1 - c2);
}

#define TOKEN_CNT_INC 8
static char** WM_LC_Tokenize_Line(char *line_data) {
    int line_length = (int) strlen(line_data);
    int token_data_length = 0;
    int line_ofs = 0;
    int token_start = 0;
    char **token_data = NULL;
    int token_count = 0;

    if (!line_length) return (NULL);

    do {
        /* ignore everything after #  */
        if (line_data[line_ofs] == '#') {
            break;
        }

        if ((line_data[line_ofs] == ' ') || (line_data[line_ofs] == '\t')) {
            /* whitespace means we aren't in a token */
            if (token_start) {
                token_start = 0;
                line_data[line_ofs] = '\0';
            }
        } else {
            if (!token_start) {
                /* the start of a token in the line */
                token_start = 1;
                if (token_count >= token_data_length) {
                    token_data_length += TOKEN_CNT_INC;
                    token_data = realloc(token_data, token_data_length * sizeof(char *));
                    if (token_data == NULL) {
                        _WM_GLOBAL_ERROR(__FUNCTION__, __LINE__, WM_ERR_MEM,"to parse config", errno);
                        return (NULL);
                    }
                }

                token_data[token_count] = &line_data[line_ofs];
                token_count++;
            }
        }
        line_ofs++;
    } while (line_ofs != line_length);

    /* if we have found some tokens then add a null token to the end */
    if (token_count) {
        if (token_count >= token_data_length) {
            token_data = realloc(token_data,
                ((token_count + 1) * sizeof(char *)));
        }
        token_data[token_count] = NULL;
    }

    return (token_data);
}

static int load_config(const char *config_file, const char *conf_dir) {
    uint32_t config_size = 0;
    char *config_buffer = NULL;
    const char *dir_end = NULL;
    char *config_dir = NULL;
    uint32_t config_ptr = 0;
    uint32_t line_start_ptr = 0;
    uint16_t patchid = 0;
    struct _patch * tmp_patch;
    char **line_tokens = NULL;
    int token_count = 0;

    config_buffer = (char *) _WM_BufferFile(config_file, &config_size);
    if (!config_buffer) {
        WM_FreePatches();
        return (-1);
    }

    if (conf_dir) {
        if (!(config_dir = wm_strdup(conf_dir))) {
            _WM_GLOBAL_ERROR(__FUNCTION__, __LINE__, WM_ERR_MEM, config_file, errno);
            WM_FreePatches();
            free(config_buffer);
            return (-1);
        }
    } else {
        dir_end = FIND_LAST_DIRSEP(config_file);
        if (dir_end) {
            config_dir = malloc((dir_end - config_file + 2));
            if (config_dir == NULL) {
                _WM_GLOBAL_ERROR(__FUNCTION__, __LINE__, WM_ERR_MEM, config_file, errno);
                WM_FreePatches();
                free(config_buffer);
                return (-1);
            }
            strncpy(config_dir, config_file, (dir_end - config_file + 1));
            config_dir[dir_end - config_file + 1] = '\0';
        }
    }

    config_ptr = 0;
    line_start_ptr = 0;

    /* handle files without a newline at the end: this relies on
     * _WM_BufferFile() allocating the buffer with one extra byte */
    config_buffer[config_size] = '\n';

    while (config_ptr <= config_size) {
        if (config_buffer[config_ptr] == '\r' ||
            config_buffer[config_ptr] == '\n')
        {
            config_buffer[config_ptr] = '\0';

            if (config_ptr != line_start_ptr) {
                _WM_Global_ErrorI = 0; /* because WM_LC_Tokenize_Line() can legitimately return NULL */
                line_tokens = WM_LC_Tokenize_Line(&config_buffer[line_start_ptr]);
                if (line_tokens) {
                    if (wm_strcasecmp(line_tokens[0], "dir") == 0) {
                        free(config_dir);
                        if (!line_tokens[1]) {
                            _WM_GLOBAL_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID_ARG, "(missing name in dir line)", 0);
                            WM_FreePatches();
                            free(line_tokens);
                            free(config_buffer);
                            return (-1);
                        } else if (!(config_dir = wm_strdup(line_tokens[1]))) {
                            _WM_GLOBAL_ERROR(__FUNCTION__, __LINE__, WM_ERR_MEM, config_file, errno);
                            WM_FreePatches();
                            free(line_tokens);
                            free(config_buffer);
                            return (-1);
                        }
                        if (!IS_DIR_SEPARATOR(config_dir[strlen(config_dir) - 1])) {
                            config_dir[strlen(config_dir) + 1] = '\0';
                            config_dir[strlen(config_dir)] = DIR_SEPARATOR_CHAR;
                        }
                    } else if (wm_strcasecmp(line_tokens[0], "source") == 0) {
                        char *new_config = NULL;
                        if (!line_tokens[1]) {
                            _WM_GLOBAL_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID_ARG, "(missing name in source line)", 0);
                            WM_FreePatches();
                            free(line_tokens);
                            free(config_buffer);
                            return (-1);
                        } else if (!IS_ABSOLUTE_PATH(line_tokens[1]) && config_dir) {
                            new_config = malloc(strlen(config_dir) + strlen(line_tokens[1]) + 1);
                            if (new_config == NULL) {
                                _WM_GLOBAL_ERROR(__FUNCTION__, __LINE__, WM_ERR_MEM, config_file, errno);
                                WM_FreePatches();
                                free(config_dir);
                                free(line_tokens);
                                free(config_buffer);
                                return (-1);
                            }
                            strcpy(new_config, config_dir);
                            strcpy(&new_config[strlen(config_dir)], line_tokens[1]);
                        } else {
                            if (!(new_config = wm_strdup(line_tokens[1]))) {
                                _WM_GLOBAL_ERROR(__FUNCTION__, __LINE__, WM_ERR_MEM, config_file, errno);
                                WM_FreePatches();
                                free(line_tokens);
                                free(config_buffer);
                                return (-1);
                            }
                        }
                        if (load_config(new_config, config_dir) == -1) {
                            free(new_config);
                            free(line_tokens);
                            free(config_buffer);
                            free(config_dir);
                            return (-1);
                        }
                        free(new_config);
                    } else if (wm_strcasecmp(line_tokens[0], "bank") == 0) {
                        if (!line_tokens[1] || !wm_isdigit(line_tokens[1][0])) {
                            _WM_GLOBAL_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID_ARG, "(syntax error in bank line)", 0);
                            WM_FreePatches();
                            free(config_dir);
                            free(line_tokens);
                            free(config_buffer);
                            return (-1);
                        }
                        patchid = (atoi(line_tokens[1]) & 0xFF) << 8;
                    } else if (wm_strcasecmp(line_tokens[0], "drumset") == 0) {
                        if (!line_tokens[1] || !wm_isdigit(line_tokens[1][0])) {
                            _WM_GLOBAL_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID_ARG, "(syntax error in drumset line)", 0);
                            WM_FreePatches();
                            free(config_dir);
                            free(line_tokens);
                            free(config_buffer);
                            return (-1);
                        }
                        patchid = ((atoi(line_tokens[1]) & 0xFF) << 8) | 0x80;
                    } else if (wm_strcasecmp(line_tokens[0], "reverb_room_width") == 0) {
                        if (!line_tokens[1] || !wm_isdigit(line_tokens[1][0])) {
                            _WM_GLOBAL_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID_ARG, "(syntax error in reverb_room_width line)", 0);
                            WM_FreePatches();
                            free(config_dir);
                            free(line_tokens);
                            free(config_buffer);
                            return (-1);
                        }
                        _WM_reverb_room_width = (float) atof(line_tokens[1]);
                        if (_WM_reverb_room_width < 1.0f) {
                            _WM_DEBUG_MSG("%s: reverb_room_width < 1 meter, setting to minimum of 1 meter", config_file);
                            _WM_reverb_room_width = 1.0f;
                        } else if (_WM_reverb_room_width > 100.0f) {
                            _WM_DEBUG_MSG("%s: reverb_room_width > 100 meters, setting to maximum of 100 meters", config_file);
                            _WM_reverb_room_width = 100.0f;
                        }
                    } else if (wm_strcasecmp(line_tokens[0], "reverb_room_length") == 0) {
                        if (!line_tokens[1] || !wm_isdigit(line_tokens[1][0])) {
                            _WM_GLOBAL_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID_ARG, "(syntax error in reverb_room_length line)", 0);
                            WM_FreePatches();
                            free(config_dir);
                            free(line_tokens);
                            free(config_buffer);
                            return (-1);
                        }
                        _WM_reverb_room_length = (float) atof(line_tokens[1]);
                        if (_WM_reverb_room_length < 1.0f) {
                            _WM_DEBUG_MSG("%s: reverb_room_length < 1 meter, setting to minimum of 1 meter", config_file);
                            _WM_reverb_room_length = 1.0f;
                        } else if (_WM_reverb_room_length > 100.0f) {
                            _WM_DEBUG_MSG("%s: reverb_room_length > 100 meters, setting to maximum of 100 meters", config_file);
                            _WM_reverb_room_length = 100.0f;
                        }
                    } else if (wm_strcasecmp(line_tokens[0], "reverb_listener_posx") == 0) {
                        if (!line_tokens[1] || !wm_isdigit(line_tokens[1][0])) {
                            _WM_GLOBAL_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID_ARG, "(syntax error in reverb_listen_posx line)", 0);
                            WM_FreePatches();
                            free(config_dir);
                            free(line_tokens);
                            free(config_buffer);
                            return (-1);
                        }
                        _WM_reverb_listen_posx = (float) atof(line_tokens[1]);
                        if ((_WM_reverb_listen_posx > _WM_reverb_room_width)
                                || (_WM_reverb_listen_posx < 0.0f)) {
                            _WM_DEBUG_MSG("%s: reverb_listen_posx set outside of room", config_file);
                            _WM_reverb_listen_posx = _WM_reverb_room_width / 2.0f;
                        }
                    } else if (wm_strcasecmp(line_tokens[0],
                            "reverb_listener_posy") == 0) {
                        if (!line_tokens[1] || !wm_isdigit(line_tokens[1][0])) {
                            _WM_GLOBAL_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID_ARG, "(syntax error in reverb_listen_posy line)", 0);
                            WM_FreePatches();
                            free(config_dir);
                            free(line_tokens);
                            free(config_buffer);
                            return (-1);
                        }
                        _WM_reverb_listen_posy = (float) atof(line_tokens[1]);
                        if ((_WM_reverb_listen_posy > _WM_reverb_room_width)
                                || (_WM_reverb_listen_posy < 0.0f)) {
                            _WM_DEBUG_MSG("%s: reverb_listen_posy set outside of room", config_file);
                            _WM_reverb_listen_posy = _WM_reverb_room_length * 0.75f;
                        }
                    } else if (wm_strcasecmp(line_tokens[0], "guspat_editor_author_cant_read_so_fix_release_time_for_me") == 0) {
                        _WM_fix_release = 1;
                    } else if (wm_strcasecmp(line_tokens[0], "auto_amp") == 0) {
                        _WM_auto_amp = 1;
                    } else if (wm_strcasecmp(line_tokens[0], "auto_amp_with_amp") == 0) {
                        _WM_auto_amp = 1;
                        _WM_auto_amp_with_amp = 1;
                    } else if (wm_isdigit(line_tokens[0][0])) {
                        patchid = (patchid & 0xFF80)
                                | (atoi(line_tokens[0]) & 0x7F);
                        if (_WM_patch[(patchid & 0x7F)] == NULL) {
                            _WM_patch[(patchid & 0x7F)] = malloc(sizeof(struct _patch));
                            if (_WM_patch[(patchid & 0x7F)] == NULL) {
                                _WM_GLOBAL_ERROR(__FUNCTION__, __LINE__, WM_ERR_MEM, config_file, errno);
                                WM_FreePatches();
                                free(config_dir);
                                free(line_tokens);
                                free(config_buffer);
                                return (-1);
                            }
                            tmp_patch = _WM_patch[(patchid & 0x7F)];
                            tmp_patch->patchid = patchid;
                            tmp_patch->filename = NULL;
                            tmp_patch->amp = 1024;
                            tmp_patch->note = 0;
                            tmp_patch->next = NULL;
                            tmp_patch->first_sample = NULL;
                            tmp_patch->loaded = 0;
                            tmp_patch->inuse_count = 0;
                        } else {
                            tmp_patch = _WM_patch[(patchid & 0x7F)];
                            if (tmp_patch->patchid == patchid) {
                                free(tmp_patch->filename);
                                tmp_patch->filename = NULL;
                                tmp_patch->amp = 1024;
                                tmp_patch->note = 0;
                            } else {
                                if (tmp_patch->next) {
                                    while (tmp_patch->next) {
                                        if (tmp_patch->next->patchid == patchid)
                                            break;
                                        tmp_patch = tmp_patch->next;
                                    }
                                    if (tmp_patch->next == NULL) {
                                        if ((tmp_patch->next = malloc(sizeof(struct _patch))) == NULL) {
                                            _WM_GLOBAL_ERROR(__FUNCTION__, __LINE__, WM_ERR_MEM, config_file, 0);
                                            WM_FreePatches();
                                            free(config_dir);
                                            free(line_tokens);
                                            free(config_buffer);
                                            return (-1);
                                        }
                                        tmp_patch = tmp_patch->next;
                                        tmp_patch->patchid = patchid;
                                        tmp_patch->filename = NULL;
                                        tmp_patch->amp = 1024;
                                        tmp_patch->note = 0;
                                        tmp_patch->next = NULL;
                                        tmp_patch->first_sample = NULL;
                                        tmp_patch->loaded = 0;
                                        tmp_patch->inuse_count = 0;
                                    } else {
                                        tmp_patch = tmp_patch->next;
                                        free(tmp_patch->filename);
                                        tmp_patch->filename = NULL;
                                        tmp_patch->amp = 1024;
                                        tmp_patch->note = 0;
                                    }
                                } else {
                                    tmp_patch->next = malloc(
                                            sizeof(struct _patch));
                                    if (tmp_patch->next == NULL) {
                                        _WM_GLOBAL_ERROR(__FUNCTION__, __LINE__, WM_ERR_MEM, config_file, errno);
                                        WM_FreePatches();
                                        free(config_dir);
                                        free(line_tokens);
                                        free(config_buffer);
                                        return (-1);
                                    }
                                    tmp_patch = tmp_patch->next;
                                    tmp_patch->patchid = patchid;
                                    tmp_patch->filename = NULL;
                                    tmp_patch->amp = 1024;
                                    tmp_patch->note = 0;
                                    tmp_patch->next = NULL;
                                    tmp_patch->first_sample = NULL;
                                    tmp_patch->loaded = 0;
                                    tmp_patch->inuse_count = 0;
                                }
                            }
                        }
                        if (!line_tokens[1]) {
                            _WM_GLOBAL_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID_ARG, "(missing name in patch line)", 0);
                            WM_FreePatches();
                            free(config_dir);
                            free(line_tokens);
                            free(config_buffer);
                            return (-1);
                        } else if (!IS_ABSOLUTE_PATH(line_tokens[1]) && config_dir) {
                            tmp_patch->filename = malloc(strlen(config_dir) + strlen(line_tokens[1]) + 5);
                            if (tmp_patch->filename == NULL) {
                                _WM_GLOBAL_ERROR(__FUNCTION__, __LINE__, WM_ERR_MEM, config_file, 0);
                                WM_FreePatches();
                                free(config_dir);
                                free(line_tokens);
                                free(config_buffer);
                                return (-1);
                            }
                            strcpy(tmp_patch->filename, config_dir);
                            strcat(tmp_patch->filename, line_tokens[1]);
                        } else {
                            if (!(tmp_patch->filename = wm_strdup(line_tokens[1]))) {
                                _WM_GLOBAL_ERROR(__FUNCTION__, __LINE__, WM_ERR_MEM, config_file, 0);
                                WM_FreePatches();
                                free(config_dir);
                                free(line_tokens);
                                free(config_buffer);
                                return (-1);
                            }
                        }
                        if (wm_strncasecmp(&tmp_patch->filename[strlen(tmp_patch->filename) - 4], ".pat", 4) != 0) {
                            strcat(tmp_patch->filename, ".pat");
                        }
                        tmp_patch->env[0].set = 0x00;
                        tmp_patch->env[1].set = 0x00;
                        tmp_patch->env[2].set = 0x00;
                        tmp_patch->env[3].set = 0x00;
                        tmp_patch->env[4].set = 0x00;
                        tmp_patch->env[5].set = 0x00;
                        tmp_patch->keep = 0;
                        tmp_patch->remove = 0;

                        token_count = 0;
                        while (line_tokens[token_count]) {
                            if (wm_strncasecmp(line_tokens[token_count], "amp=", 4) == 0) {
                                if (!wm_isdigit(line_tokens[token_count][4])) {
                                    _WM_DEBUG_MSG("%s: syntax error in patch line for %s", config_file, "amp=");
                                } else {
                                    tmp_patch->amp = (atoi(&line_tokens[token_count][4]) << 10) / 100;
                                }
                            } else if (wm_strncasecmp(line_tokens[token_count], "note=", 5) == 0) {
                                if (!wm_isdigit(line_tokens[token_count][5])) {
                                    _WM_DEBUG_MSG("%s: syntax error in patch line for %s", config_file, "note=");
                                } else {
                                    tmp_patch->note = atoi(&line_tokens[token_count][5]);
                                }
                            } else if (wm_strncasecmp(line_tokens[token_count], "env_time", 8) == 0) {
                                if ((!wm_isdigit(line_tokens[token_count][8])) ||
                                    (!wm_isdigit(line_tokens[token_count][10])) ||
                                    (line_tokens[token_count][9] != '=')) {
                                    _WM_DEBUG_MSG("%s: syntax error in patch line for %s", config_file, "env_time");
                                } else {
                                    uint32_t env_no = atoi(&line_tokens[token_count][8]);
                                    if (env_no > 5) {
                                        _WM_DEBUG_MSG("%s: syntax error in patch line for %s", config_file, "env_time");
                                    } else {
                                        tmp_patch->env[env_no].time = (float) atof(&line_tokens[token_count][10]);
                                        if ((tmp_patch->env[env_no].time > 45000.0f) ||
                                            (tmp_patch->env[env_no].time < 1.47f)) {
                                            _WM_DEBUG_MSG("%s: range error in patch line %s", config_file, "env_time");
                                            tmp_patch->env[env_no].set &= 0xFE;
                                        } else {
                                            tmp_patch->env[env_no].set |= 0x01;
                                        }
                                    }
                                }
                            } else if (wm_strncasecmp(line_tokens[token_count], "env_level", 9) == 0) {
                                if ((!wm_isdigit(line_tokens[token_count][9])) ||
                                    (!wm_isdigit(line_tokens[token_count][11])) ||
                                    (line_tokens[token_count][10] != '=')) {
                                    _WM_DEBUG_MSG("%s: syntax error in patch line for %s", config_file, "env_level");
                                } else {
                                    uint32_t env_no = atoi(&line_tokens[token_count][9]);
                                    if (env_no > 5) {
                                        _WM_DEBUG_MSG("%s: syntax error in patch line for %s", config_file, "env_level");
                                    } else {
                                        tmp_patch->env[env_no].level = (float) atof(&line_tokens[token_count][11]);
                                        if ((tmp_patch->env[env_no].level > 1.0f) ||
                                            (tmp_patch->env[env_no].level < 0.0f)) {
                                            _WM_DEBUG_MSG("%s: range error in patch line for %s", config_file, "env_level");
                                            tmp_patch->env[env_no].set &= 0xFD;
                                        } else {
                                            tmp_patch->env[env_no].set |= 0x02;
                                        }
                                    }
                                }
                            } else if (wm_strcasecmp(line_tokens[token_count], "keep=loop") == 0) {
                                tmp_patch->keep |= SAMPLE_LOOP;
                            } else if (wm_strcasecmp(line_tokens[token_count], "keep=env") == 0) {
                                tmp_patch->keep |= SAMPLE_ENVELOPE;
                            } else if (wm_strcasecmp(line_tokens[token_count], "remove=sustain") == 0) {
                                tmp_patch->remove |= SAMPLE_SUSTAIN;
                            } else if (wm_strcasecmp(line_tokens[token_count], "remove=clamped") == 0) {
                                tmp_patch->remove |= SAMPLE_CLAMPED;
                            }
                            token_count++;
                        }
                    }
                }
                else if (_WM_Global_ErrorI) { /* malloc() failure in WM_LC_Tokenize_Line() */
                    WM_FreePatches();
                    free(line_tokens);
                    free(config_buffer);
                    return (-1);
                }
                /* free up tokens */
                free(line_tokens);
            }
            line_start_ptr = config_ptr + 1;
        }
        config_ptr++;
    }

    free(config_buffer);
    free(config_dir);

    return (0);
}

static int WM_LoadConfig(const char *config_file) {
    return load_config(config_file, NULL);
}

static int add_handle(void * handle) {
    struct _hndl *tmp_handle = NULL;

    if (first_handle == NULL) {
        first_handle = malloc(sizeof(struct _hndl));
        if (first_handle == NULL) {
            _WM_GLOBAL_ERROR(__FUNCTION__, __LINE__, WM_ERR_MEM, " to get ram", errno);
            return (-1);
        }
        first_handle->handle = handle;
        first_handle->prev = NULL;
        first_handle->next = NULL;
    } else {
        tmp_handle = first_handle;
        if (tmp_handle->next) {
            while (tmp_handle->next)
                tmp_handle = tmp_handle->next;
        }
        tmp_handle->next = malloc(sizeof(struct _hndl));
        if (tmp_handle->next == NULL) {
            _WM_GLOBAL_ERROR(__FUNCTION__, __LINE__, WM_ERR_MEM, " to get ram", errno);
            return (-1);
        }
        tmp_handle->next->prev = tmp_handle;
        tmp_handle = tmp_handle->next;
        tmp_handle->next = NULL;
        tmp_handle->handle = handle;
    }
    return (0);
}

//#define DEBUG_RESAMPLE

#ifdef DEBUG_RESAMPLE
#define RESAMPLE_DEBUGI(dx,dy) fprintf(stderr,"\r%s, %i\n",dx,dy)
#define RESAMPLE_DEBUGS(dx) fprintf(stderr,"\r%s\n",dx)

#else
#define RESAMPLE_DEBUGI(dx,dy)
#define RESAMPLE_DEBUGS(dx)

#endif


static int WM_GetOutput_Linear(midi * handle, int8_t *buffer, uint32_t size) {
    uint32_t buffer_used = 0;
    uint32_t i;
    struct _mdi *mdi = (struct _mdi *) handle;
    uint32_t real_samples_to_mix = 0;
    uint32_t data_pos;
    int32_t premix, left_mix, right_mix;
//  int32_t vol_mul;
    struct _note *note_data = NULL;
    uint32_t count;
    struct _event *event = mdi->current_event;
    int32_t *tmp_buffer;
    int32_t *out_buffer;

    _WM_Lock(&mdi->lock);

    buffer_used = 0;
    memset(buffer, 0, size);

    if ( (size / 2) > mdi->mix_buffer_size) {
        if ( (size / 2) <= ( mdi->mix_buffer_size * 2 )) {
            mdi->mix_buffer_size += MEM_CHUNK;
        } else {
            mdi->mix_buffer_size = size / 2;
        }
        mdi->mix_buffer = realloc(mdi->mix_buffer, mdi->mix_buffer_size * sizeof(int32_t));
    }

    tmp_buffer = mdi->mix_buffer;

    memset(tmp_buffer, 0, ((size / 2) * sizeof(int32_t)));
    out_buffer = tmp_buffer;

    do {
        if (__builtin_expect((!mdi->samples_to_mix), 0)) {
            while ((!mdi->samples_to_mix) && (event->do_event)) {
                event->do_event(mdi, &event->event_data);
                if ((mdi->extra_info.mixer_options & WM_MO_LOOP) && (event[0].do_event == *_WM_do_meta_endoftrack)) {
                    _WM_ResetToStart(mdi);
                    event = mdi->current_event;
                } else {
                    mdi->samples_to_mix = event->samples_to_next;
                    event++;
                    mdi->current_event = event;
                }
            }

            if (__builtin_expect((!mdi->samples_to_mix), 0)) {
                if (mdi->extra_info.current_sample >= mdi->extra_info.approx_total_samples) {
                    break;
                } else if ((mdi->extra_info.approx_total_samples
                             - mdi->extra_info.current_sample) > (size >> 2)) {
                    mdi->samples_to_mix = size >> 2;
                } else {
                    mdi->samples_to_mix = mdi->extra_info.approx_total_samples
                                           - mdi->extra_info.current_sample;
                }
            }
        }
        if (__builtin_expect((mdi->samples_to_mix > (size >> 2)), 1)) {
            real_samples_to_mix = size >> 2;
        } else {
            real_samples_to_mix = mdi->samples_to_mix;
            if (real_samples_to_mix == 0) {
                continue;
            }
        }

        /* do mixing here */
        count = real_samples_to_mix;

        do {
            note_data = mdi->note;
            left_mix = right_mix = 0;
            RESAMPLE_DEBUGI("SAMPLES_TO_MIX",count);
            if (__builtin_expect((note_data != NULL), 1)) {
                RESAMPLE_DEBUGS("Processing Notes");
                while (note_data) {
                    /*
                     * ===================
                     * resample the sample
                     * ===================
                     */
                    data_pos = note_data->sample_pos >> FPBITS;
                    premix = ((note_data->sample->data[data_pos] + (((note_data->sample->data[data_pos + 1] - note_data->sample->data[data_pos]) * (int32_t)(note_data->sample_pos & FPMASK)) / 1024)) * (note_data->env_level >> 12)) / 1024;

                    left_mix += (premix * (int32_t)note_data->left_mix_volume) / 1024;
                    right_mix += (premix * (int32_t)note_data->right_mix_volume) / 1024;

                    /*
                     * ========================
                     * sample position checking
                     * ========================
                     */
#ifdef DEBUG_RESAMPLE
                    fprintf(stderr,"\r\n%d -> INC %i, ENV %i, LEVEL %i, TARGET %d, RATE %i, SAMPLE POS %i, SAMPLE LENGTH %i, PREMIX %i (%i:%i)",
                            (uint32_t)note_data,
                            note_data->env_inc,
                            note_data->env, note_data->env_level,
                            note_data->sample->env_target[note_data->env],
                            note_data->sample->env_rate[note_data->env],
                            note_data->sample_pos,
                            note_data->sample->data_length,
                            premix, left_mix, right_mix);
                    if (note_data->modes & SAMPLE_LOOP)
                        fprintf(stderr,", LOOP %i + %i",
                                note_data->sample->loop_start,
                                note_data->sample->loop_size);
                    fprintf(stderr,"\r\n");
#endif

                    note_data->sample_pos += note_data->sample_inc;

                    if (__builtin_expect((note_data->modes & SAMPLE_LOOP), 1)) {
                        if (__builtin_expect(
                                             (note_data->sample_pos > note_data->sample->loop_end),
                                             0)) {
                            note_data->sample_pos = note_data->sample->loop_start
                                + ((note_data->sample_pos
                                    - note_data->sample->loop_start)
                                % note_data->sample->loop_size);
                        }

                    } else if (__builtin_expect(
                                                  (note_data->sample_pos
                                                   >= note_data->sample->data_length),
                                                  0)) {
                        goto _END_THIS_NOTE;
                    }

                    if (__builtin_expect((note_data->env_inc == 0), 0)) {
                        note_data = note_data->next;
                        RESAMPLE_DEBUGS("Next Note: 0 env_inc");
                        continue;
                    }

                    note_data->env_level += note_data->env_inc;

                    if (note_data->env_inc < 0) {
                        if (__builtin_expect((note_data->env_level
                            > note_data->sample->env_target[note_data->env]), 0)) {
                            note_data = note_data->next;
                            RESAMPLE_DEBUGS("Next Note: env_lvl > env_target");
                            continue;
                        }
                    } else if (note_data->env_inc > 0) {
                        if (__builtin_expect((note_data->env_level
                            < note_data->sample->env_target[note_data->env]), 0)) {
                            note_data = note_data->next;
                            RESAMPLE_DEBUGS("Next Note: env_lvl < env_target");
                            continue;
                        }
                    }

                    // Yes could have a condition here but
                    // it would crete another bottleneck
                    note_data->env_level =
                            note_data->sample->env_target[note_data->env];
                    switch (note_data->env) {
                    case 0:
                        if (!(note_data->modes & SAMPLE_ENVELOPE)) {
                            note_data->env_inc = 0;
                            note_data = note_data->next;
                            RESAMPLE_DEBUGS("Next Note: No Envelope");
                            continue;
                        }
                        break;
                    case 2:
                        if (note_data->modes & SAMPLE_SUSTAIN /*|| note_data->hold*/) {
                            note_data->env_inc = 0;
                            note_data = note_data->next;
                            RESAMPLE_DEBUGS("Next Note: SAMPLE_SUSTAIN");
                            continue;
                        } else if (note_data->modes & SAMPLE_CLAMPED) {
                            note_data->env = 5;
                            if (note_data->env_level
                                    > note_data->sample->env_target[5]) {
                                note_data->env_inc =
                                        -note_data->sample->env_rate[5];
                            } else {
                                note_data->env_inc =
                                        note_data->sample->env_rate[5];
                            }
                            continue;
                        }
                        break;
                    case 5:
                        if (__builtin_expect((note_data->env_level == 0), 1)) {
                            goto _END_THIS_NOTE;
                        }
                        /* sample release */
                        if (note_data->modes & SAMPLE_LOOP)
                            note_data->modes ^= SAMPLE_LOOP;
                        note_data->env_inc = 0;
                        note_data = note_data->next;
                        RESAMPLE_DEBUGS("Next Note: Sample Release");

                        continue;
                    case 6:
                        _END_THIS_NOTE:
                        if (__builtin_expect((note_data->replay != NULL), 1)) {
                            note_data->active = 0;
                            {
                                struct _note *prev_note = NULL;
                                struct _note *nte_array = mdi->note;

                                if (nte_array != note_data) {
                                    do {
                                        prev_note = nte_array;
                                        nte_array = nte_array->next;
                                    } while (nte_array != note_data);
                                }
                                if (prev_note) {
                                    prev_note->next = note_data->replay;
                                } else {
                                    mdi->note = note_data->replay;
                                }
                                note_data->replay->next = note_data->next;
                                note_data = note_data->replay;
                                note_data->active = 1;
                            }
                        } else {
                            note_data->active = 0;
                            {
                                struct _note *prev_note = NULL;
                                struct _note *nte_array = mdi->note;

                                if (nte_array != note_data) {
                                    do {
                                        prev_note = nte_array;
                                        nte_array = nte_array->next;
                                    } while ((nte_array != note_data)
                                            && (nte_array));
                                }
                                if (prev_note) {
                                    prev_note->next = note_data->next;
                                } else {
                                    mdi->note = note_data->next;
                                }
                                note_data = note_data->next;
                            }
                        }
                        RESAMPLE_DEBUGS("Next Note: Killed Off Note");
                        continue;
                    }
                    note_data->env++;

                    if (note_data->is_off == 1) {
                        _WM_do_note_off_extra(note_data);
                    } else {

                        if (note_data->env_level
                            >= note_data->sample->env_target[note_data->env]) {
                            note_data->env_inc =
                                -note_data->sample->env_rate[note_data->env];
                        } else {
                            note_data->env_inc =
                                note_data->sample->env_rate[note_data->env];
                        }
                    }
                    note_data = note_data->next;
#ifdef DEBUG_RESAMPLE
                    if (note_data != NULL)
                        RESAMPLE_DEBUGI("Next Note: Next ENV ", note_data->env);
                    else
                        RESAMPLE_DEBUGS("Next Note: Next ENV");
#endif
                    continue;
                }
            }
            *tmp_buffer++ = left_mix;
            *tmp_buffer++ = right_mix;
        } while (--count);

        buffer_used += real_samples_to_mix * 4;
        size -= (real_samples_to_mix << 2);
        mdi->extra_info.current_sample += real_samples_to_mix;
        mdi->samples_to_mix -= real_samples_to_mix;
    } while (size);

    tmp_buffer = out_buffer;

    if (mdi->extra_info.mixer_options & WM_MO_REVERB) {
        _WM_do_reverb(mdi->reverb, tmp_buffer, (buffer_used / 2));
    }

    //_WM_DynamicVolumeAdjust(mdi, tmp_buffer, (buffer_used/2));

    for (i = 0; i < buffer_used; i += 4) {
        left_mix = *tmp_buffer++;
        right_mix = *tmp_buffer++;

        /*
         * ===================
         * Write to the buffer
         * ===================
         */
#ifdef WORDS_BIGENDIAN
        (*buffer++) = ((left_mix >> 8) & 0x7f) | ((left_mix >> 24) & 0x80);
        (*buffer++) = left_mix & 0xff;
        (*buffer++) = ((right_mix >> 8) & 0x7f) | ((right_mix >> 24) & 0x80);
        (*buffer++) = right_mix & 0xff;
#else
        (*buffer++) = left_mix & 0xff;
        (*buffer++) = ((left_mix >> 8) & 0x7f) | ((left_mix >> 24) & 0x80);
        (*buffer++) = right_mix & 0xff;
        (*buffer++) = ((right_mix >> 8) & 0x7f) | ((right_mix >> 24) & 0x80);
#endif
    }

    _WM_Unlock(&mdi->lock);
    return (buffer_used);
}

static int WM_GetOutput_Gauss(midi * handle, int8_t *buffer, uint32_t size) {
    uint32_t buffer_used = 0;
    uint32_t i;
    struct _mdi *mdi = (struct _mdi *) handle;
    uint32_t real_samples_to_mix = 0;
    uint32_t data_pos;
    int32_t premix, left_mix, right_mix;
    struct _note *note_data = NULL;
    uint32_t count;
    int16_t *sptr;
    double y, xd;
    double *gptr, *gend;
    int left, right, temp_n;
    int ii, jj;
    struct _event *event = mdi->current_event;
    int32_t *tmp_buffer;
    int32_t *out_buffer;

    _WM_Lock(&mdi->lock);

    buffer_used = 0;
    memset(buffer, 0, size);

    if ( (size / 2) > mdi->mix_buffer_size) {
        if ( (size / 2) <= ( mdi->mix_buffer_size * 2 )) {
            mdi->mix_buffer_size += MEM_CHUNK;
        } else {
            mdi->mix_buffer_size = size / 2;
        }
        mdi->mix_buffer = realloc(mdi->mix_buffer, mdi->mix_buffer_size * sizeof(int32_t));
    }

    tmp_buffer = mdi->mix_buffer;

    memset(tmp_buffer, 0, ((size / 2) * sizeof(int32_t)));
    out_buffer = tmp_buffer;

    do {
        if (__builtin_expect((!mdi->samples_to_mix), 0)) {
            while ((!mdi->samples_to_mix) && (event->do_event)) {
                event->do_event(mdi, &event->event_data);
                if ((mdi->extra_info.mixer_options & WM_MO_LOOP) && (event[0].do_event == *_WM_do_meta_endoftrack)) {
                    _WM_ResetToStart(mdi);
                    event = mdi->current_event;
                } else {
                    mdi->samples_to_mix = event->samples_to_next;
                    event++;
                    mdi->current_event = event;
                }
            }

            if (!mdi->samples_to_mix) {
                if (mdi->extra_info.current_sample
                    >= mdi->extra_info.approx_total_samples) {
                    break;
                } else if ((mdi->extra_info.approx_total_samples
                            - mdi->extra_info.current_sample) > (size >> 2)) {
                    mdi->samples_to_mix = size >> 2;
                } else {
                    mdi->samples_to_mix = mdi->extra_info.approx_total_samples
                    - mdi->extra_info.current_sample;
                }
            }
        }
        if (__builtin_expect((mdi->samples_to_mix > (size >> 2)), 1)) {
            real_samples_to_mix = size >> 2;
        } else {
            real_samples_to_mix = mdi->samples_to_mix;
            if (real_samples_to_mix == 0) {
                continue;
            }
        }

        /* do mixing here */
        count = real_samples_to_mix;
        do {
            note_data = mdi->note;
            left_mix = right_mix = 0;
            if (__builtin_expect((note_data != NULL), 1)) {
                while (note_data) {
                    /*
                     * ===================
                     * resample the sample
                     * ===================
                     */
                    data_pos = note_data->sample_pos >> FPBITS;

                    /* check to see if we're near one of the ends */
                    left = data_pos;
                    right = (note_data->sample->data_length >> FPBITS) - left
                            - 1;
                    temp_n = (right << 1) - 1;
                    if (temp_n <= 0)
                        temp_n = 1;
                    if (temp_n > (left << 1) + 1)
                        temp_n = (left << 1) + 1;

                    /* use Newton if we can't fill the window */
                    if (temp_n < gauss_n) {
                        xd = note_data->sample_pos & FPMASK;
                        xd /= (1L << FPBITS);
                        xd += temp_n >> 1;
                        y = 0;
                        sptr = note_data->sample->data
                                + (note_data->sample_pos >> FPBITS)
                                - (temp_n >> 1);
                        for (ii = temp_n; ii;) {
                            for (jj = 0; jj <= ii; jj++)
                                y += sptr[jj] * newt_coeffs[ii][jj];
                            y *= xd - --ii;
                        }
                        y += *sptr;
                    } else { /* otherwise, use Gauss as usual */
                        y = 0;
                        gptr = &gauss_table[(note_data->sample_pos & FPMASK) *
                                     (gauss_n + 1)];
                        gend = gptr + gauss_n;
                        sptr = note_data->sample->data
                                + (note_data->sample_pos >> FPBITS)
                                - (gauss_n >> 1);
                        do {
                            y += *(sptr++) * *(gptr++);
                        } while (gptr <= gend);
                    }

                    premix = (int32_t)((y * (note_data->env_level >> 12)) / 1024);

                    left_mix += (premix * (int32_t)note_data->left_mix_volume) / 1024;
                    right_mix += (premix * (int32_t)note_data->right_mix_volume) / 1024;

                    /*
                     * ========================
                     * sample position checking
                     * ========================
                     */
                    note_data->sample_pos += note_data->sample_inc;
                    if (__builtin_expect(
                                         (note_data->sample_pos > note_data->sample->loop_end),
                                         0)) {
                        if (note_data->modes & SAMPLE_LOOP) {
                            note_data->sample_pos =
                            note_data->sample->loop_start
                            + ((note_data->sample_pos
                                - note_data->sample->loop_start)
                               % note_data->sample->loop_size);
                        } else if (__builtin_expect(
                                                    (note_data->sample_pos
                                                     >= note_data->sample->data_length),
                                                    0)) {
                            goto _END_THIS_NOTE;
                        }
                    }

                    if (__builtin_expect((note_data->env_inc == 0), 0)) {
                        /*
                         fprintf(stderr,"\r\nINC = 0, ENV %i, LEVEL %i, TARGET %d, RATE %i\r\n",
                                 note_data->env, note_data->env_level,
                                 note_data->sample->env_target[note_data->env],
                                 note_data->sample->env_rate[note_data->env]);
                         */
                        note_data = note_data->next;
                        continue;
                    }

                    note_data->env_level += note_data->env_inc;
                    /*
                     fprintf(stderr,"\r\nENV %i, LEVEL %i, TARGET %d, RATE %i, INC %i\r\n",
                             note_data->env, note_data->env_level,
                             note_data->sample->env_target[note_data->env],
                             note_data->sample->env_rate[note_data->env],
                             note_data->env_inc);
                     */
                    if (note_data->env_inc < 0) {
                        if (note_data->env_level
                            > note_data->sample->env_target[note_data->env]) {
                            note_data = note_data->next;
                            continue;
                        }
                    } else if (note_data->env_inc > 0) {
                        if (note_data->env_level
                            < note_data->sample->env_target[note_data->env]) {
                            note_data = note_data->next;
                            continue;
                        }
                    }

                    // Yes could have a condition here but
                    // it would crete another bottleneck

                    note_data->env_level =
                    note_data->sample->env_target[note_data->env];
                    switch (note_data->env) {
                        case 0:
                            if (!(note_data->modes & SAMPLE_ENVELOPE)) {
                                note_data->env_inc = 0;
                                note_data = note_data->next;
                                continue;
                            }
                            break;
                        case 2:
                            if (note_data->modes & SAMPLE_SUSTAIN /*|| note_data->hold*/) {
                                note_data->env_inc = 0;
                                note_data = note_data->next;
                                continue;
                            } else if (note_data->modes & SAMPLE_CLAMPED) {
                                note_data->env = 5;
                                if (note_data->env_level
                                    > note_data->sample->env_target[5]) {
                                    note_data->env_inc =
                                    -note_data->sample->env_rate[5];
                                } else {
                                    note_data->env_inc =
                                    note_data->sample->env_rate[5];
                                }
                                continue;
                            }
                            break;
                        case 5:
                            if (__builtin_expect((note_data->env_level == 0), 1)) {
                                goto _END_THIS_NOTE;
                            }
                            /* sample release */
                            if (note_data->modes & SAMPLE_LOOP)
                                note_data->modes ^= SAMPLE_LOOP;
                            note_data->env_inc = 0;
                            note_data = note_data->next;
                            continue;
                        case 6:
                        _END_THIS_NOTE:
                            if (__builtin_expect((note_data->replay != NULL), 1)) {
                                note_data->active = 0;
                                {
                                    struct _note *prev_note = NULL;
                                    struct _note *nte_array = mdi->note;

                                    if (nte_array != note_data) {
                                        do {
                                            prev_note = nte_array;
                                            nte_array = nte_array->next;
                                        } while (nte_array != note_data);
                                    }
                                    if (prev_note) {
                                        prev_note->next = note_data->replay;
                                    } else {
                                        mdi->note = note_data->replay;
                                    }
                                    note_data->replay->next = note_data->next;
                                    note_data = note_data->replay;
                                    note_data->active = 1;
                                }
                            } else {
                                note_data->active = 0;
                                {
                                    struct _note *prev_note = NULL;
                                    struct _note *nte_array = mdi->note;

                                    if (nte_array != note_data) {
                                        do {
                                            prev_note = nte_array;
                                            nte_array = nte_array->next;
                                        } while ((nte_array != note_data)
                                                 && (nte_array));
                                    }
                                    if (prev_note) {
                                        prev_note->next = note_data->next;
                                    } else {
                                        mdi->note = note_data->next;
                                    }
                                    note_data = note_data->next;
                                }
                            }
                            continue;
                    }
                    note_data->env++;

                    if (note_data->is_off == 1) {
                        _WM_do_note_off_extra(note_data);
                    } else {

                        if (note_data->env_level
                            >= note_data->sample->env_target[note_data->env]) {
                            note_data->env_inc =
                            -note_data->sample->env_rate[note_data->env];
                        } else {
                            note_data->env_inc =
                            note_data->sample->env_rate[note_data->env];
                        }
                    }
                    note_data = note_data->next;
                    continue;
                }
            }
            *tmp_buffer++ = left_mix;
            *tmp_buffer++ = right_mix;
        } while (--count);

        buffer_used += real_samples_to_mix * 4;
        size -= (real_samples_to_mix << 2);
        mdi->extra_info.current_sample += real_samples_to_mix;
        mdi->samples_to_mix -= real_samples_to_mix;
    } while (size);

    tmp_buffer = out_buffer;

    if (mdi->extra_info.mixer_options & WM_MO_REVERB) {
        _WM_do_reverb(mdi->reverb, tmp_buffer, (buffer_used / 2));
    }

    // _WM_DynamicVolumeAdjust(mdi, tmp_buffer, (buffer_used/2));

    for (i = 0; i < buffer_used; i += 4) {
        left_mix = *tmp_buffer++;
        right_mix = *tmp_buffer++;

        /*
         * ===================
         * Write to the buffer
         * ===================
         */
#ifdef WORDS_BIGENDIAN
        (*buffer++) = ((left_mix >> 8) & 0x7f) | ((left_mix >> 24) & 0x80);
        (*buffer++) = left_mix & 0xff;
        (*buffer++) = ((right_mix >> 8) & 0x7f) | ((right_mix >> 24) & 0x80);
        (*buffer++) = right_mix & 0xff;
#else
        (*buffer++) = left_mix & 0xff;
        (*buffer++) = ((left_mix >> 8) & 0x7f) | ((left_mix >> 24) & 0x80);
        (*buffer++) = right_mix & 0xff;
        (*buffer++) = ((right_mix >> 8) & 0x7f) | ((right_mix >> 24) & 0x80);
#endif
    }
    _WM_Unlock(&mdi->lock);
    return (buffer_used);
}

/*
 * =========================
 * External Functions
 * =========================
 */

WM_SYMBOL int WildMidi_ConvertToMidi (const char *file, uint8_t **out, uint32_t *size) {
    uint8_t *buf;
    int ret;

    if (!file) {
        _WM_GLOBAL_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID_ARG, "(NULL filename)", 0);
        return (-1);
    }
    if ((buf = (uint8_t *) _WM_BufferFile(file, size)) == NULL) {
        return (-1);
    }

    ret = WildMidi_ConvertBufferToMidi(buf, *size, out, size);
    free(buf);
    return ret;
}

WM_SYMBOL int WildMidi_ConvertBufferToMidi (uint8_t *in, uint32_t insize,
                                            uint8_t **out, uint32_t *outsize) {
    if (!in || !out || !outsize) {
        _WM_GLOBAL_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID_ARG, "(NULL params)", 0);
        return (-1);
    }

    if (!memcmp(in, "FORM", 4)) {
        if (_WM_xmi2midi(in, insize, out, outsize,
                _cvt_get_option(WM_CO_XMI_TYPE)) < 0) {
            return (-1);
        }
    }
    else if (!memcmp(in, "MUS", 3)) {
        if (_WM_mus2midi(in, insize, out, outsize,
                _cvt_get_option(WM_CO_FREQUENCY)) < 0) {
            return (-1);
        }
    }
    else if (!memcmp(in, "MThd", 4)) {
        _WM_GLOBAL_ERROR(__FUNCTION__, __LINE__, 0, "Already a midi file", 0);
        return (-1);
    }
    else {
        _WM_GLOBAL_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID, NULL, 0);
        return (-1);
    }

    return (0);
}

WM_SYMBOL const char *WildMidi_GetString(uint16_t info) {
    static char WM_Version[] = "WildMidi Processing Library " PACKAGE_VERSION;
    switch (info) {
    case WM_GS_VERSION:
        return WM_Version;
    }
    return NULL;
}

WM_SYMBOL long WildMidi_GetVersion (void) {
    return (LIBWILDMIDI_VERSION);
}

WM_SYMBOL int WildMidi_Init(const char *config_file, uint16_t rate, uint16_t mixer_options) {
    if (WM_Initialized) {
        _WM_GLOBAL_ERROR(__FUNCTION__, __LINE__, WM_ERR_ALR_INIT, NULL, 0);
        return (-1);
    }

    if (config_file == NULL) {
        _WM_GLOBAL_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID_ARG,
                "(NULL config file pointer)", 0);
        return (-1);
    }
    WM_InitPatches();
    if (WM_LoadConfig(config_file) == -1) {
        return (-1);
    }

    if (mixer_options & 0x0FF0) {
        _WM_GLOBAL_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID_ARG, "(invalid option)",
                0);
        WM_FreePatches();
        return (-1);
    }
    _WM_MixerOptions = mixer_options;

    if (rate < 11025) {
        _WM_GLOBAL_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID_ARG,
                "(rate out of bounds, range is 11025 - 65535)", 0);
        WM_FreePatches();
        return (-1);
    }
    _WM_SampleRate = rate;

    gauss_lock = 0;
    _WM_patch_lock = 0;
    _WM_MasterVolume = 948;
    WM_Initialized = 1;

    return (0);
}

WM_SYMBOL int WildMidi_MasterVolume(uint8_t master_volume) {
    if (!WM_Initialized) {
        _WM_GLOBAL_ERROR(__FUNCTION__, __LINE__, WM_ERR_NOT_INIT, NULL, 0);
        return (-1);
    }
    if (master_volume > 127) {
        _WM_GLOBAL_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID_ARG,
                "(master volume out of range, range is 0-127)", 0);
        return (-1);
    }

    _WM_MasterVolume = _WM_lin_volume[master_volume];

    return (0);
}

WM_SYMBOL int WildMidi_Close(midi * handle) {
    struct _mdi *mdi = (struct _mdi *) handle;
    struct _hndl * tmp_handle;

    if (!WM_Initialized) {
        _WM_GLOBAL_ERROR(__FUNCTION__, __LINE__, WM_ERR_NOT_INIT, NULL, 0);
        return (-1);
    }
    if (handle == NULL) {
        _WM_GLOBAL_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID_ARG, "(NULL handle)", 0);
        return (-1);
    }
    if (first_handle == NULL) {
        _WM_GLOBAL_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID_ARG, "(no midi's open)", 0);
        return (-1);
    }
    _WM_Lock(&mdi->lock);
    if (first_handle->handle == handle) {
        tmp_handle = first_handle->next;
        free(first_handle);
        first_handle = tmp_handle;
        if (first_handle)
            first_handle->prev = NULL;
    } else {
        tmp_handle = first_handle;
        while (tmp_handle->handle != handle) {
            tmp_handle = tmp_handle->next;
            if (tmp_handle == NULL) {
                break;
            }
        }
        if (tmp_handle) {
            tmp_handle->prev->next = tmp_handle->next;
            if (tmp_handle->next) {
                tmp_handle->next->prev = tmp_handle->prev;
            }
            free(tmp_handle);
        }
    }

    _WM_freeMDI(mdi);

    return (0);
}

WM_SYMBOL midi *WildMidi_Open(const char *midifile) {
    uint8_t *mididata = NULL;
    uint32_t midisize = 0;
    uint8_t mus_hdr[] = { 'M', 'U', 'S', 0x1A };
    uint8_t xmi_hdr[] = { 'F', 'O', 'R', 'M' };
    midi * ret = NULL;

    if (!WM_Initialized) {
        _WM_GLOBAL_ERROR(__FUNCTION__, __LINE__, WM_ERR_NOT_INIT, NULL, 0);
        return (NULL);
    }
    if (midifile == NULL) {
        _WM_GLOBAL_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID_ARG, "(NULL filename)", 0);
        return (NULL);
    }

    if ((mididata = (uint8_t *) _WM_BufferFile(midifile, &midisize)) == NULL) {
        return (NULL);
    }
    if (midisize < 18) {
        _WM_GLOBAL_ERROR(__FUNCTION__, __LINE__, WM_ERR_CORUPT, "(too short)", 0);
        return (NULL);
    }
    if (memcmp(mididata,"HMIMIDIP", 8) == 0) {
        ret = (void *) _WM_ParseNewHmp(mididata, midisize);
    } else if (memcmp(mididata, "HMI-MIDISONG061595", 18) == 0) {
        ret = (void *) _WM_ParseNewHmi(mididata, midisize);
    } else if (memcmp(mididata, mus_hdr, 4) == 0) {
        ret = (void *) _WM_ParseNewMus(mididata, midisize);
    } else if (memcmp(mididata, xmi_hdr, 4) == 0) {
        ret = (void *) _WM_ParseNewXmi(mididata, midisize);
    } else {
        ret = (void *) _WM_ParseNewMidi(mididata, midisize);
    }
    free(mididata);

    if (ret) {
        if (add_handle(ret) != 0) {
            WildMidi_Close(ret);
            ret = NULL;
        }
    }

    return (ret);
}

WM_SYMBOL midi *WildMidi_OpenBuffer(uint8_t *midibuffer, uint32_t size) {
    uint8_t mus_hdr[] = { 'M', 'U', 'S', 0x1A };
    uint8_t xmi_hdr[] = { 'F', 'O', 'R', 'M' };
    midi * ret = NULL;

    if (!WM_Initialized) {
        _WM_GLOBAL_ERROR(__FUNCTION__, __LINE__, WM_ERR_NOT_INIT, NULL, 0);
        return (NULL);
    }
    if (midibuffer == NULL) {
        _WM_GLOBAL_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID_ARG, "(NULL midi data buffer)", 0);
        return (NULL);
    }
    if (size > WM_MAXFILESIZE) {
        /* don't bother loading suspiciously long files */
        _WM_GLOBAL_ERROR(__FUNCTION__, __LINE__, WM_ERR_LONGFIL, NULL, 0);
        return (NULL);
    }
    if (size < 18) {
        _WM_GLOBAL_ERROR(__FUNCTION__, __LINE__, WM_ERR_CORUPT, "(too short)", 0);
        return (NULL);
    }
    if (memcmp(midibuffer,"HMIMIDIP", 8) == 0) {
        ret = (void *) _WM_ParseNewHmp(midibuffer, size);
    } else if (memcmp(midibuffer, "HMI-MIDISONG061595", 18) == 0) {
        ret = (void *) _WM_ParseNewHmi(midibuffer, size);
    } else if (memcmp(midibuffer, mus_hdr, 4) == 0) {
        ret = (void *) _WM_ParseNewMus(midibuffer, size);
    } else if (memcmp(midibuffer, xmi_hdr, 4) == 0) {
        ret = (void *) _WM_ParseNewXmi(midibuffer, size);
    } else {
        ret = (void *) _WM_ParseNewMidi(midibuffer, size);
    }

    if (ret) {
        if (add_handle(ret) != 0) {
            WildMidi_Close(ret);
            ret = NULL;
        }
    }

    return (ret);
}

WM_SYMBOL int WildMidi_FastSeek(midi * handle, unsigned long int *sample_pos) {
    struct _mdi *mdi;
    struct _event *event;
    struct _note *note_data;

    if (!WM_Initialized) {
        _WM_GLOBAL_ERROR(__FUNCTION__, __LINE__, WM_ERR_NOT_INIT, NULL, 0);
        return (-1);
    }
    if (handle == NULL) {
        _WM_GLOBAL_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID_ARG, "(NULL handle)", 0);
        return (-1);
    }
    if (sample_pos == NULL) {
        _WM_GLOBAL_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID_ARG, "(NULL seek position pointer)", 0);
        return (-1);
    }

    mdi = (struct _mdi *) handle;
    _WM_Lock(&mdi->lock);
    event = mdi->current_event;

    /* make sure we havent asked for a positions beyond the end of the song. */
    if (*sample_pos > mdi->extra_info.approx_total_samples) {
        /* if so set the position to the end of the song */
        *sample_pos = mdi->extra_info.approx_total_samples;
    }

    /* was end of song requested and are we are there? */
    if (*sample_pos == mdi->extra_info.approx_total_samples) {
        /* yes */
        _WM_Unlock(&mdi->lock);
        return (0);
    }

    /* did we want to fast forward? */
    if (mdi->extra_info.current_sample > *sample_pos) {
        /* no - reset some stuff */
        event = mdi->events;
        _WM_ResetToStart(handle);
        mdi->extra_info.current_sample = 0;
        mdi->samples_to_mix = 0;
    }

    if ((mdi->extra_info.current_sample + mdi->samples_to_mix) > *sample_pos) {
        mdi->samples_to_mix = (mdi->extra_info.current_sample + mdi->samples_to_mix) - *sample_pos;
        mdi->extra_info.current_sample = *sample_pos;
    } else {
        mdi->extra_info.current_sample += mdi->samples_to_mix;
        mdi->samples_to_mix = 0;
        while ((!mdi->samples_to_mix) && (event->do_event)) {
            event->do_event(mdi, &event->event_data);
            mdi->samples_to_mix = event->samples_to_next;
                
            if ((mdi->extra_info.current_sample + mdi->samples_to_mix) > *sample_pos) {
                mdi->samples_to_mix = (mdi->extra_info.current_sample + mdi->samples_to_mix) - *sample_pos;
                mdi->extra_info.current_sample = *sample_pos;
            } else {
                mdi->extra_info.current_sample += mdi->samples_to_mix;
                mdi->samples_to_mix = 0;
            }
            event++;
        }
        mdi->current_event = event;
    }

    /*
     * Clear notes as this is a fast seek so we only care
     * about new notes.
     *
     * NOTE: This function is for performance only.
     * Might need a WildMidi_SlowSeek if we need better accuracy.
     */
    note_data = mdi->note;
    if (note_data) {
        do {
            note_data->active = 0;
            if (note_data->replay) {
                note_data->replay = NULL;
            }
            note_data = note_data->next;
        } while (note_data);
    }
    mdi->note = NULL;

    /* clear the reverb buffers since we not gonna be using them here */
    _WM_reset_reverb(mdi->reverb);

    _WM_Unlock(&mdi->lock);
    return (0);
}

WM_SYMBOL int WildMidi_SongSeek (midi * handle, int8_t nextsong) {
    struct _mdi *mdi;
    struct _event *event;
    struct _event *event_new;
    struct _note *note_data;

    if (!WM_Initialized) {
        _WM_GLOBAL_ERROR(__FUNCTION__, __LINE__, WM_ERR_NOT_INIT, NULL, 0);
        return (-1);
    }
    if (handle == NULL) {
        _WM_GLOBAL_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID_ARG, "(NULL handle)", 0);
        return (-1);
    }
    mdi = (struct _mdi *) handle;
    _WM_Lock(&mdi->lock);

    if ((!mdi->is_type2) && (nextsong != 0)) {
        _WM_GLOBAL_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID_ARG, "(Illegal use. Only usable with files detected to be type 2 compatible.", 0);
        _WM_Unlock(&mdi->lock);
        return (-1);
    }
    if ((nextsong > 1) || (nextsong < -1)) {
        _WM_GLOBAL_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID_ARG, "(Invalid nextsong setting. -1 is previous song, 0 start of current song, 1 is next song)", 0);
        _WM_Unlock(&mdi->lock);
        return (-1);
    }

    event = mdi->current_event;

    if (nextsong == -1) {
        /* goto start of previous song */
        /*
         * So with this one we have to go back 2 eof's
         * then forward 1 event to get to the start of
         * the previous song.
         * NOTE: We will automatically stop at the start 
         * of the data.
         */
        uint8_t eof_cnt = 1;
        while (event != mdi->events) {
            if (event[-1].do_event == _WM_do_meta_endoftrack) {
                if (eof_cnt == 0) {
                    break;
                }
                eof_cnt = 0;
            }
            event--;
        }
        event_new = event;
        event = mdi->events;
        _WM_ResetToStart(handle);

    } else if (nextsong == 1) {
        /* goto start of next song */
        while (event->do_event != NULL) {
            if (event->do_event == _WM_do_meta_endoftrack) {
                event++;
                if (event->do_event == NULL) {
                    event--;
                    goto START_THIS_SONG;
                } else {
                    break;
                }
            }
            event++;
        }
        event_new = event;
        event = mdi->current_event;

    } else {
    START_THIS_SONG:
        /* goto start of this song */
        /* first find the offset */
        while (event != mdi->events) {
            if (event[-1].do_event == _WM_do_meta_endoftrack) {
                break;
            }
            event--;
        }
        event_new = event;
        event = mdi->events;
        _WM_ResetToStart(handle);
    }

    while (event != event_new) {
        event->do_event(mdi, &event->event_data);
        mdi->extra_info.current_sample += event->samples_to_next;
        event++;
    }

    mdi->current_event = event;

    note_data = mdi->note;
    if (note_data) {
        do {
            note_data->active = 0;
            if (note_data->replay) {
                note_data->replay = NULL;
            }
            note_data = note_data->next;
        } while (note_data);
    }
    mdi->note = NULL;

    _WM_Unlock(&mdi->lock);
    return (0);
}

WM_SYMBOL int WildMidi_GetOutput(midi * handle, int8_t *buffer, uint32_t size) {
    if (__builtin_expect((!WM_Initialized), 0)) {
        _WM_GLOBAL_ERROR(__FUNCTION__, __LINE__, WM_ERR_NOT_INIT, NULL, 0);
        return (-1);
    }
    if (__builtin_expect((handle == NULL), 0)) {
        _WM_GLOBAL_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID_ARG, "(NULL handle)", 0);
        return (-1);
    }
    if (__builtin_expect((buffer == NULL), 0)) {
        _WM_GLOBAL_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID_ARG, "(NULL buffer pointer)", 0);
        return (-1);
    }
    if (__builtin_expect((size == 0), 0)) {
        return (0);
    }
    if (__builtin_expect((!!(size % 4)), 0)) {
        _WM_GLOBAL_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID_ARG, "(size not a multiple of 4)", 0);
        return (-1);
    }

    if (((struct _mdi *) handle)->extra_info.mixer_options & WM_MO_ENHANCED_RESAMPLING) {
        if (!gauss_table) init_gauss();
        return (WM_GetOutput_Gauss(handle, buffer, size));
    }
    return (WM_GetOutput_Linear(handle, buffer, size));
}

WM_SYMBOL int WildMidi_GetMidiOutput(midi * handle, int8_t **buffer, uint32_t *size) {
    if (__builtin_expect((!WM_Initialized), 0)) {
        _WM_GLOBAL_ERROR(__FUNCTION__, __LINE__, WM_ERR_NOT_INIT, NULL, 0);
        return (-1);
    }
    if (__builtin_expect((handle == NULL), 0)) {
        _WM_GLOBAL_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID_ARG, "(NULL handle)", 0);
        return (-1);
    }
    if (__builtin_expect((buffer == NULL), 0)) {
        _WM_GLOBAL_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID_ARG, "(NULL buffer pointer)", 0);
        return (-1);
    }
    return _WM_Event2Midi(handle, (uint8_t **)buffer, size);
}


WM_SYMBOL int WildMidi_SetOption(midi * handle, uint16_t options, uint16_t setting) {
    struct _mdi *mdi;

    if (!WM_Initialized) {
        _WM_GLOBAL_ERROR(__FUNCTION__, __LINE__, WM_ERR_NOT_INIT, NULL, 0);
        return (-1);
    }
    if (handle == NULL) {
        _WM_GLOBAL_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID_ARG, "(NULL handle)", 0);
        return (-1);
    }

    mdi = (struct _mdi *) handle;
    _WM_Lock(&mdi->lock);
    if ((!(options & 0x800F)) || (options & 0x7FF0)) {
        _WM_GLOBAL_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID_ARG, "(invalid option)", 0);
        _WM_Unlock(&mdi->lock);
        return (-1);
    }
    if (setting & 0x7FF0) {
        _WM_GLOBAL_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID_ARG, "(invalid setting)", 0);
        _WM_Unlock(&mdi->lock);
        return (-1);
    }

    mdi->extra_info.mixer_options = ((mdi->extra_info.mixer_options & (0x80FF ^ options))
                                    | (options & setting));

    if (options & WM_MO_LOG_VOLUME) {
            _WM_AdjustChannelVolumes(mdi, 16);  // Settings greater than 15
                                                // adjusts all channels
    } else if (options & WM_MO_REVERB) {
        _WM_reset_reverb(mdi->reverb);
    }

    _WM_Unlock(&mdi->lock);
    return (0);
}

WM_SYMBOL int WildMidi_SetCvtOption(uint16_t tag, uint16_t setting) {
    _WM_Lock(&WM_ConvertOptions.lock);
    switch (tag) {
    case WM_CO_XMI_TYPE: /* validation happens in xmidi.c */
        WM_ConvertOptions.xmi_convert_type = setting;
        break;
    case WM_CO_FREQUENCY: /* validation happens in format */
        WM_ConvertOptions.frequency = setting;
        break;
    default:
        _WM_GLOBAL_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID_ARG, "(invalid setting)", 0);
        _WM_Unlock(&WM_ConvertOptions.lock);
        return (-1);
    }
    _WM_Unlock(&WM_ConvertOptions.lock);
    return (0);
}

WM_SYMBOL struct _WM_Info *
WildMidi_GetInfo(midi * handle) {
    struct _mdi *mdi = (struct _mdi *) handle;
    if (!WM_Initialized) {
        _WM_GLOBAL_ERROR(__FUNCTION__, __LINE__, WM_ERR_NOT_INIT, NULL, 0);
        return (NULL);
    }
    if (handle == NULL) {
        _WM_GLOBAL_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID_ARG, "(NULL handle)", 0);
        return (NULL);
    }
    _WM_Lock(&mdi->lock);
    if (mdi->tmp_info == NULL) {
        mdi->tmp_info = malloc(sizeof(struct _WM_Info));
        if (mdi->tmp_info == NULL) {
            _WM_GLOBAL_ERROR(__FUNCTION__, __LINE__, WM_ERR_MEM, "to set info", 0);
            _WM_Unlock(&mdi->lock);
            return (NULL);
        }
        mdi->tmp_info->copyright = NULL;
    }
    mdi->tmp_info->current_sample = mdi->extra_info.current_sample;
    mdi->tmp_info->approx_total_samples = mdi->extra_info.approx_total_samples;
    mdi->tmp_info->mixer_options = mdi->extra_info.mixer_options;
    mdi->tmp_info->total_midi_time = (mdi->tmp_info->approx_total_samples * 1000) / _WM_SampleRate;
    if (mdi->extra_info.copyright) {
        free(mdi->tmp_info->copyright);
        mdi->tmp_info->copyright = malloc(strlen(mdi->extra_info.copyright) + 1);
        if (mdi->tmp_info->copyright == NULL) {
            free(mdi->tmp_info);
            mdi->tmp_info = NULL;
            _WM_GLOBAL_ERROR(__FUNCTION__, __LINE__, WM_ERR_MEM, "to set copyright", 0);
            _WM_Unlock(&mdi->lock);
            return (NULL);
        } else {
            strcpy(mdi->tmp_info->copyright, mdi->extra_info.copyright);
        }
    } else {
        mdi->tmp_info->copyright = NULL;
    }
    _WM_Unlock(&mdi->lock);
    return ((struct _WM_Info *)mdi->tmp_info);
}

WM_SYMBOL int WildMidi_Shutdown(void) {
    if (!WM_Initialized) {
        _WM_GLOBAL_ERROR(__FUNCTION__, __LINE__, WM_ERR_NOT_INIT, NULL, 0);
        return (-1);
    }
    while (first_handle) {
        /* closes open handle and rotates the handles list. */
        WildMidi_Close((struct _mdi *) first_handle->handle);
    }
    WM_FreePatches();
    free_gauss();

    /* reset the globals */
    _cvt_reset_options ();
    _WM_MasterVolume = 948;
    _WM_MixerOptions = 0;
    _WM_fix_release = 0;
    _WM_auto_amp = 0;
    _WM_auto_amp_with_amp = 0;
    _WM_reverb_room_width = 16.875f;
    _WM_reverb_room_length = 22.5f;
    _WM_reverb_listen_posx = 8.4375f;
    _WM_reverb_listen_posy = 16.875f;

    WM_Initialized = 0;

    if (_WM_Global_ErrorS != NULL) free(_WM_Global_ErrorS);

    return (0);
}

/*
    char * WildMidi_GetLyric(midi * handle)

    Returns points to a \0 terminated string that contains the
    data contained in the last read lyric or text meta event.
    Or returns NULL if no lyric is waiting to be read.

    Force read from text meta event by including WM_MO_TEXTASLYRIC
    in the options in WildMidi_Init.

    Programs calling this only need to read the pointer.
    Cleanup is done by the lib.

    Once WildMidi_GetLyric is called it will return NULL
    on subsiquent calls until the next lyric event is processed
    during a WildMidi_GetOutput call.
 */
WM_SYMBOL char * WildMidi_GetLyric (midi * handle) {
    struct _mdi *mdi = (struct _mdi *) handle;
    char * lyric = NULL;

    if (!WM_Initialized) {
        _WM_GLOBAL_ERROR(__FUNCTION__, __LINE__, WM_ERR_NOT_INIT, NULL, 0);
        return (NULL);
    }
    if (handle == NULL) {
        _WM_GLOBAL_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID_ARG, "(NULL handle)", 0);
        return (NULL);
    }
    _WM_Lock(&mdi->lock);
    lyric = mdi->lyric;
    mdi->lyric = NULL;
    _WM_Unlock(&mdi->lock);
    return (lyric);
}

/*
 * Return Last Error Message
 */
WM_SYMBOL char * WildMidi_GetError (void) {
    return (_WM_Global_ErrorS);
}

/*
 * Clear any error message
 */
WM_SYMBOL void WildMidi_ClearError (void) {
    _WM_Global_ErrorI = 0;
    if (_WM_Global_ErrorS != NULL) {
        free(_WM_Global_ErrorS);
        _WM_Global_ErrorS = NULL;
    }
    return;
}

