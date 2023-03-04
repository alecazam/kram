//=============================================================================
//
// HLSLGenerator.h
//
// Created by Max McGuire (max@unknownworlds.com)
// Copyright (c) 2013, Unknown Worlds Entertainment, Inc.
//
//=============================================================================

#pragma once

#include "CodeWriter.h"
#include "HLSLTree.h"

namespace M4
{

class  HLSLTree;
struct HLSLFunction;
struct HLSLStruct;

/**
 * This class is used to generate HLSL which is compatible with the D3D9
 * compiler (i.e. no cbuffers).
 */
class HLSLGenerator
{

public:

    enum Target
    {
        Target_VertexShader,
        Target_PixelShader,
    };

    HLSLGenerator();
    
    bool Generate(HLSLTree* tree, Target target, const char* entryName);
    const char* GetResult() const;

private:

    void OutputExpressionList(HLSLExpression* expression);
    void OutputExpression(HLSLExpression* expression);
    void OutputArguments(HLSLArgument* argument);
    void OutputAttributes(int indent, HLSLAttribute* attribute);
    void OutputStatements(int indent, HLSLStatement* statement);
    void OutputDeclaration(HLSLDeclaration* declaration);
    void OutputDeclaration(const HLSLType& type, const char* name, const char* semantic = NULL, const char* registerName = NULL, HLSLExpression* defaultValue = NULL);
    void OutputDeclarationType(const HLSLType& type);
    void OutputDeclarationBody(const HLSLType& type, const char* name, const char* semantic =NULL, const char* registerName = NULL, HLSLExpression * assignment = NULL);

    /** Generates a name of the format "base+n" where n is an integer such that the name
     * isn't used in the syntax tree. */
    bool ChooseUniqueName(const char* base, char* dst, int dstLength) const;

    void Error(const char* format, ...);
    
private:

    CodeWriter      m_writer;

    const HLSLTree* m_tree;
    const char*     m_entryName;
    Target          m_target;
    bool            m_isInsideBuffer;
    bool            m_error;
};

} // M4
