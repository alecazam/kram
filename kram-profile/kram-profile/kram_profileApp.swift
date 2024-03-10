// kram - Copyright 2020-2024 by Alec Miller. - MIT License
// The license and copyright notice shall be included
// in all copies or substantial portions of the Software.

import SwiftUI
import WebKit
import UniformTypeIdentifiers

// https://github.com/gualtierofrigerio/WkWebViewJavascript/blob/master/WkWebViewJavascript/WebViewHandler.swift

// https://levelup.gitconnected.com/how-to-use-wkwebview-on-mac-with-swiftui-10266989ed11
// Signing & Capabilites set App Sanbox (allow outgoing connections)

// This is really just a wrapper to turn WKWebView into something SwiftUI
// can interop with.  SwiftUI has not browser widget.

// DONE: add bg list color depending on sort
// DONE: fn+F doesn't honor fullscreen
// DONE: Perfetto can only read .gz files, and not .zip files.
//   But could decode zip files here, and send over gz compressed.
//   Would need to idenfity zip archive > 1 file vs. zip single file.
// DONE: add gz compression to all file data.  Use libCompression
//   but it only has zlib compression.  Use DataCompression which
//   messages zlib deflate to gzip.
// DONE: if list hidden, then can't advance
// DONE: be nice to focus the search input on cmd+F just to make me happy.  (using cmd+S)
//  Browser goes to its own search which doesn’t help.

// Memory traces
// TODO: sort thread by size - repack the memory graph largest to smallest by reordering each track
//   then can focus on the bigger values.
// TODO: Sort by name and convert to count - can then see common counts
//   so keep the json loaded in Swift.  Can json be cloned and modded?
// TODO: option to colesce to count and name with sort

// TODO: across all files, many of the strings are the same.  Could replace all strings
// with an index, compress, and zip archive with the index table.  buid, perf, mem strings
// filenames are also redundant.  Just need to use @[F|S|B]hex, and then do lookup before CBA final report.
// Can rebuild references on JS side to send less data.  JS can then alias strings.

// DONE: sort files by range
// TODO: import zip, and run cba on contents, mmap and decompress each
//  can use incremental mode?
// TODO: can't mmap web link, but can load zip off server with timings
// TODO: add/update recent document list (need to hold onto dropped/opened folder)
// TODO: save/load the duration and modstamps for File.

// TODO: work on sending a more efficient form.  Could use Perfetto SDK to write to prototbuf.  The Catapult json format is overly verbose.  Need some thread and scope strings, some open/close timings that reference a scope string and thread.
// TODO: add compressed format, build up Pefetto json or binary from this
//  may need one for mmap, other for super compact deltas
//  can still alias strings from mmap
//
//  have various duration forms.
//  could have ascii form of below.
//  flags to identify optional param
// 4B magic
// n len nid name  // nid is repacked 0..table
// t len tid name  // tid is repacked to 0... table
// s len sid name  // symbols
// i tid tmin tmax count // have this written at end of file for each thread
// f fid nid line nid (file line func)
// r rid len sid sid sid sid
// s opt(tid nid fid color) dur opt(time), opt means uses prior tid/nid/color of that tid
//  (defaults if none).  May need to buffer per thread, top of buffer explicit
//  then merge with the other buffers, compare last tid data written.
// s = 1 + 3 + 3 + 8 + 8 + 4 = 29B
// smin = 1 + 8B
// need a way to tag file/line, and count into the dump

// timings can delta encoded, but with ts/dur a parent scope writes after.
// they aren't ordered by startTime.  Also missing any unclosed scoping.
// compared to -t +t sequences.  Note -0 = 0, so 0 is an open scope (check <=0 )

// 4-bit, 12-bit, 16-bit, variable, pad to 4B

// DONE: have a way to reload dropped folder
// TODO: track parent archives, folder, and loose drop files
// and when reload is hit, then reload all of that rebuild the list
// and then reload the selected file
// TODO: need mmapHelper and zipHelper to deal with archives
// also FileHelper, since may not be able to mmap.

// TODO: fix duration update modding the List item and nav title after it updates
// Currently select list, it updates, then duration is calcualated.
//   there is a objectWillChange.send() Could maybe send that from The File
// https://www.hackingwithswift.com/forums/swiftui/update-content-of-existing-row-of-list/3029

// WKWebView
// TODO: can't block drop onto the WKWebView
// TODO: can't overide "delete" key doing a back in the WKWebView history
//    Perfetto warns that content will be lost

// Perfetto Bugs
// TODO: fix the js wait, even with listener, there's still a race
//    maybe there's some ServiceWorker still loading the previous json?
//    Perfetto is using a ServiceWorker, Safari uses those now, and ping/pong unware.
// TODO: switch to Perfetto dark mode

// Multi-window
// TODO: support WindowGroup and multiwindow, each needs own webView, problem
//   is that onOpenURL opens a new window always.
// TODO: look in to hosting remotery web and/or tracy server, see if Remotery as flamechart render
//   but these don't review traces offline, and are live profilers
// TODO: add Metal capture and imgui backend to Tracy
// TODO: add Metal capture to Remotery


// TODO: switch font to Inter, bundle that with the app?
//   .environment(\.font, Font.custom("CustomFont", size: 14))
// TODO: for perf traces, compute duration between frame
//   markers.  Multiple frames in a file, then show max frame duration
//   instead of the entire file.
// TODO: no simple scrollTo, since this is all React style
//   There is a ScrollViewReader, but value only usable within.  UITableView has this.
// TODO: track when files change or get deleted, update the list item then
//   can disable list items that are deleted in case they return (can still pick if current)
//   https://developer.apple.com/documentation/coreservices/file_system_events?language=objc
// TODO: here's how to sign builds for GitHub Actions
// https://docs.github.com/en/actions/deployment/deploying-xcode-applications/installing-an-apple-certificate-on-macos-runners-for-xcode-development

// See here about Navigation API
// https://developer.apple.com/videos/play/wwdc2022/10054/

// This is how open_trace_in_ui.py tells the browser to open a file
// http://ui.perfetto.dev/#!/?url=http://127.0.0.1:9001/{fname}
// Then the http server serves up that file to the browser and sets Allow-Origin header.

// https://developer.mozilla.org/en-US/docs/Glossary/Base64#the_unicode_problem
//https://stackoverflow.com/questions/30106476/using-javascripts-atob-to-decode-base64-doesnt-properly-decode-utf-8-strings
// https://gist.github.com/chromy/170c11ce30d9084957d7f3aa065e89f8
// need to post this JavaScript

// https://stackoverflow.com/questions/32113933/how-do-i-pass-a-swift-object-to-javascript-wkwebview-swift

// https://stackoverflow.com/questions/37820666/how-can-i-send-data-from-swift-to-javascript-and-display-them-in-my-web-view

// Dealing with available and Swift and SwiftUI.  Ugh.
// https://www.swiftyplace.com/blog/swift-available#:~:text=Conditional%20Handling%20with%20if%20%23available&text=If%20the%20device%20is%20running%20an%20earlier%20version%20of%20iOS,a%20fallback%20for%20earlier%20versions.

// Video about Combine
// https://www.youtube.com/watch?v=TshpcKZmma8

