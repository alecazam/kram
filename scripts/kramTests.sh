#/bin/zsh

../scripts/kramTextures.py -p mac --ktx2 --bundle  
../scripts/kramTextures.py -p mac --bundle  

../scripts/kramTextures.py -p ios --ktx2 --bundle  
../scripts/kramTextures.py -p ios --bundle  

# this takes 15s+ with ETC2comp
../scripts/kramTextures.py -p android --ktx2 --bundle  
../scripts/kramTextures.py -p android --bundle  

# this only has ktx2 form, tests uastc which kram doesn't open/save yet
#../scripts/kramTextures.py -p any --ktx2 --bundle  


