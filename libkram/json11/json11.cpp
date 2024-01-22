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

#include "json11.h"
#include <cassert>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <limits>

// This sucks that clang C++17 in 2023 doesn't have float conversion impl
#if USE_CHARCONV
#include <charconv> // for from_chars
#endif

// not including this in KramConfig.h - used for pool
#include "BlockedLinearAllocator.h"

// Heavily modifed by Alec Miller 10/1/23
// This codebase was frozen by DropBox with very little effort put into it.
// And I liked the readability of the code.  Optimized with ImmutableStrings
// and a BlockedLinearAllocator.
//
// This is DOM reader/writer.  Building up stl data structures in a DOM
// to write isn't great memory wise.  May move to a SAX writer.
// Times to read font atlas file on M1 MBP 14".  1/21/24
//
// json11
// Release - parsed 101 KB of json using 576 KB of memory in 14.011ms
// Debug   - parsed 101 KB of json using 576 KB of memory in 26.779ms
//
// simdjson (5x faster), no mem use numbers
// Release - parsed 101 KB of json in 3.182ms
//
// TODO: json11 parser doesn't handle trailing comments

namespace json11 {

using namespace NAMESPACE_STL;
using namespace kram;

//---------------------

const char* JsonWriter::escapedString(const char* str)
{
    size_t strLen = strlen(str);
    
    // first see if string needs escaped
    bool needsEscaped = false;
    for (size_t i = 0; i < strLen; i++) {
        const char ch = str[i];
        switch(ch) {
            case '\\':
            case '"':
            case '\b':
            case '\f':
            case '\n':
            case '\r':
            case '\t':
                needsEscaped = true;
                break;
        }
        
        if (needsEscaped)
            break;
    }
    
    if (!needsEscaped) {
        return str;
    }
    
    // build the escaped string
    _escapedString.clear();
    for (size_t i = 0; i < strLen; i++) {
        const char ch = str[i];
        if (ch == '\\') {
            _escapedString.push_back('\\');
            _escapedString.push_back('\\');
        } else if (ch == '"') {
            _escapedString.push_back('\\');
            _escapedString.push_back('"');
        } else if (ch == '\b') {
            _escapedString.push_back('\\');
            _escapedString.push_back('b');
        } else if (ch == '\f') { //?
            _escapedString.push_back('\\');
            _escapedString.push_back('f');
        } else if (ch == '\n') {
            _escapedString.push_back('\\');
            _escapedString.push_back('n');
        } else if (ch == '\r') {
            _escapedString.push_back('\\');
            _escapedString.push_back('r');
        } else if (ch == '\t') {
            _escapedString.push_back('\\');
            _escapedString.push_back('t');
        } else if (static_cast<uint8_t>(ch) <= 0x1f) {
            char buf[8];
            snprintf(buf, sizeof buf, "\\u%04x", ch);
            _escapedString.append(buf);
        } else if (static_cast<uint8_t>(ch) == 0xe2 &&
                   static_cast<uint8_t>(str[i+1]) == 0x80 &&
                   static_cast<uint8_t>(str[i+2]) == 0xa8) {
            _escapedString.append("\\u2028"); // line separator
            i += 2;
        } else if (static_cast<uint8_t>(ch) == 0xe2 &&
                   static_cast<uint8_t>(str[i+1]) == 0x80 &&
                   static_cast<uint8_t>(str[i+2]) == 0xa9) {
            _escapedString.append("\\u2029"); // paragraph separator
            i += 2;
        } else {
            _escapedString.push_back(ch);
        }
    }
    
    return _escapedString.c_str();
}
    
/*
void JsonWriter::writeText(const char* text) {
    *_out += text;
}

void JsonWriter::writeNull() {
    writeText("null");
}

void JsonWriter::writeNumber(const Json& value) {
    if (isfinite(value.number_value())) {
        char buf[32];
        // TODO: this is super long
        // TODO: remove trailing 0's
        snprintf(buf, sizeof buf, "%.17g", value.number_value());
        writeText(buf);
    } else {
        // TODO: may want to write inf
        writeText("null");
    }
}

void JsonWriter::writeBool(const Json& value) {
    writeText(value.boolean_value() ? "true" : "false");
}

// encode utf8 to escaped json string
void JsonWriter::writeString(const Json& value) {
    writeText("\"");
    string dummy;
    const char* str = value.string_value(dummy);
    for (size_t i = 0, iEnd = value.count(); i < iEnd; i++) {
        const char ch = str[i];
        if (ch == '\\') {
            writeText("\\\\"); // this is 2x
        } else if (ch == '"') {
            writeString("\\\"");
        } else if (ch == '\b') {
            writeText("\\b");
        } else if (ch == '\f') { //?
            writeText("\\f");
        } else if (ch == '\n') {
            writeText("\\n");
        } else if (ch == '\r') {
            writeText("\\r");
        } else if (ch == '\t') {
            writeText("\\t");
        } else if (static_cast<uint8_t>(ch) <= 0x1f) {
            char buf[8];
            snprintf(buf, sizeof buf, "\\u%04x", ch);
            writeText(buf);
        } else if (static_cast<uint8_t>(ch) == 0xe2 && static_cast<uint8_t>(str[i+1]) == 0x80
                   && static_cast<uint8_t>(str[i+2]) == 0xa8) {
            writeText("\\u2028"); // line separator
            i += 2;
        } else if (static_cast<uint8_t>(ch) == 0xe2 && static_cast<uint8_t>(str[i+1]) == 0x80
                   && static_cast<uint8_t>(str[i+2]) == 0xa9) {
            writeText("\\u2029"); // paragraph separator
            i += 2;
        } else {
            char chStr[2] = {};
            chStr[0] = ch;
            writeText(chStr);
        }
    }
    writeText("\"");
}

void JsonWriter::writeArray(const Json &values) {
    bool first = true;
    
    // Note: this isn't handling any indenting
    writeText("[");
    for (const auto& it: values) {
        if (!first)
            writeText(", ");
        // just an array in brackets
        write(it);
        first = false;
    }
    writeText("]");
}

void JsonWriter::writeObject(const Json &values) {
    bool first = true;
    
    // Note: this isn't handling any indenting
    writeText("{");
    for (const auto& it: values) {
        if (!first)
            writeText(", ");
       
        // quoted key:value pairs in parens
        writeText("\"");
        writeText(it.key());
        writeText("\": ");
        write(it);
        first = false;
    }
    writeText("}");
}

void JsonWriter::write(const Json& json) {
    switch(json.type()) {
        case Json::TypeObject: writeObject(json); break;
        case Json::TypeArray: writeArray(json); break;
            
        case Json::TypeString: writeString(json); break;
        case Json::TypeBoolean: writeBool(json); break;
        case Json::TypeNumber: writeNumber(json); break;
        case Json::TypeNull: writeNull(); break;
    }
}

void JsonWriter::write(const Json &root, string& out) {
    _out = &out;
    write(root);
}
*/

//-----------------------------

class JsonReaderData {
public:
    JsonReaderData() 
        : _nodeAllocator(1024, sizeof(Json)),
          _keyPool(32*1024)
    { }
    
