// Clang Build Analyzer https://github.com/aras-p/ClangBuildAnalyzer
// SPDX-License-Identifier: Unlicense
#pragma once

#include <string>

#include "BuildEvents.h"

void DoAnalysis(const BuildEvents& events, BuildNames& names, std::string& out);
