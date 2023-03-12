//=============================================================================
//
// Render/MSLGenerator.cpp
//
// Created by Max McGuire (max@unknownworlds.com)
// Copyright (c) 2013, Unknown Worlds Entertainment, Inc.
//
//=============================================================================

#include "MSLGenerator.h"

#include "Engine.h"
#include "HLSLParser.h"
#include "HLSLTree.h"

#include <string.h>

// MSL limitations:
// - Some type conversions and constructors don't work exactly the same way. For example, casts to smaller size vectors are not alloweed in C++. @@ Add more details...
// - Swizzles on scalar types, whether or not it expands them. a_float.x, a_float.xxxx both cause compile errors.
// - Using ints as floats without the trailing .0 makes the compiler sad.
// Unsupported by this generator:
// - Matrix [] access is implemented as a function call, so result cannot be passed as out/inout argument.
// - Matrix [] access is not supported in all l-value expressions. Only simple assignments.
// - No support for boolean vectors and logical operators involving vectors. This is not just in metal.
// - No support for non-float texture types

namespace M4
{
    static void ParseSemantic(const char* semantic, unsigned int* outputLength, unsigned int* outputIndex)
    {
        const char* semanticIndex = semantic;

        while (*semanticIndex && !isdigit(*semanticIndex))
        {
            semanticIndex++;
        }

        *outputLength = (uint32_t)(semanticIndex - semantic);
        *outputIndex = atoi(semanticIndex);
    }

    // Parse register name and advance next register index.
    static int ParseRegister(const char* registerName, int& nextRegister)
    {
        if (!registerName)
        {
            return nextRegister++;
        }

        // skip over the u/b/t register prefix
        while (*registerName && !isdigit(*registerName))
        {
            registerName++;
        }

        if (!*registerName)
        {
            return nextRegister++;
        }

        // parse the number
        int result = atoi(registerName);

        if (nextRegister <= result)
        {
            nextRegister = result + 1;
        }

        return result;
    }

    


    MSLGenerator::MSLGenerator()
    {
        m_tree = NULL;
        m_entryName = NULL;
        m_target = Target_VertexShader;
        m_error = false;

        m_firstClassArgument = NULL;
        m_lastClassArgument = NULL;

        m_currentFunction = NULL;
    }

    // Copied from GLSLGenerator
    void MSLGenerator::Error(const char* format, ...)
    {
        // It's not always convenient to stop executing when an error occurs,
        // so just track once we've hit an error and stop reporting them until
        // we successfully bail out of execution.
        if (m_error)
        {
            return;
        }
        m_error = true;

        va_list arg;
        va_start(arg, format);
        Log_ErrorArgList(format, arg);
        va_end(arg);
    }

    inline void MSLGenerator::AddClassArgument(ClassArgument* arg)
    {
        if (m_firstClassArgument == NULL)
        {
            m_firstClassArgument = arg;
        }
        else
        {
            m_lastClassArgument->nextArg = arg;
        }
        m_lastClassArgument = arg;
    }


    void MSLGenerator::Prepass(HLSLTree* tree, Target target, HLSLFunction* entryFunction)
    {
        // Hide unused arguments. @@ It would be good to do this in the other generators too.
        
        // PruneTree resets hidden flags to true, then marks visible elements
        // based on whether entry point visits them.
        PruneTree(tree, entryFunction->name); // Note: takes second entry
        
        // This sorts tree by type, but keeps ordering
        SortTree(tree);
       
        // This strips any unused inputs to the entry point function
        HideUnusedArguments(entryFunction);
        
        // Note sure if/where to add these calls.  Just wanted to point
        // out that nothing is calling them, but could be useful.
        //EmulateAlphaTest(tree, entryName, 0.5f);
        FlattenExpressions(tree);
        
        HLSLRoot* root = tree->GetRoot();
        HLSLStatement* statement = root->statement;
        ASSERT(m_firstClassArgument == NULL);

        //HLSLType samplerType(HLSLBaseType_Sampler);

        int nextTextureRegister = 0;
        int nextSamplerRegister = 0;
        int nextBufferRegister = 0;

        while (statement != NULL)
        {
            if (statement->hidden)
            {
                statement = statement->nextStatement;
                continue;
            }
            
            if (statement->nodeType == HLSLNodeType_Declaration)
            {
                HLSLDeclaration* declaration = (HLSLDeclaration*)statement;

                if (IsTextureType(declaration->type))
                {
                    const char * textureName = declaration->name;
                    
                    int textureRegister = ParseRegister(declaration->registerName, nextTextureRegister);
                     const char * textureRegisterName = m_tree->AddStringFormat("texture(%d)", textureRegister);

                    AddClassArgument(new ClassArgument(textureName, declaration->type, textureRegisterName));
                }
                else if (IsSamplerType(declaration->type))
                {
                    const char * samplerName = declaration->name;
                    
                    int samplerRegister = ParseRegister(declaration->registerName, nextSamplerRegister);
                    const char * samplerRegisterName = m_tree->AddStringFormat("sampler(%d)", samplerRegister);
                    
                    AddClassArgument(new ClassArgument(samplerName, declaration->type, samplerRegisterName));
                }
            }
            else if (statement->nodeType == HLSLNodeType_Buffer)
            {
                HLSLBuffer * buffer = (HLSLBuffer *)statement;
                
                HLSLType type(HLSLBaseType_UserDefined);
                type.addressSpace = HLSLAddressSpace_Constant;
                
                // TODO: on cbuffer is a ubo, not tbuffer, or others
                // TODO: this is having to rename due to globals
                if (buffer->IsGlobalFields())
                    type.typeName = m_tree->AddStringFormat("%s_ubo", buffer->name);
                else
                    type.typeName = m_tree->AddStringFormat("%s", buffer->bufferStruct->name);
                
                int bufferRegister = ParseRegister(buffer->registerName, nextBufferRegister) + m_options.bufferRegisterOffset;

                const char * bufferRegisterName = m_tree->AddStringFormat("buffer(%d)", bufferRegister);

                AddClassArgument(new ClassArgument(buffer->name, type, bufferRegisterName));
            }

            statement = statement->nextStatement;
        }

        // @@ IC: instance_id parameter must be a function argument. If we find it inside a struct we must move it to the function arguments
        // and patch all the references to it!

        // Translate semantics.
        HLSLArgument* argument = entryFunction->argument;
        while (argument != NULL)
        {
            if (argument->hidden)
            {
                argument = argument->nextArgument;
                continue;
            }

            if (argument->modifier == HLSLArgumentModifier_Out)
            {
                // Translate output arguments semantics.
                if (argument->type.baseType == HLSLBaseType_UserDefined)
                {
                    // Our vertex input is a struct and its fields need to be tagged when we generate that
                    HLSLStruct* structure = tree->FindGlobalStruct(argument->type.typeName);
                    if (structure == NULL)
                    {
                        Error("Vertex shader output struct '%s' not found in shader\n", argument->type.typeName);
                    }

                    HLSLStructField* field = structure->field;
                    while (field != NULL)
                    {
                        if (!field->hidden)
                        {
                            field->sv_semantic = TranslateOutputSemantic(field->semantic);
                        }
                        field = field->nextField;
                    }
                }
                else
                {
                    argument->sv_semantic = TranslateOutputSemantic(argument->semantic);
                }
            }
            else
            {
                // Translate input arguments semantics.
                if (argument->type.baseType == HLSLBaseType_UserDefined)
                {
                    // Our vertex input is a struct and its fields need to be tagged when we generate that
                    HLSLStruct* structure = tree->FindGlobalStruct(argument->type.typeName);
                    if (structure == NULL)
                    {
                        Error("Vertex shader input struct '%s' not found in shader\n", argument->type.typeName);
                    }

                    HLSLStructField* field = structure->field;
                    while (field != NULL)
                    {
                        if (!field->hidden)
                        {
                            field->sv_semantic = TranslateInputSemantic(field->semantic);

                            // Force type to uint.
                            if (field->sv_semantic && strcmp(field->sv_semantic, "sample_id") == 0) {
                                field->type.baseType = HLSLBaseType_Uint;
                                field->type.flags |= HLSLTypeFlag_NoPromote;
                            }

                            /*if (target == Target_VertexShader && is_semantic(field->semantic, "COLOR"))
                            {
                            field->type.flags |= HLSLTypeFlag_Swizzle_BGRA;
                            }*/
                        }
                        field = field->nextField;
                    }
                }
                else
                {
                    argument->sv_semantic = TranslateInputSemantic(argument->semantic);

                    // Force type to uint.
                    if (argument->sv_semantic && strcmp(argument->sv_semantic, "sample_id") == 0) {
                        argument->type.baseType = HLSLBaseType_Uint;
                        argument->type.flags |= HLSLTypeFlag_NoPromote;
                    }
                }
            }

            argument = argument->nextArgument;
        }

        // Translate return value semantic.
        if (entryFunction->returnType.baseType != HLSLBaseType_Void)
        {
            if (entryFunction->returnType.baseType == HLSLBaseType_UserDefined)
            {
                // Our vertex input is a struct and its fields need to be tagged when we generate that
                HLSLStruct* structure = tree->FindGlobalStruct(entryFunction->returnType.typeName);
                if (structure == NULL)
                {
                    Error("Vertex shader output struct '%s' not found in shader\n", entryFunction->returnType.typeName);
                }

                HLSLStructField* field = structure->field;
                while (field != NULL)
                {
                    if (!field->hidden)
                    {
                        field->sv_semantic = TranslateOutputSemantic(field->semantic);
                    }
                    field = field->nextField;
                }
            }
            else
            {
                entryFunction->sv_semantic = TranslateOutputSemantic(entryFunction->semantic);

                //Error("MSL only supports COLOR semantic in return \n", entryFunction->returnType.typeName);
            }
        }
    }

