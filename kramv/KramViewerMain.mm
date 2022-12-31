// kram - Copyright 2020-2022 by Alec Miller. - MIT License
// The license and copyright notice shall be included
// in all copies or substantial portions of the Software.

// using -fmodules and -fcxx-modules
//@import Cocoa;
//@import Metal;
//@import MetalKit;
//@import CoreText;

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


#include <mutex> // for recursive_mutex

using mymutex = std::recursive_mutex;
using mylock = std::unique_lock<mymutex>;

#include <UniformTypeIdentifiers/UTType.h>

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

//-------------


void Action::setHighlight(bool enable) {
    isHighlighted = enable;
    
    auto On = 1; // NSControlStateValueOn;
    auto Off = 0; // NSControlStateValueOff;
    
    if (!isButtonDisabled) {
        ((__bridge NSButton*)button).state = enable ? On : Off;
    }
    ((__bridge NSMenuItem*)menuItem).state = enable ? On : Off;
}

void Action::setHidden(bool enable) {
    isHidden = enable;
    
    if (!isButtonDisabled) {
        ((__bridge NSButton*)button).hidden = enable;
    }
    ((__bridge NSMenuItem*)menuItem).hidden = enable;
}

void Action::disableButton() {
    ((__bridge NSButton*)button).hidden = true;
    isButtonDisabled = true;
}


// These are using NSFileManager to list files, so must be ObjC
void Data::listArchivesInFolder(const string& folderFilename, vector<File>& archiveFiles, bool skipSubdirs)
{
    NSURL* url = [NSURL fileURLWithPath:[NSString stringWithUTF8String:folderFilename.c_str()]];
        
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
        return false;
    }];
    
    // only display models in folder if found, ignore the png/jpg files
    while (NSURL* fileOrDirectoryURL = [directoryEnumerator nextObject]) {
        const char* name = fileOrDirectoryURL.fileSystemRepresentation;
        
        bool isArchive = isSupportedArchiveFilename(name);
        if (isArchive)
        {
            archiveFiles.emplace_back(File(name,0));
        }
    }
}

void Data::listFilesInFolder(const string& archiveFilename, int32_t urlIndex, bool skipSubdirs)
{
    // Hope this hsas same permissions
    NSURL* url = [NSURL fileURLWithPath:[NSString stringWithUTF8String:archiveFilename.c_str()]];
    
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
        return false;
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
            _files.emplace_back(File(name,urlIndex));
        }
    }
}

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
@property (nonatomic, strong) NSMutableArray<NSAttributedString*>* items;
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
    cell.textField.attributedStringValue = [self.items objectAtIndex:row];
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






//----------------------------------------------------


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
    
    // copy of modifier flags, can tie drop actions to this
    NSEventModifierFlags _modifierFlags;
    
    ShowSettings* _showSettings;
    Data _data;
}



- (void)awakeFromNib
{
    [super awakeFromNib];

    // vertical offset of table down so hud can display info
    NSScrollView* scrollView = [_tableView enclosingScrollView];
    CGRect rect = scrollView.frame;
    rect.origin.y += 50;
    scrollView.frame = rect;
    
    // C++ delegate
    _data._delegate.view = (__bridge void*)self;

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

    _showSettings = _data._showSettings;

    self.clearColor = MTLClearColorMake(0.005f, 0.005f, 0.005f, 0.0f);

    self.clearDepth = _showSettings->isReverseZ ? 0.0f : 1.0f;

    // only re-render when changes are made
    // Note: this breaks ability to gpu capture, since display link not running.
    // so disable this if want to do captures.  Or just move the cursor to capture.
//#ifndef NDEBUG  // KRAM_RELEASE
    self.enableSetNeedsDisplay = YES;
//#endif
    // openFile in appDelegate handles "Open in..."

   

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
    _showSettings->isHideUI = true;
    _buttonStack.hidden = YES;
    
    _hudLabel2 = [self _addHud:YES];
    _hudLabel = [self _addHud:NO];
    [self setHudText:""];
    
    return self;
}

- (nonnull ShowSettings *)showSettings
{
    return _showSettings;
}

