// kram - Copyright 2020 by Alec Miller. - MIT License
// The license and copyright notice shall be included
// in all copies or substantial portions of the Software.

#import <TargetConditionals.h>

#if __has_feature(modules)
@import Cocoa;
@import Metal;
@import MetalKit;
@import simd;
#else
#import <Cocoa/Cocoa.h>
#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>
#include <simd/simd.h>
#endif

#import "KramRenderer.h"
#import "KramShaders.h"
#include "KramLog.h"
#include "KTXMipper.h"
#include "KramImage.h"
#include "KramViewerBase.h"
#include "KramVersion.h" // keep kramv version in sync with libkram

#ifdef NDEBUG
static bool doPrintPanZoom = false;
#else
static bool doPrintPanZoom = true;
#endif

using namespace simd;
using namespace kram;

//-------------

@interface MyMTKView : MTKView
// for now only have a single imageURL
@property (retain, nonatomic, readwrite, nullable) NSURL* imageURL;

//@property (nonatomic, readwrite, nullable) NSPanGestureRecognizer* panGesture;
@property (retain, nonatomic, readwrite, nullable) NSMagnificationGestureRecognizer* zoomGesture;

- (BOOL)loadTextureFromURL:(NSURL*)url;

@end


//-------------

@interface AppDelegate : NSObject <NSApplicationDelegate>

- (IBAction)showAboutDialog:(id)sender;

@end

@interface AppDelegate ()

@end

@implementation AppDelegate


- (void)applicationDidFinishLaunching:(NSNotification *)aNotification {
    // Insert code here to initialize your application
    
}


- (void)applicationWillTerminate:(NSNotification *)aNotification {
    // Insert code here to tear down your application
}


- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)sender {
    return YES;
}

- (void)application:(NSApplication *)sender openURLs:(nonnull NSArray<NSURL *> *)urls
{
    // see if this is called
    NSLog(@"OpenURLs");
    
    // this is called from "Open In...", and also from OpenRecent documents menu
    MyMTKView* view = sender.mainWindow.contentView;
    
    NSURL *url = urls.firstObject;
    [view loadTextureFromURL:url];
}


- (IBAction)showAboutDialog:(id)sender {
    // calls openDocumentWithContentsOfURL above
    NSMutableDictionary<NSAboutPanelOptionKey, id>* options = [[NSMutableDictionary alloc] init];

    // name and icon are already supplied

    // want to embed the git tag here
    options[@"Copyright"] =  [NSString stringWithUTF8String:
        "kram ©2020,2021 by Alec Miller"
    ];
    
    // add a link to kram website, skip the Visit text
    NSMutableAttributedString* str = [[NSMutableAttributedString alloc] initWithString:@"https://github.com/alecazam/kram"];
    [str addAttribute: NSLinkAttributeName value: @"https://github.com/alecazam/kram" range: NSMakeRange(0, str.length)];

    [str appendAttributedString: [[NSAttributedString alloc] initWithString:[NSString stringWithUTF8String:
        "\n"
        "kram is open-source and inspired by the\n"
        "software technologies of these companies\n"
        "  Khronos, Binomial, ARM, Google, and Apple\n"
        "and devs who generously shared their work.\n"
        "  Simon Brown, Rich Geldreich, Pete Harris,\n"
        "  Philip Rideout, Romain Guy, Colt McAnlis,\n"
        "  John Ratcliff, Sean Parent, David Ireland,\n"
        "  Mark Callow, Mike Frysinger, Yann Collett\n"
    ]]];
    
    options[NSAboutPanelOptionCredits] = str;
     
     
    // skip the v character
    const char* version = KRAM_VERSION;
    version += 1;
    
    // this is the build version, should be github hash?
    options[NSAboutPanelOptionVersion] = @"";
    
    // this is app version
    options[NSAboutPanelOptionApplicationVersion] = [NSString stringWithUTF8String:
        version
    ];

    [[NSApplication sharedApplication] orderFrontStandardAboutPanelWithOptions:options];
    
    //[[NSApplication sharedApplication] orderFrontStandardAboutPanel:sender];
}

@end





//-------------

enum Key {
    A                    = 0x00,
    S                    = 0x01,
    D                    = 0x02,
    F                    = 0x03,
    H                    = 0x04,
    G                    = 0x05,
    Z                    = 0x06,
    X                    = 0x07,
    C                    = 0x08,
    V                    = 0x09,
    B                    = 0x0B,
    Q                    = 0x0C,
    W                    = 0x0D,
    E                    = 0x0E,
    R                    = 0x0F,
    Y                    = 0x10,
    T                    = 0x11,
    O                    = 0x1F,
    U                    = 0x20,
    I                    = 0x22,
    P                    = 0x23,
    L                    = 0x25,
    J                    = 0x26,
    K                    = 0x28,
    N                    = 0x2D,
    M                    = 0x2E,

    // https://eastmanreference.com/complete-list-of-applescript-key-codes
    Zero                 = 0x1D,
    
    LeftBrace            = 0x21,
    RightBrace           = 0x1E,

    LeftBracket          = 0x21,
    RightBracket         = 0x1E,

    Quote                = 0x27,
    Semicolon            = 0x29,
    Backslash            = 0x2A,
    Comma                = 0x2B,
    Slash                = 0x2C,
    
    LeftArrow            = 0x7B,
    RightArrow           = 0x7C,
    DownArrow            = 0x7D,
    UpArrow              = 0x7E,
};