    void MSLGenerator::CleanPrepass()
    {
        ClassArgument* currentArg = m_firstClassArgument;
        while (currentArg != NULL)
        {
            ClassArgument* nextArg = currentArg->nextArg;
            delete currentArg;
            currentArg = nextArg;
        }
        delete currentArg;
        m_firstClassArgument = NULL;
        m_lastClassArgument = NULL;
    }

    void MSLGenerator::PrependDeclarations()
    {
        // Any special function stubs we need go here
        // That includes special constructors to emulate HLSL not being strict
        
        //Branch internally to HLSL vs. MSL verision
        m_writer.WriteLine(0, "#include \"ShaderMSL.h\"");
    }

    bool MSLGenerator::Generate(HLSLTree* tree, Target target, const char* entryName, const Options& options)
    {
        m_firstClassArgument = NULL;
        m_lastClassArgument = NULL;

        m_tree = tree;
        m_target = target;
        ASSERT(m_target == Target_VertexShader || m_target == Target_FragmentShader);
        m_entryName = entryName;
        m_options = options;

        m_writer.Reset();

        // Find entry point function
        HLSLFunction* entryFunction = tree->FindFunction(entryName);
        if (entryFunction == NULL)
        {
            Error("Entry point '%s' doesn't exist\n", entryName);
            return false;
        }

        Prepass(tree, target, entryFunction);

        PrependDeclarations();

        HLSLRoot* root = m_tree->GetRoot();

        OutputStaticDeclarations(0, root->statement);

        // In MSL, uniforms are parameters for the entry point, not globals:
        // to limit code rewriting, we wrap the entire original shader into a class.
        // Uniforms are then passed to the constructor and copied to member variables.
        std::string shaderClassNameStr = entryName;
        shaderClassNameStr += "NS"; // to distinguish from function
        
        const char* shaderClassName = shaderClassNameStr.c_str();
        // This doesn't work if want to have multiple shaders in one file
        // (target == MSLGenerator::Target_VertexShader) ? "Vertex_Shader" : "Pixel_Shader";
        m_writer.WriteLine(0, "struct %s {", shaderClassName);

        OutputStatements(1, root->statement);

        // Generate constructor
        m_writer.WriteLine(0, "");
        m_writer.BeginLine(1);

        m_writer.Write("%s(", shaderClassName);
        
        // mod
        m_writer.EndLine();
        m_writer.BeginLine(1);
        
        const ClassArgument* currentArg = m_firstClassArgument;
        while (currentArg != NULL)
        {
            if (currentArg->type.addressSpace == HLSLAddressSpace_Constant)              m_writer.Write("constant ");
            else
                m_writer.Write("thread ");

            m_writer.Write("%s & %s", GetTypeName(currentArg->type, /*exactType=*/true), currentArg->name);

            currentArg = currentArg->nextArg;
            if (currentArg)
            {
                m_writer.Write(", ");
                
                // mod
                m_writer.EndLine();
                m_writer.BeginLine(1);
            }
        }
        m_writer.Write(")");
        
        // mod
        m_writer.EndLine(); 
        m_writer.BeginLine(1);
        
        currentArg = m_firstClassArgument;
        if (currentArg)
        {
            m_writer.Write(" : ");
        }
        while (currentArg != NULL)
        {
            m_writer.Write("%s(%s)", currentArg->name, currentArg->name);
            currentArg = currentArg->nextArg;
            if (currentArg)
            {
                m_writer.Write(", ");
                
                // mod
                m_writer.EndLine();
                m_writer.BeginLine(1);
            }
        }
        m_writer.EndLine(" {}");

        m_writer.WriteLine(0, "};"); // Class


        // Generate real entry point, the one called by Metal
        m_writer.WriteLine(0, "");

        // If function return value has a non-color output semantic, declare a temporary struct for the output.
        bool wrapReturnType = false;
        if (entryFunction->sv_semantic != NULL && strcmp(entryFunction->sv_semantic, "color(0)") != 0)
        {
            wrapReturnType = true;

            m_writer.WriteLine(0, "struct %s_output { %s tmp [[%s]]; };", entryName, GetTypeName(entryFunction->returnType, /*exactType=*/true), entryFunction->sv_semantic);

            m_writer.WriteLine(0, "");
        }


        m_writer.BeginLine(0);

        // @@ Add/Translate function attributes.
        // entryFunction->attributes

        if (m_target == Target_VertexShader)
        {
            m_writer.Write("vertex ");
        }
        else
        {
            m_writer.Write("fragment ");
        }

        // Return type.
        if (wrapReturnType)
        {
            m_writer.Write("%s_output", entryName);
        }
        else
        {
            if (entryFunction->returnType.baseType == HLSLBaseType_UserDefined)
            {
                m_writer.Write("%s::", shaderClassName);
            }
            m_writer.Write("%s", GetTypeName(entryFunction->returnType, /*exactType=*/true));
        }

        m_writer.Write(" %s(", entryName);

        // Alec added for readability
        m_writer.EndLine();
        m_writer.BeginLine(1);
        
        int argumentCount = 0;
        HLSLArgument* argument = entryFunction->argument;
        while (argument != NULL)
        {
            if (!argument->hidden)
            {
                if (argument->type.baseType == HLSLBaseType_UserDefined)
                {
                    m_writer.Write("%s::", shaderClassName);
                }
                m_writer.Write("%s %s", GetTypeName(argument->type, /*exactType=*/true), argument->name);

                // @@ IC: We are assuming that the first argument is the 'stage_in'.
                if (argument->type.baseType == HLSLBaseType_UserDefined && argument == entryFunction->argument)
                {
                    m_writer.Write(" [[stage_in]]");
                }
                else if (argument->sv_semantic)
                {
                    m_writer.Write(" [[%s]]", argument->sv_semantic);
                }
                
                argumentCount++;
            }
            argument = argument->nextArgument;
            if (argument && !argument->hidden)
            {
                m_writer.Write(", ");
                
                // Alec added for readability
                m_writer.EndLine();
                m_writer.BeginLine(1);
            }
        }

        currentArg = m_firstClassArgument;
        if (argumentCount && currentArg != NULL)
        {
            m_writer.Write(", ");
        }
        while (currentArg != NULL)
        {
            //if (currentArg->type.addressSpace == HLSLAddressSpace_Constant) m_writer.Write("constant ");
            //else m_writer.Write("thread ");

            if (currentArg->type.baseType == HLSLBaseType_UserDefined)
            {
                m_writer.Write("constant %s::%s & %s [[%s]]", shaderClassName, currentArg->type.typeName, currentArg->name, currentArg->registerName);
            }
            else
            {
                m_writer.Write("%s %s [[%s]]", GetTypeName(currentArg->type, /*exactType=*/true), currentArg->name, currentArg->registerName);
            }

            currentArg = currentArg->nextArg;
            if (currentArg)
            {
                m_writer.Write(", ");
                
                // Alec added for readability
                m_writer.EndLine();
                m_writer.BeginLine(1);
            }
        }
        m_writer.EndLine(") {");

        // Create the helper class instance and call the entry point from the original shader
        m_writer.BeginLine(1);
        m_writer.Write("%s %s", shaderClassName, entryName);

        currentArg = m_firstClassArgument;
        if (currentArg)
        {
            m_writer.Write("(");

            while (currentArg != NULL)
            {
                m_writer.Write("%s", currentArg->name);
                currentArg = currentArg->nextArg;
                if (currentArg)
                {
                    m_writer.Write(", ");
                }
            }

            m_writer.Write(")");
        }
        m_writer.EndLine(";");

        m_writer.BeginLine(1);

        if (wrapReturnType)
        {
            m_writer.Write("%s_output output; output.tmp = %s.%s(", entryName, entryName, entryName);
        }
        else
        {
            m_writer.Write("return %s.%s(", entryName, entryName);
        }

        argument = entryFunction->argument;
        while (argument != NULL)
        {
            if (!argument->hidden)
            {
                m_writer.Write("%s", argument->name);
            }
            argument = argument->nextArgument;
            if (argument && !argument->hidden)
            {
                m_writer.Write(", ");
            }
        }

        m_writer.EndLine(");");

        if (wrapReturnType)
        {
            m_writer.WriteLine(1, "return output;");
        }

        m_writer.WriteLine(0, "}");

        CleanPrepass();
        m_tree = NULL;

        // Any final check goes here, but shouldn't be needed as the Metal compiler is solid

        return !m_error;
    }

