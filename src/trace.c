#include "trace.h"
#include <pthread.h>

FILE *_trace = NULL;

static pthread_mutex_t mutex;
static int _trace_level;

void trace_open(const char *path, int level) {
   if( !_trace && level > 0 ) {
      pthread_mutex_init(&mutex, NULL);
      _trace = fopen(path, "a");
      _trace_level = level;
   }
}

void trace_write(int level, const char *format, ...) {
   if( _trace && level <= _trace_level) {
      pthread_mutex_lock(&mutex);
      va_list args;
      va_start(args, format);
      vfprintf(_trace, format, args);
      va_end(args);
      fflush(_trace);
      pthread_mutex_unlock(&mutex);
   }
}