// Description of CoreData/SwiftData and how it works
// https://davedelong.com/blog/2021/04/03/core-data-and-swiftui/
//
// List sort picker
// https://xavier7t.com/swiftui-list-with-sort-options
//
// https://stackoverflow.com/questions/70652964/how-to-search-a-table-using-swiftui-on-macos
//
// https://developer.apple.com/documentation/swiftui/adding-a-search-interface-to-your-app
// can filter list items off this

class FileSearcher: ObservableObject {
    @Published var searchIsActive = false
    @Published var searchText = ""
    
    var files: [File] = []
        
    @Published var filesSearched: [File] = []
    
    // duplicate code, but init() doesn't have self defined
    func updateFilesSearched(_ sortByDuration: Bool = false) {
        // may not want to sort everytime, or the list will change as duration is updated
        // really want to do this off a button, and then set files to that
        let sortedResults = files.sorted {
            if !sortByDuration {
                return $0.id < $1.id
            }
            else {
                // keep the groupings, just sort the duration within
                if $0.parentFolders != $1.parentFolders {
                    return $0.parentFolders < $1.parentFolders
                }
                if $0.duration == $1.duration {
                    return $0.id < $1.id
                }
                // TODO: may want to also search by last
                return $0.duration > $1.duration
            }
        }
        
        if searchText.isEmpty || sortedResults.count <= 1  {
            filesSearched = sortedResults
        }
        else if searchText.count == 1 {
            let lowercaseSearchText = searchText.lowercased()
            let uppercaseSearchText = searchText.uppercased()

            filesSearched = sortedResults.filter {
                $0.name.starts(with: uppercaseSearchText) ||
                $0.name.starts(with: lowercaseSearchText)
            }
        }
        else {
            // is search text multistring?
            $searchText.map { searchText in
                sortedResults.filter { file in
                    // Here use the name, or else the directory will have say "kram" in it and filter will trigger for all files "kr"
                    file.name.localizedCaseInsensitiveContains(searchText)
                }
            }
            .assign(to: &$filesSearched)
        }
    }
}

// https://stackoverflow.com/questions/24074479/how-to-create-a-string-with-format
extension String.StringInterpolation {

    /// Quick formatting for *floating point* values.
    mutating func appendInterpolation(float: Double, decimals: UInt = 2) {
        let floatDescription = String(format: "%.\(decimals)f%", float)
        appendLiteral(floatDescription)
    }

    /// Quick formatting for *hexadecimal* values.
    mutating func appendInterpolation(hex: Int) {
        let hexDescription = String(format: "0x%X", hex)
        appendLiteral(hexDescription)
    }

    /// Quick formatting for *percents*.
    mutating func appendInterpolation(percent: Double, decimals: UInt = 2) {
        let percentDescription = String(format: "%.\(decimals)f%%", percent * 100)
        appendLiteral(percentDescription)
    }

    /// Formats the *elapsed time* since the specified start time.
    mutating func appendInterpolation(timeSince startTime: TimeInterval, decimals: UInt = 2) {
        let elapsedTime = CACurrentMediaTime() - startTime
        let elapsedTimeDescription = String(format: "%.\(decimals)fs", elapsedTime)
        appendLiteral(elapsedTimeDescription)
    }
}

/* usage
 let number = 1.2345
 "Float: \(float: number)" // "Float: 1.23"
 "Float: \(float: number, decimals: 1)" // "Float: 1.2"

 let integer = 255
 "Hex: \(hex: integer)" // "Hex: 0xFF"

 let rate = 0.15
 "Percent: \(percent: rate)" // "Percent: 15.00%"
 "Percent: \(percent: rate, decimals: 0)" // "Percent: 15%"

 let startTime = CACurrentMediaTime()
 Thread.sleep(forTimeInterval: 2.8)
 "∆t was \(timeSince: startTime)" // "∆t was 2.80s"
 "∆t was \(timeSince: startTime, decimals: 0)" // "∆t was 3s"
 */

private let log = Log("kram-profile")

public func clamp<T>(_ value: T, _ minValue: T, _ maxValue: T) -> T where T : Comparable {
    return min(max(value, minValue), maxValue)
}

//-------------

enum ContainerType {
    case Archive // zip of 1+ files, can't enforce
    case Compressed // gzip of 1 file, can't enforce
    case Folder // from a folder drop
    case File // means file was dropped or opened directly
}

enum FileType {
    case Build
    case Memory
    case Perf
    case Unknown
}

class File: Identifiable, /*Hashable, */ Equatable, Comparable
{
    var id: String { url.absoluteString }
    var name: String { url.lastPathComponent }
    let url: URL
    let shortDirectory: String
    let parentFolders: String
    let containerType: ContainerType
    let fileType: FileType

    var duration = 0.0
    var modStamp: Date?
    var loadStamp: Date?
    
    // only available for memory file type right now
    var threadInfo = ""
    
    init(url: URL) {
        self.url = url
        self.modStamp = File.fileModificationDate(url:url)
        self.shortDirectory = File.buildShortDirectory(url:url)
        self.parentFolders = url.deletingLastPathComponent().absoluteString
        self.containerType = File.filenameToContainerType(url)
        self.fileType = File.filenameToFileType(url)
    }
    
    public static func == (lhs: File, rhs: File) -> Bool {
        return lhs.id == rhs.id
    }
    public static func < (lhs: File, rhs: File) -> Bool {
        return lhs.id < rhs.id
    }
    
    // call this when the file is loaded
    public func setLoadStamp()  {
        loadStamp = modStamp
    }
    public func isReloadNeeded() -> Bool {
        return modStamp != loadStamp
    }
    
    private static func fileModificationDate(url: URL) -> Date? {
        do {
            let attr = try FileManager.default.attributesOfItem(atPath: url.path)
            return attr[FileAttributeKey.modificationDate] as? Date
        } catch {
            return nil
        }
    }

    // show some of dir file is in, TODO: 2 levels not enough
    private static func buildShortDirectory(url: URL) -> String {
        let count = url.pathComponents.count
        
        // dir0/dir1/file.ext
        // -3/-2/-1
        
        var str = ""
        if count >= 3 {
            str += url.pathComponents[count-3]
            str += "/"
        }
        if count >= 2 {
            str += url.pathComponents[count-2]
        }
        
        return str
    }
    
    public static func filenameToContainerType(_ url: URL) -> ContainerType {
        let ext = url.pathExtension
        
        if ext == "zip" {
            return .Archive
        }
        if ext == "gz" { // could be a tarball archive, but don't support that
            return .Compressed
        }
        return .File
    }

    public static func filenameToFileType(_ url: URL) -> FileType {
        let ext = url.pathExtension
        
        if File.filenameToContainerType(url) != .File {
            // strip the .gz/.zip
            return filenameToFileType(url.deletingPathExtension())
        }
        
        if ext == "json" || ext == "buildtrace" { // build
            return .Build
        }
        else if ext == "memtrace" { // memory
            return .Memory
        }
        // TODO: eliminate trace
        else if ext == "trace" || ext == "perftrace" { // profile
            return .Perf
        }
        return .Unknown
    }
}

