/****************************************************************************
*                                                                           *
* Azimer's HLE Audio Plugin for Project64 Compatible N64 Emulators          *
* http://www.apollo64.com/                                                  *
* Copyright (C) 2000-2015 Azimer. All rights reserved.                      *
*                                                                           *
* License:                                                                  *
* GNU/GPLv2 http://www.gnu.org/licenses/gpl-2.0.html                        *
*                                                                           *
****************************************************************************/

#include "audiohle.h"

u16 adpcmtable[0x88];

void InitInput(s32 *inp, int index, u8 icode, u8 mask, u8 shifter, int vscale)
{
	inp[index] = (s16)((icode & mask) << shifter);
	inp[index] = (inp[index] * vscale) >> 16;
}

void ADPCMFillArray(s32 *a, s16* book1, s16* book2, s32 l1, s32 l2, s32 *inp)
{
	for (int i = 0; i < 8; i++)
	{
		a[i]  = (s32)book1[i] * (s32)l1;
		a[i] += (s32)book2[i] * (s32)l2;
		for (int i2 = 0; i2 < i; i2++)
		{
			a[i] += (s32)book2[(i - 1) - i2] * inp[i2];
		}
		a[i] += 2048 * inp[i];
	}
}

void ADPCM() { // Work in progress! :)
	BYTE Flags = (u8)((k0 >> 16) & 0xff);
	WORD Gain = (u16)(k0 & 0xffff);
	DWORD Address = (t9 & 0xffffff);// + SEGMENTS[(t9>>24)&0xf];
	WORD inPtr = 0;
	//s16 *out=(s16 *)(testbuff+(AudioOutBuffer>>2));
	s16 *out = (s16 *)(BufferSpace + AudioOutBuffer);
	u8 *in = (u8 *)(BufferSpace + AudioInBuffer);
	s16 count = (s16)AudioCount;
	int vscale;
	WORD index;
	WORD j;
	s32 a[8];
	s16* book1;
	s16* book2;

	/*
	if (Address > (1024*1024*8))
	Address = (t9 & 0xffffff);
	*/
	memset(out, 0, 32);

	if (!(Flags & 0x1))
	{
		if (Flags & 0x2) {
			memcpy(out, &rdram[loopval], 32);
		}
		else {
			memcpy(out, &rdram[Address], 32);
		}
	}

	s32 l1 = out[15];
	s32 l2 = out[14];
	s32 inp1[8];
	s32 inp2[8];
	out += 16;
	while (count>0)
	{
		// the first interation through, these values are
		// either 0 in the case of A_INIT, from a special
		// area of memory in the case of A_LOOP or just
		// the values we calculated the last time

		u8 code = BufferSpace[BES(AudioInBuffer + inPtr)];
		index = code & 0xf;
		index <<= 4;									// index into the adpcm code table
		book1 = (s16 *)&adpcmtable[index];
		book2 = book1 + 8;
		code >>= 4;									// upper nibble is scale
#if 0
		assert((12 - code) - 1 >= 0);
#endif
		vscale = 0x8000u >> ((12 - code) - 1);		// very strange. 0x8000 would be .5 in 16:16 format
		// so this appears to be a fractional scale based
		// on the 12 based inverse of the scale value.  note
		// that this could be negative, in which case we do
		// not use the calculated vscale value...

		inPtr++;									// coded adpcm data lies next
		j = 0;
		while (j<8)									// loop of 8, for 8 coded nibbles from 4 bytes
			// which yields 8 short pcm values
		{
			u8 icode = BufferSpace[BES(AudioInBuffer + inPtr)];
			inPtr++;

			InitInput(inp1, j, icode, 0xf0, 8, vscale); // this will in effect be signed
			j++;

			InitInput(inp1, j, icode, 0xf, 12, vscale);
			j++;
		}
		j = 0;
		while (j<8)
		{
			u8 icode = BufferSpace[BES(AudioInBuffer + inPtr)];
			inPtr++;

			InitInput(inp2, j, icode, 0xf0, 8, vscale); // this will in effect be signed
			j++;

			InitInput(inp2, j, icode, 0xf, 12, vscale);
			j++;
		}

		ADPCMFillArray(a, book1, book2, l1, l2, inp1);

		for (j = 0; j<8; j++)
		{
			a[MES(j)] >>= 11;
			a[MES(j)] = pack_signed(a[MES(j)]);
			*(out++) = a[MES(j)];
		}
		l1 = a[6];
		l2 = a[7];

		ADPCMFillArray(a, book1, book2, l1, l2, inp2);

		for (j = 0; j<8; j++)
		{
			a[MES(j)] >>= 11;
			a[MES(j)] = pack_signed(a[MES(j)]);
			*(out++) = a[MES(j)];
		}
		l1 = a[6];
		l2 = a[7];

		count -= 32;
	}
	out -= 16;
	memcpy(&rdram[Address], out, 32);
}

