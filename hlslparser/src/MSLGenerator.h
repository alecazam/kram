#pragma once

#include "CodeWriter.h"
#include "HLSLTree.h"

namespace M4 {

class HLSLTree;
struct HLSLFunction;
struct HLSLStruct;

struct MSLOptions {
    int (*attributeCallback)(const char* name, uint32_t index) = NULL;

    // no CLI to set offset
    uint32_t bufferRegisterOffset = 0;

    bool writeFileLine = false;
    bool treatHalfAsFloat = false;
};

/**
 * This class is used to generate MSL shaders.
 */
class MSLGenerator {
public:
    MSLGenerator();

    bool Generate(HLSLTree* tree, HLSLTarget target, const char* entryName, const MSLOptions& options = MSLOptions());
    const char* GetResult() const;

private:
    // @@ Rename class argument. Add buffers & textures.
    struct ClassArgument {
        const char* name;
        HLSLType type;
        //const char* typeName;     // @@ Do we need more than the type name?
        const char* registerName;
        bool isRef;

        ClassArgument* nextArg;

        ClassArgument(const char* name, HLSLType type, const char* registerName, bool isRef) : name(name), type(type), registerName(registerName), isRef(isRef)
        {
            nextArg = NULL;
        }
    };

    void AddClassArgument(ClassArgument* arg);

    void Prepass(HLSLTree* tree, HLSLTarget target, HLSLFunction* entryFunction);
    void CleanPrepass();

    void PrependDeclarations();

    void OutputStaticDeclarations(int indent, HLSLStatement* statement);
    void OutputStatements(int indent, HLSLStatement* statement);
    void OutputAttributes(int indent, HLSLAttribute* attribute);
    void OutputDeclaration(HLSLDeclaration* declaration);
    void OutputStruct(int indent, HLSLStruct* structure);
    void OutputBuffer(int indent, HLSLBuffer* buffer);
    void OutputFunction(int indent, HLSLFunction* function);
    void OutputExpression(HLSLExpression* expression, HLSLExpression* parentExpression);
    void OutputTypedExpression(const HLSLType& type, HLSLExpression* expression, HLSLExpression* parentExpression);
    bool NeedsCast(const HLSLType& target, const HLSLType& source);
    void OutputCast(const HLSLType& type);

    void OutputArguments(HLSLArgument* argument);
    void OutputDeclaration(const HLSLType& type, const char* name, HLSLExpression* assignment, bool isRef = false, bool isConst = false, int alignment = 0);
    void OutputDeclarationType(const HLSLType& type, bool isConst = false, bool isRef = false, int alignment = 0, bool isTypeCast = false);
    void OutputDeclarationBody(const HLSLType& type, const char* name, HLSLExpression* assignment, bool isRef = false);
    void OutputExpressionList(HLSLExpression* expression);
    void OutputExpressionList(const HLSLType& type, HLSLExpression* expression);
    void OutputExpressionList(HLSLArgument* argument, HLSLExpression* expression);

    void OutputFunctionCallStatement(int indent, HLSLFunctionCall* functionCall, HLSLDeclaration* assingmentExpression);
    void OutputFunctionCall(HLSLFunctionCall* functionCall, HLSLExpression* parentExpression);

    const char* TranslateInputSemantic(const char* semantic);
    const char* TranslateOutputSemantic(const char* semantic);

    const char* GetTypeName(const HLSLType& type, bool exactType);
    const char* GetAddressSpaceName(HLSLBaseType baseType, HLSLAddressSpace addressSpace) const;

    bool CanSkipWrittenStatement(const HLSLStatement* statement) const;

    void Error(const char* format, ...) const M4_PRINTF_ATTR(2, 3);

private:
    CodeWriter m_writer;

    HLSLTree* m_tree;
    const char* m_entryName;
    HLSLTarget m_target;
    MSLOptions m_options;

    mutable bool m_error;

    ClassArgument* m_firstClassArgument;
    ClassArgument* m_lastClassArgument;

    HLSLFunction* m_currentFunction;
};

} //namespace M4
