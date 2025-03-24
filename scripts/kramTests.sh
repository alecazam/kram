#!/bin/zsh

# return all args from $1 onward
# can pass --force, -c ktx, -c dds, -c ktx2
args=$@

../scripts/kramTextures.py -p mac --bundle ${args}
#../scripts/kramTextures.py -p mac -c ktx --bundle ${args}

../scripts/kramTextures.py -p ios --bundle ${args}
#../scripts/kramTextures.py -p ios -c ktx --bundle ${args} 

# this takes 15s+ with ETC2comp
../scripts/kramTextures.py -p android --bundle ${args}
#../scripts/kramTextures.py -p android -c ktx android --bundle ${args}

# this only has ktx2 form, tests uastc which kram doesn't open/save yet
#../scripts/kramTextures.py -p any --bundle ${args}


