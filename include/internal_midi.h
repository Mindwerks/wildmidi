/*
 * internal_midi.h -- Midi Wavetable Processing library
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

#ifndef __INTERNAL_MIDI_H
#define __INTERNAL_MIDI_H

struct _channel {
    uint8_t bank;
    struct _patch *patch;
    uint8_t hold;
    uint8_t volume;
    uint8_t pressure;
    uint8_t expression;
    int8_t  balance;
    int8_t  pan;
    int16_t left_adjust;
    int16_t right_adjust;
    int16_t pitch;
    int16_t pitch_range;
    int32_t pitch_adjust;
    uint16_t reg_data;
    uint8_t reg_non;
    uint8_t isdrum;
};

struct _event_data {
    uint8_t channel;
    union Data {
        uint32_t value;
        char * string;
    } data;
};

struct _note {
    uint16_t noteid;
    uint8_t velocity;
    struct _patch *patch;
    struct _sample *sample;
    uint32_t sample_pos;
    uint32_t sample_inc;
    int32_t env_inc;
    uint8_t env;
    int32_t env_level;
    uint8_t modes;
    uint8_t hold;
    uint8_t active;
    struct _note *replay;
    struct _note *next;
    uint32_t left_mix_volume;
    uint32_t right_mix_volume;
    uint8_t is_off;
    uint8_t ignore_chan_events;
};

struct _mdi;

struct _event {
    void (*do_event)(struct _mdi *mdi, struct _event_data *data);
    struct _event_data event_data;
    uint32_t samples_to_next;
    uint32_t samples_to_next_fixed;
};

struct _mdi {
    int lock;
    uint32_t samples_to_mix;
    struct _event *events;
    struct _event *current_event;
    uint32_t event_count;
    uint32_t events_size; /* try to stay optimally ahead to prevent reallocs */
    struct _WM_Info extra_info;
    struct _WM_Info *tmp_info;
    uint16_t midi_master_vol;
    struct _channel channel[16];
    struct _note *note;
    struct _note note_table[2][16][128];

    struct _patch **patches;
    uint32_t patch_count;
    int16_t amp;

    int32_t *mix_buffer;
    uint32_t mix_buffer_size;

    struct _rvb *reverb;

    int32_t dyn_vol_peak;
    double dyn_vol_adjust;
    double dyn_vol;
    double dyn_vol_to_reach;

    uint8_t is_type2;

    char *lyric;
};


extern int16_t _WM_lin_volume[];
extern uint32_t _WM_freq_table[];

/* ===================== */

/*
 * All "do" functions need to be "extern" for playback
 */
