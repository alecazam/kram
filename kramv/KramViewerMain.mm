// kram - Copyright 2020-2022 by Alec Miller. - MIT License
// The license and copyright notice shall be included
// in all copies or substantial portions of the Software.

// using -fmodules and -fcxx-modules
//@import Cocoa;
//@import Metal;
//@import MetalKit;

#import <Cocoa/Cocoa.h>
#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>
#import <TargetConditionals.h>

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

#ifdef NDEBUG
static bool doPrintPanZoom = false;
#else
static bool doPrintPanZoom = false;
#endif

#include <UniformTypeIdentifiers/UTType.h>

using namespace simd;
using namespace kram;
using namespace NAMESPACE_STL;

#define ArrayCount(x) (sizeof(x) / sizeof(x[0]))

static string filenameNoExtension(const char* filename)
{
    const char* dotPosStr = strrchr(filename, '.');
    if (dotPosStr == nullptr)
        return filename;
    auto dotPos = dotPosStr - filename;
    
    // now chop off the extension
    string filenameNoExt = filename;
    return filenameNoExt.substr(0, dotPos);
}

static void findPossibleNormalMapFromAlbedoFilename(const char* filename, vector<string>& normalFilenames)
{
    normalFilenames.clear();
    
    string filenameShort = filename;
    
    const char* ext = strrchr(filename, '.');

    const char* dotPosStr = strrchr(filenameShort.c_str(), '.');
    if (dotPosStr == nullptr)
        return;
    
    auto dotPos = dotPosStr - filenameShort.c_str();
    
    // now chop off the extension
    filenameShort = filenameShort.substr(0, dotPos);

    const char* searches[] = { "-a", "-d", "_Color", "_baseColor" };
    
    for (uint32_t i = 0; i < ArrayCount(searches); ++i) {
        const char* search = searches[i];
        if (endsWith(filenameShort, search)) {
            filenameShort = filenameShort.substr(0, filenameShort.length()-strlen(search));
            break;
        }
    }
     
    const char* suffixes[] = { "-n", "_normal", "_Normal" };
    
    string normalFilename;
    for (uint32_t i = 0; i < ArrayCount(suffixes); ++i) {
        const char* suffix = suffixes[i];
        
        // may need to try various names, and see if any exist
        normalFilename = filenameShort;
        normalFilename += suffix;
        normalFilename += ext;
        
        normalFilenames.push_back( normalFilename );
    }
}

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

static const vector<const char*> supportedModelExt = {
#if USE_GLTF
     ".gltf",
     ".glb",
#endif
#if USE_USD
    ".gltf",
    ".glb",
#endif
};

struct File {
    string name;
    int32_t urlIndex;
    
    // Note: not sorting by urlIndex currently
    bool operator <(const File& rhs) const
        { return strcasecmp(toFilenameShort(name.c_str()), toFilenameShort(rhs.name.c_str())) < 0; }
};

bool isSupportedModelFilename(const char* filename) {
    for (const char* ext: supportedModelExt) {
        if (endsWithExtension(filename, ext)) {
            return true;
        }
    }
    return false;
}
bool isSupportedArchiveFilename(const char* filename) {
    return endsWithExtension(filename, ".zip");
}

struct MouseData
{
    NSPoint originPoint;
    NSPoint oldPoint;
    NSPoint newPoint;
    
    NSPoint pan;
};

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
    Action(const char* icon_, const char* tip_, Key keyCode_)
        : icon(icon_), tip(tip_), keyCode(keyCode_) {}
    
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
//@property(retain, nonatomic, readwrite, nullable) NSURL *imageURL;

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


- (BOOL)loadTextureFromURLs:(NSArray<NSURL*>*)url;

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
    NSString* identifier = tableColumn.identifier;
    NSTableCellView* cell = [tableView makeViewWithIdentifier:identifier owner:self];
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
    
    // throw into an array
    NSArray<NSURL*>* urls = @[url];
    
    NSApplication* app = [NSApplication sharedApplication];
    MyMTKView* view = app.mainWindow.contentView;
    
    BOOL success = [view loadTextureFromURLs:urls];
    if (success) {
        // Note: if I return NO from this call then a dialog pops up that image
        // couldn't be loaded, but then the readFromURL is called everytime a new
        // image is picked from the list.

        [view setHudText:""];
        [view fixupDocumentList];
    }

    return success;
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
    MyMTKView* view = sender.mainWindow.contentView;
    [view loadTextureFromURLs:urls];
    [view fixupDocumentList];
}



