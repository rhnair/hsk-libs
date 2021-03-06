/** \file
 * HSK ICM7228 8-Digit LED Display Decoder Driver implementation
 *
 * This file implements the static functions of the ICM7228 display decoder
 * driver.
 *
 * @see Intersil ICM7228 Data Sheet: <a href="../contrib/ICM7228.pdf">ICM7228.pdf</a>
 * @author kami
 */

#include <Infineon/XC878.h>

#include "hsk_icm7228.h"

/**
 * This is a codepage to translate 7bit ASCII characters into corresponding
 * 7 segment display patterns.
 *
 * The ASCII character can be used to receive the desired 7 segment code.
 * E.g. codepage['A'] retrieves the letter 'A' from the table.
 *
 * Some letters like 'X' are badly recognisable, others cannot be well
 * represented at all. E.g. the letters 'M' and 'W' are identical to the
 * letters 'N' and 'U'.
 *
 * Capitals and small characters are identical. Characters without proper
 * encoding are filled with 0x00, which leaves only the '.' of a 7 segment
 * display active.
 *
 * The 6 characters beyond 0-9 return "ABCDEF", which permits for easier
 * display of HEX digits.
 *
 * The first 16 characters from index 0 return the characters
 * "0123456789ABCDEF" as well.
 */
static const ubyte code codepage[] = {
	0xFB, 0xB0, 0xED, 0xF5, 0xB6, 0xD7, 0xDF, 0xF0,
	0xFF, 0xF7, 0xFE, 0x9F, 0xCB, 0xBD, 0xCF, 0xCE,
	0x80, 0xC0, 0xE0, 0xF0, 0xF1, 0xF9, 0xFB, 0xFF,
	0x7F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x80, 0x00, 0x00, 0x00, 0x00, 0x82, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xFB, 0xB0, 0xED, 0xF5, 0xB6, 0xD7, 0xDF, 0xF0,
	0xFF, 0xF7, 0xFE, 0x9F, 0xCB, 0xBD, 0xCF, 0xCE,
	0x00, 0xFE, 0x9F, 0xCB, 0xBD, 0xCF, 0xCE, 0xDF,
	0xBE, 0x8A, 0xB1, 0xDE, 0x8B, 0xFA, 0xFA, 0x9D,
	0xEE, 0xF6, 0x8C, 0xD7, 0x8F, 0xBB, 0x99, 0xBB,
	0xB4, 0xB6, 0xC5, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0xFE, 0x9F, 0xCB, 0xBD, 0xCF, 0xCE, 0xDF,
	0xBE, 0x8A, 0xB1, 0xDE, 0x8B, 0xFA, 0xFA, 0x9D,
	0xEE, 0xF6, 0x8C, 0xD7, 0x8F, 0xBB, 0x99, 0xBB,
	0xB4, 0xB6, 0xC5, 0x00, 0x00, 0x00, 0x00, 0x00
};

/**
 * The offset for illuminating a number of segments.
 */
#define ILLUMINATE_OFFSET	16

void hsk_icm7228_writeString(ubyte xdata * const buffer,
		char const * str, ubyte pos, ubyte len) {
	while (len > 0 && str[0]) {
		buffer[pos] = codepage[str[0]];
		len--;
		if (str[0] != '.' && str[1] == '.') {
			buffer[pos] &= 0x7f;
			str++;
		}
		str++;
		pos++;
	}
}

void hsk_icm7228_writeDec(ubyte xdata * const buffer, uword value,
                          char power, ubyte const pos, ubyte len) {
	ubyte point = power ? 0x7f : 0xff;

	while (len > 0) {
		buffer[pos + --len] = (value || power <= 0) ? codepage[value % 10] : codepage[' '] ;
		if (power++ == 0) {
			buffer[pos + len] &= point;
		}
		value /= 10;
	}
}

void hsk_icm7228_writeHex(ubyte xdata * const buffer, uword value,
                          char power, ubyte const pos, ubyte len) {
	ubyte point = power ? 0x7f : 0xff;

	while (len > 0) {
		buffer[pos + --len] = (value || power <= 0) ? codepage[value & 0xf] : codepage[' '] ;
		if (power++ == 0) {
			buffer[pos + len] &= point;
		}
		value >>= 4;
	}
}

void hsk_icm7228_illuminate(ubyte xdata * const buffer,
                            ubyte segments, ubyte pos, ubyte len) {
	ubyte illuminate;
	while (len--) {
		illuminate = segments > 8 ? 8 : segments;
		buffer[pos++] = codepage[ILLUMINATE_OFFSET + illuminate];
		segments -= illuminate;
	}
}

