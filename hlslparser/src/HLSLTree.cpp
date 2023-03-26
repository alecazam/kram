#include "HLSLTree.h"

#include "Engine.h"

namespace M4
{

// TODO: split helper calls out to new .h, so can include that
// over to HLSLParser.cpp
extern bool IsSamplerType(const HLSLType & type);

extern bool IsScalarType(HLSLBaseType type);
extern bool IsIntegerType(HLSLBaseType type);
extern bool IsFloatingType(HLSLBaseType type);

extern int32_t GetVectorDimension(HLSLBaseType type);



HLSLTree::HLSLTree(Allocator* allocator) :
    m_allocator(allocator), m_stringPool(allocator)
{
    m_firstPage         = m_allocator->New<NodePage>();
    m_firstPage->next   = NULL;

    m_currentPage       = m_firstPage;
    m_currentPageOffset = 0;

    m_root              = AddNode<HLSLRoot>(NULL, 1);
}

HLSLTree::~HLSLTree()
{
    NodePage* page = m_firstPage;
    while (page != NULL)
    {
        NodePage* next = page->next;
        m_allocator->Delete(page);
        page = next;
    }
}

void HLSLTree::AllocatePage()
{
    NodePage* newPage    = m_allocator->New<NodePage>();
    newPage->next        = NULL;
    m_currentPage->next  = newPage;
    m_currentPageOffset  = 0;
    m_currentPage        = newPage;
}

const char* HLSLTree::AddString(const char* string)
{   
    return m_stringPool.AddString(string);
}

const char* HLSLTree::AddStringFormat(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    const char * string = m_stringPool.AddStringFormatList(format, args);
    va_end(args);
    return string;
}

bool HLSLTree::GetContainsString(const char* string) const
{
    return m_stringPool.GetContainsString(string);
}

HLSLRoot* HLSLTree::GetRoot() const
{
    return m_root;
}

void* HLSLTree::AllocateMemory(size_t size)
{
    if (m_currentPageOffset + size > s_nodePageSize)
    {
        AllocatePage();
    }
    void* buffer = m_currentPage->buffer + m_currentPageOffset;
    m_currentPageOffset += size;
    return buffer;
}

// @@ This doesn't do any parameter matching. Simply returns the first function with that name.
HLSLFunction * HLSLTree::FindFunction(const char * name)
{
    HLSLStatement * statement = m_root->statement;
    while (statement != NULL)
    {
        if (statement->nodeType == HLSLNodeType_Function)
        {
            HLSLFunction * function = (HLSLFunction *)statement;
            if (String_Equal(name, function->name))
            {
                return function;
            }
        }

        statement = statement->nextStatement;
    }

    return NULL;
}

HLSLDeclaration * HLSLTree::FindGlobalDeclaration(const char * name, HLSLBuffer ** buffer_out/*=NULL*/)
{
    HLSLStatement * statement = m_root->statement;
    while (statement != NULL)
    {
        if (statement->nodeType == HLSLNodeType_Declaration)
        {
            HLSLDeclaration * declaration = (HLSLDeclaration *)statement;
            if (String_Equal(name, declaration->name))
            {
                if (buffer_out) *buffer_out = NULL;
                return declaration;
            }
        }
        else if (statement->nodeType == HLSLNodeType_Buffer)
        {
            HLSLBuffer* buffer = (HLSLBuffer*)statement;
            
            // This searches the fields to find the buffer,
            // since cbuffer/tbuffer represent globals.
            if (buffer->IsGlobalFields())
            {
                HLSLDeclaration* field = buffer->field;
                while (field != NULL)
                {
                    ASSERT(field->nodeType == HLSLNodeType_Declaration);
                    if (String_Equal(name, field->name))
                    {
                        if (buffer_out) *buffer_out = buffer;
                        return field;
                    }
                    field = (HLSLDeclaration*)field->nextStatement;
                }
            }
            else
            {
                if (String_Equal(name, buffer->name))
                {
                    if (buffer_out) *buffer_out = buffer;
                    return NULL;
                }
                
                /* This isn't same type...
                 
                // Note: should pass buffers, but buffer/texture
                // and cbuffer fields can be global to entire shader.
                
                // find struct first
                const HLSLStruct* bufferStruct = buffer->bufferStruct;
                
                // new search those for the fields
                HLSLStructField* field = bufferStruct->field;
                while (field != NULL)
                {
                    ASSERT(field->nodeType == HLSLNodeType_Declaration);
                    if (String_Equal(name, field->name))
                    {
                        if (buffer_out) *buffer_out = buffer;
                        return field;
                    }
                    field = field->nextField;
                }
                */
            }
        }

        statement = statement->nextStatement;
    }

    if (buffer_out) *buffer_out = NULL;
    return NULL;
}

HLSLStruct * HLSLTree::FindGlobalStruct(const char * name)
{
    HLSLStatement * statement = m_root->statement;
    while (statement != NULL)
    {
        if (statement->nodeType == HLSLNodeType_Struct)
        {
            HLSLStruct * declaration = (HLSLStruct *)statement;
            if (String_Equal(name, declaration->name))
            {
                return declaration;
            }
        }

        statement = statement->nextStatement;
    }

    return NULL;
}

/* FX files
HLSLTechnique * HLSLTree::FindTechnique(const char * name)
{
    HLSLStatement * statement = m_root->statement;
    while (statement != NULL)
    {
        if (statement->nodeType == HLSLNodeType_Technique)
        {
            HLSLTechnique * technique = (HLSLTechnique *)statement;
            if (String_Equal(name, technique->name))
            {
                return technique;
            }
        }

        statement = statement->nextStatement;
    }

    return NULL;
}

HLSLPipeline * HLSLTree::FindFirstPipeline()
{
    return FindNextPipeline(NULL);
}

HLSLPipeline * HLSLTree::FindNextPipeline(HLSLPipeline * current)
{
    HLSLStatement * statement = current ? current : m_root->statement;
    while (statement != NULL)
    {
        if (statement->nodeType == HLSLNodeType_Pipeline)
        {
            return (HLSLPipeline *)statement;
        }

        statement = statement->nextStatement;
    }

    return NULL;
}

HLSLPipeline * HLSLTree::FindPipeline(const char * name)
{
    HLSLStatement * statement = m_root->statement;
    while (statement != NULL)
    {
        if (statement->nodeType == HLSLNodeType_Pipeline)
        {
            HLSLPipeline * pipeline = (HLSLPipeline *)statement;
            if (String_Equal(name, pipeline->name))
            {
                return pipeline;
            }
        }

        statement = statement->nextStatement;
    }

    return NULL;
}
*/

HLSLBuffer * HLSLTree::FindBuffer(const char * name)
{
    HLSLStatement * statement = m_root->statement;
    while (statement != NULL)
    {
        if (statement->nodeType == HLSLNodeType_Buffer)
        {
            HLSLBuffer * buffer = (HLSLBuffer *)statement;
            if (String_Equal(name, buffer->name))
            {
                return buffer;
            }
        }

        statement = statement->nextStatement;
    }

    return NULL;
}



bool HLSLTree::GetExpressionValue(HLSLExpression * expression, int & value)
{
    ASSERT (expression != NULL);

    // Expression must be constant.
    if ((expression->expressionType.flags & HLSLTypeFlag_Const) == 0) 
    {
        return false;
    }

    // We are expecting an integer scalar.
    // TODO: Add support for type conversion from uint scalar types.
    if (expression->expressionType.baseType != HLSLBaseType_Long &&
        expression->expressionType.baseType != HLSLBaseType_Short &&
        expression->expressionType.baseType != HLSLBaseType_Int &&
        
        expression->expressionType.baseType != HLSLBaseType_Bool)
    {
        return false;
    }

    if (expression->expressionType.array) 
    {
        return false;
    }

    if (expression->nodeType == HLSLNodeType_BinaryExpression) 
    {
        HLSLBinaryExpression * binaryExpression = (HLSLBinaryExpression *)expression;

        int value1, value2;
        if (!GetExpressionValue(binaryExpression->expression1, value1) ||
            !GetExpressionValue(binaryExpression->expression2, value2))
        {
            return false;
        }

        switch(binaryExpression->binaryOp)
        {
            case HLSLBinaryOp_And:
                value = value1 && value2;
                return true;
            case HLSLBinaryOp_Or:
                value = value1 || value2;
                return true;
            case HLSLBinaryOp_Add:
                value = value1 + value2;
                return true;
            case HLSLBinaryOp_Sub:
                value = value1 - value2;
                return true;
            case HLSLBinaryOp_Mul:
                value = value1 * value2;
                return true;
            case HLSLBinaryOp_Div:
                value = value1 / value2;
                return true;
            case HLSLBinaryOp_Less:
                value = value1 < value2;
                return true;
            case HLSLBinaryOp_Greater:
                value = value1 > value2;
                return true;
            case HLSLBinaryOp_LessEqual:
                value = value1 <= value2;
                return true;
            case HLSLBinaryOp_GreaterEqual:
                value = value1 >= value2;
                return true;
            case HLSLBinaryOp_Equal:
                value = value1 == value2;
                return true;
            case HLSLBinaryOp_NotEqual:
                value = value1 != value2;
                return true;
            case HLSLBinaryOp_BitAnd:
                value = value1 & value2;
                return true;
            case HLSLBinaryOp_BitOr:
                value = value1 | value2;
                return true;
            case HLSLBinaryOp_BitXor:
                value = value1 ^ value2;
                return true;
            case HLSLBinaryOp_Assign:
            case HLSLBinaryOp_AddAssign:
            case HLSLBinaryOp_SubAssign:
            case HLSLBinaryOp_MulAssign:
            case HLSLBinaryOp_DivAssign:
                // IC: These are not valid on non-constant expressions and should fail earlier when querying expression value.
                return false;
        }
    }
    else if (expression->nodeType == HLSLNodeType_UnaryExpression) 
    {
        HLSLUnaryExpression * unaryExpression = (HLSLUnaryExpression *)expression;

        if (!GetExpressionValue(unaryExpression->expression, value))
        {
            return false;
        }

        switch(unaryExpression->unaryOp)
        {
            case HLSLUnaryOp_Negative:
                value = -value;
                return true;
            case HLSLUnaryOp_Positive:
                // nop.
                return true;
            case HLSLUnaryOp_Not:
                value = !value;
                return true;
            case HLSLUnaryOp_BitNot:
                value = ~value;
                return true;
            case HLSLUnaryOp_PostDecrement:
            case HLSLUnaryOp_PostIncrement:
            case HLSLUnaryOp_PreDecrement:
            case HLSLUnaryOp_PreIncrement:
                // IC: These are not valid on non-constant expressions and should fail earlier when querying expression value.
                return false;
        }
    }
    else if (expression->nodeType == HLSLNodeType_IdentifierExpression)
    {
        HLSLIdentifierExpression * identifier = (HLSLIdentifierExpression *)expression;

        HLSLDeclaration * declaration = FindGlobalDeclaration(identifier->name);
        if (declaration == NULL) 
        {
            return false;
        }
        if ((declaration->type.flags & HLSLTypeFlag_Const) == 0)
        {
            return false;
        }

        return GetExpressionValue(declaration->assignment, value);
    }
    else if (expression->nodeType == HLSLNodeType_LiteralExpression) 
    {
        HLSLLiteralExpression * literal = (HLSLLiteralExpression *)expression;
   
        if (literal->expressionType.baseType == HLSLBaseType_Int)
            value = literal->iValue;
        else if (literal->expressionType.baseType == HLSLBaseType_Long)
            value = literal->iValue; // precision loss to Int
        else if (literal->expressionType.baseType == HLSLBaseType_Short)
            value = literal->iValue;
        
        else if (literal->expressionType.baseType == HLSLBaseType_Bool)
            value = (int)literal->bValue;
        else
            return false;
        
        return true;
    }

    return false;
}

// TODO: Nothing calling this?
bool HLSLTree::NeedsFunction(const char* name)
{
    // Early out
    if (!GetContainsString(name))
        return false;

    struct NeedsFunctionVisitor: HLSLTreeVisitor
    {
        const char* name;
        bool result;

        virtual ~NeedsFunctionVisitor() {}
        
        virtual void VisitTopLevelStatement(HLSLStatement * node)
        {
            if (!node->hidden)
                HLSLTreeVisitor::VisitTopLevelStatement(node);
        }

        virtual void VisitFunctionCall(HLSLFunctionCall * node)
        {
            result = result || String_Equal(name, node->function->name);

            HLSLTreeVisitor::VisitFunctionCall(node);
        }
    };

    NeedsFunctionVisitor visitor;
    visitor.name = name;
    visitor.result = false;

    visitor.VisitRoot(m_root);

    return visitor.result;
}

// Returns dimension, 0 if invalid.
int HLSLTree::GetExpressionValue(HLSLExpression * expression, float values[4])
{
    ASSERT (expression != NULL);

    // Expression must be constant.
    if ((expression->expressionType.flags & HLSLTypeFlag_Const) == 0) 
    {
        return 0;
    }

    HLSLBaseType type = expression->expressionType.baseType;
    
    if (IsIntegerType(type))
    {
        if (IsScalarType(type))
        {
            int intValue;
            if (GetExpressionValue(expression, intValue)) {
                for (int i = 0; i < 4; i++) values[i] = (float)intValue;   // @@ Warn if conversion is not exact.
                return 1;
            }
        }
        
        return 0;
    }
    // this skips other int types not handled above
    if (!IsFloatingType(type))
        return 0;

    // @@ Not supported yet, but we may need it?
    if (expression->expressionType.array) 
    {
        return false;
    }

    int dim = GetVectorDimension(type);

    if (expression->nodeType == HLSLNodeType_BinaryExpression) 
    {
        HLSLBinaryExpression * binaryExpression = (HLSLBinaryExpression *)expression;
        
        float values1[4], values2[4];
        int dim1 = GetExpressionValue(binaryExpression->expression1, values1);
        int dim2 = GetExpressionValue(binaryExpression->expression2, values2);

        if (dim1 == 0 || dim2 == 0)
        {
            return 0;
        }

        if (dim1 != dim2)
        {
            // Broadcast scalar to vector size.
            if (dim1 == 1)
            {
                for (int i = 1; i < dim2; i++) values1[i] = values1[0];
                dim1 = dim2;
            }
            else if (dim2 == 1)
            {
                for (int i = 1; i < dim1; i++) values2[i] = values2[0];
                dim2 = dim1;
            }
            else
            {
                return 0;
            }
        }
        ASSERT(dim == dim1);

        switch(binaryExpression->binaryOp)
        {
            case HLSLBinaryOp_Add:
                for (int i = 0; i < dim; i++) values[i] = values1[i] + values2[i];
                return dim;
            case HLSLBinaryOp_Sub:
                for (int i = 0; i < dim; i++) values[i] = values1[i] - values2[i];
                return dim;
            case HLSLBinaryOp_Mul:
                for (int i = 0; i < dim; i++) values[i] = values1[i] * values2[i];
                return dim;
            case HLSLBinaryOp_Div:
                for (int i = 0; i < dim; i++) values[i] = values1[i] / values2[i];
                return dim;
            default:
                return 0;
        }
    }
    else if (expression->nodeType == HLSLNodeType_UnaryExpression) 
    {
        HLSLUnaryExpression * unaryExpression = (HLSLUnaryExpression *)expression;
       
        int dim1 = GetExpressionValue(unaryExpression->expression, values);
        if (dim1 == 0)
        {
            return 0;
        }
        ASSERT(dim == dim1);

        switch(unaryExpression->unaryOp)
        {
            case HLSLUnaryOp_Negative:
                for (int i = 0; i < dim; i++) values[i] = -values[i];
                return dim;
            case HLSLUnaryOp_Positive:
                // nop.
                return dim;
            default:
                return 0;
        }
    }
    else if (expression->nodeType == HLSLNodeType_ConstructorExpression)
    {
        HLSLConstructorExpression * constructor = (HLSLConstructorExpression *)expression;

        int idx = 0;
        HLSLExpression * arg = constructor->argument;
        while (arg != NULL)
        {
            float tmp[4];
            int n = GetExpressionValue(arg, tmp);
            for (int i = 0; i < n; i++) values[idx + i] = tmp[i];
            idx += n;

            arg = arg->nextExpression;
        }
        ASSERT(dim == idx);

        return dim;
    }
    else if (expression->nodeType == HLSLNodeType_IdentifierExpression)
    {
        HLSLIdentifierExpression * identifier = (HLSLIdentifierExpression *)expression;

        HLSLDeclaration * declaration = FindGlobalDeclaration(identifier->name);
        if (declaration == NULL) 
        {
            return 0;
        }
        if ((declaration->type.flags & HLSLTypeFlag_Const) == 0)
        {
            return 0;
        }

        return GetExpressionValue(declaration->assignment, values);
    }
    else if (expression->nodeType == HLSLNodeType_LiteralExpression)
    {
        HLSLLiteralExpression * literal = (HLSLLiteralExpression *)expression;

        if (literal->expressionType.baseType == HLSLBaseType_Float)
            values[0] = literal->fValue;
        else if (literal->expressionType.baseType == HLSLBaseType_Half)
            values[0] = literal->fValue;
        else if (literal->expressionType.baseType == HLSLBaseType_Double)
            values[0] = literal->fValue; // TODO: need more precision
        
        else if (literal->expressionType.baseType == HLSLBaseType_Bool)
            values[0] = literal->bValue;
        
        // TODO: add uint types, fix precision of short/long/double/half
        // signed ints
        else if (literal->expressionType.baseType == HLSLBaseType_Int)
            values[0] = (float)literal->iValue;  // @@ Warn if conversion is not exact.
        else if (literal->expressionType.baseType == HLSLBaseType_Short)
            values[0] = (float)literal->iValue;
        else if (literal->expressionType.baseType == HLSLBaseType_Long)
            values[0] = (float)literal->iValue;
        else
            return 0;
        
        return 1;
    }

    return 0;
}




void HLSLTreeVisitor::VisitType(HLSLType & type)
{
}

void HLSLTreeVisitor::VisitRoot(HLSLRoot * root)
{
    HLSLStatement * statement = root->statement;
    while (statement != NULL) {
        VisitTopLevelStatement(statement);
        statement = statement->nextStatement;
    }
}

void HLSLTreeVisitor::VisitTopLevelStatement(HLSLStatement * node)
{
    if (node->nodeType == HLSLNodeType_Declaration) {
        VisitDeclaration((HLSLDeclaration *)node);
    }
    else if (node->nodeType == HLSLNodeType_Struct) {
        VisitStruct((HLSLStruct *)node);
    }
    else if (node->nodeType == HLSLNodeType_Buffer) {
        VisitBuffer((HLSLBuffer *)node);
    }
    else if (node->nodeType == HLSLNodeType_Function) {
        VisitFunction((HLSLFunction *)node);
    }
    else if (node->nodeType == HLSLNodeType_Comment) {
        VisitComment((HLSLComment*)node);
    }
    
    // FX file stuff
//    else if (node->nodeType == HLSLNodeType_Technique) {
//        VisitTechnique((HLSLTechnique *)node);
//    }
//    else if (node->nodeType == HLSLNodeType_Pipeline) {
//        VisitPipeline((HLSLPipeline *)node);
//    }
    
    else {
        ASSERT(false);
    }
}

void HLSLTreeVisitor::VisitStatements(HLSLStatement * statement)
{
    while (statement != NULL) {
        VisitStatement(statement);
        statement = statement->nextStatement;
    }
}

void HLSLTreeVisitor::VisitStatement(HLSLStatement * node)
{
    // Function statements
    if (node->nodeType == HLSLNodeType_Declaration) {
        VisitDeclaration((HLSLDeclaration *)node);
    }
    else if (node->nodeType == HLSLNodeType_ExpressionStatement) {
        VisitExpressionStatement((HLSLExpressionStatement *)node);
    }
    else if (node->nodeType == HLSLNodeType_ReturnStatement) {
        VisitReturnStatement((HLSLReturnStatement *)node);
    }
    else if (node->nodeType == HLSLNodeType_DiscardStatement) {
        VisitDiscardStatement((HLSLDiscardStatement *)node);
    }
    else if (node->nodeType == HLSLNodeType_BreakStatement) {
        VisitBreakStatement((HLSLBreakStatement *)node);
    }
    else if (node->nodeType == HLSLNodeType_ContinueStatement) {
        VisitContinueStatement((HLSLContinueStatement *)node);
    }
    else if (node->nodeType == HLSLNodeType_IfStatement) {
        VisitIfStatement((HLSLIfStatement *)node);
    }
    else if (node->nodeType == HLSLNodeType_ForStatement) {
        VisitForStatement((HLSLForStatement *)node);
    }
    else if (node->nodeType == HLSLNodeType_BlockStatement) {
        VisitBlockStatement((HLSLBlockStatement *)node);
    }
    else if (node->nodeType == HLSLNodeType_Comment) {
        VisitComment((HLSLComment *)node);
    }
    else {
        ASSERT(false);
    }
}

void HLSLTreeVisitor::VisitDeclaration(HLSLDeclaration * node)
{
    VisitType(node->type);
    /*do {
        VisitExpression(node->assignment);
        node = node->nextDeclaration;
    } while (node);*/
    if (node->assignment != NULL) {
        VisitExpression(node->assignment);
    }
    if (node->nextDeclaration != NULL) {
        VisitDeclaration(node->nextDeclaration);
    }
}

void HLSLTreeVisitor::VisitStruct(HLSLStruct * node)
{
    HLSLStructField * field = node->field;
    while (field != NULL) {
        VisitStructField(field);
        field = field->nextField;
    }
}

void HLSLTreeVisitor::VisitStructField(HLSLStructField * node)
{
    // This can use a constant in an array field that must be resolved
    if (node->type.array)
    {
        VisitExpression(node->type.arraySize);
    }
    
    VisitType(node->type);
}

void HLSLTreeVisitor::VisitBuffer(HLSLBuffer * node)
{
    if (node->IsGlobalFields())
    {
        HLSLDeclaration* field = node->field;
        while (field != NULL) {
            ASSERT(field->nodeType == HLSLNodeType_Declaration);
            VisitDeclaration(field);
            ASSERT(field->nextDeclaration == NULL);
            field = (HLSLDeclaration *)field->nextStatement;
        }
    }
    else
    {
        VisitStruct(node->bufferStruct);
    }
}

/*void HLSLTreeVisitor::VisitBufferField(HLSLBufferField * node)
{
    VisitType(node->type);
}*/

void HLSLTreeVisitor::VisitFunction(HLSLFunction * node)
{
    VisitType(node->returnType);

    HLSLArgument* argument = node->argument;
    while (argument != NULL) {
        VisitArgument(argument);
        argument = argument->nextArgument;
    }

    VisitStatements(node->statement);
}

void HLSLTreeVisitor::VisitArgument(HLSLArgument * node)
{
    VisitType(node->type);
    if (node->defaultValue != NULL) {
        VisitExpression(node->defaultValue);
    }
}

void HLSLTreeVisitor::VisitExpressionStatement(HLSLExpressionStatement * node)
{
    VisitExpression(node->expression);
}

void HLSLTreeVisitor::VisitExpression(HLSLExpression * node)
{
    VisitType(node->expressionType);

    if (node->nodeType == HLSLNodeType_UnaryExpression) {
        VisitUnaryExpression((HLSLUnaryExpression *)node);
    }
    else if (node->nodeType == HLSLNodeType_BinaryExpression) {
        VisitBinaryExpression((HLSLBinaryExpression *)node);
    }
    else if (node->nodeType == HLSLNodeType_ConditionalExpression) {
        VisitConditionalExpression((HLSLConditionalExpression *)node);
    }
    else if (node->nodeType == HLSLNodeType_CastingExpression) {
        VisitCastingExpression((HLSLCastingExpression *)node);
    }
    else if (node->nodeType == HLSLNodeType_LiteralExpression) {
        VisitLiteralExpression((HLSLLiteralExpression *)node);
    }
    else if (node->nodeType == HLSLNodeType_IdentifierExpression) {
        VisitIdentifierExpression((HLSLIdentifierExpression *)node);
    }
    else if (node->nodeType == HLSLNodeType_ConstructorExpression) {
        VisitConstructorExpression((HLSLConstructorExpression *)node);
    }
    else if (node->nodeType == HLSLNodeType_MemberAccess) {
        VisitMemberAccess((HLSLMemberAccess *)node);
    }
    else if (node->nodeType == HLSLNodeType_ArrayAccess) {
        VisitArrayAccess((HLSLArrayAccess *)node);
    }
    else if (node->nodeType == HLSLNodeType_FunctionCall) {
        VisitFunctionCall((HLSLFunctionCall *)node);
    }
    // Acoget-TODO: This was missing. Did adding it break anything?
//    else if (node->nodeType == HLSLNodeType_SamplerState) {
//        VisitSamplerState((HLSLSamplerState *)node);
//    }
    else {
        ASSERT(false);
    }
}

void HLSLTreeVisitor::VisitReturnStatement(HLSLReturnStatement * node)
{
    VisitExpression(node->expression);
}

void HLSLTreeVisitor::VisitDiscardStatement(HLSLDiscardStatement * node) {}
void HLSLTreeVisitor::VisitBreakStatement(HLSLBreakStatement * node) {}
void HLSLTreeVisitor::VisitContinueStatement(HLSLContinueStatement * node) {}

void HLSLTreeVisitor::VisitIfStatement(HLSLIfStatement * node)
{
    VisitExpression(node->condition);
    VisitStatements(node->statement);
    if (node->elseStatement) {
        VisitStatements(node->elseStatement);
    }
}

void HLSLTreeVisitor::VisitForStatement(HLSLForStatement * node)
{
    if (node->initialization) {
        VisitDeclaration(node->initialization);
    }
    if (node->condition) {
        VisitExpression(node->condition);
    }
    if (node->increment) {
        VisitExpression(node->increment);
    }
    VisitStatements(node->statement);
}

void HLSLTreeVisitor::VisitBlockStatement(HLSLBlockStatement * node)
{
    VisitStatements(node->statement);
}

void HLSLTreeVisitor::VisitUnaryExpression(HLSLUnaryExpression * node)
{
    VisitExpression(node->expression);
}

void HLSLTreeVisitor::VisitBinaryExpression(HLSLBinaryExpression * node)
{
    VisitExpression(node->expression1);
    VisitExpression(node->expression2);
}

void HLSLTreeVisitor::VisitConditionalExpression(HLSLConditionalExpression * node)
{
    VisitExpression(node->condition);
    VisitExpression(node->falseExpression);
    VisitExpression(node->trueExpression);
}

void HLSLTreeVisitor::VisitCastingExpression(HLSLCastingExpression * node)
{
    VisitType(node->type);
    VisitExpression(node->expression);
}

void HLSLTreeVisitor::VisitLiteralExpression(HLSLLiteralExpression * node) {}
void HLSLTreeVisitor::VisitIdentifierExpression(HLSLIdentifierExpression * node) {}

void HLSLTreeVisitor::VisitConstructorExpression(HLSLConstructorExpression * node)
{
    HLSLExpression * argument = node->argument;
    while (argument != NULL) {
        VisitExpression(argument);
        argument = argument->nextExpression;
    }
}

void HLSLTreeVisitor::VisitMemberAccess(HLSLMemberAccess * node)
{
    VisitExpression(node->object);
}

void HLSLTreeVisitor::VisitArrayAccess(HLSLArrayAccess * node)
{
    VisitExpression(node->array);
    VisitExpression(node->index);
}

void HLSLTreeVisitor::VisitFunctionCall(HLSLFunctionCall * node)
{
    HLSLExpression * argument = node->argument;
    while (argument != NULL) {
        VisitExpression(argument);
        argument = argument->nextExpression;
    }
}

/*
void HLSLTreeVisitor::VisitStateAssignment(HLSLStateAssignment * node) {}

void HLSLTreeVisitor::VisitSamplerState(HLSLSamplerState * node)
{
    HLSLStateAssignment * stateAssignment = node->stateAssignments;
    while (stateAssignment != NULL) {
        VisitStateAssignment(stateAssignment);
        stateAssignment = stateAssignment->nextStateAssignment;
    }
}

void HLSLTreeVisitor::VisitPass(HLSLPass * node)
{
    HLSLStateAssignment * stateAssignment = node->stateAssignments;
    while (stateAssignment != NULL) {
        VisitStateAssignment(stateAssignment);
        stateAssignment = stateAssignment->nextStateAssignment;
    }
}

void HLSLTreeVisitor::VisitTechnique(HLSLTechnique * node)
{
    HLSLPass * pass = node->passes;
    while (pass != NULL) {
        VisitPass(pass);
        pass = pass->nextPass;
    }
}


void HLSLTreeVisitor::VisitPipeline(HLSLPipeline * node)
{
    // This is for FX files
}
*/

void HLSLTreeVisitor::VisitComment(HLSLComment * node)
{
    
}

void HLSLTreeVisitor::VisitFunctions(HLSLRoot * root)
{
    HLSLStatement * statement = root->statement;
    while (statement != NULL) {
        if (statement->nodeType == HLSLNodeType_Function) {
            VisitFunction((HLSLFunction *)statement);
        }

        statement = statement->nextStatement;
    }
}

void HLSLTreeVisitor::VisitParameters(HLSLRoot * root)
{
    HLSLStatement * statement = root->statement;
    while (statement != NULL) {
        if (statement->nodeType == HLSLNodeType_Declaration) {
            VisitDeclaration((HLSLDeclaration *)statement);
        }

        statement = statement->nextStatement;
    }
}


class ResetHiddenFlagVisitor : public HLSLTreeVisitor
{
public:
    virtual ~ResetHiddenFlagVisitor() {}
    
