//=============================================================================
//
// Render/HLSLGenerator.cpp
//
// Created by Max McGuire (max@unknownworlds.com)
// Copyright (c) 2013, Unknown Worlds Entertainment, Inc.
//
//=============================================================================

#include "HLSLGenerator.h"

#include "Engine.h"
#include "HLSLParser.h"
#include "HLSLTree.h"

namespace M4
{

static const char* GetTypeName(const HLSLType& type)
{
    const char* name = GetNumericTypeName(type.baseType);
    if (name)
        return name;
    
    switch (type.baseType)
    {
    case HLSLBaseType_Texture:      return "texture";
            
    case HLSLBaseType_Sampler:      return "sampler";
    case HLSLBaseType_Sampler2D:    return "sampler2D";
    case HLSLBaseType_Sampler3D:    return "sampler3D";
    case HLSLBaseType_SamplerCube:  return "samplerCUBE";
    case HLSLBaseType_Sampler2DShadow:  return "sampler2DShadow";
    case HLSLBaseType_Sampler2DMS:  return "sampler2DMS";
    case HLSLBaseType_Sampler2DArray:    return "sampler2DArray";
    case HLSLBaseType_UserDefined:  return type.typeName;
    default: return "<unknown type>";
    }
}

// TODO: copied from MSLGenerator
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

/* unused
static int GetFunctionArguments(HLSLFunctionCall* functionCall, HLSLExpression* expression[], int maxArguments)
{
    HLSLExpression* argument = functionCall->argument;
    int numArguments = 0;
    while (argument != NULL)
    {
        if (numArguments < maxArguments)
        {
            expression[numArguments] = argument;
        }
        argument = argument->nextExpression;
        ++numArguments;
    }
    return numArguments;
}
*/

HLSLGenerator::HLSLGenerator()
{
    m_tree                          = NULL;
    m_entryName                     = NULL;
    m_target                        = Target_VertexShader;
    m_isInsideBuffer                = false;
    m_error                         = false;
}


// @@ We need a better way of doing semantic replacement:
// - Look at the function being generated.
// - Return semantic, semantics associated to fields of the return structure, or output arguments, or fields of structures associated to output arguments -> output semantic replacement.
// - Semantics associated input arguments or fields of the input arguments -> input semantic replacement.
static const char * TranslateSemantic(const char* semantic, bool output, HLSLGenerator::Target target)
{
    if (target == HLSLGenerator::Target_VertexShader)
    {
        if (output) 
        {
            if (String_Equal("POSITION", semantic))     return "SV_Position";
        }
        else {
            if (String_Equal("INSTANCE_ID", semantic))  return "SV_InstanceID";
        }
    }
    else if (target == HLSLGenerator::Target_PixelShader)
    {
        if (output)
        {
            if (String_Equal("DEPTH", semantic))      return "SV_Depth";
            if (String_Equal("COLOR", semantic))      return "SV_Target";
            if (String_Equal("COLOR0", semantic))     return "SV_Target0";
            if (String_Equal("COLOR0_1", semantic))   return "SV_Target1";
            if (String_Equal("COLOR1", semantic))     return "SV_Target1";
            if (String_Equal("COLOR2", semantic))     return "SV_Target2";
            if (String_Equal("COLOR3", semantic))     return "SV_Target3";
        }
        else
        {
            if (String_Equal("VPOS", semantic))       return "SV_Position";
            if (String_Equal("VFACE", semantic))      return "SV_IsFrontFace";    // bool   @@ Should we do type replacement too?
        }
    }
    return NULL;
}

void HLSLGenerator::Error(const char* format, ...)
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

bool HLSLGenerator::Generate(HLSLTree* tree, Target target, const char* entryName)
{
    m_tree      = tree;
    m_entryName = entryName;
    m_target    = target;
    m_isInsideBuffer = false;

    m_writer.Reset();

    // Find entry point function
    HLSLFunction* entryFunction = tree->FindFunction(entryName);
    if (entryFunction == NULL)
    {
        Error("Entry point '%s' doesn't exist\n", entryName);
        return false;
    }

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
    
    m_writer.WriteLine(0, "#include \"ShaderHLSL.h\"");
    
    // @@ Should we generate an entirely new copy of the tree so that we can modify it in place?
    //if (!legacy)
    {
        HLSLFunction * function = tree->FindFunction(entryName);

        // Handle return value semantics
        if (function->semantic != NULL) {
            function->sv_semantic = TranslateSemantic(function->semantic, /*output=*/true, target);
        }
        if (function->returnType.baseType == HLSLBaseType_UserDefined) {
            HLSLStruct * s = tree->FindGlobalStruct(function->returnType.typeName);

			HLSLStructField * sv_fields = NULL;

			HLSLStructField * lastField = NULL;
            HLSLStructField * field = s->field;
            while (field) {
				HLSLStructField * nextField = field->nextField;

                if (field->semantic) {
					field->hidden = false;
                    field->sv_semantic = TranslateSemantic(field->semantic, /*output=*/true, target);

					// Fields with SV semantics are stored at the end to avoid linkage problems.
					if (field->sv_semantic != NULL) {
						// Unlink from last.
						if (lastField != NULL) lastField->nextField = nextField;
						else s->field = nextField;

						// Add to sv_fields.
						field->nextField = sv_fields;
						sv_fields = field;
					}
                }

				if (field != sv_fields) lastField = field;
                field = nextField;
            }

			// Append SV fields at the end.
			if (sv_fields != NULL) {
				if (lastField == NULL) {
					s->field = sv_fields;
				}
				else {
					ASSERT(lastField->nextField == NULL);
					lastField->nextField = sv_fields;
				}
			}
        }

        // Handle argument semantics.
        // @@ It would be nice to flag arguments that are used by the program and skip or hide the unused ones.
        HLSLArgument * argument = function->argument;
        while (argument) {
            bool output = argument->modifier == HLSLArgumentModifier_Out;
            if (argument->semantic) {
                argument->sv_semantic = TranslateSemantic(argument->semantic, output, target); 
            }

            if (argument->type.baseType == HLSLBaseType_UserDefined) {
                HLSLStruct * s = tree->FindGlobalStruct(argument->type.typeName);

                HLSLStructField * field = s->field;
                while (field) {
                    if (field->semantic) {
						field->hidden = false;

						if (target == Target_PixelShader && !output && String_EqualNoCase(field->semantic, "POSITION")) {
                            // this is firing on NULL
							//ASSERT(String_EqualNoCase(field->sv_semantic, "SV_Position"));
							field->hidden = true;
						}

                        field->sv_semantic = TranslateSemantic(field->semantic, output, target);
                    }

                    field = field->nextField;
                }
            }

            argument = argument->nextArgument;
        }
    }
    
    HLSLRoot* root = m_tree->GetRoot();
    OutputStatements(0, root->statement);

    m_tree = NULL;
    return true;
}

const char* HLSLGenerator::GetResult() const
{
    return m_writer.GetResult();
}

void HLSLGenerator::OutputExpressionList(HLSLExpression* expression)
{
    int numExpressions = 0;
    while (expression != NULL)
    {
        if (numExpressions > 0)
        {
            m_writer.Write(", ");
        }
        OutputExpression(expression);
        expression = expression->nextExpression;
        ++numExpressions;
    }
}



void HLSLGenerator::OutputExpression(HLSLExpression* expression)
{
    if (expression->nodeType == HLSLNodeType_IdentifierExpression)
    {
        HLSLIdentifierExpression* identifierExpression = static_cast<HLSLIdentifierExpression*>(expression);
        const char* name = identifierExpression->name;
        if (IsSamplerType(identifierExpression->expressionType) && identifierExpression->global)
        {
            // @@ Handle generic sampler type.

            // Stupid HLSL doesn't have ctors, so have ctor calls.
            if (identifierExpression->expressionType.baseType == HLSLBaseType_Sampler2D)
            {
                if (identifierExpression->expressionType.samplerType == HLSLBaseType_Half) // TODO: && !m_options.treatHalfAsFloat)
                {
                    m_writer.Write("Texture2DHalfSamplerCtor(%s_texture, %s_sampler)", name, name);
                }
                else
                {
                    m_writer.Write("Texture2DSamplerCtor(%s_texture, %s_sampler)", name, name);
                }
                
                // Remove this, so have half support above
                //m_writer.Write("%s(%s_texture, %s_sampler)", "Texture2DSamplerCtor", name, name);
            }
            else if (identifierExpression->expressionType.baseType == HLSLBaseType_Sampler3D)
            {
                m_writer.Write("%s(%s_texture, %s_sampler)", "Texture3DSamplerCtor", name, name);
            }
            else if (identifierExpression->expressionType.baseType == HLSLBaseType_SamplerCube)
            {
                m_writer.Write("%s(%s_texture, %s_sampler)", "TextureCubeSamplerCtor", name, name);
            }
            else if (identifierExpression->expressionType.baseType == HLSLBaseType_Sampler2DShadow)
            {
                m_writer.Write("%s(%s_texture, %s_sampler)", "Texture2DShadowSamplerCtor", name, name);
            }
            // TODO: add all ctor types
            else if (identifierExpression->expressionType.baseType == HLSLBaseType_Sampler2DMS)
            {
                m_writer.Write("%s", name);
            }
        }
        else
        {
            m_writer.Write("%s", name);
        }
    }
    else if (expression->nodeType == HLSLNodeType_CastingExpression)
    {
        HLSLCastingExpression* castingExpression = static_cast<HLSLCastingExpression*>(expression);
        m_writer.Write("(");
        OutputDeclaration(castingExpression->type, "");
        m_writer.Write(")(");
        OutputExpression(castingExpression->expression);
        m_writer.Write(")");
    }
    else if (expression->nodeType == HLSLNodeType_ConstructorExpression)
    {
        HLSLConstructorExpression* constructorExpression = static_cast<HLSLConstructorExpression*>(expression);
        m_writer.Write("%s(", GetTypeName(constructorExpression->type));
        OutputExpressionList(constructorExpression->argument);
        m_writer.Write(")");
    }
    else if (expression->nodeType == HLSLNodeType_LiteralExpression)
    {
        HLSLLiteralExpression* literalExpression = static_cast<HLSLLiteralExpression*>(expression);
        switch (literalExpression->type)
        {
        case HLSLBaseType_Half:
        case HLSLBaseType_Float:
            {
                // Don't use printf directly so that we don't use the system locale.
                char buffer[64];
                String_FormatFloat(buffer, sizeof(buffer), literalExpression->fValue);
                String_StripTrailingFloatZeroes(buffer);
                m_writer.Write("%s", buffer);
            }
            break;        
        case HLSLBaseType_Int:
            m_writer.Write("%d", literalExpression->iValue);
            break;
        case HLSLBaseType_Bool:
            m_writer.Write("%s", literalExpression->bValue ? "true" : "false");
            break;
        default:
            ASSERT(0);
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
        case HLSLUnaryOp_PreIncrement:  op = "++"; break;
        case HLSLUnaryOp_PreDecrement:  op = "--"; break;
        case HLSLUnaryOp_PostIncrement: op = "++"; pre = false; break;
        case HLSLUnaryOp_PostDecrement: op = "--"; pre = false; break;
        case HLSLUnaryOp_BitNot:        op = "~";  break;
        }
        
        // eliminate () if pure characters
        bool addParenthesis = NeedsParenthesis(unaryExpression->expression, expression);
        if (addParenthesis) m_writer.Write("(");
        
        if (pre)
        {
            m_writer.Write("%s", op);
            OutputExpression(unaryExpression->expression);
        }
        else
        {
            OutputExpression(unaryExpression->expression);
            m_writer.Write("%s", op);
        }
        if (addParenthesis) m_writer.Write(")");
    }
    else if (expression->nodeType == HLSLNodeType_BinaryExpression)
    {
        HLSLBinaryExpression* binaryExpression = static_cast<HLSLBinaryExpression*>(expression);
        
        // TODO: to fix this need to pass in parentExpression to
        // the call.  And MSLGenerator passes NULL for most of these.
        // TODO: eliminate () if pure characters
        
        bool addParenthesis = false; // NeedsParenthesis(expression, parentExpression);
        if (addParenthesis) m_writer.Write("(");
        
        OutputExpression(binaryExpression->expression1);
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
            ASSERT(0);
        }
        m_writer.Write("%s", op);
        OutputExpression(binaryExpression->expression2);
        if (addParenthesis) m_writer.Write(")");
    }
    else if (expression->nodeType == HLSLNodeType_ConditionalExpression)
    {
        HLSLConditionalExpression* conditionalExpression = static_cast<HLSLConditionalExpression*>(expression);
        
        // TODO: eliminate () if pure characters
        m_writer.Write("((");
        OutputExpression(conditionalExpression->condition);
        m_writer.Write(")?(");
        OutputExpression(conditionalExpression->trueExpression);
        m_writer.Write("):(");
        OutputExpression(conditionalExpression->falseExpression);
        m_writer.Write("))");
    }
    else if (expression->nodeType == HLSLNodeType_MemberAccess)
    {
        HLSLMemberAccess* memberAccess = static_cast<HLSLMemberAccess*>(expression);
        
        bool addParenthesis = NeedsParenthesis(memberAccess->object, expression);
        
        // eliminate () if pure characters
        if ( addParenthesis ) m_writer.Write("(");
        OutputExpression(memberAccess->object);
        if ( addParenthesis ) m_writer.Write(")");
        m_writer.Write(".%s", memberAccess->field);
    }
    else if (expression->nodeType == HLSLNodeType_ArrayAccess)
    {
        HLSLArrayAccess* arrayAccess = static_cast<HLSLArrayAccess*>(expression);
        OutputExpression(arrayAccess->array);
        m_writer.Write("[");
        OutputExpression(arrayAccess->index);
        m_writer.Write("]");
    }
    else if (expression->nodeType == HLSLNodeType_FunctionCall)
    {
        HLSLFunctionCall* functionCall = static_cast<HLSLFunctionCall*>(expression);
        const char* name = functionCall->function->name;
        m_writer.Write("%s(", name);
        OutputExpressionList(functionCall->argument);
        m_writer.Write(")");
    }
    else
    {
        m_writer.Write("<unknown expression>");
    }
}

