/*
    reverb.h - reverb handling
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

	$Id: reverb.h,v 1.2 2008/06/05 06:06:22 wildcode Exp $
*/

#ifndef _REVERB_H
#define _REVERB_H

struct _rvb {
	/* filter data */
	signed long int l_buf_flt_in[6][2];
	signed long int l_buf_flt_out[6][2];
	signed long int r_buf_flt_in[6][2];
	signed long int r_buf_flt_out[6][2];
	signed long int coeff[6][5];
	/* buffer data */
	signed long int *l_buf;
	signed long int *r_buf;
	int l_buf_size;
	int r_buf_size;
	int l_out;
	int r_out;
	int l_sp_in[8];
	int r_sp_in[8];
	int l_in[4];
	int r_in[4];
	int gain;
};

inline void reset_reverb (struct _rvb *rvb);
inline struct _rvb *init_reverb(int rate);
inline void free_reverb (struct _rvb *rvb);
inline void do_reverb (struct _rvb *rvb, signed long int *buffer, int size);

#endif // _REVERB_H
