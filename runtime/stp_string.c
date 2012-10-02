/* -*- linux-c -*- 
 * String Functions
 * Copyright (C) 2005, 2006, 2007, 2009 Red Hat Inc.
 *
 * This file is part of systemtap, and is free software.  You can
 * redistribute it and/or modify it under the terms of the GNU General
 * Public License (GPL); either version 2, or (at your option) any
 * later version.
 */
#ifndef _STP_STRING_C_
#define _STP_STRING_C_

#include "stp_string.h"

/** @file stp_string.c
 * @brief Implements string functions.
 */
/** @addtogroup string String Functions
 *
 * @{
 */

/** Sprintf into a string.
 * Like printf, except output goes into a string.  
 *
 * NB: these are script language printf formatting directives, where
 * %d ints are 64-bits etc, so we can't use gcc level attribute printf
 * to type-check the arguments.
 *
 * @param str string
 * @param fmt A printf-style format string followed by a 
 * variable number of args.
 */

static int _stp_snprintf(char *buf, size_t size, const char *fmt, ...)
{
        va_list args;
        int i;

        va_start(args, fmt);
        i = _stp_vsnprintf(buf,size,fmt,args);
        va_end(args);
        return i;
}

static int _stp_vscnprintf(char *buf, size_t size, const char *fmt, va_list args)
{
	unsigned i = _stp_vsnprintf(buf,size,fmt,args);
	return (i >= size) ? (size - 1) : i;
}


/** Return a printable text string.
 *
 * Takes a string, and any ASCII characters that are not printable are
 * replaced by the corresponding escape sequence in the returned
 * string.
 *
 * @param outstr Output string pointer
 * @param in Input string pointer
 * @param len Maximum length of string to return not including terminating 0.
 * 0 means MAXSTRINGLEN.
 * @param quoted Put double quotes around the string. If input string is truncated
 * in will have "..." after the second quote.
 * @param user Set this to indicate the input string pointer is a userspace pointer.
 */
static void _stp_text_str(char *outstr, char *in, int len, int quoted, int user)
{
	char c = '\0', *out = outstr;

	if (len <= 0 || len > MAXSTRINGLEN-1)
		len = MAXSTRINGLEN-1;
	if (quoted) {
		len = max(len, 5) - 2;
		*out++ = '"';
	}

	if (user) {
		if (_stp_read_address(c, in, USER_DS))
			goto bad;
	} else
		c = *in;

	while (c && len > 0) {
		int num = 1;
		if (isprint(c) && isascii(c)
                    && c != '"' && c != '\\') /* quoteworthy characters */
                  *out++ = c;
		else {
			switch (c) {
			case '\a':
			case '\b':
			case '\f':
			case '\n':
			case '\r':
			case '\t':
			case '\v':
			case '"':
			case '\\':
				num = 2;
				break;
			default:
				num = 4;
				break;
			}
			
			if (len < num)
				break;

			*out++ = '\\';
			switch (c) {
			case '\a':
				*out++ = 'a';
				break;
			case '\b':
				*out++ = 'b';
				break;
			case '\f':
				*out++ = 'f';
				break;
			case '\n':
				*out++ = 'n';
				break;
			case '\r':
				*out++ = 'r';
				break;
			case '\t':
				*out++ = 't';
				break;
			case '\v':
				*out++ = 'v';
				break;
			case '"':
				*out++ = '"';
				break;
			case '\\':
				*out++ = '\\';
				break;
			default:                  /* output octal representation */
				*out++ = to_oct_digit((c >> 6) & 03);
				*out++ = to_oct_digit((c >> 3) & 07);
				*out++ = to_oct_digit(c & 07);
				break;
			}
		}
		len -= num;
		in++;
		if (user) {
			if (_stp_read_address(c, in, USER_DS))
				goto bad;
		} else
			c = *in;
	}

	if (quoted) {
		if (c) {
			out = out - 3 + len;
			*out++ = '"';
			*out++ = '.';
			*out++ = '.';
			*out++ = '.';
		} else
			*out++ = '"';
	}
	*out = '\0';
	return;
bad:
	strlcpy (outstr, "<unknown>", len);
}

/**
 * Convert a UTF-32 character into a UTF-8 string.
 *
 * @param buf The output buffer.
 * @param size The size of the output buffer.
 * @param c The character to convert.
 *
 * @return The number of bytes written (not counting \0),
 *         0 if there's not enough room for the full character,
 *         or < 0 for invalid characters (with buf untouched).
 */
static int _stp_convert_utf32(char* buf, int size, u32 c)
{
	int i, n;

	/* 0xxxxxxx */
	if (c < 0x7F)
		n = 1;

	/* 110xxxxx 10xxxxxx */
	else if (c < 0x7FF)
		n = 2;

	/* UTF-16 surrogates are not valid by themselves.
	 * XXX We could decide to be lax and just encode it anyway...
	 */
	else if (c >= 0xD800 && c <= 0xDFFF)
		return -EINVAL;

	/* 1110xxxx 10xxxxxx 10xxxxxx */
	else if (c < 0xFFFF)
		n = 3;

	/* 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx */
	else if (c < 0x10FFFF)
		n = 4;

	/* The original UTF-8 design could go up to 0x7FFFFFFF, but RFC 3629
	 * sets the upperbound to 0x10FFFF; thus all higher values are errors.
	 */
	else
		return -EINVAL;

	if (size < n + 1)
		return 0;

	buf[n] = '\0';
	if (n == 1)
		buf[0] = c;
	else {
		u8 msb = ((1 << n) - 1) << (8 - n);
		for (i = n - 1; i > 0; --i) {
			buf[i] = 0x80 | (c & 0x3F);
			c >>= 6;
		}
		buf[0] = msb | c;
	}

	return n;
}

/** @} */
#endif /* _STP_STRING_C_ */