void HLSLGenerator::OutputArguments(HLSLArgument* argument)
{
    int numArgs = 0;
    while (argument != NULL)
    {
        if (numArgs > 0)
        {
            m_writer.Write(", ");
        }

        switch (argument->modifier)
        {
        case HLSLArgumentModifier_In:
            m_writer.Write("in ");
            break;
        case HLSLArgumentModifier_Out:
            m_writer.Write("out ");
            break;
        case HLSLArgumentModifier_Inout:
            m_writer.Write("inout ");
            break;
        case HLSLArgumentModifier_Uniform:
            m_writer.Write("uniform ");
            break;
        default:
            break;
        }

        const char * semantic = argument->sv_semantic ? argument->sv_semantic : argument->semantic;

        OutputDeclaration(argument->type, argument->name, semantic, /*registerName=*/NULL, argument->defaultValue);
        argument = argument->nextArgument;
        ++numArgs;
    }
}

static const char * GetAttributeName(HLSLAttributeType attributeType)
{
    if (attributeType == HLSLAttributeType_Unroll) return "unroll";
    if (attributeType == HLSLAttributeType_Branch) return "branch";
    if (attributeType == HLSLAttributeType_Flatten) return "flatten";
    return NULL;
}

void HLSLGenerator::OutputAttributes(int indent, HLSLAttribute* attribute)
{
    while (attribute != NULL)
    {
        const char * attributeName = GetAttributeName(attribute->attributeType);
    
        if (attributeName != NULL)
        {
            m_writer.WriteLineTagged(indent, attribute->fileName, attribute->line, "[%s]", attributeName);
        }

        attribute = attribute->nextAttribute;
    }
}

