//=============================================================================
//
// Render/HLSLParser.h
//
// Created by Max McGuire (max@unknownworlds.com)
// Copyright (c) 2013, Unknown Worlds Entertainment, Inc.
//
//=============================================================================

#pragma once

#include "Engine.h"

#include "HLSLTokenizer.h"
#include "HLSLTree.h"

namespace M4
{

struct EffectState;

// This wouldn't be needed if could preprocess prior to calling parser.
struct HLSLParserOptions
{
    bool isHalfst = false;
    
    bool isHalfio = false;
};

class HLSLParser
{

public:

    HLSLParser(Allocator* allocator, const char* fileName, const char* buffer, size_t length);
    void SetKeepComments(bool enable) { m_tokenizer.SetKeepComments(enable); }
    
    bool Parse(HLSLTree* tree, const HLSLParserOptions& options = HLSLParserOptions());

private:

    bool Accept(int token);
    bool Expect(int token);

    /**
     * Special form of Accept for accepting a word that is not actually a token
     * but will be treated like one. This is useful for HLSL keywords that are
     * only tokens in specific contexts (like in/inout in parameter lists).
     */
    bool Accept(const char* token);
    bool Expect(const char* token);

    bool AcceptIdentifier(const char*& identifier);
    bool ExpectIdentifier(const char*& identifier);
    bool AcceptFloat(float& value);
	bool AcceptHalf( float& value );
    bool AcceptInt(int& value);
    bool AcceptType(bool allowVoid, HLSLType& type);
    bool ExpectType(bool allowVoid, HLSLType& type);
    bool AcceptBinaryOperator(int priority, HLSLBinaryOp& binaryOp);
    bool AcceptUnaryOperator(bool pre, HLSLUnaryOp& unaryOp);
    bool AcceptAssign(HLSLBinaryOp& binaryOp);
    bool AcceptTypeModifier(int & typeFlags);
    bool AcceptInterpolationModifier(int& flags);

    /**
     * Handles a declaration like: "float2 name[5]". If allowUnsizedArray is true, it is
     * is acceptable for the declaration to not specify the bounds of the array (i.e. name[]).
     */
    bool AcceptDeclaration(bool allowUnsizedArray, HLSLType& type, const char*& name);
    bool ExpectDeclaration(bool allowUnsizedArray, HLSLType& type, const char*& name);

    bool ParseTopLevel(HLSLStatement*& statement);
    bool ParseBlock(HLSLStatement*& firstStatement, const HLSLType& returnType);
    bool ParseStatementOrBlock(HLSLStatement*& firstStatement, const HLSLType& returnType, bool scoped = true);
    bool ParseStatement(HLSLStatement*& statement, const HLSLType& returnType);
    bool ParseDeclaration(HLSLDeclaration*& declaration);
    bool ParseFieldDeclaration(HLSLStructField*& field);
    //bool ParseBufferFieldDeclaration(HLSLBufferField*& field);
    bool ParseExpression(HLSLExpression*& expression);
    bool ParseBinaryExpression(int priority, HLSLExpression*& expression);
    bool ParseTerminalExpression(HLSLExpression*& expression, bool& needsEndParen);
    bool ParseExpressionList(int endToken, bool allowEmptyEnd, HLSLExpression*& firstExpression, int& numExpressions);
    bool ParseArgumentList(HLSLArgument*& firstArgument, int& numArguments, int& numOutputArguments);
    bool ParseDeclarationAssignment(HLSLDeclaration* declaration);
    bool ParsePartialConstructor(HLSLExpression*& expression, HLSLBaseType type, const char* typeName);

    bool ParseStateName(bool isSamplerState, bool isPipelineState, const char*& name, const EffectState *& state);
    bool ParseColorMask(int& mask);
    
// FX file
//    bool ParseStateValue(const EffectState * state, HLSLStateAssignment* stateAssignment);
//    bool ParseStateAssignment(HLSLStateAssignment*& stateAssignment, bool isSamplerState, bool isPipelineState);
//    bool ParseSamplerState(HLSLExpression*& expression);
//    bool ParseTechnique(HLSLStatement*& statement);
//    bool ParsePass(HLSLPass*& pass);
//    bool ParsePipeline(HLSLStatement*& pipeline);
//    bool ParseStage(HLSLStatement*& stage);
    
    bool ParseComment(HLSLStatement*& statement);

    bool ParseAttributeList(HLSLAttribute*& attribute);
    bool ParseAttributeBlock(HLSLAttribute*& attribute);

