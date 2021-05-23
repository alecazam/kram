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
#include "KramMipper.h"
#include "KramMmapHelper.h"
#include "KramImage.h"
#include "KramViewerBase.h"
#include "KramVersion.h" // keep kramv version in sync with libkram
#include "KramZipHelper.h"

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

- (void)setHudText:(const char*)text;

@end

//-------------

@interface KramDocument : NSDocument

@end


@interface KramDocument ()

@end

@implementation KramDocument

- (instancetype)init {
    self = [super init];
    if (self) {
        // Add your subclass-specific initialization here.
    }
    return self;
}

+ (BOOL)autosavesInPlace {
    return NO; // YES;
}

// call when "new" called
- (void)makeWindowControllers {
    // Override to return the Storyboard file name of the document.
    //NSStoryboard* storyboard = [NSStoryboard storyboardWithName:@"Main" bundle:nil];
    //NSWindowController* controller = [storyboard instantiateControllerWithIdentifier:@"NameNeeded];
    //[self addWindowController:controller];
}


- (NSData *)dataOfType:(NSString *)typeName error:(NSError **)outError {
    // Insert code here to write your document to data of the specified type. If outError != NULL, ensure that you create and set an appropriate error if you return nil.
    // Alternatively, you could remove this method and override -fileWrapperOfType:error:, -writeToURL:ofType:error:, or -writeToURL:ofType:forSaveOperation:originalContentsURL:error: instead.
    [NSException raise:@"UnimplementedMethod" format:@"%@ is unimplemented", NSStringFromSelector(_cmd)];
    return nil;
}


- (BOOL)readFromURL:(NSURL *)url ofType:(NSString *)typeName error:(NSError **)outError {
    
    
    // called from OpenRecent documents menu
    
#if 0
    //MyMTKView* view = self.windowControllers.firstObject.window.contentView;
    //return [view loadTextureFromURL:url];
#else

    NSApplication* app = [NSApplication sharedApplication];
    MyMTKView* view = app.mainWindow.contentView;
    BOOL success = [view loadTextureFromURL:url];
    if (success)
    {
        [view setHudText:""];
    
        // DONE: this recent menu only seems to work the first time
        // and not in subsequent calls to the same entry.  readFromUrl isn't even called.
        // So don't get a chance to switch back to a recent texture.
        // Maybe there's some list of documents created and so it doesn't
        // think the file needs to be reloaded.
        //
        // Note: if I return NO from this call then a dialog pops up that image
        // couldn't be loaded, but then the readFromURL is called everytime a new
        // image is picked from the list.
        
        // Clear the document list so readFromURL keeps getting called
        // Can't remove currentDoc, so have to skip that
        NSDocumentController* dc = [NSDocumentController sharedDocumentController];
        NSDocument* currentDoc = dc.currentDocument;
        NSMutableArray* docsToRemove = [[NSMutableArray alloc] init];
        for (NSDocument* doc in dc.documents) {
            if (doc != currentDoc)
                [docsToRemove addObject: doc];
        }
        
        for (NSDocument* doc in docsToRemove) {
            [dc removeDocument: doc];
        }
    }
    
    return success;
#endif
}


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
    // this is called from "Open In..."
    MyMTKView* view = sender.mainWindow.contentView;
    
    // TODO: if more than one url dropped, and they are albedo/nmap, then display them
    // together with the single uv set.  Need controls to show one or all together.
    
    // TODO: also do an overlapping diff if two files are dropped with same dimensions.
    
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
    Num1                 = 0x12,
    Num2                 = 0x13,
    Num3                 = 0x14,
    Num4                 = 0x15,
    // ...
    Num0                 = 0x1D,
    
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
    
    // This is really only useful for variable block size formats like ASTC
    // maybe some value in BC7 to BC1 comparison (original vs. BC7 vs. BC1)
 
     // TODO: have to encode and then decode astc/etc on macOS-Intel
     // load png and keep it around, and then call encode and then diff the image against the original pixels
     // 565 will always differ from the original.
     
     // Once encode generated, then cache result, only ever display two textures
     // in a comparison mode. Also have eyedropper sample from second texture
     // could display src vs. encode, or cached textures against each other
     // have PSNR too ?
     
    // see if the format is in cache
    
    // encode incremented format and cache, that way don't wait too long
    // and once all encode formats generated, can cycle through them until next image loaded
    
    // Could reuse the same buffer for all ASTC formats, larger blocks always need less mem
    //KramImage image; // TODO: move encode to KTXImage, convert png to one layer KTXImage
    //image.open(...);
    //image.encode(dstImage);
    //decodeIfNeeded(dstImage, dstImageDecoded);
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
    NSMenu* _viewMenu; // really the items
    NSStackView* _buttonStack;
    NSMutableArray<NSButton*>* _buttonArray;
    NSTextField* _hudLabel;
    NSTextField* _hudLabel2;
    
    vector<string> _textSlots;
    ShowSettings* _showSettings;
    
    // allow zip files to be dropped and opened, and can advance through bundle content
    ZipHelper _zip;
    MmapHelper _zipMmap;
    int32_t _fileIndex;
    BOOL _noImageLoaded;
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
       
    _buttonArray = [[NSMutableArray alloc] init];
    _buttonStack = [self _addButtons];
    
    // hide until image loaded
    _buttonStack.hidden = YES;
    _noImageLoaded = YES;
    
    _hudLabel2 = [self _addHud:YES];
    _hudLabel  = [self _addHud:NO];
    [self setHudText:""];
   
    return self;
}