    virtual void VisitTopLevelStatement(HLSLStatement * statement) override
    {
        statement->hidden = true;

        if (statement->nodeType == HLSLNodeType_Buffer)
        {
            VisitBuffer((HLSLBuffer*)statement);
        }
    }

    // Hide buffer fields.
    virtual void VisitDeclaration(HLSLDeclaration * node) override
    {
       // node->hidden = true;
    }

    virtual void VisitComment(HLSLComment * node) override
    {
        node->hidden = true;
    }
    
    virtual void VisitArgument(HLSLArgument * node) override
    {
        node->hidden = false;   // Arguments are visible by default.
    }
};

class MarkVisibleStatementsVisitor : public HLSLTreeVisitor
{
public:
    
    HLSLTree * tree;
    MarkVisibleStatementsVisitor(HLSLTree * tree) : tree(tree) {}

    virtual ~MarkVisibleStatementsVisitor() {}
    
    virtual void VisitComment(HLSLComment * node) override
    {
        node->hidden = false;
    }

    virtual void VisitFunction(HLSLFunction * node) override
    {
        node->hidden = false;
        HLSLTreeVisitor::VisitFunction(node);

        if (node->forward)
            VisitFunction(node->forward);
    }

    virtual void VisitFunctionCall(HLSLFunctionCall * node) override
    {
        HLSLTreeVisitor::VisitFunctionCall(node);

        if (node->function->hidden)
        {
            VisitFunction(const_cast<HLSLFunction*>(node->function));
        }
    }