// TODO: now that it's a class, can probably elimiante that lookuFile calls
func generateDuration(file: File) -> String {
    // need for duration
    let f = lookupFile(url: file.url)
    if f.duration != 0.0 {
        // TODO: may want to add s/mb based on file type
        return String(format:"%0.3f", f.duration) // sec vis to ms for now
    }
    else {
        return ""
    }
}

func generateNavigationTitle(_ sel: String?) -> String {
    if sel == nil {
        return ""
    }
    
    let f = lookupFile(selection: sel!)
    return generateDuration(file: f) + " " + f.name
}

//-------------
// Note: if a file is deleted which happens often with builds,
// then want to identify that and update the list.  At least
// indicate the item is gone, and await its return.
// Does macOS have a FileWatcher?

// Holds supported files dropped or opened from Finder, reload reparses this
var droppedFileCache : [URL] = []

// Flattened list of supported files from folders and archives
var fileCache : [URL:File] = [:]

func lookupFile(url: URL) -> File {
    let file = File(url:url)
    
    // This preseves the duration previously parsed and stored
    
    if let fileOld = fileCache[file.url] {
        if file.modStamp == nil || // means file and/or dir went away, so return fileOld
            file.modStamp! == fileOld.modStamp! {
            return fileOld
        }
    }
    
    // This wipes the duration, so it can be recomputed
    fileCache[file.url] = file
    
    return file
}

func lookupFile(selection: String) -> File {
    return lookupFile(url:URL(string:selection)!)
}

// This one won't be one in the list, though
func updateFileCache(file: File) {
    fileCache[file.url] = file
}

//-------------

class MyWebView : WKWebView {
    
    // So that keyboard events are routed
    override var acceptsFirstResponder: Bool { true }
    
    // This is to prevent bonk
    override func performKeyEquivalent(with event: NSEvent) -> Bool {
        // unclear why all events are going to WebView
        // so have to return false to not have them filtered
        
        // allow all menu shortcuts to keep working
        if event.modifierFlags.contains(.command) {
            return false
        }
        
        // allow list to use up/down
        if event.keyCode == Keycode.upArrow ||
            event.keyCode == Keycode.downArrow {
            return false
        }
        
        // TODO: delete is still unloading the currently loaded page.  Augh!!!
        
        // true means it doesn't bonk, but WKWebView still gets to
        // respond to the keys.  Ugh.  Stupid system.
        // return true
        
        return super.performKeyEquivalent(with: event)
    }
    
    /* still not working
    override func performDragOperation(_ sender: NSDraggingInfo) -> Bool {
        //let myWebView = superview as! MyWebView
        //if let dropDelegate = myWebView.dropDelegate {
        //    return dropDelegate.webView(myWebView, performDragOperation: sender)
        //}
        return false
    }
    
    // really want to shim drop
    func shimDrag() {
        // https://stackoverflow.com/questions/25096910/receiving-nsdraggingdestination-messages-with-a-wkwebview
        
        // Override the performDragOperation: method implemented on WKView so that we may get drop notification.
        let originalMethod = class_getInstanceMethod(object_getClass(subviews[0]), #selector(NSDraggingDestination.performDragOperation(_:)))
        let overridingMethod = class_getInstanceMethod(object_getClass(self), #selector(NSDraggingDestination.performDragOperation(_:)))
            method_exchangeImplementations(originalMethod!, overridingMethod!)
    }
    */
}


func newWebView(request: URLRequest) -> WKWebView {
    // set preference to run javascript on the view, can then do PostMessage
    let preferences = WKPreferences()
    //preferences.javaScriptEnabled = true
    //preferences.allowGPUOptimizedContents = true
    
    let webpagePreferences = WKWebpagePreferences()
    webpagePreferences.allowsContentJavaScript = true
    
    let configuration = WKWebViewConfiguration()
    configuration.preferences = preferences
    configuration.defaultWebpagePreferences = webpagePreferences
    
    // here frame is entire screen
    let webView = MyWebView(frame: .zero, configuration: configuration)
    //webView.shimDrag()
    
    // The page is complaining it's going to lose the data.  This disables swipe fwd/back.
    // Still occuring because this doesn't disable the "delete" key which goes back in history
    webView.allowsBackForwardNavigationGestures = false
   
    webView.load(request)
    return webView
}

// This is just an adaptor to allow WkWebView to interop with SwiftUI.
// It's unclear if WindowGroup can even have this hold state.
struct WebView : NSViewRepresentable {
    //let request: URLRequest
    let webView: WKWebView
    
    // This is set by caller to the url for the request
    func makeNSView(context: Context) -> WKWebView {
        return webView
    }
    
    // can get data back from web view
    func userContentController(_ userContentController: WKUserContentController, didReceive message: WKScriptMessage) {
       if message.name == "postMessageListener" {
           // Manage your Data
       }
    }
    
    // This is called to refresh the view
    func updateNSView(_ webView: WKWebView, context: Context) {
        
    }

    // Here's sample code to do a screenshot, can this use actual dimensions
    // https://nshipster.com/wkwebview/
//    func webView(_ webView: WKWebView, didFinish navigation: WKNavigation!)
//    {
//        var snapshotConfiguration = WKSnapshotConfiguration()
//        snapshotConfiguration.snapshotWidth = 1440
//
//        webView.takeSnapshot(with: snapshotConfiguration) { (image, error) in
//            guard let image = image, error == nil else {
//                return
//            }
//
//            // TODO: save out the image
//        }
//    }
    
}

// TODO: fix the previewz
//#Preview {
//    MyWKWebView()
//}

//-------------

/*
class MyMTKView: MTKView {
    
    
    
}

// wraps MTKView (NSView) into SwiftUI, so it can be a part of the hierarcy,
// updateNSView called when app foreground/backgrounded, or the size is changed
// also look at Tracy server
struct MTKViewWrapper: NSViewRepresentable {
    var mtkView: MyMTKView
    
    // TODO: could hand this down without rebuilding wrapper, could be @Published from UserData
    //var currentPath: String
    
    func makeNSView(context: NSViewRepresentableContext<MTKViewWrapper>) -> MyMTKView {
        return mtkView
    }

    func updateNSView(_ view: MyMTKView, context: NSViewRepresentableContext<MTKViewWrapper>) {
        //view.updateUserData(currentPath: currentPath)
        
    }
}
*/

// https to work for some reason, but all data is previewed locally
var ORIGIN = "https://ui.perfetto.dev"