- (nonnull ShowSettings*)showSettings {
    return _showSettings;
}

- (NSStackView*)_addButtons {
    const int32_t numButtons = 25; // 13;
    const char* names[numButtons*2] = {
        
        "?", "Help",
        "I", "Info",
        "H", "Hud",
        "S", "Show All",
        
        "O", "Preview",
        "W", "Repeat",
        "P", "Premul",
        "N", "Signed",
        
        "-", "",
    
        "E", "Debug",
        "D", "Grid",
        "C", "Checker",
        "U", "Toggle UI",
      
        "-", "",
        
        "M", "Mip",
        "F", "Face",
        "Y", "Array",
        "J", "Next",
        "L", "Reload",
        "0", "Fit",
        
        // TODO: need to shift hud over a little
        // "UI", - add to show/hide buttons
    
        "-", "",
        
        // make these individual toggles and exclusive toggle off shift
        "R", "Red",
        "G", "Green",
        "B", "Blue",
        "A", "Alpha",
    };
    
    NSRect rect = NSMakeRect(0,10,30,30);
    
    //#define ArrayCount(x) ((x) / sizeof(x[0]))
    
    NSMutableArray* buttons = [[NSMutableArray alloc] init];
    
    for (int32_t i = 0; i < numButtons; ++i) {
        const char* icon = names[2*i+0];
        const char* tip = names[2*i+1];
        
        NSString* name = [NSString stringWithUTF8String:icon];
        NSString* toolTip = [NSString stringWithUTF8String:tip];
        
        NSButton* button = nil;
        
        button = [NSButton buttonWithTitle:name target:self action:@selector(handleAction:)];
        [button setToolTip:toolTip];
        button.hidden = NO;
        
        // turn off rounded bezel
        button.bordered = NO;
        
        [button setFrame:rect];
        
        // stackView seems to disperse the items evenly across the area, so this doesn't work
        if (icon[0] == '-') {
            //rect.origin.y += 11;
            button.enabled = NO;
        }
        else {
            //sKrect.origin.y += 25;
            
            // keep all buttons, since stackView will remove and pack the stack
            [_buttonArray addObject:button];
        }
        
        [buttons addObject:button];
        
        
    }
    
    NSStackView* stackView = [NSStackView stackViewWithViews:buttons];
    stackView.orientation = NSUserInterfaceLayoutOrientationVertical;
    stackView.detachesHiddenViews = YES; // default, but why have to have _buttonArrary
    [self addSubview: stackView];
    
#if 1
    // Want menus, so user can define their own shortcuts to commands
    // Also need to enable/disable this via validateUserInterfaceItem
    NSApplication* app = [NSApplication sharedApplication];

    NSMenu* mainMenu = app.mainMenu;
    NSMenuItem* viewMenuItem = mainMenu.itemArray[2];
    _viewMenu = viewMenuItem.submenu;
    
    // TODO: add a view menu in the storyboard
    //NSMenu* menu = app.windowsMenu;
    //[menu addItem:[NSMenuItem separatorItem]];

    for (int32_t i = 0; i < numButtons; ++i) {
        const char* icon = names[2*i+0]; // single char
        const char* title = names[2*i+1];
    
        NSString* toolTip = [NSString stringWithUTF8String:icon];
        NSString* name = [NSString stringWithUTF8String:title];
        NSString* shortcut = @""; // for now, or AppKit turns key int cmd+shift+key
        
        if (icon[0] == '-') {
            [_viewMenu addItem:[NSMenuItem separatorItem]];
        }
        else {
            NSMenuItem* menuItem = [[NSMenuItem alloc] initWithTitle:name action:@selector(handleAction:) keyEquivalent:shortcut];
            menuItem.toolTip = toolTip; // use in findMenuItem
            
            // TODO: menus and buttons should reflect any toggle state
            // menuItem.state = Mixed/Off/On;
            
            [_viewMenu addItem: menuItem];
        }
    }
    
    [_viewMenu addItem:[NSMenuItem separatorItem]];
#endif
    
    return stackView;
}

