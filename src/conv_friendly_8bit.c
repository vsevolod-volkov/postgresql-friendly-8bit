#include <postgres.h>
#include <fmgr.h>
#include <utils/guc.h>
#include <storage/lwlock.h>
#include <dirent.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include "utf8_util.h"
#include "trace.h"

#define TRACE

PG_MODULE_MAGIC;

PG_FUNCTION_INFO_V1(byte_to_utf8);
PG_FUNCTION_INFO_V1(utf8_to_byte);

static char *friendly_8bit_directory = "";
static char *friendly_8bit_default_byte = "?";
static char *friendly_8bit_default_unicode = "?";
static char *friendly_8bit_trace = "";
static pthread_mutex_t mutex;

Datum byte_to_utf8(PG_FUNCTION_ARGS) {
	int encoding = PG_GETARG_INT32(0);
	unsigned char *src = (unsigned char *) PG_GETARG_CSTRING(2);
	// const unsigned char *srcStart = src;
	const unsigned char *srcEnd = src + PG_GETARG_INT32(4);
	unsigned char *dest = (unsigned char *) PG_GETARG_CSTRING(3);
	const unsigned char *destStart = dest;

	bool noError = PG_GETARG_BOOL(5);
	int i;

	CHECK_ENCODING_CONVERSION_ARGS(-1, PG_UTF8);

	pthread_mutex_lock(&mutex);

	SingleByteMap * const map = get_byte_to_utf8_patch(encoding);

	pthread_mutex_unlock(&mutex);

	if( map ) {
		for(; src < srcEnd && *src; src++) {
			pg_wchar unicode = conv_byte_to_utf8(map, *src);

			if( unicode ) {
				pg_wchar_to_utf8(unicode, &dest);
			} else {
				dest += sprintf((char*)dest, friendly_8bit_default_byte, (int)*src);
			}
		}
	} else {
		if( !strlen(friendly_8bit_directory) ) {
			ereport(ERROR, (
				errcode(ERRCODE_INTERNAL_ERROR),
				errmsg("friendly_8bit.directory configuration parameter is not set")
			));
		}

		ereport(WARNING, (
			errcode(ERRCODE_INTERNAL_ERROR),
			errmsg("Unexpected encoding %s (ID: %d) for friendly 8bit to UTF8 character translation.", pg_encoding_to_char(encoding), encoding)
		));

		for(; src < srcEnd && *src; src++) {
			if( *src < 0x80 ) {
				*(dest++) = *src;
			} else {
				dest += sprintf((char*)dest, friendly_8bit_default_byte, (int)*src);
			}
		}
	}
// *dest = '\0';
// trace("byte_to_utf8(\"%s\"): \"%s\"", srcStart, destStart)
	PG_RETURN_INT32(dest - destStart);
}

Datum utf8_to_byte(PG_FUNCTION_ARGS) {
	int encoding = PG_GETARG_INT32(1);
	unsigned char *src = (unsigned char *) PG_GETARG_CSTRING(2);
	const unsigned char *srcStart = src;
	const unsigned char *srcEnd = src + PG_GETARG_INT32(4);
	unsigned char *dest = (unsigned char *) PG_GETARG_CSTRING(3);
	const unsigned char *destStart = dest;

	bool noError = PG_GETARG_BOOL(5);
	int i;

	CHECK_ENCODING_CONVERSION_ARGS(PG_UTF8, -1);

	pthread_mutex_lock(&mutex);

	Byte4Map * const map = get_utf8_to_byte_patch(encoding);

	pthread_mutex_unlock(&mutex);

	if( map ) {
		while(src < srcEnd && *src) {
			pg_wchar unicode = utf8_to_pg_wchar(&src, srcEnd);

			*(dest) = unicode ? conv_utf8_to_byte(map, unicode) : 0;
			if( *dest ) {
				dest++;
			} else {
				dest += sprintf((char*)dest, friendly_8bit_default_unicode, (int)unicode);
			}
		}
	} else {
		if( !strlen(friendly_8bit_directory) ) {
			ereport(ERROR, (
				errcode(ERRCODE_INTERNAL_ERROR),
				errmsg("friendly_8bit.directory configuration parameter is not set")
			));
		}

		ereport(WARNING, (
			errcode(ERRCODE_INTERNAL_ERROR),
			errmsg("Unexpected encoding %s (ID: %d) for friendly UTF8 to 8bit character translation.", pg_encoding_to_char(encoding), encoding)
		));

		while(src < srcEnd && *src) {
			int mblen = utf8_len(*src);

			if( mblen == 1 && *src < 0x80 ) {
				*(dest++) = *(src++);
			} else {
				pg_wchar unicode = utf8_to_pg_wchar(&src, srcEnd);
				
				dest += sprintf((char*)dest, friendly_8bit_default_unicode, (int)unicode);
			}
		}
	}

// *dest = '\0';
// trace("utf8_to_byte(\"%s\"): \"%s\"", srcStart, destStart)
	PG_RETURN_INT32(dest - destStart);
}