// https://gist.github.com/pwightman/64c57076b89c5d7f8e8c
extension String {
    var javaScriptEscapedString: String {
        // Because JSON is not a subset of JavaScript, the LINE_SEPARATOR and PARAGRAPH_SEPARATOR unicode
        // characters embedded in (valid) JSON will cause the webview's JavaScript parser to error. So we
        // must encode them first. See here: http://timelessrepo.com/json-isnt-a-javascript-subset
        // Also here: http://media.giphy.com/media/wloGlwOXKijy8/giphy.gif
        let str = self.replacingOccurrences(of: "\u{2028}", with: "\\u2028")
                      .replacingOccurrences(of: "\u{2029}", with: "\\u2029")
        // Because escaping JavaScript is a non-trivial task (https://github.com/johnezang/JSONKit/blob/master/JSONKit.m#L1423)
        // we proceed to hax instead:
        do {
            let encoder = JSONEncoder()
            let data = try encoder.encode([str])
            let encodedString = String(decoding: data, as: UTF8.self)
            
            // drop surrounding {}?
            return String(encodedString.dropLast().dropFirst())
        } catch {
            return self
        }
    }
}

    
extension View {
    public func searchableOptional<S>(text: Binding<String>, isPresented: Binding<Bool>, placement: SearchFieldPlacement = .automatic, prompt: S) -> some View where S : StringProtocol {
        if #available(macOS 14.0, *) {
            return self.searchable(text: text, isPresented: isPresented, placement:
                        placement, prompt: prompt)
        }
        else {
            return self
        }
    }
  
    /*
    // This one is hard to wrap, since KeyPress.result is macOS 14.0 only
    public func onKeyPressOptional(_ key: KeyEquivalent, action: @escaping () -> KeyPress.Result) -> some View {
        if #available(macOS 14.0, *) {
            return onKeyPress(.upArrow, action: action)
        }
        else {
            return self
        }
    }
    */
}

// What if the start time in the file isn't 0.0 based for the start
struct TimeRange {
    var timeStart: Double = 0.0
    var timeEnd: Double   = 1.0
        
    // The time range should take up 80% of the visible window.
    var viewPercentage: Double = 0.8
}



func filenameToTimeRange(_ filename: String) -> TimeRange {
    var duration = 1.0
    
    switch File.filenameToFileType(URL(string: filename)!) {
        case .Build: duration = 1.0
        case .Memory: duration = 64.0
        case .Perf: duration = 0.1 // 100ms
        case .Unknown: duration = 1.0
    }
    
    return TimeRange(timeStart:0.0, timeEnd:duration)
}

func buildTimeRangeJson(_ timeRange:TimeRange) -> String? {
    if timeRange.timeEnd == 0.0 {
        return nil
    }
    
    // This is in nanos
    let timeStartInt = Int(timeRange.timeStart * 1e9)
    let timeEndInt = Int(timeRange.timeEnd * 1e9)
    
    // Time is not found, it's in ui/src/base/time.ts
    // Sending down nanos seems to work provided the number has n suffix
    // TODO: Perfetto seems to only honor this the first time it's sent.
    
    // This one doesn't go thorugh JSON.parse()
    // timeStart: Time.fromSeconds(\(timeRange.timeStart)),
    // timeEnd: Time.fromSeconds(\(timeRange.timeEnd)),
    
    // The postMessage if using Json.stringify can't handle the BigInt
    let script = """
        var objTime = {
            perfetto:{
                timeStart:\(timeStartInt)n,
                timeEnd:\(timeEndInt)n,
                viewPercentage:\(timeRange.viewPercentage)
            }};
        """
    
    return script
}

// Flutter uses this to jump to a time range
func showTimeRangeJS(objTimeScript: String) -> String? {

   
    // https://github.com/flutter/devtools/blob/master/packages/devtools_app/lib/src/screens/performance/panes/timeline_events/perfetto/_perfetto_web.dart#L174
    

    /*
     
     // The |time| type represents trace time in the same units and domain as trace
     // processor (i.e. typically boot time in nanoseconds, but most of the UI should
     // be completely agnostic to this).
     export type time = Brand<bigint, 'time'>;
     
     https://github.com/google/perfetto/blob/45fe47bfe4111454ba7063b9b4d438369090d6ba/ui/src/common/actions.ts#L97
     export interface PostedScrollToRange {
       timeStart: time;
       timeEnd: time; // ugh?
       viewPercentage?: number;
     }

     // https://github.com/flutter/devtools/blob/8bf64b754a4677b66d22fe6f1212bd72d1e789b8/packages/devtools_app/lib/src/screens/performance/panes/flutter_frames/flutter_frame_model.dart#L29
    
     */
    
    let script = """
       
        // https://jsfiddle.net/vrsofx1p/
        function waitForUI(objTime)
        {
            const timer = setInterval(() => window.postMessage('PING', '\(ORIGIN)'), 50);
            
            const onMessageHandler = (evt) => {
                if (evt.data !== 'PONG') return;
                
                // We got a PONG, the UI is ready.
                window.clearInterval(timer);
                window.removeEventListener('message', onMessageHandler);
                
                // was win, but use window instead
                window.postMessage(objTime, '\(ORIGIN)');
            }
        
            window.addEventListener('message', onMessageHandler);
        }
            
        waitForUI(objTime);
    """
    
    return objTimeScript + script
}

struct CatapultEvent: Codable {
    var cat: String?
    var pid: Int?
    var tid: Int?
    var ph: String?
    var ts: Int?
    var dur: Int?
    var name: String?
    var args: [String : AnyCodable]?
    
    // var tts: Int? - thread clock timestamp
    // var cname: String? - color name from table
    // Also can have stack frames
}

struct CatapultProfile: Codable {
    var traceEvents: [CatapultEvent]?
    
    // not a part of the Catapult spec, but clang writes this when it zeros
    // out the startTime
    var beginningOfTime: Int?
}

class ThreadInfo : Hashable, Equatable, Comparable {
    
    var id: Int = 0
    var threadName: String = ""
    var startTime: Int = Int.max
    var endTime: Int = Int.min
    var endTimeFree: Int = Int.min
    var count: Int = 0
    
    // id doesn't implement Hashable
    func hash(into hasher: inout Hasher) {
        hasher.combine(id)
    }
    
    public static func == (lhs: ThreadInfo, rhs: ThreadInfo) -> Bool {
        return lhs.id == rhs.id
    }
    public static func < (lhs: ThreadInfo, rhs: ThreadInfo) -> Bool {
        return lhs.id < rhs.id
    }
    
    func combine(_ s: Int, _ d: Int, _ name: String?) {
        let isFreeBlock = name != nil && name! == "Free"
        let e = s+d
        
        if isFreeBlock {
            endTimeFree = max(endTimeFree, e)
            
            // If all free block, this doesn't work
            // so update start/endTime assuming first block isn't Free
            if startTime > endTime {
                startTime = min(startTime, s)
                endTime = max(endTime, e)
            }
        }
        else {
            startTime = min(startTime, s)
            endTime = max(endTime, e)
        }
        
        count += 1
    }
    
    var description: String {
        let duration = Double(endTime - startTime) * 1e-6
        
        // TODO: could display freeDuration (heap size)
        var freeDuration = duration
        if endTimeFree != Int.min {
            freeDuration = Double(endTimeFree - startTime) * 1e-6
        }
        let percentage = freeDuration > 0.0 ? ((duration / freeDuration) * 100.0) : 0.0
        
        // only display percentage if needed
        if percentage > 99.9 {
            return "\(id) '\(threadName)' \(float: duration, decimals:6)s \(count)x"
        }
        else {
            return "\(id) '\(threadName)' \(float: duration, decimals:6)s \(float:percentage, decimals:0)% \(count)x"
        }
    }
    
}

