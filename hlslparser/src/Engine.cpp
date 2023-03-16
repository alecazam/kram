
#include "Engine.h"

#include <stdio.h>  // vsnprintf
#include <string.h> // strcmp, strcasecmp
#include <stdlib.h>	// strtod, strtol

// this is usually just an unordered_map internally
#include <unordered_set>

namespace M4 {

// Engine/String.cpp

int String_PrintfArgList(char * buffer, int size, const char * format, va_list args) {

    va_list tmp;
    va_copy(tmp, args);

#if _MSC_VER >= 1400
	int n = vsnprintf_s(buffer, size, _TRUNCATE, format, tmp);
#else
	int n = vsnprintf(buffer, size, format, tmp);
#endif

    va_end(tmp);

	if (n < 0 || n > size) return -1;
	return n;
}

int String_Printf(char * buffer, int size, const char * format, ...) {

    va_list args;
    va_start(args, format);

    int n = String_PrintfArgList(buffer, size, format, args);

    va_end(args);

	return n;
}

int String_FormatFloat(char * buffer, int size, float value) {
    return String_Printf(buffer, size, "%f", value);
}

bool String_Equal(const char * a, const char * b) {
	if (a == b) return true;
	if (a == NULL || b == NULL) return false;
	return strcmp(a, b) == 0;
}

bool String_EqualNoCase(const char * a, const char * b) {
	if (a == b) return true;
	if (a == NULL || b == NULL) return false;
#if _MSC_VER
	return _stricmp(a, b) == 0;
#else
	return strcasecmp(a, b) == 0;
#endif
}

double String_ToDouble(const char * str, char ** endptr) {
	return strtod(str, endptr);
}

int String_ToInteger(const char * str, char ** endptr) {
	return (int)strtol(str, endptr, 10);
}

int String_ToIntegerHex(const char * str, char ** endptr) {
	return (int)strtol(str, endptr, 16);
}


void String_StripTrailingFloatZeroes(char* buffer)
{
    const char* dotPos = strrchr(buffer, '.');
    if (dotPos == nullptr) return;
    
    uint32_t bufferLen = (uint32_t)strlen(buffer);
    
    // strip trailing zeroes
    while (bufferLen > 0)
    {
        char& c = buffer[bufferLen-1];
        if (c == '0')
        {
            c = 0;
            bufferLen--;
        }
        else
        {
            break;
        }
    }
    
    // This breaks appending h to a number in MSL
    // strip the period (only for MSL)
//    char& c = buffer[bufferLen-1];
//    if (dotPos == &c)
//    {
//        c = 0;
//        bufferLen--;
//    }
}

// Engine/Log.cpp

void Log_Error(const char * format, ...) {
    va_list args;
    va_start(args, format);
    Log_ErrorArgList(format, args);
    va_end(args);
}

void Log_ErrorArgList(const char * format, va_list args) {
#if 1 // @@ Don't we need to do this?
    va_list tmp;
    va_copy(tmp, args);
    // TODO: this doesn't work on Win/Android
    // use a real log abstraction to ODS/etc from Kram
    vfprintf( stderr, format, tmp );
    va_end(tmp);
#else
    vprintf( format, args );
#endif
}


// Engine/StringPool.cpp

// Taken from Alec's HashHelper.h

// case sensitive fnv1a hash, can pass existing hash to continue a hash
inline uint32_t HashFnv1a(const char* val, uint32_t hash = 0x811c9dc5) {
    const uint32_t prime  = 0x01000193; // 16777619 (32-bit)
    while (*val) {
        hash = (hash * prime) ^ (uint32_t)*val++;
    }
    return hash;
}

// this compares string stored as const char*
struct CompareStrings
{
    template <class _Tp>
    bool operator()(const _Tp& __x, const _Tp& __y) const
    { return strcmp( __x, __y ) == 0; }
    
    template <class _Tp>
    size_t operator()(const _Tp& __x) const {
        // assumes 32-bit hash to int64 conversion here
        return (size_t)HashFnv1a(__x);
    }
};

using StringPoolSet = std::unordered_set<const char*, CompareStrings, CompareStrings>;

#define CastImpl(imp) (StringPoolSet*)imp

StringPool::StringPool(Allocator * allocator) {
    // allocator not used
    
    m_impl = new StringPoolSet();
}
StringPool::~StringPool() {
    auto* impl = CastImpl(m_impl);
    
    // TODO: fix
    // delete the strings
    for (auto it : *impl) {
        const char* text = it;
        free((char*)text);
    }
    
    delete impl;
}

const char * StringPool::AddString(const char * text) {
    auto* impl = CastImpl(m_impl);
    auto it = impl->find(text);
    if (it != impl->end())
        return *it;
    
    // _strdup doesn't go through allocator either
#if _MSC_VER
    const char * dup = _strdup(text);
#else
    const char * dup = strdup(text);
#endif
    
    impl->insert(dup);
    return dup;
}

const char* StringPool::PrintFormattedVaList(const char* fmt, va_list args) {
    char* res = nullptr;
    
    va_list tmp;

    // va_copy needed?
    va_copy(tmp, args);
    
    // just call 2x, once for len
    int len = vsnprintf(nullptr, 0, fmt, tmp);
    if (len >= 0)
    {
        res = (char*)malloc(len+1);
        vsnprintf(res, len+1, fmt, tmp);
    }
    va_end(tmp);

    // caller responsible for freeing mem
    return res;
}

const char * StringPool::AddStringFormatList(const char * format, va_list args) {
    // don't format if no tokens
    va_list tmp;
    va_copy(tmp, args);
    const char * text = PrintFormattedVaList(format, tmp);
    va_end(tmp);

    auto* impl = CastImpl(m_impl);
    
    // add it if not found
    auto it = impl->find(text);
    if (it == impl->end())
    {
        impl->insert(text);
        return text;
    }
    
    // allocated inside PrintFormattedVaList
    free((char*)text);
    return *it;
}

const char * StringPool::AddStringFormat(const char * format, ...) {
    // TODO: don't format if no tokens
    va_list args;
    va_start(args, format);
    const char * string = AddStringFormatList(format, args);
    va_end(args);

    return string;
}

bool StringPool::GetContainsString(const char * text) const {
    const auto* impl = CastImpl(m_impl);
    return impl->find(text) != impl->end();
}

} // M4 namespace
