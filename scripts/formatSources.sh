#!/bin/zsh 

# use the app/clang_format to only process sources in app directory
# eventually replace with git hook.  This script only runs on Posix.

pushd ../source/kram
clang-format -style=file -i *.cpp
clang-format -style=file -i *.h
popd
