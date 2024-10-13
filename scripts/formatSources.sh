#!/bin/zsh 

# use the app/clang_format to only process sources in app directory
# eventually replace with git hook.  This script only runs on Posix.

# pushd ../libkram/kram
# clang-format -style=file -i Kram*.cpp
# clang-format -style=file -i Kram*.h
# clang-format -style=file -i KTX*.cpp
# clang-format -style=file -i KTX*.h
# popd

# pushd ../kramv
# clang-format -style=file -i Kram*.cpp
# clang-format -style=file -i Kram*.h
# clang-format -style=file -i Kram*.mm
# popd


# hope that the ignore file does so
#pushd ../libkram/kram
#clang-format -style=file -i *.*
#popd

#pushd ../libkram
# this doesn't seem to honor the ignore file
# find ../libkram -iname '*.h' -o -iname '*.cpp' | xargs clang-format -i
#popd

# no recursion for clang-format
pushd ../libkram
clang-format -style=file -i kram/*.(cpp|h)
clang-format -style=file -i vectormath/*.(cpp|h)
popd

# pushd ..
# clang-format -style=file -i kramv/*.*
# clang-format -style=file -i kramc/*.*
# clang-format -style=file -i kram-thumb/*.(cpp|h)
# clang-format -style=file -i kram-thumb-win/*.*
# clang-format -style=file -i kram-profile/*.*
# clang-format -style=file -i kram-preview/*.*
# clang-format -style=file -i hlslparser/*.*
# popd