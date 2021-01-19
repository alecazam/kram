#/bin/zsh

../scripts/kramTextures.py -p android --ktx2 --bundle  
../scripts/kramTextures.py -p ios --ktx2 --bundle  
../scripts/kramTextures.py -p mac --ktx2 --bundle  

../scripts/kramTextures.py -p android --bundle  
../scripts/kramTextures.py -p ios --bundle  
../scripts/kramTextures.py -p mac --bundle  

