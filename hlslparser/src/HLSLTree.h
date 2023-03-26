#pragma once

#include "Engine.h"

#include <new>

namespace M4
{

enum HLSLTarget
{
    HLSLTarget_VertexShader,
    HLSLTarget_PixelShader,
    
    HLSLTarget_ComputeShader,
    
    // none of these are portable to Metal/Android, they have own triangulation
    // HLSLTarget_GeometryShader,
    // HLSLTarget_HullShader,
    // HLSLTarget_ControlShader,
    
    // This is compute prior to frag (combined vertex + geo state)
    // HLSLTarget_MeshShader,
};

enum HLSLNodeType
{
    HLSLNodeType_Root,
    
    HLSLNodeType_Declaration,
    HLSLNodeType_Struct,
    HLSLNodeType_StructField,
    HLSLNodeType_Buffer,
    HLSLNodeType_BufferField, // TODO: or just ref structField
    
    HLSLNodeType_Function,
    HLSLNodeType_Argument,
    
    HLSLNodeType_ExpressionStatement,
    HLSLNodeType_Expression,
    HLSLNodeType_ReturnStatement,
    HLSLNodeType_DiscardStatement,
    HLSLNodeType_BreakStatement,
    HLSLNodeType_ContinueStatement,
    HLSLNodeType_IfStatement,
    HLSLNodeType_ForStatement,
    HLSLNodeType_BlockStatement,
    HLSLNodeType_UnaryExpression,
    HLSLNodeType_BinaryExpression,
    HLSLNodeType_ConditionalExpression,
    HLSLNodeType_CastingExpression,
    HLSLNodeType_LiteralExpression,
    HLSLNodeType_IdentifierExpression,
    HLSLNodeType_ConstructorExpression,
    HLSLNodeType_MemberAccess,
    HLSLNodeType_ArrayAccess,
    HLSLNodeType_FunctionCall,
    
    /* FX file stuff
    HLSLNodeType_StateAssignment,
    HLSLNodeType_SamplerState,
    HLSLNodeType_Pass,
    HLSLNodeType_Technique,
    HLSLNodeType_Pipeline,
    HLSLNodeType_Stage,
    */
    
    HLSLNodeType_Attribute,
    HLSLNodeType_Comment
};

enum HLSLBaseType
{
    HLSLBaseType_Unknown,
    HLSLBaseType_Void,
    
    // float
    HLSLBaseType_Float,
    HLSLBaseType_Float2,
    HLSLBaseType_Float3,
    HLSLBaseType_Float4,
	HLSLBaseType_Float2x2,
    HLSLBaseType_Float3x3,
    HLSLBaseType_Float4x4,
    
    HLSLBaseType_Half,
    HLSLBaseType_Half2,
    HLSLBaseType_Half3,
    HLSLBaseType_Half4,
	HLSLBaseType_Half2x2,
    HLSLBaseType_Half3x3,
    HLSLBaseType_Half4x4,
    
    HLSLBaseType_Double,
    HLSLBaseType_Double2,
    HLSLBaseType_Double3,
    HLSLBaseType_Double4,
    HLSLBaseType_Double2x2,
    HLSLBaseType_Double3x3,
    HLSLBaseType_Double4x4,
    
    // integer
    HLSLBaseType_Bool,
    HLSLBaseType_Bool2,
	HLSLBaseType_Bool3,
	HLSLBaseType_Bool4,
    
    HLSLBaseType_Int,
    HLSLBaseType_Int2,
    HLSLBaseType_Int3,
    HLSLBaseType_Int4,
    
    HLSLBaseType_Uint,
    HLSLBaseType_Uint2,
    HLSLBaseType_Uint3,
    HLSLBaseType_Uint4,
    
    HLSLBaseType_Short,
    HLSLBaseType_Short2,
    HLSLBaseType_Short3,
    HLSLBaseType_Short4,
    