- (nonnull kram::Data *)data
{
    return &_data;
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
    _data.initActions();
    
    NSRect rect = NSMakeRect(0, 10, 30, 30);

    vector<Action>& actions = _data.actions();
    int32_t numActions = actions.size();
    
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
        
        // stackView seems to disperse the items evenly across the area, so this
        // doesn't work
        bool isSeparator = icon[0] == 0;
        
        if (isSeparator) {
            // rect.origin.y += 11;
            button.enabled = NO;
        }
        else {
            action.button = (__bridge void*)button;
            
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
            
            action.menuItem = (__bridge void*)menuItem;
        }
    }

    [_viewMenu addItem:[NSMenuItem separatorItem]];

    //----------------------
    
    // don't want some buttons showing up, menu only
    _data.initDisabledButtons();
    
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

    float4x4 newMatrix = _data.computeImageTransform(_showSettings->panX,
                                                    _showSettings->panY,
                                                    zoom);

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
        _data.doZoomMath(zoom, newPan);

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
        //self.needsDisplay = YES;
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

    NSPoint point = [event locationInWindow];
    
    // This flips so upper left corner is 0,0, vs. bottom left
    point = [self convertPoint:point fromView:nil];

    // this needs to change if view is resized, but will likely receive mouseMoved
    // events
    _showSettings->cursorX = (int32_t)point.x;
    _showSettings->cursorY = (int32_t)point.y;

    // should really do this in draw call, since moved message come in so quickly
    // TODO: can this mark hud as needsDisplay, and then handle in update
    [self updateEyedropper];
}




-(void)updateEyedropper
{
    _data.updateEyedropper();
    
    // This calls setNeedsDisplay on the hud section that displays the eyeDropper
    [self updateHudText];
    
    // TODO: remove this, but only way to get drawSamples to execute right now,
    // but then entire texture re-renders and that's not power efficient.
   self.needsDisplay = YES;
}

- (void)setEyedropperText:(const char *)text
{
    _data.setEyedropperText(text);
    [self updateHudText];
}

- (void)setHudText:(const char *)text
{
    _data.setTextSlot(kTextSlotHud, text);
    [self updateHudText];
}

