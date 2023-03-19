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

// TODO: try to unify some options with MSLGenerator
struct HLSLOptions
{
    // int (*attributeCallback)(const char* name, uint32_t index) = NULL;
    // uint32_t bufferRegisterOffset = 0;
    
    bool writeFileLine = false;
    
    // TODO: hook this up
    // bool treatHalfAsFloat = false;
    // bool treadDoubleAsFloat = true;
    
    // add vk constructions to HLSL source to convert to Spriv
    // bool writeVulkan = true;
};

/**
 * This class is used to generate HLSL which is compatible with the D3D9
 * compiler (i.e. no cbuffers).
 */
class HLSLGenerator
{

public:
    HLSLGenerator();
    
    
    
    bool Generate(HLSLTree* tree, HLSLTarget target, const char* entryName, const HLSLOptions& options = HLSLOptions() );
    const char* GetResult() const;

private:

    void OutputExpressionList(HLSLExpression* expression);
    void OutputExpression(HLSLExpression* expression);
    void OutputArguments(HLSLArgument* argument);
    void OutputAttributes(int indent, HLSLAttribute* attribute);
    void OutputStatements(int indent, HLSLStatement* statement);
    void OutputDeclaration(HLSLDeclaration* declaration);
    void OutputDeclaration(const HLSLType& type, const char* name, const char* semantic = NULL, const char* registerName = NULL, HLSLExpression* defaultValue = NULL);
    void OutputDeclarationType(const HLSLType& type, bool isTypeCast = false);
    void OutputDeclarationBody(const HLSLType& type, const char* name, const char* semantic =NULL, const char* registerName = NULL, HLSLExpression * assignment = NULL);

    /** Generates a name of the format "base+n" where n is an integer such that the name
     * isn't used in the syntax tree. */
    bool ChooseUniqueName(const char* base, char* dst, int dstLength) const;

    const char* GetTypeName(const HLSLType& type);

    void Error(const char* format, ...) M4_PRINTF_ATTR(2, 3);
    
    const char* GetFormatName(HLSLBaseType bufferOrTextureType, HLSLBaseType formatType);
    
private:

    CodeWriter      m_writer;

    const HLSLTree* m_tree;
    const char*     m_entryName;
    HLSLTarget      m_target;
    bool            m_isInsideBuffer;
    bool            m_error;
    HLSLOptions     m_options;
};

} // M4