/*
 
// This is meant to advance a given image through a variety of encoder formats.
// Then can compare the encoding results and pick the better one.
// This could help artist see the effect on all mips of an encoder choice, and dial up/down the setting.
// Would this cycle through astc and bc together, or separately.
MyMTLPixelFormat encodeSrcTextureAsFormat(MyMTLPixelFormat currentFormat, bool increment) {
    // if dev drops a png, then have a mode to see different encoding styles
    // on normals, it would just be BC5, ETCrg, ASTCla and blocks
    #define findIndex(array, x) \
        for (int32_t i = 0, count = sizeof(array); i < count; ++i) { \
            if (array[i] == x) { \
                int32_t index = i; \
                if (increment) \
                    index = (index + 1) % count; \
                else \
                    index = (index + count - 1) % count; \
                newFormat = array[index]; \
                break; \
            } \
        }
 
    MyMTLPixelFormat newFormat = currentFormat;
    
    // these are formats to cycle through
    MyMTLPixelFormat bc[]     = { MyMTLPixelFormatBC7_RGBAUnorm, MyMTLPixelFormatBC3_RGBA, MyMTLPixelFormatBC1_RGBA };
    MyMTLPixelFormat bcsrgb[] = { MyMTLPixelFormatBC7_RGBAUnorm_sRGB, MyMTLPixelFormatBC3_RGBA_sRGB, MyMTLPixelFormatBC1_RGBA_sRGB };
    
    // TODO: support non-square block with astcenc
    MyMTLPixelFormat astc[]     = { MyMTLPixelFormatASTC_4x4_LDR, MyMTLPixelFormatASTC_5x5_LDR, MyMTLPixelFormatASTC_6x6_LDR, MyMTLPixelFormatASTC_8x8_LDR };
    MyMTLPixelFormat astcsrgb[] = { MyMTLPixelFormatASTC_4x4_sRGB, MyMTLPixelFormatASTC_5x5_sRGB, MyMTLPixelFormatASTC_6x6_sRGB, MyMTLPixelFormatASTC_8x8_sRGB };
    MyMTLPixelFormat astchdr[]  = { MyMTLPixelFormatASTC_4x4_HDR, MyMTLPixelFormatASTC_5x5_HDR, MyMTLPixelFormatASTC_6x6_HDR, MyMTLPixelFormatASTC_8x8_HDR };
 
    if (isASTCFormat(currentFormat)) {
        if (isHDRFormat(currentFormat)) {
            // skip it, need hdr decode for Intel
            // findIndex(astchdr, currentFormat);
        }
        else if (isSrgbFormat(currentFormat)) {
            findIndex(astcsrgb, currentFormat);
        }
        else {
            findIndex(astc, currentFormat);
        }
    }
    else if (isBCFormat(currentFormat)) {
        if (isHDRFormat(currentFormat)) {
            // skip it for now, bc6h
        }
        else if (isSrgbFormat(currentFormat)) {
            findIndex(bcsrgb, currentFormat);
        }
        else {
            findIndex(bc, currentFormat);
        }
    }
    
    #undef findIndex
    
    return newFormat;
}

void encodeSrcForEncodeComparisons(bool increment) {
    auto newFormat = encodeSrcTextureAsFormat(displayedFormat, increment);
    
     // TODO: have to encode and then decode astc on macOS-Intel
     // load png and keep it around, and then call encode and then diff the image against the original pixels
     // 565 will always differ from the original.
     
     // Once encode generated, then cache result, only ever display two textures
     // in a comparison mode. Also have eyedropper sample from second texture
     // could display src vs. encode, or cached textures against each other
     // have PSNR too ?
     
    // see if the format is in cache
    
    // encode incremented format and cache, that way don't wait too long
    // and once all encode formats generated, can cycle through them until next image loaded
    
    //KTXImage image;
    //image.open(...);
    //image.encode();
    //decodeIfNeeded(...);
    //comparisonTexture = [createImage:image];
    //set that onto the shader to diff against after recontruct
    
    // this format after decode may not be the same
    displayedFormat = newFormat;
}
*/

// also NSPasteboardTypeURL
// also NSPasteboardTypeTIFF
NSArray<NSString*>* pasteboardTypes = @[
    NSPasteboardTypeFileURL
];


@implementation MyMTKView
{
    NSTextField* _hudLabel;
    NSTextField* _hudLabel2;
    vector<string> _textSlots;
    ShowSettings* _showSettings;
}

- (void)awakeFromNib
{
    [super awakeFromNib];
    
    // TODO: see if can only open this
    // NSLog(@"AwakeFromNIB");
}

// to get upper left origin like on UIView
#if KRAM_MAC
-(BOOL)isFlipped
{
    return YES;
}
#endif

// Gesture regognizers are so annoying. pan requires click on touchpad
// when I just want to swipe.  Might only use for zoom.  This works
// nothing like Apple Preview, so they must not be using these.
// Also zoom recognizer jumps instead of smooth pan in/out.
// Could mix pinch-zoom with wheel-based pan.
//
// TODO: Sometimes getting panels from right side popping in when trying to pan on macOS
// without using pan gesture.

- (instancetype)initWithCoder:(NSCoder*)coder {
    self = [super initWithCoder:coder];
    
    _showSettings = new ShowSettings;
    
    self.clearColor = MTLClearColorMake(0.0f, 0.0f, 0.0f, 0.0f);
    
    self.clearDepth = _showSettings->isReverseZ ? 0.0f : 1.0f;
    
    // only re-render when changes are made
    // Note: this breaks ability to gpu capture, since display link not running.
    // so disable this if want to do captures.
#ifndef NDEBUG // KRAM_RELEASE
    self.enableSetNeedsDisplay = YES;
#endif
    // openFile in appDelegate handles "Open in..."
    
    _textSlots.resize(2);
    
    // added for drag-drop support
    [self registerForDraggedTypes:pasteboardTypes];
        
    _zoomGesture = [[NSMagnificationGestureRecognizer alloc] initWithTarget:self action:@selector(handleGesture:)];
    [self addGestureRecognizer:_zoomGesture];
       
    _hudLabel2 = [self _addHud:YES];
    _hudLabel  = [self _addHud:NO];
    [self setHudText:""];
   
    return self;
}

- (nonnull ShowSettings*)showSettings {
    return _showSettings;
}

