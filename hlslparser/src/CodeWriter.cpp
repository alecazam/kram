//=============================================================================
//
// Render/CodeWriter.cpp
//
// Created by Max McGuire (max@unknownworlds.com)
// Copyright (c) 2013, Unknown Worlds Entertainment, Inc.
//
//=============================================================================

#include "Engine.h"

#include "CodeWriter.h"

#include <stdarg.h>

namespace M4
{
CodeWriter::CodeWriter()
{
    m_currentLine       = 1;
    m_currentFileName   = NULL;
    m_spacesPerIndent   = 4;
    m_writeFileLine     = false;
}

void CodeWriter::BeginLine(int indent, const char* fileName, int lineNumber)
{
    // probably missing an EndLine
    ASSERT(m_currentIndent == 0);
    
    if (m_writeFileLine)
    {
        bool outputLine = false;
        bool outputFile = false;

        // Output a line number pragma if necessary.
        if (fileName != NULL && m_currentFileName != fileName)
        {
            m_currentFileName = fileName;
            fileName = m_currentFileName;
            outputFile = true;
        }
        if (lineNumber != -1 && m_currentLine != lineNumber)
        {
            m_currentLine = lineNumber;
            outputLine = true;
        }

        // if previous filename is same, only output line
        if (outputFile)
        {
            String_Printf(m_buffer, "#line %d \"%s\"\n", lineNumber, fileName);
        }
        else if (outputLine)
        {
            String_Printf(m_buffer, "#line %d\n", lineNumber);
        }
    }

    // Handle the indentation.
    if (indent)
        Write("%*s", indent * m_spacesPerIndent, "");
    
    m_currentIndent = indent;
    
}

int CodeWriter::EndLine(const char* text)
{
    if (text != NULL)
    {
        m_buffer += text;
    }
    m_buffer += "\n";
    ++m_currentLine;
    
    // so can EndLine/BeginLine
    int indent = m_currentIndent;
    m_currentIndent = 0;
    return indent;
}

void CodeWriter::Write(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    int result = String_PrintfArgList(m_buffer, format, args);
    ASSERT(result != -1);
    va_end(args);
}

void CodeWriter::WriteLine(int indent, const char* format, ...)
{
    if (indent)
        Write("%*s", indent * m_spacesPerIndent, "");
    
    va_list args;
    va_start(args, format);
    int result = String_PrintfArgList(m_buffer, format, args);
    ASSERT(result != -1);
    va_end(args);
    
    EndLine();
}

void CodeWriter::WriteLineTagged(int indent, const char* fileName, int lineNumber, const char* format, ...)
{
    // TODO: this should make sure that line isn't already Begu
    BeginLine(indent, fileName, lineNumber);
    
    va_list args;
    va_start(args, format);
    int result = String_PrintfArgList(m_buffer, format, args);
    ASSERT(result != -1);
    va_end(args);
        
    EndLine();
}

const char* CodeWriter::GetResult() const
{
    return m_buffer.c_str();
}

void CodeWriter::Reset()
{
    m_buffer.clear();
}

}
