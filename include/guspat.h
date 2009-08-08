/*
    guspat.h - guspat processing
    Copyright (C) 2001-2009 Chris Ison

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

    Email: wildcode@users.sourceforge.net
*/

#ifndef GUSPAT_H
#define GUSPAT_H

struct _env {
	float time;
	float level;
	unsigned char set;
};



struct _gus_sample_data {
	unsigned long int data_length;
	unsigned long int loop_start;
	unsigned long int loop_end;
	unsigned long int loop_size;
	unsigned char loop_fraction;
	unsigned short int rate;
	unsigned long int freq_low;
	unsigned long int freq_high;
	unsigned long int freq_root;
	unsigned char modes;
	unsigned long int env_rate[7];
	unsigned long int env_target[7];
	unsigned long int inc_div;
	signed short *data;
}

#endif // GUSPAT_H