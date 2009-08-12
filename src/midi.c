/*
    midi.c - parse MIDI file
    Copyright (C) 2001-2008 Chris Ison

    This file is part of WildMIDI.

    WildMIDI is free software: you can redistribute and/or modify the players
    under the terms of the GNU General Public License and you can redistribute
    and/or modify the library under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation, either version 3 of
   the licenses, or(at your option) any later version.

    WildMIDI is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License and
    the GNU Lesser General Public License for more details.

    You should have received a copy of the GNU General Public License and the
    GNU Lesser General Public License along with WildMIDI.  If not,  see
    <http://www.gnu.org/licenses/>.
	
    Email: cisos@bigpond.net.au
    	 wildcode@users.sourceforge.net

    $Id: midi.c,v 1.4 2008/06/04 13:08:27 wildcode Exp $
*/

#include "midi.h"

/*

*/

struct _events *
load_midifile (char *filename) {
	unsigned int buffer_size = 0;
	unsigned int buffer_ofs = 0;
	unsigned int tmp_int = 0;
	unsigned int track_size = 0;
	unsigned int tmp_delta = 0;
	unsigned char channel = 0;
	unsigned char event = 0;
	unsigned char midi_type = 0;
	unsigned char midi_tracks = 0;
	unsigned char track_no = 0;
	unsigned char *buffer = NULL;
	unsigned char *read_buf = NULL;
	struct _events *temp_events = NULL;
	struct _events *ret_events = NULL;
	
	if (filename == NULL) {
		return NULL;
	}
	
	if ((buffer = WM_BufferFile(filename, &buffer_size))== NULL) {
		return NULL;
	}
	
	if (buffer_size < 25) {
		free (buffer);
		return NULL;
	}
	
	read_buf = buffer;
	
	/* file header */
	if (strncmp(read_buf, "MThd", 4)) {
		/* unknown file format */
		free (buffer);
		return NULL;
	}
	read_buf += 4;
	
	/* header size */
	tmp_int = *read_buf++ << 24;
	tmp_int |= *read_buf++ << 16;
	tmp_int |= *read_buf++ << 8;
	tmp_int |= *read_buf++;

	if (tmp_int != 6) {
		/* unknown header format */
		free (buffer);
		return;
	}
	
	/* midi type */
	midi_type = *read_buf++ << 8;
	midi_type |= *read_buf++;
	
	/* only type 0 and 1 supported */
	if (!(midi_type | 1)) {
		/* insupported format */
		free (buffer);
		return NULL
	}
	
	/* number of tracks */
	midi_tracks = *read_buf++ << 8;
	midi_tracks |= *read_buf++;
	
	if ((!midi_type) && (midi_tracks != 1)) {
		/* invalid midi file */
		free (buffer);
		return NULL;
	}
	
	/* pulses per quarter note */
	midi_divs = *read_buf++ << 8;
	midi_divs |= *read_buf++;
	
	/* Track Data */
	while (track_no <= midi_tracks) {
		/* track header */
		if (strncmp(read_buf, "MTrk", 4)) {
			/*  bad data */
			free (buffer);
			return NULL;
		}
		read_buf += 4;

		/* track size */
		track_size = *read_buf++ << 24;
		track_size |= *read_buf++ << 16;
		track_size |= *read_buf++ << 8;
		track_size |= *read_buf++;
		
		if ((track_size < 3) || (((read_buf - buffer) + track_size) > buffer_size)) {
			/*  bad data */
			free (buffer);
			return NULL;
		}

		do {	
			switch (*read_buf >> 4) {
				case 0x8:	/* Note Off */
					read_buf += 3;
					track_size -= 3;
					break;
				case 0x9:	/* Note On */
					read_buf += 3;
					track_size -= 3;
					break;
				case 0xA:	/* Aftertouch */
					read_buf += 3;
					track_size -= 3;
					break;
				case 0xB:	/* Controller */
					read_buf += 3;
					track_size -= 3;
					break;
				case 0xC:	/* Program Change */
					read_buf += 2;
					track_size -= 2;
					break;
				case 0xD:	/* Channel Pressure */
					read_buf += 2;
					track_size -= 2;
					break;
				case 0xE:	/* Pitch Wheel */
					read_buf += 3;
					track_size -= 3;
					break;
				case 0xF:
					switch (*read_buf & 0xf) {
						case 0x0: /* sysex which is unsupported */
							do {
								read_buf++;
								track_size--;
							} while (*read_buf != 0xF7);
							read_buf++;
							break;
						case 0xF: /* meta event */
							read_buf++;
							read_buf++;
							read_buf += *read_buf;
							read_buf++;
							break;
						default: /* corrupt data? */
							if (ret_events) {
								free(ret_events);
							}
							free (buffer);
							return NULL;
					}
					break;
				default:
			}
		} while (track_size);
		
		track_no++;
	}
	
	return temp_events;
}

