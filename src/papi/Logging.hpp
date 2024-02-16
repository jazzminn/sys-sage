#pragma once

//
// Logging
//
#ifndef NDEBUG
namespace papi {
    class Logger {
    public:
        static void printf(const char* file, int line, const char* function, const char* format, ...);
    };
}
#define logprintf(format, ...) do { papi::Logger::printf(__FILE__, __LINE__, __FUNCTION__, format, ## __VA_ARGS__); } while(false);

#else
#define logprintf(format, ...)
#endif