    const char* MSLGenerator::GetResult() const
    {
        return m_writer.GetResult();
    }

    void MSLGenerator::OutputStaticDeclarations(int indent, HLSLStatement* statement)
    {
        while (statement != NULL)
        {
            if (statement->hidden)
            {
                statement = statement->nextStatement;
                continue;
            }

            if (statement->nodeType == HLSLNodeType_Declaration)
            {
                HLSLDeclaration* declaration = static_cast<HLSLDeclaration*>(statement);

                const HLSLType& type = declaration->type;

                if ((type.flags & HLSLTypeFlag_Const) && (type.flags & HLSLTypeFlag_Static))
                {
                    m_writer.BeginLine(indent, declaration->fileName, declaration->line);
                    OutputDeclaration(declaration);
                    m_writer.EndLine(";");

                    // hide declaration from subsequent passes
                    declaration->hidden = true;
                }
            }
            else if (statement->nodeType == HLSLNodeType_Function)
            {
                HLSLFunction* function = static_cast<HLSLFunction*>(statement);

                if (!function->forward)
                    OutputStaticDeclarations(indent, function->statement);
            }

            statement = statement->nextStatement;
        }
    }

    // recursive
    void MSLGenerator::OutputStatements(int indent, HLSLStatement* statement)
    {
        // Main generator loop: called recursively
        while (statement != NULL)
        {
            if (statement->hidden)
            {
                statement = statement->nextStatement;
                continue;
            }

            OutputAttributes(indent, statement->attributes);
            
            if (statement->nodeType == HLSLNodeType_Comment)
            {
                HLSLComment* comment = static_cast<HLSLComment*>(statement);
                m_writer.WriteLine(indent, "//%s", comment->text);
            }
            else if (statement->nodeType == HLSLNodeType_Declaration)
            {
                HLSLDeclaration* declaration = static_cast<HLSLDeclaration*>(statement);

                if (declaration->assignment && declaration->assignment->nodeType == HLSLNodeType_FunctionCall)
                {
                    OutputFunctionCallStatement(indent, (HLSLFunctionCall*)declaration->assignment, declaration);
                }
                else
                {
                    m_writer.BeginLine(indent, declaration->fileName, declaration->line);
                    OutputDeclaration(declaration);
                    m_writer.EndLine(";");
                }
            }
            else if (statement->nodeType == HLSLNodeType_Struct)
            {
                HLSLStruct* structure = static_cast<HLSLStruct*>(statement);
                OutputStruct(indent, structure);
            }
            else if (statement->nodeType == HLSLNodeType_Buffer)
            {
                HLSLBuffer* buffer = static_cast<HLSLBuffer*>(statement);
                OutputBuffer(indent, buffer);
            }
            else if (statement->nodeType == HLSLNodeType_Function)
            {
                HLSLFunction* function = static_cast<HLSLFunction*>(statement);

                if (!function->forward)
                {
                    OutputFunction(indent, function);
                }
            }
            else if (statement->nodeType == HLSLNodeType_ExpressionStatement)
            {
                HLSLExpressionStatement* expressionStatement = static_cast<HLSLExpressionStatement*>(statement);
                HLSLExpression* expression = expressionStatement->expression;

                if (expression->nodeType == HLSLNodeType_FunctionCall)
                {
                    OutputFunctionCallStatement(indent, (HLSLFunctionCall*)expression, NULL);
                }
                else
                {
                    m_writer.BeginLine(indent, statement->fileName, statement->line);
                    OutputExpression(expressionStatement->expression, NULL);
                    m_writer.EndLine(";");
                }
            }
            else if (statement->nodeType == HLSLNodeType_ReturnStatement)
            {
                HLSLReturnStatement* returnStatement = static_cast<HLSLReturnStatement*>(statement);
                if (m_currentFunction->numOutputArguments > 0)
                {
                    m_writer.BeginLine(indent, returnStatement->fileName, returnStatement->line);
                    m_writer.Write("return { ");

                    int numArguments = 0;
                    if (returnStatement->expression != NULL)
                    {
                        OutputTypedExpression(m_currentFunction->returnType, returnStatement->expression, NULL);
                        numArguments++;
                    }

                    HLSLArgument * argument = m_currentFunction->argument;
                    while (argument != NULL)
                    {
                        if (argument->modifier == HLSLArgumentModifier_Out || argument->modifier == HLSLArgumentModifier_Inout)
                        {
                            if (numArguments) m_writer.Write(", ");
                            m_writer.Write("%s", argument->name);
                            numArguments++;
                        }
                        argument = argument->nextArgument;
                    }

                    m_writer.EndLine(" };");
                }
                else if (returnStatement->expression != NULL)
                {
                    m_writer.BeginLine(indent, returnStatement->fileName, returnStatement->line);
                    m_writer.Write("return ");
                    OutputTypedExpression(m_currentFunction->returnType, returnStatement->expression, NULL);
                    m_writer.EndLine(";");
                }
                else
                {
                    m_writer.WriteLineTagged(indent, returnStatement->fileName, returnStatement->line, "return;");
                }
            }
            else if (statement->nodeType == HLSLNodeType_DiscardStatement)
            {
                HLSLDiscardStatement* discardStatement = static_cast<HLSLDiscardStatement*>(statement);
                m_writer.WriteLineTagged(indent, discardStatement->fileName, discardStatement->line, "discard_fragment();");
            }
            else if (statement->nodeType == HLSLNodeType_BreakStatement)
            {
                HLSLBreakStatement* breakStatement = static_cast<HLSLBreakStatement*>(statement);
                m_writer.WriteLineTagged(indent, breakStatement->fileName, breakStatement->line, "break;");
            }
            else if (statement->nodeType == HLSLNodeType_ContinueStatement)
            {
                HLSLContinueStatement* continueStatement = static_cast<HLSLContinueStatement*>(statement);
                m_writer.WriteLineTagged(indent, continueStatement->fileName, continueStatement->line, "continue;");
            }
            else if (statement->nodeType == HLSLNodeType_IfStatement)
            {
                HLSLIfStatement* ifStatement = static_cast<HLSLIfStatement*>(statement);

                if (ifStatement->isStatic) {
                    int value;
                    if (!m_tree->GetExpressionValue(ifStatement->condition, value)) {
                        Error("@if condition could not be evaluated.\n");
                    }
                    if (value != 0) {
                        OutputStatements(indent + 1, ifStatement->statement);
                    }
                    else if (ifStatement->elseStatement != NULL) {
                        OutputStatements(indent + 1, ifStatement->elseStatement);
                    }
                }
                else {
                    m_writer.BeginLine(indent, ifStatement->fileName, ifStatement->line);
                    m_writer.Write("if (");
                    OutputExpression(ifStatement->condition, NULL);
                    m_writer.Write(") {");
                    m_writer.EndLine();
                    OutputStatements(indent + 1, ifStatement->statement);
                    m_writer.WriteLine(indent, "}");
                    if (ifStatement->elseStatement != NULL)
                    {
                        m_writer.WriteLine(indent, "else {");
                        OutputStatements(indent + 1, ifStatement->elseStatement);
                        m_writer.WriteLine(indent, "}");
                    }
                }
            }
            else if (statement->nodeType == HLSLNodeType_ForStatement)
            {
                HLSLForStatement* forStatement = static_cast<HLSLForStatement*>(statement);
                m_writer.BeginLine(indent, forStatement->fileName, forStatement->line);
                m_writer.Write("for (");
                OutputDeclaration(forStatement->initialization);
                m_writer.Write("; ");
                OutputExpression(forStatement->condition, NULL);
                m_writer.Write("; ");
                OutputExpression(forStatement->increment, NULL);
                m_writer.Write(") {");
                m_writer.EndLine();
                OutputStatements(indent + 1, forStatement->statement);
                m_writer.WriteLine(indent, "}");
            }
            else if (statement->nodeType == HLSLNodeType_BlockStatement)
            {
                HLSLBlockStatement* blockStatement = static_cast<HLSLBlockStatement*>(statement);
                m_writer.WriteLineTagged(indent, blockStatement->fileName, blockStatement->line, "{");
                OutputStatements(indent + 1, blockStatement->statement);
                m_writer.WriteLine(indent, "}");
            }
            else if (statement->nodeType == HLSLNodeType_Technique)
            {
                // Techniques are ignored.
            }
            else if (statement->nodeType == HLSLNodeType_Pipeline)
            {
                // Pipelines are ignored.
            }
            else
            {
                // Unhandled statement type.
                Error("Unknown statement");
            }

            statement = statement->nextStatement;
        }
    }