int
midi_setup_noteoff (struct _mdi *mdi, unsigned char channel, unsigned char note, unsigned char velocity)
{
    if ((mdi->event_count) && (mdi->events[mdi->event_count - 1].do_event == NULL))
    {
        mdi->events[mdi->event_count - 1].do_event = *do_note_off;
        mdi->events[mdi->event_count - 1].event_data.channel = channel;
        mdi->events[mdi->event_count - 1].event_data.data = (note << 8) | velocity;         
    } else {
        mdi->events = realloc(mdi->events,((mdi->event_count + 1) * sizeof(struct _event)));
        mdi->events[mdi->event_count].do_event = *do_note_off;
        mdi->events[mdi->event_count].event_data.channel = channel;
        mdi->events[mdi->event_count].event_data.data = (note << 8) | velocity;
        mdi->events[mdi->event_count].samples_to_next = 0;
        mdi->event_count++;
    }
    return 0; 
}

int
midi_setup_noteon (struct _mdi *mdi, unsigned char channel, unsigned char note, unsigned char velocity)
{
    if ((mdi->event_count) && (mdi->events[mdi->event_count - 1].do_event == NULL))
    {
        mdi->events[mdi->event_count - 1].do_event = *do_note_on;
        mdi->events[mdi->event_count - 1].event_data.channel = channel;
        mdi->events[mdi->event_count - 1].event_data.data = (note << 8) | velocity;         
    } else {
        mdi->events = realloc(mdi->events,((mdi->event_count + 1) * sizeof(struct _event)));
        mdi->events[mdi->event_count].do_event = *do_note_on;
        mdi->events[mdi->event_count].event_data.channel = channel;
        mdi->events[mdi->event_count].event_data.data = (note << 8) | velocity;
        mdi->events[mdi->event_count].samples_to_next = 0;
        mdi->event_count++;
    }

    if ((lin_volume[mdi->channel[channel].volume] * lin_volume[velocity]) > mdi->lin_max_vol)
        mdi->lin_max_vol = lin_volume[mdi->channel[channel].volume] * lin_volume[velocity];   	

    if ((log_volume[mdi->channel[channel].volume] * sqr_volume[velocity]) > mdi->log_max_vol)
        mdi->log_max_vol = log_volume[mdi->channel[channel].volume] * sqr_volume[velocity];   	
    
    if (channel == 9)
        load_patch(mdi, ((mdi->channel[channel].bank << 8) | (note | 0x80)));
    return 0; 
}

int
midi_setup_aftertouch (struct _mdi *mdi, unsigned char channel, unsigned char note, unsigned char pressure)
{
    if ((mdi->event_count) && (mdi->events[mdi->event_count - 1].do_event == NULL))
    {
        mdi->events[mdi->event_count - 1].do_event = *do_aftertouch;
        mdi->events[mdi->event_count - 1].event_data.channel = channel;
        mdi->events[mdi->event_count - 1].event_data.data = (note << 8) | pressure;        
    } else {
        mdi->events = realloc(mdi->events,((mdi->event_count + 1) * sizeof(struct _event)));
        mdi->events[mdi->event_count].do_event = *do_aftertouch;
        mdi->events[mdi->event_count].event_data.channel = channel;
        mdi->events[mdi->event_count].event_data.data = (note << 8) | pressure;
        mdi->events[mdi->event_count].samples_to_next = 0;
        mdi->event_count++;
    }
    return 0; 
}

