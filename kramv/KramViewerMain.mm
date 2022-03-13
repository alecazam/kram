// kram - Copyright 2020-2022 by Alec Miller. - MIT License
// The license and copyright notice shall be included
// in all copies or substantial portions of the Software.

// using -fmodules and -fcxx-modules
@import Cocoa;
@import Metal;
@import MetalKit;

//#import <Cocoa/Cocoa.h>
//#import <Metal/Metal.h>
//#import <MetalKit/MetalKit.h>
//#import <TargetConditionals.h>

#import "KramRenderer.h"
#import "KramShaders.h"

// C++
#include "KramLib.h"
#include "KramVersion.h"  // keep kramv version in sync with libkram

//#include "KramMipper.h"

//#include "Kram.h"
//#include "KramFileHelper.h"
//#include "KramMmapHelper.h"
//#include "KramZipHelper.h"

//#include "KramImage.h"
#include "KramViewerBase.h"


static bool doReplaceSrgbFromType = true;

#ifdef NDEBUG
static bool doPrintPanZoom = false;
#else
static bool doPrintPanZoom = false;
#endif

using namespace simd;
using namespace kram;
using namespace NAMESPACE_STL;

struct MouseData
{
    NSPoint originPoint;
    NSPoint oldPoint;
    NSPoint newPoint;
    
    NSPoint pan;
};

// this aliases the existing string, so can't chop extension
inline const char* toFilenameShort(const char* filename) {
    const char* filenameShort = strrchr(filename, '/');
    if (filenameShort == nullptr) {
        filenameShort = filename;
    }
    else {
        filenameShort += 1;
    }
    return filenameShort;
}

//-------------

enum Key {
    A = 0x00,
    S = 0x01,
    D = 0x02,
    F = 0x03,
    H = 0x04,
    G = 0x05,
    Z = 0x06,
    X = 0x07,
    C = 0x08,
    V = 0x09,
    B = 0x0B,
    Q = 0x0C,
    W = 0x0D,
    E = 0x0E,
    R = 0x0F,
    Y = 0x10,
    T = 0x11,
    O = 0x1F,
    U = 0x20,
    I = 0x22,
    P = 0x23,
    L = 0x25,
    J = 0x26,
    K = 0x28,
    N = 0x2D,
    M = 0x2E,

    // https://eastmanreference.com/complete-list-of-applescript-key-codes
    Num1 = 0x12,
    Num2 = 0x13,
    Num3 = 0x14,
    Num4 = 0x15,
    Num5 = 0x17,
    Num6 = 0x16,
    Num7 = 0x1A,
    Num8 = 0x1C,
    Num9 = 0x19,
    Num0 = 0x1D,

    LeftBrace = 0x21,
    RightBrace = 0x1E,

    LeftBracket = 0x21,
    RightBracket = 0x1E,

    Quote = 0x27,
    Semicolon = 0x29,
    Backslash = 0x2A,
    Comma = 0x2B,
    Slash = 0x2C,

    LeftArrow = 0x7B,
    RightArrow = 0x7C,
    DownArrow = 0x7D,
    UpArrow = 0x7E,
    
    Space = 0x31,
    Escape = 0x35,
};


// This makes dealing with ui much simpler
class Action {
public:
    Action(const char* icon_, const char* tip_, Key keyCode_): icon(icon_), tip(tip_), keyCode(keyCode_) {}
    
    const char* icon;
    const char* tip;

    // Note these are not ref-counted, but AppKit already does
    id button; // NSButton*
    id menuItem; // NSMenuItem*
    Key keyCode;
    
    bool isHighlighted = false;
    bool isHidden = false;
    bool isButtonDisabled = false;
    
    void setHighlight(bool enable) {
        isHighlighted = enable;
        
        auto On = NSControlStateValueOn;
        auto Off = NSControlStateValueOff;
        
        if (!isButtonDisabled) {
            ((NSButton*)button).state = enable ? On : Off;
        }
        ((NSMenuItem*)menuItem).state = enable ? On : Off;
    }
    
    void setHidden(bool enable) {
        isHidden = enable;
        
        if (!isButtonDisabled) {
            ((NSButton*)button).hidden = enable;
        }
        ((NSMenuItem*)menuItem).hidden = enable;
    }
    
    void disableButton() {
        ((NSButton*)button).hidden = true;
        isButtonDisabled = true;
    }
};

//-------------

// This is so annoying, but otherwise the hud always intercepts clicks intended
// for the underlying TableView.
// https://stackoverflow.com/questions/15891098/nstextfield-click-through
@interface MyNSTextField : NSTextField

@end

@implementation MyNSTextField
{
    
}

// override to allow clickthrough
- (NSView*)hitTest:(NSPoint)aPoint
{
    return nil;
}

@end

//-------------

@interface MyMTKView : MTKView
// for now only have a single imageURL
@property(retain, nonatomic, readwrite, nullable) NSURL *imageURL;

//@property (nonatomic, readwrite, nullable) NSPanGestureRecognizer* panGesture;
@property(retain, nonatomic, readwrite, nullable)
    NSMagnificationGestureRecognizer *zoomGesture;

@property(nonatomic, readwrite) double lastArchiveTimestamp;

// can hide hud while list view is up
@property(nonatomic, readwrite) bool hudHidden;

// TODO: should be a part of document, but only one doc to a view
@property(nonatomic, readwrite) float originalZoom;
@property(nonatomic, readwrite) float validMagnification;
@property(nonatomic, readwrite) MouseData mouseData;


- (BOOL)loadTextureFromURL:(NSURL *)url;

- (void)setHudText:(const char *)text;

- (void)tableViewSelectionDidChange:(NSNotification *)notification;

- (void)addNotifications;

- (void)removeNotifications;

- (void)fixupDocumentList;

@end

//-------------

// https://medium.com/@kevingutowski/how-to-setup-a-tableview-in-2019-obj-c-c7dece203333
@interface TableViewController : NSObject <NSTableViewDataSource, NSTableViewDelegate>
@property (nonatomic, strong) NSMutableArray<NSString*>* items;
@end

@implementation TableViewController

- (instancetype)init {
    self = [super init];
    
    _items = [[NSMutableArray alloc] init];
    return self;
}

// NSTableViewDataSource
- (NSInteger)numberOfRowsInTableView:(NSTableView *)tableView
{
    return self.items.count;
}

// NSTableViewDelegate
-(NSView *)tableView:(NSTableView *)tableView viewForTableColumn:(NSTableColumn *)tableColumn row:(NSInteger)row
{
    NSString *identifier = tableColumn.identifier;
    NSTableCellView *cell = [tableView makeViewWithIdentifier:identifier owner:self];
    cell.textField.stringValue = [self.items objectAtIndex:row];
    return cell;
}

// NSTableViewDelegate
- (BOOL)tableView:(NSTableView *)tableView
shouldTypeSelectForEvent:(NSEvent *)event
withCurrentSearchString:(NSString *)searchString
{
    // Return NO to prevent type select (otherwise S or N key will search that key)
    // This is nice on long lists though.
    return NO;
}

- (void)tableViewSelectionDidChange:(NSNotification *)notification
{
   // does not need to respond, have a listener on this notification
}
@end

//-------------

@interface KramDocument : NSDocument

@end

@interface KramDocument ()

@end

@implementation KramDocument

- (instancetype)init
{
    self = [super init];
    if (self) {
        // Add your subclass-specific initialization here.
    }
    return self;
}

+ (BOOL)autosavesInPlace
{
    return NO;  // YES;
}

// call when "new" called
- (void)makeWindowControllers
{
    // Override to return the Storyboard file name of the document.
    // NSStoryboard* storyboard = [NSStoryboard storyboardWithName:@"Main"
    // bundle:nil]; NSWindowController* controller = [storyboard
    // instantiateControllerWithIdentifier:@"NameNeeded]; [self
    //addWindowController:controller];
}

- (NSData *)dataOfType:(nonnull NSString *)typeName
                 error:(NSError *_Nullable __autoreleasing *)outError
{
    // Insert code here to write your document to data of the specified type. If
    // outError != NULL, ensure that you create and set an appropriate error if
    // you return nil. Alternatively, you could remove this method and override
    // -fileWrapperOfType:error:, -writeToURL:ofType:error:, or
    // -writeToURL:ofType:forSaveOperation:originalContentsURL:error: instead.
    [NSException raise:@"UnimplementedMethod"
                format:@"%@ is unimplemented", NSStringFromSelector(_cmd)];
    return nil;
}