- (NSTextField*)_addHud:(BOOL)isShadow
{
    // add a label for the hud
    NSTextField *label = [[NSTextField alloc] initWithFrame:NSMakeRect(isShadow ? 11 : 10, isShadow ? 11 : 10, 400, 200)];
    label.drawsBackground = NO;
    label.textColor = !isShadow ?
        [NSColor colorWithSRGBRed:0 green:1 blue:0 alpha:1] :
        [NSColor colorWithSRGBRed:0 green:0 blue:0 alpha:1];
    label.bezeled = NO;
    label.editable = NO;
    label.selectable = NO;
    label.lineBreakMode = NSLineBreakByClipping;

    label.cell.scrollable = NO;
    label.cell.wraps = NO;

    label.hidden = !_showSettings->isHudShown;
    //label.font = NSFont.systemFont(ofSize: NSFont.systemFontSize(for: label.controlSize))

    // UILabel has shadowColor/shadowOffset but NSTextField doesn't
    
    [self addSubview: label];
    return label;
}

- (void)doZoomMath:(float)newZoom newPan:(float2&)newPan {
// transform the cursor to texture coordinate, or clamped version if outside
    
    
    Renderer* renderer = (Renderer*)self.delegate;
    float4x4 projectionViewModelMatrix = [renderer computeImageTransform:_showSettings->panX panY:_showSettings->panY zoom:_showSettings->zoom];

    // convert to clip space, or else need to apply additional viewport transform
    float halfX = _showSettings->viewSizeX * 0.5f;
    float halfY = _showSettings->viewSizeY * 0.5f;

    // sometimes get viewSizeX that's scaled by retina, and other times not.
    // account for contentScaleFactor (viewSizeX is 2x bigger than cursorX on retina display)
    // now passing down drawableSize instead of view.bounds.size
    halfX /= (float)_showSettings->viewContentScaleFactor;
    halfY /= (float)_showSettings->viewContentScaleFactor;

    NSPoint point = NSMakePoint(_showSettings->cursorX, _showSettings->cursorY);
    NSPoint clipPoint;
    clipPoint.x = (point.x - halfX) / halfX;
    clipPoint.y = -(point.y - halfY) / halfY;

    // convert point in window to point in texture
    float4x4 mInv = simd_inverse(projectionViewModelMatrix);
    mInv.columns[3].w = 1.0f; // fixes inverse, calls always leaves m[3][3] = 0.999

    float4 pixel = mInv * simd_make_float4(clipPoint.x, clipPoint.y, 1.0f, 1.0f);
    //pixel /= pixel.w; // in case perspective used

    // allow pan to extend to show all
    float maxX = 0.5f;
    float minY = -0.5f;
    if (_showSettings->isShowingAllLevelsAndMips) {
        maxX += 1.0f * (_showSettings->totalLevels() - 1);
        minY -= 1.0f * (_showSettings->maxLOD - 1);
    }
    
    // that's in model space (+/0.5f, +/0.5f), so convert to texture space
    pixel.x = clamp(pixel.x, -0.5f, maxX);
    pixel.y = clamp(pixel.y, minY, 0.5f);
    
    // now that's the point that we want to zoom towards
    // No checkson this zoom
    // old - newPosition from the zoom
    
    newPan.x = _showSettings->panX - (_showSettings->zoom  - newZoom) * _showSettings->imageBoundsX * pixel.x;
    newPan.y = _showSettings->panY + (_showSettings->zoom  - newZoom) * _showSettings->imageBoundsY * pixel.y;
}

