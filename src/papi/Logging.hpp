#pragma once

#define _DEBUG
//
// Logging
//
#ifdef _DEBUG
extern void _logprintf(const char* file, int line, const char* function, const char* format, ...);
#define logprintf(format, ...) do { _logprintf(__FILE__, __LINE__, __FUNCTION__, format, ## __VA_ARGS__); } while(false);
#else
#define logprintf(format, ...)
#endif