- (IBAction)showAboutDialog:(id)sender
{
    // calls openDocumentWithContentsOfURL above
    NSMutableDictionary<NSAboutPanelOptionKey, id>* options =
        [[NSMutableDictionary alloc] init];

    // name and icon are already supplied

    // want to embed the git tag here
    options[@"Copyright"] =
        [NSString stringWithUTF8String:"kram ©2020-2022 by Alec Miller"];

    // add a link to kram website, skip the Visit text
    NSMutableAttributedString* str = [[NSMutableAttributedString alloc]
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
    const char* version = KRAM_VERSION;
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

NSArray<NSString *>* pasteboardTypes = @[
    // don't really want generic urls, but need folders to drop
    //NSPasteboardTypeURL
    
    // this is preventing folder drops ?
    NSPasteboardTypeFileURL
];

/* correlates with
 
public.directory.
public.png,
org.khronos.ktx,
public.ktx2,
com.microsoft.dds,
public.zip-archive,
dyn.ah62d4rv4ge8043pyqf0g24pc, // ick - metallib
dyn.ah62d4rv4ge80s5dyq2,       // ick - gltf
dyn.ah62d4rv4ge80s5dc          // ick - glb
 
*/

// ktx, ktx2, png, and dds for images
// zip, metallib
// gltf, glb files for models
NSArray<NSString*>* utis = @[
  @"public.directory",
    
  [UTType typeWithFilenameExtension: @"png"].identifier,
  [UTType typeWithFilenameExtension: @"ktx"].identifier,
  [UTType typeWithFilenameExtension: @"ktx2"].identifier,
  [UTType typeWithFilenameExtension: @"dds"].identifier,
  
  [UTType typeWithFilenameExtension: @"zip"].identifier,
  [UTType typeWithFilenameExtension: @"metallib"].identifier,
  
#if USE_GLTF
  [UTType typeWithFilenameExtension: @"gltf"].identifier,
  [UTType typeWithFilenameExtension: @"glb"].identifier,
  //@"model/gltf+json",
  //@"model/gltf+binary"
#endif
#if USE_USD
  [UTType typeWithFilenameExtension: @"usd"].identifier,
  [UTType typeWithFilenameExtension: @"usd"].identifier,
  [UTType typeWithFilenameExtension: @"usda"].identifier,
#endif
  
  // read -atlas.json files
  [UTType typeWithFilenameExtension: @"json"].identifier
];
NSDictionary* pasteboardOptions = @{
    // This means only these uti can be droped.
    NSPasteboardURLReadingContentsConformToTypesKey: utis
    
    // Don't use this it prevents folder urls
    //, NSPasteboardURLReadingFileURLsOnlyKey: @YES
};

// This is an open archive
struct FileContainer {
    // allow zip files to be dropped and opened, and can advance through bundle
    // content.
    
    // TODO: Add FileHelper if acrhive file is networked, but would require
    // full load to memory.
    
    ZipHelper zip;
    MmapHelper zipMmap;
};

@implementation MyMTKView {
    NSMenu* _viewMenu;  // really the items
    NSStackView* _buttonStack;
    NSMutableArray<NSButton *>* _buttonArray;
    NSTextField* _hudLabel;
    NSTextField* _hudLabel2;
    
    // Offer list of files in archives
    // TODO: move to NSOutlineView since that can show archive folders with content inside
    IBOutlet NSTableView* _tableView;
    IBOutlet TableViewController* _tableViewController;
    
    vector<string> _textSlots;
    ShowSettings* _showSettings;

    BOOL _noImageLoaded;
    string _archiveName; // archive or blank
    
    // folders and archives and multi-drop files are filled into this
    vector<File> _files;
    int32_t _fileIndex;
   
    NSArray<NSURL*>* _urls;
    // One of these per url in _urlss
    vector<FileContainer*> _containers;
    
    Action* _actionPlay;
    Action* _actionShapeUVPreview;
    Action* _actionHelp;
    Action* _actionInfo;
    Action* _actionHud;
    Action* _actionShowAll;
    
    Action* _actionPreview;
    Action* _actionWrap;
    Action* _actionPremul;
    Action* _actionSigned;
    
    Action* _actionDiff;
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
    Action* _actionCounterpart;
    Action* _actionPrevCounterpart;
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
    
    // copy of modifier flags, can tie drop actions to this
    NSEventModifierFlags _modifierFlags;
    
    vector<Action> _actions;
}

- (void)awakeFromNib
{
    [super awakeFromNib];

    // vertical offset of table down so hud can display info
    NSScrollView* scrollView = [_tableView enclosingScrollView];
    CGRect rect = scrollView.frame;
    rect.origin.y += 50;
    scrollView.frame = rect;
    

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

    self.clearColor = MTLClearColorMake(0.005f, 0.005f, 0.005f, 0.0f);

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
    NSDocumentController* dc = [NSDocumentController sharedDocumentController];
    NSDocument* currentDoc = dc.currentDocument;
    NSMutableArray* docsToRemove = [[NSMutableArray alloc] init];
    for (NSDocument* doc in dc.documents) {
        if (doc != currentDoc)
            [docsToRemove addObject:doc];
    }

    for (NSDocument* doc in docsToRemove) {
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

        Action("Q", "Quick Diff", Key::Q), // C/D already taken
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
        Action("←", "Prev Counterpart", Key::LeftArrow),
        Action("→", "Next Counterpart", Key::RightArrow),
        
        Action("R", "Reload", Key::R),
        Action("0", "Fit", Key::Num0),

        Action("", "", Key::A), // sep

        Action(" ", "Play", Key::Space),
        Action("6", "Shape UVPreview", Key::Num6),
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
       
        &_actionDiff,
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
        &_actionPrevCounterpart,
        &_actionCounterpart,
        
        &_actionReload,
        &_actionFit,
        
        &_actionPlay,
        &_actionShapeUVPreview,
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

   int32_t numActions = ArrayCount(actions);
    
    NSMutableArray* buttons = [[NSMutableArray alloc] init];

    for (int32_t i = 0; i < numActions; ++i) {
        Action& action = actions[i];
        const char* icon = action.icon;
        const char* tip = action.tip;

        NSString* name = [NSString stringWithUTF8String:icon];
        NSString* toolTip = [NSString stringWithUTF8String:tip];

        NSButton* button = nil;

        button = [NSButton buttonWithTitle:name
                                    target:self
                                    action:@selector(handleAction:)];
        button.toolTip = toolTip;
        button.hidden = NO;

        button.buttonType = NSButtonTypeToggle;
        // NSButtonTypeOnOff
        button.bordered = NO;
        [button setFrame:rect];

        // https://stackoverflow.com/questions/4467597/how-do-you-stroke-the-outside-of-an-nsattributedstring
        
        NSMutableDictionary* attribsOff = [NSMutableDictionary dictionaryWithObjectsAndKeys:
            //[NSFont systemFontOfSize:64.0],NSFontAttributeName,
            [NSColor whiteColor],NSForegroundColorAttributeName,
            [NSNumber numberWithFloat:-2.0],NSStrokeWidthAttributeName,
            [NSColor blackColor],NSStrokeColorAttributeName,
            nil];
        NSMutableDictionary* attribsOn = [NSMutableDictionary dictionaryWithObjectsAndKeys:
            //[NSFont systemFontOfSize:64.0],NSFontAttributeName,
            [NSColor systemBlueColor],NSForegroundColorAttributeName,
            [NSNumber numberWithFloat:-2.0],NSStrokeWidthAttributeName,
            [NSColor blackColor],NSStrokeColorAttributeName,
            nil];
        button.attributedTitle = [[NSMutableAttributedString alloc] initWithString:name attributes:attribsOff];
        
        // Have to set this too, or button doesn't go blue
        button.attributedAlternateTitle = [[NSMutableAttributedString alloc] initWithString:name attributes:attribsOn];
        
#if 0 // this isn't appearing
        button.wantsLayer = YES;
        if (button.layer) {
//            CGFloat glowColor[] = {1.0, 0.0, 0.0, 1.0};
//            button.layer.masksToBounds = false;
//            button.layer.shadowColor = CGColorCreate(CGColorSpaceCreateDeviceRGB(), glowColor);
//            button.layer.shadowRadius = 10.0;
//            button.layer.shadowOpacity = 1.0;
//            //button.layer.shadowOffset = .zero;
            
            NSShadow* dropShadow = [[NSShadow alloc] init];
            [dropShadow setShadowColor:[NSColor redColor]];
            [dropShadow setShadowOffset:NSMakeSize(0, 0)];
            [dropShadow setShadowBlurRadius:10.0];
            [button setShadow: dropShadow];
        }
#endif
        
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

    NSStackView* stackView = [NSStackView stackViewWithViews:buttons];
    stackView.orientation = NSUserInterfaceLayoutOrientationVertical;
    stackView.detachesHiddenViews =
        YES;  // default, but why have to have _buttonArrary
    [self addSubview:stackView];

    // Want menus, so user can define their own shortcuts to commands
    // Also need to enable/disable this via validateUserInterfaceItem
    NSApplication* app = [NSApplication sharedApplication];

    NSMenu* mainMenu = app.mainMenu;
    NSMenuItem* viewMenuItem = mainMenu.itemArray[2];
    _viewMenu = viewMenuItem.submenu;

    // TODO: add a view menu in the storyboard
    // NSMenu* menu = app.windowsMenu;
    //[menu addItem:[NSMenuItem separatorItem]];

    for (int32_t i = 0; i < numActions; ++i) {
        Action& action = actions[i];
        const char* icon = action.icon;  // single char
        const char* title = action.tip;

        NSString* toolTip = [NSString stringWithUTF8String:icon];
        NSString* name = [NSString stringWithUTF8String:title];
        bool isSeparator = icon[0] == 0;
        
        if (isSeparator) {
            [_viewMenu addItem:[NSMenuItem separatorItem]];
        }
        else {
            // NSString *shortcut = @"";  // for now, or AppKit turns key int cmd+shift+key
            NSString* shortcut = [NSString stringWithUTF8String:icon];
            
            NSMenuItem* menuItem =
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
        const char* icon = action.icon;  // single char
        
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
    _actionPrevCounterpart->disableButton();
    _actionCounterpart->disableButton();
    
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
    NSTextField* label = [[MyNSTextField alloc]
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
    Renderer* renderer = (Renderer *)self.delegate;
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
    pixel.x = std::clamp(pixel.x, -0.5f * ar, maxX);
    pixel.y = std::clamp(pixel.y, minY, 0.5f);

    // now that's the point that we want to zoom towards
    // No checks on this zoom
    // old - newPosition from the zoom

    // normalized coords to pixel coords
    pixel.x *= _showSettings->imageBoundsX;
    pixel.y *= _showSettings->imageBoundsY;
    
    // this fixes pinch-zoom on cube which are 6:1
    pixel.x /= ar;
    
#if USE_PERSPECTIVE
    // TODO: this doesn't work for perspective
    newPan.x = _showSettings->panX - (_showSettings->zoom - newZoom) * pixel.x;
    newPan.y = _showSettings->panY + (_showSettings->zoom - newZoom) * pixel.y;
#else
    newPan.x = _showSettings->panX - (_showSettings->zoom - newZoom) * pixel.x;
    newPan.y = _showSettings->panY + (_showSettings->zoom - newZoom) * pixel.y;
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

    Renderer* renderer = (Renderer *)self.delegate;
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
    Renderer* renderer = (Renderer *)self.delegate;

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

    bool isNormal = _showSettings->texContentType == TexContentTypeNormal;
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

enum TextSlot
{
    kTextSlotHud,
    kTextSlotEyedropper
};

- (void)setEyedropperText:(const char *)text
{
    _textSlots[kTextSlotEyedropper] = text;

    [self updateHudText];
}

- (void)setHudText:(const char *)text
{
    _textSlots[kTextSlotHud] = text;

    [self updateHudText];
}

- (void)updateHudText
{
    // combine textSlots
    string text = _textSlots[kTextSlotHud];
    if (!text.empty() && text.back() != '\n')
        text += "\n";
        
    // don't show eyedropper text with table up, it's many lines and overlaps
    if (!_tableView.hidden)
        text += _textSlots[kTextSlotEyedropper];

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
    
    // From ImGui notes:
    // From macOS 12.1, scrolling with two fingers and then decelerating
    // by tapping two fingers results in two events appearing.
    if (event.phase == NSEventPhaseCancelled)
        return;
    
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
            wheelY = std::clamp(wheelY, -0.1, 0.1);
            
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
    Renderer* renderer = (Renderer *)self.delegate;
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

    bool isJumpToNextHidden = _files.size() <= 1;
    
    bool isJumpToCounterpartHidden = true;
    bool isJumpToPrevCounterpartHidden = true;
    
    if ( _files.size() > 1) {
        isJumpToCounterpartHidden = ![self hasCounterpart:YES];
        isJumpToPrevCounterpartHidden  = ![self hasCounterpart:NO];
    }
    
    bool isRedHidden = _showSettings->numChannels == 0; // models don't show rgba
    bool isGreenHidden = _showSettings->numChannels <= 1;
    bool isBlueHidden = _showSettings->numChannels <= 2 &&
                        _showSettings->texContentType != TexContentTypeNormal;  // reconstruct z = b on normals

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
    bool isPlayHidden = !_showSettings->isModel; // only for models
    bool isDiffHidden = _showSettings->isModel; // only for images
    
    _actionPlay->setHidden(isPlayHidden);
    _actionArray->setHidden(isArrayHidden);
    _actionFace->setHidden(isFaceSliceHidden);
    _actionMip->setHidden(isMipHidden);
    _actionShowAll->setHidden(isShowAllHidden);
    
    _actionDiff->setHidden(isDiffHidden);
    _actionItem->setHidden(isJumpToNextHidden);
    _actionPrevItem->setHidden(isJumpToNextHidden);
    
    _actionCounterpart->setHidden(isJumpToCounterpartHidden);
    _actionPrevCounterpart->setHidden(isJumpToPrevCounterpartHidden);
    
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
    auto premulState = toState(_showSettings->doShaderPremul);
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
    auto diffState = toState(_showSettings->isDiff);
    
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
    
    _actionCounterpart->setHighlight(Off);
    _actionPrevCounterpart->setHighlight(Off);
    
    _actionHideUI->setHighlight(uiState); // note below button always off, menu has state
    
    _actionR->setHighlight(redState);
    _actionG->setHighlight(greenState);
    _actionB->setHighlight(blueState);
    _actionA->setHighlight(alphaState);
    
    _actionShowAll->setHighlight(showAllState);
    _actionPreview->setHighlight(previewState);
    _actionDiff->setHighlight(diffState);
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
    NSEvent* theEvent = [NSApp currentEvent];
    bool isShiftKeyDown = (theEvent.modifierFlags & NSEventModifierFlagShift);

    const Action* action = nullptr;
    if ([sender isKindOfClass:[NSButton class]]) {
        NSButton* button = (NSButton *)sender;
        for (const auto& search: _actions) {
            if (search.button == button) {
                action = &search;
                break;
            }
        }
    }
    else if ([sender isKindOfClass:[NSMenuItem class]]) {
        NSMenuItem* menuItem = (NSMenuItem *)sender;
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

- (void)flagsChanged:(NSEvent *)theEvent
{
    _modifierFlags = theEvent.modifierFlags;
}

- (void)keyDown:(NSEvent *)theEvent
{
    bool isShiftKeyDown = theEvent.modifierFlags & NSEventModifierFlagShift;
    uint32_t keyCode = theEvent.keyCode;

    // for now hit esc to hide the table views
    if (keyCode == Key::Escape) {
        [self hideFileTable];
        
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

- (void)hideFileTable
{
    // fix broken NSTableView, keeps showing scroll and responding to pan
    // so set scroll to hidden instead of tables
    NSScrollView* scrollView = [_tableView enclosingScrollView];
    scrollView.hidden = YES;
}

- (void)showFileTable
{
    // fix broken NSTableView, keeps showing scroll and responding to pan
    // so set scroll to hidden instead of tables
    NSScrollView* scrollView = [_tableView enclosingScrollView];
    scrollView.hidden = NO;
}

- (void)updateHudVisibility
{
    _hudLabel.hidden = _hudHidden || !_showSettings->isHudShown;
    _hudLabel2.hidden = _hudHidden || !_showSettings->isHudShown;
}

- (void)setLoadedText:(string&)text
{
    text = "Loaded ";

    string filename = _showSettings->lastFilename;
    text += toFilenameShort(filename.c_str());

    // archives and file systems have folders, split that off
    string folderName;
    const char* slashPos = strrchr(filename.c_str(), '/');
    if (slashPos != nullptr) {
        folderName = filename.substr(0, slashPos - filename.c_str());
    }

    if (!folderName.empty()) {
        text += " in folder ";
        text += folderName;
    }

    if (!_archiveName.empty()) {
        text += " from archive ";
        text += _archiveName;
    }
}

- (bool)handleEventAction:(const Action*)action isShiftKeyDown:(bool)isShiftKeyDown
{
    // Some data depends on the texture data (isSigned, isNormal, ..)
    bool isChanged = false;
    bool isStateChanged = false;

    // TODO: fix isChanged to only be set when value changes
    // f.e. clamped values don't need to re-render
    string text;

    Renderer* renderer = (Renderer*)self.delegate;
    
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
           
            renderer.playAnimations = !renderer.playAnimations;
            
            text = renderer.playAnimations ? "Play" : "Pause";
            isChanged = true;
            
            //[renderer updateAnimationState:self];
        }
        else {
            //[renderer updateAnimationState:self];
        }
    }
    else if (action == _actionShapeUVPreview) {
        
        // toggle state
        _showSettings->isUVPreview = !_showSettings->isUVPreview;
        text = _showSettings->isUVPreview ? "Show UVPreview" : "Hide UvPreview";
        isChanged = true;
        
        _showSettings->uvPreviewFrames = 10;
        
        // also need to call this in display link, for when it reaches end
        //[renderer updateAnimationState:self];
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
            "Mip, Face, Y-array\n"
            "↓-next item, →-next counterpart\n"
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
        //bool success =
            [self loadFile];

        // reload at actual size
        if (isShiftKeyDown) {
            _showSettings->zoom = 1.0f;
        }

        // Name change if image
        if (_showSettings->isModel)
            text = "Reload Model\n";
        else
            text = "Reload Image\n";
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
    else if (action == _actionPreview) {
        _showSettings->isPreview = !_showSettings->isPreview;
        isChanged = true;
        text = "Preview ";
        text += _showSettings->isPreview ? "On" : "Off";
    }
    else if (action == _actionDiff) {
        _showSettings->isDiff = !_showSettings->isDiff;
        isChanged = true;
        text = "Diff ";
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

        // if block size is 1, then this shouldn't toggle
        _showSettings->isBlockGridShown = false;
        _showSettings->isAtlasGridShown = false;
        _showSettings->isPixelGridShown = false;

        advanceGrid(grid, isShiftKeyDown);

        static const uint32_t gridSizes[kNumGrids] = {
            0, 1, 4, 32, 64, 128, 256  // grid sizes
        };

        if (grid == 0) {
            sprintf(text, "Grid Off");
        }
        else if (grid == 1) {
            _showSettings->isPixelGridShown = true;

            sprintf(text, "Pixel Grid 1x1");
        }
        else if (grid == 2 && _showSettings->blockX > 1) {
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
            
            // also hide the file table, since this can be long
            [self hideFileTable];
            
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
            _showSettings->doShaderPremul = !_showSettings->doShaderPremul;
            isChanged = true;
            text = "Premul ";
            text += _showSettings->doShaderPremul ? "On" : "Off";
        }
    }

    else if (action == _actionItem || action == _actionPrevItem) {
        if (!action->isHidden) {
            // invert shift key for prev, since it's reversese
            if (action == _actionPrevItem) {
                isShiftKeyDown = !isShiftKeyDown;
            }
            
            if ([self advanceFile:!isShiftKeyDown]) {
                //_hudHidden = true;
                //[self updateHudVisibility];
                [self setEyedropperText:""];
                
                isChanged = true;
                
                [self setLoadedText:text];
            }
        }
    }

    else if (action == _actionCounterpart || action == _actionPrevCounterpart) {
        if (!action->isHidden) {
            // invert shift key for prev, since it's reversese
            if (action == _actionPrevCounterpart) {
                isShiftKeyDown = !isShiftKeyDown;
            }
            if (_files.size() > 1) {
                if ([self advanceCounterpart:!isShiftKeyDown]) {
                    _hudHidden = true;
                    [self updateHudVisibility];
                    [self setEyedropperText:""];
                    
                    isChanged = true;
                    
                    [self setLoadedText:text];
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
// need to register for NSURL instead of NSPasteboardTypeFileURL.  For example,
// in canReadObjectForClasses had to use NSURL.

// drag and drop support
- (NSDragOperation)draggingEntered:(id)sender
{
    if (([sender draggingSourceOperationMask] & NSDragOperationGeneric) ==
        NSDragOperationGeneric) {
        NSPasteboard* pasteboard = [sender draggingPasteboard];
        
        bool canReadPasteboardObjects =
            [pasteboard canReadObjectForClasses:@[ [NSURL class] ]
                                        options:pasteboardOptions];

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
    NSPasteboard* pasteboard = [sender draggingPasteboard];
    
    NSArray<NSURL*>* urls = [pasteboard readObjectsForClasses:@[[NSURL class]]
                                                      options: pasteboardOptions];
    int filesCount = [urls count];
    if (filesCount > 0) {
        if ([self loadTextureFromURLs:urls]) {
            [self setHudText:""];
            return YES;
        }
    }

    return NO;
}

// TODO: convert to using a C++ json lib like yyJson or simdJson
// Then can move into libkram, and embed in the ktx/ktx2 metadata.
- (BOOL)loadAtlasFile:(const char*)filename
{
    NSURL* url = [NSURL fileURLWithPath:[NSString stringWithUTF8String:filename]];
    NSData* assetData = [NSData dataWithContentsOfURL:url];
    
    NSError* error = nil;
    NSDictionary* rootObject = [NSJSONSerialization JSONObjectWithData:assetData options:NSJSONReadingFragmentsAllowed error:&error];
    
    if (error != nil) {
        // TODO: avoid NSLog
        NSLog(@"%@", error);
        return NO;
    }
    
    // Can use hover or a show all on these entries and names.
    // Draw names on screen using system text in the upper left corner if 1
    // if showing all, then show names across each mip level.  May want to
    // snap to pixels on each mip level so can see overlap.
    
    _showSettings->atlas.clear();
    
    // TODO: this ObjC parser is insanely slow, dump it
    // look into yyJson (dom) or sax parsers.
    
    
    // TODO: support multiple atlases in one file, but need to then
    // parse an store and apply to differently named textures.
    
    NSDictionary* atlasProps = rootObject;
    
    {
        const char* atlasName = [atlasProps[@"name"] UTF8String];
        
        NSNumber* widthProp = atlasProps[@"width"];
        NSNumber* heightProp = atlasProps[@"height"];
        if (!heightProp) heightProp = widthProp;
        
        int width = [widthProp intValue];
        int height = [heightProp intValue];
        
        int slice = [atlasProps[@"slice"] intValue];
        
        float uPad = 0.0f;
        float vPad = 0.0f;
        NSArray<NSNumber*>* pauvProp = atlasProps[@"paduv"];
        if (pauvProp)
        {
            uPad = [pauvProp[0] floatValue];
            vPad = [pauvProp[1] floatValue];
        }
        
        NSArray<NSNumber*>* padpxProp = atlasProps[@"padpx"];
        if (padpxProp)
        {
            uPad = [padpxProp[0] intValue];
            vPad = [padpxProp[1] intValue];
            
            uPad /= width;
            vPad /= height;
        }
        
        NSDictionary* regions = atlasProps[@"regions"];
        
        for (NSDictionary* regionProps in regions)
        {
            const char* name = [regionProps[@"name"] UTF8String];
            
            float x = 0.0f;
            float y = 0.0f;
            float w = 0.0f;
            float h = 0.0f;
            
            NSArray<NSNumber*>* rectuv = regionProps[@"ruv"];
            if (rectuv) {
                // Note: could convert pixel and mip0 size to uv.
                // normalized uv make these easier to draw across all mips
                x = [rectuv[0] floatValue];
                y = [rectuv[1] floatValue];
                w = [rectuv[2] floatValue];
                h = [rectuv[3] floatValue];
            }
            
            NSArray<NSNumber*>* rectpx = regionProps[@"rpx"];
            if (rectpx) {
                x = [rectpx[0] intValue];
                y = [rectpx[1] intValue];
                w = [rectpx[2] intValue];
                h = [rectpx[3] intValue];
                
                // normalize to uv using the width/height
                x /= width;
                y /= height;
                w /= width;
                h /= height;
            }
            
            // optional
            // optional uv padding - need two values for non-square
            // could be inherited by all elements to avoid redundancy
            // optional horizontal and vertical orient
            // optional slice for 2d arrays
            
            const char* verticalProp = "f"; // [regionProps[@"o"] UTF8String];
            bool isVertical = verticalProp && verticalProp[0] == 't';
            
            Atlas atlas = {name, x,y, w,h, uPad,vPad, isVertical, (uint32_t)slice};
            _showSettings->atlas.emplace_back(std::move(atlas));
        }
    }
    
    // TODO: also need to be able to bring in vector shapes
    // maybe from svg or files written out from figma or photoshop.
    // Can triangulate those, and use xatlas to pack those.
    
    return YES;
}

// opens archive
- (BOOL)openArchive:(const char *)zipFilename urlIndex:(int32_t)urlIndex
{
    // grow the array, ptrs so that existing mmaps aren't destroyed
    if (urlIndex >= _containers.size()) {
        _containers.resize(urlIndex + 1, nullptr);
    }
    
    if (_containers[urlIndex] == nullptr)
        _containers[urlIndex] = new FileContainer;
    
    FileContainer& container = *_containers[urlIndex];
    MmapHelper& zipMmap = container.zipMmap;
    ZipHelper& zip = container.zip;
    
    // close any previous zip
    zipMmap.close();
    
    // open the mmap again
    if (!zipMmap.open(zipFilename)) {
        return NO;
    }
    if (!zip.openForRead(zipMmap.data(), zipMmap.dataLength())) {
        return NO;
    }
    return YES;
}

// lists archive into _files
- (BOOL)listFilesInArchive:(int32_t)urlIndex
{
    FileContainer& container = *_containers[urlIndex];
    ZipHelper& zip = container.zip;
    
    // filter out unsupported extensions
    vector<string> extensions = {
        ".ktx", ".ktx2", ".png", ".dds" // textures
#if USE_GLTF
        // TODO: can't support these until have a loader from memory block
        // GLTFAsset requires a URL.
        //, ".glb", ".gltf" // models
#endif
#if USE_USD
        , ".usd", ".usda", ".usb"
#endif
    };
    
    container.zip.filterExtensions(extensions);
    
    // don't switch to empty archive
    if (zip.zipEntrys().empty()) {
        return NO;
    }
    
    for (const auto& entry: zip.zipEntrys()) {
        _files.push_back({string(entry.filename), urlIndex});
    }
    
    return YES;
}

// TODO: can simplify by storing counterpart id when file list is created
- (BOOL)hasCounterpart:(BOOL)increment {
    if (_files.size() <= 1) {
        return NO;
    }
    
    const File& file = _files[_fileIndex];
    string currentFilename = filenameNoExtension(toFilenameShort(file.name.c_str()));
   
    uint32_t nextFileIndex = _fileIndex;
    
    size_t numEntries = _files.size();
    if (increment)
        nextFileIndex++;
    else
        nextFileIndex += numEntries - 1;  // back 1
    
    nextFileIndex = nextFileIndex % numEntries;
    
    const File& nextFile = _files[nextFileIndex];
    string nextFilename = filenameNoExtension(toFilenameShort(nextFile.name.c_str()));
    
    // if short name matches (no ext) then it's a counterpart
    if (currentFilename != nextFilename)
       return NO;
    
    return YES;
}

- (BOOL)advanceCounterpart:(BOOL)increment
{
    if (_files.size() <= 1) {
        return NO;
    }
    
    // see if file has counterparts
    const File& file = _files[_fileIndex];
    string currentFilename = filenameNoExtension(toFilenameShort(file.name.c_str()));
    
    // TODO: this should cycle through only the counterparts
    uint32_t nextFileIndex = _fileIndex;
    
    size_t numEntries = _files.size();
    if (increment)
        nextFileIndex++;
    else
        nextFileIndex += numEntries - 1;  // back 1

    nextFileIndex = nextFileIndex % numEntries;
    
    const File& nextFile = _files[nextFileIndex];
    string nextFilename = filenameNoExtension(toFilenameShort(nextFile.name.c_str()));
    
    if (currentFilename != nextFilename)
        return NO;
    
    _fileIndex = nextFileIndex;
    
    // set selection
    [_tableView selectRowIndexes:[NSIndexSet indexSetWithIndex:_fileIndex] byExtendingSelection:NO];
    [_tableView scrollRowToVisible:_fileIndex];
    
    // want it to respond to arrow keys
    //[self.window makeFirstResponder: _tableView];
    
    // show the files table
    [self showFileTable];
    [self setEyedropperText:""];
    
    return [self loadFile];
}

- (BOOL)advanceFile:(BOOL)increment
{
    if (_files.empty()) {
        return NO;
    }
    
    size_t numEntries = _files.size();
    if (increment)
        _fileIndex++;
    else
        _fileIndex += numEntries - 1;  // back 1

    _fileIndex = _fileIndex % numEntries;

    // set selection
    [_tableView selectRowIndexes:[NSIndexSet indexSetWithIndex:_fileIndex] byExtendingSelection:NO];
    [_tableView scrollRowToVisible:_fileIndex];
    
    // want it to respond to arrow keys
    //[self.window makeFirstResponder: _tableView];
    
    // show the files table
    [self showFileTable];
    [self setEyedropperText:""];
    
    return [self loadFile];
}

- (BOOL)setImageFromSelection:(NSInteger)index {
    if (!_files.empty()) {
        if (_fileIndex != index) {
            _fileIndex = index;
            return [self loadFile];
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

- (BOOL)findFilename:(const string&)filename
{
    bool isFound = false;
    
    // linear search
    for (const auto& search : _files) {
        if (search.name == filename) {
            isFound = true;
            break;
        }
    }
    return isFound;
}

- (BOOL)isArchive
{
    NSURL* url = _urls[_files[_fileIndex].urlIndex];
    const char* filename = url.fileSystemRepresentation;
    return isSupportedArchiveFilename(filename);
}

- (BOOL)loadFile
{
    if ([self isArchive]) {
        return [self loadFileFromArchive];
    }

    // now lookup the filename and data at that entry
    const File& file = _files[_fileIndex];
    const char* filename = file.name.c_str();
   
    string fullFilename = filename;
    auto timestamp = FileHelper::modificationTimestamp(filename);
    
    bool isTextureChanged = _showSettings->isFileChanged(filename, timestamp);
    if (!isTextureChanged) {
        return YES;
    }
    
#if USE_GLTF || USE_USD
    bool isModel = isSupportedModelFilename(filename);
    if (isModel) {
        return [self loadModelFile:filename];
    }
#endif
    
    // have already filtered filenames out, so this should never get hit
    if (!isSupportedFilename(filename)) {
        return NO;
    }

    // Note: better to extract from filename instead of root of folder dropped
    // or just keep displaying full path of filename.
    
    _archiveName.clear();
    
    vector<string> possibleNormalFilenames;
    string normalFilename;
    bool hasNormal = false;

    TexContentType texContentType = findContentTypeFromFilename(filename);
    if (texContentType == TexContentTypeAlbedo) {
        findPossibleNormalMapFromAlbedoFilename(filename, possibleNormalFilenames);
     
       for (const auto& name: possibleNormalFilenames) {
            hasNormal = [self findFilename:name];
            
            if (hasNormal) {
                normalFilename = name;
                break;
            }
        }
    }
    
    // see if there is an atlas file too, and load the rectangles for preview
    // note sidecar atlas files are a pain to view with a sandbox, may want to
    // splice into ktx/ktx2 files, but no good metadata for png/dds.
    _showSettings->atlas.clear();
    
    string atlasFilename = filenameNoExtension(filename);
    bool hasAtlas = false;
    
    // replace -a, -d, with -atlas.json
    const char* dashPosStr = strrchr(atlasFilename.c_str(), '-');
    if (dashPosStr != nullptr) {
        atlasFilename = atlasFilename.substr(0, dashPosStr - atlasFilename.c_str());
    }
    atlasFilename += "-atlas.json";
    if ( [self findFilename:atlasFilename.c_str()]) {
        if ([self loadAtlasFile:atlasFilename.c_str()]) {
            hasAtlas = true;
        }
    }
    if (!hasAtlas) {
        atlasFilename.clear();
    }
    
    // if it's a compressed file, then set a diff target if a corresponding png
    // is found.  Eventually see if a src dds/ktx/ktx2 exists.  Want to stop
    // using png as source images.  Note png don't have custom mips, unless
    // flattened to one image.  So have to fabricate mips here.
    
    string diffFilename = filenameNoExtension(filename);
    bool hasDiff = false;
    
    diffFilename += ".png";
    if ( diffFilename != filename && [self findFilename:diffFilename.c_str()]) {
        // TODO: defer load until diff enabled
        //if ([self loadDiffFile:diffFilename.c_str()]) {
            hasDiff = true;
        //}
    }
    if (!hasDiff) {
        diffFilename.clear();
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
    
    // Release any loading model textures
    Renderer* renderer = (Renderer *)self.delegate;
    [renderer releaseAllPendingTextures];
    
    if (![renderer loadTextureFromImage:fullFilename.c_str()
                              timestamp:timestamp
                                  image:image
                            imageNormal:hasNormal ? &imageNormal : nullptr
                              isArchive:NO]) {
        return NO;
    }

    //-------------------------------

    string title = _showSettings->windowTitleString(filename);
    self.window.title = [NSString stringWithUTF8String:title.c_str()];

    // doesn't set imageURL or update the recent document menu

    // show the controls
    if (_noImageLoaded) {
        _buttonStack.hidden = NO;  // show controls
        _noImageLoaded = NO;
    }

    // show/hide button
    [self updateUIAfterLoad];
    
    self.needsDisplay = YES;
    return YES;
}

- (BOOL)loadFileFromArchive
{
    // now lookup the filename and data at that entry
    const File& file = _files[_fileIndex];
    FileContainer& container = *_containers[file.urlIndex];
    ZipHelper& zip = container.zip;
    
    const char* filename = file.name.c_str();
    const auto* entry = zip.zipEntry(filename);
    string fullFilename = entry->filename;
    double timestamp = (double)entry->modificationDate;

    bool isTextureChanged = _showSettings->isFileChanged(filename, timestamp);
    if (!isTextureChanged) {
        return YES;
    }
    
// TODO: don't have a version which loads gltf model from memory block
//    bool isModel = isSupportedModelFilename(filename);
//    if (isModel)
//        return [self loadModelFile:filename];
    
    //--------
    
    if (!isSupportedFilename(filename)) {
        return NO;
    }
    
    const uint8_t* imageData = nullptr;
    uint64_t imageDataLength = 0;

    // search for main file - can be albedo or normal
    if (!zip.extractRaw(filename, &imageData, imageDataLength)) {
        return NO;
    }

    const uint8_t* imageNormalData = nullptr;
    uint64_t imageNormalDataLength = 0;
    
    string normalFilename;
    bool hasNormal = false;
    vector<string> normalFilenames;
    
    TexContentType texContentType = findContentTypeFromFilename(filename);
    if (texContentType == TexContentTypeAlbedo) {
        findPossibleNormalMapFromAlbedoFilename(filename, normalFilenames);
     
        for (const auto& name: normalFilenames) {
            hasNormal = zip.extractRaw(name.c_str(), &imageNormalData,
                                        imageNormalDataLength);
            if (hasNormal) {
                normalFilename = name;
                break;
            }
        }
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

    Renderer* renderer = (Renderer *)self.delegate;
    [renderer releaseAllPendingTextures];
    
    if (![renderer loadTextureFromImage:fullFilename.c_str()
                              timestamp:(double)timestamp
                                  image:image
                            imageNormal:hasNormal ? &imageNormal : nullptr
                              isArchive:YES]) {
        return NO;
    }

    //---------------------------------
    
    NSURL* archiveURL = _urls[file.urlIndex];
    _archiveName = toFilenameShort(archiveURL.fileSystemRepresentation);
    
    string title = _showSettings->windowTitleString(filename);
    self.window.title = [NSString stringWithUTF8String:title.c_str()];

    // doesn't set imageURL or update the recent document menu

    // show the controls
    if (_noImageLoaded) {
        _buttonStack.hidden = NO;  // show controls
        _noImageLoaded = NO;
    }

    // show/hide button
    [self updateUIAfterLoad];
    
    self.needsDisplay = YES;
    return YES;
}

-(void)listArchivesInFolder:(NSURL*)url archiveFiles:(vector<File>&)archiveFiles skipSubdirs:(BOOL)skipSubdirs
{
    NSDirectoryEnumerationOptions options = NSDirectoryEnumerationSkipsHiddenFiles;
    if (skipSubdirs)
        options |= NSDirectoryEnumerationSkipsSubdirectoryDescendants;
    
    NSDirectoryEnumerator* directoryEnumerator =
    [[NSFileManager defaultManager]
     enumeratorAtURL:url
     includingPropertiesForKeys:[NSArray array]
     options:options
     errorHandler:  // nil
     ^BOOL(NSURL *urlArg, NSError *error) {
        macroUnusedVar(urlArg);
        macroUnusedVar(error);
        
        // handle error
        return NO;
    }];
    
    // only display models in folder if found, ignore the png/jpg files
    while (NSURL* fileOrDirectoryURL = [directoryEnumerator nextObject]) {
        const char* name = fileOrDirectoryURL.fileSystemRepresentation;
        
        bool isArchive = isSupportedArchiveFilename(name);
        if (isArchive)
        {
            archiveFiles.push_back({string(name),0});
        }
    }
}

-(void)listFilesInFolder:(NSURL*)url urlIndex:(int32_t)urlIndex skipSubdirs:(BOOL)skipSubdirs
{
    NSDirectoryEnumerationOptions options = NSDirectoryEnumerationSkipsHiddenFiles;
    if (skipSubdirs)
        options |= NSDirectoryEnumerationSkipsSubdirectoryDescendants;
    
    NSDirectoryEnumerator* directoryEnumerator =
    [[NSFileManager defaultManager]
     enumeratorAtURL:url
     includingPropertiesForKeys:[NSArray array]
     options:options
     errorHandler:  // nil
     ^BOOL(NSURL *urlArg, NSError *error) {
        macroUnusedVar(urlArg);
        macroUnusedVar(error);
        
        // handle error - don't change to folder if devoid of valid content
        return NO;
    }];
    
    while (NSURL* fileOrDirectoryURL = [directoryEnumerator nextObject]) {
        const char* name = fileOrDirectoryURL.fileSystemRepresentation;
        
        bool isValid = isSupportedFilename(name);
        
#if USE_GLTF || USE_USD
        // note: many gltf reference jpg which will load via GltfAsset, but
        // kram and kramv do not import jpg files.
        if (!isValid) {
            isValid = isSupportedModelFilename(name);
        }
#endif
        
        if (!isValid) {
            isValid = isSupportedJsonFilename(name);
        }
        if (isValid) {
            _files.push_back({name,urlIndex});
        }
    }
}

bool isSupportedJsonFilename(const char* filename)
{
    return endsWith(filename, "-atlas.json");
}

    
-(void)loadFilesFromUrls:(NSArray<NSURL*>*)urls
{
    // don't recurse down subdirs, if cmd key held during drop or recent menu item selection
    bool skipSubdirs = ( _modifierFlags & NSEventModifierFlagCommand ) != 0;
    
    // reverse logic, so have to hold cmd to see subfolders
    skipSubdirs = !skipSubdirs;
    
    // Using a member for archives, so limited to one archive in a drop
    // but that's probably okay for now.  Add a separate array of open
    // archives if want > 1.

    // copy the existing files list
    string existingFilename;
    if (_fileIndex < (int32_t)_files.size())
        existingFilename = _files[_fileIndex].name;
    
    // Fill this out again
    _files.clear();
    
    // clear pointers
    for (FileContainer* container: _containers)
        delete container;
    _containers.clear();
    
    // this will flatten the list
    int32_t urlIndex = 0;
    
    NSMutableArray<NSURL*>* urlsExtracted = [NSMutableArray new];
    
    for (NSURL* url in urls) {
        // These will flatten out to a list of files
        const char* filename = url.fileSystemRepresentation;
        
        if (isSupportedArchiveFilename(filename) &&
            [self openArchive:filename urlIndex:urlIndex] &&
            [self listFilesInArchive:urlIndex])
        {
            [urlsExtracted addObject:url];
            urlIndex++;
        }
        else if (url.hasDirectoryPath) {
            
            // this first loads only models, then textures if only those
            [self listFilesInFolder:url urlIndex:urlIndex skipSubdirs:skipSubdirs];
            
            // could skip if nothing added
            [urlsExtracted addObject:url];
            urlIndex++;
            
            // handle archives within folder
            vector<File> archiveFiles;
            [self listArchivesInFolder:url archiveFiles:archiveFiles skipSubdirs:skipSubdirs];
        
            for (const File& archiveFile: archiveFiles) {
                const char* archiveFilename = archiveFile.name.c_str();
                if ([self openArchive:archiveFilename urlIndex:urlIndex] &&
                    [self listFilesInArchive:urlIndex]) {
                    
                    NSURL* urlArchive = [NSURL fileURLWithPath:[NSString stringWithUTF8String:archiveFilename]];
                    [urlsExtracted addObject:urlArchive];
                    urlIndex++;
                }

            }
        }
        else if (isSupportedFilename(filename)
#if USE_GLTF
                 || isSupportedModelFilename(filename)
#endif
            ) {
            _files.push_back({filename, urlIndex});
            
            [urlsExtracted addObject:url];
            urlIndex++;
        }
        else if (isSupportedJsonFilename(filename)) {
            _files.push_back({filename, urlIndex});
            
            [urlsExtracted addObject:url];
            urlIndex++;
        }
    
    }
    
    // TODO: sort by urlIndex
#if USE_EASTL
    NAMESPACE_STL::quick_sort(_files.begin(), _files.end());
#else
    std::sort(_files.begin(), _files.end());
#endif
    
    // preserve old file selection

    _urls = urlsExtracted;
   
    // preserve filename before load, and restore that index, by finding
    // that name in refreshed folder list
    _fileIndex = 0;
    if (!existingFilename.empty()) {
        for (uint32_t i = 0; i < _files.size(); ++i) {
            if (_files[i].name == existingFilename) {
                _fileIndex = i;
                break;
            }
        }
    }
    
    // add the files into the file list
    [_tableViewController.items removeAllObjects];
    for (const auto& file: _files) {
        const char* filenameShort = toFilenameShort(file.name.c_str());
        [_tableViewController.items addObject: [NSString stringWithUTF8String: filenameShort]];
    }
    
    uint32_t savedFileIndex = _fileIndex;
    // This calls selectionDidChange which then sets _fileIndex = 0;
    [_tableView reloadData];
    _fileIndex = savedFileIndex;
    
    // Set the active file
    [_tableView selectRowIndexes:[NSIndexSet indexSetWithIndex:_fileIndex] byExtendingSelection:NO];
    [_tableView scrollRowToVisible:_fileIndex];
    
    [self hideFileTable];
    
    // add it to recent docs (only 10 slots)
    if (urls.count == 1) {
        NSDocumentController* dc =
        [NSDocumentController sharedDocumentController];
        [dc noteNewRecentDocumentURL:urls[0]];
    }
}


- (BOOL)loadTextureFromURLs:(NSArray<NSURL*>*)urls
{
    // turn back on the hud if was in a list view
    _hudHidden = false;
    [self updateHudVisibility];
    
    NSURL* url = urls[0];
    const char* filename = url.fileSystemRepresentation;
    bool isSingleFile = urls.count == 1;
    
    Renderer* renderer = (Renderer *)self.delegate;
    
    // Handle shader hotload
    if (isSingleFile && endsWithExtension(filename, ".metallib")) {
        if ([renderer hotloadShaders:filename]) {
            NSURL* metallibFileURL =
            [NSURL fileURLWithPath:[NSString stringWithUTF8String:filename]];
            
            // add to recent docs, so can reload quickly
            NSDocumentController* dc =
            [NSDocumentController sharedDocumentController];
            [dc noteNewRecentDocumentURL:metallibFileURL];
            
            return YES;
        }
        return NO;
    }
    
    // don't leave archive table open
    if (isSingleFile)
        [self hideFileTable];

    [self loadFilesFromUrls:urls];
    
    BOOL success = [self loadFile];
    return success;
}
   
-(BOOL)loadModelFile:(const char*)filename
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
    
    Renderer* renderer = (Renderer *)self.delegate;
    [renderer releaseAllPendingTextures];
    
    setErrorLogCapture(true);
    
    const char* filenameShort = toFilenameShort(filename);
    double timestamp = FileHelper::modificationTimestamp(filename);
    
    // This code only takes url, so construct one
    NSURL* fileURL =
        [NSURL fileURLWithPath:[NSString stringWithUTF8String:filename]];
    BOOL success = [renderer loadModel:fileURL];
    
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
    setErrorLogCapture(false);

    // was using subtitle, but that's macOS 11.0 feature.
    string title = "kramv - ";
    title += filenameShort;
    self.window.title = [NSString stringWithUTF8String:title.c_str()];
    
    // show the controls
    if (_noImageLoaded) {
        _buttonStack.hidden = NO;  // show controls
        _noImageLoaded = NO;
    }

    // store the filename
    _showSettings->lastFilename = filename;
    _showSettings->lastTimestamp = timestamp;
    
    self.needsDisplay = YES;

    return success;
#else
    return NO;
#endif
}

/* Don't need this anymore
 
-(BOOL)loadImageFile:(NSURL*)url
{
    Renderer* renderer = (Renderer *)self.delegate;
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

    // this is already handled by drop
    // add to recent document menu
    //NSDocumentController* dc = [NSDocumentController sharedDocumentController];
    //[dc noteNewRecentDocumentURL:url];

    //self.imageURL = url;

    // show the controls
    if (_noImageLoaded) {
        _buttonStack.hidden = NO;  // show controls
        _noImageLoaded = NO;
    }

    // show/hide button
    [self updateUIAfterLoad];
    // no need for file table on single files
    [self hideFileTable];
    
    self.needsDisplay = YES;
    return YES;
}
*/

- (void)setupUI
{
    [self hideFileTable];
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
    MyMTKView* _view;

    Renderer* _renderer;

    NSTrackingArea* _trackingArea;
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