extern char **environ;

const char f8b_suffix[] = ".f8b";
const char f8b_suffix_length = sizeof(f8b_suffix) - 1;

static int f8b_to_encoding(const char *name) {
	const int len = strlen(name);

	if( len < f8b_suffix_length || strcmp(name + len - f8b_suffix_length, ".f8b") )
		return -1;

	char basename[len - f8b_suffix_length + 1];

	memset(basename, 0, sizeof(basename));
	strncpy(basename, name, len - f8b_suffix_length);

	return pg_char_to_encoding(basename);
}

static int filter_f8b(const struct dirent *dirent) {
   return (
		dirent->d_type == DT_REG &&
		f8b_to_encoding(dirent->d_name) >= 0
	);
}

static int load_f8b(const char *name, int encoding) {
	trace("START load_f8b(\"%s\", %d)", name, encoding);

	FILE *f = fopen(name, "r");

	if( !f ) {
		trace("Can not open \"%s\" file.", name);
		elog(WARNING, "Can not open \"%s\" file.", name);
		return 0;
	}

	while( !feof(f) ) {
		char *line = NULL;
		size_t len = 0;

		if( getline(&line, &len, f) < 0 ) {
			if( errno ) {
				trace("Can not read from \"%s\" file.", name);
				elog(WARNING, "Can not read from \"%s\" file.", name);
				break;
			} else {
				continue;
			}
		}

		int byte, unicode;

		if( sscanf(line, "%i %i", &byte, &unicode) == 2 ) {
			if( byte < 1 || byte > 255 ) {
				trace("Single-byte character code %d (0x%d) occured in codepage while expected in 1...255 range. Ignored.", byte, byte);
				ereport(WARNING, (
					errcode(ERRCODE_INTERNAL_ERROR),
					errmsg("Single-byte character code %d (0x%d) occured in codepage while expected in 1...255 range. Ignored.", byte, byte)
				));
				continue;
			}

			if( unicode < 1 ) {
				trace("Unicode character scalar code %d (0x%d) occured in codepage while expected positive. Ignored.", unicode, unicode);
				ereport(WARNING, (
					errcode(ERRCODE_INTERNAL_ERROR),
					errmsg("Unicode character scalar code %d (0x%d) occured in codepage while expected positive. Ignored.", unicode, unicode)
				));
				continue;
			}

			const char *message = add_codepage_patch(encoding, byte, unicode);

			if( message ) {
				trace("Adding conversion single byte %d (0x%d) and unicode %d (0x%d) caused error \"%s\". Ignored.", byte, byte, unicode, unicode, message);
				ereport(WARNING, (
					errcode(ERRCODE_INTERNAL_ERROR),
					errmsg("Adding conversion single byte %d (0x%d) and unicode %d (0x%d) caused error \"%s\". Ignored.", byte, byte, unicode, unicode, message)
				));

			}
		}
	}

	fclose(f);
	trace("END load_f8b(\"%s\", %d)", name, encoding);
	return 1;
}