    Json* allocateJson() {
        Json* node = _nodeAllocator.allocateItem<Json>();
        assert(node);
        return node;
    }
    
    void reset() {
        _nodeAllocator.reset();
    }
    void resetAndFree() {
        _nodeAllocator.resetAndFree();
    }
    
    const char* getJsonString(uint32_t offset) const {
        // TODO: assert(offset < _jsonDataSize);
        return _jsonData + offset;
    }
    
    bool isImmutableKey(const char* key) const {
        return _keyPool.isImmutableString(key);
    }
    ImmutableString getImmutableKey(const char* key) const {
        return _keyPool.getImmutableString(key);
    }
    
    uint16_t getIndexFromKey(const char* key) const {
        ImmutableString str = _keyPool.getImmutableString(key);
        return _keyPool.getCounter(str);
    }
    
    ImmutableString getKeyFromIndex(uint16_t keyIndex) {
        return _keyPool.getImmutableString(keyIndex);
    }
    bool isWriting() const { return _isWriting; }
    
    // track allocated string memory
    void trackMemory(int32_t size) {
        _memoryUsage += size;
        if (_memoryUsageMax < _memoryUsage)
            _memoryUsageMax = _memoryUsage;
    }
    size_t getMemory() const { return _memoryUsage; }
    size_t getMemoryMax() const { return _memoryUsageMax; }
    
