#include "papi/Logging.hpp"

//
// Logging
//
#ifndef NDEBUG
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

using namespace papi;

void Logger::printf(const char* file, int line, const char* function, const char* format, ...) {
    va_list arg;
    va_start(arg, format);
    auto fileName = strrchr(file, '/') + 1;
    fprintf(stderr, "%s:%d (%s)\t", fileName, line, function);
    vfprintf(stderr, format, arg);
    fprintf(stderr, "\n");
    va_end(arg);
}
#endif