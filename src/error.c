/*
    error.c - Error handling
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
 
     $Id: error.c,v 1.8 2008/05/23 13:22:31 wildcode Exp $
*/

#include <stdio.h>
#include <string.h>

#include "error.h"

/*
	WM_EEROR - Internal Error Reporting
	
	inline void WM_ERROR( const char *func, unsigned long int lne, int wmerno, const char *wmfor, int error)

	func		the name of the function the error occured
	lne		the line number where WM_ERROR was called
	wmerno	the internal error number
	wmfor	the text to accompany the error message
	error		the error number reported by the system
	
	wmerno values
	0 	Unable to Obtain Memory		An attempt to obtain required memory has failed
	1 	Unable To Stat				An attempt to get information about a file has failed
	2 	Unable To Load				An attempt to load a file has failed
	3	Unable To Open				An attempt to open a file has failed
	4	Unable To Read				An attempt to read a file has failed
	5	Invalid or Unsupported File Format	A file contained an unrecognized format
	6	File Corrupt					A file contained corrupt data
	7	Invalid Argument				A function was called with invalid information
	8	Library Not Initialized			A function winin the library was called before the Library was initialized
	9	Library Already Initialized		The library has already been initialized
	
	Returns Nothing
*/

inline void
WM_ERROR( const char *func, unsigned long int lne, int wmerno, const char *wmfor, int error) {
	const char *errors[] = {
		"Unable to obtain memory\0",
		"Unable to stat\0",
		"Unable to load\0",
		"Unable to open\0",
		"Unable to read\0",
		"Invalid or Unsuported file format\0",
		"File corrupt\0",
		"Invalid argument\0",
		"Library not Initialized\0",
		"Library Already Initialized\0"
	};
	
	/*
		Output the error information
	*/
	if (wmfor != NULL) {
		if (error != 0) {
			fprintf(stderr,"\rlibWildMidi(%s:%lu): ERROR %s %s (%s)\n",func, lne, errors[wmerno], wmfor, strerror(error));
		} else {
			fprintf(stderr,"\rlibWildMidi(%s:%lu): ERROR %s %s\n",func, lne, errors[wmerno], wmfor);
		}
	} else {
		if (error != 0) {
			fprintf(stderr,"\rlibWildMidi(%s:%lu): ERROR %s (%s)\n",func, lne, errors[wmerno], strerror(error));
		} else {
			fprintf(stderr,"\rlibWildMidi(%s:%lu): ERROR %s\n",func, lne, errors[wmerno]);
		}
	}
}