    HLSLBaseType_Ushort,
    HLSLBaseType_Ushort2,
    HLSLBaseType_Ushort3,
    HLSLBaseType_Ushort4,
    
    HLSLBaseType_Long,
    HLSLBaseType_Long2,
    HLSLBaseType_Long3,
    HLSLBaseType_Long4,
    
    HLSLBaseType_Ulong,
    HLSLBaseType_Ulong2,
    HLSLBaseType_Ulong3,
    HLSLBaseType_Ulong4,
    
    // Seems like these should be subtype of HLSLTexture, but
    // many of the intrinsics require a specific type of texture.
    // MSL has many more types, included depth vs. regular textures.
    HLSLBaseType_Texture2D,
    HLSLBaseType_Texture3D,
    HLSLBaseType_TextureCube,
    HLSLBaseType_Texture2DArray,
    HLSLBaseType_TextureCubeArray,
    HLSLBaseType_Texture2DMS,
    
    HLSLBaseType_Depth2D,
    HLSLBaseType_Depth2DArray,
    HLSLBaseType_DepthCube,
    // TODO: add more depth types as needed (pair with SamplerComparisonState)
    
    HLSLBaseType_RWTexture2D,
    
    // Only 2 sampler types. - type is for defining state inside them
    HLSLBaseType_SamplerState,
    HLSLBaseType_SamplerComparisonState,
    
    HLSLBaseType_UserDefined,       // struct
    HLSLBaseType_Expression,        // type argument for defined() sizeof() and typeof().
    //HLSLBaseType_Auto,            // this wasn't hooked up
    HLSLBaseType_Comment,           // single line comments optionally transferred to output
    
    // Buffer subtypes below
    HLSLBaseType_Buffer,
    
    HLSLBaseType_Count,
    
    // counts
    //HLSLBaseType_FirstNumeric = HLSLBaseType_Float,
    //HLSLBaseType_LastNumeric = HLSLBaseType_Ulong4,
    
    //HLSLBaseType_FirstInteger = HLSLBaseType_Bool,
    //HLSLBaseType_LastInteger = HLSLBaseType_LastNumeric,
   
    HLSLBaseType_NumericCount = HLSLBaseType_Ulong4 - HLSLBaseType_Float + 1
};
  
// This a subtype to HLSLBaseType_Buffer
enum HLSLBufferType
{
    // DX9
    HLSLBufferType_CBuffer,
    HLSLBufferType_TBuffer,
    
    // DX10 templated types
    HLSLBufferType_ConstantBuffer, // indexable
    HLSLBufferType_StructuredBuffer,
    HLSLBufferType_RWStructuredBuffer,
    HLSLBufferType_ByteAddressBuffer,
    HLSLBufferType_RWByteAddressBuffer
};

enum HLSLBinaryOp
{
    // bit ops
    HLSLBinaryOp_And,
    HLSLBinaryOp_Or,
    
    // math ops
    HLSLBinaryOp_Add,
    HLSLBinaryOp_Sub,
    HLSLBinaryOp_Mul,
    HLSLBinaryOp_Div,
    
    // comparison ops
    HLSLBinaryOp_Less,
    HLSLBinaryOp_Greater,
    HLSLBinaryOp_LessEqual,
    HLSLBinaryOp_GreaterEqual,
    HLSLBinaryOp_Equal,
    HLSLBinaryOp_NotEqual,
    
    // bit ops
    HLSLBinaryOp_BitAnd,
    HLSLBinaryOp_BitOr,
    HLSLBinaryOp_BitXor,
    
