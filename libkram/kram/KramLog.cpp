// kram - Copyright 2020-2023 by Alec Miller. - MIT License
// The license and copyright notice shall be included
// in all copies or substantial portions of the Software.

#include "KramLog.h"

//#include <string>

#if KRAM_IOS || KRAM_MAC
#define KRAM_LOG_STACKTRACE KRAM_DEBUG
#elif KRAM_WIN
// TODO: need to debug code before enabling
#define KRAM_LOG_STACKTRACE 0 // KRAM_DEBUG
#else
#define KRAM_LOG_STACKTRACE 0 // KRAM_DEBUG
#endif

// for Win
#include <stdarg.h>

#include <mutex>

#if KRAM_WIN
#include <windows.h>
#include <intrin.h> // for AddressOfReturnAdress, ReturnAddress

#if KRAM_LOG_STACKTRACE
// There is a DbgHelp.lib that is redistributable
#include <dbghelp.h>
#pragma comment(lib, "DbgHelp.lib");
#endif

#elif KRAM_ANDROID
#include <log.h>

#elif KRAM_IOS || KRAM_MAC
#include <os/log.h>
#include <cxxabi.h> // demangle
#include <dlfcn.h>  // address to symbol
#include <execinfo.h>
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


#if KRAM_WIN
// https://stackoverflow.com/questions/18547251/when-i-use-strlcpy-function-in-c-the-compilor-give-me-an-error

// '_cups_strlcat()' - Safely concatenate two strings.
size_t                    // O - Length of string
strlcat(char       *dst,  // O - Destination string
        const char *src,  // I - Source string
        size_t     size)  // I - Size of destination string buffer
{
  size_t    srclen;         // Length of source string
  size_t    dstlen;         // Length of destination string


   // Figure out how much room is left...
  dstlen = strlen(dst);
  size   -= dstlen + 1;

  if (!size)
    return (dstlen);        // No room, return immediately...

  // Figure out how much room is needed...
  srclen = strlen(src);

  // Copy the appropriate amount...
  if (srclen > size)
    srclen = size;

  memcpy(dst + dstlen, src, srclen);
  dst[dstlen + srclen] = '\0';

  return (dstlen + srclen);
}

// '_cups_strlcpy()' - Safely copy two strings.
size_t                          // O - Length of string
strlcpy(char       *dst,        // O - Destination string
        const char *src,        // I - Source string
        size_t      size)       // I - Size of destination string buffer
{
  size_t    srclen; // Length of source string


  // Figure out how much room is needed...
  size --;

  srclen = strlen(src);

  // Copy the appropriate amount...
  if (srclen > size)
    srclen = size;

  memcpy(dst, src, srclen);
  dst[srclen] = '\0';

  return (srclen);
}
#endif

#if KRAM_LOG_STACKTRACE

#if KRAM_WIN
// https://learn.microsoft.com/en-us/windows/win32/debug/retrieving-symbol-information-by-address?redirectedfrom=MSDN
class AddressHelper
{
private:
    
    HANDLE m_process = 0;
    
public:
    AddressHelper()
    {
        m_process = GetCurrentProcess();
        
        // produces line number and demangles name
        SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME);
        