- (BOOL)readFromURL:(nonnull NSURL *)url
             ofType:(nonnull NSString *)typeName
              error:(NSError *_Nullable __autoreleasing *)outError
{
    // called from OpenRecent documents menu

#if 0
    //MyMTKView* view = self.windowControllers.firstObject.window.contentView;
    //return [view loadTextureFromURL:url];
#else

    // TODO: This is only getting called on first open on macOS 12.0 even with hack below.
    // find out why.
    
    NSApplication *app = [NSApplication sharedApplication];
    MyMTKView *view = app.mainWindow.contentView;
    BOOL success = [view loadTextureFromURL:url];
    if (success) {
        // Note: if I return NO from this call then a dialog pops up that image
        // couldn't be loaded, but then the readFromURL is called everytime a new
        // image is picked from the list.

        [view setHudText:""];
        [view fixupDocumentList];
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

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification
{
    // Insert code here to initialize your application
}

- (void)applicationWillTerminate:(NSNotification *)aNotification
{
    // Insert code here to tear down your application
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:
    (NSApplication *)sender
{
    return YES;
}

- (void)application:(NSApplication *)sender
           openURLs:(nonnull NSArray<NSURL *> *)urls
{
    // this is called from "Open In..."
    MyMTKView *view = sender.mainWindow.contentView;

    // TODO: if more than one url dropped, and they are albedo/nmap, then display
    // them together with the single uv set.  Need controls to show one or all
    // together.

    // TODO: also do an overlapping diff if two files are dropped with same
    // dimensions.

    NSURL *url = urls.firstObject;
    [view loadTextureFromURL:url];
    [view fixupDocumentList];
}



- (IBAction)showAboutDialog:(id)sender
{
    // calls openDocumentWithContentsOfURL above
    NSMutableDictionary<NSAboutPanelOptionKey, id> *options =
        [[NSMutableDictionary alloc] init];

    // name and icon are already supplied

    // want to embed the git tag here
    options[@"Copyright"] =
        [NSString stringWithUTF8String:"kram ©2020-2022 by Alec Miller"];

    // add a link to kram website, skip the Visit text
    NSMutableAttributedString *str = [[NSMutableAttributedString alloc]
        initWithString:@"https://github.com/alecazam/kram"];
    [str addAttribute:NSLinkAttributeName
                value:@"https://github.com/alecazam/kram"
                range:NSMakeRange(0, str.length)];

    [str appendAttributedString:
             [[NSAttributedString alloc]
                 initWithString:
                     [NSString
                         stringWithUTF8String:
                             "\n"
                             "kram is open-source and inspired by the\n"
                             "software technologies of these companies\n"
                             "  Khronos, Binomial, ARM, Google, and Apple\n"
                             "and devs who generously shared their work.\n"
                             "  Simon Brown, Rich Geldreich, Pete Harris,\n"
                             "  Philip Rideout, Romain Guy, Colt McAnlis,\n"
                             "  John Ratcliff, Sean Parent, David Ireland,\n"
                             "  Mark Callow, Mike Frysinger, Yann Collett\n"]]];

    options[NSAboutPanelOptionCredits] = str;

    // skip the v character
    const char *version = KRAM_VERSION;
    version += 1;

    // this is the build version, should be github hash?
    options[NSAboutPanelOptionVersion] = @"";

    // this is app version
    options[NSAboutPanelOptionApplicationVersion] =
        [NSString stringWithUTF8String:version];

    [[NSApplication sharedApplication]
        orderFrontStandardAboutPanelWithOptions:options];

    //[[NSApplication sharedApplication] orderFrontStandardAboutPanel:sender];
}

@end

// also NSPasteboardTypeURL
// also NSPasteboardTypeTIFF
NSArray<NSString *> *pasteboardTypes = @[ NSPasteboardTypeFileURL ];

@implementation MyMTKView {
    NSMenu *_viewMenu;  // really the items
    NSStackView *_buttonStack;
    NSMutableArray<NSButton *> *_buttonArray;
    NSTextField *_hudLabel;
    NSTextField *_hudLabel2;
    
    // Offer list of files in archives
    // TODO: move to NSOutlineView since that can show archive folders with content inside
    IBOutlet NSTableView *_tableView;
    IBOutlet TableViewController *_tableViewController;
    
    vector<string> _textSlots;
    ShowSettings *_showSettings;

    // allow zip files to be dropped and opened, and can advance through bundle
    // content
    ZipHelper _zip;
    MmapHelper _zipMmap;
    int32_t _fileArchiveIndex;
    BOOL _noImageLoaded;

    vector<string> _folderFiles;
    int32_t _fileFolderIndex;
    
    Action* _actionPlay;
    Action* _actionHelp;
    Action* _actionInfo;
    Action* _actionHud;
    Action* _actionShowAll;
    
    Action* _actionPreview;
    Action* _actionWrap;
    Action* _actionPremul;
    Action* _actionSigned;
    
    Action* _actionDebug;
    Action* _actionGrid;
    Action* _actionChecker;
    Action* _actionHideUI;
    Action* _actionVertical;
    
    Action* _actionMip;
    Action* _actionFace;
    Action* _actionArray;
    Action* _actionItem;
    Action* _actionPrevItem;
    Action* _actionReload;
    Action* _actionFit;
    
    Action* _actionShapeMesh;
    Action* _actionShapeChannel;
    Action* _actionLighting;
    Action* _actionTangent;
    
    Action* _actionR;
    Action* _actionG;
    Action* _actionB;
    Action* _actionA;
    
    vector<Action> _actions;
}

- (void)awakeFromNib
{
    [super awakeFromNib];

    // TODO: see if can only open this
    // NSLog(@"AwakeFromNIB");
}

// to get upper left origin like on UIView
#if KRAM_MAC
- (BOOL)isFlipped
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
// TODO: Sometimes getting panels from right side popping in when trying to pan
// on macOS without using pan gesture.

- (instancetype)initWithCoder:(NSCoder *)coder
{
    self = [super initWithCoder:coder];

    _showSettings = new ShowSettings;

    self.clearColor = MTLClearColorMake(0.0f, 0.0f, 0.0f, 0.0f);

    self.clearDepth = _showSettings->isReverseZ ? 0.0f : 1.0f;

    // only re-render when changes are made
    // Note: this breaks ability to gpu capture, since display link not running.
    // so disable this if want to do captures.  Or just move the cursor to capture.
#ifndef NDEBUG  // KRAM_RELEASE
    self.enableSetNeedsDisplay = YES;
#endif
    // openFile in appDelegate handles "Open in..."

    _textSlots.resize(2);
    
    // added for drag-drop support
    [self registerForDraggedTypes:pasteboardTypes];

    // This gesture only works for trackpad
    _zoomGesture = [[NSMagnificationGestureRecognizer alloc]
        initWithTarget:self
                action:@selector(handleGesture:)];
    [self addGestureRecognizer:_zoomGesture];

    _originalZoom = 1.0f;
    _validMagnification = 1.0f;

    _buttonArray = [[NSMutableArray alloc] init];
    _buttonStack = [self _addButtons];

    // hide until image loaded
    _buttonStack.hidden = YES;
    _noImageLoaded = YES;

    _hudLabel2 = [self _addHud:YES];
    _hudLabel = [self _addHud:NO];
    [self setHudText:""];
    
    return self;
}

- (nonnull ShowSettings *)showSettings
{
    return _showSettings;
}

-(void)fixupDocumentList
{
    // DONE: this recent menu only seems to work the first time
    // and not in subsequent calls to the same entry.  readFromUrl isn't even
    // called. So don't get a chance to switch back to a recent texture. Maybe
    // there's some list of documents created and so it doesn't think the file
    // needs to be reloaded.
   
    // Clear the document list so readFromURL keeps getting called
    // Can't remove currentDoc, so have to skip that
    NSDocumentController *dc = [NSDocumentController sharedDocumentController];
    NSDocument *currentDoc = dc.currentDocument;
    NSMutableArray *docsToRemove = [[NSMutableArray alloc] init];
    for (NSDocument *doc in dc.documents) {
        if (doc != currentDoc)
            [docsToRemove addObject:doc];
    }

    for (NSDocument *doc in docsToRemove) {
        [dc removeDocument:doc];
    }
}

- (NSStackView *)_addButtons
{
    // Don't reorder without also matching actionPtrs below
    Action actions[] = {
        Action("?", "Help", Key::Slash),
        Action("I", "Info", Key::I),
        Action("H", "Hud", Key::H),
        Action("U", "UI", Key::U),
        Action("V", "UI Vertical", Key::V),

        Action("D", "Debug", Key::D),
        Action("G", "Grid", Key::G),
        Action("B", "Checkerboard", Key::B),
        
        Action("", "", Key::A), // sep

        Action("P", "Preview", Key::P),
        Action("W", "Wrap", Key::W),
        Action("8", "Premul", Key::Num8),
        Action("7", "Signed", Key::Num7),
        
        Action("", "", Key::A), // sep

        Action("A", "Show All", Key::A),
        Action("M", "Mip", Key::M),
        Action("F", "Face", Key::F),
        Action("Y", "Array", Key::Y),
        
        Action("↑", "Prev Item", Key::UpArrow),
        Action("↓", "Next Item", Key::DownArrow),
        Action("R", "Reload", Key::R),
        Action("0", "Fit", Key::Num0),

        Action("", "", Key::A), // sep

        Action(" ", "Play", Key::Space), // TODO: really need icon on this
        Action("S", "Shape", Key::S),
        Action("C", "Shape Channel", Key::C),
        Action("L", "Lighting", Key::L),
        Action("T", "Tangents", Key::T),

        Action("", "", Key::A), // sep

        // make these individual toggles and exclusive toggle off shift
        Action("1", "Red", Key::Num1),
        Action("2", "Green", Key::Num2),
        Action("3", "Blue", Key::Num3),
        Action("4", "Alpha", Key::Num4),
    };
    
    // These have to be in same order as above.  May want to go back to search for text above.
    Action** actionPtrs[] = {
        &_actionHelp,
        &_actionInfo,
        &_actionHud,
        &_actionHideUI,
        &_actionVertical,
       
        &_actionDebug,
        &_actionGrid,
        &_actionChecker,
        
        &_actionPreview,
        &_actionWrap,
        &_actionPremul,
        &_actionSigned,
        
        &_actionShowAll,
        &_actionMip,
        &_actionFace,
        &_actionArray,
        
        &_actionPrevItem,
        &_actionItem,
        &_actionReload,
        &_actionFit,
        
        &_actionPlay,
        &_actionShapeMesh,
        &_actionShapeChannel,
        &_actionLighting,
        &_actionTangent,
        
        &_actionR,
        &_actionG,
        &_actionB,
        &_actionA,
    };
    
    NSRect rect = NSMakeRect(0, 10, 30, 30);

    #define ArrayCount(x) (sizeof(x) / sizeof(x[0]))

    int32_t numActions = ArrayCount(actions);
    
    NSMutableArray *buttons = [[NSMutableArray alloc] init];

    for (int32_t i = 0; i < numActions; ++i) {
        Action& action = actions[i];
        const char *icon = action.icon;
        const char *tip = action.tip;

        NSString *name = [NSString stringWithUTF8String:icon];
        NSString *toolTip = [NSString stringWithUTF8String:tip];

        NSButton *button = nil;

        button = [NSButton buttonWithTitle:name
                                    target:self
                                    action:@selector(handleAction:)];
        button.toolTip = toolTip;
        button.hidden = NO;

        button.buttonType = NSButtonTypeToggle;
        // NSButtonTypeOnOff
        button.bordered = NO;
        [button setFrame:rect];

        // stackView seems to disperse the items evenly across the area, so this
        // doesn't work
        bool isSeparator = icon[0] == 0;
        
        if (isSeparator) {
            // rect.origin.y += 11;
            button.enabled = NO;
        }
        else {
            action.button = button;
            
            // rect.origin.y += 25;

            // TODO: add icons
            //button.image = ...;
            
            // keep all buttons, since stackView will remove and pack the stack
            [_buttonArray addObject:button];
        }

        [buttons addObject:button];
    }

    NSStackView *stackView = [NSStackView stackViewWithViews:buttons];
    stackView.orientation = NSUserInterfaceLayoutOrientationVertical;
    stackView.detachesHiddenViews =
        YES;  // default, but why have to have _buttonArrary
    [self addSubview:stackView];

    // Want menus, so user can define their own shortcuts to commands
    // Also need to enable/disable this via validateUserInterfaceItem
    NSApplication *app = [NSApplication sharedApplication];

    NSMenu *mainMenu = app.mainMenu;
    NSMenuItem *viewMenuItem = mainMenu.itemArray[2];
    _viewMenu = viewMenuItem.submenu;

    // TODO: add a view menu in the storyboard
    // NSMenu* menu = app.windowsMenu;
    //[menu addItem:[NSMenuItem separatorItem]];

    for (int32_t i = 0; i < numActions; ++i) {
        Action& action = actions[i];
        const char *icon = action.icon;  // single char
        const char *title = action.tip;

        NSString *toolTip = [NSString stringWithUTF8String:icon];
        NSString *name = [NSString stringWithUTF8String:title];
        bool isSeparator = icon[0] == 0;
        
        if (isSeparator) {
            [_viewMenu addItem:[NSMenuItem separatorItem]];
        }
        else {
            // NSString *shortcut = @"";  // for now, or AppKit turns key int cmd+shift+key
            NSString *shortcut = [NSString stringWithUTF8String:icon];
            
            NSMenuItem *menuItem =
                [[NSMenuItem alloc] initWithTitle:name
                                           action:@selector(handleAction:)
                                    keyEquivalent:shortcut];
            menuItem.toolTip = toolTip;
            
            // All key-equivalents assume cmd, so unset cmd
            // still leaves shift next to keys, but better than nothing
            menuItem.keyEquivalentModifierMask = (NSEventModifierFlags)0;
            
            // TODO: add icons, also onStateImage, offStageImage, mixedStateImage
            //menuItem.image = ...;
             
            // can set an integer constant that represents menu that avoid testing string (actionID)
            //menuItem.tag = ...;
            
            // TODO: menus and buttons should reflect any toggle state
            // menuItem.state = Mixed/Off/On;

            [_viewMenu addItem:menuItem];
            
            action.menuItem = menuItem;
        }
    }

    [_viewMenu addItem:[NSMenuItem separatorItem]];

    //----------------------
    
    // copy all of them to a vector, and then assign the action ptrs
    for (int32_t i = 0; i < numActions; ++i) {
        Action& action = actions[i];
        const char *icon = action.icon;  // single char
        
        // skip separators
        bool isSeparator = icon[0] == 0;
        if (isSeparator) continue;
        
        _actions.push_back(action);
    }
    
    // now alias Actions to the vector above
    //assert(_actions.size() == ArrayCount(actionPtrs));
    for (int32_t i = 0; i < _actions.size(); ++i) {
        *(actionPtrs[i]) = &_actions[i];
    }
    
    // don't want these buttons showing up, menu only
    _actionPrevItem->disableButton();
    _actionItem->disableButton();
    
    _actionHud->disableButton();
    _actionHelp->disableButton();
    _actionHideUI->disableButton();
    _actionVertical->disableButton();
    
    return stackView;
}

- (NSTextField *)_addHud:(BOOL)isShadow
{
    // TODO: This text field is clamping to the height, so have it set to 1200.
    // really want field to expand to fill the window height for large output
    uint32_t w = 800;
    uint32_t h = 1220;
    
    // add a label for the hud
    NSTextField *label = [[MyNSTextField alloc]
        initWithFrame:NSMakeRect(isShadow ? 21 : 20, isShadow ? 21 : 20, w,
                                 h)];
    
    label.preferredMaxLayoutWidth = w;

    label.drawsBackground = NO;
    label.textColor = !isShadow
                          ? [NSColor colorWithSRGBRed:0 green:1 blue:0 alpha:1]
                          : [NSColor colorWithSRGBRed:0 green:0 blue:0 alpha:1];
    label.bezeled = NO;
    label.editable = NO;
    label.selectable = NO;
    label.lineBreakMode = NSLineBreakByClipping;
    label.maximumNumberOfLines = 0;  // fill to height

    // important or interferes with table view
    label.refusesFirstResponder = YES;
    label.enabled = NO;
    
    label.cell.scrollable = NO;
    label.cell.wraps = NO;

    label.hidden = !_showSettings->isHudShown;
    // label.font = NSFont.systemFont(ofSize: NSFont.systemFontSize(for:
    // label.controlSize))

    // UILabel has shadowColor/shadowOffset but NSTextField doesn't

    [self addSubview:label];

    return label;
}

- (void)doZoomMath:(float)newZoom newPan:(float2 &)newPan
{
    // transform the cursor to texture coordinate, or clamped version if outside
    Renderer *renderer = (Renderer *)self.delegate;
    float4x4 projectionViewModelMatrix =
        [renderer computeImageTransform:_showSettings->panX
                                   panY:_showSettings->panY
                                   zoom:_showSettings->zoom];

    // convert from pixel to clip space
    float halfX = _showSettings->viewSizeX * 0.5f;
    float halfY = _showSettings->viewSizeY * 0.5f;
    
    // sometimes get viewSizeX that's scaled by retina, and other times not.
    // account for contentScaleFactor (viewSizeX is 2x bigger than cursorX on
    // retina display) now passing down drawableSize instead of view.bounds.size
    halfX /= (float)_showSettings->viewContentScaleFactor;
    halfY /= (float)_showSettings->viewContentScaleFactor;
    
    float4x4 viewportMatrix =
    {
        (float4){ halfX,      0, 0, 0 },
        (float4){ 0,     -halfY, 0, 0 },
        (float4){ 0,          0, 1, 0 },
        (float4){ halfX,  halfY, 0, 1 },
    };
    viewportMatrix = inverse(viewportMatrix);
    
    float4 cursor = float4m(_showSettings->cursorX, _showSettings->cursorY, 0.0f, 1.0f);
    
    cursor = viewportMatrix * cursor;
    
    //NSPoint clipPoint;
    //clipPoint.x = (point.x - halfX) / halfX;
    //clipPoint.y = -(point.y - halfY) / halfY;

    // convert point in window to point in model space
    float4x4 mInv = inverse(projectionViewModelMatrix);
    
    float4 pixel = mInv * float4m(cursor.x, cursor.y, 1.0f, 1.0f);
    pixel.xyz /= pixel.w; // in case perspective used

    // allow pan to extend to show all
    float ar = _showSettings->imageAspectRatio();
    float maxX = 0.5f * ar;
    float minY = -0.5f;
    if (_showSettings->isShowingAllLevelsAndMips) {
        maxX += ar * 1.0f * (_showSettings->totalChunks() - 1);
        minY -= 1.0f * (_showSettings->mipCount - 1);
    }

    // X bound may need adjusted for ar ?
    // that's in model space (+/0.5f, +/0.5f), so convert to texture space
    pixel.x = NAMESPACE_STL::clamp(pixel.x, -0.5f * ar, maxX);
    pixel.y = NAMESPACE_STL::clamp(pixel.y, minY, 0.5f);

    // now that's the point that we want to zoom towards
    // No checks on this zoom
    // old - newPosition from the zoom

#if USE_PERSPECTIVE
    // TODO: this doesn't work for perspective
    newPan.x = _showSettings->panX - (_showSettings->zoom - newZoom) *
                                         _showSettings->imageBoundsX * pixel.x;
    newPan.y = _showSettings->panY + (_showSettings->zoom - newZoom) *
                                         _showSettings->imageBoundsY * pixel.y;
#else
    newPan.x = _showSettings->panX - (_showSettings->zoom - newZoom) *
                                         _showSettings->imageBoundsX * pixel.x;
    newPan.y = _showSettings->panY + (_showSettings->zoom - newZoom) *
                                         _showSettings->imageBoundsY * pixel.y;
#endif
}

- (void)handleGesture:(NSGestureRecognizer *)gestureRecognizer
{
    // skip until image loaded
    if (_showSettings->imageBoundsX == 0) {
        return;
    }
    
    // https://cocoaosxrevisited.wordpress.com/2018/01/06/chapter-18-mouse-events/
    if (gestureRecognizer != _zoomGesture) {
        return;
    }

    bool isFirstGesture = _zoomGesture.state == NSGestureRecognizerStateBegan;

    float zoom = _zoomGesture.magnification;
    if (isFirstGesture) {
        _zoomGesture.magnification = 1.0f;
        
        _validMagnification = 1.0f;
        _originalZoom = _showSettings->zoom;
        
        zoom = _originalZoom;
    }
    else if (zoom * _originalZoom < 0.1f) {
        // can go negative otherwise
        zoom = 0.1f / _originalZoom;
        _zoomGesture.magnification = zoom;
    }
    
    if (!isFirstGesture) {
        // try expontental (this causes a jump, comparison avoids an initial jump
        // zoom = powf(zoom, 1.05f);

        // doing multiply instead of equals here, also does exponential zom
        zoom *= _originalZoom;
    }
    
    [self updateZoom:zoom];
}

-(void)updateZoom:(float)zoom
{
    // https://developer.apple.com/documentation/uikit/touches_presses_and_gestures/handling_uikit_gestures/handling_pinch_gestures?language=objc
    // need to sync up the zoom when action begins or zoom will jump
    

    // https://stackoverflow.com/questions/30002361/image-zoom-centered-on-mouse-position

    // DONE: rect is now ar:1 for rect case, so these x values need to be half ar
    // and that's only if it's not rotated.  box/cube/ellipse make also not correspond
    float ar = _showSettings->imageAspectRatio();
    
    // find the cursor location with respect to the image
    float4 bottomLeftCorner = float4m(-0.5 * ar, -0.5f, 0.0f, 1.0f);
    float4 topRightCorner = float4m(0.5 * ar, 0.5f, 0.0f, 1.0f);

    Renderer *renderer = (Renderer *)self.delegate;
    float4x4 newMatrix = [renderer computeImageTransform:_showSettings->panX
                                                    panY:_showSettings->panY
                                                    zoom:zoom];

    // don't allow panning the entire image off the view boundary
    // transform the upper left and bottom right corner of the image
    float4 pt0 = newMatrix * bottomLeftCorner;
    float4 pt1 = newMatrix * topRightCorner;

    // for perspective
    pt0 /= pt0.w;
    pt1 /= pt1.w;

    // see that rectangle intersects the view, view is -1 to 1
    // this handles inversion
    float2 ptOrigin = simd::min(pt0.xy, pt1.xy);
    float2 ptSize = abs(pt0.xy - pt1.xy);

    CGRect imageRect = CGRectMake(ptOrigin.x, ptOrigin.y, ptSize.x, ptSize.y);
    CGRect viewRect = CGRectMake(-1.0f, -1.0f, 2.0f, 2.0f);

    int32_t numTexturesX = _showSettings->totalChunks();
    int32_t numTexturesY = _showSettings->mipCount;

    if (_showSettings->isShowingAllLevelsAndMips) {
        imageRect.origin.y -= (numTexturesY - 1) * imageRect.size.height;

        imageRect.size.width *= numTexturesX;
        imageRect.size.height *= numTexturesY;
    }

    float visibleWidth = imageRect.size.width * _showSettings->viewSizeX /
                         _showSettings->viewContentScaleFactor;
    float visibleHeight = imageRect.size.height * _showSettings->viewSizeY /
                          _showSettings->viewContentScaleFactor;

    // don't allow image to get too big
    // take into account zoomFit, or need to limit zoomFit and have smaller images
    // be smaller on screen
    float maxZoom = std::max(128.0f, _showSettings->zoomFit);
    //float minZoom = std::min(1.0f/8.0f, _showSettings->zoomFit);

    // TODO: 3d models have imageBoundsY of 1, so the limits are hit immediately
    
    int32_t gap = _showSettings->showAllPixelGap;
    
    // Note this includes chunks and mips even if those are not shown
    // so image could be not visible.
    float2 maxZoomXY;
    maxZoomXY.x = maxZoom * (_showSettings->imageBoundsX + gap) * numTexturesX;
    maxZoomXY.y = maxZoom * (_showSettings->imageBoundsY + gap) * numTexturesY;
    
    float minPixelSize = 4;
    float2 minZoomXY;
    minZoomXY.x = minPixelSize; // minZoom * (_showSettings->imageBoundsX + gap) * numTexturesX;
    minZoomXY.y = minPixelSize; // minZoom * (_showSettings->imageBoundsY + gap) * numTexturesY;
   
    // don't allow image to get too big
    bool isZoomChanged = true;
    
    if (visibleWidth > maxZoomXY.x || visibleHeight > maxZoomXY.y) {
        isZoomChanged = false;
    }

    // or too small
    if (visibleWidth < minZoomXY.x || visibleHeight < minZoomXY.y) {
        isZoomChanged = false;
    }

    // or completely off-screen
    if (!NSIntersectsRect(imageRect, viewRect)) {
        isZoomChanged = false;
    }
    
    if (!isZoomChanged) {
        _zoomGesture.magnification = _validMagnification;
        return;
    }

    if (_showSettings->zoom != zoom) {
        // DONE: zoom also changes the pan to zoom about the cursor, otherwise zoom
        // feels wrong. now adjust the pan so that cursor text stays locked under
        // (zoom to cursor)
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
                    _showSettings->panX, _showSettings->panY, _showSettings->zoom);
            [self setHudText:text.c_str()];
        }

        [self updateEyedropper];
        self.needsDisplay = YES;
    }
}


// left mouse button down
- (void)mouseDown:(NSEvent *)event
{
    // skip until image loaded
    if (_showSettings->imageBoundsX == 0) {
        return;
    }
    
    _mouseData.originPoint =
    _mouseData.oldPoint =
    _mouseData.newPoint = [self convertPoint:[event locationInWindow] fromView:nil];

    // capture pan value and cursor value
    _mouseData.pan = NSMakePoint(_showSettings->panX, _showSettings->panY);
}

// drag is mouse movement with left button down
- (void)mouseDragged:(NSEvent *)event
{
    // skip until image loaded
    if (_showSettings->imageBoundsX == 0) {
        return;
    }
    
    _mouseData.oldPoint = _mouseData.newPoint;
    _mouseData.newPoint = [self convertPoint:[event locationInWindow] fromView:nil];

    // TODO: need to account for zoom
    NSPoint delta;
    delta.x = _mouseData.newPoint.x - _mouseData.originPoint.x;
    delta.y = _mouseData.newPoint.y - _mouseData.originPoint.y;
    delta.x = -delta.x;
    delta.y = -delta.y;
    
    // scale to actual px or mouse cursor doesn't track drag
    delta.x *= _showSettings->viewContentScaleFactor;
    delta.y *= _showSettings->viewContentScaleFactor;
    
    // This is correct, but scale to image so cursor tracks the pick location
    // might be over a different mip/chunk though.
    float panX = _mouseData.pan.x + delta.x;
    float panY = _mouseData.pan.y + delta.y;
    
    [self updatePan:panX panY:panY];
}

- (void)mouseUp:(NSEvent *)event
{
    // ignore up even though cursor may have moved

}

- (void)mouseMoved:(NSEvent *)event
{
    // skip until image loaded
    if (_showSettings->imageBoundsX == 0) {
        return;
    }
    
    // pixel in non-square window coords, run thorugh inverse to get texel space
    // I think magnofication of zoom gesture is affecting coordinates reported by
    // this

    NSPoint point = [self convertPoint:[event locationInWindow] fromView:nil];

    // this needs to change if view is resized, but will likely receive mouseMoved
    // events
    _showSettings->cursorX = (int32_t)point.x;
    _showSettings->cursorY = (int32_t)point.y;

    // should really do this in draw call, since moved messeage come in so quickly
    [self updateEyedropper];
}

inline float4 toPremul(const float4 &c)
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
float  toSnorm8(float c)  { return (255.0f / 127.0f) * c - (128.0f / 127.0f); }
float2 toSnorm8(float2 c) { return (255.0f / 127.0f) * c - (128.0f / 127.0f); }
float3 toSnorm8(float3 c) { return (255.0f / 127.0f) * c - (128.0f / 127.0f); }
float4 toSnorm8(float4 c) { return (255.0f / 127.0f) * c - (128.0f / 127.0f); }

float4 toSnorm(float4 c)  { return 2.0f * c - 1.0f; }

- (void)updateEyedropper
{
    if ((!_showSettings->isHudShown)) {
        return;
    }

    if (_showSettings->imageBoundsX == 0) {
        // TODO: this return will leave old hud text up
        return;
    }

    // don't wait on renderer to update this matrix
    Renderer *renderer = (Renderer *)self.delegate;

    if (_showSettings->isEyedropperFromDrawable()) {
        // this only needs the cursor location, but can't supply uv to
        // displayPixelData

        if (_showSettings->lastCursorX != _showSettings->cursorX ||
            _showSettings->lastCursorY != _showSettings->cursorY) {
            // TODO: this means pan/zoom doesn't update data, may want to track some
            // absolute location in virtal canvas.

            _showSettings->lastCursorX = _showSettings->cursorX;
            _showSettings->lastCursorY = _showSettings->cursorY;

            // This just samples from drawable, so no re-render is needed
            [self showEyedropperData:float2m(0, 0)];

            // TODO: remove this, but only way to get drawSamples to execute right
            // now, but then entire texture re-renders and that's not power efficient.
            // Really just want to sample from the already rendered texture since
            // content isn't animated.

            self.needsDisplay = YES;
        }

        return;
    }

    float4x4 projectionViewModelMatrix =
        [renderer computeImageTransform:_showSettings->panX
                                   panY:_showSettings->panY
                                   zoom:_showSettings->zoom];

    // convert to clip space, or else need to apply additional viewport transform
    float halfX = _showSettings->viewSizeX * 0.5f;
    float halfY = _showSettings->viewSizeY * 0.5f;

    // sometimes get viewSizeX that's scaled by retina, and other times not.
    // account for contentScaleFactor (viewSizeX is 2x bigger than cursorX on
    // retina display) now passing down drawableSize instead of view.bounds.size
    halfX /= (float)_showSettings->viewContentScaleFactor;
    halfY /= (float)_showSettings->viewContentScaleFactor;

    float4 cursor = float4m(_showSettings->cursorX, _showSettings->cursorY, 0.0f, 1.0f);
    
    float4x4 pixelToClipTfm =
    {
        (float4){ halfX,      0, 0, 0 },
        (float4){ 0,     -halfY, 0, 0 },
        (float4){ 0,          0, 1, 0 },
        (float4){ halfX,  halfY, 0, 1 },
    };
    pixelToClipTfm = inverse(pixelToClipTfm);
    
    cursor = pixelToClipTfm * cursor;
    
    //float4 clipPoint;
    //clipPoint.x = (point.x - halfX) / halfX;
    //clipPoint.y = -(point.y - halfY) / halfY;

    // convert point in window to point in texture
    float4x4 mInv = inverse(projectionViewModelMatrix);
    
    float4 pixel = mInv * float4m(cursor.x, cursor.y, 1.0f, 1.0f);
    pixel.xyz /= pixel.w; // in case perspective used

    float ar = _showSettings->imageAspectRatio();
    
    // that's in model space (+/0.5f * ar, +/0.5f), so convert to texture space
    pixel.x = 0.999f * (pixel.x / ar + 0.5f);
    pixel.y = 0.999f * (-pixel.y + 0.5f);

    float2 uv = pixel.xy;

    // pixels are 0 based
    pixel.x *= _showSettings->imageBoundsX;
    pixel.y *= _showSettings->imageBoundsY;

    // TODO: finish this logic, need to account for gaps too, and then isolate to
    // a given level and mip to sample
    //    if (_showSettings->isShowingAllLevelsAndMips) {
    //        pixel.x *= _showSettings->totalChunks();
    //        pixel.y *= _showSettings->mipCount;
    //    }

    // TODO: clearing out the last px visited makes it hard to gather data
    // put value on clipboard, or allow click to lock the displayed pixel and
    // value. Might just change header to px(last): ...
    string text;

    // only display pixel if over image
    if (pixel.x < 0.0f || pixel.x >= (float)_showSettings->imageBoundsX) {
        sprintf(text, "canvas: %d %d\n", (int32_t)pixel.x, (int32_t)pixel.y);
        [self setEyedropperText:text.c_str()];  // ick
        return;
    }
    if (pixel.y < 0.0f || pixel.y >= (float)_showSettings->imageBoundsY) {
        // was blanking out, but was going blank on color_grid-a when over zoomed in
        // image maybe not enough precision with float.
        sprintf(text, "canvas: %d %d\n", (int32_t)pixel.x, (int32_t)pixel.y);
        [self setEyedropperText:text.c_str()];
        return;
    }

    // Note: fromView: nil returns isFlipped coordinate, fromView:self flips it
    // back.

    int32_t newX = (int32_t)pixel.x;
    int32_t newY = (int32_t)pixel.y;

    if (_showSettings->textureLookupX != newX ||
        _showSettings->textureLookupY != newY) {
        // Note: this only samples from the original texture via compute shaders
        // so preview mode pixel colors are not conveyed.  But can see underlying
        // data driving preview.

        // %.0f rounds the value, but want truncation
        _showSettings->textureLookupX = newX;
        _showSettings->textureLookupY = newY;

        [self showEyedropperData:uv];

        // TODO: remove this, but only way to get drawSamples to execute right now,
        // but then entire texture re-renders and that's not power efficient.
        self.needsDisplay = YES;
    }
}

- (void)showEyedropperData:(float2)uv
{
    string text;
    string tmp;

    float4 c = _showSettings->textureResult;

    // DONE: use these to format the text
    MyMTLPixelFormat format = _showSettings->originalFormat;
    bool isSrgb = isSrgbFormat(format);
    bool isSigned = isSignedFormat(format);

    bool isHdr = isHdrFormat(format);
    bool isFloat = isHdr;

    int32_t numChannels = _showSettings->numChannels;

    bool isNormal = _showSettings->isNormal;
    bool isColor = !isNormal;

    bool isDirection = false;
    bool isValue = false;

    if (_showSettings->isEyedropperFromDrawable()) {
        // TODO: could write barycentric, then lookup uv from that
        // then could show the block info.

        // interpret based on shapeChannel, debugMode, etc
        switch (_showSettings->shapeChannel) {
            case ShapeChannelDepth:
                isSigned = false;  // using fract on uv

                isValue = true;
                isFloat = true;
                numChannels = 1;
                break;
            case ShapeChannelUV0:
                isSigned = false;  // using fract on uv

                isValue = true;
                isFloat = true;
                numChannels = 2;  // TODO: fix for 3d uvw
                break;

            case ShapeChannelFaceNormal:
            case ShapeChannelNormal:
            case ShapeChannelTangent:
            case ShapeChannelBitangent:
                isDirection = true;
                numChannels = 3;

                // convert unorm to snnorm
                c = toSnorm(c);
                break;

            case ShapeChannelMipLevel:
                isValue = true;
                isSigned = false;
                isFloat = true;

                // viz is mipNumber as alpha
                numChannels = 1;
                c.r = 4.0 - (c.a * 4.0);
                break;

            default:
                break;
        }

        // debug mode

        // preview vs. not
    }
    else {
        // this will be out of sync with gpu eval, so may want to only display px
        // from returned lookup this will always be a linear color

        int32_t x = _showSettings->textureResultX;
        int32_t y = _showSettings->textureResultY;

        // show uv, so can relate to gpu coordinates stored in geometry and find
        // atlas areas
        append_sprintf(text, "uv:%0.3f %0.3f\n",
                       (float)x / _showSettings->imageBoundsX,
                       (float)y / _showSettings->imageBoundsY);

        // pixel at top-level mip
        append_sprintf(text, "px:%d %d\n", x, y);

        // show block num
        int mipLOD = _showSettings->mipNumber;

        int mipX = _showSettings->imageBoundsX;
        int mipY = _showSettings->imageBoundsY;

        mipX = mipX >> mipLOD;
        mipY = mipY >> mipLOD;

        mipX = std::max(1, mipX);
        mipY = std::max(1, mipY);

        mipX = (int32_t)(uv.x * mipX);
        mipY = (int32_t)(uv.y * mipY);

        _showSettings->textureLookupMipX = mipX;
        _showSettings->textureLookupMipY = mipY;

        // TODO: may want to return mip in pixel readback
        // don't have it right now, so don't display if preview is enabled
        if (_showSettings->isPreview)
            mipLOD = 0;

        auto blockDims = blockDimsOfFormat(format);
        if (blockDims.x > 1)
            append_sprintf(text, "bpx: %d %d\n", mipX / blockDims.x,
                           mipY / blockDims.y);

        // TODO: on astc if we have original blocks can run analysis from
        // astc-encoder about each block.

        // show the mip pixel (only if not preview and mip changed)
        if (mipLOD > 0 && !_showSettings->isPreview)
            append_sprintf(text, "mpx: %d %d\n", mipX, mipY);

        // TODO: more criteria here, can have 2 channel PBR metal-roughness
        // also have 4 channel normals where zw store other data.

        bool isDecodeSigned = isSignedFormat(_showSettings->decodedFormat);
        if (isSigned && !isDecodeSigned) {
            c = toSnorm8(c);
        }
    }

    if (isValue) {
        printChannels(tmp, "val: ", c, numChannels, isFloat, isSigned);
        text += tmp;
    }
    else if (isDirection) {
        // print direction
        isFloat = true;
        isSigned = true;

        printChannels(tmp, "dir: ", c, numChannels, isFloat, isSigned);
        text += tmp;
    }
    else if (isNormal) {
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
        printChannels(tmp, "lin: ", c, numChannels, isFloat, isSigned);
        text += tmp;

        // print direction
        float4 d = float4m(nx, ny, nz, 0.0f);
        isFloat = true;
        isSigned = true;
        printChannels(tmp, "dir: ", d, 3, isFloat, isSigned);
        text += tmp;
    }
    else if (isColor) {
        // DONE: write some print helpers based on float4 and length
        printChannels(tmp, "lin: ", c, numChannels, isFloat, isSigned);
        text += tmp;

        if (isSrgb) {
            // this saturates the value, so don't use for extended srgb
            float4 s = linearToSRGB(c);

            printChannels(tmp, "srg: ", s, numChannels, isFloat, isSigned);
            text += tmp;
        }

        // display the premul values too, but not fully transparent pixels
        if (c.a > 0.0 && c.a < 1.0f) {
            printChannels(tmp, "lnp: ", toPremul(c), numChannels, isFloat, isSigned);
            text += tmp;

            // TODO: do we need the premul srgb color too?
            if (isSrgb) {
                // this saturates the value, so don't use for extended srgb
                float4 s = linearToSRGB(c);

                printChannels(tmp, "srp: ", toPremul(s), numChannels, isFloat,
                              isSigned);
                text += tmp;
            }
        }
    }

    [self setEyedropperText:text.c_str()];

    // TODO: range display of pixels is useful, only showing pixels that fall
    // within a given range, but would need slider then, and determine range of
    // pixels.
    // TODO: Auto-range is also useful for depth (ignore far plane of 0 or 1).

    // TOOD: display histogram from compute, bin into buffer counts of pixels

    // DONE: stop clobbering hud text, need another set of labels
    // and a zoom preview of the pixels under the cursor.
    // Otherwise, can't really see the underlying color.

    // TODO: Stuff these on clipboard with a click, or use cmd+C?
}

- (void)setEyedropperText:(const char *)text
{
    _textSlots[0] = text;

    [self updateHudText];
}

- (void)setHudText:(const char *)text
{
    _textSlots[1] = text;

    [self updateHudText];
}

- (void)updateHudText
{
    // combine textSlots
    string text = _textSlots[0] + _textSlots[1];

    NSString *textNS = [NSString stringWithUTF8String:text.c_str()];
    _hudLabel2.stringValue = textNS;
    _hudLabel2.needsDisplay = YES;
    
    _hudLabel.stringValue = textNS;
    _hudLabel.needsDisplay = YES;
}

- (void)scrollWheel:(NSEvent *)event
{
    // skip until image loaded
    if (_showSettings->imageBoundsX == 0) {
        return;
    }
    
    double wheelX = [event scrollingDeltaX];
    double wheelY = [event scrollingDeltaY];

    // Ugh, how we we tell mouseWheel from trackpad gesture calling this?
    // if([event phase]) - supposedly only set on trackpad, but Apple MagicMouse does this on wheel
    //   and trackpad fires on that too causing the image to zoom away to nothing (inertia maybe)
    // https://stackoverflow.com/questions/6642058/mac-cocoa-how-can-i-detect-trackpad-scroll-gestures
    bool isMouse = ![event hasPreciseScrollingDeltas];
    
    if (isMouse) {
        // zoom with mouse
        float zoom = _zoomGesture.magnification;
        if (wheelY != 0.0) {
            wheelY *= 0.01;
            wheelY = clamp(wheelY, -0.1, 0.1);
            
            zoom *= 1.0 + wheelY;
            
            // here have to modify the magnfication, since gesture isn't driving it
            _zoomGesture.magnification = zoom;
            
            [self updateZoom: zoom];
        }
    }
    else {
        // pan with trackpad
        wheelY = -wheelY;
        wheelX = -wheelX;

        float panX = _showSettings->panX + wheelX;
        float panY = _showSettings->panY + wheelY;
        
        [self updatePan:panX panY:(float)panY];
    }
}

- (void)updatePan:(float)panX panY:(float)panY
{
    Renderer *renderer = (Renderer *)self.delegate;
    float4x4 projectionViewModelMatrix =
        [renderer computeImageTransform:panX
                                   panY:panY
                                   zoom:_showSettings->zoom];

    // don't allow panning the entire image off the view boundary
    // transform the upper left and bottom right corner or the image

    // what if zoom moves it outside?
    float ar = _showSettings->imageAspectRatio();
    
    float4 pt0 = projectionViewModelMatrix * float4m(-0.5 * ar, -0.5f, 0.0f, 1.0f);
    float4 pt1 = projectionViewModelMatrix * float4m(0.5 * ar, 0.5f, 0.0f, 1.0f);

    // for perspective
    pt0.xyz /= pt0.w;
    pt1.xyz /= pt1.w;

    float2 ptOrigin = simd::min(pt0.xy, pt1.xy);
    float2 ptSize = abs(pt0.xy - pt1.xy);

    // see that rectangle intersects the view, view is -1 to 1
    CGRect imageRect = CGRectMake(ptOrigin.x, ptOrigin.y, ptSize.x, ptSize.y);
    CGRect viewRect = CGRectMake(-1.0f, -1.0f, 2.0f, 2.0f);

    int32_t numTexturesX = _showSettings->totalChunks();
    int32_t numTexturesY = _showSettings->mipCount;

    if (_showSettings->isShowingAllLevelsAndMips) {
        imageRect.origin.y -= (numTexturesY - 1) * imageRect.size.height;

        imageRect.size.width *= numTexturesX;
        imageRect.size.height *= numTexturesY;
    }

    if (!NSIntersectsRect(imageRect, viewRect)) {
        return;
    }

    if (_showSettings->panX != panX || _showSettings->panY != panY) {
        _showSettings->panX = panX;
        _showSettings->panY = panY;

        if (doPrintPanZoom) {
            string text;
            sprintf(text,
                    "Pan %.3f,%.3f\n"
                    "Zoom %.2fx\n",
                    _showSettings->panX, _showSettings->panY, _showSettings->zoom);
            [self setHudText:text.c_str()];
        }

        [self updateEyedropper];
        self.needsDisplay = YES;
    }
}

// use this to enable/disable menus, buttons, etc.  Called on every event
// when not implemented, then user items are always enabled
- (BOOL)validateUserInterfaceItem:(id<NSValidatedUserInterfaceItem>)item
{
    // TODO: tie to menus and buttons states for enable/disable toggles
    // https://developer.apple.com/library/archive/documentation/Cocoa/Conceptual/MenuList/Articles/EnablingMenuItems.html

    // MTKView is not doc based, so can't all super
    // return [super validateUserInterfaceItem:anItem];

    return YES;
}

- (void)updateUIAfterLoad
{
    // TODO: move these to actions, and test their state instead of looking up
    // buttons here and in HandleKey.

    // base on showSettings, hide some fo the buttons
    bool isShowAllHidden =
        _showSettings->totalChunks() <= 1 && _showSettings->mipCount <= 1;

    bool isArrayHidden = _showSettings->arrayCount <= 1;
    bool isFaceSliceHidden =
        _showSettings->faceCount <= 1 && _showSettings->sliceCount <= 1;
    bool isMipHidden = _showSettings->mipCount <= 1;

    bool isJumpToNextHidden =
        !(_showSettings->isArchive || _showSettings->isFolder);

    bool isRedHidden = _showSettings->numChannels == 0; // models don't show rgba
    bool isGreenHidden = _showSettings->numChannels <= 1;
    bool isBlueHidden = _showSettings->numChannels <= 2 &&
                        !_showSettings->isNormal;  // reconstruct z = b on normals

    // TODO: also need a hasAlpha for pixels, since many compressed formats like
    // ASTC always have 4 channels but internally store R,RG01,... etc.  Can get
    // more data from swizzle in the props. Often alpha doesn't store anything
    // useful to view.

    // DONE: may want to disable isPremul on block textures that already have
    // premul in data or else premul is applied a second time to the visual

    bool hasAlpha = _showSettings->numChannels >= 3;

    bool isAlphaHidden = !hasAlpha;
    bool isPremulHidden = !hasAlpha;
    bool isCheckerboardHidden = !hasAlpha;

    bool isSignedHidden = !isSignedFormat(_showSettings->originalFormat);
    bool isPlayHidden = !_showSettings->isModel;
    
    _actionPlay->setHidden(isPlayHidden);
    _actionArray->setHidden(isArrayHidden);
    _actionFace->setHidden(isFaceSliceHidden);
    _actionMip->setHidden(isMipHidden);
    _actionShowAll->setHidden(isShowAllHidden);
    
    _actionItem->setHidden(isJumpToNextHidden);
    _actionPrevItem->setHidden(isJumpToNextHidden);
    
    _actionR->setHidden(isRedHidden);
    _actionG->setHidden(isGreenHidden);
    _actionB->setHidden(isBlueHidden);
    _actionA->setHidden(isAlphaHidden);
    
    _actionPremul->setHidden(isPremulHidden);
    _actionSigned->setHidden(isSignedHidden);
    _actionChecker->setHidden(isCheckerboardHidden);
    
    // also need to call after each toggle
    [self updateUIControlState];
}

- (void)updateUIControlState
{
    // there is also mixed state, but not using that
    auto On = true;
    auto Off = false;
    
#define toState(x) (x) ? On : Off

    Renderer* renderer = (Renderer*)self.delegate;
    auto showAllState = toState(_showSettings->isShowingAllLevelsAndMips);
    auto premulState = toState(_showSettings->isPremul);
    auto signedState = toState(_showSettings->isSigned);
    auto checkerboardState = toState(_showSettings->isCheckerboardShown);
    auto previewState = toState(_showSettings->isPreview);
    auto gridState = toState(_showSettings->isAnyGridShown());
    auto wrapState = toState(_showSettings->isWrap);
    auto debugState = toState(_showSettings->debugMode != DebugModeNone);
    auto playState = toState(_showSettings->isModel && renderer.playAnimations);
    auto hudState = toState(_showSettings->isHudShown);
    
    TextureChannels &channels = _showSettings->channels;

    auto redState = toState(channels == TextureChannels::ModeR001);
    auto greenState = toState(channels == TextureChannels::Mode0G01);
    auto blueState = toState(channels == TextureChannels::Mode00B1);
    auto alphaState = toState(channels == TextureChannels::ModeAAA1);

    auto arrayState = toState(_showSettings->arrayNumber > 0);
    auto faceState = toState(_showSettings->faceNumber > 0);
    auto mipState = toState(_showSettings->mipNumber > 0);

    auto meshState = toState(_showSettings->meshNumber > 0);
    auto meshChannelState = toState(_showSettings->shapeChannel > 0);
    auto lightingState =
        toState(_showSettings->lightingMode != LightingModeNone);
    auto tangentState = toState(_showSettings->useTangent);

    auto verticalState = toState(_buttonStack.orientation == NSUserInterfaceLayoutOrientationVertical);
    auto uiState = toState(_buttonStack.hidden);

    _actionVertical->setHighlight(verticalState);
    
    // TODO: pass boolean, and change in the call
    _actionPlay->setHighlight(playState);
    _actionHelp->setHighlight(Off);
    _actionInfo->setHighlight(Off);
    _actionHud->setHighlight(hudState);
    
    _actionArray->setHighlight(arrayState);
    _actionFace->setHighlight(faceState);
    _actionMip->setHighlight(mipState);
    
    // these never show check state
    _actionItem->setHighlight(Off);
    _actionPrevItem->setHighlight(Off);
    
    _actionHideUI->setHighlight(uiState); // note below button always off, menu has state
    
    _actionR->setHighlight(redState);
    _actionG->setHighlight(greenState);
    _actionB->setHighlight(blueState);
    _actionA->setHighlight(alphaState);
    
    _actionShowAll->setHighlight(showAllState);
    _actionPreview->setHighlight(previewState);
    _actionShapeMesh->setHighlight(meshState);
    _actionShapeChannel->setHighlight(meshChannelState);
    _actionLighting->setHighlight(lightingState);
    _actionWrap->setHighlight(wrapState);
    _actionGrid->setHighlight(gridState);
    _actionDebug->setHighlight(debugState);
    _actionTangent->setHighlight(tangentState);
    
    _actionPremul->setHighlight(premulState);
    _actionSigned->setHighlight(signedState);
    _actionChecker->setHighlight(checkerboardState);
}

// TODO: convert to C++ actions, and then call into Base holding all this
// move pan/zoom logic too.  Then use that as start of Win32 kramv.

- (IBAction)handleAction:(id)sender
{
    NSEvent *theEvent = [NSApp currentEvent];
    bool isShiftKeyDown = (theEvent.modifierFlags & NSEventModifierFlagShift);

    const Action* action = nullptr;
    if ([sender isKindOfClass:[NSButton class]]) {
        NSButton *button = (NSButton *)sender;
        for (const auto& search: _actions) {
            if (search.button == button) {
                action = &search;
                break;
            }
        }
    }
    else if ([sender isKindOfClass:[NSMenuItem class]]) {
        NSMenuItem *menuItem = (NSMenuItem *)sender;
        for (const auto& search: _actions) {
            if (search.menuItem == menuItem) {
                action = &search;
                break;
            }
        }
    }
    
    if (!action) {
        KLOGE("kram", "unknown UI element");
        return;
    }
    
    [self handleEventAction:action isShiftKeyDown:isShiftKeyDown];
}

- (void)keyDown:(NSEvent *)theEvent
{
    bool isShiftKeyDown = theEvent.modifierFlags & NSEventModifierFlagShift;
    uint32_t keyCode = theEvent.keyCode;

    // for now hit esc to hide the table views
    if (keyCode == Key::Escape) {
        [self hideTables];
        
        _hudHidden = false;
        [self updateHudVisibility];
        return;
    }
    
    const Action* action = nullptr;
    for (const auto& search: _actions) {
        if (search.keyCode == keyCode) {
            action = &search;
            break;
        }
    }
    
    if (!action) {
        [super keyDown:theEvent];
        //KLOGE("kram", "unknown UI element");
        return;
    }
    
    bool isHandled = [self handleEventAction:action isShiftKeyDown:isShiftKeyDown];
    if (!isHandled) {
        // this will bonk
        [super keyDown:theEvent];
    }
}

- (void)hideTables
{
    _tableView.hidden = true;
}

- (void)updateHudVisibility {
    _hudLabel.hidden = _hudHidden || !_showSettings->isHudShown;
    _hudLabel2.hidden = _hudHidden || !_showSettings->isHudShown;
}

- (bool)handleEventAction:(const Action*)action isShiftKeyDown:(bool)isShiftKeyDown
{
    // Some data depends on the texture data (isSigned, isNormal, ..)
    bool isChanged = false;
    bool isStateChanged = false;

    // TODO: fix isChanged to only be set when value changes
    // f.e. clamped values don't need to re-render
    string text;

    if (action == _actionVertical) {
        bool isVertical =
            _buttonStack.orientation == NSUserInterfaceLayoutOrientationVertical;
        isVertical = !isVertical;

        _buttonStack.orientation = isVertical
                                       ? NSUserInterfaceLayoutOrientationVertical
                                       : NSUserInterfaceLayoutOrientationHorizontal;
        text = isVertical ? "Vert UI" : "Horiz UI";

        // just to update toggle state to Off
        isStateChanged = true;
    }
    else if (action == _actionHideUI) {
        // this means no image loaded yet
        if (_noImageLoaded) {
            return true;
        }

        _buttonStack.hidden = !_buttonStack.hidden;
        text = _buttonStack.hidden ? "Hide UI" : "Show UI";

        // just to update toggle state to Off
        isStateChanged = true;
    }

    else if (action == _actionR) {
        if (!action->isHidden) {
            TextureChannels& channels = _showSettings->channels;

            if (channels == TextureChannels::ModeR001) {
                channels = TextureChannels::ModeRGBA;
                text = "Mask RGBA";
            }
            else {
                channels = TextureChannels::ModeR001;
                text = "Mask R001";
            }
            isChanged = true;
        }

    }
    else if (action == _actionG) {
        if (!action->isHidden) {
            TextureChannels& channels = _showSettings->channels;

            if (channels == TextureChannels::Mode0G01) {
                channels = TextureChannels::ModeRGBA;
                text = "Mask RGBA";
            }
            else {
                channels = TextureChannels::Mode0G01;
                text = "Mask 0G01";
            }
            isChanged = true;
        }
    }
    else if (action == _actionB) {
        if (!action->isHidden) {
            TextureChannels& channels = _showSettings->channels;

            if (channels == TextureChannels::Mode00B1) {
                channels = TextureChannels::ModeRGBA;
                text = "Mask RGBA";
            }
            else {
                channels = TextureChannels::Mode00B1;
                text = "Mask 00B1";
            }

            isChanged = true;
        }
    }
    else if (action == _actionA) {
        if (!action->isHidden) {
            TextureChannels& channels = _showSettings->channels;

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
        
    }
    else if (action == _actionPlay) {
        if (!action->isHidden) {
            Renderer* renderer = (Renderer*)self.delegate;
            
            renderer.playAnimations = !renderer.playAnimations;
            
            text = renderer.playAnimations ? "Play" : "Pause";
            isChanged = true;
            
            self.enableSetNeedsDisplay = !renderer.playAnimations;
            self.paused = !renderer.playAnimations;
        }
        else {
            self.enableSetNeedsDisplay = YES;
            self.paused = YES;
        }
       
    }

    else if (action == _actionShapeChannel) {
        _showSettings->advanceShapeChannel(isShiftKeyDown);
        
        text = _showSettings->shapeChannelText();
        isChanged = true;
    }
    else if (action == _actionLighting) {
        _showSettings->advanceLightingMode(isShiftKeyDown);
        text = _showSettings->lightingModeText();
        isChanged = true;
    }
    else if (action == _actionTangent) {
        _showSettings->useTangent = !_showSettings->useTangent;
        if (_showSettings->useTangent)
            text = "Vertex Tangents";
        else
            text = "Fragment Tangents";
        isChanged = true;
    }
    else if (action == _actionDebug) {
        _showSettings->advanceDebugMode(isShiftKeyDown);
        text = _showSettings->debugModeText();
        isChanged = true;
    }
    else if (action == _actionHelp) {
        // display the chars for now
        text =
            "1234-rgba, Preview, Debug, A-show all\n"
            "Info, Hud, Reload, 0-fit\n"
            "Checker, Grid\n"
            "Wrap, 8-signed, 9-premul\n"
            "Mip, Face, Y-array, ↓-next item\n"
            "Lighting, S-shape, C-shape channel\n";
        
        // just to update toggle state to Off
        isStateChanged = true;
    }

    else if (action == _actionFit) {
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
        zoom *= 1.0f / (1 << _showSettings->mipNumber);

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
                    _showSettings->panX, _showSettings->panY, _showSettings->zoom);
            text += tmp;
        }

        isChanged = true;
    }
    // reload key (also a quick way to reset the settings)
    else if (action == _actionReload) {
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
                    _showSettings->panX, _showSettings->panY, _showSettings->zoom);
            text += tmp;
        }

        isChanged = true;
    }
    // P already used for premul
    else if (action == _actionPreview) {
        _showSettings->isPreview = !_showSettings->isPreview;
        isChanged = true;
        text = "Preview ";
        text += _showSettings->isPreview ? "On" : "Off";
    }
    // TODO: might switch c to channel cycle, so could just hit that
    // and depending on the content, it cycles through reasonable channel masks

    // toggle checkerboard for transparency
    else if (action == _actionChecker) {
        if (action->isHidden) {
            _showSettings->isCheckerboardShown = !_showSettings->isCheckerboardShown;
            isChanged = true;
            text = "Checker ";
            text += _showSettings->isCheckerboardShown ? "On" : "Off";
        }
    }

    // toggle pixel grid when magnified above 1 pixel, can happen from mipmap
    // changes too
    else if (action == _actionGrid) {
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
            0, 1, 2, 32, 64, 128, 256  // atlas sizes
        };

        if (grid == 0) {
            sprintf(text, "Grid Off");
        }
        else if (grid == 1) {
            _showSettings->isPixelGridShown = true;

            sprintf(text, "Pixel Grid 1x1");
        }
        else if (grid == 2) {
            _showSettings->isBlockGridShown = true;

            sprintf(text, "Block Grid %dx%d", _showSettings->blockX,
                    _showSettings->blockY);
        }
        else {
            _showSettings->isAtlasGridShown = true;

            // want to be able to show altases tht have long entries derived from
            // props but right now just a square grid atlas
            _showSettings->gridSizeX = _showSettings->gridSizeY = gridSizes[grid];

            sprintf(text, "Atlas Grid %dx%d", _showSettings->gridSizeX,
                    _showSettings->gridSizeY);
        }

        isChanged = true;
    }
    else if (action == _actionShowAll) {
        if (!action->isHidden) {
            // TODO: have drawAllMips, drawAllLevels, drawAllLevelsAndMips
            _showSettings->isShowingAllLevelsAndMips =
                !_showSettings->isShowingAllLevelsAndMips;
            isChanged = true;
            text = "Show All ";
            text += _showSettings->isShowingAllLevelsAndMips ? "On" : "Off";
        }
    }

    // toggle hud that shows name and pixel value under the cursor
    // this may require calling setNeedsDisplay on the UILabel as cursor moves
    else if (action == _actionHud) {
        _showSettings->isHudShown = !_showSettings->isHudShown;
        [self updateHudVisibility];
        // isChanged = true;
        text = "Hud ";
        text += _showSettings->isHudShown ? "On" : "Off";
        isStateChanged = true;
    }

    // info on the texture, could request info from lib, but would want to cache
    // that info
    else if (action == _actionInfo) {
        if (_showSettings->isHudShown) {
            sprintf(text, "%s",
                    isShiftKeyDown ? _showSettings->imageInfoVerbose.c_str()
                                   : _showSettings->imageInfo.c_str());
        }
        // just to update toggle state to Off
        isStateChanged = true;
    }

    // toggle wrap/clamp
    else if (action == _actionWrap) {
        // TODO: cycle through all possible modes (clamp, repeat, mirror-once,
        // mirror-repeat, ...)
        _showSettings->isWrap = !_showSettings->isWrap;
        isChanged = true;
        text = "Wrap ";
        text += _showSettings->isWrap ? "On" : "Off";
    }

    // toggle signed vs. unsigned
    else if (action == _actionSigned) {
        if (!action->isHidden) {
            _showSettings->isSigned = !_showSettings->isSigned;
            isChanged = true;
            text = "Signed ";
            text += _showSettings->isSigned ? "On" : "Off";
        }
    }

    // toggle premul alpha vs. unmul
    else if (action == _actionPremul) {
        if (!action->isHidden) {
            _showSettings->isPremul = !_showSettings->isPremul;
            isChanged = true;
            text = "Premul ";
            text += _showSettings->isPremul ? "On" : "Off";
        }
    }

    else if (action == _actionItem || action == _actionPrevItem) {
        if (!action->isHidden) {
            // invert shift key for prev, since it's reversese
            if (action == _actionPrevItem)
                isShiftKeyDown = !isShiftKeyDown;
            
            if (_showSettings->isArchive) {
                if ([self advanceFileFromAchive:!isShiftKeyDown]) {
                    _hudHidden = true;
                    [self updateHudVisibility];
                    
                    isChanged = true;
                    text = "Loaded " + _showSettings->lastFilename;
                }
            }
            else if (_showSettings->isFolder) {
                if ([self advanceFileFromFolder:!isShiftKeyDown]) {
                    _hudHidden = true;
                    [self updateHudVisibility];
                    
                    isChanged = true;
                    text = "Loaded " + _showSettings->lastFilename;
                }
            }
        }
    }

    // test out different shapes
    else if (action == _actionShapeMesh) {
        if (_showSettings->meshCount > 1) {
            _showSettings->advanceMeshNumber(isShiftKeyDown);
            text = _showSettings->meshNumberText();
            isChanged = true;
        }
    }

    // TODO: should probably have these wrap and not clamp to count limits

    // mip up/down
    else if (action == _actionMip) {
        if (_showSettings->mipCount > 1) {
            if (isShiftKeyDown) {
                _showSettings->mipNumber = MAX(_showSettings->mipNumber - 1, 0);
            }
            else {
                _showSettings->mipNumber =
                    MIN(_showSettings->mipNumber + 1, _showSettings->mipCount - 1);
            }
            sprintf(text, "Mip %d/%d", _showSettings->mipNumber,
                    _showSettings->mipCount);
            isChanged = true;
        }
    }

    else if (action == _actionFace) {
        // cube or cube array, but hit s to pick cubearray
        if (_showSettings->faceCount > 1) {
            if (isShiftKeyDown) {
                _showSettings->faceNumber = MAX(_showSettings->faceNumber - 1, 0);
            }
            else {
                _showSettings->faceNumber =
                    MIN(_showSettings->faceNumber + 1, _showSettings->faceCount - 1);
            }
            sprintf(text, "Face %d/%d", _showSettings->faceNumber,
                    _showSettings->faceCount);
            isChanged = true;
        }
    }

    else if (action == _actionArray) {
        // slice
        if (_showSettings->sliceCount > 1) {
            if (isShiftKeyDown) {
                _showSettings->sliceNumber = MAX(_showSettings->sliceNumber - 1, 0);
            }
            else {
                _showSettings->sliceNumber =
                    MIN(_showSettings->sliceNumber + 1, _showSettings->sliceCount - 1);
            }
            sprintf(text, "Slice %d/%d", _showSettings->sliceNumber,
                    _showSettings->sliceCount);
            isChanged = true;
        }
        // array
        else if (_showSettings->arrayCount > 1) {
            if (isShiftKeyDown) {
                _showSettings->arrayNumber = MAX(_showSettings->arrayNumber - 1, 0);
            }
            else {
                _showSettings->arrayNumber =
                    MIN(_showSettings->arrayNumber + 1, _showSettings->arrayCount - 1);
            }
            sprintf(text, "Array %d/%d", _showSettings->arrayNumber,
                    _showSettings->arrayCount);
            isChanged = true;
        }
    }
    else {
        // non-handled action
        return false;
    }

    if (!text.empty()) {
        [self setHudText:text.c_str()];
    }

    if (isChanged || isStateChanged) {
        [self updateUIControlState];
    }

    if (isChanged) {
        self.needsDisplay = YES;
    }
    return true;
}

