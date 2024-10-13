//=============================================================================
//
// Render/CodeWriter.h
//
// Created by Max McGuire (max@unknownworlds.com)
// Copyright (c) 2013, Unknown Worlds Entertainment, Inc.
//
//=============================================================================

#pragma once

#include "Engine.h"

// stl
#include <string>

namespace M4 {

class Allocator;

/**
 * This class is used for outputting code. It handles indentation and inserting #line markers
 * to match the desired output line numbers.
 */
class CodeWriter {
public:
    CodeWriter();

    void SetWriteFileLine(bool enable) { m_writeFileLine = enable; }

    void BeginLine(int indent, const char* fileName = NULL, int lineNumber = -1);
    void Write(const char* format, ...) M4_PRINTF_ATTR(2, 3);
    int EndLine(const char* text = NULL);

    void WriteLine(int indent, const char* format, ...) M4_PRINTF_ATTR(3, 4);
    void WriteLineTagged(int indent, const char* fileName, int lineNumber, const char* format, ...) M4_PRINTF_ATTR(5, 6);

    const char* GetResult() const;
    void Reset();

private:
    std::string m_buffer;
    int m_currentLine;
    const char* m_currentFileName;
    int m_spacesPerIndent;
    int m_currentIndent;
    bool m_writeFileLine;
};

} //namespace M4