void ADPCM2() { // Verified to be 100% Accurate...
	BYTE Flags = (u8)((k0 >> 16) & 0xff);
	WORD Gain = (u16)(k0 & 0xffff);
	DWORD Address = (t9 & 0xffffff);// + SEGMENTS[(t9>>24)&0xf];
	WORD inPtr = 0;
	//s16 *out=(s16 *)(testbuff+(AudioOutBuffer>>2));
	s16 *out = (s16 *)(BufferSpace + AudioOutBuffer);
	u8 *in = (u8 *)(BufferSpace + AudioInBuffer);
	s16 count = (s16)AudioCount;
	int vscale;
	WORD index;
	WORD j;
	s32 a[8];
	s16* book1;
	s16* book2;

	u8 srange;
	u8 inpinc;
	u8 mask1;
	u8 mask2;
	u8 shifter;

	memset(out, 0, 32);

	if (!(Flags & 0x1))
		if (Flags & 0x2)
			memcpy(out, &rdram[loopval], 32);
		else
			memcpy(out, &rdram[Address], 32);
	if (Flags & 0x4) { // Tricky lil Zelda MM and ABI2!!! hahaha I know your secrets! :DDD
		srange = 0xE;
		inpinc = 0x5;
		mask1 = 0xC0;
		mask2 = 0x30;
		shifter = 10;
	}
	else {
		srange = 0xC;
		inpinc = 0x9;
		mask1 = 0xf0;
		mask2 = 0x0f;
		shifter = 12;
	}

	s32 l1 = out[15];
	s32 l2 = out[14];
	s32 inp1[8];
	s32 inp2[8];
	out += 16;
	while (count>0) {
		u8 code = BufferSpace[BES(AudioInBuffer + inPtr)];
		index = code & 0xf;
		index <<= 4;
		book1 = (s16 *)&adpcmtable[index];
		book2 = book1 + 8;
		code >>= 4;
#if 0
		assert((srange - code) - 1 >= 0);
#endif
		vscale = 0x8000u >> ((srange - code) - 1);

		inPtr++;
		j = 0;

		while (j<8) {
			u8 icode = BufferSpace[BES(AudioInBuffer + inPtr)];
			inPtr++;

			InitInput(inp1, j, icode, mask1, 8, vscale); // this will in effect be signed
			j++;

			InitInput(inp1, j, icode, mask2, shifter, vscale);
			j++;

			if (Flags & 4) {
				InitInput(inp1, j, icode, 0xC, 12, vscale); // this will in effect be signed
				j++;

				InitInput(inp1, j, icode, 0x3, 14, vscale);
				j++;
			} // end flags
		} // end while



		j = 0;
		while (j<8) {
			u8 icode = BufferSpace[BES(AudioInBuffer + inPtr)];
			inPtr++;

			InitInput(inp2, j, icode, mask1, 8, vscale);
			j++;

			InitInput(inp2, j, icode, mask2, shifter, vscale);
			j++;

			if (Flags & 4) {
				InitInput(inp2, j, icode, 0xC, 12, vscale);
				j++;

				InitInput(inp2, j, icode, 0x3, 14, vscale);
				j++;
			} // end flags
		}

		ADPCMFillArray(a, book1, book2, l1, l2, inp1);

		for (j = 0; j<8; j++)
		{
			a[MES(j)] >>= 11;
			a[MES(j)] = pack_signed(a[MES(j)]);
			*(out++) = a[MES(j)];
		}
		l1 = a[6];
		l2 = a[7];

		ADPCMFillArray(a, book1, book2, l1, l2, inp2);

		for (j = 0; j<8; j++)
		{
			a[MES(j)] >>= 11;
			a[MES(j)] = pack_signed(a[MES(j)]);
			*(out++) = a[MES(j)];
		}
		l1 = a[6];
		l2 = a[7];

		count -= 32;
	}
	out -= 16;
	memcpy(&rdram[Address], out, 32);
}