// Note: docs state that drag&drop should be handled automatically by UTI setup
// via openURLs but I find these calls are needed, or it doesn't work.  Maybe
// need to register for NSRUL instead of NSPasteboardTypeFileURL.  For example,
// in canReadObjectForClasses had to use NSURL.

// drag and drop support
- (NSDragOperation)draggingEntered:(id)sender
{
    if ((NSDragOperationGeneric & [sender draggingSourceOperationMask]) ==
        NSDragOperationGeneric) {
        NSPasteboard *pasteboard = [sender draggingPasteboard];

        bool canReadPasteboardObjects =
            [pasteboard canReadObjectForClasses:@[ [NSURL class] ]
                                        options:nil];

        // don't copy dropped item, want to alias large files on disk without that
        if (canReadPasteboardObjects) {
            return NSDragOperationGeneric;
        }
    }

    // not a drag we can use
    return NSDragOperationNone;
}

- (BOOL)prepareForDragOperation:(id)sender
{
    return YES;
}

- (BOOL)performDragOperation:(id)sender
{
    NSPasteboard *pasteboard = [sender draggingPasteboard];

    NSString *desiredType = [pasteboard availableTypeFromArray:pasteboardTypes];

    if ([desiredType isEqualToString:NSPasteboardTypeFileURL]) {
        // TODO: use readObjects to drag multiple files onto one view
        // load one mip of all those, use smaller mips for thumbnail

        // the pasteboard contains a list of filenames
        NSString *urlString =
            [pasteboard propertyListForType:NSPasteboardTypeFileURL];

        // this turns it into a real path (supposedly works even with sandbox)
        NSURL *url = [NSURL URLWithString:urlString];

        // convert the original path and then back to a url, otherwise reload fails
        // when this file is replaced.
        const char *filename = url.fileSystemRepresentation;
        if (filename == nullptr) {
            KLOGE("kramv", "Fix this drop url returning nil issue");
            return NO;
        }

        NSString *filenameString = [NSString stringWithUTF8String:filename];

        url = [NSURL fileURLWithPath:filenameString];

        if ([self loadTextureFromURL:url]) {
            [self setHudText:""];

            return YES;
        }
    }

    return NO;
}

