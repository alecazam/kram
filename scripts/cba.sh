#!/bin/zsh

# so can use aliases, why isn't this automatic?
source ~/.zshrc

# Note this part will change depending on build type, etc.  TODO: see if can obtain from xcodebuild
# or could force projects to use fixed output folder
# erbkczkopelnfhennypqjfnicqai

ClangBuildAnalyzer --all ~/Library/Developer/Xcode/DerivedData/kram-erbkczkopelnfhennypqjfnicqai/Build/Intermediates.noindex/kram.build/Debug/kram.build/Objects-normal/arm64 ClangBuildAnalysisPre.dat

ClangBuildAnalyzer --analyze ClangBuildAnalysisPre.dat > ClangBuildAnalysis.txt

subl ClangBuildAnalysis.txt

