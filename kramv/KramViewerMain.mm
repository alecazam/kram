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
    
    self.clearColor = MTLClearColorMake(0.0f, 0.0f, 0.0f, 0.0f);
    
    self.clearDepth = gShowSettings.isReverseZ ? 0.0f : 1.0f;
    
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

    label.hidden = !gShowSettings.isHudShown;
    //label.font = NSFont.systemFont(ofSize: NSFont.systemFontSize(for: label.controlSize))

    // UILabel has shadowColor/shadowOffset but NSTextField doesn't
    
    [self addSubview: label];
    return label;
}

- (void)doZoomMath:(float)newZoom newPan:(float2&)newPan {
// transform the cursor to texture coordinate, or clamped version if outside
    
    
    Renderer* renderer = (Renderer*)self.delegate;
    float4x4 projectionViewModelMatrix = [renderer computeImageTransform:gShowSettings.panX panY:gShowSettings.panY zoom:gShowSettings.zoom];

    // convert to clip space, or else need to apply additional viewport transform
    float halfX = gShowSettings.viewSizeX * 0.5f;
    float halfY = gShowSettings.viewSizeY * 0.5f;

    // sometimes get viewSizeX that's scaled by retina, and other times not.
    // account for contentScaleFactor (viewSizeX is 2x bigger than cursorX on retina display)
    // now passing down drawableSize instead of view.bounds.size
    halfX /= (float)gShowSettings.viewContentScaleFactor;
    halfY /= (float)gShowSettings.viewContentScaleFactor;

    NSPoint point = NSMakePoint(gShowSettings.cursorX, gShowSettings.cursorY);
    NSPoint clipPoint;
    clipPoint.x = (point.x - halfX) / halfX;
    clipPoint.y = -(point.y - halfY) / halfY;

    // convert point in window to point in texture
    float4x4 mInv = simd_inverse(projectionViewModelMatrix);
    mInv.columns[3].w = 1.0f; // fixes inverse, calls always leaves m[3][3] = 0.999

    float4 pixel = mInv * simd_make_float4(clipPoint.x, clipPoint.y, 1.0f, 1.0f);
    //pixel /= pixel.w; // in case perspective used

    // that's in model space (+/0.5f, +/0.5f), so convert to texture space
    pixel.x = clamp(pixel.x, -0.5f, 0.5f);
    pixel.y = clamp(pixel.y, -0.5f, 0.5f);
    
    // now that's the point that we want to zoom towards
    // No checkson this zoom
    // old - newPosition from the zoom
    
    newPan.x = gShowSettings.panX - (gShowSettings.zoom  - newZoom) * gShowSettings.imageBoundsX * pixel.x;
    newPan.y = gShowSettings.panY + (gShowSettings.zoom  - newZoom) * gShowSettings.imageBoundsY * pixel.y;
}