    // assign ops
    HLSLBinaryOp_Assign,
    HLSLBinaryOp_AddAssign,
    HLSLBinaryOp_SubAssign,
    HLSLBinaryOp_MulAssign,
    HLSLBinaryOp_DivAssign,
};

inline bool IsCompareOp( HLSLBinaryOp op )
{
	return op == HLSLBinaryOp_Less ||
		op == HLSLBinaryOp_Greater ||
		op == HLSLBinaryOp_LessEqual ||
		op == HLSLBinaryOp_GreaterEqual ||
		op == HLSLBinaryOp_Equal ||
		op == HLSLBinaryOp_NotEqual;
}

inline bool IsArithmeticOp( HLSLBinaryOp op )
{
    return op == HLSLBinaryOp_Add ||
        op == HLSLBinaryOp_Sub ||
        op == HLSLBinaryOp_Mul ||
        op == HLSLBinaryOp_Div;
}

inline bool IsLogicOp( HLSLBinaryOp op )
{
    return op == HLSLBinaryOp_And ||
        op == HLSLBinaryOp_Or;
}

inline bool IsAssignOp( HLSLBinaryOp op )
{
    return op == HLSLBinaryOp_Assign ||
        op == HLSLBinaryOp_AddAssign ||
        op == HLSLBinaryOp_SubAssign ||
        op == HLSLBinaryOp_MulAssign ||
        op == HLSLBinaryOp_DivAssign;
}

inline bool IsBitOp( HLSLBinaryOp op )
{
    return op == HLSLBinaryOp_BitAnd ||
        op == HLSLBinaryOp_BitOr ||
        op == HLSLBinaryOp_BitXor;
}
    
enum HLSLUnaryOp
{
    HLSLUnaryOp_Negative,       // -x
    HLSLUnaryOp_Positive,       // +x
    HLSLUnaryOp_Not,            // !x
    HLSLUnaryOp_PreIncrement,   // ++x
    HLSLUnaryOp_PreDecrement,   // --x
    HLSLUnaryOp_PostIncrement,  // x++
    HLSLUnaryOp_PostDecrement,  // x++
    HLSLUnaryOp_BitNot,         // ~x
};

enum HLSLArgumentModifier
{
    HLSLArgumentModifier_None,
    HLSLArgumentModifier_In,
    HLSLArgumentModifier_Out,
    HLSLArgumentModifier_Inout,
    HLSLArgumentModifier_Uniform,
    HLSLArgumentModifier_Const,
};

enum HLSLTypeFlags
{
    HLSLTypeFlag_None = 0,
    HLSLTypeFlag_Const = 0x01,
    HLSLTypeFlag_Static = 0x02,
    //HLSLTypeFlag_Uniform = 0x04,
    //HLSLTypeFlag_Extern = 0x10,
    //HLSLTypeFlag_Volatile = 0x20,
    //HLSLTypeFlag_Shared = 0x40,
    //HLSLTypeFlag_Precise = 0x80,

    HLSLTypeFlag_Input = 0x100,
    HLSLTypeFlag_Output = 0x200,

    // Interpolation modifiers.
    HLSLTypeFlag_Linear = 0x10000,
    HLSLTypeFlag_Centroid = 0x20000,
    HLSLTypeFlag_NoInterpolation = 0x40000,
    HLSLTypeFlag_NoPerspective = 0x80000,
    HLSLTypeFlag_Sample = 0x100000,

    // Misc.
    HLSLTypeFlag_NoPromote = 0x200000,
};

enum HLSLAttributeType
{
    HLSLAttributeType_Unknown,
    
    // TODO: a lot more attributes, these are loop attributes
    // f.e. specialization constant and numthreads for HLSL
    HLSLAttributeType_Unroll,
    HLSLAttributeType_Branch,
    HLSLAttributeType_Flatten,
    HLSLAttributeType_NoFastMath,
    
};

enum HLSLAddressSpace
{
    HLSLAddressSpace_Undefined,
    