- (void)handleGesture:(NSGestureRecognizer *)gestureRecognizer
{
    // https://cocoaosxrevisited.wordpress.com/2018/01/06/chapter-18-mouse-events/
    if  (gestureRecognizer != _zoomGesture) {
        return;
    }
    
    bool isFirstGesture = _zoomGesture.state == NSGestureRecognizerStateBegan;
    
    static float _originalZoom = 1.0f;
    
    float zoom = _zoomGesture.magnification;
    if (isFirstGesture) {
        _zoomGesture.magnification = 1.0f;
        zoom = _showSettings->zoom;
    }
    else if (zoom * _originalZoom < 0.1f) {
        // can go negative otherwise
        zoom = 0.1f / _originalZoom;
        _zoomGesture.magnification = zoom;
    }
    
    static float _validMagnification;
    
    //-------------------------------------
    
    // https://developer.apple.com/documentation/uikit/touches_presses_and_gestures/handling_uikit_gestures/handling_pinch_gestures?language=objc
    // need to sync up the zoom when action begins or zoom will jump
    if (isFirstGesture) {
        _validMagnification = 1.0f;
        _originalZoom = zoom;
    }
    else {
        // try expontental (this causes a jump, comparison avoids an initial jump
        //zoom = powf(zoom, 1.05f);
        
        // doing multiply instead of equals here, also does exponential zom
        zoom *= _originalZoom;
    }
    
    // https://stackoverflow.com/questions/30002361/image-zoom-centered-on-mouse-position
    
    // find the cursor location with respect to the image
    float4 bottomLeftCorner = simd_make_float4(-0.5, -0.5f, 0.0f, 1.0f);
    float4 topRightCorner = simd_make_float4(0.5, 0.5f, 0.0f, 1.0f);
    
    Renderer* renderer = (Renderer*)self.delegate;
    float4x4 newMatrix = [renderer computeImageTransform:_showSettings->panX panY:_showSettings->panY zoom:zoom];

    // don't allow panning the entire image off the view boundary
    // transform the upper left and bottom right corner of the image
    float4 pt0 = newMatrix * bottomLeftCorner;
    float4 pt1 = newMatrix * topRightCorner;
    
    // for perspective
    //pt0 /= pt0.w;
    //pt1 /= pt1.w;
    
    // see that rectangle intersects the view, view is -1 to 1
    CGRect imageRect = CGRectMake(pt0.x, pt0.y, pt1.x - pt0.x, pt1.y - pt0.y);
    CGRect viewRect = CGRectMake(-1.0f, -1.0f, 2.0f, 2.0f);
   
    int32_t numTexturesX = _showSettings->totalLevels();
    int32_t numTexturesY = _showSettings->maxLOD;
    
    if (_showSettings->isShowingAllLevelsAndMips) {
        imageRect.origin.y -= (numTexturesY - 1 ) * imageRect.size.height;
        
        imageRect.size.width *= numTexturesX;
        imageRect.size.height *= numTexturesY;
    }
    
    float visibleWidth = imageRect.size.width * _showSettings->viewSizeX / _showSettings->viewContentScaleFactor;
    float visibleHeight = imageRect.size.height * _showSettings->viewSizeY / _showSettings->viewContentScaleFactor;
    
    // don't allow image to get too big
    // take into account zoomFit, or need to limit zoomFit and have smaller images be smaller on screen
    float maxZoom = std::max(128.0f, _showSettings->zoomFit);
        
    // don't allow image to get too big
    int32_t gap = _showSettings->showAllPixelGap;
    if ((visibleWidth > maxZoom * (_showSettings->imageBoundsX + gap) * numTexturesX) ||
        (visibleHeight > maxZoom * (_showSettings->imageBoundsY + gap) * numTexturesY)) {
        _zoomGesture.magnification = _validMagnification;
        return;
    }
    
    // don't allow image to get too small
    int32_t minPixelSize = 4;
    if ((visibleWidth < std::min((int32_t)_showSettings->imageBoundsX, minPixelSize)) ||
        (visibleHeight < std::min((int32_t)_showSettings->imageBoundsY, minPixelSize))) {
        _zoomGesture.magnification = _validMagnification;
        return;
    }
    
    if (!NSIntersectsRect(imageRect, viewRect)) {
        _zoomGesture.magnification = _validMagnification;
        return;
    }
    
    
    if (_showSettings->zoom != zoom) {
        // DONE: zoom also changes the pan to zoom about the cursor, otherwise zoom feels wrong.
        // now adjust the pan so that cursor text stays locked under (zoom to cursor)
        float2 newPan;
        [self doZoomMath:zoom newPan:newPan];
        
        // store this
        _validMagnification = _zoomGesture.magnification;
        
        _showSettings->zoom = zoom;
        
        _showSettings->panX = newPan.x;
        _showSettings->panY = newPan.y;
        
        if (doPrintPanZoom) {
            string text;
            sprintf(text,
                "Pan %.3f,%.3f\n"
                "Zoom %.2fx\n",
                _showSettings->panX, _showSettings->panY,
                _showSettings->zoom);
            [self setHudText:text.c_str()];
        }
        
        [self updateEyedropper];
        self.needsDisplay = YES;
    }
}


- (void)mouseMoved:(NSEvent*)event
{
    // pixel in non-square window coords, run thorugh inverse to get texel space
    // I think magnofication of zoom gesture is affecting coordinates reported by this
    
    NSPoint point = [self convertPoint:[event locationInWindow] fromView:nil];
    
    // this needs to change if view is resized, but will likely receive mouseMoved events
    _showSettings->cursorX = (int32_t)point.x;
    _showSettings->cursorY = (int32_t)point.y;
    
    // should really do this in draw call, since moved messeage come in so quickly
    [self updateEyedropper];
}