    virtual void VisitIdentifierExpression(HLSLIdentifierExpression * node) override
    {
        HLSLTreeVisitor::VisitIdentifierExpression(node);
        
        if (node->global)
        {
            HLSLBuffer* buffer = NULL;
            HLSLDeclaration * declaration = tree->FindGlobalDeclaration(node->name, &buffer);

            if (declaration != NULL && declaration->hidden)
            {
                declaration->hidden = false;
                VisitDeclaration(declaration);
            }
            if (buffer != NULL && buffer->hidden)
            {
                buffer->hidden = false;
            }
        }
    }
        
    virtual void VisitType(HLSLType & type) override
    {
//        if (type.array)
//        {
//            //  Alec added this to try to handle structs with array constants, but
//            // it causes other issues.  VisitStructField calls VisitType.
//
//            // handle sized or unsized array, since sized may use constant
//            // VisitExpression(type.arraySize);
//            int bp = 0;
//            bp = bp;
//        }
//        else
        if (type.baseType == HLSLBaseType_UserDefined)
        {
            HLSLStruct * globalStruct = tree->FindGlobalStruct(type.typeName);
            if (globalStruct != NULL)
            {
                globalStruct->hidden = false;
                VisitStruct(globalStruct);
            }
        }
    }

};


void PruneTree(HLSLTree* tree, const char* entryName0, const char* entryName1/*=NULL*/)
{
    HLSLRoot* root = tree->GetRoot();

    // Reset all flags.
    ResetHiddenFlagVisitor reset;
    reset.VisitRoot(root);

    // Mark all the statements necessary for these entrypoints.
    HLSLFunction* entry = tree->FindFunction(entryName0);
    if (entry != NULL)
    {
        MarkVisibleStatementsVisitor mark(tree);
        mark.VisitFunction(entry);
    }

    if (entryName1 != NULL)
    {
        entry = tree->FindFunction(entryName1);
        if (entry != NULL)
        {
            MarkVisibleStatementsVisitor mark(tree);
            mark.VisitFunction(entry);
        }
    }

    // Mark buffers visible, if any of their fields is visible.
    HLSLStatement * statement = root->statement;
    while (statement != NULL)
    {
        if (statement->nodeType == HLSLNodeType_Buffer)
        {
            HLSLBuffer* buffer = (HLSLBuffer*)statement;

            if (buffer->IsGlobalFields())
            {
                // mark buffer visible if any of its fields are used
                HLSLDeclaration* field = buffer->field;
                while (field != NULL)
                {
                    ASSERT(field->nodeType == HLSLNodeType_Declaration);
                    if (!field->hidden)
                    {
                        buffer->hidden = false;
                        break;
                    }
                    field = (HLSLDeclaration*)field->nextStatement;
                }
            }
            else
            {
                // TODO: these load from a struct so may just need
                // to somehow mark this if present.
                
                /* all struct fields are hidden = false, so this doesn't work
                // mark buffer visible if any struct fields are used
                HLSLStructField* field = buffer->bufferStruct->field;
                while (field != NULL)
                {
                    ASSERT(field->nodeType == HLSLNodeType_StructField);
                    if (!field->hidden)
                    {
                        buffer->hidden = false;
                        break;
                    }
                    field = (HLSLStructField*)field->nextField;
                }
                */
            }
        }

        statement = statement->nextStatement;
    }
}


void SortTree(HLSLTree * tree)
{
    // Stable sort so that statements are in this order:
    // const scalars for arrays, structs, declarations, functions, techniques.
	// but their relative order is preserved.

    HLSLRoot* root = tree->GetRoot();

    HLSLStatement* constScalarDeclarations = NULL;
    HLSLStatement* lastConstScalarDeclaration = NULL;
    
    HLSLStatement* structs = NULL;
    HLSLStatement* lastStruct = NULL;
    
    HLSLStatement* constDeclarations = NULL;
    HLSLStatement* lastConstDeclaration = NULL;
    
    HLSLStatement* declarations = NULL;
    HLSLStatement* lastDeclaration = NULL;
    
    HLSLStatement* functions = NULL;
    HLSLStatement* lastFunction = NULL;
    
    HLSLStatement* other = NULL;
    HLSLStatement* lastOther = NULL;

    
#define AppendToList(statement, list, listLast) \
    if (list == NULL) list = statement; \
    if (listLast != NULL) listLast->nextStatement = statement; \
    listLast = statement;
    
    HLSLStatement* statement = root->statement;
    while (statement != NULL) {
        HLSLStatement* nextStatement = statement->nextStatement;
        statement->nextStatement = NULL;

        if (statement->nodeType == HLSLNodeType_Struct) {
            AppendToList(statement, structs, lastStruct);
        }
        else if (statement->nodeType == HLSLNodeType_Declaration ||
                 statement->nodeType == HLSLNodeType_Buffer)
        {
            // There are cases where a struct uses a const array size,
            // so those need to be ordered prior to the struct.
            if (statement->nodeType == HLSLNodeType_Declaration)
            {
                HLSLDeclaration* decl = (HLSLDeclaration *)statement;
                
                if (decl->type.flags & HLSLTypeFlag_Const)
                {
                    // this is a global scalar, so best to order first
                    if (IsScalarType(decl->type.baseType))
                    {
                        AppendToList(statement, constScalarDeclarations, lastConstScalarDeclaration);
                    }
                    else
                    {
                        AppendToList(statement, constDeclarations, lastConstDeclaration);
                    }
                }
                else
                {
                    AppendToList(statement, declarations, lastDeclaration);
                }
            }
            else if (statement->nodeType == HLSLNodeType_Buffer)
            {
                AppendToList(statement, declarations, lastDeclaration);
            }
        }
        else if (statement->nodeType == HLSLNodeType_Function)
        {
            AppendToList(statement, functions, lastFunction);
        }
        else
        {
            AppendToList(statement, other, lastOther);
        }

        statement = nextStatement;
    }

    // Chain all the statements in the order that we want.
    HLSLStatement * firstStatement = constScalarDeclarations;
    HLSLStatement * lastStatement = lastConstScalarDeclaration;

    if (structs != NULL) {
        if (firstStatement == NULL) firstStatement = structs;
        else lastStatement->nextStatement = structs;
        lastStatement = lastStruct;
    }
    
    if (constDeclarations != NULL) {
        if (firstStatement == NULL) firstStatement = constDeclarations;
        else lastStatement->nextStatement = constDeclarations;
        lastStatement = lastConstDeclaration;
    }

    if (declarations != NULL) {
        if (firstStatement == NULL) firstStatement = declarations;
        else lastStatement->nextStatement = declarations;
        lastStatement = lastDeclaration;
    }

    if (functions != NULL) {
        if (firstStatement == NULL) firstStatement = functions;
        else lastStatement->nextStatement = functions;
        lastStatement = lastFunction;
    }

    if (other != NULL) {
        if (firstStatement == NULL) firstStatement = other;
        else lastStatement->nextStatement = other;
        lastStatement = lastOther;
    }

    root->statement = firstStatement;
}





// First and last can be the same.
void AddStatements(HLSLRoot * root, HLSLStatement * before, HLSLStatement * first, HLSLStatement * last)
{
    if (before == NULL) {
        last->nextStatement = root->statement;
        root->statement = first;
    }
    else {
        last->nextStatement = before->nextStatement;
        before->nextStatement = first;
    }
}

void AddSingleStatement(HLSLRoot * root, HLSLStatement * before, HLSLStatement * statement)
{
    AddStatements(root, before, statement, statement);
}


/* *X file releated
// @@ This is very game-specific. Should be moved to pipeline_parser or somewhere else.
void GroupParameters(HLSLTree * tree)
{
    // Sort parameters based on semantic and group them in cbuffers.

    HLSLRoot* root = tree->GetRoot();

    HLSLDeclaration * firstPerItemDeclaration = NULL;
    HLSLDeclaration * lastPerItemDeclaration = NULL;

    HLSLDeclaration * instanceDataDeclaration = NULL;

    HLSLDeclaration * firstPerPassDeclaration = NULL;
    HLSLDeclaration * lastPerPassDeclaration = NULL;

    HLSLDeclaration * firstPerItemSampler = NULL;
    HLSLDeclaration * lastPerItemSampler = NULL;

    HLSLDeclaration * firstPerPassSampler = NULL;
    HLSLDeclaration * lastPerPassSampler = NULL;

    HLSLStatement * statementBeforeBuffers = NULL;
    
    HLSLStatement* previousStatement = NULL;
    HLSLStatement* statement = root->statement;
    while (statement != NULL)
    {
        HLSLStatement* nextStatement = statement->nextStatement;

        if (statement->nodeType == HLSLNodeType_Struct) // Do not remove this, or it will mess the else clause below.
        {   
            statementBeforeBuffers = statement;
        }
        else if (statement->nodeType == HLSLNodeType_Declaration)
        {
            HLSLDeclaration* declaration = (HLSLDeclaration*)statement;

            // We insert buffers after the last const declaration.
            if ((declaration->type.flags & HLSLTypeFlag_Const) != 0)
            {
                statementBeforeBuffers = statement;
            }

            // Do not move samplers or static/const parameters.
            if ((declaration->type.flags & (HLSLTypeFlag_Static|HLSLTypeFlag_Const)) == 0)
            {
                // Unlink statement.
                statement->nextStatement = NULL;
                if (previousStatement != NULL) previousStatement->nextStatement = nextStatement;
                else root->statement = nextStatement;

                while(declaration != NULL)
                {
                    HLSLDeclaration* nextDeclaration = declaration->nextDeclaration;

                    if (declaration->semantic != NULL && String_EqualNoCase(declaration->semantic, "PER_INSTANCED_ITEM"))
                    {
                        ASSERT(instanceDataDeclaration == NULL);
                        instanceDataDeclaration = declaration;
                    }
                    else
                    {
                        // Select group based on type and semantic.
                        HLSLDeclaration ** first, ** last;
                        if (declaration->semantic == NULL || String_EqualNoCase(declaration->semantic, "PER_ITEM") || String_EqualNoCase(declaration->semantic, "PER_MATERIAL"))
                        {
                            if (IsSamplerType(declaration->type))
                            {
                                first = &firstPerItemSampler;
                                last = &lastPerItemSampler;
                            }
                            else
                            {
                                first = &firstPerItemDeclaration;
                                last = &lastPerItemDeclaration;
                            }
                        }
                        else
                        {
                            if (IsSamplerType(declaration->type))
                            {
                                first = &firstPerPassSampler;
                                last = &lastPerPassSampler;
                            }
                            else
                            {
                                first = &firstPerPassDeclaration;
                                last = &lastPerPassDeclaration;
                            }
                        }

                        // Add declaration to new list.
                        if (*first == NULL) *first = declaration;
                        else (*last)->nextStatement = declaration;
                        *last = declaration;
                    }

                    // Unlink from declaration list.
                    declaration->nextDeclaration = NULL;

                    // Reset attributes.
                    declaration->registerName = NULL;
                    //declaration->semantic = NULL;         // @@ Don't do this!

                    declaration = nextDeclaration;
                }
            }
        }
        //else
        //{
        //if (statementBeforeBuffers == NULL) {
        //        This is the location where we will insert our buffers.
        //        statementBeforeBuffers = previousStatement;
        //    }
        //}

        if (statement->nextStatement == nextStatement) {
            previousStatement = statement;
        }
        statement = nextStatement;
    }


    // Add instance data declaration at the end of the per_item buffer.
    if (instanceDataDeclaration != NULL)
    {
        if (firstPerItemDeclaration == NULL) firstPerItemDeclaration = instanceDataDeclaration;
        else lastPerItemDeclaration->nextStatement = instanceDataDeclaration;
    }


    // Add samplers.
    if (firstPerItemSampler != NULL) {
        AddStatements(root, statementBeforeBuffers, firstPerItemSampler, lastPerItemSampler);
        statementBeforeBuffers = lastPerItemSampler;
    }
    if (firstPerPassSampler != NULL) {
        AddStatements(root, statementBeforeBuffers, firstPerPassSampler, lastPerPassSampler);
        statementBeforeBuffers = lastPerPassSampler;
    }


    // @@ We are assuming per_item and per_pass buffers don't already exist. @@ We should assert on that.

    if (firstPerItemDeclaration != NULL)
    {
        // Create buffer statement.
        HLSLBuffer * perItemBuffer = tree->AddNode<HLSLBuffer>(firstPerItemDeclaration->fileName, firstPerItemDeclaration->line-1);
        perItemBuffer->name = tree->AddString("per_item");
        perItemBuffer->registerName = tree->AddString("b0");
        perItemBuffer->field = firstPerItemDeclaration;
        
        // Set declaration buffer pointers.
        HLSLDeclaration * field = perItemBuffer->field;
        while (field != NULL)
        {
            field->buffer = perItemBuffer;
            field = (HLSLDeclaration *)field->nextStatement;
        }

        // Add buffer to statements.
        AddSingleStatement(root, statementBeforeBuffers, perItemBuffer);
        statementBeforeBuffers = perItemBuffer;
    }

    if (firstPerPassDeclaration != NULL)
    {
        // Create buffer statement.
        HLSLBuffer * perPassBuffer = tree->AddNode<HLSLBuffer>(firstPerPassDeclaration->fileName, firstPerPassDeclaration->line-1);
        perPassBuffer->name = tree->AddString("per_pass");
        perPassBuffer->registerName = tree->AddString("b1");
        perPassBuffer->field = firstPerPassDeclaration;

        // Set declaration buffer pointers.
        HLSLDeclaration * field = perPassBuffer->field;
        while (field != NULL)
        {
            field->buffer = perPassBuffer;
            field = (HLSLDeclaration *)field->nextStatement;
        }
        
        // Add buffer to statements.
        AddSingleStatement(root, statementBeforeBuffers, perPassBuffer);
    }
}
*/

class FindArgumentVisitor : public HLSLTreeVisitor
{
public:
    bool found;
    const char * name;

