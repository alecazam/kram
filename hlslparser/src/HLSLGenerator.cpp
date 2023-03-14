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

const char* HLSLGenerator::GetTypeName(const HLSLType& type)
{
    HLSLBaseType baseType = type.baseType;
    
    const char* name = GetNumericTypeName(baseType);
    if (name)
        return name;
    
    if (baseType == HLSLBaseType_UserDefined)
        return type.typeName;
    
    // Functions can return void, especially with compute
    if (baseType == HLSLBaseType_Void)
        return "void";
    
    // TODO: pull names from table, they should be same
    if (IsSamplerType(baseType))
    {
        switch (baseType)
        {
            // samplers
            case HLSLBaseType_SamplerState:              return "SamplerState";
                
            // can only pair this with depth texture to match Metal
            case HLSLBaseType_SamplerComparisonState:    return "SamplerComparisonState";
            default: break;
        }
    }
    else if (IsTextureType(baseType))
    {
        switch (baseType)
        {
            // depth textures just use Texture2D typedef
            // TODO: add ms, others
            case HLSLBaseType_Depth2D:           return "Depth2D";
            case HLSLBaseType_Depth2DArray:      return "Depth2DArray";
            case HLSLBaseType_DepthCube:         return "DepthCube";
           
            case HLSLBaseType_Texture2D:         return "Texture2D";
            case HLSLBaseType_Texture2DArray:    return "Texture2DArray";
            case HLSLBaseType_Texture3D:         return "Texture3D";
            case HLSLBaseType_TextureCube:       return "TextureCube";
            case HLSLBaseType_TextureCubeArray:  return "TextureCubeArray";
            case HLSLBaseType_Texture2DMS:       return "Texture2DMS";
            default: break;
        }
    }
    
    Error("Unknown type");
    return NULL;
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
    m_target                        = HLSLTarget_VertexShader;
    m_isInsideBuffer                = false;
    m_error                         = false;
}


// @@ We need a better way of doing semantic replacement:
// - Look at the function being generated.
// - Return semantic, semantics associated to fields of the return structure, or output arguments, or fields of structures associated to output arguments -> output semantic replacement.
// - Semantics associated input arguments or fields of the input arguments -> input semantic replacement.
static const char * TranslateSemantic(const char* semantic, bool output, HLSLTarget target)
{
    // Note: these are all just passthrough of the DX10 semantics
    // except for BASEVERTEX/INSTANCE which doesn't seem to dxc compile.
    
    if (target == HLSLTarget_VertexShader)
    {
        if (output) 
        {

        }
        else {
            // see here for sample of builtin notation
            // https://github.com/microsoft/DirectXShaderCompiler/commit/b6fe9886ad
            
            // Vulkan/MSL only, requires ext DrawParameters
            // [[vk::builtin(\"BaseVertex\")]] uint baseVertex :
            // [[vk::builtin(\"BaseInstance\")]] uint instance : SV_BaseInstance
            
            if (String_Equal(semantic, "BASEVERTEX"))
                return "BaseVertex";  // vulkan only
            if (String_Equal(semantic, "BASEINSTANCE"))
                return "BaseInstance";  // vulkan only
        }
    }
    else if (target == HLSLTarget_PixelShader)
    {
        if (output)
        {

        }
        else
        {

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

bool HLSLGenerator::Generate(HLSLTree* tree, HLSLTarget target, const char* entryName)
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
    
    // Alec commented out to see if COmments survive
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

                // TODO: may have to be careful with SV_Position, since this puts
                // those last.  SSBO won't use those semantics, so should be okay.
                
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
        
        m_writer.Write("%s", name);
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
                m_writer.Write("%s%s", buffer, literalExpression->type == HLSLBaseType_Half ? "h" : "" );
            }
            break;        
        case HLSLBaseType_Int:
            m_writer.Write("%d", literalExpression->iValue);
            break;
        // TODO: missing uint, u/short, double
                
        case HLSLBaseType_Bool:
            m_writer.Write("%s", literalExpression->bValue ? "true" : "false");
            break;
        default:
            Error("Unhandled literal");
            //ASSERT(false);
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
            Error("Unhandled binary op");
            //ASSERT(false);
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
        Error("unknown expression");
    }
}