- (void)handleGesture:(NSGestureRecognizer *)gestureRecognizer
{
    // https://cocoaosxrevisited.wordpress.com/2018/01/06/chapter-18-mouse-events/
    if  (gestureRecognizer != _zoomGesture) {
        return;
    }
    
    float zoom;
    
    static float _originalZoom;
    static float _validMagnification;
    
    // https://developer.apple.com/documentation/uikit/touches_presses_and_gestures/handling_uikit_gestures/handling_pinch_gestures?language=objc
    // need to sync up the zoom when action begins or zoom will jump
    if (_zoomGesture.state == NSGestureRecognizerStateBegan) {
        _zoomGesture.magnification = 1.0f;
        _validMagnification = 1.0f;
        
        _originalZoom = gShowSettings.zoom;
        
        zoom = gShowSettings.zoom;
        
    }
    else {
        zoom = _zoomGesture.magnification;
        
        // try expontental (this causes a jump, comparison avoids an initial jump
        //zoom = powf(zoom, 1.05f);
        
        zoom *= _originalZoom;
    }
    
    // https://stackoverflow.com/questions/30002361/image-zoom-centered-on-mouse-position
    
    // find the cursor location with respect to the image
    float4 bottomLeftCorner = simd_make_float4(-0.5, -0.5f, 0.0f, 1.0f);
    float4 topRightCorner = simd_make_float4(0.5, 0.5f, 0.0f, 1.0f);
    
    Renderer* renderer = (Renderer*)self.delegate;
    float4x4 newMatrix = [renderer computeImageTransform:gShowSettings.panX panY:gShowSettings.panY zoom:zoom];

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
   
    float visibleWidth = imageRect.size.width * gShowSettings.viewSizeX / gShowSettings.viewContentScaleFactor;
    float visibleHeight = imageRect.size.height * gShowSettings.viewSizeY / gShowSettings.viewContentScaleFactor;
    
    // don't allow image to get too big
    // take into account zoomFit, or need to limit zoomFit and have smaller images be smaller on screen
    float maxZoom = std::max(128.0f, gShowSettings.zoomFit);
    
    if ((visibleWidth > maxZoom * (gShowSettings.imageBoundsX + 2)) ||
        (visibleHeight > maxZoom * (gShowSettings.imageBoundsY + 2))) {
        _zoomGesture.magnification = _validMagnification;
        return;
    }
    
    // don't allow image to get too small
    if ((visibleWidth < std::min((int)gShowSettings.imageBoundsX, 4)) ||
        (visibleHeight < std::min((int)gShowSettings.imageBoundsY, 4))) {
        _zoomGesture.magnification = _validMagnification;
        return;
    }
    
    if (!NSIntersectsRect(imageRect, viewRect)) {
        _zoomGesture.magnification = _validMagnification;
        return;
    }
    
    
    if (gShowSettings.zoom != zoom) {
        // DONE: zoom also changes the pan to zoom about the cursor, otherwise zoom feels wrong.
        // now adjust the pan so that cursor text stays locked under (zoom to cursor)
        float2 newPan;
        [self doZoomMath:zoom newPan:newPan];
        
        // store this
        _validMagnification = _zoomGesture.magnification;
        
        gShowSettings.zoom = zoom;
        
        gShowSettings.panX = newPan.x;
        gShowSettings.panY = newPan.y;
        
        if (doPrintPanZoom) {
            KLOGI("kramv", "Zoom %.3f, pan %.3f,%.3f",
                  gShowSettings.zoom, gShowSettings.panX, gShowSettings.panY);
        }
        
        [self updateEyedropper];
        self.needsDisplay = YES;
    }
}

static void printChannels(string& tmp, const string& label, float4 c, int numChannels, bool isFloat, bool isSigned)
{
    if (isFloat || isSigned) {
        switch(numChannels) {
            case 1: sprintf(tmp, "%s%.3f\n", label.c_str(), c.r); break;
            case 2: sprintf(tmp, "%s%.3f, %.3f\n", label.c_str(), c.r, c.g); break;
            case 3: sprintf(tmp, "%s%.3f, %.3f, %.3f\n", label.c_str(), c.r, c.g, c.b); break;
            case 4: sprintf(tmp, "%s%.3f, %.3f, %.3f, %.3f\n", label.c_str(), c.r, c.g, c.b, c.a); break;
        }
    }
    else {
        // unorm data, 8-bit values displayed
        c *= 255.1f;
        
        switch(numChannels) {
            case 1: sprintf(tmp, "%s%.0f\n", label.c_str(), c.r); break;
            case 2: sprintf(tmp, "%s%.0f, %.0f\n", label.c_str(), c.r, c.g); break;
            case 3: sprintf(tmp, "%s%.0f, %.0f, %.0f\n", label.c_str(), c.r, c.g, c.b); break;
            case 4: sprintf(tmp, "%s%.0f, %.0f, %.0f, %.0f\n", label.c_str(), c.r, c.g, c.b, c.a); break;
        }
    }
}

- (void)mouseMoved:(NSEvent*)event
{
    // pixel in non-square window coords, run thorugh inverse to get texel space
    // I think magnofication of zoom gesture is affecting coordinates reported by this
    
    NSPoint point = [self convertPoint:[event locationInWindow] fromView:nil];
    
    // this needs to change if view is resized, but will likely receive mouseMoved events
    gShowSettings.cursorX = (int)point.x;
    gShowSettings.cursorY = (int)point.y;
    
    // should really do this in draw call, since moved messeage come in so quickly
    [self updateEyedropper];
}

