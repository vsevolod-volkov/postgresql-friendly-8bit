#ifndef __UTF8_UTIL_H__
#define __UTF8_UTIL_H__

#include <postgres.h>
#include <fmgr.h>
#include <mb/pg_wchar.h>

typedef unsigned char Byte1Map[256];
typedef Byte1Map* Byte2Map[256];
typedef Byte2Map* Byte3Map[256];
typedef Byte3Map* Byte4Map[256];

typedef pg_wchar SingleByteMap[256];

static inline size_t utf8_len(unsigned char ch) {
	return (!ch & 0x80) ? 1 :
		(ch & 0xe0 == 0xc0) ? 2 :
		(ch & 0xf0 == 0xe0) ? 3 :
		(ch & 0xf8 == 0xf0) ? 4 :
		1;
}

static inline void pg_wchar_to_utf8(pg_wchar unicode, unsigned char **dest) {
	if (unicode <= 0x7F) {
		(*(*dest)++) = unicode;
	} else if (unicode <= 0x7FF) {
		(*(*dest)++) = 0xC0 | ((unicode >> 6) & 0x1F);
		(*(*dest)++) = 0x80 | (unicode & 0x3F);
	} else if (unicode <= 0xFFFF) {
		(*(*dest)++) = 0xE0 | ((unicode >> 12) & 0x0F);
		(*(*dest)++) = 0x80 | ((unicode >> 6) & 0x3F);
		(*(*dest)++) = 0x80 | (unicode & 0x3F);
	} else {
		(*(*dest)++) = 0xF0 | ((unicode >> 18) & 0x07);
		(*(*dest)++) = 0x80 | ((unicode >> 12) & 0x3F);
		(*(*dest)++) = 0x80 | ((unicode >> 6) & 0x3F);
		(*(*dest)++) = 0x80 | (unicode & 0x3F);
	}
}

static inline pg_wchar utf8_to_pg_wchar(unsigned char **src, const unsigned char *end) {
	pg_wchar unicode;

	if ((**src & 0x80) == 0) {
		unicode = (pg_wchar) *((*src)++);
	} else if ((**src & 0xe0) == 0xc0) {
		if( *src + 2 > end) return 0;
		unicode = (pg_wchar) ((*((*src)++) & 0x1f) << 6);
		unicode |= (pg_wchar) (*((*src)++) & 0x3f);
	} else if ((**src & 0xf0) == 0xe0) {
		if( *src + 3 > end) return 0;
		unicode = (pg_wchar) ((*((*src)++) & 0x0f) << 12);
		unicode |= (pg_wchar) ((*((*src)++) & 0x3f) << 6);
		unicode |= (pg_wchar) (*((*src)++) & 0x3f);
	} else if ((**src & 0xf8) == 0xf0) {
		if( *src + 4 > end) return 0;
		unicode = (pg_wchar) ((*((*src)++) & 0x07) << 18);
		unicode |= (pg_wchar) ((*((*src)++) & 0x3f) << 12);
		unicode |= (pg_wchar) ((*((*src)++) & 0x3f) << 6);
		unicode |= (pg_wchar) (*((*src)++) & 0x3f);
	} else {
		(*src)++;
		unicode = 0;
	}

	return unicode;
}

extern const char* add_codepage_patch(int encoding, unsigned char byte, pg_wchar unicode);

Byte4Map * const get_utf8_to_byte_patch(int encoding);
SingleByteMap * const get_byte_to_utf8_patch(int encoding);

extern pg_wchar conv_byte_to_utf8(SingleByteMap * const map, unsigned char byte);
extern unsigned char conv_utf8_to_byte(Byte4Map * const map, pg_wchar unicode);

extern void destroy_codepages();
#endif