    virtual ~FindArgumentVisitor() {}
    
	FindArgumentVisitor()
	{
		found = false;
		name  = NULL;
	}

    bool FindArgument(const char * _name, HLSLFunction * function)
    {
        found = false;
        name = _name;
        VisitStatements(function->statement);
        return found;
    }
    
    virtual void VisitStatements(HLSLStatement * statement) override
    {
        while (statement != NULL && !found)
        {
            VisitStatement(statement);
            statement = statement->nextStatement;
        }
    }

    virtual void VisitIdentifierExpression(HLSLIdentifierExpression * node) override
    {
        if (node->name == name)
        {
            found = true;
        }
    }
};


void HideUnusedArguments(HLSLFunction * function)
{
    FindArgumentVisitor visitor;
 
    // For each argument.
    HLSLArgument * arg = function->argument;
    while (arg != NULL)
    {
        if (!visitor.FindArgument(arg->name, function))
        {
            arg->hidden = true;
        }

        arg = arg->nextArgument;
    }
}

bool NeedsFlattening(HLSLExpression * expr, int level = 0) {
    if (expr == NULL) {
        return false;
    }
    if (expr->nodeType == HLSLNodeType_UnaryExpression) {
        HLSLUnaryExpression * unaryExpr = (HLSLUnaryExpression *)expr;
        return NeedsFlattening(unaryExpr->expression, level+1) || NeedsFlattening(expr->nextExpression, level);
    }
    else if (expr->nodeType == HLSLNodeType_BinaryExpression) {
        HLSLBinaryExpression * binaryExpr = (HLSLBinaryExpression *)expr;
        if (IsAssignOp(binaryExpr->binaryOp)) {
            return NeedsFlattening(binaryExpr->expression2, level+1) || NeedsFlattening(expr->nextExpression, level);
        }
        else {
            return NeedsFlattening(binaryExpr->expression1, level+1) || NeedsFlattening(binaryExpr->expression2, level+1) || NeedsFlattening(expr->nextExpression, level);
        }
    }
    else if (expr->nodeType == HLSLNodeType_ConditionalExpression) {
        HLSLConditionalExpression * conditionalExpr = (HLSLConditionalExpression *)expr;
        return NeedsFlattening(conditionalExpr->condition, level+1) || NeedsFlattening(conditionalExpr->trueExpression, level+1) || NeedsFlattening(conditionalExpr->falseExpression, level+1) || NeedsFlattening(expr->nextExpression, level);
    }
    else if (expr->nodeType == HLSLNodeType_CastingExpression) {
        HLSLCastingExpression * castingExpr = (HLSLCastingExpression *)expr;
        return NeedsFlattening(castingExpr->expression, level+1) || NeedsFlattening(expr->nextExpression, level);
    }
    else if (expr->nodeType == HLSLNodeType_LiteralExpression) {
        return NeedsFlattening(expr->nextExpression, level);
    }
    else if (expr->nodeType == HLSLNodeType_IdentifierExpression) {
        return NeedsFlattening(expr->nextExpression, level);
    }
    else if (expr->nodeType == HLSLNodeType_ConstructorExpression) {
        HLSLConstructorExpression * constructorExpr = (HLSLConstructorExpression *)expr;
        return NeedsFlattening(constructorExpr->argument, level+1) || NeedsFlattening(expr->nextExpression, level);
    }
    else if (expr->nodeType == HLSLNodeType_MemberAccess) {
        return NeedsFlattening(expr->nextExpression, level+1);
    }
    else if (expr->nodeType == HLSLNodeType_ArrayAccess) {
        HLSLArrayAccess * arrayAccess = (HLSLArrayAccess *)expr;
        return NeedsFlattening(arrayAccess->array, level+1) || NeedsFlattening(arrayAccess->index, level+1) || NeedsFlattening(expr->nextExpression, level);
    }
    else if (expr->nodeType == HLSLNodeType_FunctionCall) {
        HLSLFunctionCall * functionCall = (HLSLFunctionCall *)expr;
        if (functionCall->function->numOutputArguments > 0) {
            if (level > 0) {
                return true;
            }
        }
        return NeedsFlattening(functionCall->argument, level+1) || NeedsFlattening(expr->nextExpression, level);
    }
    else {
        //assert(false);
        return false;
    }
}


struct StatementList {
    HLSLStatement * head = NULL;
    HLSLStatement * tail = NULL;
    void append(HLSLStatement * st) {
        if (head == NULL) {
            tail = head = st;
        }
        tail->nextStatement = st;
        tail = st;
    }
};