    // Called by OutputStatements
    void MSLGenerator::OutputAttributes(int indent, HLSLAttribute* attribute)
    {
        // IC: These do not appear to exist in MSL.
        while (attribute != NULL) {
            if (attribute->attributeType == HLSLAttributeType_Unroll)
            {
                // @@ Do any of these work?
                //m_writer.WriteLine(indent, attribute->fileName, attribute->line, "#pragma unroll");
                //m_writer.WriteLine(indent, attribute->fileName, attribute->line, "[[unroll]]");
            }
            else if (attribute->attributeType == HLSLAttributeType_Flatten)
            {
                // @@
            }
            else if (attribute->attributeType == HLSLAttributeType_Branch)
            {
                // @@, [[likely]]?
            }

            attribute = attribute->nextAttribute;
        }
    }

    void MSLGenerator::OutputDeclaration(HLSLDeclaration* declaration)
    {
        if (IsSamplerType(declaration->type))
        {
            m_writer.Write("thread sampler& %s", declaration->name);
        }
        else if (IsTextureType(declaration->type))
        {
            // Declare a texture and a sampler instead
            // We do not handle multiple textures on the same line
            // ASSERT(declaration->nextDeclaration == NULL);
            
            const char* textureName = GetTypeName(declaration->type, true);
            if (textureName)
                m_writer.Write("thread %s& %s", textureName, declaration->name);
            else
                Error("Unknown texture");
        }
        else
        {
            OutputDeclaration(declaration->type, declaration->name, declaration->assignment);

            declaration = declaration->nextDeclaration;
            while (declaration != NULL)
            {
                m_writer.Write(",");
                OutputDeclarationBody(declaration->type, declaration->name, declaration->assignment);
                declaration = declaration->nextDeclaration;
            };
        }
    }

    void MSLGenerator::OutputStruct(int indent, HLSLStruct* structure)
    {
        m_writer.WriteLineTagged(indent, structure->fileName, structure->line, "struct %s {", structure->name);
        HLSLStructField* field = structure->field;
        while (field != NULL)
        {
            if (!field->hidden)
            {
                m_writer.BeginLine(indent + 1, field->fileName, field->line);
                OutputDeclaration(field->type, field->name, NULL);
                
                // DONE: would need a semantic remap for all possible semantics
                // just use the name the caller specified if sv_semantic
                // is not set.  The header can handle translating
                if (field->sv_semantic)
                {
                    m_writer.Write(" [[%s]]", field->sv_semantic);
                }
// Alec added this fallback, but then it tags too many fields
//                else if (field->semantic)
//                {
//                    m_writer.Write(" [[%s]]", field->semantic);
//                }
                

                m_writer.Write(";");
                m_writer.EndLine();
            }
            field = field->nextField;
        }
        m_writer.WriteLine(indent, "};");
    }

    void MSLGenerator::OutputBuffer(int indent, HLSLBuffer* buffer)
    {
        if (!buffer->IsGlobalFields())
        {
            m_writer.BeginLine(indent, buffer->fileName, buffer->line);
            
            // TODO: handle array case for indexing, also
            if (buffer->bufferType == HLSLBufferType_ConstantBuffer ||
               buffer->bufferType == HLSLBufferType_ByteAddressBuffer ||
               buffer->bufferType == HLSLBufferType_StructuredBuffer)
            {
                m_writer.WriteLine(indent, "constant %s & %s", buffer->bufferStruct->name, buffer->name);
            }
            else
            {
                // is this thread space?
                m_writer.WriteLine(indent, "thread %s & %s",  buffer->bufferStruct->name, buffer->name);
            }
            
            m_writer.EndLine(";");
        }
        else
        {
            HLSLDeclaration* field = buffer->field;
            
            m_writer.BeginLine(indent, buffer->fileName, buffer->line);
            
            // TODO: these aren't all ubo, some are structured buffer
            m_writer.Write("struct %s_ubo", buffer->name);
            m_writer.EndLine(" {");
            while (field != NULL)
            {
                if (!field->hidden)
                {
                    m_writer.BeginLine(indent + 1, field->fileName, field->line);
                    OutputDeclaration(field->type, field->name, field->assignment, false, false, 0); // /*alignment=*/16);
                    m_writer.EndLine(";");
                }
                field = (HLSLDeclaration*)field->nextStatement;
            }
            m_writer.WriteLine(indent, "};");
            
            m_writer.WriteLine(indent, "constant %s_ubo & %s;", buffer->name, buffer->name);
        }
    }

    void MSLGenerator::OutputFunction(int indent, HLSLFunction* function)
    {
        const char* functionName = function->name;
        const char* returnTypeName = GetTypeName(function->returnType, /*exactType=*/false);

        // Declare output tuple.
        if (function->numOutputArguments > 0)
        {
            returnTypeName = m_tree->AddStringFormat("%s_out%d", functionName, function->line); // @@ Find a better way to generate unique name.

            m_writer.BeginLine(indent, function->fileName, function->line);
            m_writer.Write("struct %s { ", returnTypeName);
            m_writer.EndLine();

            if (function->returnType.baseType != HLSLBaseType_Void)
            {
                m_writer.BeginLine(indent + 1, function->fileName, function->line);
                OutputDeclaration(function->returnType, "__result", /*defaultValue=*/NULL, /*isRef=*/false, /*isConst=*/false);
                m_writer.EndLine(";");
            }

            HLSLArgument * argument = function->argument;
            while (argument != NULL)
            {
                if (argument->modifier == HLSLArgumentModifier_Out || argument->modifier == HLSLArgumentModifier_Inout)
                {
                    m_writer.BeginLine(indent + 1, function->fileName, function->line);
                    OutputDeclaration(argument->type, argument->name, /*defaultValue=*/NULL, /*isRef=*/false, /*isConst=*/false);
                    m_writer.EndLine(";");
                }
                argument = argument->nextArgument;
            }

            m_writer.WriteLine(indent, "};");

            // Create unique function name to avoid collision with overloads and different return types.
            m_writer.BeginLine(indent, function->fileName, function->line);
            m_writer.Write("%s %s_%d(", returnTypeName, functionName, function->line);
        }
        else
        {
            m_writer.BeginLine(indent, function->fileName, function->line);
            m_writer.Write("%s %s(", returnTypeName, functionName);
        }

        OutputArguments(function->argument);

        m_writer.EndLine(") {");
        m_currentFunction = function;

        // Local declarations for output arguments.
        HLSLArgument * argument = function->argument;
        while (argument != NULL)
        {
            if (argument->modifier == HLSLArgumentModifier_Out)
            {
                m_writer.BeginLine(indent + 1, function->fileName, function->line);
                OutputDeclaration(argument->type, argument->name, /*defaultValue=*/NULL, /*isRef=*/false, /*isConst=*/false);
                m_writer.EndLine(";");
            }
            argument = argument->nextArgument;
        }

        OutputStatements(indent + 1, function->statement); // @@ Modify return statements if function has multiple output arguments!

        // Output implicit return.
        if (function->numOutputArguments > 0)
        {
            bool needsImplicitReturn = true;
            HLSLStatement * statement = function->statement;
            if (statement != NULL)
            {
                while (statement->nextStatement != NULL)
                {
                    statement = statement->nextStatement;
                }
                needsImplicitReturn = (statement->nodeType != HLSLNodeType_ReturnStatement) && function->returnType.baseType == HLSLBaseType_Void;
            }

            if (needsImplicitReturn)
            {
                m_writer.BeginLine(indent + 1);
                m_writer.Write("return { ");

                int numArguments = 0;
                HLSLArgument * argument = m_currentFunction->argument;
                while (argument != NULL)
                {
                    if (argument->modifier == HLSLArgumentModifier_Out || argument->modifier == HLSLArgumentModifier_Inout)
                    {
                        if (numArguments) m_writer.Write(", ");
                        m_writer.Write("%s ", argument->name);
                        numArguments++;
                    }
                    argument = argument->nextArgument;
                }

                m_writer.EndLine(" };");
            }
        }

        m_writer.WriteLine(indent, "};");
        m_currentFunction = NULL;
    }


    // @@ We could be a lot smarter removing parenthesis based on the operator precedence of the parent expression.
    static bool NeedsParenthesis(HLSLExpression* expression, HLSLExpression* parentExpression) {

        // For now we just omit the parenthesis if there's no parent expression.
        if (parentExpression == NULL)
        {
            return false;
        }

        // One more special case that's pretty common.
        if (parentExpression->nodeType == HLSLNodeType_MemberAccess)
        {
            if (expression->nodeType == HLSLNodeType_IdentifierExpression ||
                expression->nodeType == HLSLNodeType_ArrayAccess ||
                expression->nodeType == HLSLNodeType_MemberAccess)
            {
                return false;
            }
        }

        return true;
    }