    // These only apply to MSL
    HLSLAddressSpace_Constant,
    HLSLAddressSpace_Device,
    HLSLAddressSpace_Thread,
    HLSLAddressSpace_Shared,
    // TODO: Threadgroup,
    // TODO: ThreadgroupImageblock
};


struct HLSLNode;
struct HLSLRoot;
struct HLSLStatement;
struct HLSLAttribute;
struct HLSLDeclaration;
struct HLSLStruct;
struct HLSLStructField;
struct HLSLBuffer;
struct HLSLFunction;
struct HLSLArgument;
struct HLSLExpressionStatement;
struct HLSLExpression;
struct HLSLBinaryExpression;
struct HLSLLiteralExpression;
struct HLSLIdentifierExpression;
struct HLSLConstructorExpression;
struct HLSLFunctionCall;
struct HLSLArrayAccess;
struct HLSLAttribute;

struct HLSLType
{
    explicit HLSLType(HLSLBaseType _baseType = HLSLBaseType_Unknown)
    { 
        baseType    = _baseType;
    }
    bool TestFlags(int flags_) const { return (flags & flags_) == flags_; }
    
    HLSLBaseType        baseType = HLSLBaseType_Unknown;
    HLSLBaseType        formatType = HLSLBaseType_Float;    // Half or Float (only applies to templated params like buffer/texture)
    const char*         typeName = NULL;       // For user defined types.
    bool                array = false;
    HLSLExpression*     arraySize = NULL; // can ref constant like NUM_LIGHTS
    int                 flags = 0;
    HLSLAddressSpace    addressSpace = HLSLAddressSpace_Undefined; // MSL mostly
};

// Only Statment, Argument, StructField can be marked hidden.
// But many elements like Buffer derive from Statement.

/// Base class for all nodes in the HLSL AST
struct HLSLNode
{
    HLSLNodeType        nodeType; // set to s_type
    const char*         fileName = NULL;
    int                 line = 0;
};

struct HLSLRoot : public HLSLNode
{
    static const HLSLNodeType s_type = HLSLNodeType_Root;
    HLSLStatement*      statement = NULL;          // First statement.
};

struct HLSLStatement : public HLSLNode
{
    HLSLStatement*      nextStatement = NULL;      // Next statement in the block.
    HLSLAttribute*      attributes = NULL;
    
    // This allows tree pruning.  Marked true after traversing use in
    mutable bool        hidden = false;
    
    // This is marked as false at start, and multi endpoint traversal marks
    // when a global is already written, and next write is skipped.
    mutable bool        written = false;
};

// [unroll]
struct HLSLAttribute : public HLSLNode
{
    static const HLSLNodeType s_type = HLSLNodeType_Attribute;
    HLSLAttributeType   attributeType = HLSLAttributeType_Unknown;
    HLSLExpression*     argument = NULL;
    HLSLAttribute*      nextAttribute = NULL;
};

struct HLSLDeclaration : public HLSLStatement
{
    static const HLSLNodeType s_type = HLSLNodeType_Declaration;
    const char*         name  = NULL;
    HLSLType            type;
    const char*         registerName  = NULL;       // @@ Store register index?
    const char*         semantic  = NULL;
    HLSLDeclaration*    nextDeclaration = NULL;    // If multiple variables declared on a line.
    HLSLExpression*     assignment = NULL;
    
    HLSLBuffer*         buffer = NULL; // reference cbuffer for decl
};

struct HLSLStruct : public HLSLStatement
{
    static const HLSLNodeType s_type = HLSLNodeType_Struct;
    const char*         name = NULL;
    HLSLStructField*    field = NULL;              // First field in the structure.
};

struct HLSLStructField : public HLSLNode
{
    static const HLSLNodeType s_type = HLSLNodeType_StructField;
    const char*         name = NULL;
    HLSLType            type;
    const char*         semantic = NULL;
    const char*         sv_semantic = NULL;
    HLSLStructField*    nextField = NULL;      // Next field in the structure.
    bool                hidden = false;
};

/// Buffer declaration.
struct HLSLBuffer : public HLSLStatement
{
    // These spill a ton of globals throughout shader
    bool IsGlobalFields() const
    {
        return  bufferType == HLSLBufferType_CBuffer ||
                bufferType == HLSLBufferType_TBuffer;
    }
    