- (BOOL)loadArchive:(const char *)zipFilename
{
    _zipMmap.close();
    if (!_zipMmap.open(zipFilename)) {
        return NO;
    }

    // Note: if mmap fails, could read entire zip into memory
    // and then still use the same code below.

    if (!_zip.openForRead(_zipMmap.data(), _zipMmap.dataLength())) {
        return NO;
    }

    // filter out unsupported extensions
    vector<string> extensions = {
        ".ktx", ".ktx2", ".png", ".dds" // textures
#if USE_GLTF
        , ".glb", ".gltf" // models
#endif
    };
    
    _zip.filterExtensions(extensions);

    // don't switch to empty archive
    if (_zip.zipEntrys().empty()) {
        return NO;
    }

    // load the first entry in the archive
    _fileArchiveIndex = 0;
    
    // copy names into the files view
    [_tableViewController.items removeAllObjects];
    for (const auto& entry: _zip.zipEntrys()) {
        const char *filenameShort = toFilenameShort(entry.filename);
        [_tableViewController.items addObject: [NSString stringWithUTF8String: filenameShort]];
    }
    [_tableView reloadData];
    
    // set selection
    [_tableView selectRowIndexes:[NSIndexSet indexSetWithIndex:_fileArchiveIndex] byExtendingSelection:NO];
    [_tableView scrollRowToVisible:_fileArchiveIndex];
    
    // want it to respond to arrow keys
    [self.window makeFirstResponder: _tableView];
    
    // hack to see table
    [self hideTables];
    
    return YES;
}

