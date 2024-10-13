
#include "Engine.h"

#include <stdio.h> // vsnprintf
#include <stdlib.h> // strtod, strtol
#include <string.h> // strcmp, strcasecmp

// this is usually just an unordered_map internally
#include <unordered_set>

namespace M4 {

// Engine/String.cpp

void String_Copy(char* str, const char* b, uint32_t size)
{
#ifdef WIN32
    strncpy(str, b, size);
    str[size - 1] = 0;
#else
    strlcpy(str, b, size);
#endif
}

// This version doesn't truncate and is simpler
int String_PrintfArgList(std::string& buffer, const char* format, va_list args)
{
    int n = 0;

    if (!String_HasChar(format, '%')) {
        buffer += format;
        n = (uint32_t)strlen(format);
    }
    else if (String_Equal(format, "%s")) {
        va_list tmp;
        va_copy(tmp, args);
        const char* text = va_arg(args, const char*);
        n = (uint32_t)strlen(text);
        buffer += text;
        va_end(tmp);
    }
    else {
        va_list tmp;
        va_copy(tmp, args);

        int len = vsnprintf(nullptr, 0, format, tmp);
        if (len >= 0) {
            size_t bufferLength = buffer.length();
            buffer.resize(bufferLength + len);
            vsnprintf((char*)buffer.data() + bufferLength, len + 1, format, tmp);

            n = len;
        }
        va_end(tmp);
    }

    return n;
}

// This version truncates but works on stack
int String_PrintfArgList(char* buffer, int size, const char* format, va_list args)
{
    int n;

    if (!String_HasChar(format, '%')) {
        String_Copy(buffer, format, size);

        // truncation or not
        n = (int)strlen(format);
    }
    else if (String_Equal(format, "%s")) {
        va_list tmp;
        va_copy(tmp, args);
        const char* text = va_arg(args, const char*);
        n = (int)strlen(text);

        // truncation
        String_Copy(buffer, text, size);
        va_end(tmp);
    }
    else {
        va_list tmp;
        va_copy(tmp, args);

        // truncation
        // vsnprint returns -1 on failure
        n = vsnprintf(buffer, size, format, tmp);
        va_end(tmp);
    }

    if (n < 0 || (n + 1) > size)
        return -1;

    return n;
}

int String_Printf(std::string& buffer, const char* format, ...)
{
    va_list args;
    va_start(args, format);

    int n = String_PrintfArgList(buffer, format, args);

    va_end(args);

    return n;
}

int String_Printf(char* buffer, int size, const char* format, ...)
{
    va_list args;
    va_start(args, format);

    int n = String_PrintfArgList(buffer, size, format, args);

    va_end(args);

    return n;
}

int String_FormatFloat(char* buffer, int size, float value)
{
    return String_Printf(buffer, size, "%.6f", value);
}

bool String_HasChar(const char* str, char c)
{
    return strchr(str, c) != NULL;
}

bool String_HasString(const char* str, const char* search)
{
    return strstr(str, search) != NULL;
}

bool String_Equal(const char* a, const char* b)
{
    if (a == b) return true;
    if (a == NULL || b == NULL) return false;
    return strcmp(a, b) == 0;
}

bool String_EqualNoCase(const char* a, const char* b)
{
    if (a == b) return true;
    if (a == NULL || b == NULL) return false;
#if _MSC_VER
    return _stricmp(a, b) == 0;
#else
    return strcasecmp(a, b) == 0;
#endif
}

double String_ToDouble(const char* str, char** endptr)
{
    return strtod(str, endptr);
}

float String_ToFloat(const char* str, char** endptr)
{
    return strtof(str, endptr);
}

static const int kBase10 = 10;
static const int kBase16 = 16;

int32_t String_ToIntHex(const char* str, char** endptr)
{
    return (int)strtol(str, endptr, kBase16);
}

int32_t String_ToInt(const char* str, char** endptr)
{
    return (int)strtol(str, endptr, kBase10);
}

uint32_t String_ToUint(const char* str, char** endptr)
{
    return (int)strtoul(str, endptr, kBase10);
}

uint64_t String_ToUlong(const char* str, char** endptr)
{
    return (int)strtoull(str, endptr, kBase10);
}

int64_t String_ToLong(const char* str, char** endptr)
{
    return (int)strtoll(str, endptr, kBase10);
}

void String_StripTrailingFloatZeroes(char* buffer)
{
    const char* dotPos = strrchr(buffer, '.');
    if (dotPos == nullptr) return;

    uint32_t bufferLen = (uint32_t)strlen(buffer);

    // strip trailing zeroes
    while (bufferLen > 0) {
        char& c = buffer[bufferLen - 1];
        if (c == '0') {
            c = 0;
            bufferLen--;
        }
        else {
            break;
        }
    }

    // This breaks appending h to a number in MSL
    // strip the period (only for MSL)
    // char& c = buffer[bufferLen-1];
    // if (dotPos == &c)
    // {
    //     c = 0;
    //     bufferLen--;
    // }
}

// Engine/Log.cpp

void Log_Error(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    Log_ErrorArgList(format, args);
    va_end(args);
}

void Log_ErrorArgList(const char* format, va_list args, const char* filename, uint32_t line)
{
    va_list tmp;
    va_copy(tmp, args);

    // Not thread-safe
    static std::string buffer;
    buffer.clear();
    String_PrintfArgList(buffer, format, tmp);

    // TODO: this doesn't work on Win/Android
    // use a real log abstraction to ODS/etc from Kram
    if (filename)
        fprintf(stderr, "%s:%d: error: %s", filename, line, buffer.c_str());
    else
        fprintf(stderr, "error: %s", buffer.c_str());

    va_end(tmp);
}

// Engine/StringPool.cpp

using StringPoolSet = std::unordered_set<const char*, CompareAndHandStrings, CompareAndHandStrings>;

#define CastImpl(imp) (StringPoolSet*)imp

StringPool::StringPool(Allocator* allocator)
{
    // NOTE: allocator not used

    m_impl = new StringPoolSet();
}
StringPool::~StringPool()
{
    auto* impl = CastImpl(m_impl);

    // delete the strings
    for (auto it : *impl) {
        const char* text = it;
        free((char*)text);
    }

    delete impl;
}

const char* StringPool::AddString(const char* text)
{
    auto* impl = CastImpl(m_impl);
    auto it = impl->find(text);
    if (it != impl->end())
        return *it;

    // _strdup doesn't go through allocator either
#if _MSC_VER
    const char* dup = _strdup(text);
#else
    const char* dup = strdup(text);
#endif

    impl->insert(dup);
    return dup;
}

const char* StringPool::PrintFormattedVaList(const char* fmt, va_list args)
{
    char* res = nullptr;

    va_list tmp;

    // va_copy needed?
    va_copy(tmp, args);

    // just call 2x, once for len
    int len = vsnprintf(nullptr, 0, fmt, tmp);
    if (len >= 0) {
        res = (char*)malloc(len + 1);
        vsnprintf(res, len + 1, fmt, tmp);
    }
    va_end(tmp);

    // caller responsible for freeing mem
    return res;
}

const char* StringPool::AddStringFormatList(const char* format, va_list args)
{
    // don't format if no tokens
    va_list tmp;
    va_copy(tmp, args);
    const char* text = PrintFormattedVaList(format, tmp);
    va_end(tmp);

    auto* impl = CastImpl(m_impl);

    // add it if not found
    auto it = impl->find(text);
    if (it == impl->end()) {
        impl->insert(text);
        return text;
    }

    // allocated inside PrintFormattedVaList
    free((char*)text);
    return *it;
}

const char* StringPool::AddStringFormat(const char* format, ...)
{
    // TODO: don't format if no tokens
    va_list args;
    va_start(args, format);
    const char* string = AddStringFormatList(format, args);
    va_end(args);

    return string;
}

bool StringPool::GetContainsString(const char* text) const
{
    const auto* impl = CastImpl(m_impl);
    return impl->find(text) != impl->end();
}

} //namespace M4