    // DX changes registers for read-only vs. read-write buffers (SRV vs. UAV)
    // so constant/cbuffer use b, structured/byte use t (like textures),
    // and read-write use u.  MSL only has u and
    bool IsReadOnly() const
    {
        return  bufferType == HLSLBufferType_CBuffer ||
                bufferType == HLSLBufferType_TBuffer ||
                bufferType == HLSLBufferType_ConstantBuffer ||
                bufferType == HLSLBufferType_StructuredBuffer ||
                bufferType == HLSLBufferType_ByteAddressBuffer;
    }
    
    static const HLSLNodeType s_type = HLSLNodeType_Buffer;
    const char*         name = NULL;
    const char*         registerName = NULL;
    HLSLDeclaration*    field = NULL;
    HLSLBufferType      bufferType = HLSLBufferType_CBuffer;
    HLSLStruct*         bufferStruct = NULL;
};


/// Function declaration
struct HLSLFunction : public HLSLStatement
{
    static const HLSLNodeType s_type = HLSLNodeType_Function;
    const char*         name  = NULL;
    HLSLType            returnType;
    HLSLBaseType        memberType = HLSLBaseType_Unknown;
    const char*         semantic  = NULL;
    const char*         sv_semantic = NULL;
    int                 numArguments = 0;
    int                 numOutputArguments = 0;     // Includes out and inout arguments.
    HLSLArgument*       argument = NULL;
    HLSLStatement*      statement = NULL;
    HLSLFunction*       forward = NULL; // Which HLSLFunction this one forward-declares
    