        // load the symbols
        SymInitialize(m_process, NULL, TRUE);
    }
    
    ~AddressHelper()
    {
        SymCleanup(m_process);
    }
    
    bool isStackTraceSupported() const { return true; }
    
    bool getAddressInfo(const void* address, string& symbolName, string& filename, uint32_t& line)
    {
        string.clear();
        filename.clear();
        line = 0;
        
        IMAGEHLP_LINE64 loc = {};
        loc.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
        DWORD  displacement = 0;
        
        // This grabs the symbol name
        char buffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME * sizeof(TCHAR)] = {};
        SYMBOL_INFO& symbol = *(SYMBOL_INFO*)buffer;
        symbol.SizeOfStruct = sizeof(SYMBOL_INFO);
        symbol.MaxNameLen = MAX_SYM_NAME;
        SymFromAddr(process, (ULONG64)address, &displacement, &symbol);
        symbolName = symbol.Name;
        
        // all demangle ops are single-threaded, so run this under log mutex
        if (!SymGetLineFromAddr64(m_process, (DWORD64)address, &displacement, &loc))
            return false;
        
        filename = loc.Filename;
        line = loc.LineNumber;
        return true;
    }
    
    void getStackInfo(string& stackInfo, uint32_t skipStackLevels)
    {
        string symbolName, filename;
        uint32_t line = 0;
        
        const uint32_t kMaxStackTrace = 128;
        void* stacktrace[kMaxStackTrace] = {};
        
        // Can use this hash to uniquely identify stacks that are the same
        ULONG stackTraceHash = 0;
        
        // This provides the symbols
        uint32_t frameCount = CaptureStackBackTrace(skipStackLevels, kMaxStackTrace, stacktrace, &stackTraceHash);
        
        for(uint32_t i = 0; i < frameCount; ++i) {
            if (getAddressInfo(stacktrace[i], symbolName, filename, line))
                append_sprintf(stackInfo, "%s:%u: %s", filename.c_str(), line, symbolName.c_str());
            
            // Note: can get Module with a different call if above fails
        }
    }
    
    // See here on using StackWalk64 to walk up the SEH context.
    // https://stackoverflow.com/questions/22467604/how-can-you-use-capturestackbacktrace-to-capture-the-exception-stack-not-the-ca
};

// Check this out
// https://github.com/vtjnash/dbghelp2

// Also here, cross-platform symbol lookup/demangle
// handles ASLR on mach-o, but only using relative offsets on the .o file
// https://dynamorio.org/page_drsyms.html

// https://stackoverflow.com/questions/22467604/how-can-you-use-capturestackbacktrace-to-capture-the-exception-stack-not-the-ca
// CaptureStackBackTrace().

#else

// here's nm on osx
// https://opensource.apple.com/source/cctools/cctools-622.5.1/misc/nm.c


// The dladdr() function is available only in dynamically linked programs.
// #include <dlfcn.h>
// int dladdr(const void *addr, Dl_info *info);
// int dladdr1(const void *addr, Dl_info *info, void **extra_info,
//                   int flags);
// Here's supposed to be code that deals with static libs too
// https://stackoverflow.com/questions/19848567/how-to-get-the-build-uuid-in-runtime-and-the-image-base-address/19859516#19859516

class AddressHelper
{
private:
    void substr(string& str, const char* start, const char* end) {
        str = str.substr(start - str.c_str(), end - start);
    }
    
    const char* strrchr(const char* start, const char* end, char c) {
        while (end > start) {
            end--;
            if (*end == c)
                return end;
        }
        
        return nullptr;
    }
    
    void demangleSymbol(string& symbolName)
    {
        size_t size = 0;
        int status = 0;
        
        // This one is getting chopped up incorrect
        // 10  AppKit                              0x0000000193079730 __24-[NSViewController view]_block_invoke + 28
        
        // Some other examples
        // 14  AppKit                              0x0000000192c7b230 NSPerformVisuallyAtomicChange + 108
        // 24  kramv                               0x0000000104c6b4e0 main + 76
        
        const char* text = symbolName.c_str();
        // chop off the "+ 132" offset
        const char* plusOffsetEnd = strstr(text, " +");
        const char* objCStart = strstr(text, " -");
        if (!objCStart)
            objCStart = strstr(text, " __24-"); // block invoke
        
        const char* cppStart = strstr(text, " _ZN4");
        const char* spaceStart = plusOffsetEnd ? strrchr(text, plusOffsetEnd, ' ') : nullptr;
        
        if (objCStart)
            substr(symbolName, objCStart+1, plusOffsetEnd);
        else if (cppStart)
            substr(symbolName, cppStart+1, plusOffsetEnd);
        else if (spaceStart)
            substr(symbolName, spaceStart+1, plusOffsetEnd);
        
        // Note: some objC does need demangle
        if (cppStart) {
            // This allocates memory using malloc
            // Must have just the name, not all the other cruft around it
            // ObjC is not manged.
            char* symbol = abi::__cxa_demangle(symbolName.c_str(), nullptr, &size, &status);
            if (status == 0) {
                symbolName = symbol;
                free(symbol);
            }
        }
    }
    
public:
    bool isStackTraceSupported() const { return true; }
   