    class ExpressionFlattener : public HLSLTreeVisitor
    {
    public:
        HLSLTree * m_tree;
        int tmp_index;
        HLSLStatement ** statement_pointer;
        HLSLFunction * current_function;
        
        ExpressionFlattener()
        {
            m_tree = NULL;
            tmp_index = 0;
            statement_pointer = NULL;
            current_function = NULL;
        }
        virtual ~ExpressionFlattener() {}
        
        void FlattenExpressions(HLSLTree * tree)
        {
            m_tree = tree;
            VisitRoot(tree->GetRoot());
        }

        // Visit all statements updating the statement_pointer so that we can insert and replace statements. @@ Add this to the default visitor?
        virtual void VisitFunction(HLSLFunction * node) override
        {
            current_function = node;
            statement_pointer = &node->statement;
            VisitStatements(node->statement);
            statement_pointer = NULL;
            current_function = NULL;
        }

        virtual void VisitComment(HLSLComment * node) override
        {
            // TODO: do nothing?
        }
        
        virtual void VisitIfStatement(HLSLIfStatement * node) override
        {
            if (NeedsFlattening(node->condition, 1)) {
                assert(false);  // @@ Add statements before if statement.
            }
            
            statement_pointer = &node->statement;
            VisitStatements(node->statement);
            if (node->elseStatement) {
                statement_pointer = &node->elseStatement;
                VisitStatements(node->elseStatement);
            }
        }
        
