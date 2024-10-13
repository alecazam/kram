#pragma once

#include "Engine.h"

namespace M4 {

/** In addition to the values in this enum, all of the ASCII characters are
valid tokens. */
enum HLSLToken {
    // The order here must match the order in the _reservedWords

    // Built-in types.
    HLSLToken_Float = 256,
    HLSLToken_Float2,
    HLSLToken_Float3,
    HLSLToken_Float4,
    HLSLToken_Float2x2,
    HLSLToken_Float3x3,
    HLSLToken_Float4x4,

    // for Nvidia/Adreno
    HLSLToken_Halfio,
    HLSLToken_Half2io,
    HLSLToken_Half3io,
    HLSLToken_Half4io,

    // for Android w/o fp16 storage
    HLSLToken_Halfst,
    HLSLToken_Half2st,
    HLSLToken_Half3st,
    HLSLToken_Half4st,

    HLSLToken_Half,
    HLSLToken_Half2,
    HLSLToken_Half3,
    HLSLToken_Half4,
    HLSLToken_Half2x2,
    HLSLToken_Half3x3,
    HLSLToken_Half4x4,

    HLSLToken_Double,
    HLSLToken_Double2,
    HLSLToken_Double3,
    HLSLToken_Double4,
    HLSLToken_Double2x2,
    HLSLToken_Double3x3,
    HLSLToken_Double4x4,

    HLSLToken_Bool,
    HLSLToken_Bool2,
    HLSLToken_Bool3,
    HLSLToken_Bool4,

    HLSLToken_Int,
    HLSLToken_Int2,
    HLSLToken_Int3,
    HLSLToken_Int4,

    HLSLToken_Uint,
    HLSLToken_Uint2,
    HLSLToken_Uint3,
    HLSLToken_Uint4,

    HLSLToken_Short,
    HLSLToken_Short2,
    HLSLToken_Short3,
    HLSLToken_Short4,

    HLSLToken_Ushort,
    HLSLToken_Ushort2,
    HLSLToken_Ushort3,
    HLSLToken_Ushort4,

    HLSLToken_Long,
    HLSLToken_Long2,
    HLSLToken_Long3,
    HLSLToken_Long4,

    HLSLToken_Ulong,
    HLSLToken_Ulong2,
    HLSLToken_Ulong3,
    HLSLToken_Ulong4,

    // TODO: u/char
    HLSLToken_Texture2D,
    HLSLToken_Texture3D,
    HLSLToken_TextureCube,
    HLSLToken_Texture2DArray,
    HLSLToken_TextureCubeArray,
    HLSLToken_Texture2DMS,

    HLSLToken_Depth2D,
    HLSLToken_Depth2DArray,
    HLSLToken_DepthCube,
    // TODO: other depth types

    HLSLToken_RWTexture2D,

    HLSLToken_SamplerState,
    HLSLToken_SamplerComparisonState,

    // Reserved words.
    HLSLToken_If,
    HLSLToken_Else,
    HLSLToken_For,
    HLSLToken_While,
    HLSLToken_Break,
    HLSLToken_True,
    HLSLToken_False,
    HLSLToken_Void,
    HLSLToken_Struct,

    // dx9
    HLSLToken_CBuffer,
    HLSLToken_TBuffer,

    // dx10 templated types (TODO: hook to parser and generator)
    HLSLToken_ConstantBuffer,
    HLSLToken_StructuredBuffer,
    HLSLToken_RWStructuredBuffer,
    // HLSLToken_AppendStructuredBuffer,
    // HLSLToken_ConsumeStructuredBuffer,
    HLSLToken_ByteAddressBuffer,
    HLSLToken_RWByteAddressBuffer,
    // RWTexture, and other types

    HLSLToken_Register,
    HLSLToken_Return,
    HLSLToken_Continue,
    HLSLToken_Discard,

    HLSLToken_Const,
    HLSLToken_Static,
    HLSLToken_Inline,

    // Input modifiers.
    HLSLToken_Uniform,
    HLSLToken_In,
    HLSLToken_Out,
    HLSLToken_InOut,

    // Effect keywords.
    //HLSLToken_SamplerStateBlock,
    //HLSLToken_Technique,
    //HLSLToken_Pass,

    // These all start with #
    HLSLToken_Include,
    // HLSLToken_Pragma
    // HLSLToken_Line

    //===================
    // End of strings that have to match in _reservedWords in .cpp

    // Multi-character symbols.
    HLSLToken_LessEqual,
    HLSLToken_GreaterEqual,
    HLSLToken_EqualEqual,
    HLSLToken_NotEqual,
    HLSLToken_PlusPlus,
    HLSLToken_MinusMinus,
    HLSLToken_PlusEqual,
    HLSLToken_MinusEqual,
    HLSLToken_TimesEqual,
    HLSLToken_DivideEqual,
    HLSLToken_LogicalAnd, // &&
    HLSLToken_LogicalOr, // ||

    // Other token types.
    HLSLToken_FloatLiteral,
    HLSLToken_HalfLiteral,
    HLSLToken_IntLiteral,

    HLSLToken_Identifier,
    HLSLToken_Comment, // Alec added this

    HLSLToken_EndOfStream,
};

class HLSLTokenizer {
public:
    /// Maximum string length of an identifier.
    constexpr static int s_maxIdentifier = 255 + 1;
    constexpr static int s_maxComment = 4096;

    /// The file name is only used for error reporting.
    HLSLTokenizer(const char* fileName, const char* buffer, size_t length);

    /// Advances to the next token in the stream.
    void Next();

    /// Returns the current token in the stream.
    int GetToken() const;

    /// Returns the number of the current token.
    float GetFloat() const;
    int GetInt() const;

    /// Returns the identifier for the current token.
    const char* GetIdentifier() const;

    /// Returns the comment text for the current token.
    const char* GetComment() const;

    /// Returns the line number where the current token began.
    int GetLineNumber() const;

    /// Returns the file name where the current token began.
    const char* GetFileName() const;

    /// Gets a human readable text description of the current token.
    void GetTokenName(char buffer[s_maxIdentifier]) const;

    /// Reports an error using printf style formatting. The current line number
    /// is included. Only the first error reported will be output.
    void Error(const char* format, ...) M4_PRINTF_ATTR(2, 3);

    /// Gets a human readable text description of the specified token.
    static void GetTokenName(int token, char buffer[s_maxIdentifier]);

    /// Tokenizer will default to strip double-slash comments, but this tries to preserve them if true
    void SetKeepComments(bool enable) { m_keepComments = enable; }

private:
    bool SkipWhitespace();
    bool SkipComment();
    bool SkipPragmaDirective();
    bool SkipInclude();

    bool ScanNumber();
    bool ScanLineDirective();

private:
    const char* m_fileName = nullptr;
    const char* m_buffer = nullptr;
    const char* m_bufferEnd = nullptr;
    int m_lineNumber = 0;
    bool m_error = false;

    int m_token = 0;
    float m_fValue = 0.0f;
    int m_iValue = 0;
    char m_identifier[s_maxIdentifier] = {};
    char m_comment[s_maxComment] = {};
    char m_lineDirectiveFileName[s_maxIdentifier] = {};
    int m_tokenLineNumber = 0;
    bool m_keepComments = false;
};

} //namespace M4
