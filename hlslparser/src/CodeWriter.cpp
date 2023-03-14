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

static const int _maxLineLength = 4096;

CodeWriter::CodeWriter(bool writeFileNames)
{
    m_currentLine       = 1;
    m_currentFileName   = NULL;
    m_spacesPerIndent   = 4;
    m_writeLines        = true;
    m_writeFileNames    = writeFileNames;
}

void CodeWriter::BeginLine(int indent, const char* fileName, int lineNumber)
{
    // probably missing an EndLine
    ASSERT(m_currentIndent == 0);
    
    if (m_writeLines)
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

        /* TODO: Alec, removed this for now
            This writes in #line directives back to orignal source files
         
        if (outputLine || outputFile)
        {
            char buffer[256];
            String_Printf(buffer, sizeof(buffer), "#line %d", lineNumber);
            m_buffer += buffer;
            if (outputFile && m_writeFileNames)
            {
                m_buffer += " \"";
                m_buffer += fileName;
                m_buffer += "\"\n";
            }
            else
            {
                m_buffer += "\n";
            }
        }
        */
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

    char buffer[_maxLineLength];
    String_PrintfArgList(buffer, sizeof(buffer), format, args);

    m_buffer += buffer;

    va_end(args);      
}

void CodeWriter::WriteLine(int indent, const char* format, ...)
{
    va_list args;
    va_start(args, format);

    char buffer[_maxLineLength];

    int result = String_PrintfArgList(buffer, sizeof(buffer), format, args);
    ASSERT(result != -1);

    for (int i = 0; i < indent * m_spacesPerIndent; ++i)
    {
        m_buffer += " ";
    }
    m_buffer += buffer;

    EndLine();

    va_end(args);        
}

void CodeWriter::WriteLineTagged(int indent, const char* fileName, int lineNumber, const char* format, ...)
{
    va_list args;
    va_start(args, format);

    BeginLine(indent, fileName, lineNumber);

    char buffer[_maxLineLength];
    int result = String_PrintfArgList(buffer, sizeof(buffer), format, args);
    ASSERT(result != -1);

    m_buffer += buffer;

    EndLine();

    va_end(args);        
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