        virtual void VisitForStatement(HLSLForStatement * node) override
        {
            if (NeedsFlattening(node->initialization->assignment, 1)) {
                assert(false);  // @@ Add statements before for statement.
            }
            if (NeedsFlattening(node->condition, 1) || NeedsFlattening(node->increment, 1)) {
                assert(false);  // @@ These are tricky to implement. Need to handle all loop exits.
            }

            statement_pointer = &node->statement;
            VisitStatements(node->statement);
        }
        
        virtual void VisitBlockStatement(HLSLBlockStatement * node) override
        {
            statement_pointer = &node->statement;
            VisitStatements(node->statement);
        }
        
        virtual void VisitStatements(HLSLStatement * statement) override
        {
            while (statement != NULL) {
                VisitStatement(statement);
                statement_pointer = &statement->nextStatement;
                statement = statement->nextStatement;
            }
        }

        // This is usually a function call or assignment.
        virtual void VisitExpressionStatement(HLSLExpressionStatement * node) override
        {
            if (NeedsFlattening(node->expression, 0))
            {
                StatementList statements;
                Flatten(node->expression, statements, false);
                
                // Link beginning of statement list.
                *statement_pointer = statements.head;

                // Link end of statement list.
                HLSLStatement * tail = statements.tail;
                tail->nextStatement = node->nextStatement;
                
                // Update statement pointer.
                statement_pointer = &tail->nextStatement;
                
                // @@ Delete node?
            }
        }