// TODO: Hook this up, build more efficient array of thread events
func sortThreadsByName(_ catapultProfile: inout CatapultProfile) {
    
    var threads: [Int: [Int]] = [:]
    
    // first sort each thread by
    for i in 0..<catapultProfile.traceEvents!.count {
        let event = catapultProfile.traceEvents![i]
        
        guard let tid = event.tid else { continue }
        if event.ts == nil || event.dur == nil { continue }
        
        if event.name != nil && (event.name! == "thread_name" || event.name! == "process_name") {
            continue
        }
        
        if threads[tid] == nil {
            threads[tid] = []
        }
        // just store the even index
        threads[tid]!.append(i)
    }
    
    for var thread in threads.values {
        // TODO: want to buble the top allocators by count or mem
        // to the front of the list.  Once have names sorted
        // then can group totals
        
        // sort each thread by name then dur
        thread = thread.sorted {
            let lval = catapultProfile.traceEvents![$0]
            let rval = catapultProfile.traceEvents![$1]
            
            let lname = lval.name ?? ""
            let rname = lval.name ?? ""
            
            if lname < rname {
                return true
            }
            return lval.dur! < rval.dur!
        }
        
        // rewrite the start of the events
        // Note this 0's them out, but could preserve min startTime
        var startTime = 0
        for i in thread {
            catapultProfile.traceEvents![i].ts = startTime
            startTime += catapultProfile.traceEvents![i].dur!
        }
        
        // combine nodes, and store a count into the name
        // easier to mke a new array, and replace the other
        //var combineIndex = 0
        // for i in 1..<catapultProfile.traceEvents![i]
        
    }
    
    // have option to consolidate and rename, but must remove nodes
}

// parse json trace
func updateThreadInfo(_ catapultProfile: CatapultProfile, _ file: inout File) {
    // was using Set<>, but having trouble with lookup
    var threadInfos: [Int: ThreadInfo] = [:]
    
    for i in 0..<catapultProfile.traceEvents!.count {
        let event = catapultProfile.traceEvents![i]
        
        // have to have tid to associate with ThreadInfo
        guard let tid = event.tid else { continue }
        
        if threadInfos[tid] == nil {
            let info = ThreadInfo()
            info.id = tid
            
            threadInfos[tid] = info
        }
        
        if event.name != nil && event.name! == "thread_name" {
            let threadName = event.args!["name"]!.value as! String
            threadInfos[tid]!.threadName = threadName
        }
        else if event.ts != nil && event.dur != nil {
            let s = event.ts!
            let d = event.dur!
            
            threadInfos[tid]!.combine(s, d, event.name)
        }
    }
    
    // DONE: could store this in the File object, just append with \n
    var text = ""
    for threadInfo in threadInfos.values.sorted() {
        // log.info(threadInfo.description)
        text += threadInfo.description
        text += "\n"
    }
    
    file.threadInfo = text
    updateFileCache(file: file)
}

func updateDuration(_ catapultProfile: CatapultProfile, _ file: inout File) {
    var startTime = Int.max
    var endTime = Int.min
    
    for i in 0..<catapultProfile.traceEvents!.count {
        let event = catapultProfile.traceEvents![i]
        
        if event.ts != nil && event.dur != nil {
            let s = event.ts!
            let d = event.dur!
            
            startTime = min(startTime, s)
            endTime = max(endTime, s+d)
        }
    }
    
    if startTime <= endTime {
        // for now assume micros
        file.duration = Double(endTime - startTime) * 1e-6
        
        updateFileCache(file: file)
    }
}


func loadFileJS(_ path: String) -> String? {
    
    let fileURL = URL(string: path)!
    
    // Note may need to modify directly
    var file = lookupFile(url: fileURL)
    
    log.debug(path)
    
    do {
        // use this for binary data, but need to fixup some json before it's sent
        var fileContentBase64 = ""
        
        let isFileGzip = file.containerType == .Compressed
        //let isFileZip = type == .Archive
        
        // Note: json.gz and json.zip build files are not marked as Build
        // but need to rewrite them.
        let isBuildFile = file.fileType == .Build
        
        //let filename = fileURL.lastPathComponent
        
        let fileContent = try Data(contentsOf: fileURL)
        
        // decompress archive from zip, since Perfetto can't yet decompress zip
        // Note this will typically be fileType unknown, but have to flatten
        // content within to the list.  This just means part of a zip archive.
        if file.containerType == .Archive {
            return nil
            //fileContent = unzippedContent
        }
        
        if !isBuildFile {
            // perfetto only supports gzip, comments indicate zip is possible but only with refactor
            // don't recompress gzip, note can't do timing if not decompressed
            let doCompress = !isFileGzip
            
            // This is how Perfetto guesses as to format.  Why no consistent 4 char magic?
            // https://cs.android.com/android/platform/superproject/main/+/main:external/perfetto/src/trace_processor/forwarding_trace_parser.cc;drc=30039988b8b71541ce97f9fb200c96ba23da79d7;l=176
            
            fileContentBase64 = fileContent.base64EncodedString()
            
            if !isFileGzip {
                // see if it's binary or json.  If binary, then can't parse duration below
                // https://forums.swift.org/t/improving-indexing-into-swift-strings/41450/18
                let jsonDetector = "ewoiZG" // "{\""
                let jsonDetector2 = "eyJ0cm" // utf16?
                let firstTwoChars = fileContentBase64.prefix(6)
                let isJson = firstTwoChars == jsonDetector || firstTwoChars == jsonDetector2
                
                // convert to gzip format, so send less data across to Safari
                if doCompress {
                    guard let compressedData: Data = fileContent.gzip() else { return nil }
                    fileContentBase64 = compressedData.base64EncodedString()
                }
                
                // walk the file and compute the duration if we don't already have it
                if isJson && file.duration == 0.0 {
                    let decoder = JSONDecoder()
                    let catapultProfile = try decoder.decode(CatapultProfile.self, from: fileContent)
                    
                    if catapultProfile.traceEvents == nil {
                        return nil
                    }
                    
                    updateDuration(catapultProfile, &file)
                    
                    // For now, just log the per-thread info
                    if file.fileType == .Memory {
                        updateThreadInfo(catapultProfile, &file)
                    }
                }
            }
        }
        else {
            // replace Source with actual file name on Clang.json files
            // That's just for the parse phase, probably need for optimization
            // phase too.  The optimized function names need demangled - ugh.
            
            // Clang has some build data as durations on fake threads
            // but those are smaller than the full duration.
            let doCompress = true
            
            var json : Data
            
            if file.containerType == .Compressed {
                guard let unzippedContent = fileContent.gunzip() else {
                    return nil
                }
                json = unzippedContent
            }
            else if file.containerType == .Archive {
                // this has already been decoded to json
                json = fileContent
            }
            else {
                json = fileContent
            }
            
            let decoder = JSONDecoder()
            var catapultProfile = try decoder.decode(CatapultProfile.self, from: json)
            
            if catapultProfile.traceEvents == nil { // an array
                return nil
            }
            else {
                for i in 0..<catapultProfile.traceEvents!.count {
                    let event = catapultProfile.traceEvents![i]
                    if event.name == "Source" ||
                        event.name == "OptModule"
                    {
                        // This is a path
                        let detail = event.args!["detail"]!.value as! String
                        let url = URL(string:detail)!
                        
                        // stupid immutable arrays.  Makes this code untempable
                        catapultProfile.traceEvents![i].name = url.lastPathComponent
                    }
                    else if event.name == "InstantiateFunction" ||
                                event.name == "InstantiateClass" ||
                                event.name == "OptFunction" ||
                                event.name == "ParseClass" ||
                                event.name == "DebugType" // these take a while
                    {
                        // This is a name
                        let detail = event.args!["detail"]!.value as! String
                        catapultProfile.traceEvents![i].name = detail
                    }
                }
                
                // walk the file and compute the duration if we don't already have ti
                if file.duration == 0.0 {
                    updateDuration(catapultProfile, &file)
                    
                    // For now, just log the per-thread info
                    if file.fileType == .Memory {
                        updateThreadInfo(catapultProfile, &file)
                    }
                }
            }
            
            let encoder = JSONEncoder()
            let fileContentFixed = try encoder.encode(catapultProfile)
            
            // gzip compress the data before sending it over
            if doCompress {
                guard let compressedData = fileContentFixed.gzip() else { return nil }
                fileContentBase64 = compressedData.base64EncodedString()
            }
            else {
                fileContentBase64 = fileContentFixed.base64EncodedString()
            }
        }
        
        return generateLoadFileJS(fileContentBase64: fileContentBase64, title:fileURL.lastPathComponent)
    }
    catch {
        log.error(error.localizedDescription)
        return nil
    }
}

