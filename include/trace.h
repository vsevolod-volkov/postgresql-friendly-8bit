#ifndef __TRACE_H__
#define __TRACE_H__

#include <stdarg.h>
#include <stdio.h>
#include <unistd.h>

extern FILE *_trace;

extern void trace_open(const char *path, int level);
extern void trace_write(int level, const char *format, ...);

#define trace(level, format, ...) if( _trace ) { trace_write(level, "%d: " format "\n", getpid(), __VA_ARGS__); }

#endif