- (void)updateEyedropper {
    if ((!_showSettings->isHudShown)) {
        return;
    }
    
    if (_showSettings->imageBoundsX == 0) {
        // TODO: this return will leave old hud text up
        return;
    }
    
    // don't wait on renderer to update this matrix
    Renderer* renderer = (Renderer*)self.delegate;
    float4x4 projectionViewModelMatrix = [renderer computeImageTransform:_showSettings->panX panY:_showSettings->panY zoom:_showSettings->zoom];

    // convert to clip space, or else need to apply additional viewport transform
    float halfX = _showSettings->viewSizeX * 0.5f;
    float halfY = _showSettings->viewSizeY * 0.5f;
    
    // sometimes get viewSizeX that's scaled by retina, and other times not.
    // account for contentScaleFactor (viewSizeX is 2x bigger than cursorX on retina display)
    // now passing down drawableSize instead of view.bounds.size
    halfX /= (float)_showSettings->viewContentScaleFactor;
    halfY /= (float)_showSettings->viewContentScaleFactor;
    
    NSPoint point = NSMakePoint(_showSettings->cursorX, _showSettings->cursorY);
    NSPoint clipPoint;
    clipPoint.x = (point.x - halfX) / halfX;
    clipPoint.y = -(point.y - halfY) / halfY;
    
    // convert point in window to point in texture
    float4x4 mInv = simd_inverse(projectionViewModelMatrix);
    mInv.columns[3].w = 1.0f; // fixes inverse, calls always leaves m[3][3] = 0.999
    
    float4 pixel = mInv * simd_make_float4(clipPoint.x, clipPoint.y, 1.0f, 1.0f);
    //pixel /= pixel.w; // in case perspective used
    
    // that's in model space (+/0.5f, +/0.5f), so convert to texture space
    pixel.y *= -1.0f;
    
    pixel.x += 0.5f;
    pixel.y += 0.5f;
    
    pixel.x *= 0.999f;
    pixel.y *= 0.999f;
    
    // pixels are 0 based
    pixel.x *= _showSettings->imageBoundsX;
    pixel.y *= _showSettings->imageBoundsY;

// TODO: finish this logic, need to account for gaps too, and then isolate to a given level and mip to sample
//    if (_showSettings->isShowingAllLevelsAndMips) {
//        pixel.x *= _showSettings->totalLevels();
//        pixel.y *= _showSettings->maxLOD;
//    }
    
    // TODO: clearing out the last px visited makes it hard to gather data
    // put value on clipboard, or allow click to lock the displayed pixel and value.
    // Might just change header to px(last): ...
    string text;
    
    // only display pixel if over image
    if (pixel.x < 0.0f || pixel.x >= (float)_showSettings->imageBoundsX) {
        sprintf(text, "canvas: %d %d\n", (int32_t)pixel.x, (int32_t)pixel.y);
        [self setEyedropperText:text.c_str()]; // ick
        return;
    }
    if (pixel.y < 0.0f || pixel.y >= (float)_showSettings->imageBoundsY) {
        
        // was blanking out, but was going blank on color_grid-a when over zoomed in image
        // maybe not enough precision with float.
        sprintf(text, "canvas: %d %d\n", (int32_t)pixel.x, (int32_t)pixel.y);
        [self setEyedropperText:text.c_str()];
        return;
    }

    // Note: fromView: nil returns isFlipped coordinate, fromView:self flips it back.

    int32_t newX = (int32_t)pixel.x;
    int32_t newY = (int32_t)pixel.y;
    
    if (_showSettings->textureLookupX != newX ||
        _showSettings->textureLookupY != newY)
    {
        // Note: this only samples from the original texture via compute shaders
        // so preview mode pixel colors are not conveyed.  But can see underlying data driving preview.
        
        MyMTLPixelFormat format = (MyMTLPixelFormat)_showSettings->originalFormat;
        
        // DONE: use these to format the text
        bool isSrgb = isSrgbFormat(format);
        bool isSigned = isSignedFormat(format);
        bool isHdr = isHdrFormat(format);
        int32_t numChannels = _showSettings->numChannels;
        
        // %.0f rounds the value, but want truncation
        _showSettings->textureLookupX = newX;
        _showSettings->textureLookupY = newY;
        
        // this will be out of sync with gpu eval, so may want to only display px from returned lookup
        // this will always be a linear color
        float4 c = _showSettings->textureResult;
        
        // this saturates the value, so don't use for extended srgb
        float4 s = linearToSRGB(c);
        
        int32_t x = _showSettings->textureResultX;
        int32_t y = _showSettings->textureResultY;
        
        sprintf(text, "px:%d %d\n", x, y);
        
        // TODO: more criteria here, can have 2 channel PBR metal-roughness
        // also have 4 channel normals where zw store other data.
        bool isNormal = _showSettings->isNormal;
        bool isFloat = isHdr;
        
        if (isNormal) {
            float x = c.x;
            float y = c.y;
            
            // unorm -> snorm
            if (!isSigned) {
                x = x * 2.0f - 1.0f;
                y = y * 2.0f - 1.0f;
            }
            
            // this is always postive on tan-space normals
            // assuming we're not viewing world normals
            float z = sqrt(1.0f - std::min(x * x + y * y, 1.0f));
            
            // print the underlying color (some nmaps are xy in 4 channels)
            string tmp;
            printChannels(tmp, "c: ", c, numChannels, isFloat, isSigned);
            text += tmp;
            
            // print direction
            float4 d = simd_make_float4(x,y,z,0.0f);
            isFloat = true;
            printChannels(tmp, "d: ", d, 3, isFloat, isSigned);
            text += tmp;
        }
        else {
            // DONE: write some print helpers based on float4 and length
            string tmp;
            printChannels(tmp, "l: ", c, numChannels, isFloat, isSigned);
            text += tmp;
            
            if (isSrgb) {
                printChannels(tmp, "s: ", s, numChannels, isFloat, isSigned);
                text += tmp;
            }
        }
        
        [self setEyedropperText:text.c_str()];

        // TODO: range display of pixels is useful, only showing pixels that fall
        // within a given range, but would need slider then, and determine range of pixels.
        // TODO: Auto-range is also useful for depth (ignore far plane of 0 or 1).
        
        // TOOD: display histogram from compute, bin into buffer counts of pixels
        
        // DONE: stop clobbering hud text, need another set of labels
        // and a zoom preview of the pixels under the cursor.
        // Otherwise, can't really see the underlying color.
        
        // TODO: Stuff these on clipboard with a click, or use cmd+C?

        // TODO: remove this, but only way to get drawSamples to execute right now, but then
        // entire texture re-renders and that's not power efficient.
        self.needsDisplay = YES;
    }
}

- (void)setEyedropperText:(const char*)text {
    _textSlots[0] = text;
    
    [self updateHudText];
}

- (void)setHudText:(const char*)text {
    _textSlots[1] = text;
    
    [self updateHudText];
}

- (void)updateHudText {
    // combine textSlots
    string text = _textSlots[0] + _textSlots[1];
    
    NSString* textNS = [NSString stringWithUTF8String:text.c_str()];
    _hudLabel2.stringValue = textNS;
    _hudLabel2.needsDisplay = YES;
    
    _hudLabel.stringValue = textNS;
    _hudLabel.needsDisplay = YES;
}

