// kram - Copyright 2020-2023 by Alec Miller. - MIT License
// The license and copyright notice shall be included
// in all copies or substantial portions of the Software.

#pragma once

#include <cassert>
//#include <string>

// #include "KramConfig.h"

namespace kram {

enum LogLevel {
    LogLevelDebug = 0,
    LogLevelInfo = 1,
    LogLevelWarning = 2,
    LogLevelError = 3,
};

// these validate the inputs to any sprintf like format + args
// these come from sys/cdefs.h on Apple, but need to be define for __clang__ on other platforms
#ifndef __printflike
#define __printflike(fmtIndex, varargIndex)
#endif
#ifndef __scanflike
#define __scanflike(fmtIndex, varargIndex)
#endif

extern int32_t logMessage(const char* group, int32_t logLevel,
                          const char* file, int32_t line, const char* func,
                          const char* fmt, ...) __printflike(6, 7);

// verify leaves conditional code in the build
#if KRAM_DEBUG
#define KASSERT(x) assert(x)
#define KVERIFY(x) KASSERT(x)
#else
#define KASSERT(x)
#define KVERIFY(x) (x)
#endif

// save code space, since file/func aren't output for debug/info
#define KLOGD(group, fmt, ...) logMessage(group, kram::LogLevelDebug, /* __FILE__ */ nullptr, __LINE__, /* __FUNCTION__ */ nullptr, fmt, ##__VA_ARGS__)
#define KLOGI(group, fmt, ...) logMessage(group, kram::LogLevelInfo, /* __FILE__ */ nullptr, __LINE__, /* __FUNCTION__ */ nullptr, fmt, ##__VA_ARGS__)
#define KLOGW(group, fmt, ...) logMessage(group, kram::LogLevelWarning, __FILE__, __LINE__, __FUNCTION__, fmt, ##__VA_ARGS__)
#define KLOGE(group, fmt, ...) logMessage(group, kram::LogLevelError, __FILE__, __LINE__, __FUNCTION__, fmt, ##__VA_ARGS__)

// TODO: move to Strings.h
using namespace NAMESPACE_STL;

// when set true, the internal string is cleared
void setErrorLogCapture(bool enable);

bool isErrorLogCapture();

// return the text
void getErrorLogCaptureText(string& text);

//-----------------------
// String Ops

// returns length of string, -1 if failure
int32_t sprintf(string& str, const char* format, ...) __printflike(2, 3);

// returns length of chars appended, -1 if failure
int32_t append_sprintf(string& str, const char* format, ...) __printflike(2, 3);

// returns length of chars appended, -1 if failure
int32_t append_vsprintf(string& str, const char* format, va_list args);

bool startsWith(const char* str, const string& substring);

bool endsWithExtension(const char* str, const string& substring);

// https://stackoverflow.com/questions/874134/find-out-if-string-ends-with-another-string-in-c
bool endsWith(const string& value, const string& ending);

#if KRAM_WIN
size_t strlcat(char* dst, const char* src, size_t size);
size_t strlcpy(char* dst, const char* src, size_t size);
#endif

// Note: never use atoi, it doesn't handle unsigned value with high bit set.
inline int64_t StringToInt64(const char* num)
{
    char* endPtr = nullptr;
    int64_t value = strtol(num, &endPtr, 10);
    return value;
}

inline uint64_t StringToUInt64(const char* num)
{
    char* endPtr = nullptr;
    uint64_t value = strtoul(num, &endPtr, 10);
    return value;
}

inline int32_t StringToInt32(const char* num)
{
    return (int32_t)StringToInt64(num);
}

inline uint32_t StringToUInt32(const char* num)
{
    return (int32_t)StringToUInt64(num);
}


}  // namespace kram
