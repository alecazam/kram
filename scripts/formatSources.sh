#!/bin/zsh 

# use the app/clang_format to only process sources in app directory
# eventually replace with git hook.  This script only runs on Posix.

pushd ../libkram/kram
clang-format -style=file -i Kram*.cpp
clang-format -style=file -i Kram*.h
clang-format -style=file -i KTX*.cpp
clang-format -style=file -i KTX*.h
popd

pushd ../kramv
clang-format -style=file -i Kram*.cpp
clang-format -style=file -i Kram*.h
clang-format -style=file -i Kram*.mm
popd