- (void)scrollWheel:(NSEvent *)event
{
    double wheelX = [event scrollingDeltaX];
    double wheelY = [event scrollingDeltaY];
    
//    if ([event hasPreciseScrollingDeltas])
//    {
//        //wheelX *= 0.01;
//        //wheelY *= 0.01;
//    }
//    else {
//        //wheelX *= 0.1;
//        //wheelY *= 0.1;
//    }

    //---------------------------------------
    
    // pan
    wheelY = -wheelY;
    wheelX = -wheelX;

    float panX = _showSettings->panX + wheelX;
    float panY = _showSettings->panY + wheelY;
    
    Renderer* renderer = (Renderer*)self.delegate;
    float4x4 projectionViewModelMatrix = [renderer computeImageTransform:panX panY:panY zoom:_showSettings->zoom];

    // don't allow panning the entire image off the view boundary
    // transform the upper left and bottom right corner or the image
    
    // what if zoom moves it outside?
    
    float4 pt0 = projectionViewModelMatrix * simd_make_float4(-0.5, -0.5f, 0.0f, 1.0f);
    float4 pt1 = projectionViewModelMatrix * simd_make_float4(0.5, 0.5f, 0.0f, 1.0f);
    
    // for perspective
    //pt0 /= pt0.w;
    //pt1 /= pt1.w;
    
    // see that rectangle intersects the view, view is -1 to 1
    CGRect imageRect = CGRectMake(pt0.x, pt0.y, pt1.x - pt0.x, pt1.y - pt0.y);
    CGRect viewRect = CGRectMake(-1.0f, -1.0f, 2.0f, 2.0f);
   
    int32_t numTexturesX = _showSettings->totalLevels();
    int32_t numTexturesY = _showSettings->maxLOD;
    
    if (_showSettings->isShowingAllLevelsAndMips) {
        imageRect.origin.y -= (numTexturesY - 1 ) * imageRect.size.height;
        
        imageRect.size.width *= numTexturesX;
        imageRect.size.height *= numTexturesY;
    }
    
    if (!NSIntersectsRect(imageRect, viewRect)) {
        return;
    }
    
    if (_showSettings->panX != panX ||
        _showSettings->panY != panY)
    {
        _showSettings->panX = panX;
        _showSettings->panY = panY;
        
        if (doPrintPanZoom) {
            string text;
            sprintf(text,
                "Pan %.3f,%.3f\n"
                "Zoom %.2fx\n",
                _showSettings->panX, _showSettings->panY,
                _showSettings->zoom);
            [self setHudText:text.c_str()];
        }
        
        [self updateEyedropper];
        self.needsDisplay = YES;
    }
}


// TODO: convert to C++ actions, and then call into Base holding all this
// move pan/zoom logic too.  Then use that as start of Win32 kramv.