- (void)updateHudText
{
    // combine textSlots
    string text = _data.textFromSlots();

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

// TODO: movef to data, but eliminate CGRect usage
- (void)updatePan:(float)panX panY:(float)panY
{
    //Renderer* renderer = (Renderer *)self.delegate;
    float4x4 projectionViewModelMatrix =
        _data.computeImageTransform(panX,
                                   panY,
                                   _showSettings->zoom);

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
        //self.needsDisplay = YES;
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




- (IBAction)handleAction:(id)sender
{
    NSEvent* theEvent = [NSApp currentEvent];
    bool isShiftKeyDown = (theEvent.modifierFlags & NSEventModifierFlagShift);

    void* senderPtr = (__bridge void*)sender;
    const Action* action = nullptr;
    if ([sender isKindOfClass:[NSButton class]]) {
        action = _data.actionFromButton(senderPtr);
    }
    else if ([sender isKindOfClass:[NSMenuItem class]]) {
        action = _data.actionFromMenu(senderPtr);
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
    
    const Action* action = _data.actionFromKey(keyCode);
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


- (bool)handleEventAction:(const Action*)action isShiftKeyDown:(bool)isShiftKeyDown
{
    ActionState actionState;
    if (!_data.handleEventAction(action, isShiftKeyDown, actionState))
        return false;
        
    // Do the leftover action work to call ObjC
    if (action == _data._actionVertical) {
        _buttonStack.orientation = _showSettings->isVerticalUI
            ? NSUserInterfaceLayoutOrientationVertical
            : NSUserInterfaceLayoutOrientationHorizontal;
    }
    else if (action == _data._actionHideUI) {
        _buttonStack.hidden = _showSettings->isHideUI;
    }
    else if (action == _data._actionHud) {
        [self updateHudVisibility];
    }
    else if (action == _data._actionInfo) {
        if (_showSettings->isHudShown) {
            
            // also hide the file table, since this can be long
            [self hideFileTable];
        }
    }
    else if (action == _data._actionPlay) {
        if (!action->isHidden) {
            Renderer* renderer = (Renderer*)self.delegate;
            renderer.playAnimations = _showSettings->isPlayAnimations;
        }
    }
            
    if (!actionState.hudText.empty()) {
        [self setHudText:actionState.hudText.c_str()];
    }

    if (actionState.isChanged || actionState.isStateChanged) {
        _data.updateUIControlState();
    }

    if (actionState.isChanged) {
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





- (void)updateFileSelection
{
    // set selection
    uint32_t fileIndex = _data._fileIndex;
    [_tableView selectRowIndexes:[NSIndexSet indexSetWithIndex:fileIndex] byExtendingSelection:NO];
    [_tableView scrollRowToVisible:fileIndex];
}

- (BOOL)setImageFromSelection:(NSInteger)index {
    if (!_data._files.empty()) {
        if (_data._fileIndex != index) {
            _data._fileIndex = index;
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




-(BOOL)loadFile
{
    // lookup the filename and data at that entry
    const File& file = _data._files[_data._fileIndex];
    const char* filename = file.nameShort.c_str();
    
    setErrorLogCapture( true );
    
    bool success = _data.loadFile();
    if (!success) {
        string errorText;
        getErrorLogCaptureText(errorText);
        setErrorLogCapture(false);
        
        string finalErrorText;
        append_sprintf(finalErrorText, "Could not load from file:\n %s\n",
                       filename);
        finalErrorText += errorText;
        
        [self setHudText:finalErrorText.c_str()];
        return NO;
    }
    setErrorLogCapture( false );
    
    // -------------
    string title = _showSettings->windowTitleString(filename);
    self.window.title = [NSString stringWithUTF8String:title.c_str()];
    
    // doesn't set imageURL or update the recent document menu
    
    // show the controls
    if (!_data._noImageLoaded) {
        _showSettings->isHideUI = false;
        _buttonStack.hidden = NO;  // show controls
        _data._noImageLoaded = false;
    }
    
    // show/hide button
    _data.updateUIAfterLoad();
    
    self.needsDisplay = YES;
    return YES;
}



-(void)loadFilesFromUrls:(NSArray<NSURL*>*)urls skipSubdirs:(BOOL)skipSubdirs
{
    // convert urls to vector<string> for C++
    vector<string> urlStrings;
    for (NSURL* url in urls) {
        urlStrings.push_back(url.fileSystemRepresentation);
    }
    
    // C++ to build list
    _data.loadFilesFromUrls(urlStrings, skipSubdirs);
    
    //-------------------
    
    NSMutableDictionary* attribsOff = [NSMutableDictionary dictionaryWithObjectsAndKeys:
        //[NSFont systemFontOfSize:64.0],NSFontAttributeName,
        [NSColor whiteColor],NSForegroundColorAttributeName,
        [NSNumber numberWithFloat:-2.0],NSStrokeWidthAttributeName,
        [NSColor blackColor],NSStrokeColorAttributeName,
        nil];
    
    // add the files into the file list
    [_tableViewController.items removeAllObjects];
    for (const auto& file: _data._files) {
        const char* filenameShort = file.nameShort.c_str();
        
        NSString* fileMenuText = [NSString stringWithUTF8String: filenameShort];
        NSMutableAttributedString* fileMenuStr = [[NSMutableAttributedString alloc] initWithString:fileMenuText attributes:attribsOff];
        
        [_tableViewController.items addObject:fileMenuStr];
    }
    
    // reloadData calls selectionDidChange which then sets _fileIndex = 0;
    uint32_t fileIndex = _data._fileIndex;
    [_tableView reloadData];
    _data._fileIndex = fileIndex;
    
    [self updateFileSelection];
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

    // only recurse down subdirs if cmd key held during drop or recent menu item selection
    bool skipSubdirs = ( _modifierFlags & NSEventModifierFlagCommand ) == 0;
    
    [self loadFilesFromUrls:urls skipSubdirs:skipSubdirs];
    
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
    
    // const char* filenameShort = toFilenameShort(filename);
    //double timestamp = FileHelper::modificationTimestamp(filename);
    
    // TODO: this used to compare filename timestamp?
    
    // This code only takes url, so construct one
    Renderer* renderer = (Renderer *)self.delegate;
    [renderer releaseAllPendingTextures];
    BOOL success = [renderer loadModel:filename];
    
    // TODO: split this off to a completion handler, since loadModel is async
    // and should probably also have a cancellation (or counter)
    
    // show/hide button
    _data.updateUIAfterLoad();
    
    if (!success) {
        return NO;
    }
    
    return success;
#else
    return NO;
#endif
}

- (void)setupUI
{
    [self hideFileTable];
}

- (void)concludeDragOperation:(id)sender
{
    // did setNeedsDisplay, but already doing that in loadTextureFromURL
}

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
                                              settings:_view.showSettings
                                              data:_view.data];

    
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

    // ObjC++ delegate
    _view.delegate = _renderer;
}



@end

bool DataDelegate::loadFile(bool clear)
{
    MyMTKView* view_ = (__bridge MyMTKView*)view;
    
    if (clear) {
        // set selection
        [view_ updateFileSelection];
        
        // want it to respond to arrow keys
        //[self.window makeFirstResponder: _tableView];
        
        // show the files table
        [view_ showFileTable];
        [view_ setEyedropperText:""];
    }
    
    return [view_ loadFile];
}

bool DataDelegate::loadModelFile(const char* filename)
{
    MyMTKView* view_ = (__bridge MyMTKView*)view;
    return [view_ loadModelFile:filename];
}

bool DataDelegate::loadTextureFromImage(const char* fullFilename, double timestamp, KTXImage& image, KTXImage* imageNormal, bool isArchive)
{
    MyMTKView* view_ = (__bridge MyMTKView*)view;
    Renderer* renderer = (Renderer *)view_.delegate;
    [renderer releaseAllPendingTextures];
    
    if (![renderer loadTextureFromImage:fullFilename
                              timestamp:timestamp
                                  image:image
                            imageNormal:imageNormal
                              isArchive:isArchive]) {
        return false;
    }
    
    return true;
}


//-------------

int main(int argc, const char *argv[])
{
    @autoreleasepool {
        // Setup code that might create autoreleased objects goes here.
    }
    return NSApplicationMain(argc, argv);
}
