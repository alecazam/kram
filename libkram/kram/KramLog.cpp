// kram - Copyright 2020-2023 by Alec Miller. - MIT License
// The license and copyright notice shall be included
// in all copies or substantial portions of the Software.

#include "KramLog.h"

//#include <string>

// for Win
#include <stdarg.h>

#include <mutex>

#if KRAM_WIN
#include <windows.h>
#elif KRAM_ANDROID
#include <log.h>
#endif

#include "KramFmt.h"
#include "KramTimer.h"
#include "format.h" // really fmt/format.h

namespace kram {

// Pulled in from TaskSystem.cpp
constexpr const uint32_t kMaxThreadName = 32;
extern void getCurrentThreadName(char name[kMaxThreadName]);

using mymutex = std::recursive_mutex;
using mylock = std::unique_lock<mymutex>;

using namespace NAMESPACE_STL;



// TODO: install assert handler to intercept, and also add a verify (assert that leaves source in)
//void __assert(const char *expression, const char *file, int32_t line) {
//
//}

// Note: careful with stdio sscanf.  In clang, this does an initial strlen which for long buffers
// being parsed (f.e. mmapped Json) can significantly slow a parser down.

int32_t append_vsprintf(string& str, const char* format, va_list args)
{
    // for KLOGE("group", "%s", "text")
    if (strcmp(format, "%s") == 0) {
        const char* firstArg = va_arg(args, const char*);
        str += firstArg;
        return (int32_t)strlen(firstArg);
    }

    // This is important for the case where ##VAR_ARGS only leaves the format.
    // In this case "text" must be a compile time constant string to avoid security warning needed for above.
    // for KLOGE("group", "text")
    if (strrchr(format, '%') == nullptr) {
        str += format;
        return (int32_t)strlen(format);
    }

    // format once to get length (without NULL at end)
    va_list argsCopy;
    va_copy(argsCopy, args);
    int32_t len = vsnprintf(NULL, 0, format, argsCopy);
    va_end(argsCopy);

    if (len > 0) {
        size_t existingLen = str.length();

        // resize and format again into string
        str.resize(existingLen + len, 0);

        vsnprintf((char*)str.c_str() + existingLen, len + 1, format, args);
    }

    return len;
}

static int32_t vsprintf(string& str, const char* format, va_list args)
{
    str.clear();
    return append_vsprintf(str, format, args);
}

int32_t sprintf(string& str, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    int32_t len = vsprintf(str, format, args);
    va_end(args);

    return len;
}

int32_t append_sprintf(string& str, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    int32_t len = append_vsprintf(str, format, args);
    va_end(args);

    return len;
}

//----------------------------------

static size_t my_formatted_size(fmt::string_view format, fmt::format_args args)
{
    auto buf = fmt::detail::counting_buffer<>();
    fmt::detail::vformat_to(buf, format, args, {});
    return buf.count();
}

// returns length of chars appended, -1 if failure
int32_t append_sprintf_impl(string& str, fmt::string_view format, fmt::format_args args)
{
    size_t size = my_formatted_size(format, args);
    
    // TODO: write directly to end of str
    string text = vformat(format, args);
    
    // this does all formatting work
    str.resize(str.size() + size);
    str.insert(str.back(), text);
    
    return size; // how many chars appended, no real failure case yet
}

// returns length of string, -1 if failure
int32_t sprintf_impl(string& str, fmt::string_view format, fmt::format_args args)
{
    str.clear();
    return append_sprintf_impl(str, format, args);
}

//----------------------------------

bool startsWith(const char* str, const string& substring)
{
    return strncmp(str, substring.c_str(), substring.size()) == 0;
}

// https://stackoverflow.com/questions/874134/find-out-if-string-ends-with-another-string-in-c
bool endsWith(const string& value, const string& ending)
{
    if (ending.size() > value.size()) {
        return false;
    }

    // reverse comparison at end of value
    if (value.size() < ending.size())
        return false;
    uint32_t start = value.size() - ending.size();
        
    for (uint32_t i = 0; i < ending.size(); ++i) {
        if (value[start + i] != ending[i])
            return false;
    }
    
    return true;
}

bool endsWithExtension(const char* str, const string& substring)
{
    const char* search = strrchr(str, '.');
    if (search == NULL) {
        return false;
    }

    return strcmp(search, substring.c_str()) == 0;
}

//----------------------------------

#if KRAM_WIN
// Adapted from Emil Persson's post.
// https://twitter.com/_Humus_/status/1629165133359460352
// ODS that supports UTF8 characters
inline void OutputDebugStringU(LPCSTR lpOutputString, uint32_t len8)
{
    // empty string
    if (len8 == 0) return;
    
    // Run the conversion twice, first to get length, then to do the conversion
    int len16 = MultiByteToWideChar(CP_UTF8, 0, lpOutputString, (int)len8, nullptr, 0);
    
    // watch out for large len16
    if (len16 == 0 || len16 > 128*1024) return;
    
    wchar_t* strWide = (wchar_t*)_malloca(len16 * sizeof(wchar_t));
    
    // ran out of stack
    if (!strWide) return;
    
    MultiByteToWideChar(CP_UTF8, 0, lpOutputString, (int)len8, strWide, len16);
    
    ULONG_PTR args[4] = {
        (ULONG_PTR)len16 + 1, (ULONG_PTR)strWide,
        (ULONG_PTR)len8 + 1, (ULONG_PTR)lpOutputString
    };
    
    // TODO: note that there is a limit to the length of this string
    // so may want to split up the string in a loop.
    
    RaiseException(0x4001000A, 0, 4, args); // DBG_PRINTEXCEPTION_WIDE_C
    
    _freea(strWide);
    
    // Can't use OutputDebugStringW.
    // OutputDebugStringW converts the specified string based on the current system
    // locale information and passes it to OutputDebugStringA to be displayed. As a
    // result, some Unicode characters may not be displayed correctly.  So above is
    // what ODSA calls internally but with the action wide string.
}

#endif

struct LogState
{
    mymutex lock;
    string errorLogCaptureText;
    string buffer;
    bool isErrorLogCapture = false;
    uint32_t counter = 0;
    
#if KRAM_WIN
    bool isWindowsSubsystemApp = false;
    bool isWindowsDebugger = false;
#endif
};
static LogState gLogState;

void setErrorLogCapture(bool enable)
{
    gLogState.isErrorLogCapture = enable;
    if (enable) {
        mylock lock(gLogState.lock);
        gLogState.errorLogCaptureText.clear();
    }
}

bool isErrorLogCapture() { return gLogState.isErrorLogCapture; }

// return the text
void getErrorLogCaptureText(string& text)
{
    if (gLogState.isErrorLogCapture) {
        mylock lock(gLogState.lock);
        text = gLogState.errorLogCaptureText;
    }
    else {
        text.clear();
    }
}

struct LogMessage
{
    const char* group;
    int32_t logLevel;
    