void HLSLGenerator::OutputArguments(HLSLArgument* argument)
{
    int numArgs = 0;
    while (argument != NULL)
    {
        if (numArgs > 0)
        {
            int indent = m_writer.EndLine(",");
            m_writer.BeginLine(indent);
        }

        const char * semantic = argument->sv_semantic ? argument->sv_semantic : argument->semantic;

        // Have to inject vulkan
        if (semantic)
        {
            if (strcmp(semantic, "PSIZE") == 0)
                m_writer.Write("%s ", "[[vk::builtin(\"PointSize\")]]");
            else if (strcmp(semantic, "BaseVertex") == 0)
                m_writer.Write("%s ", "[[vk::builtin(\"BaseVertex\")]]");
            else if (strcmp(semantic, "BaseInstance") == 0)
                m_writer.Write("%s ", "[[vk::builtin(\"BaseInstance\")]]");
        }
        
        // Then modifier
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

static const char* BufferTypeToName(HLSLBufferType bufferType)
{
    const char* name = "";
    switch(bufferType)
    {
        case HLSLBufferType_CBuffer: name = "cbuffer"; break;
        case HLSLBufferType_TBuffer: name = "tbuffer"; break;
            
        case HLSLBufferType_ConstantBuffer: name = "ConstantBuffer"; break;
        case HLSLBufferType_StructuredBuffer: name = "StructuredBuffer"; break;
        case HLSLBufferType_RWStructuredBuffer: name = "RWStructuredBuffer"; break;
        case HLSLBufferType_ByteAddressBuffer: name = "ByteAddressBuffer"; break;
        case HLSLBufferType_RWByteAddressBuffer: name = "RWByteAddresssBuffer"; break;
    }
    
    return name;
}
void HLSLGenerator::OutputStatements(int indent, HLSLStatement* statement)
{
    while (statement != NULL)
    {
        // skip pruned statements
        if (statement->hidden) 
        {
            statement = statement->nextStatement;
            continue;
        }

        // skip writing some types across multiple entry points
        if (statement->written &&
            (statement->nodeType == HLSLNodeType_Comment ||
             statement->nodeType == HLSLNodeType_Buffer ||
             statement->nodeType == HLSLNodeType_Struct))
        {
            statement = statement->nextStatement;
            continue;
        }
        statement->written = true;
        
        OutputAttributes(indent, statement->attributes);

        if (statement->nodeType == HLSLNodeType_Comment)
        {
            HLSLComment* comment = static_cast<HLSLComment*>(statement);
            m_writer.WriteLine(indent, "//%s", comment->text);
        }
        else if (statement->nodeType == HLSLNodeType_Declaration)
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

            if (!buffer->IsGlobalFields())
            {
                m_writer.BeginLine(indent, buffer->fileName, buffer->line);
                
                // write out template
                m_writer.Write("%s<%s> %s",
                               BufferTypeToName(buffer->bufferType),
                               buffer->bufferStruct->name,
                               buffer->name);
                
                // write out optinal register
                if (buffer->registerName != NULL)
                {
                     m_writer.Write(" : register(%s)", buffer->registerName);
                }
                
                m_writer.Write(";");
                m_writer.EndLine();
            }
            else
            {
                m_writer.BeginLine(indent, buffer->fileName, buffer->line);
                
                // not templated
                m_writer.Write("%s %s",
                               BufferTypeToName(buffer->bufferType),
                               buffer->name);
                
                // write out optional register
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
        // FX file constructs
//        else if (statement->nodeType == HLSLNodeType_Technique)
//        {
//            // Techniques are ignored.
//        }
//        else if (statement->nodeType == HLSLNodeType_Pipeline)
//        {
//            // Pipelines are ignored.
//        }
        else
        {
            // Unhanded statement type.
            Error("Unhandled statemement");
            //ASSERT(false);
        }

        statement = statement->nextStatement;
    }
}

void HLSLGenerator::OutputDeclaration(HLSLDeclaration* declaration)
{
    if (IsSamplerType(declaration->type))
    {
        int reg = -1;
        if (declaration->registerName != NULL)
        {
            sscanf(declaration->registerName, "s%d", &reg);
        }
        
        // sampler
        const char* samplerType = nullptr;
        if (declaration->type.baseType == HLSLBaseType_SamplerState)
        {
            samplerType = "SamplerState";
        }
        else if (declaration->type.baseType == HLSLBaseType_SamplerComparisonState)
        {
            samplerType = "SamplerComparisonState";
        }
        
        if (samplerType)
        {
            if (reg != -1)
            {
                m_writer.Write("%s %s : register(s%d)", samplerType, declaration->name, reg);
            }
            else
            {
                m_writer.Write("%s %s", samplerType, declaration->name);
            }
        }
        return;
    }
    if (IsTextureType(declaration->type))
    {
        int reg = -1;
        if (declaration->registerName != NULL)
        {
            sscanf(declaration->registerName, "t%d", &reg);
        }

        // @@ Handle generic sampler type.

        // TODO: have a way to disable use of half (like on MSLGenerator)
        bool isHalf = declaration->type.textureType == HLSLBaseType_Half;
        
        // Can't use half4 textures with spriv
        // Can tell Vulkan was written by/for desktop IHVs.
        // https://github.com/microsoft/DirectXShaderCompiler/issues/2711
        bool isSpirvTarget = true; // TODO: tie to CLI option
        if (isSpirvTarget)
            isHalf = false;
        
        const char* formatType = isHalf ? "half4" : "float4";
        
        // Unlike Metal, that just uses half/float, the type
        // seems to be dimension specific on HLSL.  So may need
        // caller to specify more types.
        
        // texture carts the dimension and format
        const char* textureType = GetTypeName(declaration->type);
    
        if (textureType != NULL)
        {
            if (reg != -1)
            {
                m_writer.Write("%s<%s> %s : register(t%d)", textureType, formatType, declaration->name, reg);
            }
            else
            {
                m_writer.Write("%s<%s> %s", textureType, formatType, declaration->name);
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