- (BOOL)advanceFileFromAchive:(BOOL)increment
{
    if ((!_zipMmap.data()) || _zip.zipEntrys().empty()) {
        // no archive loaded or it's empty
        return NO;
    }
    size_t numEntries = _zip.zipEntrys().size();

    if (increment)
        _fileArchiveIndex++;
    else
        _fileArchiveIndex += numEntries - 1;  // back 1

    _fileArchiveIndex = _fileArchiveIndex % numEntries;

    // set selection
    [_tableView selectRowIndexes:[NSIndexSet indexSetWithIndex:_fileArchiveIndex] byExtendingSelection:NO];
    [_tableView scrollRowToVisible:_fileArchiveIndex];
    
    // want it to respond to arrow keys
    [self.window makeFirstResponder: _tableView];
    
    // show the files table
    [self hideTables];
    _tableView.hidden = NO;
    
    // also have to hide hud or it will obscure the visible table
    _hudHidden = true;
    [self updateHudVisibility];
    
    return [self loadFileFromArchive];
}

- (BOOL)advanceFileFromFolder:(BOOL)increment
{
    if (_folderFiles.empty()) {
        // no archive loaded
        return NO;
    }

    size_t numEntries = _folderFiles.size();
    if (increment)
        _fileFolderIndex++;
    else
        _fileFolderIndex += numEntries - 1;  // back 1

    _fileFolderIndex = _fileFolderIndex % numEntries;

    // set selection
    [_tableView selectRowIndexes:[NSIndexSet indexSetWithIndex:_fileFolderIndex] byExtendingSelection:NO];
    [_tableView scrollRowToVisible:_fileFolderIndex];
    
    // want it to respond to arrow keys
    [self.window makeFirstResponder: _tableView];
    
    // show the files table
    [self hideTables];
    _tableView.hidden = NO;
    
    _hudHidden = true;
    [self updateHudVisibility];
    
    return [self loadFileFromFolder];
}