    bool getAddressInfo(const void* address, string& symbolName, string& filename, uint32_t& line)
    {
        void* callstack[1] = { (void*)address };
        
        // this allocates memory
        char** strs = backtrace_symbols(callstack, 1);
        
        // may need -no_pie to turn off ASLR, also don't reuse stack-frame reg
        // Will have to parse symbolName, filename, line
        symbolName = strs[0];
        
        free(strs);
        
        // TODO: figure out file/line lookup, don't want to fire nm/addr2line process each time
        // Those are GPL poison into codebases.  But they no doubt do a ton
        // or work on each launch to then lookup 1+ symbols.
        // Apple doesn't even have addr2line, and needs to use atos. 
        // But atos doesn't exist except on dev systems.
        // atos goes to private framework CoreSymbolication.  Ugh.
        // There is also boost::stack_trace which does gen a valid stack somehow.
        
        // CoreSymbolicate might have calls
        // https://opensource.apple.com/source/xnu/xnu-3789.21.4/tools/tests/darwintests/backtracing.c.auto.html
        
        // https://developer.apple.com/documentation/xcode/adding-identifiable-symbol-names-to-a-crash-report
        // https://developer.apple.com/documentation/xcode/analyzing-a-crash-report
        
        // Note: this can provide the file/line, but requires calling out to external process
        // also nm and addr2line
        // system("atos -o kramv.app.dSYM/Contents/Resources/DWARF/kramv -arch arm64 -l %p", address);
        
        filename.clear();
        line = 0;
        
        demangleSymbol(symbolName);
        
        return true;
    }
    
    void getStackInfo(string& stackInfo, uint32_t skipStackLevels)
    {
        void* callstack[128];
        uint32_t frames = backtrace(callstack, 128);
        
        // Also this call, but can't use it to lookup a symbol, and it's ObjC.
        // but it just returns the same data as below (no file/line).
        // +[NSThread callStackSymbols]
        
        // backtrace_symbols() attempts to transform a call stack obtained by
        // backtrace() into an array of human-readable strings using dladdr().
        char** strs = backtrace_symbols(callstack, frames);
        string symbolName;
        for (uint32_t i = skipStackLevels; i < frames; ++i) {
            symbolName = strs[i];
            
            demangleSymbol(symbolName);
            
            append_sprintf(stackInfo, "[%2u] ", i-skipStackLevels);
            stackInfo += symbolName;
            stackInfo += "\n";
        }
        
        free(strs);
    }
    
    // nm is typically used to decode, but that's an executable
};
#endif

#else

class AddressHelper
{
public:
    bool isStackTraceSupported() const { return false; }
    bool getAddressInfo(const void* address, string& symbolName, string& filename, uint32_t& line) { return false; }
    void getStackInfo(string& stackInfo, uint32_t skipStackLevels) {}
};

#endif

static AddressHelper gAddressHelper;


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
    bool isWindowsGuiApp = false; // default isConsole
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
    
    // from macro
    const char* file;
    int32_t line;
    const char* func;
    
    // embelished
    const char* threadName;
    double timestamp;
    
    void* dso;
    void* returnAddress;
    
    const char* msg;
    bool msgHasNewline;
};

enum DebuggerType
{
    DebuggerOutputDebugString,
    DebuggerOSLog,
    DebuggerLogcat,
    Debugger,
};

constexpr const uint32_t kMaxTokens = 32;