func generateLoadFileJS(fileContentBase64: String, title: String) -> String?
{
    do {
        // https://stackoverflow.com/questions/62035494/how-to-call-postmessage-in-wkwebview-to-js
        struct PerfettoFile: Codable {
            var buffer: String // really ArrayBuffer, but will get replaced
            var title: String
            
            // About keepApiOpen
            // https://github.com/flutter/devtools/blob/master/packages/devtools_app/lib/src/screens/performance/panes/timeline_events/perfetto/_perfetto_web.dart#L174
            var keepApiOpen: Bool
            
            // optional fields
            //var fileName: String?
            // url cannot be file://, has to be http served.  Can we set fileName?
            //var url: String?
        }
        
        struct Perfetto: Codable {
            var perfetto: PerfettoFile
        }
        
        // really the url is the only part that needs encoded
        let perfetto = Perfetto(perfetto: PerfettoFile(buffer: "",
                                                       title: title,
                                                       keepApiOpen: true))
        var perfettoEncode = ""
        
        if true {
            let encoder = JSONEncoder()
            let data = try encoder.encode(perfetto)
            let encodedString = String(decoding: data, as: UTF8.self)
            perfettoEncode = String(encodedString.dropLast().dropFirst())
            perfettoEncode = perfettoEncode
                .replacingOccurrences(of: "\u{2028}", with: "\\u2028")
                .replacingOccurrences(of: "\u{2029}", with: "\\u2029")
        }
        
        let script = """
        
        //function bytesToBase64(bytes) {
        //  const binString = String.fromCodePoint(...bytes);
        //  return btoa(binString);
        
        function base64ToBytes(base64) {
          const binString = atob(base64);
          return Uint8Array.from(binString, (m) => m.codePointAt(0));
        }
        
        var fileData = '\(fileContentBase64)';
        
        // convert from string -> Uint8Array -> ArrayBuffer
        var obj = JSON.parse('{\(perfettoEncode)}');
        
        // convert base64 back
        obj.perfetto.buffer = base64ToBytes(fileData).buffer;
        
        // Fix race between page load, and loading the file.  Although
        // page is only loaded once.
        
        // What if last listener isn't complete, or is doing the postMessage
        // JS is all running on one thread though?
        
        // https://jsfiddle.net/vrsofx1p/
        function waitForUI(obj)
        {
            const timer = setInterval(() => window.postMessage('PING', '\(ORIGIN)'), 50);
            
            const onMessageHandler = (evt) => {
                if (evt.data !== 'PONG') return;
                
                // We got a PONG, the UI is ready.
                window.clearInterval(timer);
                window.removeEventListener('message', onMessageHandler);
                
                window.postMessage(obj, '\(ORIGIN)');
            }
        
            window.addEventListener('message', onMessageHandler);
        }
        
        waitForUI(obj);
        """
        
// This was trying to block the native drop handler
//                    // ugh, document does notwork either
//                    if (false) {
//                        // tried adding to various parts above.  Need to install
//                        // after the page is open, but this doesn't override the default
//                        // turn off drop handling, or it won't fixup json or appear in list
//                        // This doesn't work
//                        window.addEventListener('drop', function(e) {
//                          e.preventDefault();
//                          e.stopPropagation();
//                        });
//                        window.addEventListener('dragover', function(e) {
//                          e.preventDefault();
//                          e.stopPropagation();
//                        });
//                    }
        
        return script
    }
    catch {
        log.error(error.localizedDescription)
        return nil
    }
}

class AppDelegate: NSObject, NSApplicationDelegate {
    // Where to set this
    var window: NSWindow?
    
    // don't rename params in this class. These are the function signature
    func applicationShouldTerminateAfterLastWindowClosed(_ application: NSApplication) -> Bool {
        return true
    }
}

@main
struct kram_profileApp: App {
    //@State private var files: [File] = []
    
    // @State private var files: [File] = []
    @State private var selection: String?
    
    // close app when last window is
    @NSApplicationDelegateAdaptor private var appDelegate: AppDelegate
    
    func runJavascript(_ webView: WKWebView, _ script: String) {
        webView.evaluateJavaScript(script) { (result, error) in
            if error != nil {
                log.error("problem running script")
            }
        }
    }
     
    func isSupportedFilename(_ url: URL) -> Bool {
        let ext = url.pathExtension
        
        // what ext does trace.zip, or trace.gz come in as ?
        // should this limit compressed files to the names supported below - json, trace, memtrace?
        
        if ext == "gz" {
            return true
        }
//        if ext == "zip" {
//            return true
//        }
            
        // clang build files use generic .json ext
        if ext == "json" || ext == "buildtrace" {
            let filename = url.lastPathComponent
            
            // filter out some by name, so don't have to open files
            if filename == "build-description.json" ||
                filename == "build-request.json" ||
                filename == "manifest.json" ||
                filename.hasSuffix("diagnostic-filename-map.json") ||
                filename.hasSuffix(".abi.json") ||
                filename.hasSuffix("-OutputFileMap.json") ||
                filename.hasSuffix("_const_extract_protocols.json")
            {
                return false
            }
            return true
        }
        
        // profiling
        if ext == "perftrace" || ext == "trace" {
            return true
        }
        
        // memory
        if ext == "memtrace" {
            return true
        }
        
        return false
    }
    
    func listFilesFromArchive(_ url: URL) -> [File] {
        // TODO:
        // also add zip to supported format
        // flatten archive and add archive to list
        // need to mmap the archive, point content files to archive loc
        
        return []
    }
    
