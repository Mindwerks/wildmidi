/*
	MUS2MIDI: DMX (DOOM) MUS to MIDI Library

	Copyright (C) 2014  Bret Curtis

	This library is free software; you can redistribute it and/or
	modify it under the terms of the GNU Library General Public
	License as published by the Free Software Foundation; either
	version 2 of the License, or (at your option) any later version.

	This library is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
	Library General Public License for more details.

	You should have received a copy of the GNU Library General Public
	License along with this library; if not, write to the
	Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
	Boston, MA  02110-1301, USA.
*/

#include <stddef.h>
#include <stdlib.h>
#include "mus.h"
#include "wm_error.h"

struct mus_ctx {
	uint8_t *src, *src_ptr;
	uint32_t srcsize;
	uint32_t datastart;
	uint8_t *dst, *dst_ptr;
	uint32_t dstsize, dstrem;
};

uint8_t *mus_getmididata(struct mus_ctx *ctx){
	return ctx->dst;
}

uint32_t mus_getmidisize(struct mus_ctx *ctx){
	return ctx->dstsize - ctx->dstrem;
}

void mus_free(struct mus_ctx *ctx){
	if (!ctx) return;
	free(ctx->dst);
	free(ctx);
}

struct mus_ctx *mus2midi(uint8_t *data, uint32_t size){

}