- (void)updateEyedropper {
    if ((!gShowSettings.isHudShown)) {
        return;
    }
    
    if (gShowSettings.imageBoundsX == 0) {
        // TODO: this return will leave old hud text up
        return;
    }
    
    // don't wait on renderer to update this matrix
    Renderer* renderer = (Renderer*)self.delegate;
    float4x4 projectionViewModelMatrix = [renderer computeImageTransform:gShowSettings.panX panY:gShowSettings.panY zoom:gShowSettings.zoom];

    // convert to clip space, or else need to apply additional viewport transform
    float halfX = gShowSettings.viewSizeX * 0.5f;
    float halfY = gShowSettings.viewSizeY * 0.5f;
    
    // sometimes get viewSizeX that's scaled by retina, and other times not.
    // account for contentScaleFactor (viewSizeX is 2x bigger than cursorX on retina display)
    // now passing down drawableSize instead of view.bounds.size
    halfX /= (float)gShowSettings.viewContentScaleFactor;
    halfY /= (float)gShowSettings.viewContentScaleFactor;
    
    NSPoint point = NSMakePoint(gShowSettings.cursorX, gShowSettings.cursorY);
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
    pixel.x *= gShowSettings.imageBoundsX;
    pixel.y *= gShowSettings.imageBoundsY;

    // TODO: clearing out the last px visited makes it hard to gather data
    // put value on clipboard, or allow click to lock the displayed pixel and value.
    // Might just change header to px(last): ...
    string text;
    
    // only display pixel if over image
    if (pixel.x < 0.0f || pixel.x >= (float)gShowSettings.imageBoundsX) {
        sprintf(text, "canvas: %d %d\n", (int)pixel.x, (int)pixel.y);
        [self setEyedropperText:text.c_str()];
        return;
    }
    if (pixel.y < 0.0f || pixel.y >= (float)gShowSettings.imageBoundsY) {
        
        // was blanking out, but was going blank on color_grid-a when over zoomed in image
        // maybe not enough precision with float.
        sprintf(text, "canvas: %d %d\n", (int)pixel.x, (int)pixel.y);
        [self setEyedropperText:text.c_str()];
        return;
    }

    // Note: fromView: nil returns isFlipped coordinate, fromView:self flips it back.

    int newX = (int)pixel.x;
    int newY = (int)pixel.y;
    
    if (gShowSettings.textureLookupX != newX ||
        gShowSettings.textureLookupY != newY)
    {
        // Note: this only samples from the original texture via compute shaders
        // so preview mode pixel colors are not conveyed.  But can see underlying data driving preview.
        
        MyMTLPixelFormat format = (MyMTLPixelFormat)gShowSettings.originalFormat;
        
        // DONE: use these to format the text
        bool isSrgb = isSrgbFormat(format);
        bool isSigned = isSignedFormat(format);
        bool isHdr = isHdrFormat(format);
        int numChannels = gShowSettings.numChannels;
        
        // %.0f rounds the value, but want truncation
        gShowSettings.textureLookupX = newX;
        gShowSettings.textureLookupY = newY;
        
        // this will be out of sync with gpu eval, so may want to only display px from returned lookup
        // this will always be a linear color
        float4 c = gShowSettings.textureResult;
        
        // this saturates the value, so don't use for extended srgb
        float4 s = linearToSRGB(c);
        
        int x = gShowSettings.textureResultX;
        int y = gShowSettings.textureResultY;
        
        sprintf(text, "px:%d %d\n", x, y);
        
        // TODO: more criteria here, can have 2 channel PBR metal-roughness
        // also have 4 channel normals where zw store other data.
        bool isNormal2Channel = numChannels == 2;
        if (isNormal2Channel) {
            numChannels = 3;
            
            float x = c.x;
            float y = c.y;
            
            if (!isSigned) {
                x = x * 2.0f - 1.0f;
                y = y * 2.0f - 1.0f;
            }
            
            c.z = sqrt(1.0f - std::min(x * x + y * y, 1.0f));
            
            // back to unorm
            if (!isSigned) {
                c.z = c.z * 0.5f + 0.5f;
            }
            
            // should we indicate z channel is reconstruct?
        }
        
        // TODO: write some print helpers based on float4 and length
        bool isFloat = isHdr;
        string tmp;
        printChannels(tmp, "l: ", c, numChannels, isFloat, isSigned);
        text += tmp;
        
        if (isSrgb) {
            printChannels(tmp, "s: ", s, numChannels, isFloat, isSigned);
            text += tmp;
        }
    
        [self setEyedropperText:text.c_str()];

        // TODO: range display of pixels is useful, only showing pixels that fall
        // within a given range, but would need slider then, and determine range of pixels.
        // TODO: Auto-range is also useful for depth (ignore far plane of 0 or 1).
        
        // TOOD: display histogram from compute, bin into buffer counts of pixels
        
        // TODO: stop clobbering hud text, need another set of labels
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

    // pan
    wheelY = -wheelY;
    wheelX = -wheelX;

    float panX = gShowSettings.panX + wheelX;
    float panY = gShowSettings.panY + wheelY;
    
    Renderer* renderer = (Renderer*)self.delegate;
    float4x4 projectionViewModelMatrix = [renderer computeImageTransform:panX panY:panY zoom:gShowSettings.zoom];

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
   
    if (!NSIntersectsRect(imageRect, viewRect)) {
        return;
    }
    
    if (doPrintPanZoom) {
        KLOGI("kramv", "Pan %.3f,%.3f", panX, panY);
    }
    
    if (gShowSettings.panX != panX ||
        gShowSettings.panY != panY)
    {
        gShowSettings.panX = panX;
        gShowSettings.panY = panY;
        
        [self updateEyedropper];
        self.needsDisplay = YES;
    }
}

void advanceDebugMode(bool isShiftKeyDown) {
    int numEnums = DebugModeCount;
    if (isShiftKeyDown) {
        gShowSettings.debugMode = (DebugMode)(((int)gShowSettings.debugMode - 1 + numEnums) % numEnums);
    }
    else {
        gShowSettings.debugMode = (DebugMode)(((int)gShowSettings.debugMode + 1) % numEnums);
    }
    
    MyMTLPixelFormat format = (MyMTLPixelFormat)gShowSettings.originalFormat;
    bool isHdr = isHdrFormat(format);
    int numChannels = gShowSettings.numChannels;
    
    // DONE: work on skipping some of these based on image
    bool isAlpha = isAlphaFormat(format);
    bool isColor = isColorFormat(format);
    
    if (gShowSettings.debugMode == DebugModeTransparent && (numChannels <= 3 || !isAlpha)) {
        advanceDebugMode(isShiftKeyDown);
    }
    
    // 2 channel textures don't really color or grayscale pixels
    if (gShowSettings.debugMode == DebugModeColor && (numChannels <= 2 || !isColor)) {
        advanceDebugMode(isShiftKeyDown);
    }
    
    if (gShowSettings.debugMode == DebugModeGray && numChannels <= 2) {
        advanceDebugMode(isShiftKeyDown);
    }
    
    if (gShowSettings.debugMode == DebugModeHDR && !isHdr) {
        advanceDebugMode(isShiftKeyDown);
    }
    
    // for 3 and for channel textures could skip these with more info about image (hasColor)
    // if (gShowSettings.debugMode == DebugModeGray && !hasColor) advanceDebugMode(isShiftKeyDown);
    
    bool isNormal = numChannels == 2; // TODO: better criteria on this
    
    // for normals show directions
    if (gShowSettings.debugMode == DebugModePosX && !isNormal) {
        advanceDebugMode(isShiftKeyDown);
    }
    if (gShowSettings.debugMode == DebugModePosY && !isNormal) {
        advanceDebugMode(isShiftKeyDown);
    }
    
    // TODO: have a clipping mode against a variable range too, only show pixels within that range
    // to help isolate problem pixels.  Useful for depth, and have auto-range scaling for it and hdr.
    // make sure to ignore 0 or 1 for clear color of farPlane.
    
}

- (void)keyDown:(NSEvent *)theEvent
{
    // Some data depends on the texture data (isSigned, isNormal, ..)
    TextureChannels& channels = gShowSettings.channels;
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
            advanceDebugMode(isShiftKeyDown);
            
            switch(gShowSettings.debugMode) {
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
            text = "RGBA, Hud,L-reload,O-preview,0-fit,E-debug\n"
                   "Checker,D-grid,Info\n"
                   "W-wrap,Premul,N-signed\n"
                   "Mip,Face,Y-array/slice";
            break;
            
        case Key::Zero: { // scale and reset pan
            float zoom;
            // fit image or mip
            if (isShiftKeyDown) {
                zoom = 1.0f;
            }
            else {
                // fit to topmost image
                zoom = gShowSettings.zoomFit;
            }
            
            // This zoom needs to be checked against zoom limits
            // there's a cap on the zoom multiplier.
            // This is reducing zoom which expands the image.
            zoom *= 1.0f / (1 << gShowSettings.mipLOD);
            
            // even if zoom same, still do this since it resets the pan
            gShowSettings.zoom = zoom;
            
            gShowSettings.panX = 0.0f;
            gShowSettings.panY = 0.0f;
            
            if (doPrintPanZoom) {
                KLOGI("kramv", "Zoom %.3f, pan %.3f,%.3f", gShowSettings.zoom, gShowSettings.panX, gShowSettings.panY);
            }
            
            isChanged = true;
            text = "Scale Image";
            break;
        }
        // reload key (also a quick way to reset the settings)
        case Key::L:
            [self loadTextureFromURL:self.imageURL];
            
            // reload at actual size
            if (isShiftKeyDown) {
                gShowSettings.zoom = 1.0f;
            }
            
            if (doPrintPanZoom) {
                KLOGI("kramv", "Zoom %.3f, pan %.3f,%.3f", gShowSettings.zoom, gShowSettings.panX, gShowSettings.panY);
            }
            
            isChanged = true;
            text = "Reload Image";
            break;
            
        // P already used for premul
        case Key::O:
            gShowSettings.isPreview = !gShowSettings.isPreview;
            isChanged = true;
            text = "Preview ";
            text += gShowSettings.isPreview ? "On" : "Off";
            break;
            
        // TODO: might switch c to channel cycle, so could just hit that
        // and depending on the content, it cycles through reasonable channel masks
            
        // TODO: reset zoom off cmd+0 ?
    
        // toggle checkerboard for transparency
        case Key::C:
            gShowSettings.isCheckerboardShown = !gShowSettings.isCheckerboardShown;
            isChanged = true;
            text = "Checker ";
            text += gShowSettings.isCheckerboardShown ? "On" : "Off";
            break;
        
        // toggle pixel grid when magnified above 1 pixel, can happen from mipmap changes too
        case Key::D:
            // TODO: display how many blocks there are
            if (isShiftKeyDown && gShowSettings.blockX > 1) {
                // if block size is 1, then this shouldn't toggle
                gShowSettings.isBlockGridShown = !gShowSettings.isBlockGridShown;
                gShowSettings.isPixelGridShown = false;
                sprintf(text, "Block Grid %dx%d %s",
                        gShowSettings.blockX, gShowSettings.blockY,
                        gShowSettings.isBlockGridShown ? "On" : "Off");
            }
            else {
               
                gShowSettings.isPixelGridShown = !gShowSettings.isPixelGridShown;
                gShowSettings.isBlockGridShown = false;
                text = "Pixel Grid ";
                text += gShowSettings.isPixelGridShown ? "On" : "Off";
            }
        
            isChanged = true;
            
            break;
        
        // toggle hud that shows name and pixel value under the cursor
        // this may require calling setNeedsDisplay on the UILabel as cursor moves
        case Key::H:
            gShowSettings.isHudShown = !gShowSettings.isHudShown;
            _hudLabel.hidden = !gShowSettings.isHudShown;
            _hudLabel2.hidden = !gShowSettings.isHudShown;
            //isChanged = true;
            text = "Hud ";
            text += gShowSettings.isHudShown ? "On" : "Off";
            break;
            
        // info on the texture, could request info from lib, but would want to cache that info
        case Key::I:
            if (gShowSettings.isHudShown) {
                sprintf(text, "%s", gShowSettings.imageInfo.c_str());
            }
            break;
        
        // toggle wrap/clamp
        case Key::W:
            // TODO: cycle through all possible modes (clamp, repeat, mirror-once, mirror-repeat, ...)
            gShowSettings.isWrap = !gShowSettings.isWrap;
            isChanged = true;
            text = "Wrap ";
            text += gShowSettings.isWrap ? "On" : "Off";
            break;
            
        // toggle signed vs. unsigned
        case Key::N:
            gShowSettings.isSigned = !gShowSettings.isSigned;
            isChanged = true;
            text = "Signed ";
            text += gShowSettings.isSigned ? "On" : "Off";
            break;
            
        // toggle premul alpha vs. unmul
        case Key::P:
            gShowSettings.isPremul = !gShowSettings.isPremul;
            isChanged = true;
            text = "Premul ";
            text += gShowSettings.isPremul ? "On" : "Off";
            break;
            
            
        // mip up/down
        case Key::M:
            if (isShiftKeyDown) {
                gShowSettings.mipLOD = MAX(gShowSettings.mipLOD - 1, 0);
            }
            else {
                gShowSettings.mipLOD = MIN(gShowSettings.mipLOD + 1, gShowSettings.maxLOD - 1);
            }
            sprintf(text, "Mip %d/%d", gShowSettings.mipLOD, gShowSettings.maxLOD);
            isChanged = true;
            break;
           
        case Key::F:
            // cube or cube array, but hit s to pick cubearray
            if (gShowSettings.faceCount) {
                if (isShiftKeyDown) {
                    gShowSettings.faceNumber = MAX(gShowSettings.faceNumber - 1, 0);
                }
                else {
                    gShowSettings.faceNumber = MIN(gShowSettings.faceNumber + 1, gShowSettings.faceCount - 1);
                }
                sprintf(text, "Face %d/%d", gShowSettings.faceNumber, gShowSettings.faceCount);
                isChanged = true;
            }
            break;
            
        
        case Key::Y:
            // slice
            if (gShowSettings.sliceCount > 1) {
                if (isShiftKeyDown) {
                    gShowSettings.sliceNumber = MAX(gShowSettings.sliceNumber - 1, 0);
                }
                else {
                    gShowSettings.sliceNumber = MIN(gShowSettings.sliceNumber + 1, gShowSettings.sliceCount - 1);
                }
                sprintf(text, "Slice %d/%d", gShowSettings.sliceNumber, gShowSettings.sliceCount);
                isChanged = true;
            }
            // array
            else if (gShowSettings.arrayCount > 1) {
                if (isShiftKeyDown) {
                    gShowSettings.arrayNumber = MAX(gShowSettings.arrayNumber - 1, 0);
                }
                else {
                    gShowSettings.arrayNumber = MIN(gShowSettings.arrayNumber + 1, gShowSettings.arrayCount - 1);
                }
                sprintf(text, "Array %d/%d", gShowSettings.arrayNumber, gShowSettings.arrayCount);
                isChanged = true;
            }
            break;
            
        // srgb - this was removed, doesn't correctly reflect enabling/disabling srgb
        // thought it might be useful if image used srgb and shouldn't.  f.e. on PNG where
        // srgb flag is always set, even when it's not indended.
//        case Key::S:
//            gShowSettings.isSRGBShown = !gShowSettings.isSRGBShown;
//
//            text = "Srgb ";
//            text += gShowSettings.isSRGBShown ? "On" : "Off";
//
//            isChanged = true;
//            break;
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
    else if (endsWithExtension(filename, ".ktx")) {
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

    _renderer = [[Renderer alloc] initWithMetalKitView:_view];

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