int
midi_setup_control (struct _mdi *mdi, unsigned char channel, unsigned char controller, unsigned char setting)
{
    void (*tmp_event)(struct _mdi *mdi, struct _event_data *data) = NULL;
    
    switch(controller)
    {
        case 0:
            tmp_event = *do_control_bank_select;
            mdi->channel[channel].bank = setting;
        break;
        case 6:
            tmp_event = *do_control_data_entry_course;
            break;
        case 7:
            tmp_event = *do_control_channel_volume;
            mdi->channel[channel].volume = setting;
            break;
        case 8:
            tmp_event = *do_control_channel_balance;
            break;
        case 10:
            tmp_event = *do_control_channel_pan;
            break;
        case 11:
            tmp_event = *do_control_channel_expression;
            break;
        case 38:
            tmp_event = *do_control_data_entry_fine;
            break;
        case 64:
            tmp_event = *do_control_channel_hold;
            break;
        case 100:
            tmp_event = *do_control_registered_param_fine;
            break;
        case 101:
            tmp_event = *do_control_registered_param_course;
            break;
        case 120:
            tmp_event = *do_control_channel_sound_off;
            break;
        case 121:
            tmp_event = *do_control_channel_controllers_off;
            break;
        case 123:
            tmp_event = *do_control_channel_notes_off;
            break;
        default:
            return 0;
    }
    if ((mdi->event_count) && (mdi->events[mdi->event_count - 1].do_event == NULL))
    {
        mdi->events[mdi->event_count - 1].do_event = tmp_event;
        mdi->events[mdi->event_count - 1].event_data.channel = channel;
        mdi->events[mdi->event_count - 1].event_data.data = setting;
    } else {
        mdi->events = realloc(mdi->events,((mdi->event_count + 1) * sizeof(struct _event)));
        mdi->events[mdi->event_count].do_event = tmp_event;
        mdi->events[mdi->event_count].event_data.channel = channel;
        mdi->events[mdi->event_count].event_data.data = setting;
        mdi->events[mdi->event_count].samples_to_next = 0;
        mdi->event_count++;
    }
    return 0; 
}

int
midi_setup_patch (struct _mdi *mdi, unsigned char channel, unsigned char patch)
{
    if ((mdi->event_count) && (mdi->events[mdi->event_count - 1].do_event == NULL))
    {
        mdi->events[mdi->event_count - 1].do_event = *do_patch;
        mdi->events[mdi->event_count - 1].event_data.channel = channel;
        mdi->events[mdi->event_count - 1].event_data.data = patch;
    } else {
        mdi->events = realloc(mdi->events,((mdi->event_count + 1) * sizeof(struct _event)));
        mdi->events[mdi->event_count].do_event = *do_patch;
        mdi->events[mdi->event_count].event_data.channel = channel;
        mdi->events[mdi->event_count].event_data.data = patch;
        mdi->events[mdi->event_count].samples_to_next = 0;
        mdi->event_count++;
    }
    if (channel == 9)
    {
        mdi->channel[channel].bank = patch;
    } else {
        load_patch(mdi, ((mdi->channel[channel].bank << 8) | patch));
    }
    return 0; 
}

int
midi_setup_channel_pressure (struct _mdi *mdi, unsigned char channel, unsigned char pressure)
{
    
    if ((mdi->event_count) && (mdi->events[mdi->event_count - 1].do_event == NULL))
    {
        mdi->events[mdi->event_count - 1].do_event = *do_channel_pressure;
        mdi->events[mdi->event_count - 1].event_data.channel = channel;
        mdi->events[mdi->event_count - 1].event_data.data = pressure;
    } else {
        mdi->events = realloc(mdi->events,((mdi->event_count + 1) * sizeof(struct _event)));
        mdi->events[mdi->event_count].do_event = *do_channel_pressure;
        mdi->events[mdi->event_count].event_data.channel = channel;
        mdi->events[mdi->event_count].event_data.data = pressure;
        mdi->events[mdi->event_count].samples_to_next = 0;
        mdi->event_count++;
    }

    return 0; 
}

int
midi_setup_pitch (struct _mdi *mdi, unsigned char channel, unsigned short pitch)
{
    if ((mdi->event_count) && (mdi->events[mdi->event_count - 1].do_event == NULL))
    {
        mdi->events[mdi->event_count - 1].do_event = *do_pitch;
        mdi->events[mdi->event_count - 1].event_data.channel = channel;
        mdi->events[mdi->event_count - 1].event_data.data = pitch;
    } else {
        mdi->events = realloc(mdi->events,((mdi->event_count + 1) * sizeof(struct _event)));
        mdi->events[mdi->event_count].do_event = *do_pitch;
        mdi->events[mdi->event_count].event_data.channel = channel;
        mdi->events[mdi->event_count].event_data.data = pitch;
        mdi->events[mdi->event_count].samples_to_next = 0;
        mdi->event_count++;
    }
    return 0; 
}

struct _event {
	unsigned char type;
	unsigned char channel;
	unsigned long int data;
	unsigned long int samples_to_next;
};

struct _mdi {
	struct _event *events;
};

