# kram-thumb-win.dll

Windows thumbnailer for DDS/KTX/KTX2 containers in C++.  To use the thumbnailer:

* Go to build or bin folder.  
* Install "regsvr32.exe kram-thumb-win.dll".  
* Uninstall "regsvr32.exe /u kram-thumb-win.dll"

# About kram thumbnailer

The thumbnailer dll runs the same libkram decoders that kramv thumbnailer uses for macOS.  An ancient Win7 thumbnil service calls over to the dll.  The Microsoft service harkens back to Win7, was last updated in vista, and their sample didn't work off github.  So thankfully a dev on github can cleaned all this up. 

A sanitized stream of bytes is supplied by the Explorer thumbnail service to the dll, the dll uses libkram to decode the image container to a single image, and returns the closest mip as a bitmap to the service.  The bitmap is assumed to be sRGB, but there are few details or settings.  Explorer caches the thumbnails.  Windows also seems to generate thumbnails when apps are tied to specific extensions.

For some reason, Microsoft doesn't upscale small 2x2 thumbnails.  These show up as barely visible dots despite a request for a 32x32 pixel.  macOS does upscale these so they are viewable.

These are the default thumbnail sizes that are subject to change.  Note that Microsoft bases dpi off multiples of 96, where macOS uses 72.

* 32x32
* 96x96
* 256x256
* 1024x1024

Adapted from Microsoft sample code that iOrange cleaned to generate thumbnails for QOI images. 

https://github.com/iOrange/QOIThumbnailProvider

This code doesn't work and the ability to run this as an exe don't seem to be present.

https://learn.microsoft.com/en-us/samples/microsoft/windows-classic-samples/recipethumbnailprovider/

