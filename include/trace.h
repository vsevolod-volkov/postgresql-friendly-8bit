#ifndef __TRACE_H__
#define __TRACE_H__

#include <stdarg.h>
#include <stdio.h>
#include <unistd.h>

extern FILE *_trace;

extern void trace_open(const char *path);
extern void trace_write(const char *format, ...);

#define trace(format, ...) if( _trace ) { trace_write("%d: " format "\n", getpid(), __VA_ARGS__); }

#endif