inline struct _mdi *
parse_midi (unsigned char *midi_data, unsigned int midi_size, int rate) {
	unsigned char *midi_data_start = midi_data;
	unsigned long int tmp_val = 0;
	unsigned short int no_tracks = 0;
	unsigned int divisions = 96;
    unsigned int tempo = 500000;
    unsigned int samples_per_delta = 0;
	unsigned int track_size = 0;
	unsigned int track_sample_cnt = 0;
	unsigned int event_sample_cnt = 0;
	struct _event *event = NULL;
	struct _event *tmp_event = NULL;
	struct _event *first_event = NULL;
	int i = 0;
	
	/* we jump over RIFF headers */
	if (strncmp(midi_data,"RIFF",4) == 0)
    {
		midi_data += 20;
		midi_size -= 20;
    }
	
	
	if (strncmp(midi_data,"MThd",4) != 0) {
        printf("Not a midi file\n");
		return NULL;
	}
	midi_data += 4;
	midi_size -= 4;

    if (midi_size < 10)
    {
        printf("Midi File Too Short\n");
		return NULL;
    }

	/*
		Get Midi Header Size - must always be 6
	*/	
	tmp_val = *midi_data++ << 24;
	tmp_val |= *midi_data++ << 16;
	tmp_val |= *midi_data++ << 8;
	tmp_val |= *midi_data++;
    midi_size -= 4;
    if (tmp_val != 6)
    {
        printf("Corrupt Midi Header\n");
        free (mdi);
        return NULL;
    }

	/*
		Get Midi Format - we only support 0 and 1
	*/
    tmp_val = *midi_data++ << 8;
	tmp_val |= *midi_data++;
    midi_size -= 2;  
    if (tmp_val > 1)
    {
        printf("Midi Format Not Supported\n");
        free (mdi);
        return NULL;
    }
	
	/*
		Get No. of Tracks
	*/
    tmp_val = *midi_data++ << 8;
	tmp_val |= *midi_data++;
    midi_size -= 2;
    if (tmp_val < 1)
    {
        printf("Midi Contains No Tracks\n");
        return NULL;    
    }
    no_tracks = tmp_val;
	
	divisions = *midi_data++ << 8;
	divisions |= *midi_data++;
    midi_size -= 2;
    if (divisions & 0x00008000)
    {
        printf("Division Type Note Supported\n");
        return NULL;            
    }
	
	samples_per_delta = (rate << 10) / ((1000000 * divisions) / tempo);
		
	for (i = 0; i < no_tracks; i++)
    {
        /*
			check there is enough data to read
		*/
		if (midi_size < 11)
        {
            printf("Midi File Too Short\n");
    		return NULL;        
        }
        
        if (strncmp(midi_data,"MTrk",4) != 0) {
            printf("Expected Track Header\n");
    		return NULL;
    	}
    	midi_data += 4;
    	midi_size -= 4;
        
        track_size = *midi_data++ << 24;
        track_size |= *midi_data++ << 16;
        track_size |= *midi_data++ << 8;
        track_size |= *midi_data++;
        midi_size -= 4;
        if (midi_size < track_size)
        {
            printf("Midi File Too Short\n");
    		return NULL;        
        }
        if ((midi_data[track_size-3] != 0xFF) || (midi_data[track_size-2] != 0x2F) || (midi_data[track_size-1] != 0x00))
        {
            printf("Corrupt Midi, Expected EOT\n");
            return NULL;
        }
		
		next_track = midi_data + track_size;
		events = first_event;
		track_sample_cnt = 0;
		event_sample_cnt = 0;
		running_event = 0;
		current_event = 0;
		do {
			if (*midi_data > 0x7F) {
				current_event = *midi_data;
				midi_data++;
			} else {
				current_event = running_event;
			}
			switch (current_event >> 4) {
				case 0x8: // Note Off
					if (event != NULL) {
						if (event->next != NULL) {
							tmp_event = event_next;
							event->next = malloc(sizeof(struct _event));
							event = event->next;
							event->next = tmp_event;
						} else {
							event->next = malloc(sizeof(struct _event));
							event = event>next;
							event->next = NULL;
						}
					} else {
						first_event = malloc(sizeof(struct _event));
						event = first_event;
						event->next = NULL;
					}
					event->type = MIDI_NOTEOFF;
					event->channel = current_event &0x0F;
					event->data = (*midi_data << 8) | *(midi_data+1);
					
					midi_data += 2;
					break;

				case 0x9:
					if (event != NULL) {
						if (event->next != NULL) {
							tmp_event = event_next;
							event->next = malloc(sizeof(struct _event));
							event = event->next;
							event->next = tmp_event;
						} else {
							event->next = malloc(sizeof(struct _event));
							event = event>next;
							event->next = NULL;
						}
					} else {
						first_event = malloc(sizeof(struct _event));
						event = first_event;
						event->next = NULL;
					}
					event->type = MIDI_NOTEON;
					event->channel = current_event &0x0F;
					event->data = (*midi_data << 8) | *(midi_data+1);
					
					midi_data += 2;
					break;
				case 0xA:
					if (event != NULL) {
						if (event->next != NULL) {
							tmp_event = event_next;
							event->next = malloc(sizeof(struct _event));
							event = event->next;
							event->next = tmp_event;
						} else {
							event->next = malloc(sizeof(struct _event));
							event = event>next;
							event->next = NULL;
						}
					} else {
						first_event = malloc(sizeof(struct _event));
						event = first_event;
						event->next = NULL;
					}
					event->type = MIDI_AFTERTOUCH;
					event->channel = current_event &0x0F;
					event->data = (*midi_data << 8) | *(midi_data+1);
					
					midi_data += 2;
					break;

				case 0xB:
					if (event != NULL) {
						if (event->next != NULL) {
							tmp_event = event_next;
							event->next = malloc(sizeof(struct _event));
							event = event->next;
							event->next = tmp_event;
						} else {
							event->next = malloc(sizeof(struct _event));
							event = event>next;
							event->next = NULL;
						}
					} else {
						first_event = malloc(sizeof(struct _event));
						event = first_event;
						event->next = NULL;
					}
					event->type = MIDI_CONTROL;
					event->channel = current_event &0x0F;
					event->data = (*midi_data << 8) | *(midi_data+1);
					
					midi_data += 2;
					break;
				
				case 0xC:
					if (event != NULL) {
						if (event->next != NULL) {
							tmp_event = event_next;
							event->next = malloc(sizeof(struct _event));
							event = event->next;
							event->next = tmp_event;
						} else {
							event->next = malloc(sizeof(struct _event));
							event = event>next;
							event->next = NULL;
						}
					} else {
						first_event = malloc(sizeof(struct _event));
						event = first_event;
						event->next = NULL;
					}
					event->type = MIDI_PATCH;
					event->channel = current_event &0x0F;
					event->data = *midi_data;
					midi_data++;
					break;

				case 0xD:
					if (event != NULL) {
						if (event->next != NULL) {
							tmp_event = event_next;
							event->next = malloc(sizeof(struct _event));
							event = event->next;
							event->next = tmp_event;
						} else {
							event->next = malloc(sizeof(struct _event));
							event = event>next;
							event->next = NULL;
						}
					} else {
						first_event = malloc(sizeof(struct _event));
						event = first_event;
						event->next = NULL;
					}
					event->type = MIDI_PRESSURE;
					event->channel = current_event &0x0F;
					event->data = *midi_data;
					midi_data++;

				case 0xE:
					if (event != NULL) {
						if (event->next != NULL) {
							tmp_event = event_next;
							event->next = malloc(sizeof(struct _event));
							event = event->next;
							event->next = tmp_event;
						} else {
							event->next = malloc(sizeof(struct _event));
							event = event>next;
							event->next = NULL;
						}
					} else {
						first_event = malloc(sizeof(struct _event));
						event = first_event;
						event->next = NULL;
					}
					event->type = MIDI_PITCH;
					event->channel = current_event &0x0F;
					event->data = (*midi_data << 8) | *(midi_data+1);
					
					midi_data += 2;
					break;

                case 0xF:
                    if (current_event == 0xFF) {
                        if ((*midi_data == 0x2F) && (*(midi_data + 1) == 0x00)) {
							midi_data = next_track;
                        } else if ((*midi_data == 0x51) && (*(midi_data + 1) == 0x03)) {
                            tempo = (*(midi_data + 2) << 16) + (*(midi_data + 3) << 8) + *(midi_data + 4);
                            midi_data += 5;
                            if (!tempo) {
                                samples_per_delta = (rate << 10) / (2 * divisions);
                            } else {
                                samples_per_delta = (rate << 10) / ((1000000 * divisions) / tempo);
							}
                        } else {
                                tmp_length = 0;
                                tracks[i]++;
                                while (*tracks[i] > 0x7f)
                                {
                                    tmp_length = (tmp_length << 7) + (*tracks[i] & 0x7f);
                                    tracks[i]++;
                                }
                                tmp_length = (tmp_length << 7) + (*tracks[i] & 0x7f);
                                tracks[i] += tmp_length + 1;
                            }
                        } else if (current_event == 0xF0)
                        {
                            running_event[i] = 0;
                            while (*tracks[i] != 0xF7)
                            {
                                tracks[i]++;
                            }
                            tracks[i]++;
                        } else {
                            printf("Um, WTF is this?\n");
                            free (tracks);
                            free (track_end);
                            free (track_delta);
                            if (mdi->events)
                                free (mdi->events);
                            free (mdi);
                            return NULL;
                        }
                        break;
                    default:
                        printf("Should Never of Gotten Here\n");
                        free (tracks);
                        free (track_end);
                        free (track_delta);
                        if (mdi->events)
                            free (mdi->events);
                        free (mdi);
                        return NULL;
			}
		} while (midi_data < next_track);
    }
}

