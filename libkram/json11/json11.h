/* json11
 *
 * json11 is a tiny JSON library for C++11, providing JSON parsing and serialization.
 *
 * The core object provided by the library is json11::Json. A Json object represents any JSON
 * value: null, bool, number (int or double), string (string), array (vector), or
 * object (map).
 *
 * Json objects act like values: they can be assigned, copied, moved, compared for equality or
 * order, etc. There are also helper methods Json::dump, to serialize a Json to a string, and
 * Json::parse (static) to parse a string as a Json object.
 *
 * Internally, the various types of Json object are represented by the JsonValue class
 * hierarchy.
 *
 * A note on numbers - JSON specifies the syntax of number formatting but not its semantics,
 * so some JSON implementations distinguish between integers and floating-point numbers, while
 * some don't. In json11, we choose the latter. Because some JSON implementations (namely
 * Javascript itself) treat all numbers as the same type, distinguishing the two leads
 * to JSON that will be *silently* changed by a round-trip through those implementations.
 * Dangerous! To avoid that risk, json11 stores all numbers as double internally, but also
 * provides integer helpers.
 *
 * Fortunately, double-precision IEEE754 ('double') can precisely store any integer in the
 * range +/-2^53, which includes every 'int' on most systems. (Timestamps often use int64
 * or long long to avoid the Y2038K problem; a double storing microseconds since some epoch
 * will be exact for +/- 275 years.)
 */

/* Copyright (c) 2013 Dropbox, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#pragma once

#include "KramConfig.h"

#include "ImmutableString.h"

namespace kram {
class ICompressedStream;
}

namespace json11 {

using namespace NAMESPACE_STL;
using namespace kram;

class Json;
class JsonReaderData;

//--------------------------

/* Don't want to maintain this form from json11.  Use SAX not DOM for writer.
// Write json nodes out to a string.  String data is encoded.
class JsonWriter final {
public:
    // Serialize.
    // caller must clear string.
    // TODO: accumulating to a string for large amounts of json is bad,
    // consider a real IO path use FileHelper or something.
    void write(const Json& root, string& out);
   
private:
    void write(const Json& root);
    
    void writeObject(const Json &values);
    void writeArray(const Json &values);
        
    void writeString(const Json &value);
    void writeNumber(const Json &value);
    void writeBool(const Json &value);
    void writeNull();
        
    // This could write to a FILE* instead of the string
    void writeText(const char* str);
    
private:
    string* _out = nullptr;
};
*/

// This code is part of kram, not json11.  Will move out.
// Write json nodes out to a string.  String data is encoded.
// This is way simpler than building up stl DOM to then write it out.
// And keys go out in the order added.
class JsonWriter final {
public:
    // This writes into a buffer
    JsonWriter(string* str) : _out(str) {}
    
    // This writes into a small buffer, and then into a compressed stream
    JsonWriter(string* str, ICompressedStream* stream) : _out(str), _stream(stream) {}
    ~JsonWriter();
    
    void pushObject(const char* key = "");
    void popObject();
    
    void pushArray(const char* key = "");
    void popArray();
    
    // keys for adding to object
    void writeString(const char* key, const char* value);
    void writeDouble(const char* key, double value);
    void writeInt32(const char* key, int32_t value);
    void writeBool(const char* key, bool value);
    void writeNull(const char* key);
    
    // These are keyless for adding to an array
    void writeString(const char* value);
    void writeDouble(double value);
    void writeInt32(int32_t value);
    void writeBool(bool value);
    void writeNull();
    
    // can write out json in parallel and combine
    void writeJson(const JsonWriter& json);
    
private:
    void writeFormat(const char* fmt, ...) __printflike(2, 3);
    
    bool isArray() const { return _stack.back() == ']'; }
    bool isObject() const { return _stack.back() == '}'; }
   
    void pop();
    
    void writeCommaAndNewline();
    const char* escapedString(const char* str);
    