    func listFilesFromURLs(_ urls: [URL]) -> [File]
    {
        var files: [File] = []
       
        for url in urls {
            // now filter a list of all the files under the dir
            if url.hasDirectoryPath {
                // list out all matching files
                // also these [.skipsHiddenFiles, .skipsSubdirectoryDescendants]
                
                // recurse into directory
                let directoryEnumerator = FileManager.default.enumerator(
                    at: url,
                    includingPropertiesForKeys: nil
                    // options: [.skipsHiddenFiles]
                )
                
                while let fileURL = directoryEnumerator?.nextObject() as? URL {
                    let isSupported = isSupportedFilename(fileURL)
                    if isSupported {
                        let isArchive  = File.filenameToContainerType(url) == .Archive
                        if isArchive {
                           files += listFilesFromArchive(fileURL)
                        }
                        else {
                            files.append(lookupFile(url:fileURL));
                        }
                    }
                }
            }
            else if url.isFileURL {
                let isSupported = isSupportedFilename(url)
                if isSupported {
                    let isArchive = File.filenameToContainerType(url) == .Archive
                    if isArchive {
                        files += listFilesFromArchive(url)
                    }
                    else {
                        files.append(lookupFile(url:url))
                    }
                }
            }
        }
    
        return files
    }
    
    // What is used when Inter isn't installed.  Can this be bundled?
    //let customFont = Font.custom("Inter Variable", size: 14)
                
    func openFileFromURLs(urls: [URL]) {
        droppedFileCache = urls
        reopenFileFromURLs()
    }
    
    func reopenFileFromURLs() {
        let urls = droppedFileCache
        
        if urls.count >= 1 {
            let filesNew = listFilesFromURLs(urls)
            
            // for now wipe the old list
            if filesNew.count > 0 {
                // turning this off for now, File must impl Hashable
//                let mergeFiles = false
//                
//                if mergeFiles {
//                    fileSearcher.files = Array(Set(fileSearcher.files + filesNew))
//                }
//                else 
                //{
                    // reset the list
                    fileSearcher.files = filesNew
                //}
                
                fileSearcher.updateFilesSearched()
                
                log.debug("found \(fileSearcher.files.count) files")
                
                // preserve the original selection if still present
                if selection != nil {
                    var found = false
                    for file in fileSearcher.files {
                        if file.id == selection {
                            found = true
                            break;
                        }
                    }
                    
                    // load first file in the list
                    if !found {
                        selection = fileSearcher.files[0].id
                    }
                }
                else {
                    // load first file in the list
                    selection = fileSearcher.files[0].id
                }
            }
        }
    }
    
    func openContainingFolder(_ str: String) {
        let url = URL(string: str)!
        NSWorkspace.shared.activateFileViewerSelecting([url]);
    }
    
//    func isReloadEnabled(_ selection: String?) -> Bool {
//        guard let sel = selection else { return false }
//        let file = lookupFile(selection: sel)
//        return file.isReloadNeeded()
//    }
    
    func openFile() {
        let panel = NSOpenPanel()
        panel.allowsMultipleSelection = true
        panel.canChooseDirectories = true
        panel.canChooseFiles = true
        panel.allowedContentTypes = fileTypes
        
        panel.begin { reponse in
            if reponse == .OK {
                openFileFromURLs(urls: panel.urls)
            }
        }
    }
    
    func openFileSelection(_ webView: WKWebView) {
        if let sel = selection {
            
            // This should only reload if selection previously loaded
            // to a valid file, or if modstamp changed on current selection
            
            // TODO: fix this
            let objTimeScript: String? = nil // buildTimeRangeJson(filenameToTimeRange(sel))
            
            var str = loadFileJS(sel)
            if str != nil {
                runJavascript(webView, str!)
                
                let file = lookupFile(selection: sel)
                file.setLoadStamp()
                updateFileCache(file: file)
            }
            
            // Want to be able to lock the scale of the
            // trace, so that when moving across traces the range is consistent.
            // Otherwise, small traces get expanded to look huge.
            // This only works the first time a file loads.
            if objTimeScript != nil {
                str = showTimeRangeJS(objTimeScript: objTimeScript!)
                if str != nil {
                    runJavascript(webView, str!)
                }
            }
        }
    }
    
    func aboutPanel() {
        NSApplication.shared.orderFrontStandardAboutPanel(
            options: [
                NSApplication.AboutPanelOptionKey.credits: NSAttributedString(
                    string:
"""
A tool to help profile mem, perf, and builds.
© 2020-2024 Alec Miller
""",
                    
                    attributes: [
                        // TODO: fix font
                        NSAttributedString.Key.font: NSFont.boldSystemFont(ofSize: NSFont.smallSystemFontSize)
                    ]
                ),
                NSApplication.AboutPanelOptionKey(
                    rawValue: "kram-profile"
                ): "© 2020-2024 Alec Miller"
            ]
        )
    }
    
    // DONE: have files ending in .memtrace, .trace, and .json
    // TODO: archives in the zip file.
    
    let fileTypes: [UTType] = [
        // This is what macOS generates when doing "compress file".  But could be archive.
        // These are 12x smaller often times.  Decompression lib only handles zlib.
        // TODO: need zip archive util to extract the 1+ files, can still use libCompression to decompress
        // .zip,
       
        // Perfetto can only open gzip and not zip yet
        // These are 12x smaller often times
        .gzip,
              
        // A mix of json or binary format files
        .json, // clang build files
        UTType(filenameExtension:"trace", conformingTo:.data)!, // conformingTo: .json didn't work
        UTType(filenameExtension:"memtrace", conformingTo:.data)!,
        UTType(filenameExtension:"perftrace", conformingTo:.data)!,
        UTType(filenameExtension:"buildtrace", conformingTo:.data)!,
    ]
    
    func selectFile(_ selection: String?, _ fileList: [File], _ advanceList: Bool) -> String? {
        guard let sel = selection else { return nil }
        if fileList.count == 1 { return selection }
        
        var index = 0
        for i in 0..<fileList.count {
            let file = fileList[i]
            if file.id == sel {
                index = i
                break
            }
        }
        
        let doClamp = true
        if doClamp {
            index = clamp(index + (advanceList == true ? 1:-1), 0, fileList.count-1)
        }
        else {
            // this wraps, but List view needs to scroll to this if not visible
            index = (index + fileList.count + (advanceList == true ? 1:-1)) % fileList.count
        }
        return fileList[index].id
    }
    
    // about hideSideBar
    // https://github.com/google/perfetto/issues/716
    // Allocating here only works for a single Window, not for WindowGroup
    @State var myWebView = newWebView(request: URLRequest(url:URL(string: ORIGIN + "/?hideSidebar=true")!))
    
    // Focus state says which panel has keyboard routing
    enum Field: Hashable {
        case webView
        case listView
    }
    @FocusState private var focusedField: Field?

    @StateObject var fileSearcher = FileSearcher()

    // TODO: do this when building the searchResults
    // can do O(N) then and mark which items need separator
    func isSeparatorVisible(_ file: File, _ searchResults: [File]) -> Bool {
        
        // TODO: faster way to find index of file
        // probably worth caching up these
        for i in 0..<searchResults.count-1 {
            if file == searchResults[i] {
                let nextFile = searchResults[i+1]
                if file.url.deletingLastPathComponent().path != nextFile.url.deletingLastPathComponent().path {
                    return true
                }
            }
        }
        
        return false
    }
    