- (BOOL)setImageFromSelection:(NSInteger)index {
    if (_zipMmap.data() && !_zip.zipEntrys().empty()) {
        if (_fileArchiveIndex != index) {
            _fileArchiveIndex = index;
            return [self loadFileFromArchive];
        }
    }

    if (!_folderFiles.empty()) {
        if (_fileFolderIndex != index) {
            _fileFolderIndex = index;
            return [self loadFileFromFolder];
        }
    }
    return NO;
}

- (BOOL)setShapeFromSelection:(NSInteger)index {
    if (_showSettings->meshNumber != index) {
        _showSettings->meshNumber = index;
        self.needsDisplay = YES;
        return YES;
    }
    return NO;
}

- (BOOL)findFilenameInFolders:(const string &)filename
{
    // TODO: binary search for the filename in the array, but would have to be in
    // same directory

    bool isFound = false;
    for (const auto &search : _folderFiles) {
        if (search == filename) {
            isFound = true;
            break;
        }
    }
    return isFound;
}

- (BOOL)loadFileFromFolder
{
    // now lookup the filename and data at that entry
    const char *filename = _folderFiles[_fileFolderIndex].c_str();
    string fullFilename = filename;
    auto timestamp = FileHelper::modificationTimestamp(filename);
    
    bool isModel =
        endsWithExtension(filename, ".gltf") ||
        endsWithExtension(filename, ".gtb");
    if (isModel)
        return [self loadModelFile:nil filename:filename];
    
    // have already filtered filenames out, so this should never get hit
    bool isPNG = isPNGFilename(filename);
    if (!(isPNG ||
          isKTXFilename(filename) ||
          isKTX2Filename(filename) ||
          isDDSFilename(filename))
    ) {
        return NO;
    }

    const char *ext = strrchr(filename, '.');

    // first only do this on albedo/diffuse textures

    // find matching png
    string search = "-a";
    search += ext;

    auto searchPos = fullFilename.find(search);
    bool isFound = searchPos != string::npos;

    if (!isFound) {
        search = "-d";
        search += ext;

        searchPos = fullFilename.find(search);
        isFound = searchPos != string::npos;
    }

    bool isSrgb = isFound;

    string normalFilename;
    bool hasNormal = false;

    if (isFound) {
        normalFilename = fullFilename;
        normalFilename = normalFilename.erase(searchPos);
        normalFilename += "-n";
        normalFilename += ext;

        hasNormal = [self findFilenameInFolders:normalFilename];
    }

    //-------------------------------

    KTXImage image;
    KTXImageData imageDataKTX;

    KTXImage imageNormal;
    KTXImageData imageNormalDataKTX;

    // this requires decode and conversion to RGBA8u
    if (!imageDataKTX.open(fullFilename.c_str(), image)) {
        return NO;
    }

    if (hasNormal &&
        imageNormalDataKTX.open(normalFilename.c_str(), imageNormal)) {
        // shaders only pull from albedo + normal on these texture types
        if (imageNormal.textureType == image.textureType &&
            (imageNormal.textureType == MyMTLTextureType2D ||
             imageNormal.textureType == MyMTLTextureType2DArray)) {
            // hasNormal = true;
        }
        else {
            hasNormal = false;
        }
    }
    
    if (doReplaceSrgbFromType && isPNG) {
        image.pixelFormat = isSrgb ? MyMTLPixelFormatRGBA8Unorm_sRGB : MyMTLPixelFormatRGBA8Unorm;
    }

    
    Renderer *renderer = (Renderer *)self.delegate;
    [renderer releaseAllPendingTextures];
    
    if (![renderer loadTextureFromImage:fullFilename.c_str()
                              timestamp:timestamp
                                  image:image
                            imageNormal:hasNormal ? &imageNormal : nullptr
                              isArchive:NO]) {
        return NO;
    }

    //-------------------------------

    // set title to filename, chop this to just file+ext, not directory
    const char *filenameShort = strrchr(filename, '/');
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

    self.window.title = [NSString stringWithUTF8String:title.c_str()];

    // doesn't set imageURL or update the recent document menu

    // show the controls
    if (_noImageLoaded) {
        _buttonStack.hidden = NO;  // show controls
        _noImageLoaded = NO;
    }

    _showSettings->isArchive = false;
    _showSettings->isFolder = true;

    // show/hide button
    [self updateUIAfterLoad];
    
    self.needsDisplay = YES;
    return YES;
}