    vector<bool> _isFirst; // could store counts
    string* _out = nullptr;
    string _stack;
    string _escapedString;
    ICompressedStream* _stream = nullptr;
};

class JsonArrayScope {
public:
    JsonArrayScope(JsonWriter& json_, const char* key = "") : json( &json_ ) {
        json->pushArray(key);
    }
    ~JsonArrayScope() {
        close();
    }
    void close() {
        if (json) { json->popArray(); json = nullptr; }
    }
private:
    JsonWriter* json = nullptr;
};

class JsonObjectScope {
public:
    JsonObjectScope(JsonWriter& json_, const char* key = "") : json( &json_ ) {
        json->pushObject(key);
    }
    ~JsonObjectScope() {
        close();
    }
    void close() {
        if (json) { json->popObject(); json = nullptr; }
    }
private:
    JsonWriter* json = nullptr;
};


//--------------------------

// DOM-based parser with nice memory characteristics and small API.
class JsonReader final {
public:
    JsonReader();
    ~JsonReader();
    
    // Parse. If parse fails, return Json() and assign an error message to err.
    // Strings are aliased out of the incoming buffer. Keys are aliased
    // from an immutable pool.  And json nodes are allocated from a block
    // linear allocator.  So the returned Json only lives while reader does.
    Json* read(const char* str, uint32_t strCount);
    const string& error() const { return err; }
    
    void resetAndFree();
    size_t memoryUse() const;
    
    ImmutableString getImmutableKey(const char* key);
    
    // TODO: add call to decode and allocate all strings in tree.  Would allow mmap to be released.
    // should this have a string allocator, or use the existing block allocator?
    
private:
    void fail(const string& msg);
    
    void consume_whitespace();
    bool consume_comment();
    void consume_garbage();
    char get_next_token();
    bool compareStringForLength(const char* expected, size_t length);
    bool expect(const char* expected);
    
    void parse_string_location(uint32_t& count);
    double parse_number();
    ImmutableString parse_key();
    void parse_json(int depth, Json& parent, ImmutableString key = nullptr);
       
private:
    // State
    const char* str = nullptr;
    size_t strSize  = 0;
    size_t i = 0; // iterates through str, poor choice for member variable
    
    // error state
    string err;
    bool failed = false;
    uint32_t lineCount = 1; // lines are 1 based
    
    // parser is recursive instead of iterative, so has max depth to prevent runaway parsing.
    uint32_t maxDepth = 200;
    
    // allocator and immutable string pool are here
    unique_ptr<JsonReaderData> _data;
};

//--------------------------

// Json value type.  This is a tree of nodes with iterators and search.
class Json final {
public:
    // iterator to simplify walking lists/objects
    class const_iterator final
    {
    public:
        const_iterator(const Json* node) : _curr(node) { }
  
        const_iterator& operator=(const Json* node) {
            _curr = node; 
            return *this;
        }
  
        // Prefix ++ overload
        const_iterator& operator++() {
            if (_curr)
                _curr = _curr->_next;
            return *this;
        }
  
        // Postfix ++ overload
        const_iterator operator++(int)
        {
            const_iterator iterator = *this;
            ++(*this);
            return iterator;
        }
  
        bool operator==(const const_iterator& iterator) const { return _curr == iterator._curr; }
        bool operator!=(const const_iterator& iterator) const { return _curr != iterator._curr; }
        const Json& operator*() const { return *_curr; }
  
    private:
        const Json* _curr;
    };
    
    // Type
    enum Type : uint8_t {
        TypeNull,
        TypeNumber,
        TypeBoolean,
        TypeString,
        TypeArray,
        TypeObject
    };
        
    // Only public for use by sNullValue
    Json() noexcept {}
   
    // Accessors
    Type type() const { return _type; }
    
    // Only for object type, caller can create from JsonReader
    ImmutableString key() const;
    void setKey(ImmutableString key);
    
