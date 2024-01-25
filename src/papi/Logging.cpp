#include "papi/Logging.hpp"

//
// Logging
//
#ifdef _DEBUG
#include <stdio.h>
#include <stdarg.h>
#include <string.h>


void _logprintf(const char* file, int line, const char* function, const char* format, ...) {
    va_list arg;
    va_start(arg, format);
    auto fileName = strrchr(file, '/') + 1;
    fprintf(stderr, "%s:%d (%s)\t", fileName, line, function);
    vfprintf(stderr, format, arg);
    fprintf(stderr, "\n");
    va_end(arg);
}
#endif