    const char* file;
    int32_t line;
    const char* func;
    const char* threadName;
    
    const char* msg;
    bool msgHasNewline;
    
    double timestamp;
};

static const char* getFormatTokens(const LogMessage& msg) {
#if KRAM_WIN
    if (msg.logLevel <= LogLevelInfo)
        return "m\n";
    if (msg.file)
        return "[l] g m\n"
               "F: L: t u\n";
    return "[l] g m\n";
#elif KRAM_ANDROID
    return "m\n";
#else
    // copy of formatters above
    if (msg.logLevel <= LogLevelInfo)
        return "m\n";
    if (msg.file)
        return "[l] g m\n"
               "F: L: t u\n";
    return "[l] g m\n";
#endif
}


static void formatMessage(string& buffer, const LogMessage& msg, const char* tokens)
{
    buffer.clear();
   
    char c = 0;
    while ((c = *tokens++) != 0) {
        switch(c) {
            case ' ':
            case ':':
            case '[':
            case ']':
            case '\n':
                buffer += c;
                break;
                
            case 'l':
            case 'L': { // level
                bool isVerbose = c == 'L';
                const char* level = "";
                switch(msg.logLevel) {
                    case LogLevelDebug:
                        level = isVerbose ? "debug" : "D";
                        break;
                    case LogLevelInfo:
                        level = isVerbose ? "info" : "I";
                        break;
                    case LogLevelWarning:
                        level = isVerbose ? "warning" : "W";
                        break;
                    case LogLevelError:
                        level = isVerbose ? "error" : "E";
                        break;
                }
                buffer += level;
                break;
            }
            case 'g': { // group
                buffer += msg.group;
                break;
            }
                
            case 'u': { // func
                if (msg.func) {
                    buffer += msg.func;
                    int32_t len = (int32_t)strlen(msg.func);
                    if (len > 1 && msg.func[len-1] != ']')
                        buffer += "()";
                }
                break;
            }
                
            case 'd': { // date/timestamp
                if (msg.timestamp != 0.0) {
                    append_sprintf(buffer, "%f", msg.timestamp);
                }
                break;
            }
                
            case 't': { // thread
                if (msg.threadName) {
                    buffer += msg.threadName;
                }
                break;
            }
            case 'm': { // message
                if (msg.msg) {
                    // strip any trailing newline, should be in the tokens
                    buffer += msg.msg;
                    if (msg.msgHasNewline)
                        buffer.pop_back();
                }
                break;
            }
                
            case 'f': // file:line
            case 'F': {
                if (msg.file) {
#if KRAM_WIN
                    const char fileSeparator = '\\';
#else
                    const char fileSeparator = '/';
#endif
                    bool isVerbose = c == 'L';
                    
                    const char* filename = msg.file;
                    
                    // shorten filename
                    if (!isVerbose) {
                        const char* shortFilename = strrchr(filename, fileSeparator);
                        if (shortFilename) {
                            shortFilename++;
                            filename = shortFilename;
                        }
                    }
                    
#if KRAM_WIN
                    // format needed for Visual Studio to collect/clickthrough
                    append_sprintf(buffer, "%s(%d)", filename, msg.line);
#else
                    // format needed for Xcode/VSCode to print
                    append_sprintf(buffer, "%s:%d", filename, msg.line);
#endif
                }
                break;
            }
        }
    }
}



void setMessageFields(LogMessage& msg, char threadName[kMaxThreadName])
{
    const char* text = msg.msg;
    
    msg.msgHasNewline = false;
    int32_t len = (int32_t)strlen(text);
    if (len >= 1 && text[len - 1] == '\n')
        msg.msgHasNewline = true;
    
    // Note: this could analyze the format tokens for all reporters.
    // Also may want a log file with own formatting/fields.
#if KRAM_ANDROID
    // logcat only needs the tag, file, line, message
    return;
#else
    // only reporting message on info/debug
    if (msg.logLevel <= LogLevelInfo)
        return;
#endif
    
    // fill out thread name
    getCurrentThreadName(threadName);
    if (threadName[0] != 0)
        msg.threadName = threadName;
    
    // retrieve timestamp
    msg.timestamp = currentTimestamp();
}

static int32_t logMessageImpl(const LogMessage& msg)
{
    // TODO: add any filtering up here, or before msg is built
    
    mylock lock(gLogState.lock);

    // this means caller needs to know all errors to display in the hud
    if (gLogState.isErrorLogCapture && msg.logLevel == LogLevelError) {
        gLogState.errorLogCaptureText += msg.msg;
        if (!msg.msgHasNewline)
            gLogState.errorLogCaptureText += "\n";
    }

    // format into a buffer (it's under lock, so can use static)
    string& buffer = gLogState.buffer;
    
    gLogState.counter++;
    
    int32_t status = (msg.logLevel == LogLevelError) ? 1 : 0;
    
#if KRAM_WIN
    
    // This is only needed for Window subsystem.
    // Assumes gui app didn't call AllocConsole.
    // TODO: keep testing IsDebuggerPresent from time to time for attach
    if (gLogState.counter == 1) {
        gLogState.isWindowsSubsystemApp = GetStdHandle( ) == nullptr;
        gLogState.isWindowsDebugger = ::IsDebuggerPresent();
    }
    
    if (gLogState.isWindowsSubsystemApp && !gLogState.isWindowsDebugger)
        return status;
    
    formatMessage(buffer, msg, getFormatTokens(msg));
    
    if (gLogState.isWindowsSubsystemApp) {
        // TODO: split string up into multiple logs
        // this is limited to 32K
        // OutputDebugString(buffer.c_str());
        
        // This supports UTF8 strings by converting them to wide.
        // TODO: Wine doesn't handle.
        OutputDebugStringU(buffer.c_str(), buffer.size());
    }
    else {
        // avoid double print to debugger
        FILE* fp = stdout;
        fwrite(buffer.c_str(), 1, buffer.size(), fp);
        // if heavy logging, then could delay fflush
        fflush(fp);
    }
#elif KRAM_ANDROID
    // TODO: move higher up
    // API 30
    if (!__android_log_is_loggable(androidLogLevel, msg.group, androidLogLevel))
        return status;
    
    formatMessage(buffer, msg, getFormatTokens(msg));
    
    AndroidLogLevel androidLogLevel = ANDROID_LOG_ERROR;
    switch (logLevel) {
        case LogLevelDebug:
            androidLogLevel = ANDROID_LOG_DEBUG;
            break;
        case LogLevelInfo:
            androidLogLevel = ANDROID_LOG_INFO;
            break;

        case LogLevelWarning:
            androidLogLevel = ANDROID_LOG_WARNING;
            break;
        case LogLevelError:
            androidLogLevel = ANDROID_LOG_ERROR;
            break;
    }
    
    // TODO: split string up into multiple logs by /n
    // this can only write 4K - 80 chars at time, don't use print it's 1023
    // API 30
    __android_log_message msg = {
        LOG_ID_MAIN, msg.file, msg.line, buffer.c_str(), androidLogLevel, sizeof(__android_log_message), msg.group);
    }
    __android_log_write_log_message(msg);
#else
    // Note: this doesn't go out to Console, but does go out to Xcode.
    // NSLog/os_log sucks with no intercepts, filtering, or formatting.
    
