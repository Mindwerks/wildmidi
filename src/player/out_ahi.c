/*
 * out_ahi.c -- Amiga AHI output
 *
 * Copyright (C) WildMidi Developers 2020
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

#if (AUDIODRV_AHI == 1)

/* Driver for output to native Amiga AHI device:
 * Written by Szilárd Biró <col.lawrence@gmail.com>, loosely based
 * on an old AOS4 version by Fredrik Wikstrom <fredrik@a500.org>
 */

#define BUFFERSIZE (4 << 10)

static struct MsgPort *AHImp = NULL;
static struct AHIRequest *AHIReq[2] = { NULL, NULL };
static int active = 0;
static int8_t *AHIBuf[2] = { NULL, NULL };

int open_ahi_output(void) {
    AHImp = CreateMsgPort();
    if (AHImp) {
        AHIReq[0] = (struct AHIRequest *) CreateIORequest(AHImp, sizeof(struct AHIRequest));
        if (AHIReq[0]) {
            AHIReq[0]->ahir_Version = 4;
            AHIReq[1] = (struct AHIRequest *) AllocVec(sizeof(struct AHIRequest), SHAREDMEMFLAG);
            if (AHIReq[1]) {
                if (!OpenDevice(AHINAME, AHI_DEFAULT_UNIT, (struct IORequest *)AHIReq[0], 0)) {
                    /*AHIReq[0]->ahir_Std.io_Message.mn_Node.ln_Pri = 0;*/
                    AHIReq[0]->ahir_Std.io_Command = CMD_WRITE;
                    AHIReq[0]->ahir_Std.io_Data = NULL;
                    AHIReq[0]->ahir_Std.io_Offset = 0;
                    AHIReq[0]->ahir_Frequency = rate;
                    AHIReq[0]->ahir_Type = AHIST_S16S;/* 16 bit stereo */
                    AHIReq[0]->ahir_Volume = 0x10000;
                    AHIReq[0]->ahir_Position = 0x8000;
                    CopyMem(AHIReq[0], AHIReq[1], sizeof(struct AHIRequest));

                    AHIBuf[0] = (int8_t *) AllocVec(BUFFERSIZE, SHAREDMEMFLAG | MEMF_CLEAR);
                    if (AHIBuf[0]) {
                        AHIBuf[1] = (int8_t *) AllocVec(BUFFERSIZE, SHAREDMEMFLAG | MEMF_CLEAR);
                        if (AHIBuf[1]) {
                            return (0);
                        }
                    }
                }
            }
        }
    }

    close_ahi_output();
    fprintf(stderr, "ERROR: Unable to open AHI output\r\n");
    return (-1);
}

int write_ahi_output(int8_t *output_data, int output_size) {
    int chunk;
    while (output_size > 0) {
        if (AHIReq[active]->ahir_Std.io_Data) {
            WaitIO((struct IORequest *) AHIReq[active]);
        }
        chunk = (output_size < BUFFERSIZE)? output_size : BUFFERSIZE;
        memcpy(AHIBuf[active], output_data, chunk);
        output_size -= chunk;
        output_data += chunk;

        AHIReq[active]->ahir_Std.io_Data = AHIBuf[active];
        AHIReq[active]->ahir_Std.io_Length = chunk;
        AHIReq[active]->ahir_Link = !CheckIO((struct IORequest *) AHIReq[active ^ 1]) ? AHIReq[active ^ 1] : NULL;
        SendIO((struct IORequest *)AHIReq[active]);
        active ^= 1;
    }
    return (0);
}

void close_ahi_output(void) {
    if (AHIReq[1]) {
        AHIReq[0]->ahir_Link = NULL; /* in case we are linked to req[0] */
        if (!CheckIO((struct IORequest *) AHIReq[1])) {
            AbortIO((struct IORequest *) AHIReq[1]);
            WaitIO((struct IORequest *) AHIReq[1]);
        }
        FreeVec(AHIReq[1]);
        AHIReq[1] = NULL;
    }
    if (AHIReq[0]) {
        if (!CheckIO((struct IORequest *) AHIReq[0])) {
            AbortIO((struct IORequest *) AHIReq[0]);
            WaitIO((struct IORequest *) AHIReq[0]);
        }
        if (AHIReq[0]->ahir_Std.io_Device) {
            CloseDevice((struct IORequest *) AHIReq[0]);
            AHIReq[0]->ahir_Std.io_Device = NULL;
        }
        DeleteIORequest((struct IORequest *) AHIReq[0]);
        AHIReq[0] = NULL;
    }
    if (AHImp) {
        DeleteMsgPort(AHImp);
        AHImp = NULL;
    }
    if (AHIBuf[0]) {
        FreeVec(AHIBuf[0]);
        AHIBuf[0] = NULL;
    }
    if (AHIBuf[1]) {
        FreeVec(AHIBuf[1]);
        AHIBuf[1] = NULL;
    }
}


#endif // AUDIODRV_AHI == 1