void _PG_init() {
	pthread_mutex_init(&mutex, NULL);
	pthread_mutex_lock(&mutex);

	DefineCustomStringVariable(
		"friendly_8bit.directory", //name
		"Directory path where friendly_8bit project finds codepage configuration files", // short_desc
		NULL, // long_desc
		&friendly_8bit_directory, // valueAddr
		"", // bootValue
		PGC_SIGHUP, // context
		0, // flags
		NULL, // check_hook
		NULL, // assign_hook
		NULL // show_hook
	);

	static const unsigned char default_byte[] = { 0xF0, 0x9F, 0x8F, 0xB4, 0xE2, 0x80, 0x8D, 0xE2, 0x98, 0xA0, 0xEF, 0xB8, 0x8F, 0 };
	DefineCustomStringVariable(
		"friendly_8bit.default_byte", //name
		"UTF-8 sequence to be used instead of any character absent in byte-to-UNICODE conversion", // short_desc
		NULL, // long_desc
		&friendly_8bit_default_byte, // valueAddr
		(const char*) &default_byte, // bootValue
		PGC_SIGHUP, // context
		0, // flags
		NULL, // check_hook
		NULL, // assign_hook
		NULL // show_hook
	);

	DefineCustomStringVariable(
		"friendly_8bit.default_unicode", //name
		"Single-character scring to be used instead of any character absent in UNICODE-to-byte conversion", // short_desc
		NULL, // long_desc
		&friendly_8bit_default_unicode, // valueAddr
		"?", // bootValue
		PGC_SIGHUP, // context
		0, // flags
		NULL, // check_hook
		NULL, // assign_hook
		NULL // show_hook
	);

	DefineCustomStringVariable(
		"friendly_8bit.trace", //name
		"Optional full path to trace file", // short_desc
		NULL, // long_desc
		&friendly_8bit_trace, // valueAddr
		"", // bootValue
		PGC_SIGHUP, // context
		0, // flags
		NULL, // check_hook
		NULL, // assign_hook
		NULL // show_hook
	);

	int dirlen = strlen(friendly_8bit_directory);
	char dir[dirlen + 2];
	strcpy(dir, friendly_8bit_directory);
	if( friendly_8bit_directory[dirlen - 1] != '/' )
		strcat(dir, "/");
	dirlen = strlen(dir);

	if( strlen(friendly_8bit_trace) ) {
		elog(NOTICE, "friendly_8bit.trace = \"%s\"", friendly_8bit_trace);
		trace_open(friendly_8bit_trace);
	}

	trace("START _PG_init()", 0);

	elog(NOTICE, "_PG_init()");

	trace("friendly_8bit.directory = \"%s\"", friendly_8bit_directory);
	trace("friendly_8bit.default_byte = \"%s\"", friendly_8bit_default_byte);
	trace("friendly_8bit.default_unicode = \"%s\"", friendly_8bit_default_unicode);
	trace("friendly_8bit.trace = \"%s\"", friendly_8bit_trace);

   if( strlen(friendly_8bit_directory) ) {
		struct dirent **dirent;
		int name_count = scandir(friendly_8bit_directory, &dirent, filter_f8b, alphasort);

		trace("Directory contains %d items.", name_count);

		if( name_count < 0) {
			trace("Can not list \"%s\" directory contents.", friendly_8bit_directory);
			elog(NOTICE, "Can not list \"%s\" directory contents.", friendly_8bit_directory);
		} else {
			for(size_t i = 0; i < name_count; i++) {
				char filename[dirlen + 1];
				strcpy(filename, dir);
				strcat(filename, dirent[i]->d_name);

				load_f8b(filename, f8b_to_encoding(dirent[i]->d_name));
				free(dirent[i]);
			}
			free(dirent);
		}
	}
	trace("END _PG_init()", 0);
	pthread_mutex_unlock(&mutex);
}

void _PG_fini() {
	trace("_PG_fini()", 0);
	pthread_mutex_lock(&mutex);
	destroy_codepages();
	pthread_mutex_unlock(&mutex);
	pthread_mutex_destroy(&mutex);
}