- (NSTextField*)_addHud:(BOOL)isShadow
{
    // TODO: This text field is clamping to the height, so have it set to 1200.
    // really want field to expand to fill the window height for large output
    
    // add a label for the hud
    NSTextField *label = [[NSTextField alloc] initWithFrame:NSMakeRect(isShadow ? 21 : 20, isShadow ? 21 : 20, 800, 1200)];
    label.drawsBackground = NO;
    label.textColor = !isShadow ?
        [NSColor colorWithSRGBRed:0 green:1 blue:0 alpha:1] :
        [NSColor colorWithSRGBRed:0 green:0 blue:0 alpha:1];
    label.bezeled = NO;
    label.editable = NO;
    label.selectable = NO;
    label.lineBreakMode = NSLineBreakByClipping;
    label.maximumNumberOfLines = 0; // fill to height
    
    label.cell.scrollable = NO;
    label.cell.wraps = NO;

    label.hidden = !_showSettings->isHudShown;
    //label.font = NSFont.systemFont(ofSize: NSFont.systemFontSize(for: label.controlSize))

    // UILabel has shadowColor/shadowOffset but NSTextField doesn't
    
    [self addSubview: label];
    
    // add vertical constrains to have it fill window, but keep 800 width
    label.preferredMaxLayoutWidth = 800;

    //NSDictionary* views = @{ @"label" : label };
    //[self addConstraints:[NSLayoutConstraint constraintsWithVisualFormat:@"H:|-[label]" options:0 metrics:nil views:views]];
    //[self addConstraints:[NSLayoutConstraint constraintsWithVisualFormat:@"V:|-[label]" options:0 metrics:nil views:views]];
    
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

    float4 pixel = mInv * float4m(clipPoint.x, clipPoint.y, 1.0f, 1.0f);
    //pixel /= pixel.w; // in case perspective used

    // allow pan to extend to show all
    float maxX = 0.5f;
    float minY = -0.5f;
    if (_showSettings->isShowingAllLevelsAndMips) {
        maxX += 1.0f * (_showSettings->totalChunks() - 1);
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
    float4 bottomLeftCorner = float4m(-0.5, -0.5f, 0.0f, 1.0f);
    float4 topRightCorner = float4m(0.5, 0.5f, 0.0f, 1.0f);
    
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
   
    int32_t numTexturesX = _showSettings->totalChunks();
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

inline float4 toPremul(const float4& c)
{
    // premul with a
    float4 cpremul = c;
    float a = c.a;
    cpremul.w = 1.0f;
    cpremul *= a;
    return cpremul;
}

// Writing out to rgba32 for sampling, but unorm formats like ASTC and RGBA8
// are still off and need to use the following.
float toSnorm8(float c)
{
    return (255.0 / 127.0) * c - (128 / 127.0);
}

float2 toSnorm8(float2 c)
{
    return (255.0 / 127.0) * c - (128 / 127.0);
}

float3 toSnorm8(float3 c)
{
    return (255.0 / 127.0) * c - (128 / 127.0);
}
float4 toSnorm8(float4 c)
{
    return (255.0 / 127.0) * c - (128 / 127.0);
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
    
    float4 pixel = mInv * float4m(clipPoint.x, clipPoint.y, 1.0f, 1.0f);
    //pixel /= pixel.w; // in case perspective used
    
    // that's in model space (+/0.5f, +/0.5f), so convert to texture space
    pixel.y *= -1.0f;
    
    pixel.x += 0.5f;
    pixel.y += 0.5f;
    
    pixel.x *= 0.999f;
    pixel.y *= 0.999f;
    
    float uvX = pixel.x;
    float uvY = pixel.y;
    
    // pixels are 0 based
    pixel.x *= _showSettings->imageBoundsX;
    pixel.y *= _showSettings->imageBoundsY;
    
// TODO: finish this logic, need to account for gaps too, and then isolate to a given level and mip to sample
//    if (_showSettings->isShowingAllLevelsAndMips) {
//        pixel.x *= _showSettings->totalChunks();
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
        
        int32_t x = _showSettings->textureResultX;
        int32_t y = _showSettings->textureResultY;
        
        // show uv, so can relate to gpu coordinates stored in geometry and find atlas areas
        append_sprintf(text, "uv:%0.3f %0.3f\n",
            (float)x / _showSettings->imageBoundsX,
            (float)y / _showSettings->imageBoundsY
        );
        
        // pixel at top-level mip
        append_sprintf(text, "px:%d %d\n", x, y);
        
        // show block num
        int mipLOD = _showSettings->mipLOD;
        
        // TODO: these block numbers are not accurate on Toof at 4x4
        // there is resizing going on to the dimensions
        
        int mipX = _showSettings->imageBoundsX;
        int mipY = _showSettings->imageBoundsY;
        
        for (int i = 0; i < mipLOD; ++i) {
            mipX = mipX >> 1;
            mipY = mipY >> 1;
        }
        mipX = std::max(1, mipX);
        mipY = std::max(1, mipY);
        
        mipX = (int32_t)(uvX * mipX);
        mipY = (int32_t)(uvY * mipY);
        
        _showSettings->textureLookupMipX = mipX;
        _showSettings->textureLookupMipY = mipY;
        
        // TODO: may want to return mip in pixel readback
        // don't have it right now, so don't display if preview is enabled
        if (_showSettings->isPreview)
            mipLOD = 0;
        
        auto blockDims = blockDimsOfFormat(format);
        if (blockDims.x > 1)
            append_sprintf(text, "bpx: %d %d\n", mipX / blockDims.x, mipY / blockDims.y);
        
        // TODO: on astc if we have original blocks can run analysis from astc-encoder
        // about each block.
        
        // show the mip pixel (only if not preview and mip changed)
        if (mipLOD > 0 && !_showSettings->isPreview)
            append_sprintf(text, "mpx: %d %d\n", mipX, mipY);
        
        // TODO: more criteria here, can have 2 channel PBR metal-roughness
        // also have 4 channel normals where zw store other data.
        bool isNormal = _showSettings->isNormal;
        bool isFloat = isHdr;
        
        bool isDecodeSigned = isSignedFormat(_showSettings->decodedFormat);
        if (isSigned && !isDecodeSigned) {
            c = toSnorm8(c.x);
        }
        
        if (isNormal) {
            float nx = c.x;
            float ny = c.y;
            
            // unorm -> snorm
            if (!isSigned) {
                nx = toSnorm8(nx);
                ny = toSnorm8(ny);
            }
            
            // Note: not clamping nx,ny to < 1 like in shader
            
            // this is always postive on tan-space normals
            // assuming we're not viewing world normals
            const float maxLen2 = 0.999 * 0.999;
            float len2 = nx * nx + ny * ny;
            if (len2 > maxLen2)
                len2 = maxLen2;
            
            float nz = sqrt(1.0f - len2);
            
            // print the underlying color (some nmaps are xy in 4 channels)
            string tmp;
            printChannels(tmp, "ln: ", c, numChannels, isFloat, isSigned);
            text += tmp;
            
            // print direction
            float4 d = float4m(nx,ny,nz,0.0f);
            isFloat = true;
            isSigned = true;
            printChannels(tmp, "dr: ", d, 3, isFloat, isSigned);
            text += tmp;
        }
        else {
            // DONE: write some print helpers based on float4 and length
            string tmp;
            printChannels(tmp, "ln: ", c, numChannels, isFloat, isSigned);
            text += tmp;
            
            if (isSrgb) {
                // this saturates the value, so don't use for extended srgb
                float4 s = linearToSRGB(c);
                
                printChannels(tmp, "sr: ", s, numChannels, isFloat, isSigned);
                text += tmp;
            }
            
            // display the premul values too, but not fully transparent pixels
            if (c.a > 0.0 && c.a < 1.0f)
            {
                printChannels(tmp, "lnp: ", toPremul(c), numChannels, isFloat, isSigned);
                text += tmp;
                
                // TODO: do we need the premul srgb color too?
                if (isSrgb) {
                    // this saturates the value, so don't use for extended srgb
                    float4 s = linearToSRGB(c);
                    
                    printChannels(tmp, "srp: ", toPremul(s), numChannels, isFloat, isSigned);
                    text += tmp;
                }
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
    
    float4 pt0 = projectionViewModelMatrix * float4m(-0.5, -0.5f, 0.0f, 1.0f);
    float4 pt1 = projectionViewModelMatrix * float4m(0.5, 0.5f, 0.0f, 1.0f);
    
    // for perspective
    //pt0 /= pt0.w;
    //pt1 /= pt1.w;
    
    // see that rectangle intersects the view, view is -1 to 1
    CGRect imageRect = CGRectMake(pt0.x, pt0.y, pt1.x - pt0.x, pt1.y - pt0.y);
    CGRect viewRect = CGRectMake(-1.0f, -1.0f, 2.0f, 2.0f);
   
    int32_t numTexturesX = _showSettings->totalChunks();
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

- (NSButton*)findButton:(const char*)name {
    NSString* title = [NSString stringWithUTF8String:name];
    for (NSButton* button in _buttonArray) {
        if (button.title == title)
            return button;
    }
    return nil;
}

- (NSMenuItem*)findMenuItem:(const char*)name {
    NSString* title = [NSString stringWithUTF8String:name];
    
    for (NSMenuItem* menuItem in _viewMenu.itemArray) {
        if (menuItem.toolTip == title)
            return menuItem;
    }
    return nil;
}

// use this to enable/disable menus, buttons, etc.  Called on every event
// when not implemented, then user items are always enabled
- (BOOL)validateUserInterfaceItem:(id<NSValidatedUserInterfaceItem>)item
{
    // TODO: tie to menus and buttons states for enable/disable toggles
    // https://developer.apple.com/library/archive/documentation/Cocoa/Conceptual/MenuList/Articles/EnablingMenuItems.html
    
    // MTKView is not doc based, so can't all super
    //return [super validateUserInterfaceItem:anItem];
    
    return YES;
}

- (void)updateUIAfterLoad {
    
    // TODO: move these to actions, and test their state instead of looking up buttons
    // here and in HandleKey.
    
    // base on showSettings, hide some fo the buttons
    bool isShowAllHidden = _showSettings->totalChunks() <= 1 && _showSettings->maxLOD <= 1;
    
    bool isArrayHidden = _showSettings->arrayCount <= 1;
    bool isFaceSliceHidden = _showSettings->faceCount <= 1 && _showSettings->sliceCount <= 1;
    bool isMipHidden = _showSettings->maxLOD <= 1;
    
    bool isJumpToNextHidden = !_showSettings->isArchive;
    
    bool isGreenHidden = _showSettings->numChannels <= 1;
    bool isBlueHidden  = _showSettings->numChannels <= 2 && !_showSettings->isNormal; // reconstruct z = b on normals
    
    // TODO: also need a hasAlpha for pixels, since many compressed formats like ASTC always have 4 channels
    // but internally store R,RG01,... etc.  Can get more data from swizzle in the props.
    // Often alpha doesn't store anything useful to view.
    
    bool hasAlpha = _showSettings->numChannels >= 3;
    
    bool isAlphaHidden = !hasAlpha;
    bool isPremulHidden = !hasAlpha;
    bool isCheckerboardHidden = !hasAlpha;
   
    bool isSignedHidden = !isSignedFormat(_showSettings->originalFormat);
    
    // buttons
    [self findButton:"Y"].hidden = isArrayHidden;
    [self findButton:"F"].hidden = isFaceSliceHidden;
    [self findButton:"M"].hidden = isMipHidden;
    [self findButton:"S"].hidden = isShowAllHidden;
    [self findButton:"J"].hidden = isJumpToNextHidden;
    
    [self findButton:"G"].hidden = isGreenHidden;
    [self findButton:"B"].hidden = isBlueHidden;
    [self findButton:"A"].hidden = isAlphaHidden;
    
    [self findButton:"P"].hidden = isPremulHidden;
    [self findButton:"N"].hidden = isSignedHidden;
    [self findButton:"C"].hidden = isCheckerboardHidden;
    
    // menus (may want to disable, not hide)
    // problem is crashes since menu seems to strip hidden items
    // enabled state has to be handled in validateUserInterfaceItem
    [self findMenuItem:"Y"].hidden = isArrayHidden;
    [self findMenuItem:"F"].hidden = isFaceSliceHidden;
    [self findMenuItem:"M"].hidden = isMipHidden;
    [self findMenuItem:"S"].hidden = isShowAllHidden;
    [self findMenuItem:"J"].hidden = isJumpToNextHidden;
    
    [self findMenuItem:"G"].hidden = isGreenHidden;
    [self findMenuItem:"B"].hidden = isBlueHidden;
    [self findMenuItem:"A"].hidden = isAlphaHidden;
    
    [self findMenuItem:"P"].hidden = isPremulHidden;
    [self findMenuItem:"N"].hidden = isSignedHidden;
    [self findMenuItem:"C"].hidden = isCheckerboardHidden;
}


// TODO: convert to C++ actions, and then call into Base holding all this
// move pan/zoom logic too.  Then use that as start of Win32 kramv.

- (IBAction)handleAction:(id)sender {
    
    NSEvent* theEvent = [NSApp currentEvent];
    bool isShiftKeyDown = (theEvent.modifierFlags & NSEventModifierFlagShift);
    
    string title;
    
    // sender is the UI element/NSButton
    if ([sender isKindOfClass:[NSButton class]]) {
        NSButton* button = (NSButton*)sender;
        title = [button.title UTF8String];
    }
    else if ([sender isKindOfClass:[NSMenuItem class]]) {
        NSMenuItem* menuItem = (NSMenuItem*)sender;
        title = [menuItem.toolTip UTF8String];
    }
    else {
        KLOGE("kram", "unknown UI element");
        return;
    }
    
    int32_t keyCode = -1;
    
    if (title == "?")
        keyCode = Key::Slash; // help
    else if (title == "I")
        keyCode = Key::I;
    else if (title == "H")
        keyCode = Key::H;
    
    else if (title == "S")
        keyCode = Key::S;
    else if (title == "O")
        keyCode = Key::O;
    else if (title == "W")
        keyCode = Key::W;
    else if (title == "P")
        keyCode = Key::P;
    else if (title == "N")
        keyCode = Key::N;
    
    else if (title == "E")
        keyCode = Key::E;
    else if (title == "D")
        keyCode = Key::D;
    else if (title == "C")
        keyCode = Key::C;
    else if (title == "U")
        keyCode = Key::U;
    
    else if (title == "M")
        keyCode = Key::M;
    else if (title == "F")
        keyCode = Key::F;
    else if (title == "Y")
        keyCode = Key::Y;
    else if (title == "J")
        keyCode = Key::J;
    else if (title == "L")
        keyCode = Key::L;
    else if (title == "0")
        keyCode = Key::Num0;
    
    else if (title == "R")
        keyCode = Key::R;
    else if (title == "G")
        keyCode = Key::G;
    else if (title == "B")
        keyCode = Key::B;
    else if (title == "A")
        keyCode = Key::A;
    
    
    if (keyCode >= 0)
        [self handleKey:keyCode isShiftKeyDown:isShiftKeyDown];
}

- (void)keyDown:(NSEvent *)theEvent
{
    bool isShiftKeyDown = theEvent.modifierFlags & NSEventModifierFlagShift;
    uint32_t keyCode = theEvent.keyCode;

    [self handleKey:keyCode isShiftKeyDown:isShiftKeyDown];
}

- (void)handleKey:(uint32_t)keyCode isShiftKeyDown:(bool)isShiftKeyDown
{
    // Some data depends on the texture data (isSigned, isNormal, ..)
    TextureChannels& channels = _showSettings->channels;
    bool isChanged = false;
    
    // TODO: fix isChanged to only be set when value changes
    // f.e. clamped values don't need to re-render
    string text;
    
    switch(keyCode) {
        case Key::V: {
            bool isVertical = _buttonStack.orientation == NSUserInterfaceLayoutOrientationVertical;
            isVertical = !isVertical;
            
            _buttonStack.orientation = isVertical ? NSUserInterfaceLayoutOrientationVertical : NSUserInterfaceLayoutOrientationHorizontal;
            text = isVertical ? "Vert UI" : "Horiz UI";
            break;
        }
        case Key::U:
            // this means no image loaded yet
            if (_noImageLoaded) {
                return;
            }
            
            _buttonStack.hidden = !_buttonStack.hidden;
            text = _buttonStack.hidden ? "Hide UI" : "Show UI";
            break;
            
        // rgba channels
        case Key::Num1:
        case Key::R:
            if (![self findButton:"R"].isHidden) {
                if (channels == TextureChannels::ModeRRR1 || channels == TextureChannels::ModeR001) {
                    channels = TextureChannels::ModeRGBA;
                    text = "Mask RGBA";
                }
                else {
                    channels = isShiftKeyDown ? TextureChannels::ModeRRR1 : TextureChannels::ModeR001;
                    text = isShiftKeyDown ? "Mask RRR1" : "Mask R001";
                }
                isChanged = true;
            }
    
            break;
            
        case Key::Num2:
        case Key::G:
            if (![self findButton:"G"].isHidden) {
                if (channels == TextureChannels::ModeGGG1 || channels == TextureChannels::Mode0G01) {
                    channels = TextureChannels::ModeRGBA;
                    text = "Mask RGBA";
                }
                else {
                    channels = isShiftKeyDown ? TextureChannels::ModeGGG1 : TextureChannels::Mode0G01;
                    text = isShiftKeyDown ? "Mask GGG1" : "Mask 0G01";
                }
                isChanged = true;
            }
            break;
            
        case Key::Num3:
        case Key::B:
            if (![self findButton:"B"].isHidden) {
                if (channels == TextureChannels::ModeBBB1 || channels == TextureChannels::Mode00B1) {
                    channels = TextureChannels::ModeRGBA;
                    text = "Mask RGBA";
                }
                else {
                    channels = isShiftKeyDown ? TextureChannels::ModeBBB1 : TextureChannels::Mode00B1;
                    text = isShiftKeyDown ? "Mask BBB1" : "Mask 00B1";
                }
                isChanged = true;
            }
            break;
            
        case Key::Num4:
        case Key::A:
            if (![self findButton:"A"].isHidden) {
                if (channels == TextureChannels::ModeAAA1) {
                    channels = TextureChannels::ModeRGBA;
                    text = "Mask RGBA";
                }
                else {
                    channels = TextureChannels::ModeAAA1;
                    text = "Mask AAA1";
                }
                isChanged = true;
            }
            break;
            
        case Key::E: {
            _showSettings->advanceDebugMode(isShiftKeyDown);
            
            switch(_showSettings->debugMode) {
                case DebugModeNone: text = "Debug Off"; break;
                case DebugModeTransparent: text = "Debug Transparent"; break;
                case DebugModeNonZero: text = "Debug NonZero"; break;
                case DebugModeColor: text = "Debug Color"; break;
                case DebugModeGray: text = "Debug Gray"; break;
                case DebugModeHDR: text = "Debug HDR"; break;
                case DebugModePosX: text = "Debug +X"; break;
                case DebugModePosY: text = "Debug +Y"; break;
                case DebugModeCircleXY: text = "Debug XY>=1"; break;
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
                   "⇧Mip, ⇧Face, ⇧Y-array/slice\n"
                   "⇧J-next bundle image\n";
            break;
            
        case Key::Num0: { // scale and reset pan
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
            if (![self findButton:"C"].isHidden) {
                _showSettings->isCheckerboardShown = !_showSettings->isCheckerboardShown;
                isChanged = true;
                text = "Checker ";
                text += _showSettings->isCheckerboardShown ? "On" : "Off";
            }
            break;
        
        // toggle pixel grid when magnified above 1 pixel, can happen from mipmap changes too
        case Key::D: {
            static int grid = 0;
            static const int kNumGrids = 7;
            
            #define advanceGrid(g, dec) \
                grid = (grid + kNumGrids + (dec ? -1 : 1)) % kNumGrids
            
            // TODO: display how many blocks there are
                
            // if block size is 1, then this shouldn't toggle
            _showSettings->isBlockGridShown = false;
            _showSettings->isAtlasGridShown = false;
            _showSettings->isPixelGridShown = false;

            advanceGrid(grid, isShiftKeyDown);

            if (grid == 2 && _showSettings->blockX == 1) {
                // skip it
                advanceGrid(grid, isShiftKeyDown);
            }

            static const uint32_t gridSizes[kNumGrids] = {
                0, 1, 2,
                32, 64, 128, 256 // atlas sizes
            };

            if (grid == 0) {
                sprintf(text, "Grid Off");
            }
            else if (grid == 1) {
                _showSettings->isPixelGridShown = true;

                sprintf(text, "Pixel Grid 1x1 On");
            }
            else if (grid == 2) {
                _showSettings->isBlockGridShown = true;

                sprintf(text, "Block Grid %dx%d On",
                        _showSettings->blockX, _showSettings->blockY);
            }
            else {
                _showSettings->isAtlasGridShown = true;

                // want to be able to show altases tht have long entries derived from props
                // but right now just a square grid atlas
                _showSettings->gridSizeX =
                _showSettings->gridSizeY = gridSizes[grid];
                
                sprintf(text, "Atlas Grid %dx%d On",
                        _showSettings->gridSizeX, _showSettings->gridSizeY);
            }
            
            isChanged = true;
            
            break;
        }
        case Key::S:
            if (![self findButton:"S"].isHidden) {
            
                // TODO: have drawAllMips, drawAllLevels, drawAllLevelsAndMips
                _showSettings->isShowingAllLevelsAndMips = !_showSettings->isShowingAllLevelsAndMips;
                isChanged = true;
                text = "Show All ";
                text += _showSettings->isShowingAllLevelsAndMips ? "On" : "Off";
            }
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
                sprintf(text, "%s", isShiftKeyDown ? _showSettings->imageInfoVerbose.c_str() : _showSettings->imageInfo.c_str());
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
            if (![self findButton:"N"].isHidden) {
                _showSettings->isSigned = !_showSettings->isSigned;
                isChanged = true;
                text = "Signed ";
                text += _showSettings->isSigned ? "On" : "Off";
            }
            break;
            
        // toggle premul alpha vs. unmul
        case Key::P:
            if (![self findButton:"P"].isHidden) {
                _showSettings->isPremul = !_showSettings->isPremul;
                isChanged = true;
                text = "Premul ";
                text += _showSettings->isPremul ? "On" : "Off";
            }
            break;
            
        case Key::J:
            if (![self findButton:"J"].isHidden) {
                if ([self advanceTextureFromAchive:!isShiftKeyDown]) {
                    isChanged = true;
                    text = "Loaded " + _showSettings->lastFilename;
                }
            }
            break;
            
        // mip up/down
        case Key::M:
            if (_showSettings->maxLOD > 1) {
                if (isShiftKeyDown) {
                    _showSettings->mipLOD = MAX(_showSettings->mipLOD - 1, 0);
                }
                else {
                    _showSettings->mipLOD = MIN(_showSettings->mipLOD + 1, _showSettings->maxLOD - 1);
                }
                sprintf(text, "Mip %d/%d", _showSettings->mipLOD, _showSettings->maxLOD);
                isChanged = true;
            }
            break;
           
        case Key::F:
            // cube or cube array, but hit s to pick cubearray
            if (_showSettings->faceCount > 1) {
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
        
        // convert the original path and then back to a url, otherwise reload fails
        // when this file is replaced.
        const char* filename = url.fileSystemRepresentation;
        if (filename == nullptr)
        {
            KLOGE("kramv", "Fix this drop url returning nil issue");
            return NO;
        }
        
        NSString* filenameString = [NSString stringWithUTF8String:filename];
        
        url = [NSURL fileURLWithPath:filenameString];
        
        if ([self loadTextureFromURL:url]) {
            [self setHudText:""];
    
            return YES;
        }
   }

    return NO;
}


-(BOOL)loadArchive:(const char*)zipFilename
{
    // TODO: avoid loading the zip again if name and/or timestamp hasn't changed on it
    
    _zipMmap.close();
    if (!_zipMmap.open(zipFilename)) {
        return NO;
    }
   
    // Note: if mmap fails, could read entire zip into memory
    // and then still use the same code below.

    if (!_zip.openForRead(_zipMmap.data(), _zipMmap.dataLength())) {
        return NO;
    }
    
    // load the first entry in the archive
    _fileIndex = 0;
    
    return YES;
}

-(BOOL)advanceTextureFromAchive:(BOOL)increment
{
    if (!_zipMmap.data()) {
        // no archive loaded
        return NO;
    }
    
    // this advances through the fileIndex of a dropped
    size_t numEntries = _zip.zipEntrys().size();
    if (numEntries == 0) {
        return NO;
    }
    
    if (increment)
        _fileIndex = (_fileIndex + 1) % numEntries;
    else
        _fileIndex = (_fileIndex - 1 + numEntries) % numEntries;

    // now lookup the filename and data at that entry
    const auto& entry = _zip.zipEntrys()[_fileIndex];
    const char* filename = entry.filename;
    double timestamp = (double)entry.modificationDate;
    
    return [self loadTextureFromArchive:filename timestamp:timestamp];
}

- (BOOL)loadTextureFromArchive:(const char*)filename timestamp:(double)timestamp
{
    if (!(//endsWithExtension(filename, ".png") ||
          endsWithExtension(filename, ".ktx") ||
          endsWithExtension(filename, ".ktx2")) )
    {
        return NO;
    }
        
    const uint8_t* imageData = nullptr;
    uint64_t imageDataLength = 0;
    if (!_zip.extractRaw(filename, &imageData, imageDataLength)) {
        return NO;
    }
     
    string fullFilename = filename;
    Renderer* renderer = (Renderer*)self.delegate;
    if (![renderer loadTextureFromData:fullFilename timestamp:(double)timestamp imageData:imageData imageDataLength:imageDataLength]) {
        return NO;
    }
    
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
    title += formatTypeName(_showSettings->originalFormat);
    title += " - ";
    title += filenameShort;
    
    self.window.title = [NSString stringWithUTF8String: title.c_str()];
        
    // doesn't set imageURL or update the recent document menu
    
    // show the controls
    if (_noImageLoaded) {
        _buttonStack.hidden = NO; // show controls
        _noImageLoaded = NO;
    }

    _showSettings->isArchive = false;
   
    // show/hide button
    [self updateUIAfterLoad];
    
    self.needsDisplay = YES;
    return YES;
}
    
- (BOOL)loadTextureFromURL:(NSURL*)url {
    //NSLog(@"LoadTexture");
    
    const char* filename = url.fileSystemRepresentation;
    if (filename == nullptr)
    {
        // Fixed by converting dropped urls into paths then back to a url.
        // When file replaced the drop url is no longer valid.
        KLOGE("kramv", "Fix this load url returning nil issue");
        return NO;
    }
    
    if (endsWithExtension(filename, ".zip")) {
        if (!self.imageURL || ![self.imageURL isEqualTo:url]) {
            BOOL isArchiveLoaded = [self loadArchive:filename];
            
            if (!isArchiveLoaded) {
                return NO;
            }
            
            // store the
            self.imageURL = url;
        }
       
        // now reload the filename if needed
        const auto& entry = _zip.zipEntrys()[_fileIndex];
        const char* filename = entry.filename;
        double timestamp = entry.modificationDate;
        
        setErrorLogCapture(true);
        
        BOOL success = [self loadTextureFromArchive:filename timestamp:timestamp];
        
        if (!success) {
            string errorText;
            getErrorLogCaptureText(errorText);
            setErrorLogCapture(false);
            
            // prepend filename
            string finalErrorText;
            append_sprintf(finalErrorText,
                           "Could not load from archive:\n %s\n", filename);
            finalErrorText += errorText;
            
            [self setHudText: finalErrorText.c_str()];
        }
        
        setErrorLogCapture(false);
        return success;
    }
        
    if (!(endsWithExtension(filename, ".png") ||
          endsWithExtension(filename, ".ktx") ||
          endsWithExtension(filename, ".ktx2")) )
    {
        string errorText = "Unsupported file extension, must be .zip, .png, .ktx, ktx2\n";
        
        string finalErrorText;
        append_sprintf(finalErrorText,
                       "Could not load from archive:\n %s\n", filename);
        finalErrorText += errorText;
        
        [self setHudText: finalErrorText.c_str()];
        return NO;
    }
        
    Renderer* renderer = (Renderer*)self.delegate;
    setErrorLogCapture(true);
    
    BOOL success = [renderer loadTexture:url];
    
    if (!success) {
        string errorText;
        getErrorLogCaptureText(errorText);
        setErrorLogCapture(false);
        
        // prepend filename
        string finalErrorText;
        append_sprintf(finalErrorText,
                       "Could not load from file\n %s\n", filename);
        finalErrorText += errorText;
       
        [self setHudText: finalErrorText.c_str()];
        return NO;
    }
    setErrorLogCapture(false);
    
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
    title += formatTypeName(_showSettings->originalFormat);
    title += " - ";
    title += filenameShort;
    
    self.window.title = [NSString stringWithUTF8String: title.c_str()];
    
     // topmost entry will be the recently opened document
    // some entries may go stale if directories change, not sure who validates the list
    
    // add to recent document menu
    NSDocumentController* dc = [NSDocumentController sharedDocumentController];
    [dc noteNewRecentDocumentURL:url];

    self.imageURL = url;
    
    // show the controls
    if (_noImageLoaded) {
        _buttonStack.hidden = NO; // show controls
        _noImageLoaded = NO;
    }
    
    _showSettings->isArchive = false;
   
    // show/hide button
    [self updateUIAfterLoad];
    
    self.needsDisplay = YES;
    return YES;
}


- (void)concludeDragOperation:(id)sender {
    // did setNeedsDisplay, but already doing that in loadTextureFromURL
}



// this doesn't seem to enable New.  Was able to get "Open" to highlight by setting NSDocument as class for doc types.
// https://developer.apple.com/library/archive/documentation/Cocoa/Conceptual/EventOverview/EventArchitecture/EventArchitecture.html
#if 0
/*
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
*/
#endif


- (BOOL)acceptsFirstResponder {
    return YES;
}


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
    
    // programmatically add some buttons
    // think limited to 11 viewws before they must be wrapepd in a container.  That's how SwiftUI was.
    
}


@end


//-------------

int main(int argc, const char * argv[]) {
    @autoreleasepool {
        // Setup code that might create autoreleased objects goes here.
    }
    return NSApplicationMain(argc, argv);
}