    size_t memoryUse() const { return getMemoryMax() + _nodeAllocator.memoryUse(); }
    
private:
    const char* _jsonData = nullptr;
    
    BlockedLinearAllocator _nodeAllocator;
    mutable ImmutableStringPool _keyPool; // only mutable because getImmutable is non-const
    bool _isWriting = false;
    
    // debug stuff
    size_t _memoryUsage = 0;
    size_t _memoryUsageMax = 0;
};

static const Json sNullValue;

//-----------------------------

void Json::setKey(ImmutableString key) {
    // was converting to uint16_t
    _key = key;
}

ImmutableString Json::key() const {
    // was converting from uint16_t
    return _key;
}

const Json & Json::operator[] (uint32_t i) const {
    assert(_type == TypeArray);
    
    // Has to walk node linked list
    if (i < _count) {
        uint32_t counter = 0;
        for (const auto& it : *this) {
            if (i == counter++)
                return it;
        }
    }
    
    return sNullValue;
}
    

// run this to delete nodes that used allocateJson
/* add/fix this, iterator is const only
void Json::deleteJsonTree() {
    // need to call placement delete on anything with allocations
    // but right now that's only allocated strings from writer
    for (auto& it : *this) {
        it.deleteJsonTree();
    }
    if (is_string())
        this->~Json();
    
}
*/

const Json & Json::find(ImmutableString key) const {
    
    for (const auto& it : *this) {
        // since can't remap incoming string to immutable or index, have to do strcmp
        // and also store 8B instead of 2B for the key.
        if (key == it._key)
            return it;
    }
 
    return sNullValue;
}

const Json & Json::operator[] (const char* key) const {
    assert(_type == TypeObject);
    
    // Need to be able to test incoming for immutable
    // and could skip the strcmp then
    
    // Has to walk node linked list
    for (const auto& it : *this) {
        // since can't remap incoming string to immutable or index, have to do strcmp
        // and also store 8B instead of 2B for the key.
        if (key == it._key) // immutable passed in
            return it;
    
        // could speedup with comparing lengths first
        // all immutable kes have size at ptr - 2
        if (strcmp(key, it._key) == 0)
            return it;
    }
    return sNullValue;
}

bool Json::iterate(const Json*& it) const {
    if (it == nullptr) {
        assert(_type == TypeObject || _type == TypeArray);
        it = _value.aval;
    }
    else {
        it = it->_next;
    }
    
    return it != nullptr;
}

//bool Json::operator== (const Json &other) const {
//    // Some cases where comparing int/double is okay, or bool to 1/0, etc
//    if (_type != other._type)
//        return false;
//    
//    // what about comparing key?
//    
//    switch(_type) {
//        case NUL: return true;
//        case BOOL: return _value.bval == other._value.bval;
//        case NUMBER: return _value.dval == other._value.dval;
//        case STRING: 
//            // This doesn't work if not null terminated, need count of each string
//            return strcmp(_value.sval, other._value.sval) == 0;
//        case ARRAY:
//        case OBJECT: return _value.aval == other._value.aval;
//    }
//}
//
// not needed with linear key search
//bool Json::operator< (const Json &other) const {
//    if (_type != other._type)
//        return _type < other._type;
//    
//    // what about sorting key?
//   
//    switch(_type) {
//        case NUL: return true;
//        case BOOL: return _value.bval < other._value.bval;
//        case NUMBER: return _value.dval < other._value.dval;
//        case STRING: return strcmp(_value.sval, other._value.sval) < 0;
//        case ARRAY:
//        case OBJECT: return _value.aval < other._value.aval;
//    }
//}

//-------------------

Json::JsonValue::JsonValue(const char* v, uint32_t count, bool allocated) : sval(v) {
    if (allocated) {
        // need to allocate memory here
        sval = new char[count+1];
        memcpy((void*)sval, v, count+1);
    }
}

Json::JsonValue::JsonValue(const Json::array& values, Type t) : aval(nullptr) {
    /* This is more complex
    assert(_data.isWriting());
    
    Json** next = &aval;
    for (const auto& it: values) {
        Json* json = _data.allocateJson();
        // TODO: new (json) Json(*it);
        if (json->is_string()) {
            json->_flags = FlagsAllocatedUnencoded;
        }

        // chain the nodes, but not doubly-linked list
        *next = json->_next;
        next = &json->_next;
        
        // TODO: this has to be recursive to copy the array
        // since nodes weren't allocated from the BlockAllocator.
    }
    
    *next = nullptr;
    */
}

/////////////////////////////////
// Parsing

// Format char c suitable for printing in an error message.
static inline string esc(char c) {
    char buf[12];
    if (static_cast<uint8_t>(c) >= 0x20 && static_cast<uint8_t>(c) <= 0x7f) {
        snprintf(buf, sizeof buf, "'%c' (%d)", c, c);
    } else {
        snprintf(buf, sizeof buf, "(%d)", c);
    }
    return string(buf);
}

static inline bool in_range(long x, long lower, long upper) {
    return (x >= lower && x <= upper);
}

// decode to UTF-8 and add it to out.
static void decode_to_utf8(long pt, string & out) {
    if (pt < 0)
        return;

    if (pt < 0x80) {
        out += static_cast<char>(pt);
    } else if (pt < 0x800) {
        out += static_cast<char>((pt >> 6) | 0xC0);
        out += static_cast<char>((pt & 0x3F) | 0x80);
    } else if (pt < 0x10000) {
        out += static_cast<char>((pt >> 12) | 0xE0);
        out += static_cast<char>(((pt >> 6) & 0x3F) | 0x80);
        out += static_cast<char>((pt & 0x3F) | 0x80);
    } else {
        out += static_cast<char>((pt >> 18) | 0xF0);
        out += static_cast<char>(((pt >> 12) & 0x3F) | 0x80);
        out += static_cast<char>(((pt >> 6) & 0x3F) | 0x80);
        out += static_cast<char>((pt & 0x3F) | 0x80);
    }
}

// Parse a string, starting at the current position.
static bool decode_string(const char* str, uint32_t strSize, string& out) {
    uint32_t i = 0;
    long last_escaped_codepoint = -1;
    while (true) {
        if (i == strSize) {
            //fail("unexpected end of input in string");
            return false;
        }

        char ch = str[i++];

        if (ch == '"') {
            decode_to_utf8(last_escaped_codepoint, out);
            return true;
        }

        if (in_range(ch, 0, 0x1f)) {
            //fail("unescaped " + esc(ch) + " in string");
            return false;
        }
        // The usual case: non-escaped characters
        if (ch != '\\') {
            decode_to_utf8(last_escaped_codepoint, out);
            last_escaped_codepoint = -1;
            out += ch;
            continue;
        }

        // Handle escapes
        if (i == strSize) {
            //fail("unexpected end of input in string");
            return false;
        }
        
        ch = str[i++];

        if (ch == 'u') {
            // Explicitly check length of the substring. The following loop
            // relies on string returning the terminating NUL when
            // accessing str[length]. Checking here reduces brittleness.
            if (i + 4 >= strSize) {
                //fail("bad \\u escape:");
                return false;
            }
            
            // Extract 4-byte escape sequence
            char esc[5] = {};
            esc[0] = str[i+0];
            esc[1] = str[i+1];
            esc[2] = str[i+2];
            esc[3] = str[i+3];
            i += 4;
            
            for (size_t j = 0; j < 4; j++) {
                if (!in_range(esc[j], 'a', 'f') && !in_range(esc[j], 'A', 'F')
                    && !in_range(esc[j], '0', '9'))
                {
                    //fail("bad \\u escape: " + esc);
                    return false;
                }
            }

            long codepoint = strtol(esc, nullptr, 16);

            // JSON specifies that characters outside the BMP shall be encoded as a pair
            // of 4-hex-digit \u escapes encoding their surrogate pair components. Check
            // whether we're in the middle of such a beast: the previous codepoint was an
            // escaped lead (high) surrogate, and this is a trail (low) surrogate.
            if (in_range(last_escaped_codepoint, 0xD800, 0xDBFF)
                    && in_range(codepoint, 0xDC00, 0xDFFF)) {
                // Reassemble the two surrogate pairs into one astral-plane character, per
                // the UTF-16 algorithm.
                decode_to_utf8((((last_escaped_codepoint - 0xD800) << 10)
                             | (codepoint - 0xDC00)) + 0x10000, out);
                last_escaped_codepoint = -1;
            } else {
                decode_to_utf8(last_escaped_codepoint, out);
                last_escaped_codepoint = codepoint;
            }

            i += 4;
            continue;
        }

        decode_to_utf8(last_escaped_codepoint, out);
        last_escaped_codepoint = -1;

        if (ch == 'b') {
            out += '\b';
        } else if (ch == 'f') {
            out += '\f';
        } else if (ch == 'n') {
            out += '\n';
        } else if (ch == 'r') {
            out += '\r';
        } else if (ch == 't') {
            out += '\t';
        } else if (ch == '"' || ch == '\\' || ch == '/') {
            out += ch;
        } else {
            //fail("invalid escape character " + esc(ch));
            return false;
        }
    }
    
    return true;
}

//------------------------------------

JsonReader::JsonReader() {
    _data = make_unique<JsonReaderData>();
}
JsonReader::~JsonReader() {
    //delete _data;
    //_data = nullptr;
}

size_t JsonReader::memoryUse() const {
    return _data->memoryUse();
}

ImmutableString JsonReader::getImmutableKey(const char* key) {
    return _data->getImmutableKey(key);
}

void JsonReader::fail(const string& msg) {
    if (!failed) {
        err = msg;
        
        // append the line count
        err += " line:";
        err += to_string(lineCount);
        
        failed = true;
    }
}

// Advance until the current character is non-whitespace.
void JsonReader::consume_whitespace() {
    while (char ch = str[i]) {
        if (!(ch == ' ' || ch == '\r' || ch == '\n' || ch == '\t'))
            break;
        if (ch == '\n') {
            lineCount++;
        }
        i++;
    }
}

// Advance comments (c-style inline and multiline).
bool JsonReader::consume_comment() {
  bool comment_found = false;
  if (str[i] == '/') {
    i++;
    if (i == strSize) {
        fail("unexpected end of input after start of comment");
        return false;
    }
    if (str[i] == '/') { // inline comment
      i++;
      // advance until next line, or end of input
      while (i < strSize && str[i] != '\n') {
        i++;
      }
      comment_found = true;
    }
    else if (str[i] == '*') { // multiline comment
      i++;
      if (i > strSize-2) {
        fail("unexpected end of input inside multi-line comment");
        return false;
      }
      // advance until closing tokens
      while (!(str[i] == '*' && str[i+1] == '/')) {
        i++;
          if (i > strSize-2) {
              fail("unexpected end of input inside multi-line comment");
              return false;
          }
      }
      i += 2;
      comment_found = true;
    }
    else {
        fail("malformed comment");
        return false;
    }
  }
  return comment_found;
}

// Advance until the current character is non-whitespace and non-comment.
void JsonReader::consume_garbage() {
    consume_whitespace();
    bool comment_found = false;
    do {
        comment_found = consume_comment();
        if (failed)
            return;
        consume_whitespace();
    }
    while(comment_found);
}

                   
// Return the next non-whitespace character. If the end of the input is reached,
// flag an error and return 0.
char JsonReader::get_next_token() {
    consume_garbage();
    if (failed)
        return 0;
    if (i == strSize) {
        fail("unexpected end of input");
        return 0;
    }
    return str[i++];
}

// This is much simpler since it just searches for ending "
void JsonReader::parse_string_location(uint32_t& count) {
    while (true) {
        if (i == strSize) {
            fail("unexpected end of input in string");
            return;
        }

        count++;
        char ch = str[i++];

        // stop when reach the end quote
        if (ch == '\"') {
            --count;
            return;
        }
    }
}

// This is not used in parsing, but will need to convert strings
//   that are aliased and unescape them.



// Parse a double.
double JsonReader::parse_number() {
    size_t start_pos = i;
    double value = 0.0;
    
    // this is mostly a bunch of validation, then atoi/strtod
    if (str[i] == '-')
        i++;

    // Integer part
    if (str[i] == '0') {
        i++;
        if (in_range(str[i], '0', '9')) {
            fail("leading 0s not permitted in numbers");
            return value;
        }
    } else if (in_range(str[i], '1', '9')) {
        i++;
        while (in_range(str[i], '0', '9'))
            i++;
    } else {
        fail("invalid " + esc(str[i]) + " in number");
        return value;
    }

    if (str[i] != '.' && str[i] != 'e' && str[i] != 'E'
            && (i - start_pos) <= static_cast<size_t>(numeric_limits<int>::digits10)) {
        
        
#if USE_CHARCONV
        // TODO:: switch to from_chars, int but not fp supported
        from_chars(str + start_pos, str + i, value);
#else
        // this is locale dependent, other bad stuff
        value = (double)StringToInt64(str + start_pos);
#endif
        return value;
    }

    // Decimal part
    if (str[i] == '.') {
        i++;
        if (!in_range(str[i], '0', '9')) {
            fail("at least one digit required in fractional part");
            return value;
        }
        
        while (in_range(str[i], '0', '9'))
            i++;
    }

    // Exponent part
    if (str[i] == 'e' || str[i] == 'E') {
        i++;

        if (str[i] == '+' || str[i] == '-')
            i++;

        if (!in_range(str[i], '0', '9')) {
            fail("at least one digit required in exponent");
            return value;
        }
        
        while (in_range(str[i], '0', '9'))
            i++;
    }

#if USE_CHARCONV
    from_chars(str + start_pos, str + i, value);
#else
    value = strtod(str + start_pos, nullptr);
#endif
    return value;
}

bool JsonReader::compareStringForLength(const char* expected, size_t length)
{
    if (i + length > strSize)
        return false;
    
    for (uint32_t j = 0; j < length; ++j) {
        if (str[i+j] != expected[j])
            return false;
    }
    return true;
}

// Expect that 'str' starts at the character that was just read. If it does, advance
// the input and return res. If not, flag an error.
bool JsonReader::expect(const char* expected) {
    size_t expectedLength = strlen(expected);
    assert(i != 0);
    i--;
    if (compareStringForLength(expected, expectedLength)) {
        i += expectedLength;
        return true;
    } else {
        fail(string("parse error: expected ") + expected);
        return false;
    }
}

ImmutableString JsonReader::parse_key()
{
    uint32_t keyCount = 0;
    const char* keyStart = &str[i];
    parse_string_location(keyCount);
    
    // need to null terminate to convert to ImmutableString
    assert(keyCount < 256);
    char tmp[256] = {};
    memcpy(tmp, keyStart, keyCount);
    tmp[keyCount] = 0;
    
    const char* key = _data->getImmutableKey(tmp);
    return key;
}

// Parse a JSON object.  Recursive so limit stack/heap vars.
void JsonReader::parse_json(int depth, Json& parent, ImmutableString key) {
    if (depth > maxDepth) {
        fail("exceeded maximum nesting depth");
        return;
    }

    // exit out of the recursion (added this)
    if (failed) {
        return;
    }
    
    char ch = get_next_token();
    if (failed) {
        return;
    }
    
    if (ch == '-' || (ch >= '0' && ch <= '9')) {
        i--;
        Json* json = _data->allocateJson();
        parent.addNumber(json, parse_number(), key);
        return;
    }

    if (ch == 't') {
        if (expect("true")) {
            Json* json = _data->allocateJson();
            parent.addBoolean(json, true, key);
        }
        return;
    }
    
    if (ch == 'f') {
        if (expect("false")) {
            Json* json = _data->allocateJson();
            parent.addBoolean(json, false, key);
        }
        return;
    }
    
    if (ch == 'n') {
        if (expect("null")) {
            Json* json = _data->allocateJson();
            parent.addNull(json, key);
        }
        return;
    }
    
    if (ch == '"') {
        uint32_t strStart = i;
        uint32_t strCount = 0;
        parse_string_location(strCount);
        Json* json = _data->allocateJson();
        parent.addString(json, &str[strStart], strCount, Json::FlagsAliasedEncoded, key);
        return;
    }

    // TODO: dedupe keys?
    if (ch == '{') {
        ch = get_next_token();
            
        // empty object
        Json* json = _data->allocateJson();
        parent.addObject(json, key);
            
        if (ch == '}')
            return;

        while (true) {
            // parse the "key" : value
            if (ch != '"') {
                fail("expected '\"' in object, got " + esc(ch));
                return;
            }
            
            const char* objectKey = parse_key();
            if (failed)
                return;

            ch = get_next_token();
            if (ch != ':') {
                fail("expected ':' in object, got " + esc(ch));
                return;
            }
            
            // parse the value, and add it if valid
            parse_json(depth + 1, *json, objectKey);
            if (failed)
                return;

            ch = get_next_token();
            if (ch == '}')
                break;
            if (ch != ',') {
                fail("expected ',' in object, got " + esc(ch));
                return;
            }
            ch = get_next_token();
        }
        return;
    }

    // arrays have numeric keys, and are not sparse
    if (ch == '[') {
         ch = get_next_token();
            
        // empty array
        Json* json = _data->allocateJson();
        parent.addArray(json, key);
            
        if (ch == ']')
            return;

        while (true) {
            i--;
            parse_json(depth + 1, *json);
            if (failed)
                return;

            ch = get_next_token();
            if (ch == ']')
                break;
            if (ch != ',') {
                fail("expected ',' in list, got " + esc(ch));
                return;
            }
            
            ch = get_next_token();
            (void)ch;
        }
        
        // this is what was adding array up to parent
        return;
    }

    fail("expected value, got " + esc(ch));
    return;
}

//----------------


void Json::createRoot()
{
    _type = TypeArray; // TODO: should this have unique type?, basically just retrieve aval[0]
}

void Json::addJson(Json* json)
{
    assert(is_array() || is_object());
    
    // link to end of list, requires a search, how to speed this up
    Json** next = &_value.aval;
    for (uint32_t i = 0; i < _count; ++i) {
        next = &((*next)->_next);
    }
    
    *next = json;
    _count++;
}

void Json::addString(Json* json, const char* str, uint32_t len, Flags flags, ImmutableString key)
{
    new (json) Json(str, len, false);
    json->_flags = flags;
    if (key) json->setKey(key);
    addJson(json);
}

void Json::addNull(Json* json, ImmutableString key) {
    new (json) Json();
    if (key) json->setKey(key);
    addJson(json);
}

void Json::addNumber(Json* json, double number, ImmutableString key) {
    new (json) Json(number);
    if (key) json->setKey(key);
    addJson(json);
}

void Json::addBoolean(Json* json, bool b, ImmutableString key) {
    new (json) Json(b);
    if (key) json->setKey(key);
    addJson(json);
}

void Json::addArray(Json* json, ImmutableString key) {
    new (json) Json();
    json->_type = TypeArray;
    if (key) json->setKey(key);
    addJson(json);
}

void Json::addObject(Json* json, ImmutableString key) {
    new (json) Json();
    json->_type = TypeObject;
    if (key) json->setKey(key);
    addJson(json);
}

//------------------------

Json::Json(const Json::array &values, Json::Type t)
    : _type(t), _count(values.size()), _value(values) {
    assert(t == TypeObject || t == TypeArray);
}

Json::~Json() {
    switch(_type) {
        case TypeString:
            if (_flags == FlagsAllocatedUnencoded) {
                delete [] _value.sval;
                //_data.trackMemory(-_count);
                _value.sval = nullptr;
                _count = 0;
            }
            break;
            
//        case TypeArray:
//        case TypeObject:
//            // TODO: this has to free tree if there are allocated strings
//            // but that would only be during writes?
//            _value.aval = nullptr;
//            _count = 0;
//            break;
        default: break;
    }
}

void Json::trackMemory(int32_t size)
{
    //_data->trackMemory(size);
}

void JsonReader::resetAndFree() {
    _data->resetAndFree();
}

Json* JsonReader::read(const char* str_, uint32_t strCount_) {
    // construct parser off input
    // Something needs to keep data alive for the Json nodes
    //JsonReader parser { str, strCount, 0 };
    
    str = str_;
    strSize = strCount_;
    failed = false;
    err.clear();
    
    // reuse the memory,
    // this should be state that caller holds onto.
    _data->reset();
    
    // fake root
    Json* root = _data->allocateJson();
    new (root) Json();
    root->createRoot();
    
    // parse it
    parse_json(0, *root);

    // Check for any trailing garbage
    consume_garbage();
    if (failed) {
        return nullptr;
    }
    if (i != strSize) {
        fail("unexpected trailing " + esc(str[i]));
        return nullptr;
    }
    return root;
}

const char* Json::string_value(string& str) const {
    str.clear();
    if (!is_string())
        return "";
    
    if (_flags == FlagsAliasedEncoded) {
        // This string length is the encoded length, so decoded should be shorter
        if (!decode_string(_value.sval, _count, str)) {
            return "";
        }
        return str.c_str();
    }
    
    // already not-encoded, so can return.  When written this goes through encode.
    return _value.sval;
}


// revisit this later
// Documented in json11.h
//Json::array Json::parse_multi(const char* in,
//                               size_t &parser_stop_pos,
//                               string &err) {
//    JsonReader parser { in, strlen(in), 0, err, false };
//    parser_stop_pos = 0;
//    Json::array json_vec;
//    while (parser.i != parser.strSize && !parser.failed) {
//        json_vec.push_back(parser.parse_json(0));
//        if (parser.failed)
//            break;
//
//        // Check for another object
//        parser.consume_garbage();
//        if (parser.failed)
//            break;
//        parser_stop_pos = parser.i;
//    }
//    return json_vec;
//}

/////////////////////////////////
// Shape-checking

/*
bool Json::has_shape(const shape & types, string & err) const {
    if (!is_object()) {
        err = "expected JSON object, got " + dump();
        return false;
    }

    const auto& obj_items = object_items();
    for (auto & item : types) {
        const auto it = obj_items.find(item.first);
        if (it == obj_items.cend() || it->second.type() != item.second) {
            err = "bad type for " + item.first + " in " + dump();
            return false;
        }
    }

    return true;
}
*/

} // namespace json11