- (BOOL)loadFileFromArchive
{
    // now lookup the filename and data at that entry
    const auto& entry = _zip.zipEntrys()[_fileArchiveIndex];
    const char* filename = entry.filename;
    string fullFilename = filename;
    double timestamp = (double)entry.modificationDate;

    bool isModel =
        endsWithExtension(filename, ".gltf") ||
        endsWithExtension(filename, ".gtb");
    if (isModel)
        return [self loadModelFile:nil filename:filename];
    
    //--------
    
    bool isPNG = isPNGFilename(filename);

    if (!(isPNG ||
          isKTXFilename(filename) ||
          isKTX2Filename(filename) ||
          isDDSFilename(filename))) {
        return NO;
    }

    const char *ext = strrchr(filename, '.');

    // first only do this on albedo/diffuse textures

    string search = "-a";
    search += ext;

    auto searchPos = fullFilename.find(search);
    bool isFound = searchPos != string::npos;

    if (!isFound) {
        search = "-d";
        search += ext;

        searchPos = fullFilename.find(search);
        isFound = searchPos != string::npos;
    }

    bool isSrgb = isFound;

    //---------------------------

    const uint8_t *imageData = nullptr;
    uint64_t imageDataLength = 0;

    const uint8_t *imageNormalData = nullptr;
    uint64_t imageNormalDataLength = 0;

    // search for main file - can be albedo or normal
    if (!_zip.extractRaw(filename, &imageData, imageDataLength)) {
        return NO;
    }

    // search for normal map in the same archive
    string normalFilename;
    bool hasNormal = false;

    if (isFound) {
        normalFilename = fullFilename;
        normalFilename = normalFilename.erase(searchPos);
        normalFilename += "-n";
        normalFilename += ext;

        hasNormal = _zip.extractRaw(normalFilename.c_str(), &imageNormalData,
                                    imageNormalDataLength);
    }

    //---------------------------

    // files in archive are just offsets into the mmap
    // That's why we can't just pass filenames to the renderer
    KTXImage image;
    KTXImageData imageDataKTX;

    KTXImage imageNormal;
    KTXImageData imageNormalDataKTX;

    if (!imageDataKTX.open(imageData, imageDataLength, image)) {
        return NO;
    }

    if (hasNormal && imageNormalDataKTX.open(
                         imageNormalData, imageNormalDataLength, imageNormal)) {
        // shaders only pull from albedo + normal on these texture types
        if (imageNormal.textureType == image.textureType &&
            (imageNormal.textureType == MyMTLTextureType2D ||
             imageNormal.textureType == MyMTLTextureType2DArray)) {
            // hasNormal = true;
        }
        else {
            hasNormal = false;
        }
    }

    if (doReplaceSrgbFromType && isPNG) {
        image.pixelFormat = isSrgb ? MyMTLPixelFormatRGBA8Unorm_sRGB : MyMTLPixelFormatRGBA8Unorm;
    }
    
//    if (isPNG && isSrgb) {
//        image.pixelFormat = MyMTLPixelFormatRGBA8Unorm_sRGB;
//    }

    Renderer *renderer = (Renderer *)self.delegate;
    [renderer releaseAllPendingTextures];
    
    if (![renderer loadTextureFromImage:fullFilename.c_str()
                              timestamp:(double)timestamp
                                  image:image
                            imageNormal:hasNormal ? &imageNormal : nullptr
                              isArchive:YES]) {
        return NO;
    }

    //---------------------------------

    // set title to filename, chop this to just file+ext, not directory
    const char *filenameShort = strrchr(filename, '/');
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

    self.window.title = [NSString stringWithUTF8String:title.c_str()];

    // doesn't set imageURL or update the recent document menu

    // show the controls
    if (_noImageLoaded) {
        _buttonStack.hidden = NO;  // show controls
        _noImageLoaded = NO;
    }

    _showSettings->isArchive = true;
    _showSettings->isFolder = false;

    // show/hide button
    [self updateUIAfterLoad];

    
    self.needsDisplay = YES;
    return YES;
}