- (void)keyDown:(NSEvent *)theEvent
{
    // Some data depends on the texture data (isSigned, isNormal, ..)
    TextureChannels& channels = _showSettings->channels;
    bool isChanged = false;
    
    // TODO: fix isChanged to only be set when value changes
    // f.e. clamped values don't need to re-render
    bool isShiftKeyDown = theEvent.modifierFlags & NSEventModifierFlagShift;
    string text;
    
    switch(theEvent.keyCode) {
        // rgba channels
        case Key::R:
            if (channels == TextureChannels::ModeRRR1 || channels == TextureChannels::ModeR001) {
                channels = TextureChannels::ModeRGBA;
                text = "Mask RGBA";
            }
            else {
                channels = isShiftKeyDown ? TextureChannels::ModeRRR1 : TextureChannels::ModeR001;
                text = isShiftKeyDown ? "Mask RRR1" : "Mask R001";
            }
            isChanged = true;
            
            break;
        case Key::G:
            if (channels == TextureChannels::ModeGGG1 || channels == TextureChannels::Mode0G01) {
                channels = TextureChannels::ModeRGBA;
                text = "Mask RGBA";
            }
            else {
                channels = isShiftKeyDown ? TextureChannels::ModeGGG1 : TextureChannels::Mode0G01;
                text = isShiftKeyDown ? "Mask GGG1" : "Mask 0G01";
            }
            isChanged = true;
            break;
        case Key::B:
            if (channels == TextureChannels::ModeBBB1 || channels == TextureChannels::Mode00B1) {
                channels = TextureChannels::ModeRGBA;
                text = "Mask RGBA";
            }
            else {
                channels = isShiftKeyDown ? TextureChannels::ModeBBB1 : TextureChannels::Mode00B1;
                text = isShiftKeyDown ? "Mask BBB1" : "Mask 00B1";
            }
            isChanged = true;
            break;
        case Key::A:
            if (channels == TextureChannels::ModeAAA1) {
                channels = TextureChannels::ModeRGBA;
                text = "Mask RGBA";
            }
            else {
                channels = TextureChannels::ModeAAA1;
                text = "Mask AAA1";
            }
            isChanged = true;
            break;
            
        case Key::E: {
            _showSettings->advanceDebugMode(isShiftKeyDown);
            
            switch(_showSettings->debugMode) {
                case DebugModeNone: text = "Debug Off"; break;
                case DebugModeTransparent: text = "Debug Transparent"; break;
                case DebugModeColor: text = "Debug Color"; break;
                case DebugModeGray: text = "Debug Gray"; break;
                case DebugModeHDR: text = "Debug HDR"; break;
                case DebugModePosX: text = "Debug +X"; break;
                case DebugModePosY: text = "Debug +Y"; break;
                default: break;
            }
            isChanged = true;
            break;
        }
        case Key::Slash: // has ? mark above it
            // display the chars for now
            text = "⇧RGBA, O-preview, ⇧E-debug, Show all\n"
                   "Hud, ⇧L-reload, ⇧0-fit\n"
                   "Checker, ⇧D-block/px grid, Info\n"
                   "W-wrap, Premul, N-signed\n"
                   "⇧Mip, ⇧Face, ⇧Y-array/slice";
            break;
            
        case Key::Zero: { // scale and reset pan
            float zoom;
            // fit image or mip
            if (isShiftKeyDown) {
                zoom = 1.0f;
            }
            else {
                // fit to topmost image
                zoom = _showSettings->zoomFit;
            }
            
            // This zoom needs to be checked against zoom limits
            // there's a cap on the zoom multiplier.
            // This is reducing zoom which expands the image.
            zoom *= 1.0f / (1 << _showSettings->mipLOD);
            
            // even if zoom same, still do this since it resets the pan
            _showSettings->zoom = zoom;
            
            _showSettings->panX = 0.0f;
            _showSettings->panY = 0.0f;
            
            text = "Scale Image\n";
            if (doPrintPanZoom) {
                string tmp;
                sprintf(tmp,
                        "Pan %.3f,%.3f\n"
                        "Zoom %.2fx\n",
                    _showSettings->panX, _showSettings->panY,
                    _showSettings->zoom);
                text += tmp;
            }
            
            isChanged = true;
            
            break;
        }
        // reload key (also a quick way to reset the settings)
        case Key::L:
            [self loadTextureFromURL:self.imageURL];
            
            // reload at actual size
            if (isShiftKeyDown) {
                _showSettings->zoom = 1.0f;
            }
            
            text = "Reload Image";
            if (doPrintPanZoom) {
                string tmp;
                sprintf(tmp,
                        "Pan %.3f,%.3f\n"
                        "Zoom %.2fx\n",
                    _showSettings->panX, _showSettings->panY,
                    _showSettings->zoom);
                text += tmp;
            }
            
            isChanged = true;
            break;
            
        // P already used for premul
        case Key::O:
            _showSettings->isPreview = !_showSettings->isPreview;
            isChanged = true;
            text = "Preview ";
            text += _showSettings->isPreview ? "On" : "Off";
            break;
            
        // TODO: might switch c to channel cycle, so could just hit that
        // and depending on the content, it cycles through reasonable channel masks
            
        // toggle checkerboard for transparency
        case Key::C:
            _showSettings->isCheckerboardShown = !_showSettings->isCheckerboardShown;
            isChanged = true;
            text = "Checker ";
            text += _showSettings->isCheckerboardShown ? "On" : "Off";
            break;
        
        // toggle pixel grid when magnified above 1 pixel, can happen from mipmap changes too
        case Key::D:
            // TODO: display how many blocks there are
            if (isShiftKeyDown && _showSettings->blockX > 1) {
                // if block size is 1, then this shouldn't toggle
                _showSettings->isBlockGridShown = !_showSettings->isBlockGridShown;
                _showSettings->isPixelGridShown = false;
                sprintf(text, "Block Grid %dx%d %s",
                        _showSettings->blockX, _showSettings->blockY,
                        _showSettings->isBlockGridShown ? "On" : "Off");
            }
            else {
               
                _showSettings->isPixelGridShown = !_showSettings->isPixelGridShown;
                _showSettings->isBlockGridShown = false;
                text = "Pixel Grid ";
                text += _showSettings->isPixelGridShown ? "On" : "Off";
            }
        
            isChanged = true;
            
            break;
        
        case Key::S:
            // TODO: have drawAllMips, drawAllLevels, drawAllLevelsAndMips
            _showSettings->isShowingAllLevelsAndMips = !_showSettings->isShowingAllLevelsAndMips;
            isChanged = true;
            text = "Show All ";
            text += _showSettings->isShowingAllLevelsAndMips ? "On" : "Off";
            break;
            
        // toggle hud that shows name and pixel value under the cursor
        // this may require calling setNeedsDisplay on the UILabel as cursor moves
        case Key::H:
            _showSettings->isHudShown = !_showSettings->isHudShown;
            _hudLabel.hidden = !_showSettings->isHudShown;
            _hudLabel2.hidden = !_showSettings->isHudShown;
            //isChanged = true;
            text = "Hud ";
            text += _showSettings->isHudShown ? "On" : "Off";
            break;
            
        // info on the texture, could request info from lib, but would want to cache that info
        case Key::I:
            if (_showSettings->isHudShown) {
                sprintf(text, "%s", _showSettings->imageInfo.c_str());
            }
            break;
        
        // toggle wrap/clamp
        case Key::W:
            // TODO: cycle through all possible modes (clamp, repeat, mirror-once, mirror-repeat, ...)
            _showSettings->isWrap = !_showSettings->isWrap;
            isChanged = true;
            text = "Wrap ";
            text += _showSettings->isWrap ? "On" : "Off";
            break;
            
        // toggle signed vs. unsigned
        case Key::N:
            _showSettings->isSigned = !_showSettings->isSigned;
            isChanged = true;
            text = "Signed ";
            text += _showSettings->isSigned ? "On" : "Off";
            break;
            
        // toggle premul alpha vs. unmul
        case Key::P:
            _showSettings->isPremul = !_showSettings->isPremul;
            isChanged = true;
            text = "Premul ";
            text += _showSettings->isPremul ? "On" : "Off";
            break;
            
            
        // mip up/down
        case Key::M:
            if (isShiftKeyDown) {
                _showSettings->mipLOD = MAX(_showSettings->mipLOD - 1, 0);
            }
            else {
                _showSettings->mipLOD = MIN(_showSettings->mipLOD + 1, _showSettings->maxLOD - 1);
            }
            sprintf(text, "Mip %d/%d", _showSettings->mipLOD, _showSettings->maxLOD);
            isChanged = true;
            break;
           
        case Key::F:
            // cube or cube array, but hit s to pick cubearray
            if (_showSettings->faceCount) {
                if (isShiftKeyDown) {
                    _showSettings->faceNumber = MAX(_showSettings->faceNumber - 1, 0);
                }
                else {
                    _showSettings->faceNumber = MIN(_showSettings->faceNumber + 1, _showSettings->faceCount - 1);
                }
                sprintf(text, "Face %d/%d", _showSettings->faceNumber, _showSettings->faceCount);
                isChanged = true;
            }
            break;
            
        
        case Key::Y:
            // slice
            if (_showSettings->sliceCount > 1) {
                if (isShiftKeyDown) {
                    _showSettings->sliceNumber = MAX(_showSettings->sliceNumber - 1, 0);
                }
                else {
                    _showSettings->sliceNumber = MIN(_showSettings->sliceNumber + 1, _showSettings->sliceCount - 1);
                }
                sprintf(text, "Slice %d/%d", _showSettings->sliceNumber, _showSettings->sliceCount);
                isChanged = true;
            }
            // array
            else if (_showSettings->arrayCount > 1) {
                if (isShiftKeyDown) {
                    _showSettings->arrayNumber = MAX(_showSettings->arrayNumber - 1, 0);
                }
                else {
                    _showSettings->arrayNumber = MIN(_showSettings->arrayNumber + 1, _showSettings->arrayCount - 1);
                }
                sprintf(text, "Array %d/%d", _showSettings->arrayNumber, _showSettings->arrayCount);
                isChanged = true;
            }
            break;
    }
    
    if (!text.empty()) {
        [self setHudText:text.c_str()];
    }
    
    if (isChanged) {
        self.needsDisplay = YES;
    }
}

