#pragma once

#include "Engine.h"

namespace M4
{

/** In addition to the values in this enum, all of the ASCII characters are
valid tokens. */
enum HLSLToken
{
    // The order here must match the order in the _reservedWords
    
    // Built-in types.
    HLSLToken_Float         = 256,
    HLSLToken_Float2,
    HLSLToken_Float3,
    HLSLToken_Float4,
    
	HLSLToken_Float2x2,
    HLSLToken_Float3x3,
    HLSLToken_Float4x4,
    //HLSLToken_Float4x3,
   // HLSLToken_Float4x2,
    
    HLSLToken_Half,
    HLSLToken_Half2,
    HLSLToken_Half3,
    HLSLToken_Half4,
    
	HLSLToken_Half2x2,
    HLSLToken_Half3x3,
    HLSLToken_Half4x4,
    //HLSLToken_Half4x3,
    //HLSLToken_Half4x2,
    
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
    
    // TODO: double, u/char
    HLSLToken_Texture2D,
    HLSLToken_Texture3D,
    HLSLToken_TextureCube,
    HLSLToken_Texture2DArray,
    HLSLToken_TextureCubeArray,
    HLSLToken_Texture2DMS,
    
    HLSLToken_SamplerState,
    HLSLToken_SamplerComparisonState,
    
    // these are all the texture ops
    // do these need tokenized?
    /*
    HLSLToken_Sample,
    HLSLToken_SampleLevel,
    HLSLToken_SampleCmp,
    HLSLToken_SampleCmpLevelZero,
    HLSLToken_SampleCmpGrad,
    HLSLToken_SampleBias,
    HLSLToken_GatherRed,
    HLSLToken_GatherGreen,
    HLSLToken_GatherBlue,
    HLSLToken_GatherAlpha, // + GatherCmdRed/Green/Blue/Alpha
    HLSLToken_GetDimensions,
    */
    
//    HLSLToken_Texture,
//    HLSLToken_Sampler,
//    HLSLToken_Sampler2D,
//    HLSLToken_Sampler3D,
//    HLSLToken_SamplerCube,
//    HLSLToken_Sampler2DShadow,
//    HLSLToken_Sampler2DMS,
//    HLSLToken_Sampler2DArray,

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
    HLSLToken_CBuffer,
    HLSLToken_TBuffer,
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
    HLSLToken_SamplerStateBlock, 
    HLSLToken_Technique,
    HLSLToken_Pass,

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
    HLSLToken_LogicalAnd,       // &&
    HLSLToken_LogicalOr,        // ||
    
    // Other token types.
    HLSLToken_FloatLiteral,
	HLSLToken_HalfLiteral,
    HLSLToken_IntLiteral,
    
    HLSLToken_Identifier,
    HLSLToken_Comment,          // Alec added this
    
    HLSLToken_EndOfStream,
};

class HLSLTokenizer
{

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
    int   GetInt() const;

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
    void Error(const char* format, ...);

    /// Gets a human readable text description of the specified token.
    static void GetTokenName(int token, char buffer[s_maxIdentifier]);

    /// Tokenizer will default to strip double-slash comments, but this tries to preserve them if true
    void SetKeepComments(bool enable) { m_keepComments = enable; }
    
private:

    bool SkipWhitespace();
    bool SkipComment();
	bool SkipPragmaDirective();
    bool ScanNumber();
    bool ScanLineDirective();

private:

    const char*         m_fileName = nullptr;
    const char*         m_buffer = nullptr;
    const char*         m_bufferEnd = nullptr;
    int                 m_lineNumber = 0;
    bool                m_error = false;

    int                 m_token = 0;
    float               m_fValue = 0.0f;
    int                 m_iValue = 0;
    char                m_identifier[s_maxIdentifier] = {};
    char                m_comment[s_maxComment] = {};
    char                m_lineDirectiveFileName[s_maxIdentifier] = {};
    int                 m_tokenLineNumber = 0;
    bool                m_keepComments = false;

};

}