struct _mdi *
WM_ParseNewMidi (unsigned char *midi_data, unsigned int midi_size)
{
    struct _mdi *mdi;
    unsigned int tmp_val;
    unsigned int track_size;
    unsigned char **tracks;
    unsigned int end_of_tracks = 0;
    unsigned int no_tracks;
    unsigned int i;
    unsigned int divisions = 96;
    unsigned int tempo = 500000;
    unsigned int samples_per_delta = 0;
    unsigned long int sample_count = 0;
    unsigned long int sample_remainder = 0;
    
    struct _hndl *tmp_handle = NULL;

    unsigned long int *track_delta;
    unsigned char *track_end;
    unsigned long int smallest_delta = 0;
    unsigned long int subtract_delta = 0;
    unsigned long int tmp_length = 0;
    unsigned char current_event;
    unsigned char *running_event;


    mdi = malloc(sizeof (struct _mdi));
    memset(mdi, 0, (sizeof(struct _mdi)));

    if (first_handle == NULL) {
		first_handle = malloc(sizeof(struct _hndl));
		if (first_handle == NULL) {
			WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_MEM," to parse midi data", errno);
			free(mdi);
			return NULL;
		}
		first_handle->handle = (void *)mdi;
		first_handle->prev = NULL;
		first_handle->next = NULL;
	} else {
		tmp_handle = first_handle;
		if (tmp_handle->next != NULL) {
			while (tmp_handle->next != NULL)
				tmp_handle = tmp_handle->next;
		}
		tmp_handle->next = malloc(sizeof(struct _hndl));
		if (tmp_handle->next == NULL) {
			WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_MEM," to parse midi data", errno);
			free(mdi);
			return NULL;
		}
		tmp_handle->next->prev = tmp_handle;
		tmp_handle = tmp_handle->next;
		tmp_handle->next = NULL;
		tmp_handle->handle = (void *)mdi;
	}
    mdi->info.mixer_options = WM_MixerOptions;
    
   	load_patch(mdi, 0x0000);
	
	for (i=0; i<16; i++) {
		mdi->channel[i].volume = 100;
		mdi->channel[i].pressure = 127;
		mdi->channel[i].expression = 127;
		mdi->channel[i].pitch_range = 200;
		mdi->channel[i].reg_data = 0xFFFF;
		mdi->channel[i].patch = get_patch_data(mdi, 0x0000);
	}

    
    if (strncmp(midi_data,"RIFF",4) == 0)
    {
		midi_data += 20;
		midi_size -= 20;
    }	
    if (strncmp(midi_data,"MThd",4) != 0) {
        printf("Not a midi file\n");
		free(mdi);
		return NULL;
	}
	midi_data += 4;
	midi_size -= 4;

    if (midi_size < 10)
    {
        printf("Midi File Too Short\n");
		free(mdi);
		return NULL;
    }

