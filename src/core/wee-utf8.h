/*
 * Copyright (c) 2003-2009 by FlashCode <flashcode@flashtux.org>
 * See README for License detail, AUTHORS for developers list.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#ifndef __WEECHAT_UTF8_H
#define __WEECHAT_UTF8_H 1

#ifndef __USE_XOPEN
#define __USE_XOPEN
#endif

#if defined(__OpenBSD__)
#include <utf8/wchar.h>
#else
#include <wchar.h>
#endif

extern int local_utf8;

extern void utf8_init ();
extern int utf8_has_8bits (const char *string);
extern int utf8_is_valid (const char *string, char **error);
extern void utf8_normalize (const char *string, char replacement);
extern char *utf8_prev_char (const char *string_start, const char *string);
extern char *utf8_next_char (const char *string);
extern int utf8_char_int (const char *string);
extern wint_t utf8_wide_char (const char *string);
extern int utf8_char_size (const char *string);
extern int utf8_strlen (const char *string);
extern int utf8_strnlen (const char *string, int bytes);
extern int utf8_strlen_screen (const char *string);
extern int utf8_charcmp (const char *string1, const char *string2);
extern int utf8_charcasecmp (const char *string1, const char *string2);
extern int utf8_char_size_screen (const char *string);
extern char *utf8_add_offset (const char *string, int offset);
extern int utf8_real_pos (const char *string, int pos);
extern int utf8_pos (const char *string, int real_pos);
extern char *utf8_strndup (const char *string, int max_chars);

#endif /* wee-utf8.h */