    bool IsMemberFunction() const { return memberType != HLSLBaseType_Unknown; }
};

/// Declaration of an argument to a function.
struct HLSLArgument : public HLSLNode
{
    static const HLSLNodeType s_type = HLSLNodeType_Argument;
    const char*             name = NULL;
    HLSLArgumentModifier    modifier = HLSLArgumentModifier_None;
    HLSLType                type;
    const char*             semantic = NULL;
    const char*             sv_semantic = NULL;
    HLSLExpression*         defaultValue = NULL;
    HLSLArgument*           nextArgument = NULL;
    bool                    hidden = false;
};

/// A expression which forms a complete statement.
struct HLSLExpressionStatement : public HLSLStatement
{
    static const HLSLNodeType s_type = HLSLNodeType_ExpressionStatement;
    HLSLExpression*     expression = NULL;
};

struct HLSLReturnStatement : public HLSLStatement
{
    static const HLSLNodeType s_type = HLSLNodeType_ReturnStatement;
    HLSLExpression*     expression = NULL;
};

struct HLSLDiscardStatement : public HLSLStatement
{
    static const HLSLNodeType s_type = HLSLNodeType_DiscardStatement;
};

struct HLSLBreakStatement : public HLSLStatement
{
    static const HLSLNodeType s_type = HLSLNodeType_BreakStatement;
};

struct HLSLContinueStatement : public HLSLStatement
{
    static const HLSLNodeType s_type = HLSLNodeType_ContinueStatement;
};

struct HLSLIfStatement : public HLSLStatement
{
    static const HLSLNodeType s_type = HLSLNodeType_IfStatement;
    HLSLExpression*     condition = NULL;
    HLSLStatement*      statement = NULL;
    HLSLStatement*      elseStatement = NULL;
    bool                isStatic = false;
};

struct HLSLForStatement : public HLSLStatement
{
    static const HLSLNodeType s_type = HLSLNodeType_ForStatement;
    HLSLDeclaration*    initialization = NULL;
    HLSLExpression*     condition = NULL;
    HLSLExpression*     increment = NULL;
    HLSLStatement*      statement = NULL;
};

struct HLSLBlockStatement : public HLSLStatement
{
    static const HLSLNodeType s_type = HLSLNodeType_BlockStatement;
    HLSLStatement*      statement = NULL;
};


/// Base type for all types of expressions.
struct HLSLExpression : public HLSLNode
{
    static const HLSLNodeType s_type = HLSLNodeType_Expression;
    HLSLType            expressionType;
    HLSLExpression*     nextExpression = NULL; // Used when the expression is part of a list, like in a function call.
};

// -a
struct HLSLUnaryExpression : public HLSLExpression
{
    static const HLSLNodeType s_type = HLSLNodeType_UnaryExpression;
    HLSLUnaryOp         unaryOp = {};
    HLSLExpression*     expression = NULL;
};

/// a + b
struct HLSLBinaryExpression : public HLSLExpression
{
    static const HLSLNodeType s_type = HLSLNodeType_BinaryExpression;
    HLSLBinaryOp        binaryOp = {};
    HLSLExpression*     expression1 = NULL;
    HLSLExpression*     expression2 = NULL;
};

/// ? : construct
struct HLSLConditionalExpression : public HLSLExpression
{
    static const HLSLNodeType s_type = HLSLNodeType_ConditionalExpression;
    HLSLExpression*     condition = NULL;
    HLSLExpression*     trueExpression = NULL;
    HLSLExpression*     falseExpression = NULL;
};

/// v = (half4)v2
struct HLSLCastingExpression : public HLSLExpression
{
    static const HLSLNodeType s_type = HLSLNodeType_CastingExpression;
    HLSLType            type;
    HLSLExpression*     expression = NULL;
};

/// Float, integer, boolean, etc. literal constant.
struct HLSLLiteralExpression : public HLSLExpression
{
    static const HLSLNodeType s_type = HLSLNodeType_LiteralExpression;
    HLSLBaseType        type = HLSLBaseType_Unknown;   // Note, not all types can be literals.
    union
    {
        bool            bValue;
        float           fValue;
        int32_t         iValue;
    };
};

/// An identifier, typically a variable name or structure field name.
struct HLSLIdentifierExpression : public HLSLExpression
{
    static const HLSLNodeType s_type = HLSLNodeType_IdentifierExpression;
    const char*         name = NULL;
    bool                global = false; // This is a global variable.
};

/// float2(1, 2)
struct HLSLConstructorExpression : public HLSLExpression
{
    static const HLSLNodeType s_type = HLSLNodeType_ConstructorExpression;
	HLSLType            type;
    HLSLExpression*     argument = NULL;
};

/// object.member input.member or input[10].member
struct HLSLMemberAccess : public HLSLExpression
{
    static const HLSLNodeType s_type = HLSLNodeType_MemberAccess;
	HLSLExpression*     object = NULL;
    const char*         field = NULL;
    bool                swizzle = false;
};

/// array[index]
struct HLSLArrayAccess : public HLSLExpression
{
    static const HLSLNodeType s_type = HLSLNodeType_ArrayAccess;
	HLSLExpression*     array = NULL;
    HLSLExpression*     index = NULL;
};

/// c-style foo(arg1, arg2) - args can have defaults that are parsed
struct HLSLFunctionCall : public HLSLExpression
{
    static const HLSLNodeType s_type = HLSLNodeType_FunctionCall;
    const HLSLFunction* function = NULL;
    HLSLExpression*     argument = NULL;
    int                 numArguments = 0;
};

// TODO: finish adding this for texture and buffer ops
/// c++ style member.foo(arg1, arg2)
struct HLSLMemberFunctionCall : public HLSLExpression
{
    static const HLSLNodeType s_type = HLSLNodeType_FunctionCall;
    
    // could be buffer, texture, raytrace
    HLSLBaseType        memberType = HLSLBaseType_Unknown; // may need type for typeName?
    