    @State private var isShowingPopover = false
    
    func getSelectedThreadInfo(_ selection: String?) -> String {
        if selection == nil {
            return ""
        }
        return lookupFile(selection: selection!).threadInfo
    }
    
    func isMemoryFileType(_ selection: String?) -> Bool {
        if selection == nil {
            return false
        }
        return lookupFile(selection: selection!).fileType == .Memory
    }
    
    // iOS can go compact reduces to 1 column
    // can add as a subheading of the text then
//    private var isCompact: Bool {
//        horizontalSizeClass == .compact
//      }
    
    // here's about inheriting from CoreData, @FetchRquest, NSManagedObject, @NSManaged, etc.
    // so it's retained, and not dealing with silly Swift struct getting destroyed.
    // and using a class.
    // https://useyourloaf.com/blog/swiftui-tables-quick-guide/
    
    // Comparators aren't same as KeyPathComparator, ugh
    // https://useyourloaf.com/blog/custom-sort-comparators/
    
    let buildIcon = Image(systemName: "c.square") // compile
    let perfIcon = Image(systemName: "p.square")
    let memoryIcon = Image(systemName: "m.square")
    let unknownIcon = Image(systemName: "questionmark.app")
    
    // https://www.hackingwithswift.com/example-code/uikit/how-to-use-system-icons-in-your-app
    func generateIcon(_ file: File) -> Image {
        switch file.fileType {
            case .Build: return buildIcon
            case .Memory: return memoryIcon
            case .Perf: return perfIcon
            case .Unknown: return unknownIcon
        }
    }
    
    var body: some Scene {
        
        // TODO: WindowGroup brings up old windows which isn't really what I want
    
        Window("Main", id: "main") {
            NavigationSplitView() {
                VStack {
                    List(fileSearcher.filesSearched, selection:$selection) { file in
                        HStack() {
                            // If number ir icon is first, then that's all SwiftUI
                            // uses for typeahead list search.  Seems to
                            // be no control over that.
                            generateIcon(file)
                            
                            // name gets truncated too soon if it's first
                            // and try to align the text with trailing.
                            Text(file.name)
                                .help(file.shortDirectory)
                                .truncationMode(.tail)
                            
                            // TODO: how to fix align to right of nav panel?
                            Text(generateDuration(file: file))
                                //.frame(maxWidth: 70)
                                .frame(maxWidth: .infinity, alignment: .trailing)
                        }
                        .listRowSeparator(isSeparatorVisible(file, fileSearcher.filesSearched) ? .visible : .hidden)
                        .listRowSeparatorTint(.white)
                    }
                    .focused($focusedField, equals: .listView)
                    .focusable()
                }
            }
            detail: {
                VStack {
                    // This button conveys data Perfetto does not
                    // It's basically a hud.
                    HStack {
                        // only display if FileType.Memory
                        if isMemoryFileType(selection) {
                            Button("Info") {
                                self.isShowingPopover.toggle()
                            }
                            .keyboardShortcut("I", modifiers:.command)
                            .disabled(selection == nil)
                            .popover(isPresented: $isShowingPopover) {
                                ScrollView() {
                                    Text(getSelectedThreadInfo(selection))
                                        .multilineTextAlignment(.leading)
                                    //.lineLimit(16)
                                        .padding()
                                }
                            }
                        }
                    }
                    WebView(webView: myWebView)
                        .focused($focusedField, equals: .webView)
                        .focusable()
                }
            }
            
            // This only seems to display with List, not with Table.  Or it's under Table
            .searchableOptional(text: $fileSearcher.searchText, isPresented: $fileSearcher.searchIsActive, placement: .sidebar, prompt: "Filter")
                
            .onChange(of: selection /*, initial: true*/) { newState in
                openFileSelection(myWebView)
                //focusedField = .webView
            }
            .navigationTitle(generateNavigationTitle(selection))
            .onOpenURL { url in
                openFileFromURLs(urls: [url])
                //focusedField = .webView
            }
            .dropDestination(for: URL.self) { (items, _) in
                // This acutally works!
                openFileFromURLs(urls: items)
                //focusedField = .webView
                return true
            }
        }
        // This is causing an error on GitHub CI build, but not when building locally
        //.environment(\.font, customFont)
        // https://nilcoalescing.com/blog/CustomiseAboutPanelOnMacOSInSwiftUI/
        .commands {
            CommandGroup(after: .newItem) {
                Button("Open...") {
                    openFile()
                }
                .keyboardShortcut("O", modifiers:[.command])
                
                // Really want to go to .h selected in flamegraph, but that would violate sandbox.
                // This just goes to the trace json file somewhere in DerviceData which is less useful.
                // For selected file can only put on clipboard.
                Button("Go to File") {
                    if selection != nil {
                        openContainingFolder(selection!);
                    }
                }
                .keyboardShortcut("G", modifiers:[.command])
                
                Button("Sort Name") {
                    fileSearcher.updateFilesSearched(false)
                }
                .keyboardShortcut("T", modifiers:[.shift, .command])
                
                Button("Sort Range") {
                    fileSearcher.updateFilesSearched(true)
                }
                .keyboardShortcut("T", modifiers:[.command])
                
                Button("Reload") {
                    // TODO: This loses the order if sorted by duration
                    // but easy enough to resort
                    
                    // this reloads the entier file list
                    reopenFileFromURLs()
                    
                    // this tries to reload the selection
                    openFileSelection(myWebView)
                }
                .keyboardShortcut("R", modifiers:[.command])
                .disabled(selection == nil) // !isReloadEnabled(selection))
                
                // These work even if the list view is collapsed
                Button("Prev File") {
                    if selection != nil {
                        selection = selectFile(selection, fileSearcher.filesSearched, false)
                        
                        // TODO: no simple scrollTo, since this is all React style
                        // There is a ScrollViewReader, but valid only usable within
                    }
                }
                .keyboardShortcut("N", modifiers:[.shift, .command])
                .disabled(selection == nil)
                
                Button("Next File") {
                    if selection != nil {
                        selection = selectFile(selection, fileSearcher.filesSearched, true)
                    }
                }
                .keyboardShortcut("N", modifiers:[.command])
                .disabled(selection == nil)
                
                // TODO: these may need to be attached to detail view
                // The list view eats them, and doesn't fwd onto the web view
                
                // Can see the commands.  Wish I could remap this.
                // https://github.com/google/perfetto/blob/98921c2a0c99fa8e97f5e6c369cc3e16473c695e/ui/src/frontend/app.ts#L718
                // Perfetto Command
                Button("Search") {
                    // Don't need to do anything
                }
                .keyboardShortcut("S", modifiers:[.command])
                .disabled(selection == nil && focusedField == .webView)
                          
                // Perfetto command
                Button("Parse Command") {
                    // don't need to do anything
                }
                .keyboardShortcut("P", modifiers:[.shift, .command])
                .disabled(selection == nil && focusedField == .webView) // what if selection didn't load
            }
            CommandGroup(replacing: .appInfo) {
                Button("About kram-profile") {
                    aboutPanel()
                }
            }
        }
    }
}

