// kram - Copyright 2020 by Alec Miller. - MIT License
// The license and copyright notice shall be included
// in all copies or substantial portions of the Software.

#include "KramLog.h"

#include <string>

// for Win
#include <stdarg.h>

#include <mutex>

namespace kram {
using namespace std;

// TODO: install assert handler to intercept, and also add a verify (assert that leaves source in)
//void __assert(const char *expression, const char *file, int line) {
//
//}

static int vsprintf(string& str, const char* format, va_list args)
{
    if (strchr(format, '%') == nullptr) {
        str = format;
        return (int)str.length();
    }

    // can't reuse args after vsnprintf
    va_list argsCopy;
    va_copy(argsCopy, args);

    // format once to get length (without NULL at end)
    int len = vsnprintf(NULL, 0, format, argsCopy);

    if (len > 0) {
        // resize and format again into string
        str.resize(len);

        vsnprintf(&str[0], len + 1, format, args);
    }
    return len;
}

int sprintf(string& str, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    int len = vsprintf(str, format, args);
    va_end(args);

    return len;
}

bool startsWith(const char *str, const string &substring) {
    return strncmp(str, substring.c_str(), substring.size()) == 0;
}

// https://stackoverflow.com/questions/874134/find-out-if-string-ends-with-another-string-in-c
bool endsWith(const string& value, const string& ending)
{
    if (ending.size() > value.size()) {
        return false;
    }
    return equal(ending.rbegin(), ending.rend(), value.rbegin());
}

bool endsWithExtension(const char *str, const string &substring) {
    const char *search = strrchr(str, '.');
    if (search == NULL ) {
        return false;
    }
    
    return strcmp(search, substring.c_str()) == 0;
}

extern int logMessage(const char* group, int logLevel,
                      const char* file, int line, const char* func,
                      const char* fmt, ...)
{
    // TOOD: add any filtering up here

    // convert var ags to a msg
    const char* msg;
    string str;

    va_list args;
    va_start(args, fmt);
    if (strstr(fmt, "%") == nullptr) {
        msg = fmt;
    }
    else {
        va_start(args, fmt);
        vsprintf(str, fmt, args);
        va_end(args);

        msg = str.c_str();
    }
    va_end(args);

    // pipe to correct place, could even be file output
    FILE* fp = stdout;
    if (logLevel >= LogLevelWarning)
        fp = stderr;

    // see if newline required
    int len = (int)strlen(fmt);
    bool needsNewline = false;
    if (len >= 1)
        needsNewline = fmt[len - 1] != '\n';

    if (needsNewline) {
        len = (int)strlen(msg);
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

#if _WIN32
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
    static mutex gLogLock;
    unique_lock<mutex> lock(gLogLock);

    fprintf(fp, "%s%s%s%s%s%s", tag, groupString, space, msg, needsNewline ? "\n" : "", fileLineFunc.c_str());

    return 0;  // reserved for later
}

}  // namespace kram