void ADPCM3() { // Verified to be 100% Accurate...
	BYTE Flags = (u8)((t9 >> 0x1c) & 0xff);
	//WORD Gain=(u16)(k0&0xffff);
	DWORD Address = (k0 & 0xffffff);// + SEGMENTS[(t9>>24)&0xf];
	WORD inPtr = (t9 >> 12) & 0xf;
	//s16 *out=(s16 *)(testbuff+(AudioOutBuffer>>2));
	s16 *out = (s16 *)(BufferSpace + (t9 & 0xfff) + 0x4f0);
	BYTE *in = (BYTE *)(BufferSpace + ((t9 >> 12) & 0xf) + 0x4f0);
	s16 count = (s16)((t9 >> 16) & 0xfff);
	int vscale;
	WORD index;
	WORD j;
	s32 a[8];
	s16* book1;
	s16* book2;

	memset(out, 0, 32);

	if (!(Flags & 0x1))
		if (Flags & 0x2)
			memcpy(out, &rdram[loopval], 32);
		else
			memcpy(out, &rdram[Address], 32);

	s32 l1 = out[15];
	s32 l2 = out[14];
	s32 inp1[8];
	s32 inp2[8];
	out += 16;
	while (count>0)
	{
		// the first interation through, these values are
		// either 0 in the case of A_INIT, from a special
		// area of memory in the case of A_LOOP or just
		// the values we calculated the last time

		u8 code = BufferSpace[BES(0x4f0 + inPtr)];
		index = code & 0xf;
		index <<= 4;									// index into the adpcm code table
		book1 = (s16 *)&adpcmtable[index];
		book2 = book1 + 8;
		code >>= 4;									// upper nibble is scale

		vscale = 0x8000u >> ((12 - code) - 1);		// very strange. 0x8000 would be .5 in 16:16 format
		// so this appears to be a fractional scale based
		// on the 12 based inverse of the scale value.  note
		// that this could be negative, in which case we do
		// not use the calculated vscale value...
		if ((12 - code) - 1 < 0)
			vscale = 0x10000; /* null operation:  << 16 then >> 16 */

		inPtr++;									// coded adpcm data lies next
		j = 0;
		while (j<8)									// loop of 8, for 8 coded nibbles from 4 bytes
			// which yields 8 short pcm values
		{
			u8 icode = BufferSpace[BES(0x4f0 + inPtr)];
			inPtr++;

			InitInput(inp1, j, icode, 0xf0, 8, vscale); // this will in effect be signed
			j++;

			InitInput(inp1, j, icode, 0xf, 12, vscale);
			j++;
		}
		j = 0;
		while (j<8)
		{
			u8 icode = BufferSpace[BES(0x4f0 + inPtr)];
			inPtr++;

			InitInput(inp2, j, icode, 0xf0, 8, vscale); // this will in effect be signed
			j++;

			InitInput(inp2, j, icode, 0xf, 12, vscale);
			j++;
		}

		ADPCMFillArray(a, book1, book2, l1, l2, inp1);

		for (j = 0; j<8; j++)
		{
			a[MES(j)] >>= 11;
			a[MES(j)] = pack_signed(a[MES(j)]);
			*(out++) = a[MES(j)];
			//*(out+j)=a[MES(j)];
		}
		//out += 0x10;
		l1 = a[6];
		l2 = a[7];

		ADPCMFillArray(a, book1, book2, l1, l2, inp2);

		for (j = 0; j<8; j++)
		{
			a[MES(j)] >>= 11;
			a[MES(j)] = pack_signed(a[MES(j)]);
			*(out++) = a[MES(j)];
			//*(out+j+0x1f8)=a[MES(j)];
		}
		l1 = a[6];
		l2 = a[7];

		count -= 32;
	}
	out -= 16;
	memcpy(&rdram[Address], out, 32);
}

void LOADADPCM() { // Loads an ADPCM table - Works 100% Now 03-13-01
	u32 v0;
	size_t i, limit;

	v0 = (t9 & 0xffffff);// + SEGMENTS[(t9>>24)&0xf];
	//	if (v0 > (1024*1024*8))
	//		v0 = (t9 & 0xffffff);
	//	memcpy (dmem+0x4c0, rdram+v0, k0&0xffff); // Could prolly get away with not putting this in dmem
	//	assert ((k0&0xffff) <= 0x80);
	u16 *table = (u16 *)(rdram + v0);

	limit = (k0 & 0x0000FFFF) >> 4;
	for (i = 0; i < limit; i++)
		swap_elements(&adpcmtable[8*i], &table[8*i]);
}

void LOADADPCM2() { // Loads an ADPCM table - Works 100% Now 03-13-01
	u32 v0;
	size_t i, limit;

	v0 = (t9 & 0xffffff);// + SEGMENTS[(t9>>24)&0xf];
	u16 *table = (u16 *)(rdram + v0); // Zelda2 Specific...

	limit = (k0 & 0x0000FFFF) >> 4;
	for (i = 0; i < limit; i++)
		swap_elements(&adpcmtable[8*i], &table[8*i]);
}

void LOADADPCM3() { // Loads an ADPCM table - Works 100% Now 03-13-01
	u32 v0;
	size_t i, limit;

	v0 = (t9 & 0xffffff);
	//memcpy (dmem+0x3f0, rdram+v0, k0&0xffff);
	//assert ((k0&0xffff) <= 0x80);
	u16 *table = (u16 *)(rdram + v0);

	limit = (k0 & 0x0000FFFF) >> 4;
	for (i = 0; i < limit; i++)
		swap_elements(&adpcmtable[8*i], &table[8*i]);
}