- (BOOL)acceptsFirstResponder {
    return YES;
}

// Note: docs state that drag&drop should be handled automatically by UTI setup via openURLs
// but I find these calls are needed, or it doesn't work.  Maybe need to register for NSRUL
// instead of NSPasteboardTypeFileURL.  For example, in canReadObjectForClasses had to use NSURL.

// drag and drop support
- (NSDragOperation)draggingEntered:(id)sender {
   if ((NSDragOperationGeneric & [sender draggingSourceOperationMask]) == NSDragOperationGeneric) {
       NSPasteboard *pasteboard = [sender draggingPasteboard];
       
       bool canReadPasteboardObjects = [pasteboard canReadObjectForClasses: @[[NSURL class]] options: nil];
       
       // don't copy dropped item, want to alias large files on disk without that
       if (canReadPasteboardObjects) {
           return NSDragOperationGeneric;
       }
   }

   // not a drag we can use
   return NSDragOperationNone;
}

- (BOOL)prepareForDragOperation:(id)sender {
   return YES;
}


- (BOOL)performDragOperation:(id)sender {
    NSPasteboard *pasteboard = [sender draggingPasteboard];

    NSString *desiredType =
                [pasteboard availableTypeFromArray:pasteboardTypes];

    if ([desiredType isEqualToString:NSPasteboardTypeFileURL]) {
        // TODO: use readObjects to drag multiple files onto one view
        // load one mip of all those, use smaller mips for thumbnail
        
        // the pasteboard contains a list of filenames
        NSString *urlString = [pasteboard propertyListForType:NSPasteboardTypeFileURL];
        
        // this turns it into a real path (supposedly works even with sandbox)
        NSURL * url = [NSURL URLWithString:urlString];
        
        
        if ([self loadTextureFromURL:url]) {
            return YES;
        }
   }

    return NO;
}

- (BOOL)loadTextureFromURL:(NSURL*)url {
    //NSLog(@"LoadTexture");
    
    const char* filename = url.fileSystemRepresentation;

    // set title to filename, chop this to just file+ext, not directory
    const char* filenameShort = strrchr(filename, '/');
    if (filenameShort == nullptr) {
        filenameShort = filename;
    }
    else {
        filenameShort += 1;
    }
    
    // was using subtitle, but that's macOS 11.0 feature.
    string title = "kramv - ";
    title += filenameShort;
    
    self.window.title = [NSString stringWithUTF8String: title.c_str()];
    
    if (endsWithExtension(filename, ".png")) {
        Renderer* renderer = (Renderer*)self.delegate;
        if (![renderer loadTexture:url]) {
            return NO;
        }
    }
    else if (endsWithExtension(filename, ".ktx") || endsWithExtension(filename, ".ktx2")) {
        Renderer* renderer = (Renderer*)self.delegate;
        if (![renderer loadTexture:url]) {
            return NO;
        }
    }
    else {
        return NO;
    }

    // add to recent document menu
    NSDocumentController *dc = [NSDocumentController sharedDocumentController];
    [dc noteNewRecentDocumentURL:url];

    self.imageURL = url;
    
    self.needsDisplay = YES;
    return YES;
}


- (void)concludeDragOperation:(id)sender {
    // did setNeedsDisplay, but already doing that in loadTextureFromURL
}



// this doesn't seem to enable New/Open File menu items, but it should
// https://developer.apple.com/library/archive/documentation/Cocoa/Conceptual/EventOverview/EventArchitecture/EventArchitecture.html
#if 0

// "New"" calls this
- (__kindof NSDocument *)openUntitledDocumentAndDisplay:(BOOL)displayDocument
                                                  error:(NSError * _Nullable *)outError
{
    // TODO: this should add an empty MyMTKView and can drag/drop to that.
    // Need to track images and data dropped per view then.
    return nil;
}

// "Open File" calls this
- (void)openDocumentWithContentsOfURL:(NSURL *)url
                              display:(BOOL)displayDocument
                    completionHandler:(void (^)(NSDocument *document, BOOL documentWasAlreadyOpen, NSError *error))completionHandler
{
    [self loadTextureFromURL:url];
}

- (IBAction)openDocument {
    // calls openDocumentWithContentsOfURL above
}

- (IBAction)newDocument {
    // calls openUntitledDocumentAndDisplay above
}
#endif

@end

//-------------

// Our macOS view controller.
@interface GameViewController : NSViewController

@end

@implementation GameViewController
{
    MyMTKView *_view;

    Renderer *_renderer;
    
    NSTrackingArea *_trackingArea;
}


- (void)viewDidLoad
{
    [super viewDidLoad];

    _view = (MyMTKView *)self.view;

    _view.device = MTLCreateSystemDefaultDevice();

    if(!_view.device)
    {
        return;
    }

    _renderer = [[Renderer alloc] initWithMetalKitView:_view settings:_view.showSettings];

    // original sample code was sending down _view.bounds.size, but need drawableSize
    // this was causing all sorts of inconsistencies
    [_renderer mtkView:_view drawableSizeWillChange:_view.drawableSize];

    _view.delegate = _renderer;
    
    // https://developer.apple.com/library/archive/documentation/Cocoa/Conceptual/EventOverview/TrackingAreaObjects/TrackingAreaObjects.html
    // this is better than requesting mousemoved events, they're only sent when cursor is inside
    _trackingArea = [[NSTrackingArea alloc] initWithRect:_view.bounds
                options: (NSTrackingMouseEnteredAndExited | NSTrackingMouseMoved | NSTrackingActiveInKeyWindow )
                owner:_view userInfo:nil];
    [_view addTrackingArea:_trackingArea];

}




@end


//-------------

int main(int argc, const char * argv[]) {
    @autoreleasepool {
        // Setup code that might create autoreleased objects goes here.
    }
    return NSApplicationMain(argc, argv);
}