    bool MSLGenerator::NeedsCast(const HLSLType & target, const HLSLType & source)
    {
        HLSLBaseType targetType = target.baseType;
        HLSLBaseType sourceType = source.baseType;

        if (sourceType == HLSLBaseType_Int) {
            // int k = 1;
        }

        /*if (IsScalarType(target))
        {
        // Scalar types do not need casting.
        return false;
        }*/

        if (m_options.treatHalfAsFloat)
        {
            // use call to convert half back to float type
            if (IsHalf(targetType)) targetType = HalfToFloatBaseType(targetType);
            if (IsHalf(sourceType)) sourceType = HalfToFloatBaseType(sourceType );
        }

        return targetType != sourceType && (IsCoreTypeEqual(targetType, sourceType) || IsScalarType(sourceType));
    }


    void MSLGenerator::OutputTypedExpression(const HLSLType& type, HLSLExpression* expression, HLSLExpression* parentExpression)
    {
        // If base types are not exactly the same, do explicit cast.
        bool closeCastExpression = false;
        if (NeedsCast(type, expression->expressionType))
        {
            OutputCast(type);
            m_writer.Write("(");
            closeCastExpression = true;
        }

        OutputExpression(expression, parentExpression);

        if (closeCastExpression)
        {
            m_writer.Write(")");
        }
    }

    void MSLGenerator::OutputExpression(HLSLExpression* expression, HLSLExpression* parentExpression)
    {
        if (expression->nodeType == HLSLNodeType_IdentifierExpression)
        {
            HLSLIdentifierExpression* identifierExpression = static_cast<HLSLIdentifierExpression*>(expression);
            const char* name = identifierExpression->name;
            
            /* don't need this either, just pass the same name
                calls now take tex and sampler if specified
             
            if (identifierExpression->global && IsSamplerType(identifierExpression->expressionType))
            {
                // TODO: just pass in
                m_writer.Write("%s", name);
            }
            else if (identifierExpression->global && IsTextureType(identifierExpression->expressionType))
            {
                // TODO: just pass in
                m_writer.Write("%s", name);
            }
            */
            
            /* Stop wrapping in structs
            // For texture declaration, generate a struct instead
            if (identifierExpression->global && IsSamplerType(identifierExpression->expressionType))
            {
                // TODO: this code doesn't preserve user name for reflection
                // Appends_texture/_sampler appended to name and forces combined tex/sampler.
                
                if (identifierExpression->expressionType.baseType == HLSLBaseType_Sampler2D)
                {
                    if (identifierExpression->expressionType.samplerType == HLSLBaseType_Half && !m_options.treatHalfAsFloat)
                    {
                        m_writer.Write("Texture2DHalfSampler(%s_texture, %s_sampler)", name, name);
                    }
                    else
                    {
                        m_writer.Write("Texture2DSampler(%s_texture, %s_sampler)", name, name);
                    }
                }
                else if (identifierExpression->expressionType.baseType == HLSLBaseType_Sampler3D)
                    m_writer.Write("Texture3DSampler(%s_texture, %s_sampler)", name, name);
                else if (identifierExpression->expressionType.baseType == HLSLBaseType_SamplerCube)
                    m_writer.Write("TextureCubeSampler(%s_texture, %s_sampler)", name, name);
                else if (identifierExpression->expressionType.baseType == HLSLBaseType_Sampler2DShadow)
                    m_writer.Write("Texture2DShadowSampler(%s_texture, %s_sampler)", name, name);
                else if (identifierExpression->expressionType.baseType == HLSLBaseType_Sampler2DMS)
                    m_writer.Write("%s_texture", name);
                else if (identifierExpression->expressionType.baseType == HLSLBaseType_Sampler2DArray)
                    m_writer.Write("Texture2DArraySampler(%s_texture, %s_sampler)", name, name);
                else
                    Error("<unhandled texture type>");
            }
            
            else */
            {
                if (identifierExpression->global)
                {
                    HLSLBuffer * buffer;
                    HLSLDeclaration * declaration = m_tree->FindGlobalDeclaration(identifierExpression->name, &buffer);

                    if (declaration && declaration->buffer)
                    {
                        ASSERT(buffer == declaration->buffer);
                        m_writer.Write("%s.", declaration->buffer->name);
                    }
                }
                m_writer.Write("%s", name);

                // IC: Add swizzle if this is a member access of a field that has the swizzle flag.
                /*if (parentExpression->nodeType == HLSLNodeType_MemberAccess)
                {
                HLSLMemberAccess* memberAccess = (HLSLMemberAccess*)parentExpression;
                const HLSLType & objectType = memberAccess->object->expressionType;
                const HLSLStruct* structure = m_tree->FindGlobalStruct(objectType.typeName);
                if (structure != NULL)
                {
                const HLSLStructField* field = structure->field;
                while (field != NULL)
                {
                if (field->name == name)
                {
                if (field->type.flags & HLSLTypeFlag_Swizzle_BGRA)
                {
                m_writer.Write(".bgra", name);
                }
                }
                }
                }
                }*/
            }
        }
        else if (expression->nodeType == HLSLNodeType_CastingExpression)
        {
            HLSLCastingExpression* castingExpression = static_cast<HLSLCastingExpression*>(expression);
            OutputCast(castingExpression->type);
            m_writer.Write("(");
            OutputExpression(castingExpression->expression, castingExpression);
            m_writer.Write(")");
        }
        else if (expression->nodeType == HLSLNodeType_ConstructorExpression)
        {
            HLSLConstructorExpression* constructorExpression = static_cast<HLSLConstructorExpression*>(expression);
            
            m_writer.Write("%s(", GetTypeName(constructorExpression->type, /*exactType=*/false));
            //OutputExpressionList(constructorExpression->type, constructorExpression->argument);   // @@ Get element type.
            OutputExpressionList(constructorExpression->argument);
            m_writer.Write(")");
        }
        else if (expression->nodeType == HLSLNodeType_LiteralExpression)
        {
            HLSLLiteralExpression* literalExpression = static_cast<HLSLLiteralExpression*>(expression);
            
            char floatBuffer[32];
            
            switch (literalExpression->type)
            {
            case HLSLBaseType_Half:
                if (m_options.treatHalfAsFloat) {
                    snprintf(floatBuffer, sizeof(floatBuffer), "%f", literalExpression->fValue);
                    String_StripTrailingFloatZeroes(floatBuffer);
                    m_writer.Write("%s", floatBuffer);
                }
                else {
                    // TODO: reduce digits since fp16 has much less precision
                    snprintf(floatBuffer, sizeof(floatBuffer), "%f", literalExpression->fValue);
                    String_StripTrailingFloatZeroes(floatBuffer);
                    m_writer.Write("%sh", floatBuffer);
                }
                break;
            case HLSLBaseType_Float:
                snprintf(floatBuffer, sizeof(floatBuffer), "%f", literalExpression->fValue);
                String_StripTrailingFloatZeroes(floatBuffer);
                m_writer.Write("%s", floatBuffer);
                break;
            case HLSLBaseType_Int:
                m_writer.Write("%d", literalExpression->iValue);
                break;
            case HLSLBaseType_Bool:
                m_writer.Write("%s", literalExpression->bValue ? "true" : "false");
                break;
            // TODO: missing uint, u/short, double
            default:
                Error("Unhandled literal");
                //ASSERT(0);
            }
        }
        else if (expression->nodeType == HLSLNodeType_UnaryExpression)
        {
            HLSLUnaryExpression* unaryExpression = static_cast<HLSLUnaryExpression*>(expression);
            const char* op = "?";
            bool pre = true;
            switch (unaryExpression->unaryOp)
            {
            case HLSLUnaryOp_Negative:      op = "-";  break;
            case HLSLUnaryOp_Positive:      op = "+";  break;
            case HLSLUnaryOp_Not:           op = "!";  break;
            case HLSLUnaryOp_BitNot:        op = "~";  break;
            case HLSLUnaryOp_PreIncrement:  op = "++"; break;
            case HLSLUnaryOp_PreDecrement:  op = "--"; break;
            case HLSLUnaryOp_PostIncrement: op = "++"; pre = false; break;
            case HLSLUnaryOp_PostDecrement: op = "--"; pre = false; break;
            }
            bool addParenthesis = NeedsParenthesis(unaryExpression->expression, expression);
            if (addParenthesis) m_writer.Write("(");
            if (pre)
            {
                m_writer.Write("%s", op);
                OutputExpression(unaryExpression->expression, unaryExpression);
            }
            else
            {
                OutputExpression(unaryExpression->expression, unaryExpression);
                m_writer.Write("%s", op);
            }
            if (addParenthesis) m_writer.Write(")");
        }
        else if (expression->nodeType == HLSLNodeType_BinaryExpression)
        {
            HLSLBinaryExpression* binaryExpression = static_cast<HLSLBinaryExpression*>(expression);

            bool addParenthesis = NeedsParenthesis(expression, parentExpression);
            if (addParenthesis) m_writer.Write("(");

            /* forcing use of column matrices, this column work isn't needed
            bool rewrite_assign = false;
            if (binaryExpression->binaryOp == HLSLBinaryOp_Assign && binaryExpression->expression1->nodeType == HLSLNodeType_ArrayAccess)
            {
                HLSLArrayAccess* arrayAccess = static_cast<HLSLArrayAccess*>(binaryExpression->expression1);
                if (!arrayAccess->array->expressionType.array && IsMatrixType(arrayAccess->array->expressionType.baseType))
                {
                    // TODO: Alec, I eliminated the set_column code
                    // does that need added back?
                    rewrite_assign = true;

                    m_writer.Write("set_column(");
                    OutputExpression(arrayAccess->array, NULL);
                    m_writer.Write(", ");
                    OutputExpression(arrayAccess->index, NULL);
                    m_writer.Write(", ");
                    OutputExpression(binaryExpression->expression2, NULL);
                    m_writer.Write(")");
                }
            }

            if (!rewrite_assign)
            */
            {
                if (IsArithmeticOp(binaryExpression->binaryOp) || IsLogicOp(binaryExpression->binaryOp))
                {
                    // Do intermediate type promotion, without changing dimension:
                    HLSLType promotedType = binaryExpression->expression1->expressionType;

                    if (!IsNumericTypeEqual(binaryExpression->expressionType.baseType, promotedType.baseType))
                    {
                        promotedType.baseType = PromoteType(binaryExpression->expressionType.baseType, promotedType.baseType);
                    }

                    OutputTypedExpression(promotedType, binaryExpression->expression1, binaryExpression);
                }
                else
                {
                    OutputExpression(binaryExpression->expression1, binaryExpression);
                }

                const char* op = "?";
                switch (binaryExpression->binaryOp)
                {
                case HLSLBinaryOp_Add:          op = " + "; break;
                case HLSLBinaryOp_Sub:          op = " - "; break;
                case HLSLBinaryOp_Mul:          op = " * "; break;
                case HLSLBinaryOp_Div:          op = " / "; break;
                case HLSLBinaryOp_Less:         op = " < "; break;
                case HLSLBinaryOp_Greater:      op = " > "; break;
                case HLSLBinaryOp_LessEqual:    op = " <= "; break;
                case HLSLBinaryOp_GreaterEqual: op = " >= "; break;
                case HLSLBinaryOp_Equal:        op = " == "; break;
                case HLSLBinaryOp_NotEqual:     op = " != "; break;
                case HLSLBinaryOp_Assign:       op = " = "; break;
                case HLSLBinaryOp_AddAssign:    op = " += "; break;
                case HLSLBinaryOp_SubAssign:    op = " -= "; break;
                case HLSLBinaryOp_MulAssign:    op = " *= "; break;
                case HLSLBinaryOp_DivAssign:    op = " /= "; break;
                case HLSLBinaryOp_And:          op = " && "; break;
                case HLSLBinaryOp_Or:           op = " || "; break;
                case HLSLBinaryOp_BitAnd:       op = " & "; break;
                case HLSLBinaryOp_BitOr:        op = " | "; break;
                case HLSLBinaryOp_BitXor:       op = " ^ "; break;
                default:
                    Error("unhandled literal");
                    //ASSERT(0);
                }
                m_writer.Write("%s", op);

                if (binaryExpression->binaryOp == HLSLBinaryOp_MulAssign ||
                    binaryExpression->binaryOp == HLSLBinaryOp_DivAssign ||
                    IsArithmeticOp(binaryExpression->binaryOp) ||
                    IsLogicOp(binaryExpression->binaryOp))
                {
                    // Do intermediate type promotion, without changing dimension:
                    HLSLType promotedType = binaryExpression->expression2->expressionType;

                    if (!IsNumericTypeEqual(binaryExpression->expressionType.baseType, promotedType.baseType))
                    {
                        // This should only promote up (half->float, etc)
                        promotedType.baseType = PromoteType(binaryExpression->expressionType.baseType, promotedType.baseType);
                    }

                    OutputTypedExpression(promotedType, binaryExpression->expression2, binaryExpression);
                }
                else if (IsAssignOp(binaryExpression->binaryOp))
                {
                    OutputTypedExpression(binaryExpression->expressionType, binaryExpression->expression2, binaryExpression);
                }
                else
                {
                    OutputExpression(binaryExpression->expression2, binaryExpression);
                }
            }
            if (addParenthesis) m_writer.Write(")");
        }
        else if (expression->nodeType == HLSLNodeType_ConditionalExpression)
        {
            HLSLConditionalExpression* conditionalExpression = static_cast<HLSLConditionalExpression*>(expression);
            
            // TODO: @@ Remove parenthesis.
            m_writer.Write("((");
            OutputExpression(conditionalExpression->condition, NULL);
            m_writer.Write(")?(");
            OutputExpression(conditionalExpression->trueExpression, NULL);
            m_writer.Write("):(");
            OutputExpression(conditionalExpression->falseExpression, NULL);
            m_writer.Write("))");
        }
        else if (expression->nodeType == HLSLNodeType_MemberAccess)
        {
            HLSLMemberAccess* memberAccess = static_cast<HLSLMemberAccess*>(expression);
            bool addParenthesis = NeedsParenthesis(memberAccess->object, expression);

            if (addParenthesis)
            {
                m_writer.Write("(");
            }
            OutputExpression(memberAccess->object, NULL);
            if (addParenthesis)
            {
                m_writer.Write(")");
            }

            m_writer.Write(".%s", memberAccess->field);
        }
        else if (expression->nodeType == HLSLNodeType_ArrayAccess)
        {
            HLSLArrayAccess* arrayAccess = static_cast<HLSLArrayAccess*>(expression);
            
            // Just use the matrix notation, using column_order instead of row_order
            if (arrayAccess->array->expressionType.array) // || !IsMatrixType(arrayAccess->array->expressionType.baseType))
            {
                OutputExpression(arrayAccess->array, expression);
                m_writer.Write("[");
                OutputExpression(arrayAccess->index, NULL);
                m_writer.Write("]");
            }
//            else
//            {
//                // @@ This doesn't work for l-values!
//                m_writer.Write("column(");
//                OutputExpression(arrayAccess->array, NULL);
//                m_writer.Write(", ");
//                OutputExpression(arrayAccess->index, NULL);
//                m_writer.Write(")");
//            }
        }
        else if (expression->nodeType == HLSLNodeType_FunctionCall)
        {
            HLSLFunctionCall* functionCall = static_cast<HLSLFunctionCall*>(expression);
            OutputFunctionCall(functionCall, parentExpression);
        }
        else
        {
            m_writer.Write("<unknown expression>");
        }
    }

