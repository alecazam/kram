#pragma once

#if _MSC_VER
#define _CRT_SECURE_NO_WARNINGS 1
#endif

#include <stdarg.h> // va_list, vsnprintf
#include <stdlib.h> // malloc
#include <new> // for placement new

// stl
#include <string>

#ifndef NULL
#define NULL    0
#endif

#ifndef va_copy
#define va_copy(a, b) (a) = (b)
#endif

// Engine/Assert.h
#include <cassert>
//#define assert(...)
#define ASSERT assert

// this is similar to printflike macro, checks format args
#if defined(__GNUC__) || defined(__clang__)
#define M4_PRINTF_ATTR(string_index, first_to_check) __attribute__((format(printf, string_index, first_to_check)))
#else
#define M4_PRINTF_ATTR(string_index, first_to_check)
#endif

namespace M4 {


// Engine/Allocator.h

// This doesn't do placement new/delete, but is only
// use to allocate NodePage and StringPool.  Then placement
// new/delete is called explicitly by say NewNode.  So
// there default ctor variable initializers are safe to use.
class Allocator {
public:
    template <typename T> T * New() {
        return (T *)malloc(sizeof(T));
    }
    template <typename T> T * New(size_t count) {
        return (T *)malloc(sizeof(T) * count);
    }
    template <typename T> void Delete(T * ptr) {
        free((void *)ptr);
    }
    template <typename T> T * Realloc(T * ptr, size_t count) {
        return (T *)realloc(ptr, sizeof(T) * count);
    }
};


// Engine/String.h



int String_FormatFloat(char * buffer, int size, float value);
bool String_Equal(const char * a, const char * b);
bool String_EqualNoCase(const char * a, const char * b);

double String_ToDouble(const char * str, char ** end);
float String_ToFloat(const char * str, char ** end);
// no half

int32_t String_ToIntHex(const char * str, char ** end);
int32_t String_ToInt(const char * str, char ** end);
uint32_t String_ToUint(const char * str, char ** end);

uint64_t String_ToUlong(const char * str, char ** end);
int64_t String_ToLong(const char * str, char ** end);

bool String_HasChar(const char* str, char c);
bool String_HasString(const char* str, const char* search);

// just use these, it's way easier than using fixed buffers
int String_PrintfArgList(std::string& buffer, const char * format, va_list args);
int String_Printf(std::string& buffer, const char * format, ...) M4_PRINTF_ATTR(2, 3);

// These 3 calls have truncation issues
int String_Printf(char * buffer, int size, const char * format, ...) M4_PRINTF_ATTR(3, 4);
int String_PrintfArgList(char * buffer, int size, const char * format, va_list args);
void String_Copy(char* str, const char* b, uint32_t size);

void String_StripTrailingFloatZeroes(char* buffer);

// Hash and Compare are taken out of kram
// case sensitive fnv1a hash, can pass existing hash to continue a hash
inline uint32_t HashFnv1a(const char* val, uint32_t hash = 0x811c9dc5)
{
    const uint32_t prime  = 0x01000193; // 16777619 (32-bit)
    while (*val)
    {
        hash = (hash * prime) ^ (uint32_t)*val++;
    }
    return hash;
}

// this compares string stored as const char*
struct CompareAndHandStrings
{
    template <class _Tp>
    bool operator()(const _Tp& __x, const _Tp& __y) const
    { return String_Equal( __x, __y ); }
    
    template <class _Tp>
    size_t operator()(const _Tp& __x) const {
        // assumes 32-bit hash to int64 conversion here
        return (size_t)HashFnv1a(__x);
    }
};

// Engine/Log.h

void Log_Error(const char * format, ...) M4_PRINTF_ATTR(1, 2);
void Log_ErrorArgList(const char * format, va_list args);


// Engine/Array.h

template <typename T>
void ConstructRange(T * buffer, int new_size, int old_size) {
    for (int i = old_size; i < new_size; i++) {
        new(buffer+i) T; // placement new
    }
}

template <typename T>
void ConstructRange(T * buffer, int new_size, int old_size, const T & val) {
    for (int i = old_size; i < new_size; i++) {
        new(buffer+i) T(val); // placement new
    }
}

template <typename T>
void DestroyRange(T * buffer, int new_size, int old_size) {
    for (int i = new_size; i < old_size; i++) {
        (buffer+i)->~T(); // Explicit call to the destructor
    }
}


template <typename T>
class Array {
public:
    Array(Allocator * allocator) : allocator(allocator), buffer(NULL), size(0), capacity(0) {}

    void PushBack(const T & val) {
        ASSERT(&val < buffer || &val >= buffer+size);

        int old_size = size;
        int new_size = size + 1;

        SetSize(new_size);

        ConstructRange(buffer, new_size, old_size, val);
    }
    T & PushBackNew() {
        int old_size = size;
        int new_size = size + 1;

        SetSize(new_size);

        ConstructRange(buffer, new_size, old_size);

        return buffer[old_size];
    }
    void Resize(int new_size) {
        int old_size = size;

        DestroyRange(buffer, new_size, old_size);

        SetSize(new_size);

        ConstructRange(buffer, new_size, old_size);
    }

    int GetSize() const { return size; }
    const T & operator[](int i) const { ASSERT(i < size); return buffer[i]; }
    T & operator[](int i) { ASSERT(i < size); return buffer[i]; }

private:

    // Change array size.
    void SetSize(int new_size) {
        size = new_size;

        if (new_size > capacity) {
            int new_buffer_size;
            if (capacity == 0) {
                // first allocation is exact
                new_buffer_size = new_size;
            }
            else {
                // following allocations grow array by 25%
                new_buffer_size = new_size + (new_size >> 2);
            }

            SetCapacity(new_buffer_size);
        }
    }

    // Change array capacity.
    void SetCapacity(int new_capacity) {
        ASSERT(new_capacity >= size);

        if (new_capacity == 0) {
            // free the buffer.
            if (buffer != NULL) {
                allocator->Delete<T>(buffer);
                buffer = NULL;
            }
        }
        else {
            // realloc the buffer
            buffer = allocator->Realloc<T>(buffer, new_capacity);
        }

        capacity = new_capacity;
    }


private:
    Allocator * allocator; // @@ Do we really have to keep a pointer to this?
    T * buffer;
    int size;
    int capacity;
};


// Engine/StringPool.h

// @@ Implement this with a hash table!
struct StringPool {
    StringPool(Allocator * allocator);
    ~StringPool();

    const char * AddString(const char * text);
    const char * AddStringFormat(const char * fmt, ...) M4_PRINTF_ATTR(2, 3);
    const char * AddStringFormatList(const char * fmt, va_list args);
    bool GetContainsString(const char * text) const;
private:
    const char*PrintFormattedVaList(const char* fmt, va_list args);
    void* m_impl = NULL;
};


} // M4 namespace