extern void _WM_do_midi_divisions(struct _mdi *mdi, struct _event_data *data);
extern void _WM_do_note_off(struct _mdi *mdi, struct _event_data *data);
extern void _WM_do_note_on(struct _mdi *mdi, struct _event_data *data);
extern void _WM_do_aftertouch(struct _mdi *mdi, struct _event_data *data);
extern void _WM_do_control_bank_select(struct _mdi *mdi, struct _event_data *data);
extern void _WM_do_control_data_entry_course(struct _mdi *mdi, struct _event_data *data);
extern void _WM_do_control_channel_volume(struct _mdi *mdi, struct _event_data *data);
extern void _WM_do_control_channel_balance(struct _mdi *mdi, struct _event_data *data);
extern void _WM_do_control_channel_pan(struct _mdi *mdi, struct _event_data *data);
extern void _WM_do_control_channel_expression(struct _mdi *mdi, struct _event_data *data);
extern void _WM_do_control_data_entry_fine(struct _mdi *mdi, struct _event_data *data);
extern void _WM_do_control_channel_hold(struct _mdi *mdi, struct _event_data *data);
extern void _WM_do_control_data_increment(struct _mdi *mdi, struct _event_data *data);
extern void _WM_do_control_data_decrement(struct _mdi *mdi, struct _event_data *data);
extern void _WM_do_control_non_registered_param_fine(struct _mdi *mdi, struct _event_data *data);
extern void _WM_do_control_non_registered_param_course(struct _mdi *mdi, struct _event_data *data);
extern void _WM_do_control_registered_param_fine(struct _mdi *mdi, struct _event_data *data);
extern void _WM_do_control_registered_param_course(struct _mdi *mdi, struct _event_data *data);
extern void _WM_do_control_channel_sound_off(struct _mdi *mdi, struct _event_data *data);
extern void _WM_do_control_channel_controllers_off(struct _mdi *mdi, struct _event_data *data);
extern void _WM_do_control_channel_notes_off(struct _mdi *mdi, struct _event_data *data);
extern void _WM_do_control_dummy(struct _mdi *mdi, struct _event_data *data);
extern void _WM_do_patch(struct _mdi *mdi, struct _event_data *data);
extern void _WM_do_channel_pressure(struct _mdi *mdi, struct _event_data *data);
extern void _WM_do_pitch(struct _mdi *mdi, struct _event_data *data);
extern void _WM_do_sysex_roland_drum_track(struct _mdi *mdi, struct _event_data *data);
extern void _WM_do_sysex_gm_reset(struct _mdi *mdi, struct _event_data *data);
extern void _WM_do_sysex_roland_reset(struct _mdi *mdi, struct _event_data *data);
extern void _WM_do_sysex_yamaha_reset(struct _mdi *mdi, struct _event_data *data);
extern void _WM_do_meta_endoftrack(struct _mdi *mdi, struct _event_data *data);
extern void _WM_do_meta_tempo(struct _mdi *mdi, struct _event_data *data);
extern void _WM_do_meta_timesignature(struct _mdi *mdi, struct _event_data *data);
extern void _WM_do_meta_keysignature(struct _mdi *mdi, struct _event_data *data);
extern void _WM_do_meta_sequenceno(struct _mdi *mdi, struct _event_data *data);
extern void _WM_do_meta_channelprefix(struct _mdi *mdi, struct _event_data *data);
extern void _WM_do_meta_portprefix(struct _mdi *mdi, struct _event_data *data);
extern void _WM_do_meta_smpteoffset(struct _mdi *mdi, struct _event_data *data);
extern void _WM_do_meta_text(struct _mdi *mdi, struct _event_data *data);
extern void _WM_do_meta_copyright(struct _mdi *mdi, struct _event_data *data);
extern void _WM_do_meta_trackname(struct _mdi *mdi, struct _event_data *data);
extern void _WM_do_meta_instrumentname(struct _mdi *mdi, struct _event_data *data);
extern void _WM_do_meta_lyric(struct _mdi *mdi, struct _event_data *data);
extern void _WM_do_meta_marker(struct _mdi *mdi, struct _event_data *data);
extern void _WM_do_meta_cuepoint(struct _mdi *mdi, struct _event_data *data);

/*
 * We need to expose these fuctions for use on some or the parsers due to some
 * formats not being able to trigger these events via _WM_Setup_Midi_Event.
 */
extern int _WM_midi_setup_noteoff(struct _mdi *mdi, uint8_t channel, uint8_t note, uint8_t velocity);
extern int _WM_midi_setup_endoftrack(struct _mdi *mdi);
extern int _WM_midi_setup_tempo(struct _mdi *mdi, uint32_t setting);

/* ===================== */

/*
 * Only non-standard midi event or non-track event setup functions need to be here
 */
extern int _WM_midi_setup_divisions(struct _mdi *mdi, uint32_t divisions);

/* ===================== */

/*
 * All other declarations
 */

extern struct _mdi * _WM_initMDI(void);
extern void _WM_freeMDI(struct _mdi *mdi);
extern uint32_t _WM_SetupMidiEvent(struct _mdi *mdi, uint8_t * event_data, uint8_t running_event);
extern void _WM_ResetToStart(struct _mdi *mdi);
extern void _WM_do_pan_adjust(struct _mdi *mdi, uint8_t ch);
extern void _WM_do_note_off_extra(struct _note *nte);
/* extern void _WM_DynamicVolumeAdjust(struct _mdi *mdi, int32_t *tmp_buffer, uint32_t buffer_used);*/
extern void _WM_AdjustChannelVolumes(struct _mdi *mdi, uint8_t ch);
extern float _WM_GetSamplesPerTick(uint32_t divisions, uint32_t tempo);

#endif /* __INTERNAL_MIDI_H */