    const HLSLFunction* function = NULL;
    HLSLExpression*     argument = NULL;
    int                 numArguments = 0;
};


#if 1
/*
// These are all FX file constructs
// TODO: may remove these, they just complicate the code
//   but do want to specify mix of vs/ps/cs in single files

// fx
struct HLSLStateAssignment : public HLSLNode
{
    static const HLSLNodeType s_type = HLSLNodeType_StateAssignment;
    const char*             stateName = NULL;
    int                     d3dRenderState = 0;
    union {
        int32_t             iValue;
        float               fValue;
        const char *        sValue;
    };
    HLSLStateAssignment*    nextStateAssignment = NULL;
};

// fx
struct HLSLSamplerState : public HLSLExpression // @@ Does this need to be an expression? Does it have a type? I guess type is useful.
{
    static const HLSLNodeType s_type = HLSLNodeType_SamplerState;
    int                     numStateAssignments = 0;
    HLSLStateAssignment*    stateAssignments = NULL;
};

// fx
struct HLSLPass : public HLSLNode
{
    static const HLSLNodeType s_type = HLSLNodeType_Pass;
    const char*             name = NULL;
    int                     numStateAssignments = 0;
    HLSLStateAssignment*    stateAssignments = NULL;
    HLSLPass*               nextPass = NULL;
};

// fx
struct HLSLTechnique : public HLSLStatement
{
    static const HLSLNodeType s_type = HLSLNodeType_Technique;
    const char*         name = NULL;
    int                 numPasses = 0;
    HLSLPass*           passes = NULL;
};

// fx
struct HLSLPipeline : public HLSLStatement
{
    static const HLSLNodeType s_type = HLSLNodeType_Pipeline;
    const char*             name = NULL;
    int                     numStateAssignments = 0;
    HLSLStateAssignment*    stateAssignments = NULL;
};

// fx
struct HLSLStage : public HLSLStatement
{
    static const HLSLNodeType s_type = HLSLNodeType_Stage;
    const char*             name = NULL;
    HLSLStatement*          statement = NULL;
    HLSLDeclaration*        inputs = NULL;
    HLSLDeclaration*        outputs = NULL;
};
*/
#endif

struct HLSLComment : public HLSLStatement
{
    static const HLSLNodeType s_type = HLSLNodeType_Comment;
    const char*             text = NULL;
};

/// Abstract syntax tree for parsed HLSL code.
class HLSLTree
{

public:

    explicit HLSLTree(Allocator* allocator);
    ~HLSLTree();

    /// Adds a string to the string pool used by the tree.
    const char* AddString(const char* string);
    const char* AddStringFormat(const char* string, ...) M4_PRINTF_ATTR(2, 3);

    /// Returns true if the string is contained within the tree.
    bool GetContainsString(const char* string) const;

    /// Returns the root block in the tree */
    HLSLRoot* GetRoot() const;

    /// Adds a new node to the tree with the specified type.
    template <class T>
    T* AddNode(const char* fileName, int line)
    {
        HLSLNode* node = new (AllocateMemory(sizeof(T))) T();
        node->nodeType  = T::s_type;
        node->fileName  = fileName;
        node->line      = line;
        return static_cast<T*>(node);
    }

    HLSLFunction * FindFunction(const char * name);
    HLSLDeclaration * FindGlobalDeclaration(const char * name, HLSLBuffer ** buffer_out = NULL);
    
    HLSLStruct * FindGlobalStruct(const char * name);
    HLSLBuffer * FindBuffer(const char * name);

// FX files
//    HLSLTechnique * FindTechnique(const char * name);
//    HLSLPipeline * FindFirstPipeline();
//    HLSLPipeline * FindNextPipeline(HLSLPipeline * current);
//    HLSLPipeline * FindPipeline(const char * name);
 
    bool GetExpressionValue(HLSLExpression * expression, int & value);
    int GetExpressionValue(HLSLExpression * expression, float values[4]);

    bool NeedsFunction(const char * name);

private:

    void* AllocateMemory(size_t size);
    void  AllocatePage();

private:

    static const size_t s_nodePageSize = 1024 * 4;

    struct NodePage
    {
        NodePage*   next;
        char        buffer[s_nodePageSize];
    };

    Allocator*      m_allocator;
    StringPool      m_stringPool;
    HLSLRoot*       m_root;

    NodePage*       m_firstPage;
    NodePage*       m_currentPage;
    size_t          m_currentPageOffset;

};



class HLSLTreeVisitor
{
public:
    virtual ~HLSLTreeVisitor() {}
    virtual void VisitType(HLSLType & type);

    virtual void VisitRoot(HLSLRoot * node);
    virtual void VisitTopLevelStatement(HLSLStatement * node);
    virtual void VisitStatements(HLSLStatement * statement);
    virtual void VisitStatement(HLSLStatement * node);
    virtual void VisitDeclaration(HLSLDeclaration * node);
    virtual void VisitStruct(HLSLStruct * node);
    virtual void VisitStructField(HLSLStructField * node);
    virtual void VisitBuffer(HLSLBuffer * node);
    //virtual void VisitBufferField(HLSLBufferField * node); // TODO:
    virtual void VisitFunction(HLSLFunction * node);
    virtual void VisitArgument(HLSLArgument * node);
    virtual void VisitExpressionStatement(HLSLExpressionStatement * node);
    virtual void VisitExpression(HLSLExpression * node);
    virtual void VisitReturnStatement(HLSLReturnStatement * node);
    virtual void VisitDiscardStatement(HLSLDiscardStatement * node);
    virtual void VisitBreakStatement(HLSLBreakStatement * node);
    virtual void VisitContinueStatement(HLSLContinueStatement * node);
    virtual void VisitIfStatement(HLSLIfStatement * node);
    virtual void VisitForStatement(HLSLForStatement * node);
    virtual void VisitBlockStatement(HLSLBlockStatement * node);
    virtual void VisitUnaryExpression(HLSLUnaryExpression * node);
    virtual void VisitBinaryExpression(HLSLBinaryExpression * node);
    virtual void VisitConditionalExpression(HLSLConditionalExpression * node);
    virtual void VisitCastingExpression(HLSLCastingExpression * node);
    virtual void VisitLiteralExpression(HLSLLiteralExpression * node);
    virtual void VisitIdentifierExpression(HLSLIdentifierExpression * node);
    virtual void VisitConstructorExpression(HLSLConstructorExpression * node);
    virtual void VisitMemberAccess(HLSLMemberAccess * node);
    virtual void VisitArrayAccess(HLSLArrayAccess * node);
    virtual void VisitFunctionCall(HLSLFunctionCall * node);
    
    virtual void VisitComment(HLSLComment * node);

    virtual void VisitFunctions(HLSLRoot * root);
    virtual void VisitParameters(HLSLRoot * root);

    HLSLFunction * FindFunction(HLSLRoot * root, const char * name);
    HLSLDeclaration * FindGlobalDeclaration(HLSLRoot * root, const char * name);
    HLSLStruct * FindGlobalStruct(HLSLRoot * root, const char * name);
    
    // These are fx file constructs
//    virtual void VisitStateAssignment(HLSLStateAssignment * node);
//    virtual void VisitSamplerState(HLSLSamplerState * node);
//    virtual void VisitPass(HLSLPass * node);
//    virtual void VisitTechnique(HLSLTechnique * node);
//    virtual void VisitPipeline(HLSLPipeline * node);
};


// Tree transformations:
extern void PruneTree(HLSLTree* tree, const char* entryName0, const char* entryName1 = NULL);
extern void SortTree(HLSLTree* tree);
//extern void GroupParameters(HLSLTree* tree);
extern void HideUnusedArguments(HLSLFunction * function);
extern void FlattenExpressions(HLSLTree* tree);
    
} // M4