void HLSLGenerator::OutputStatements(int indent, HLSLStatement* statement)
{
    while (statement != NULL)
    {
        if (statement->hidden) 
        {
            statement = statement->nextStatement;
            continue;
        }

        OutputAttributes(indent, statement->attributes);

        if (statement->nodeType == HLSLNodeType_Declaration)
        {
            HLSLDeclaration* declaration = static_cast<HLSLDeclaration*>(statement);
            m_writer.BeginLine(indent, declaration->fileName, declaration->line);
            OutputDeclaration(declaration);
            m_writer.EndLine(";");
        }
        else if (statement->nodeType == HLSLNodeType_Struct)
        {
            HLSLStruct* structure = static_cast<HLSLStruct*>(statement);
            m_writer.WriteLineTagged(indent, structure->fileName, structure->line, "struct %s {", structure->name);
            HLSLStructField* field = structure->field;
            while (field != NULL)
            {
                if (!field->hidden)
                {
                    m_writer.BeginLine(indent + 1, field->fileName, field->line);
                    const char * semantic = field->sv_semantic ? field->sv_semantic : field->semantic;
                    OutputDeclaration(field->type, field->name, semantic);
                    m_writer.Write(";");
                    m_writer.EndLine();
                }
                field = field->nextField;
            }
            m_writer.WriteLine(indent, "};");
        }
        else if (statement->nodeType == HLSLNodeType_Buffer)
        {
            HLSLBuffer* buffer = static_cast<HLSLBuffer*>(statement);
            HLSLDeclaration* field = buffer->field;

            m_writer.BeginLine(indent, buffer->fileName, buffer->line);
            m_writer.Write("cbuffer %s", buffer->name);
            if (buffer->registerName != NULL)
            {
                m_writer.Write(" : register(%s)", buffer->registerName);
            }
            m_writer.EndLine(" {");
            

            m_isInsideBuffer = true;

            while (field != NULL)
            {
                if (!field->hidden)
                {
                    m_writer.BeginLine(indent + 1, field->fileName, field->line);
                    OutputDeclaration(field->type, field->name, /*semantic=*/NULL, /*registerName*/field->registerName, field->assignment);
                    m_writer.Write(";");
                    m_writer.EndLine();
                }
                field = (HLSLDeclaration*)field->nextStatement;
            }

            m_isInsideBuffer = false;

            m_writer.WriteLine(indent, "};");
        }
        else if (statement->nodeType == HLSLNodeType_Function)
        {
            HLSLFunction* function = static_cast<HLSLFunction*>(statement);

            // Use an alternate name for the function which is supposed to be entry point
            // so that we can supply our own function which will be the actual entry point.
            const char* functionName   = function->name;
            const char* returnTypeName = GetTypeName(function->returnType);

            m_writer.BeginLine(indent, function->fileName, function->line);
            m_writer.Write("%s %s(", returnTypeName, functionName);

            OutputArguments(function->argument);

            const char * semantic = function->sv_semantic ? function->sv_semantic : function->semantic;
            if (semantic != NULL)
            {
                m_writer.Write(") : %s {", semantic);
            }
            else
            {
                m_writer.Write(") {");
            }

            m_writer.EndLine();

            OutputStatements(indent + 1, function->statement);
            m_writer.WriteLine(indent, "};");
        }
        else if (statement->nodeType == HLSLNodeType_ExpressionStatement)
        {
            HLSLExpressionStatement* expressionStatement = static_cast<HLSLExpressionStatement*>(statement);
            m_writer.BeginLine(indent, statement->fileName, statement->line);
            OutputExpression(expressionStatement->expression);
            m_writer.EndLine(";");
        }
        else if (statement->nodeType == HLSLNodeType_ReturnStatement)
        {
            HLSLReturnStatement* returnStatement = static_cast<HLSLReturnStatement*>(statement);
            if (returnStatement->expression != NULL)
            {
                m_writer.BeginLine(indent, returnStatement->fileName, returnStatement->line);
                m_writer.Write("return ");
                OutputExpression(returnStatement->expression);
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
            m_writer.WriteLineTagged(indent, discardStatement->fileName, discardStatement->line, "discard;");
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
            m_writer.BeginLine(indent, ifStatement->fileName, ifStatement->line);
            m_writer.Write("if (");
            OutputExpression(ifStatement->condition);
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
        else if (statement->nodeType == HLSLNodeType_ForStatement)
        {
            HLSLForStatement* forStatement = static_cast<HLSLForStatement*>(statement);
            m_writer.BeginLine(indent, forStatement->fileName, forStatement->line);
            m_writer.Write("for (");
            OutputDeclaration(forStatement->initialization);
            m_writer.Write("; ");
            OutputExpression(forStatement->condition);
            m_writer.Write("; ");
            OutputExpression(forStatement->increment);
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
            // Unhanded statement type.
            ASSERT(0);
        }

        statement = statement->nextStatement;
    }
}

void HLSLGenerator::OutputDeclaration(HLSLDeclaration* declaration)
{
    bool isSamplerType = IsSamplerType(declaration->type);

    if (isSamplerType)
    {
        int reg = -1;
        if (declaration->registerName != NULL)
        {
            sscanf(declaration->registerName, "s%d", &reg);
        }

        const char* textureType = NULL;
        const char* samplerType = "SamplerState";
        // @@ Handle generic sampler type.

        if (declaration->type.baseType == HLSLBaseType_Sampler2D)
        {
            textureType = "Texture2D";
        }
        else if (declaration->type.baseType == HLSLBaseType_Sampler3D)
        {
            textureType = "Texture3D";
        }
        else if (declaration->type.baseType == HLSLBaseType_SamplerCube)
        {
            textureType = "TextureCube";
        }
        else if (declaration->type.baseType == HLSLBaseType_Sampler2DShadow)
        {
            textureType = "Texture2D";
            samplerType = "SamplerComparisonState";
        }
        else if (declaration->type.baseType == HLSLBaseType_Sampler2DMS)
        {
            textureType = "Texture2DMS<float4>";  // @@ Is template argument required?
            samplerType = NULL;
        }

        if (samplerType != NULL)
        {
            if (reg != -1)
            {
                m_writer.Write("%s %s_texture : register(t%d); %s %s_sampler : register(s%d)", textureType, declaration->name, reg, samplerType, declaration->name, reg);
            }
            else
            {
                m_writer.Write("%s %s_texture; %s %s_sampler", textureType, declaration->name, samplerType, declaration->name);
            }
        }
        else
        {
            if (reg != -1)
            {
                m_writer.Write("%s %s : register(t%d)", textureType, declaration->name, reg);
            }
            else
            {
                m_writer.Write("%s %s", textureType, declaration->name);
            }
        }
        return;
    }

    OutputDeclarationType(declaration->type);
    OutputDeclarationBody(declaration->type, declaration->name, declaration->semantic, declaration->registerName, declaration->assignment);
    declaration = declaration->nextDeclaration;

    while(declaration != NULL) {
        m_writer.Write(", ");
        OutputDeclarationBody(declaration->type, declaration->name, declaration->semantic, declaration->registerName, declaration->assignment);
        declaration = declaration->nextDeclaration;
    };
}

void HLSLGenerator::OutputDeclarationType(const HLSLType& type)
{
    const char* typeName = GetTypeName(type);
    
    if (type.baseType == HLSLBaseType_Sampler2D)
    {
        if (type.samplerType == HLSLBaseType_Half
            // TODO: && !m_options.treatHalfAsFloat
            ) {
            typeName = "Texture2DHalfSampler";
        }
        else {
            typeName = "Texture2DSampler";
        }
    }
    else if (type.baseType == HLSLBaseType_Sampler3D)
    {
        typeName = "Texture3DSampler";
    }
    else if (type.baseType == HLSLBaseType_SamplerCube)
    {
        typeName =  "TextureCubeSampler";
    }
    else if (type.baseType == HLSLBaseType_Sampler2DShadow)
    {
        typeName = "Texture2DShadowSampler";
    }
    else if (type.baseType == HLSLBaseType_Sampler2DMS)
    {
        typeName = "Texture2DMS<float4>";
    }
    

    if (type.flags & HLSLTypeFlag_Const)
    {
        m_writer.Write("const ");
    }
    if (type.flags & HLSLTypeFlag_Static)
    {
        m_writer.Write("static ");
    }

    // Interpolation modifiers.
    if (type.flags & HLSLTypeFlag_Centroid)
    {
        m_writer.Write("centroid ");
    }
    if (type.flags & HLSLTypeFlag_Linear)
    {
        m_writer.Write("linear ");
    }
    if (type.flags & HLSLTypeFlag_NoInterpolation)
    {
        m_writer.Write("nointerpolation ");
    }
    if (type.flags & HLSLTypeFlag_NoPerspective)
    {
        m_writer.Write("noperspective ");
    }
    if (type.flags & HLSLTypeFlag_Sample)   // @@ Only in shader model >= 4.1
    {
        m_writer.Write("sample ");
    }

    m_writer.Write("%s ", typeName);
}

void HLSLGenerator::OutputDeclarationBody(const HLSLType& type, const char* name, const char* semantic/*=NULL*/, const char* registerName/*=NULL*/, HLSLExpression * assignment/*=NULL*/)
{
    m_writer.Write("%s", name);

    if (type.array)
    {
        ASSERT(semantic == NULL);
        m_writer.Write("[");
        if (type.arraySize != NULL)
        {
            OutputExpression(type.arraySize);
        }
        m_writer.Write("]");
    }

    if (semantic != NULL) 
    {
        m_writer.Write(" : %s", semantic);
    }

    if (registerName != NULL)
    {
        if (m_isInsideBuffer)
        {
            m_writer.Write(" : packoffset(%s)", registerName);
        }
        else 
        {
            m_writer.Write(" : register(%s)", registerName);
        }
    }

    if (assignment != NULL && !IsSamplerType(type))
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
            OutputExpression(assignment);
        }
    }
}

void HLSLGenerator::OutputDeclaration(const HLSLType& type, const char* name, const char* semantic/*=NULL*/, const char* registerName/*=NULL*/, HLSLExpression * assignment/*=NULL*/)
{
    OutputDeclarationType(type);
    OutputDeclarationBody(type, name, semantic, registerName, assignment);
}

bool HLSLGenerator::ChooseUniqueName(const char* base, char* dst, int dstLength) const
{
    // IC: Try without suffix first.
    String_Printf(dst, dstLength, "%s", base);
    if (!m_tree->GetContainsString(base))
    {
        return true;
    }

    for (int i = 1; i < 1024; ++i)
    {
        String_Printf(dst, dstLength, "%s%d", base, i);
        if (!m_tree->GetContainsString(dst))
        {
            return true;
        }
    }
    return false;
}

}
