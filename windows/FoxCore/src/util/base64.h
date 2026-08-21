#pragma once
#include <iostream>
#include <Windows.h>

static const char base64_alphabet[] = {
	'A', 'B', 'C', 'D', 'E', 'F', 'G',
	'H', 'I', 'J', 'K', 'L', 'M', 'N',
	'O', 'P', 'Q', 'R', 'S', 'T',
	'U', 'V', 'W', 'X', 'Y', 'Z',
	'a', 'b', 'c', 'd', 'e', 'f', 'g',
	'h', 'i', 'j', 'k', 'l', 'm', 'n',
	'o', 'p', 'q', 'r', 's', 't',
	'u', 'v', 'w', 'x', 'y', 'z',
	'0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
	'+', '/'
};

static const unsigned char base64_suffix_map[256] = {
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 253, 255,
	255, 253, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
	255, 255, 255, 255, 255, 255, 255, 255, 253, 255, 255, 255,
	255, 255, 255, 255, 255, 255, 255, 62, 255, 255, 255, 63,
	52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 255, 255,
	255, 254, 255, 255, 255, 0, 1, 2, 3, 4, 5, 6,
	7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18,
	19, 20, 21, 22, 23, 24, 25, 255, 255, 255, 255, 255,
	255, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36,
	37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48,
	49, 50, 51, 255, 255, 255, 255, 255, 255, 255, 255, 255,
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
	255, 255, 255, 255
};

static char cmove_bits(unsigned char src, unsigned lnum, unsigned rnum)
{
	src <<= lnum;
	src >>= rnum;
	return src;
}

int base64_encode(const char* indata, int inlen, char* outdata, int* outlen)
{
	int ret = 0;
	if (indata == NULL || inlen == 0 || outdata == NULL)
	{
		return -1;
	}

	// source string processed, if in_len not multiple of 3, pad to multiple of 3
	int in_len = 0;

	// pad chars needed: 2, 1, or 0 (0 means no padding)
	int pad_num = 0;
	if (inlen % 3 != 0)
	{
		pad_num = 3 - inlen % 3;
	}

	in_len = inlen + pad_num;

	int out_len = in_len * 8 / 6;

	char* p = outdata;

	for (int i = 0; i < in_len; i += 3)
	{
		if (i == in_len - 3 && pad_num != 0)
		{
			int value = (unsigned char)(*indata) >> 2;
			*p = base64_alphabet[value];

			if (pad_num == 1) 
			{
				*(p + 1) = base64_alphabet[(int)(cmove_bits(*indata, 6, 2) + cmove_bits(*(indata + 1), 0, 4))];
				*(p + 2) = base64_alphabet[(int)cmove_bits(*(indata + 1), 4, 2)];
				*(p + 3) = '=';
			}
			else if (pad_num == 2) 
			{
				*(p + 1) = base64_alphabet[(int)cmove_bits(*indata, 6, 2)];
				*(p + 2) = '=';
				*(p + 3) = '=';
			}
		}
		else
		{
			int value = (unsigned char)(*indata) >> 2;
			*p = base64_alphabet[value];

			*(p + 1) = base64_alphabet[cmove_bits(*indata, 6, 2) + cmove_bits(*(indata + 1), 0, 4)];
			*(p + 2) = base64_alphabet[cmove_bits(*(indata + 1), 4, 2) + cmove_bits(*(indata + 2), 0, 6)];
			*(p + 3) = base64_alphabet[*(indata + 2) & 0x3f];
			indata += 3; 
		}
		p += 4;
	}

	if (outlen != NULL)
	{
		*outlen = out_len;
	}
	return ret;
}

int base64_decode(const char* indata, int inlen, char* outdata, int* outlen)
{
	int ret = 0;
	if (indata == NULL || inlen <= 0 || outdata == NULL || outlen == NULL)
	{
		return -1;
	}
	if (inlen % 4 != 0)
	{
		return -2;
	}

	int t = 0, x = 0, y = 0, i = 0;
	unsigned char c = 0;
	int g = 3;

	while (x < inlen && indata[x] != 0)
	{
		c = base64_suffix_map[(unsigned char)indata[x++]];

		if (c == 255)
			return -1;

		if (c == 253)
			continue;

		if (c == 254)
		{
			c = 0; g--;
		}

		t = (t << 6) | c;

		if (++y == 4)
		{
			outdata[i++] = (unsigned char)((t >> 16) & 0xff);
			if (g > 1) outdata[i++] = (unsigned char)((t >> 8) & 0xff);
			if (g > 2) outdata[i++] = (unsigned char)(t & 0xff);
			y = t = 0;
		}
	}
	if (outlen != NULL)
	{
		*outlen = i;
	}
	return ret;
}