/*
 * Get Midi Header Size - must always be 6
 */	
	tmp_val = *midi_data++ << 24;
	tmp_val |= *midi_data++ << 16;
	tmp_val |= *midi_data++ << 8;
	tmp_val |= *midi_data++;
    midi_size -= 4;
    if (tmp_val != 6)
    {
        printf("Corrupt Midi Header\n");
        free (mdi);
        return NULL;
    }

/*
 * Get Midi Format - we only support 0 and 1
 */
    tmp_val = *midi_data++ << 8;
	tmp_val |= *midi_data++;
    midi_size -= 2;  
    if (tmp_val > 1)
    {
        printf("Midi Format Not Supported\n");
        free (mdi);
        return NULL;
    }

/*
 * Get No. of Tracks
 */
    tmp_val = *midi_data++ << 8;
	tmp_val |= *midi_data++;
    midi_size -= 2;
    if (tmp_val < 1)
    {
        printf("Midi Contains No Tracks\n");
        free (mdi);
        return NULL;    
    }
    no_tracks = tmp_val;
    
/*
 * Get Divisions
 */
    divisions = *midi_data++ << 8;
	divisions |= *midi_data++;
    midi_size -= 2;
    if (divisions & 0x00008000)
    {
        printf("Division Type Note Supported\n");
        free (mdi);
        return NULL;            
    }

    samples_per_delta = (WM_SampleRate << 10) / ((1000000 * divisions) / tempo);
    tracks = malloc (sizeof(char *) * no_tracks);
    track_delta = malloc (sizeof(unsigned long int) * no_tracks);
    track_end = malloc (sizeof(unsigned char) * no_tracks);
    running_event = malloc (sizeof(unsigned char) * no_tracks);
    smallest_delta = 0;

    for (i = 0; i < no_tracks; i++)
    {
        if (midi_size < 8)
        {
            printf("Midi File Too Short\n");
            free(tracks);
    		free(mdi);
    		return NULL;        
        }
        
        if (strncmp(midi_data,"MTrk",4) != 0) {
            printf("Expected Track Header\n");
            free(tracks);
    		free(mdi);
    		return NULL;
    	}
    	midi_data += 4;
    	midi_size -= 4;
        
        track_size = *midi_data++ << 24;
        track_size |= *midi_data++ << 16;
        track_size |= *midi_data++ << 8;
        track_size |= *midi_data++;
        midi_size -= 4;
        if (midi_size < track_size)
        {
            printf("Midi File Too Short\n");
            free(tracks);
    		free(mdi);
    		return NULL;        
        }
        if ((midi_data[track_size-3] != 0xFF) || (midi_data[track_size-2] != 0x2F) || (midi_data[track_size-1] != 0x00))
        {
            printf("Corrupt Midi, Expected EOT\n");
            free(tracks);
            free(mdi);
            return NULL;
        }
        tracks[i] = midi_data;
        midi_data += track_size;
        midi_size -= track_size;
        track_end[i] = 0;
        running_event[i] = 0;
        track_delta[i] = 0;
        while (*tracks[i] > 0x7F)
        {
              track_delta[i] = (track_delta[i] << 7) + (*tracks[i] & 0x7F);
              tracks[i]++;
        }
        track_delta[i] = (track_delta[i] << 7) + (*tracks[i] & 0x7F);
        tracks[i]++;
    }
    
    while (end_of_tracks != no_tracks)
    {
        smallest_delta = 0;  
        for (i = 0; i < no_tracks; i++)
        {
            if (track_end[i])
                continue;
                
            if (track_delta[i])
            {
                track_delta[i] -= subtract_delta;
                if (track_delta[i])
                {
                    if ((!smallest_delta) || (smallest_delta > track_delta[i]))
                    {
                        smallest_delta = track_delta[i];
                    }
                    continue;
                }
            }

            do {            
                if (*tracks[i] > 0x7F)
                {
                    current_event = *tracks[i];
                    tracks[i]++;
                } else {
                    current_event = running_event[i];
                    if (running_event[i] < 0x80)
                    {
                        printf("Invalid Data in Midi, Expected Event\n");
                        free (tracks);
                        free (track_end);
                        free (track_delta);
                        if (mdi->events)
                            free (mdi->events);
                        free (mdi);
                        return NULL;
                    }
                }
                switch (current_event >> 4)
                {
                    case 0x8:
                        midi_setup_noteoff (mdi, (current_event & 0x0F), tracks[i][0], tracks[i][1]);
                        tracks[i] += 2;
                        running_event[i] = current_event;
                        break;
                    case 0x9:
                        midi_setup_noteon (mdi, (current_event & 0x0F), tracks[i][0], tracks[i][1]);
                        tracks[i] += 2;
                        running_event[i] = current_event;
                        break;
                    case 0xA:
                        midi_setup_aftertouch (mdi, (current_event & 0x0F), tracks[i][0], tracks[i][1]);
                        tracks[i] += 2;
                        running_event[i] = current_event;
                        break;
                    case 0xB:
                        midi_setup_control (mdi, (current_event & 0x0F), tracks[i][0], tracks[i][1]);
                        tracks[i] += 2;
                        running_event[i] = current_event;
                        break;
                    case 0xC:
                        midi_setup_patch (mdi,  (current_event & 0x0F), *tracks[i]);
                        tracks[i]++;
                        running_event[i] = current_event;
                        break;
                    case 0xD:
                        midi_setup_channel_pressure (mdi,  (current_event & 0x0F), *tracks[i]);
                        tracks[i]++;
                        running_event[i] = current_event;
                        break;
                    case 0xE:
                        midi_setup_pitch (mdi, (current_event & 0x0F), ((tracks[i][1] << 7) | (tracks[i][0] & 0x7F)));
                        tracks[i] += 2;
                        running_event[i] = current_event;
                        break;
                    case 0xF:
                        if (current_event == 0xFF)
                        {
                            if ((tracks[i][0] == 0x2F) && (tracks[i][1] == 0x00))
                            {
                                end_of_tracks++;
                                track_end[i] = 1;
                                goto NEXT_TRACK;
                            } else if ((tracks[i][0] == 0x51) && (tracks[i][1] == 0x03))
                            {
                                tempo = (tracks[i][2] << 16) + (tracks[i][3] << 8) + tracks[i][4];
                                tracks[i] += 5;
                                if (!tempo)
                                    samples_per_delta = (WM_SampleRate << 10) / (2 * divisions);
                                else
                                    samples_per_delta = (WM_SampleRate << 10) / ((1000000 * divisions) / tempo);
                            } else {
                                tmp_length = 0;
                                tracks[i]++;
                                while (*tracks[i] > 0x7f)
                                {
                                    tmp_length = (tmp_length << 7) + (*tracks[i] & 0x7f);
                                    tracks[i]++;
                                }
                                tmp_length = (tmp_length << 7) + (*tracks[i] & 0x7f);
                                tracks[i] += tmp_length + 1;
                            }
                        } else if (current_event == 0xF0)
                        {
                            running_event[i] = 0;
                            while (*tracks[i] != 0xF7)
                            {
                                tracks[i]++;
                            }
                            tracks[i]++;
                        } else {
                            printf("Um, WTF is this?\n");
                            free (tracks);
                            free (track_end);
                            free (track_delta);
                            if (mdi->events)
                                free (mdi->events);
                            free (mdi);
                            return NULL;
                        }
                        break;
                    default:
                        printf("Should Never of Gotten Here\n");
                        free (tracks);
                        free (track_end);
                        free (track_delta);
                        if (mdi->events)
                            free (mdi->events);
                        free (mdi);
                        return NULL;
                }
                while (*tracks[i] > 0x7F)
                {
                    track_delta[i] = (track_delta[i] << 7) + (*tracks[i] &0x7F);
                    tracks[i]++;
                }
                track_delta[i] = (track_delta[i] << 7) + (*tracks[i] &0x7F);
                tracks[i]++;
            } while (!track_delta[i]);
            if ((!smallest_delta) || (smallest_delta > track_delta[i]))
            {
                smallest_delta = track_delta[i];
            }
            NEXT_TRACK:
            continue;
        }
        
        subtract_delta = smallest_delta;
        sample_count = ((smallest_delta * samples_per_delta) + sample_remainder) >> 10;
        sample_remainder = sample_count & 0x3FF;
        if ((mdi->event_count) && (mdi->events[mdi->event_count - 1].do_event == NULL))
        {
            mdi->events[mdi->event_count - 1].samples_to_next += sample_count;
        } else {
            mdi->events = realloc(mdi->events,((mdi->event_count + 1) * sizeof(struct _event)));
            mdi->events[mdi->event_count].do_event = NULL;
            mdi->events[mdi->event_count].event_data.channel = 0;
            mdi->events[mdi->event_count].event_data.data = 0;
            mdi->events[mdi->event_count].samples_to_next = sample_count;
            mdi->event_count++;
        }
        mdi->info.approx_total_samples += sample_count; 
    }
    if ((mdi->event_count) && (mdi->events[mdi->event_count - 1].do_event == NULL))
    {
        mdi->info.approx_total_samples -= mdi->events[mdi->event_count - 1].samples_to_next;
        mdi->event_count--;
    }
    mdi->info.approx_total_samples += WM_SampleRate * 3;
    init_reverb(&mdi->reverb);
    mdi->info.current_sample = 0;
    mdi->current_event = &mdi->events[0];
    mdi->samples_to_mix = 0;
    mdi->last_note = mdi->note;

	if (mdi->info.mixer_options & WM_MO_LOG_VOLUME)
    {
		mdi->amp = (281 * ((mdi->lin_max_vol << 10) / mdi->log_max_vol)) >> 10;
	} else {
		mdi->amp = 281;
	}
    mdi->amp = (mdi->amp * (((lin_volume[127] * lin_volume[127]) << 10) / mdi->lin_max_vol)) >> 10;
    
    WM_ResetToStart(mdi);

    free (track_end);
    free (track_delta);
    free (tracks);
    return mdi;
}
