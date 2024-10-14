// kram - Copyright 2020-2023 by Alec Miller. - MIT License
// The license and copyright notice shall be included
// in all copies or substantial portions of the Software.

#if !KRAM_VISION // this is breaking link on visionOS

#pragma once

#include <cassert>
//#include <string>

//#include "KramConfig.h"

#include "KramLog.h"
#include "core.h" // really fmt/core.h
#include "format.h" // really fmt/format.h - for FMT_STRING

// Note this is split off from KramLog.h, just because fmt pulls in so
// much template and inline formating code.

namespace kram {

int32_t logMessage(const char* group, int32_t logLevel,
                   const char* file, int32_t line, const char* func,
                   fmt::string_view format, fmt::format_args args);

// This is a way to convert to single function call, so handling
// can be buriend within that.
template <typename S, typename... Args>
int32_t logMessageConverter(const char* group, int32_t logLevel,
                            const char* file, int32_t line, const char* func,
                            const S& format, Args&&... args)
{
    return logMessage(group, logLevel, file, line, func,
                      format, fmt::make_format_args(args...));
}

// save code space, since file/func aren't output for debug/info
#define KLOGFD(group, fmt, ...) \
    logMessageConverter(group, kram::LogLevelDebug, /* __FILE__ */ nullptr, __LINE__, /* __FUNCTION__ */ nullptr, FMT_STRING(fmt), ##__VA_ARGS__)
#define KLOGFI(group, fmt, ...) \
    logMessageConverter(group, kram::LogLevelInfo, /* __FILE__ */ nullptr, __LINE__, /* __FUNCTION__ */ nullptr, FMT_STRING(fmt), ##__VA_ARGS__)
#define KLOGFW(group, fmt, ...) \
    logMessageConverter(group, kram::LogLevelWarning, __FILE__, __LINE__, __FUNCTION__, FMT_STRING(fmt), ##__VA_ARGS__)
#define KLOGFE(group, fmt, ...) \
    logMessageConverter(group, kram::LogLevelError, __FILE__, __LINE__, __FUNCTION__, FMT_STRING(fmt), ##__VA_ARGS__)

// fmt already has versions of these in printf.h, but want buried impls

// returns length of string, -1 if failure
int32_t sprintf_impl(string& str, fmt::string_view format, fmt::format_args args);

// returns length of chars appended, -1 if failure
int32_t append_sprintf_impl(string& str, fmt::string_view format, fmt::format_args args);

// This is a way to convert to single function call, so handling
// can be buried within that.  May need to wrap format with FMT_STRING
template <typename S, typename... Args>
int32_t sprintf_fmt(string& s, const S& format, Args&&... args)
{
    return sprintf_impl(s, format, fmt::make_format_args(args...));
}

template <typename S, typename... Args>
int32_t append_sprintf_fmt(string& s, const S& format, Args&&... args)
{
    return append_sprintf_impl(s, format, fmt::make_format_args(args...));
}

} // namespace kram

#endif