    formatMessage(buffer, msg, getFormatTokens(msg));
    
    FILE* fp = stdout;
    fwrite(buffer.c_str(), 1, buffer.size(), fp);
    // if heavy logging, then could delay fflush
    fflush(fp);
#endif

    return status;  // reserved for later
}

int32_t logMessage(const char* group, int32_t logLevel,
                          const char* file, int32_t line, const char* func,
                          const char* fmt, ...)
{
    // convert var ags to a msg
    const char* msg;

    string str;
    if (strrchr(fmt, '%') == nullptr) {
        msg = fmt;
    }
    else if (strcmp(fmt, "%s") == 0) {
        // hope can reference this past va_end
        va_list args;
        va_start(args, fmt);
        msg = va_arg(args, const char*);
        va_end(args);
    }
    else {
        va_list args;
        va_start(args, fmt);
        vsprintf(str, fmt, args);
        va_end(args);

        msg = str.c_str();
    }
    
    LogMessage logMessage = {
        group, logLevel,
        file, line, func, nullptr,
        msg, false, 0.0
    };
    char threadName[kMaxThreadName] = {};
    setMessageFields(logMessage, threadName);
    return logMessageImpl(logMessage);
}


// This is the api reference for fmt.
// Might be able to use std::format in C++20 instead, but nice
// to have full source to impl to fix things in fmt.
// https://fmt.dev/latest/api.html#_CPPv4IDpEN3fmt14formatted_sizeE6size_t13format_stringIDp1TEDpRR1T

// TODO: can this use NAMESPACE_STL::string_view instead ?
int32_t logMessage(const char* group, int32_t logLevel,
                          const char* file, int32_t line, const char* func,
                          fmt::string_view format, fmt::format_args args)
{
    // TODO: size_t size = std::formatted_size(format, args);
    // and then reserve that space in str.  Use that for impl of append_format.
    // can then append to existing string (see vsprintf)
    
    string str = fmt::vformat(format, args);
    const char* msg = str.c_str();
    
    LogMessage logMessage = {
        group, logLevel,
        file, line, func, nullptr,
        msg, false, 0.0
    };
    char threadName[kMaxThreadName] = {};
    setMessageFields(logMessage, threadName);
    return logMessageImpl(logMessage);
}

}  // namespace kram