    // array/objects have count and iterate call
    size_t count() const { return _count; };
    // Return a reference to arr[i] if this is an array, Json() otherwise.
    const Json & operator[](uint32_t i) const;
    // Return a reference to obj[key] if this is an object, Json() otherwise.
    const Json & operator[](const char* key) const;
    
    // implement standard iterator paradigm for linked list
    const_iterator begin() const { assert(is_array() || is_object()); return const_iterator(_value.aval); }
    const_iterator end() const { return const_iterator(nullptr); }
    
    bool iterate(const Json*& it) const;
    
    bool is_null()   const  { return _type == TypeNull; }
    bool is_number() const  { return _type == TypeNumber; }
    bool is_boolean() const { return _type == TypeBoolean; }
    bool is_string() const  { return _type == TypeString; }
    bool is_array()  const  { return _type == TypeArray; }
    bool is_object() const  { return _type == TypeObject; }

    // Return the enclosed value if this is a number, 0 otherwise. Note that json11 does not
    // distinguish between integer and non-integer numbers - number_value() and int_value()
    // can both be applied to a NUMBER-typed object.
    double number_value() const { return is_number() ? _value.dval : 0.0; }
    double double_value() const { return number_value(); }
    float float_value() const { return (float)number_value(); }
    int int_value() const { return (int)number_value(); }
    
    // Return the enclosed value if this is a boolean, false otherwise.
    bool boolean_value() const { return is_boolean() ? _value.bval : false; }
    // Return the enclosed string if this is a string, empty string otherwise
    const char* string_value(string& str) const;

    // quickly find a node using immutable string
    const Json& find(ImmutableString key) const;
    
private:
    friend class JsonReader;
    
    // Doesn't seem to work with namespaced class
    void createRoot();

    // Constructors for the various types of JSON value.
    Json(nullptr_t) noexcept         {}
    Json(double value)               : _type(TypeNumber), _value(value) {}
    Json(bool value)                 : _type(TypeBoolean), _value(value) {}
    
    // only works for aliased string
    Json(const char* value, uint32_t count_)
        : _type(TypeString), _count(count_), _value(value)
    {
    }
    
    // Only for JsonReader
    void addJson(Json* json);
    void addString(Json* json, const char* str, uint32_t len, ImmutableString key = nullptr);
    void addNull(Json* json, ImmutableString key = nullptr);
    void addBoolean(Json* json, bool b, ImmutableString key = nullptr);
    void addNumber(Json* json, double number, ImmutableString key = nullptr);
    void addArray(Json* json, ImmutableString key = nullptr);
    void addObject(Json* json,ImmutableString key = nullptr);

private:
    void trackMemory(int32_t size);

    // This type is 32B / node w/key ptr, w/2B key it's 24B / node.
    
    // 8B - objects store key in children
    // debugging difficult without key as string
    const char* _key = nullptr;
    
    // 2B, but needs lookup table then
    //uint16_t _key = 0;
    uint16_t _padding = 0;
    
    // 1B
    uint8_t  _padding1 = 0;
    
    // 1B - really 3 bits (could pack into ptr, but check immutable align)
    Type _type = TypeNull;
    
    // 4B - count used by array/object, also by string
    uint32_t _count = 0;
    
    // 8B - value to hold double and ptrs
    union JsonValue {
        JsonValue() : aval(nullptr) { }
        JsonValue(double v) : dval(v) {}
        JsonValue(bool v) : bval(v) {}
        JsonValue(const char* v) : sval(v) {}
        
        double     dval;
        bool       bval;
        const char* sval;
        Json* aval; // aliased children, chained with _next to form tree
    } _value;
    
    // 8B - arrays/object chain values, so this is non-null on more than just array/object type
    // aval is the root of the children.
    //uint32_t _next = 0;
    Json* _next = nullptr;
};

} // namespace json11