    void MSLGenerator::OutputCast(const HLSLType& type)
    {
        if (type.baseType == HLSLBaseType_Float3x3)
        {
            m_writer.Write("float3x3");
        }
        else
        {
            m_writer.Write("(");
            OutputDeclarationType(type, /*isConst=*/false, /*isRef=*/false, /*alignment=*/0, /*isTypeCast=*/true);
            m_writer.Write(")");
        }
    }

    // Called by the various Output functions
    void MSLGenerator::OutputArguments(HLSLArgument* argument)
    {
        int numArgs = 0;
        while (argument != NULL)
        {
            // Skip hidden and output arguments.
            if (argument->hidden || argument->modifier == HLSLArgumentModifier_Out)
            {
                argument = argument->nextArgument;
                continue;
            }

            if (numArgs > 0)
            {
                m_writer.Write(", ");
            }

            //bool isRef = false;
            bool isConst = false;
            /*if (argument->modifier == HLSLArgumentModifier_Out || argument->modifier == HLSLArgumentModifier_Inout)
            {
            isRef = true;
            }*/
            if (argument->modifier == HLSLArgumentModifier_In || argument->modifier == HLSLArgumentModifier_Const)
            {
                isConst = true;
            }

            OutputDeclaration(argument->type, argument->name, argument->defaultValue, /*isRef=*/false, isConst);
            argument = argument->nextArgument;
            ++numArgs;
        }
    }

    void MSLGenerator::OutputDeclaration(const HLSLType& type, const char* name, HLSLExpression* assignment, bool isRef, bool isConst, int alignment)
    {
        OutputDeclarationType(type, isRef, isConst, alignment);
        OutputDeclarationBody(type, name, assignment, isRef);
    }

