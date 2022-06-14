// kram - Copyright 2020-2022 by Alec Miller. - MIT License
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
namespace kram {

using mymutex = std::recursive_mutex;
using mylock = std::unique_lock<mymutex>;

using namespace NAMESPACE_STL;

static mymutex gLogLock;
static string gErrorLogCaptureText;
static bool gIsErrorLogCapture = false;
void setErrorLogCapture(bool enable)
{
    gIsErrorLogCapture = enable;
    if (enable) {
        mylock lock(gLogLock);
        gErrorLogCaptureText.clear();
    }
}
bool isErrorLogCapture() { return gIsErrorLogCapture; }

// return the text
void getErrorLogCaptureText(string& text)
{
    if (gIsErrorLogCapture) {
        mylock lock(gLogLock);
        text = gErrorLogCaptureText;
    }
    else {
        text.clear();
    }
}

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

        vsnprintf((char*)str.data() + existingLen, len + 1, format, args);
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
    return equal(ending.rbegin(), ending.rend(), value.rbegin());
}

bool endsWithExtension(const char* str, const string& substring)
{
    const char* search = strrchr(str, '.');
    if (search == NULL) {
        return false;
    }

    return strcmp(search, substring.c_str()) == 0;
}

extern int32_t logMessage(const char* group, int32_t logLevel,
                          const char* file, int32_t line, const char* func,
                          const char* fmt, ...)
{
    // TOOD: add any filtering up here

    // convert var ags to a msg
    const char* msg;

    string str;
    if (strrchr(fmt, '%') == nullptr) {
        msg = fmt;
    }
    else {
        va_list args;
        va_start(args, fmt);
        vsprintf(str, fmt, args);
        va_end(args);

        msg = str.c_str();
    }

    // pipe to correct place, could even be file output
    FILE* fp = stdout;
    if (logLevel >= LogLevelWarning)
        fp = stderr;

    // see if newline required
    int32_t len = (int32_t)strlen(fmt);
    bool needsNewline = false;
    if (len >= 1)
        needsNewline = fmt[len - 1] != '\n';

    if (needsNewline) {
        len = (int32_t)strlen(msg);
        if (len >= 1)
            needsNewline = msg[len - 1] != '\n';
    }

    // format soem strings to embellish the message
    const char* tag = "";
    const char* groupString = "";
    const char* space = "";

    string fileLineFunc;
    switch (logLevel) {
        case LogLevelDebug:
            //tag = "[D]";
            break;
        case LogLevelInfo:
            //tag = "[I]";
            break;

        case LogLevelWarning:
            tag = "[W]";
            groupString = group;
            space = " ";

            break;
        case LogLevelError: {
            tag = "[E]";
            groupString = group;
            space = " ";

#if KRAM_WIN
            const char fileSeparator = '\\';
#else
            const char fileSeparator = '/';
#endif

            // shorten filename
            const char* filename = strrchr(file, fileSeparator);
            if (filename) {
                filename++;
            }
            else {
                filename = file;
            }

            // TODO: in clang only __PRETTY_FUNCTION__ has namespace::className::function(args...)
            // __FUNCTION__ is only the function call, but want class name if it's a method without going to RTTI
            // https://stackoverflow.com/questions/1666802/is-there-a-class-macro-in-c
            sprintf(fileLineFunc, "@%s(%d): %s()\n", filename, line, func);
            break;
        }
        default:
            break;
    }

    // stdout isn't thread safe, so to prevent mixed output put this under mutex
    mylock lock(gLogLock);

    // this means caller needs to know all errors to display in the hud
    if (gIsErrorLogCapture && logLevel == LogLevelError) {
        gErrorLogCaptureText += msg;
        if (needsNewline) {
            gErrorLogCaptureText += "\n";
        }
    }

    // format into a buffer
    static string buffer;
    sprintf(buffer, "%s%s%s%s%s%s", tag, groupString, space, msg, needsNewline ? "\n" : "", fileLineFunc.c_str());
    
#if KRAM_WIN
    if (::IsDebuggerPresent()) {
        // TODO: split string up into multiple logs
        // this is limited to 32K
        OutputDebugString(buffer.c_str());
    }
    else {
        // avoid double print to debugger
        fprintf(fp, "%s", buffer.c_str());
    }
#elif KRAM_ANDROID
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
    
    // TODO: can also fix printf to work on Android
    // but can't set log level like with this call, but no dump buffer limit
    
    // TODO: split string up into multiple logs
    // this can only write 4K - 40? chars at time, don't use print it's 1023
    __android_log_write(androidLogLevel, tag, buffer.c_str());
#else
    fprintf(fp, "%s", buffer.c_str());
#endif

    return 0;  // reserved for later
}

}  // namespace kram
