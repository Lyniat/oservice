#pragma author Laurin "lyniat" Muth
#pragma description OService Serialization Protocol

import std.sys;
import std.string;

#pragma extension ossp
#pragma endian big
#pragma magic [ 50 53 53 4F ] @ 0x00 //OSSP

// colors https://lospec.com/palette-list/fairydust-8

using Float = double;
using Integer = s64;
using MagicNumber = u32;
using Flags = u64;
using EOD_Position = u32;

enum ST_TYPE : u8 {
    ST_FALSE = 0,
    ST_TRUE = 1,
    ST_INT = 2,
    ST_FLOAT = 3,
    ST_SYMBOL = 4,
    ST_HASH = 5,
    ST_ARRAY = 6,
    ST_STRING = 7,
    ST_UNDEF = 8,
    ST_NIL = 9,
    ST_EOF = 69,
} [[color("f0dab1")]];

using KeyValuePair;
using DataValue;

struct ST_Int {
    Integer value;
} [[name(value), color("2ba9b4")]];

struct ST_Float {
    Float value;
} [[name(value), color("93d4b5")]];

struct ST_Symbol {
    u16 len [[hidden]];
    char value[len];
} [[name(value), color("634b7d")]];

struct ST_String {
    u16 len [[hidden]];
    char value[len];
} [[name(value), color("e39aac")]];

struct ST_Array {
    u16 count [[hidden]];
    DataValue elements[count] [[inline]];
} [[name("Array"), color("f0f6e8")]];

struct ST_Hash {
    u16 count [[hidden]];
    KeyValuePair pairs[count] [[inline]];
} [[name("Hash"), color("f0f6e8")]];

struct ST_False {
    padding[0];
} [[name("False")]];

struct ST_True {
    padding[0];
} [[name("True")]];

struct ST_Nil {
    padding[0];
} [[name("Nil")]];

struct ST_Undef {
    padding[0];
} [[name("Undefined")]];

struct ST_EOD{
    char value[3] [[color("ff0000")]];
} [[name("EOD!")]];

struct ST_EOF{
    char value[3] [[color("ff0000")]];
} [[name("EOF!")]];

struct METADATA{
    char string[while(std::mem::read_string($, std::string::length("\0")) != "\0" && std::mem::read_unsigned($, 1) != 0x00)];
} [[name("Metadata"), color("ffff00")]];

struct DataStart {
    MagicNumber;
    EOD_Position;
    Flags;
    DataValue;
    ST_EOD;
    METADATA;
    char value[1] [[color("ffff00")]];
    ST_EOF;
};

struct DataValue {
    ST_TYPE type;

    if(type == ST_TYPE::ST_FALSE) {
        ST_False value;
    } else if (type == ST_TYPE::ST_TRUE) {
        ST_True value;
    } else if (type == ST_TYPE::ST_INT) {
        ST_Int value;
    } else if (type == ST_TYPE::ST_FLOAT) {
        ST_Float value;
    } else if (type == ST_TYPE::ST_SYMBOL) {
        ST_Symbol value;
    } else if (type == ST_TYPE::ST_HASH) {
        ST_Hash value;
    } else if (type == ST_TYPE::ST_ARRAY) {
        ST_Array value;
    } else if (type == ST_TYPE::ST_STRING) {
        ST_String value;
    } else if (type == ST_TYPE::ST_UNDEF) {
        ST_Undef value;
    } else if (type == ST_TYPE::ST_NIL) {
        ST_Nil value;
    }
};

struct KeyValuePair {
    ST_TYPE key_type [[hidden]];

    if(key_type == ST_TYPE::ST_FALSE) {
        padding[0] [[type("ST_FALSE")]];
    } else if (key_type == ST_TYPE::ST_TRUE) {
        padding[0] [[type("ST_TRUE")]];
    } else if (key_type == ST_TYPE::ST_INT) {
        ST_Int key;
    } else if (key_type == ST_TYPE::ST_FLOAT) {
        ST_Float key;
    } else if (key_type == ST_TYPE::ST_SYMBOL) {
        ST_Symbol key;
    } else if (key_type == ST_TYPE::ST_STRING) {
        ST_String key [[name("Key")]];
    }

    DataValue value [[inline]];
} [[name(key.value)]];

DataStart root @ 0x00;