        virtual void VisitDeclaration(HLSLDeclaration * node) override
        {
            // Skip global declarations.
            if (statement_pointer == NULL) return;
            
            if (NeedsFlattening(node->assignment, 1))
            {
                StatementList statements;
                HLSLIdentifierExpression * ident = Flatten(node->assignment, statements, true);
                
                // @@ Delete node->assignment?
                
                node->assignment = ident;
                statements.append(node);
                
                // Link beginning of statement list.
                *statement_pointer = statements.head;
                
                // Link end of statement list.
                HLSLStatement * tail = statements.tail;
                tail->nextStatement = node->nextStatement;
                
                // Update statement pointer.
                statement_pointer = &tail->nextStatement;
            }
        }

        virtual void VisitReturnStatement(HLSLReturnStatement * node) override
        {
            if (NeedsFlattening(node->expression, 1))
            {
                StatementList statements;
                HLSLIdentifierExpression * ident = Flatten(node->expression, statements, true);

                // @@ Delete node->expression?
                
                node->expression = ident;
                statements.append(node);
                
                // Link beginning of statement list.
                *statement_pointer = statements.head;
                
                // Link end of statement list.
                HLSLStatement * tail = statements.tail;
                tail->nextStatement = node->nextStatement;
                
                // Update statement pointer.
                statement_pointer = &tail->nextStatement;
            }
        }

        
        HLSLDeclaration * BuildTemporaryDeclaration(HLSLExpression * expr)
        {
            assert(expr->expressionType.baseType != HLSLBaseType_Void);
            
            HLSLDeclaration * declaration = m_tree->AddNode<HLSLDeclaration>(expr->fileName, expr->line);
            declaration->name = m_tree->AddStringFormat("tmp%d", tmp_index++);
            declaration->type = expr->expressionType;
            declaration->assignment = expr;
            
            //HLSLIdentifierExpression * ident = (HLSLIdentifierExpression *)expr;
            
            return declaration;
        }