    void MSLGenerator::OutputDeclarationType(const HLSLType& type, bool isRef, bool isConst, int alignment, bool isTypeCast)
    {
        const char* typeName = GetTypeName(type, /*exactType=*/isTypeCast);  // @@ Don't allow type changes in uniform/globals or casts!

        /*if (!isTypeCast)*/
        {
            if (isRef && !isTypeCast)
            {
                m_writer.Write("thread ");
            }
            if (isConst || type.flags & HLSLTypeFlag_Const)
            {
                m_writer.Write("const ");

                if ((type.flags & HLSLTypeFlag_Static) != 0 && !isTypeCast)
                {
                    m_writer.Write("static constant constexpr ");
                }
            }
        }
        
        if (alignment != 0 && !isTypeCast)
        {
            // caller can request alignment, but default is 0
            m_writer.Write("alignas(%d) ", alignment);
        }

        m_writer.Write("%s", typeName);

        if (isTypeCast)
        {
            // Do not output modifiers inside type cast expressions.
            return;
        }

        // Interpolation modifiers.
        if (type.flags & HLSLTypeFlag_NoInterpolation)
        {
            m_writer.Write(" [[flat]]");
        }
        else
        {
            if (type.flags & HLSLTypeFlag_NoPerspective)
            {
                if (type.flags & HLSLTypeFlag_Centroid)
                {
                    m_writer.Write(" [[centroid_no_perspective]]");
                }
                else if (type.flags & HLSLTypeFlag_Sample)
                {
                    m_writer.Write(" [[sample_no_perspective]]");
                }
                else
                {
                    m_writer.Write(" [[center_no_perspective]]");
                }
            }
            else
            {
                if (type.flags & HLSLTypeFlag_Centroid)
                {
                    m_writer.Write(" [[centroid_perspective]]");
                }
                else if (type.flags & HLSLTypeFlag_Sample)
                {
                    m_writer.Write(" [[sample_perspective]]");
                }
                else
                {
                    // Default.
                    //m_writer.Write(" [[center_perspective]]");
                }
            }
        }
    }

    void MSLGenerator::OutputDeclarationBody(const HLSLType& type, const char* name, HLSLExpression* assignment, bool isRef)
    {
        if (isRef)
        {
            // Arrays of refs are illegal in C++ and hence MSL, need to "link" the & to the var name
            m_writer.Write("(&");
        }

        // Then name
        m_writer.Write(" %s", name);

        if (isRef)
        {
            m_writer.Write(")");
        }

        // Add brackets for arrays
        if (type.array)
        {
            m_writer.Write("[");
            if (type.arraySize != NULL)
            {
                OutputExpression(type.arraySize, NULL);
            }
            m_writer.Write("]");
        }

        // Semantics and registers unhandled for now

        // Assignment handling
        if (assignment != NULL)
        {
            m_writer.Write(" = ");
            if (type.array)
            {
                m_writer.Write("{ ");
                OutputExpressionList(assignment);
                m_writer.Write(" }");
            }
            else
            {
                OutputTypedExpression(type, assignment, NULL);
            }
        }
    }

    void MSLGenerator::OutputExpressionList(HLSLExpression* expression)
    {
        int numExpressions = 0;
        while (expression != NULL)
        {
            if (numExpressions > 0)
            {
                m_writer.Write(", ");
            }
            OutputExpression(expression, NULL);
            expression = expression->nextExpression;
            ++numExpressions;
        }
    }

    // Cast all expressions to given type.
    void MSLGenerator::OutputExpressionList(const HLSLType & type, HLSLExpression* expression)
    {
        int numExpressions = 0;
        while (expression != NULL)
        {
            if (numExpressions > 0)
            {
                m_writer.Write(", ");
            }

            OutputTypedExpression(type, expression, NULL);
            expression = expression->nextExpression;
            ++numExpressions;
        }
    }

    // Cast each expression to corresponding argument type.
    void MSLGenerator::OutputExpressionList(HLSLArgument* argument, HLSLExpression* expression)
    {
        int numExpressions = 0;
        while (expression != NULL)
        {
            ASSERT(argument != NULL);
            if (argument->modifier != HLSLArgumentModifier_Out)
            {
                if (numExpressions > 0)
                {
                    m_writer.Write(", ");
                }

                OutputTypedExpression(argument->type, expression, NULL);
                ++numExpressions;
            }

            expression = expression->nextExpression;
            argument = argument->nextArgument;
        }
    }



    inline bool isAddressable(HLSLExpression* expression)
    {
        if (expression->nodeType == HLSLNodeType_IdentifierExpression)
        {
            return true;
        }
        if (expression->nodeType == HLSLNodeType_ArrayAccess)
        {
            return true;
        }
        if (expression->nodeType == HLSLNodeType_MemberAccess)
        {
            HLSLMemberAccess* memberAccess = (HLSLMemberAccess*)expression;
            return !memberAccess->swizzle;
        }
        return false;
    }


    void MSLGenerator::OutputFunctionCallStatement(int indent, HLSLFunctionCall* functionCall, HLSLDeclaration* declaration)
    {
        // Nothing special about these cases:
        if (functionCall->function->numOutputArguments == 0)
        {
            m_writer.BeginLine(indent, functionCall->fileName, functionCall->line);
            if (declaration)
            {
                OutputDeclaration(declaration);
            }
            else
            {
                OutputExpression(functionCall, NULL);
            }
            m_writer.EndLine(";");
            return;
        }


        // Transform this:
        // float foo = functionCall(bah, poo);

        // Into:
        // auto tmp = functionCall(bah, poo);
        // bah = tmp.bah;
        // poo = tmp.poo;
        // float foo = tmp.__result;

        const char* functionName = functionCall->function->name;

        m_writer.BeginLine(indent, functionCall->fileName, functionCall->line);
        m_writer.Write("auto out%d = %s_%d(", functionCall->line, functionName, functionCall->function->line);
        OutputExpressionList(functionCall->function->argument, functionCall->argument);
        m_writer.EndLine(");");

        HLSLExpression * expression = functionCall->argument;
        HLSLArgument * argument = functionCall->function->argument;
        while (argument != NULL)
        {
            if (argument->modifier == HLSLArgumentModifier_Out || argument->modifier == HLSLArgumentModifier_Inout)
            {
                m_writer.BeginLine(indent);
                OutputExpression(expression, NULL);
                // @@ This assignment may need a cast.
                m_writer.Write(" = ");
                if (NeedsCast(expression->expressionType, argument->type)) {
                    m_writer.Write("(%s)", GetTypeName(expression->expressionType, true));
                }
                m_writer.Write("out%d.%s;", functionCall->line, argument->name);
                m_writer.EndLine();
            }

            expression = expression->nextExpression;
            argument = argument->nextArgument;
        }

        if (declaration)
        {
            m_writer.BeginLine(indent);
            OutputDeclarationType(declaration->type);
            m_writer.Write(" %s = out%d.__result;", declaration->name, functionCall->line);
            m_writer.EndLine();
        }


/* TODO: Alec, why is all this chopped out?
 
        int argumentIndex = 0;
        HLSLArgument* argument = functionCall->function->argument;
        HLSLExpression* expression = functionCall->argument;
        while (argument != NULL)
        {
            if (!isAddressable(expression))
            {
                if (argument->modifier == HLSLArgumentModifier_Out)
                {
                    m_writer.BeginLine(indent, functionCall->fileName, functionCall->line);
                    OutputDeclarationType(argument->type);
                    m_writer.Write("tmp%d;", argumentIndex);
                    m_writer.EndLine();
                }
                else if (argument->modifier == HLSLArgumentModifier_Inout)
                {
                    m_writer.BeginLine(indent, functionCall->fileName, functionCall->line);
                    OutputDeclarationType(argument->type);
                    m_writer.Write("tmp%d = ", argumentIndex);
                    OutputExpression(expression, NULL);
                    m_writer.EndLine(";");
                }
            }
            argument = argument->nextArgument;
            expression = expression->nextExpression;
            argumentIndex++;
        }

        m_writer.BeginLine(indent, functionCall->fileName, functionCall->line);
        const char* name = functionCall->function->name;
        m_writer.Write("%s(", name);
        //OutputExpressionList(functionCall->argument);

        // Output expression list with temporary substitution.
        argumentIndex = 0;
        argument = functionCall->function->argument;
        expression = functionCall->argument;
        while (expression != NULL)
        {
            if (!isAddressable(expression) && (argument->modifier == HLSLArgumentModifier_Out || argument->modifier == HLSLArgumentModifier_Inout))
            {
                m_writer.Write("tmp%d", argumentIndex);
            }
            else
            {
                OutputExpression(expression, NULL);
            }

            argument = argument->nextArgument;
            expression = expression->nextExpression;
            argumentIndex++;
            if (expression)
            {
                m_writer.Write(", ");
            }
        }
        m_writer.EndLine(");");

        argumentIndex = 0;
        argument = functionCall->function->argument;
        expression = functionCall->argument;
        while (expression != NULL)
        {
            if (!isAddressable(expression) && (argument->modifier == HLSLArgumentModifier_Out || argument->modifier == HLSLArgumentModifier_Inout))
            {
                m_writer.BeginLine(indent, functionCall->fileName, functionCall->line);
                OutputExpression(expression, NULL);
                m_writer.Write(" = tmp%d", argumentIndex);
                m_writer.EndLine(";");
            }

            argument = argument->nextArgument;
            expression = expression->nextExpression;
            argumentIndex++;
        }
*/
    }