static const char* getFormatTokens(char tokens[kMaxTokens], const LogMessage& msg, DebuggerType type) 
{
#if KRAM_WIN
    if (msg.logLevel <= LogLevelInfo) {
        strlcpy(tokens, "m\n", kMaxTokens);
    }
    else if (msg.file) {
        strlcpy(tokens, "[l] g m\n" "F: L: t u\n", kMaxTokens);
    }
    else {
        strlcpy(tokens, "[l] g m\n", kMaxTokens);
    }
#elif KRAM_ANDROID
    // Android logcat has level, tag, file/line passed in the mesasge
   strlcpy(tokens, "m\n", kMaxTokens);
#else
    // copy of formatters above
    if (msg.logLevel <= LogLevelInfo) {
        strlcpy(tokens, "m\n", kMaxTokens);
    }
    else if (msg.file) {
        strlcpy(tokens, "[l] g m\n" "F: L: t u\n", kMaxTokens);
    }
    else {
        strlcpy(tokens, "[l] g m\n", kMaxTokens);
    }
    
    if (gAddressHelper.isStackTraceSupported() && msg.logLevel >= LogLevelError) {
        
        // can just report the caller, and not a full stack
        // already have function, so returnAddress printing is the same.
        /* if (msg.returnAddress) {
            strlcat(tokens, "s\n", kMaxTokens);
        }
        else */ {
            strlcat(tokens, "S", kMaxTokens);
        }
    }
#endif
    return tokens;
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
                
            case 's': { // return address (1 line stack)
                if (msg.returnAddress) {
                    string symbolName, filename;
                    uint32_t line = 0;
                    gAddressHelper.getAddressInfo(msg.returnAddress, symbolName, filename, line);
                    buffer += symbolName;
                }
                break;
            }
            case 'S': { // full stack
                uint32_t skipStackLevels = 3;
#if KRAM_DEBUG
                skipStackLevels++;
#endif
                gAddressHelper.getStackInfo(buffer, skipStackLevels);
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


bool isMessageFiltered(const LogMessage& msg) {
#if KRAM_RELEASE
    if (msg.logLevel == LogLevelDebug)
        return true;
#endif
    return false;
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
    
    // This is only needed for Window Gui.
    // Assumes gui app didn't call AllocConsole.
    if (gLogState.counter == 1) {
        bool hasConsole = ::GetStdHandle(STD_OUTPUT_HANDLE) != nullptr;
        
        // only way to debug a gui app without console is to attach debugger
        gLogState.isWindowsGuiApp = !hasConsole;
    }
    // TODO: test IsDebuggerPresent once per frame, not on every log
    gLogState.isWindowsDebugger = ::IsDebuggerPresent();
    
    if (gLogState.isWindowsGuiApp && !gLogState.isWindowsDebugger)
        return status;
    
    
    if (gLogState.isWindowsGuiApp) {
        char tokens[kMaxTokens] = {};
        getFormatTokens(tokens, msg, DebuggerOutputDebugString);
        formatMessage(buffer, msg, tokens);
        
        // TODO: split string up into multiple logs
        // this is limited to 32K
        // OutputDebugString(buffer.c_str());
        
        // This supports UTF8 strings by converting them to wide.
        // TODO: Wine doesn't handle.
        OutputDebugStringU(buffer.c_str(), buffer.size());
    }
    else {
        char tokens[kMaxTokens] = {};
        getFormatTokens(tokens, msg, Debugger);
        formatMessage(buffer, msg, tokens);
        
        // avoid double print to debugger
        FILE* fp = stdout;
        fwrite(buffer.c_str(), 1, buffer.size(), fp);
        // if heavy logging, then could delay fflush
        fflush(fp);
    }
#elif KRAM_ANDROID
    // TODO: move higher up
    // API 30
    AndroidLogLevel osLogLevel = ANDROID_LOG_ERROR;
    switch (msg.logLevel) {
        case LogLevelDebug:
            osLogLevel = ANDROID_LOG_DEBUG;
            break;
        case LogLevelInfo:
            osLogLevel = ANDROID_LOG_INFO;
            break;
            
        case LogLevelWarning:
            osLogLevel = ANDROID_LOG_WARNING;
            break;
        case LogLevelError:
            osLogLevel = ANDROID_LOG_ERROR;
            break;
    }
    
    if (!__android_log_is_loggable(osLogLevel, msg.group, __android_log_get_minimum_priority())) // will be default level if not set
        return status;
    
    char tokens[kMaxTokens] = {};
    getFormatTokens(tokens, msg, DebuggerLogcat);
    formatMessage(buffer, msg, tokens);
    
    // TODO: split string up into multiple logs by /n
    // this can only write 4K - 80 chars at time, don't use print it's 1023
    // API 30
    __android_log_message msg = {
        LOG_ID_MAIN, msg.file, msg.line, buffer.c_str(), osLogLevel, sizeof(__android_log_message), msg.group
    };
    __android_log_write_log_message(msg);
#else
    
#if KRAM_IOS || KRAM_MAC
    // test os_log
    
    static bool useOSLog = true;
    if (useOSLog)
    {
        char tokens[kMaxTokens] = {};
        getFormatTokens(tokens, msg, DebuggerOSLog);
        formatMessage(buffer, msg, tokens);
        
        // os_log reports this as the callsite, and doesn't jump to another file
        // or if the dso is even passed from this file, the file/line aren't correct.
        // So os_log_impl is grabbing return address whithin the function that can't be set.
        // So have to inject the NSLog, os_log, syslog calls directly into code, but that
        // not feasible.   This will at least color the mesages.
        
        auto osLogLevel = OS_LOG_TYPE_INFO;
        switch (msg.logLevel) {
            case LogLevelDebug:
                osLogLevel = OS_LOG_TYPE_DEBUG;
                break;
            case LogLevelInfo:
                osLogLevel = OS_LOG_TYPE_INFO;
                break;
                
            case LogLevelWarning:
                osLogLevel = OS_LOG_TYPE_ERROR; // no warning level
                break;
            case LogLevelError:
                osLogLevel = OS_LOG_TYPE_FAULT;
                break;
        }
        
        // TODO: have kramc and kramv using this logger, can we get at subsystem?
        const char* subsystem = "com.hialec.kram";
        
        os_log_with_type(os_log_create(subsystem, msg.group), osLogLevel, "%{public}s", buffer.c_str());
    }
    else
#endif
    {
        char tokens[kMaxTokens] = {};
        getFormatTokens(tokens, msg, Debugger);
        formatMessage(buffer, msg, tokens);
        
        FILE* fp = stdout;
        fwrite(buffer.c_str(), 1, buffer.size(), fp);
        // if heavy logging, then could delay fflush
        fflush(fp);
    }
#endif

    return status;  // reserved for later
}


                     
int32_t logMessage(const char* group, int32_t logLevel,
                          const char* file, int32_t line, const char* func,
                          const char* fmt, ...)
{
    void* dso = nullptr;
    void* logAddress = nullptr;
    
#if KRAM_IOS || KRAM_MAC
    dso = &__dso_handle; // may need to come from call site for the mach_header of .o
    logAddress = __builtin_return_address(0); // or __builtin_frame_address(0))
#elif KRAM_WIN
    //
    // TODO: use SymFromAddr to convert address to mangled symbol, and demangle it
    // from DbgHelp.dll
    logAddress = _ReturnAddress(); // or _AddressOfReturnAddress()
#endif
    
    LogMessage logMessage = {
        group, logLevel,
        file, line, func, 
        nullptr, 0.0, // threadname, timestamp
        
        // must set -no_pie to use __builtin_return_address to turn off ASLR
        dso, logAddress,
        nullptr, false, // msg, msgHasNewline
    };
    
    if (isMessageFiltered(logMessage)) {
        return 0;
    }
    
    // convert var ags to a msg
    const char* msg = nullptr;

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
        int res = vsprintf(str, fmt, args);
        va_end(args);
        if (res < 0) return 0;
        
        msg = str.c_str();
    }
    
    logMessage.msg = msg;
    
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
#if KRAM_IOS || KRAM_MAC
    void* dso = &__dso_handle;
    void* logAddress = __builtin_return_address(0); // or __builtin_frame_address(0))
#else
    void* dso = nullptr;
    void* logAddress = nullptr;
#endif
    
    LogMessage logMessage = {
        group, logLevel,
        file, line, func, nullptr, 0.0, // threadName, timestamp
        
        // must set -no_pie to use __builtin_return_address to turn off ASLR
        dso, logAddress,
        nullptr, false, // msg, msgHasNewline
    };
    if (isMessageFiltered(logMessage)) {
        return 0;
    }
    
    string str = fmt::vformat(format, args);
    const char* msg = str.c_str();
    
    logMessage.msg = msg;
    
    char threadName[kMaxThreadName] = {};
    setMessageFields(logMessage, threadName);
    return logMessageImpl(logMessage);
}

}  // namespace kram