        HLSLExpressionStatement * BuildExpressionStatement(HLSLExpression * expr)
        {
            HLSLExpressionStatement * statement = m_tree->AddNode<HLSLExpressionStatement>(expr->fileName, expr->line);
            statement->expression = expr;
            return statement;
        }

        HLSLIdentifierExpression * AddExpressionStatement(HLSLExpression * expr, StatementList & statements, bool wantIdent)
        {
            if (wantIdent) {
                HLSLDeclaration * declaration = BuildTemporaryDeclaration(expr);
                statements.append(declaration);
                
                HLSLIdentifierExpression * ident = m_tree->AddNode<HLSLIdentifierExpression>(expr->fileName, expr->line);
                ident->name = declaration->name;
                ident->expressionType = declaration->type;
                return ident;
            }
            else {
                HLSLExpressionStatement * statement = BuildExpressionStatement(expr);
                statements.append(statement);
                return NULL;
            }
        }
        
        HLSLIdentifierExpression * Flatten(HLSLExpression * expr, StatementList & statements, bool wantIdent = true)
        {
            if (!NeedsFlattening(expr, wantIdent)) {
                return AddExpressionStatement(expr, statements, wantIdent);
            }
            
            if (expr->nodeType == HLSLNodeType_UnaryExpression) {
                assert(expr->nextExpression == NULL);
                
                HLSLUnaryExpression * unaryExpr = (HLSLUnaryExpression *)expr;
                
                HLSLIdentifierExpression * tmp = Flatten(unaryExpr->expression, statements, true);
                
                HLSLUnaryExpression * newUnaryExpr = m_tree->AddNode<HLSLUnaryExpression>(unaryExpr->fileName, unaryExpr->line);
                newUnaryExpr->unaryOp = unaryExpr->unaryOp;
                newUnaryExpr->expression = tmp;
                newUnaryExpr->expressionType = unaryExpr->expressionType;

                return AddExpressionStatement(newUnaryExpr, statements, wantIdent);
            }
            else if (expr->nodeType == HLSLNodeType_BinaryExpression) {
                assert(expr->nextExpression == NULL);
                
                HLSLBinaryExpression * binaryExpr = (HLSLBinaryExpression *)expr;
                
                if (IsAssignOp(binaryExpr->binaryOp)) {
                    // Flatten right hand side only.
                    HLSLIdentifierExpression * tmp2 = Flatten(binaryExpr->expression2, statements, true);
                    
                    HLSLBinaryExpression * newBinaryExpr = m_tree->AddNode<HLSLBinaryExpression>(binaryExpr->fileName, binaryExpr->line);
                    newBinaryExpr->binaryOp = binaryExpr->binaryOp;
                    newBinaryExpr->expression1 = binaryExpr->expression1;
                    newBinaryExpr->expression2 = tmp2;
                    newBinaryExpr->expressionType = binaryExpr->expressionType;
                    
                    return AddExpressionStatement(newBinaryExpr, statements, wantIdent);
                }
                else {
                    HLSLIdentifierExpression * tmp1 = Flatten(binaryExpr->expression1, statements, true);
                    HLSLIdentifierExpression * tmp2 = Flatten(binaryExpr->expression2, statements, true);

                    HLSLBinaryExpression * newBinaryExpr = m_tree->AddNode<HLSLBinaryExpression>(binaryExpr->fileName, binaryExpr->line);
                    newBinaryExpr->binaryOp = binaryExpr->binaryOp;
                    newBinaryExpr->expression1 = tmp1;
                    newBinaryExpr->expression2 = tmp2;
                    newBinaryExpr->expressionType = binaryExpr->expressionType;
                    
                    return AddExpressionStatement(newBinaryExpr, statements, wantIdent);
                }
            }
            else if (expr->nodeType == HLSLNodeType_ConditionalExpression) {
                assert(false);
            }
            else if (expr->nodeType == HLSLNodeType_CastingExpression) {
                assert(false);
            }
            else if (expr->nodeType == HLSLNodeType_LiteralExpression) {
                assert(false);
            }
            else if (expr->nodeType == HLSLNodeType_IdentifierExpression) {
                assert(false);
            }
            else if (expr->nodeType == HLSLNodeType_ConstructorExpression) {
                assert(false);
            }
            else if (expr->nodeType == HLSLNodeType_MemberAccess) {
                assert(false);
            }
            else if (expr->nodeType == HLSLNodeType_ArrayAccess) {
                assert(false);
            }
            else if (expr->nodeType == HLSLNodeType_FunctionCall) {
                HLSLFunctionCall * functionCall = (HLSLFunctionCall *)expr;

                // @@ Output function as is?
                // @@ We have to flatten function arguments! This is tricky, need to handle input/output arguments.
                assert(!NeedsFlattening(functionCall->argument));
                
                return AddExpressionStatement(expr, statements, wantIdent);
            }
            else {
                assert(false);
            }
            return NULL;
        }
    };

    
void FlattenExpressions(HLSLTree* tree) {
    ExpressionFlattener flattener;
    flattener.FlattenExpressions(tree);
}

} // M4