- (BOOL)loadTextureFromURL:(NSURL *)url
{
    // NSLog(@"LoadTexture");

    // turn back on the hud if was in a list view
    _hudHidden = false;
    [self updateHudVisibility];
    
    const char *filename = url.fileSystemRepresentation;
    if (filename == nullptr) {
        // Fixed by converting dropped urls into paths then back to a url.
        // When file replaced the drop url is no longer valid.
        KLOGE("kramv", "Fix this load url returning nil issue");
        return NO;
    }
    
    Renderer *renderer = (Renderer *)self.delegate;
    
    // folders can have a . in them f.e. 2.0/blah/...
    bool isDirectory = url.hasDirectoryPath;
    
    // this likely means it's a local file directory
    if (isDirectory) {
        // make list of all file in the directory

        if (!self.imageURL || (!([self.imageURL isEqualTo:url]))) {
            NSDirectoryEnumerator *directoryEnumerator =
                [[NSFileManager defaultManager]
                               enumeratorAtURL:url
                    includingPropertiesForKeys:[NSArray array]
                                       options:0
                                  errorHandler:  // nil
                                      ^BOOL(NSURL *urlArg, NSError *error) {
                                          macroUnusedVar(urlArg);
                                          macroUnusedVar(error);

                                          // handle error
                                          return NO;
                                      }];

            vector<string> files;
#if USE_GLTF
            // only display models in folder if found, ignore the png/jpg files
            while (NSURL *fileOrDirectoryURL = [directoryEnumerator nextObject]) {
                const char *name = fileOrDirectoryURL.fileSystemRepresentation;

                bool isGLTF = endsWithExtension(name, ".gltf");
                bool isGLB = endsWithExtension(name, ".glb");
                if (isGLTF || isGLB)
                {
                    files.push_back(name);
                }
            }
#endif

            // don't change to this folder if it's devoid of content
            if (files.empty()) {
#if USE_GLTF
                // reset the enumerator
                directoryEnumerator =
                    [[NSFileManager defaultManager]
                                   enumeratorAtURL:url
                        includingPropertiesForKeys:[NSArray array]
                                           options:0
                                      errorHandler:  // nil
                                          ^BOOL(NSURL *urlArg, NSError *error) {
                                              macroUnusedVar(urlArg);
                                              macroUnusedVar(error);

                                              // handle error
                                              return NO;
                                          }];
#endif
                while (NSURL *fileOrDirectoryURL = [directoryEnumerator nextObject]) {
                    const char *name = fileOrDirectoryURL.fileSystemRepresentation;

                    // filter only types that are supported
                    bool isPNG = isPNGFilename(name);
                    bool isKTX = isKTXFilename(name);
                    bool isKTX2 = isKTX2Filename(name);
                    bool isDDS = isDDSFilename(name);
    
                    if (isPNG || isKTX || isKTX2 || isDDS)
                    {
                        files.push_back(name);
                    }
                }
            }
            
            if (files.empty()) {
                return NO;
            }

            // add it to recent docs
            NSDocumentController *dc =
                [NSDocumentController sharedDocumentController];
            [dc noteNewRecentDocumentURL:url];

            // sort them
#if USE_EASTL
            NAMESPACE_STL::quick_sort(files.begin(), files.end());
#else
            NAMESPACE_STL::sort(files.begin(), files.end());
#endif
            // replicate archive logic below

            self.imageURL = url;

            // preserve old folder
            string existingFilename;
            if (_fileFolderIndex < (int32_t)_folderFiles.size())
                existingFilename = _folderFiles[_fileFolderIndex];
            else
                _fileFolderIndex = 0;

            _folderFiles = files;

            // TODO: preserve filename before load, and restore that index, by finding
            // that name in refreshed folder list

            if (!existingFilename.empty()) {
                uint32_t index = 0;
                for (const auto &fileIt : _folderFiles) {
                    if (fileIt == existingFilename) {
                        break;
                    }
                }

                _fileFolderIndex = index;
            }
            
            [_tableViewController.items removeAllObjects];
            for (const auto& file: files) {
                const char *filenameShort = toFilenameShort(file.c_str());
                [_tableViewController.items addObject: [NSString stringWithUTF8String: filenameShort]];
            }
            [_tableView reloadData];
            
            [_tableView selectRowIndexes:[NSIndexSet indexSetWithIndex:_fileFolderIndex] byExtendingSelection:NO];
            [_tableView scrollRowToVisible:_fileFolderIndex];
            
            [self hideTables];
        }

        // now load image from directory
        _showSettings->isArchive = false;
        _showSettings->isFolder = true;

        
        // now load the file at the index
        setErrorLogCapture(true);

        BOOL success = [self loadFileFromFolder];

        if (!success) {
            // get back error text from the failed load
            string errorText;
            getErrorLogCaptureText(errorText);
            setErrorLogCapture(false);

            const string &folder = _folderFiles[_fileFolderIndex];

            // prepend filename
            string finalErrorText;
            append_sprintf(finalErrorText, "Could not load from folder:\n %s\n",
                           folder.c_str());
            finalErrorText += errorText;

            [self setHudText:finalErrorText.c_str()];
        }

        setErrorLogCapture(false);
        return success;
    }

    //-------------------
    
    if (endsWithExtension(filename, ".metallib")) {
        if ([renderer hotloadShaders:filename]) {
            NSURL *metallibFileURL =
                [NSURL fileURLWithPath:[NSString stringWithUTF8String:filename]];

            // add to recent docs, so can reload quickly
            NSDocumentController *dc =
                [NSDocumentController sharedDocumentController];
            [dc noteNewRecentDocumentURL:metallibFileURL];

            return YES;
        }
        return NO;
    }

    // file is not a supported extension
    if (!(
          // archive
          endsWithExtension(filename, ".zip") ||
          
          // images
          isPNGFilename(filename) ||
          isKTXFilename(filename) ||
          isKTX2Filename(filename) ||
          isDDSFilename(filename) ||
          
          // models
          endsWithExtension(filename, ".gltf") ||
          endsWithExtension(filename, ".glb")
        ))
    {
        string errorText =
            "Unsupported file extension, must be .zip, .png, .ktx, .ktx2, .dds, .gltf, .glb\n";

        string finalErrorText;
        append_sprintf(finalErrorText, "Could not load from file:\n %s\n",
                       filename);
        finalErrorText += errorText;

        [self setHudText:finalErrorText.c_str()];
        return NO;
    }

    if (endsWithExtension(filename, ".gltf") ||
        endsWithExtension(filename, ".glb"))
    {
        return [self loadModelFile:url filename:nullptr];
    }
    
    // for now, knock out model if loading an image
    // TODO: might want to unload even before loading a new model
    [renderer unloadModel];
    
    //-------------------

    if (endsWithExtension(filename, ".zip")) {
        auto archiveTimestamp = FileHelper::modificationTimestamp(filename);

        if (!self.imageURL || (!([self.imageURL isEqualTo:url])) ||
            (self.lastArchiveTimestamp != archiveTimestamp)) {
            // copy this out before it's replaced
            string existingFilename;
            if (_fileArchiveIndex < (int32_t)_zip.zipEntrys().size())
                existingFilename = _zip.zipEntrys()[_fileArchiveIndex].filename;
            else
                _fileArchiveIndex = 0;

            BOOL isArchiveLoaded = [self loadArchive:filename];
            if (!isArchiveLoaded) {
                return NO;
            }

            // store the archive url
            self.imageURL = url;
            self.lastArchiveTimestamp = archiveTimestamp;

            // add it to recent docs
            NSDocumentController *dc =
                [NSDocumentController sharedDocumentController];
            [dc noteNewRecentDocumentURL:url];

            // now reload the filename if needed
            if (!existingFilename.empty()) {
                const ZipEntry *formerEntry = _zip.zipEntry(existingFilename.c_str());
                if (formerEntry) {
                    // lookup the index in the remapIndices table
                    _fileArchiveIndex =
                        (uintptr_t)(formerEntry - &_zip.zipEntrys().front());
                }
                else {
                    _fileArchiveIndex = 0;
                }
            }
        }

        setErrorLogCapture(true);

        BOOL success = [self loadFileFromArchive];

        if (!success) {
            // get back error text from the failed load
            string errorText;
            getErrorLogCaptureText(errorText);
            setErrorLogCapture(false);

            const auto &entry = _zip.zipEntrys()[_fileArchiveIndex];
            const char *archiveFilename = entry.filename;

            // prepend filename
            string finalErrorText;
            append_sprintf(finalErrorText, "Could not load from archive:\n %s\n",
                           archiveFilename);
            finalErrorText += errorText;

            [self setHudText:finalErrorText.c_str()];
        }

        setErrorLogCapture(false);
        return success;
    }

    return [self loadImageFile:url];
}

-(BOOL)loadModelFile:(NSURL*)url filename:(const char*)filename
{
#if USE_GLTF
    // Right now can only load these if they are embedded, since sandbox will
    // fail to load related .png and .bin files.  There's a way to opt into
    // related items, but they must all be named the same.  I think if folder
    // instead of the file is selected, then could search and find the gltf files
    // and the other files.

    //----------------------
    // These assets should be combined into a single hierarchy, and be able to
    // save out a scene with all of them in a single scene.  But that should
    // probably reference original content in case it's updated.
    
    Renderer *renderer = (Renderer *)self.delegate;
    [renderer releaseAllPendingTextures];
    
    setErrorLogCapture(true);

    // set title to filename, chop this to just file+ext, not directory
    if (url != nil)
        filename = url.fileSystemRepresentation;
    const char* filenameShort = toFilenameShort(filename);

    NSURL* gltfFileURL =
        [NSURL fileURLWithPath:[NSString stringWithUTF8String:filename]];

    BOOL success = [renderer loadModel:gltfFileURL];
    
    // TODO: split this off to a completion handler, since loadModel is async
    // and should probably also have a cancellation (or counter)
    
    // show/hide button
    [self updateUIAfterLoad];
    
    if (!success) {
        string errorText;
        getErrorLogCaptureText(errorText);
        setErrorLogCapture(false);
        
        string finalErrorText;
        append_sprintf(finalErrorText, "Could not load model from file:\n %s\n",
                       filename);
        finalErrorText += errorText;

        [self setHudText:finalErrorText.c_str()];
        
        return NO;
    }

    // was using subtitle, but that's macOS 11.0 feature.
    string title = "kramv - ";
    title += filenameShort;

    self.window.title = [NSString stringWithUTF8String:title.c_str()];

    // if url is nil, then loading out of archive or folder
    // and don't want to save that or set imageURL
    if (url != nil)
    {
        // add to recent docs, so can reload quickly
        NSDocumentController *dc =
            [NSDocumentController sharedDocumentController];
        [dc noteNewRecentDocumentURL:gltfFileURL];

        // TODO: not really an image
        self.imageURL = gltfFileURL;
        
        // this may be loading out of folder/archive, but if url passed then it isn't
        _showSettings->isArchive = false;
        _showSettings->isFolder = false;
        
        // no need for file table on single files
        _tableView.hidden = YES;
    }
    
    // show the controls
    if (_noImageLoaded) {
        _buttonStack.hidden = NO;  // show controls
        _noImageLoaded = NO;
    }

    setErrorLogCapture(false);

    self.needsDisplay = YES;

    return success;
#else
    return NO;
#endif
}

-(BOOL)loadImageFile:(NSURL*)url
{
    Renderer *renderer = (Renderer *)self.delegate;
    setErrorLogCapture(true);

    // set title to filename, chop this to just file+ext, not directory
    const char* filename = url.fileSystemRepresentation;
    const char* filenameShort = toFilenameShort(filename);
    
    BOOL success = [renderer loadTexture:url];

    if (!success) {
        // get back error text from the failed load
        string errorText;
        getErrorLogCaptureText(errorText);
        setErrorLogCapture(false);

        // prepend filename
        string finalErrorText;
        append_sprintf(finalErrorText, "Could not load from file\n %s\n", filename);
        finalErrorText += errorText;

        [self setHudText:finalErrorText.c_str()];
        return NO;
    }
    setErrorLogCapture(false);

    // was using subtitle, but that's macOS 11.0 feature.
    string title = "kramv - ";
    title += formatTypeName(_showSettings->originalFormat);
    title += " - ";
    title += filenameShort;

    self.window.title = [NSString stringWithUTF8String:title.c_str()];

    // topmost entry will be the recently opened document
    // some entries may go stale if directories change, not sure who validates the
    // list

    // add to recent document menu
    NSDocumentController *dc = [NSDocumentController sharedDocumentController];
    [dc noteNewRecentDocumentURL:url];

    self.imageURL = url;

    // show the controls
    if (_noImageLoaded) {
        _buttonStack.hidden = NO;  // show controls
        _noImageLoaded = NO;
    }

    _showSettings->isArchive = false;
    _showSettings->isFolder = false;

    // show/hide button
    [self updateUIAfterLoad];
    // no need for file table on single files
    _tableView.hidden = YES;
    
    self.needsDisplay = YES;
    return YES;
}

- (void)setupUI
{
    [self hideTables];
}

- (void)concludeDragOperation:(id)sender
{
    // did setNeedsDisplay, but already doing that in loadTextureFromURL
}

// this doesn't seem to enable New.  Was able to get "Open" to highlight by
// setting NSDocument as class for doc types.
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

- (void)tableViewSelectionDidChange:(NSNotification *)notification
{
    if (notification.object == _tableView)
    {
        // image
        NSInteger selectedRow = [_tableView selectedRow];
        [self setImageFromSelection:selectedRow];
    }
}

- (void)addNotifications
{
    // listen for the selection change messages
    [[NSNotificationCenter defaultCenter] addObserver:self
                                              selector:@selector(tableViewSelectionDidChange:)
                                                  name:NSTableViewSelectionDidChangeNotification object:nil];
}

- (void)removeNotifications
{
    // listen for the selection change messages
    [[NSNotificationCenter defaultCenter] removeObserver:self];
}


- (BOOL)acceptsFirstResponder
{
    return YES;
}

@end

//-------------

// Our macOS view controller.
@interface GameViewController : NSViewController

@end

@implementation GameViewController {
    MyMTKView *_view;

    Renderer *_renderer;

    NSTrackingArea *_trackingArea;
}

- (void)viewWillDisappear
{
    [_view removeNotifications];
}

- (void)viewDidLoad
{
    [super viewDidLoad];

    _view = (MyMTKView *)self.view;

    // have to disable this since reading back from textures
    // that slows the blit to the screen
    _view.framebufferOnly = NO;

    _view.device = MTLCreateSystemDefaultDevice();

    if (!_view.device) {
        return;
    }

    _renderer = [[Renderer alloc] initWithMetalKitView:_view
                                              settings:_view.showSettings];

    
    // https://developer.apple.com/library/archive/documentation/Cocoa/Conceptual/EventOverview/TrackingAreaObjects/TrackingAreaObjects.html
    // this is better than requesting mousemoved events, they're only sent when
    // cursor is inside
    _trackingArea = [[NSTrackingArea alloc]
        initWithRect:_view.bounds
             options:(NSTrackingMouseEnteredAndExited | NSTrackingMouseMoved |
                      // NSTrackingActiveWhenFirstResponder |
                      // NSTrackingActiveInKeyWindow |
                      NSTrackingActiveInActiveApp |
                      NSTrackingInVisibleRect // ignore rect above, bur resizes with view
                      )
               owner:_view
            userInfo:nil];
    [_view addTrackingArea:_trackingArea];

    [_view addNotifications];
    
    [_view setupUI];
    
    // original sample code was sending down _view.bounds.size, but need
    // drawableSize this was causing all sorts of inconsistencies
    [_renderer mtkView:_view drawableSizeWillChange:_view.drawableSize];

    _view.delegate = _renderer;
}



@end

//-------------

int main(int argc, const char *argv[])
{
    @autoreleasepool {
        // Setup code that might create autoreleased objects goes here.
    }
    return NSApplicationMain(argc, argv);
}
