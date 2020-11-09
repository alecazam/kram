// kram - Copyright 2020 by Alec Miller. - MIT License
// The license and copyright notice shall be included
// in all copies or substantial portions of the Software.

#pragma once

#include <cassert>
#include <string>

namespace kram {

enum LogLevel {
    LogLevelDebug = 0,
    LogLevelInfo = 1,
    LogLevelWarning = 2,
    LogLevelError = 3,
};

extern int logMessage(const char* group, int logLevel,
                      const char* file, int line, const char* func,
                      const char* fmt, ...);

// verify leaves conditional code in the build
#ifdef NDEBUG
#define verify(x) (x)
#else
#define verify(x) assert(x)
#endif

#define KLOGD(group, fmt, ...) logMessage(group, kram::LogLevelDebug, __FILE__, __LINE__, __FUNCTION__, fmt, ##__VA_ARGS__)
#define KLOGI(group, fmt, ...) logMessage(group, kram::LogLevelInfo, __FILE__, __LINE__, __FUNCTION__, fmt, ##__VA_ARGS__)
#define KLOGW(group, fmt, ...) logMessage(group, kram::LogLevelWarning, __FILE__, __LINE__, __FUNCTION__, fmt, ##__VA_ARGS__)
#define KLOGE(group, fmt, ...) logMessage(group, kram::LogLevelError, __FILE__, __LINE__, __FUNCTION__, fmt, ##__VA_ARGS__)

// TODO: move to Strings.h
using namespace std;
int sprintf(string& str, const char* format, ...);

}  // namespace kram