    void MSLGenerator::OutputFunctionCall(HLSLFunctionCall* functionCall, HLSLExpression * parentExpression)
    {
        if (functionCall->function->numOutputArguments > 0)
        {
            ASSERT(false);
        }

        const char* functionName = functionCall->function->name;

        // If function begins with tex, then it returns float4 or half4 depending on options.halfTextureSamplers
        /*if (strncmp(functionName, "tex", 3) == 0)
        {
        if (parentExpression && IsFloat(parentExpression->expressionType.baseType))
        {
        if (m_options.halfTextureSamplers)
        {
        OutputCast(functionCall->expressionType);
        }
        }
        }*/

        {
            m_writer.Write("%s(", functionName);
            OutputExpressionList(functionCall->function->argument, functionCall->argument);
            //OutputExpressionList(functionCall->argument);
            m_writer.Write(")");
        }
    }

    const char* MSLGenerator::TranslateInputSemantic(const char * semantic)
    {
        if (semantic == NULL)
            return NULL;

        unsigned int length, index;
        ParseSemantic(semantic, &length, &index);

        if (m_target == MSLGenerator::Target_VertexShader)
        {
            // These are DX10 convention
            if (String_Equal(semantic, "SV_InstanceID"))
                return "instance_id";
            if (String_Equal(semantic, "SV_VertexID"))
                return "vertex_id";

            // requires SPV_KHR_shader_draw_parameters for Vulkan
            // not a DX12 construct.
            if (String_Equal(semantic, "BASEVERTEX"))
                return "base_vertex";
            if (String_Equal(semantic, "BASEINSTANCE"))
                return "base_instance";
            //if (String_Equal(semantic, "DRAW_INDEX"))
            //    return "draw_index";
            
            // TODO: primitive_id, barycentric
            
            // Handle attributes
            
            // Can set custom attributes via a callback
            if (m_options.attributeCallback)
            {
                char name[64];
                ASSERT(length < sizeof(name));

                strncpy(name, semantic, length);
                name[length] = 0;

                int attribute = m_options.attributeCallback(name, index);

                if (attribute >= 0)
                {
                    return m_tree->AddStringFormat("attribute(%d)", attribute);
                }
            }
            
            if (String_Equal(semantic, "SV_Position"))
                return "attribute(POSITION)";

            return m_tree->AddStringFormat("attribute(%s)", semantic);
        }
        else if (m_target == MSLGenerator::Target_FragmentShader)
        {
            // PS inputs
            
            if (String_Equal(semantic, "SV_Position"))
                return "position";
            
            //  if (String_Equal(semantic, "POSITION"))
            //    return "position";
            if (String_Equal(semantic, "SV_IsFrontFace"))
                return "front_facing";
            
            // VS sets what layer to render into, ps can look at it.
            // Gpu Family 5.
            if (String_Equal(semantic, "SV_RenderTargetArrayIndex"))
                return "render_target_array_index";
            
            // dual source? passes in underlying color
            if (String_Equal(semantic, "DST_COLOR"))
                return "color(0)";
            
            if (String_Equal(semantic, "SV_SampleIndex"))
                return "sample_id";
            //if (String_Equal(semantic, "SV_Coverage")) return "sample_mask";
            //if (String_Equal(semantic, "SV_Coverage")) return "sample_mask,post_depth_coverage";
        }

        return NULL;
    }

    const char* MSLGenerator::TranslateOutputSemantic(const char * semantic)
    {
        if (semantic == NULL)
            return NULL;

        unsigned int length, index;
        ParseSemantic(semantic, &length, &index);

        if (m_target == MSLGenerator::Target_VertexShader)
        {
            if (String_Equal(semantic, "SV_Position"))
                return "position";
            
            // PSIZE is non-square in DX9, and square in DX10 (and MSL)
            // https://github.com/KhronosGroup/glslang/issues/1154
            if (String_Equal(semantic, "PSIZE"))
                return "point_size";

            // control layer in Gpu Family 5
            if (String_Equal(semantic, "SV_RenderTargetArrayIndex"))
                return "render_target_array_index";
            
            // TODO: add
            // SV_ViewportArrayIndex
            // SV_ClipDistance0..n, SV_CullDistance0..n
        }
        else if (m_target == MSLGenerator::Target_FragmentShader)
        {
            if (m_options.flags & MSLGenerator::Flag_NoIndexAttribute)
            {
                // No dual-source blending on iOS, and no index() attribute
                if (String_Equal(semantic, "COLOR0_1")) return NULL;
            }
            else
            {
                // See these settings
                // MTLBlendFactorSource1Color, OneMinusSource1Color, Source1Alpha, OneMinuSource1Alpha.
                
                // @@ IC: Hardcoded for this specific case, extend ParseSemantic?
                if (String_Equal(semantic, "COLOR0_1"))
                    return "color(0), index(1)";
            }

            if (strncmp(semantic, "SV_Target", length) == 0)
            {
                return m_tree->AddStringFormat("color(%d)", index);
            }
//            if (strncmp(semantic, "COLOR", length) == 0)
//            {
//                return m_tree->AddStringFormat("color(%d)", index);
//            }

            // depth variants to preserve earlyz, use greater on reverseZ
            if (String_Equal(semantic, "SV_Depth"))
                return "depth(any)";
            
            // These don't quite line up, since comparison is not ==
            // Metal can only use any/less/greater.  Preserve early z when outputting depth.
            // reverseZ would use greater.
            if (String_Equal(semantic, "SV_DepthGreaterEqual"))
                return "depth(greater)";
            if (String_Equal(semantic, "SV_DepthLessEqual"))
                return "depth(less)";
            
            if (String_Equal(semantic, "SV_Coverage"))
                return "sample_mask";
        }

        return NULL;
    }


    const char* MSLGenerator::GetTypeName(const HLSLType& type, bool exactType)
    {
        bool promote = ((type.flags & HLSLTypeFlag_NoPromote) == 0);

        // number
        bool isHalfNumerics = promote && !m_options.treatHalfAsFloat;
        auto baseType = type.baseType;
        if (!isHalfNumerics)
            baseType = HalfToFloatBaseType(baseType);
        
        const char* name = GetNumericTypeName(baseType);
        if (name)
            return name;
        
        // struct
        if (baseType == HLSLBaseType_UserDefined)
            return type.typeName;
        
        // sampler
        if (IsSamplerType(baseType))
            return "sampler";
        
        // texture
        if (IsTextureType(baseType))
        {
            // TODO: hook isDepth up to texture flag
            // unclear if depth supports half, may have to be float always
            bool isDepth = false;
            
            bool isHalfTexture  = promote && type.textureType == HLSLBaseType_Half && !m_options.treatHalfAsFloat;
            
            switch (baseType)
            {
                case HLSLBaseType_Texture2D:
                    if (isDepth)
                        return isHalfTexture ? "depth2d<half>" : "depth2d<float>";
                    
                    return isHalfTexture ? "texture2d<half>" : "texture2d<float>";
                case HLSLBaseType_Texture3D:
                    // no depth equivalent for 3d
                    //if (isDepth)
                    //    return isHalfTexture ? "depth2d<half>" : "depth2d<float>";
                    
                    return isHalfTexture ? "texture3d<half>" : "texture3d<float>";
                case HLSLBaseType_TextureCube:
                    if (isDepth)
                        return isHalfTexture ? "depthcube<half>" : "depthcube<float>";
                    
                    return isHalfTexture ? "texturecube<half>" : "texturecube<float>";
                    
                    // TODO: no equivalent of this yet
                    // depth_ms_array
                    
                case HLSLBaseType_Texture2DMS:
                    if (isDepth)
                        return isHalfTexture ? "depth2d_ms<half>" : "depth2d_ms<float>";
                    
                    return isHalfTexture ? "texture2d_ms<half>" : "texture2d_ms<float>";
                case HLSLBaseType_TextureCubeArray:
                    if (isDepth)
                        return isHalfTexture ? "depthcube_array<half>" : "depthcube_array<float>";
                    
                    return isHalfTexture ? "texturecube_array<half>" : "texturecube_array<float>";
                case HLSLBaseType_Texture2DArray:
                    if (isDepth)
                        return isHalfTexture ? "depth2d_array<half>" : "depth2d_array<float>";
                    
                    return isHalfTexture ? "texture2d_array<half>" : "texture2d_array<float>";
                    
                default:
                    break;
            }
        }
        
        Error("Uknown Type");
        return NULL;
    }



} // M4