    bool CheckForUnexpectedEndOfStream(int endToken);

    const HLSLStruct* FindUserDefinedType(const char* name) const;

    void BeginScope();
    void EndScope();

    void DeclareVariable(const char* name, const HLSLType& type);

    /// Returned pointer is only valid until Declare or Begin/EndScope is called. 
    const HLSLType* FindVariable(const char* name, bool& global) const;

    const HLSLFunction* FindFunction(const char* name) const;
    const HLSLFunction* FindFunction(const HLSLFunction* fun) const;

    bool GetIsFunction(const char* name) const;
    
    /// Finds the overloaded function that matches the specified call.
    /// Pass memberType to match member functions.
    const HLSLFunction* MatchFunctionCall(const HLSLFunctionCall* functionCall, const char* name, const HLSLType* memberType = NULL);

    /// Gets the type of the named field on the specified object type (fieldName can also specify a swizzle. )
    bool GetMemberType(const HLSLType& objectType, HLSLMemberAccess * memberAccess);

    bool CheckTypeCast(const HLSLType& srcType, const HLSLType& dstType);

    const char* GetFileName();
    int GetLineNumber() const;

private:

    struct Variable
    {
        const char*     name;
        HLSLType        type;
    };

    HLSLTokenizer           m_tokenizer;
    Array<HLSLStruct*>      m_userTypes;
    Array<Variable>         m_variables;
    Array<HLSLFunction*>    m_functions;
    int                     m_numGlobals;

    HLSLTree*               m_tree;
    
    bool                    m_allowUndeclaredIdentifiers = false;
    bool                    m_disableSemanticValidation = false;
    
    HLSLParserOptions       m_options;
};

enum NumericType
{
    NumericType_Float,
    NumericType_Half,
    NumericType_Double, // not in MSL
    
    NumericType_Bool,
    NumericType_Int,
    NumericType_Uint,
    NumericType_Short,
    NumericType_Ushort,
    NumericType_Ulong,
    NumericType_Long,
    
    // TODO: HLSL doesn't have byte/ubyte, MSL does
    // NumericType_UByte,
    // NumericType_Byte,
    
    NumericType_Count,
    
    NumericType_NaN, // not in count?
};

bool IsHalf(HLSLBaseType type);
bool IsFloat(HLSLBaseType type);
bool IsDouble(HLSLBaseType type);

bool IsInt(HLSLBaseType type);
bool IsUnit(HLSLBaseType type);
bool IsShort(HLSLBaseType type);
bool IsUshort(HLSLBaseType type);
bool IsLong(HLSLBaseType type);
bool IsUlong(HLSLBaseType type);
bool IsBool(HLSLBaseType type);

bool IsSamplerType(HLSLBaseType baseType);
bool IsMatrixType(HLSLBaseType baseType);
bool IsVectorType(HLSLBaseType baseType);
bool IsScalarType(HLSLBaseType baseType);
bool IsTextureType(HLSLBaseType baseType);
bool IsDepthTextureType(HLSLBaseType baseType);
bool IsBufferType(HLSLBaseType baseType);
bool IsNumericType(HLSLBaseType baseType);

bool IsFloatingType(HLSLBaseType type);
bool IsIntegerType(HLSLBaseType type);

bool IsCoreTypeEqual(HLSLBaseType lhsType, HLSLBaseType rhsType);
bool IsNumericTypeEqual(HLSLBaseType lhsType, HLSLBaseType rhsType);
bool IsDimensionEqual(HLSLBaseType lhsType, HLSLBaseType rhsType);
bool IsCrossDimensionEqual(HLSLBaseType lhsType, HLSLBaseType rhsType);

bool IsScalarType(const HLSLType& type);
bool IsVectorType(const HLSLType& type);
bool IsMatrixType(const HLSLType& type);

bool IsSamplerType(const HLSLType& type);
bool IsTextureType(const HLSLType& type);

HLSLBaseType PromoteType(HLSLBaseType toType, HLSLBaseType type);
HLSLBaseType HalfToFloatBaseType(HLSLBaseType type);
HLSLBaseType DoubleToFloatBaseType(HLSLBaseType type);

const char* GetNumericTypeName(HLSLBaseType type);

const char* GetTypeNameHLSL(const HLSLType& type);
const char* GetTypeNameMetal(const HLSLType& type);

HLSLBaseType GetScalarType(HLSLBaseType type);

// returns 1 for scalar or 2/3/4 for vector types.
int32_t GetVectorDimension(HLSLBaseType type);

}
