#include "trace.h"
#include <pthread.h>

FILE *_trace = NULL;

static pthread_mutex_t mutex;

void trace_open(const char *path) {
	pthread_mutex_init(&mutex, NULL);
   if( !_trace ) {
      _trace = fopen(path, "a");
   }
}

void trace_write(const char *format, ...) {
   if( _trace ) {
      pthread_mutex_lock(&mutex);
      va_list args;
      va_start(args, format);
      vfprintf(_trace, format, args);
      va_end(args);
      fflush(_trace);
      pthread_mutex_unlock(&mutex);
   }
}