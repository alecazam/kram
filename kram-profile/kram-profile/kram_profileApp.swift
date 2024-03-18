// kram - Copyright 2020-2024 by Alec Miller. - MIT License
// The license and copyright notice shall be included
// in all copies or substantial portions of the Software.

import SwiftUI
import WebKit
import UniformTypeIdentifiers

// https://github.com/gualtierofrigerio/WkWebViewJavascript/blob/master/WkWebViewJavascript/WebViewHandler.swift

// https://levelup.gitconnected.com/how-to-use-wkwebview-on-mac-with-swiftui-10266989ed11
// Signing & Capabilites set App Sandbox (allow outgoing connections)

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

// TODO: Hitting T to sort, changes the selection.  That shouldn't happen.

// Memory traces
// TODO: sort thread by size - repack the memory graph largest to smallest by reordering each track
//   then can focus on the bigger values.
// TODO: Sort by name and convert to count - can then see common counts
//   so keep the json loaded in Swift.  Can json be cloned and modded?
// TODO: option to coalesce to count and name with sort

// Build traces
// DONE: build hierarchy and self times
// DONE: background process to compute buildTimings across all files
// TODO: parse totals from build traces, what CBA is doing
// TODO: present total time, and % of total in the nav panel

// Perf traces
// TODO: build per-thread hierarchy and self times

// TODO: track kram-profile memory use, jettison Data that isn't needed after have built up timings.
// can re-decompress from zip mmap.

// DONE: import zip
// DONE: add/update recent document list (need to hold onto dropped/opened folder)
// DONE: can't mmap web link, but can load zip off server with timings

// TODO: run cba on files, mmap and decompress each can use incremental mode?
// TODO: save/load the duration and modstamps for File at quit, and any other metadata (totals per section)
// TODO: add jump to source/header, but path would need to be correct (sandbox block?)

// TODO: look into fast crc32 ops on M1
// https://dougallj.wordpress.com/2022/05/22/faster-crc32-on-the-apple-m1/

// Build traces
// DONE: OptFunction needs demangled.  All backend strings are still mangled.
//  Don’t need the library CBA uses just use api::__cxa_demangle() on macOS.
//  https://github.com/llvm/llvm-project/issues/459

// TODO: across all files, many of the strings are the same.  Could replace all strings
// with an index, compress, and zip archive with the index table.  buid, perf, mem strings
// filenames are also redundant.  Just need to use @[F|S|B]num, and then do lookup before CBA final report.
// table if global would need to use same index across all files.
// Can rebuild references on JS side to send less data.  JS can then alias strings ftw.
// Just add special ph type that is ignored by web to specify the alias.
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

// DONE: recent documents list doesn't survive relaunch, but only when app is rebuilt
// but still kind of annoying for development

// DONE: have a way to reload dropped folder
// DONE: track parent archives, folder, and loose drop files
// and when reload is hit, then reload all of that rebuild the list
// and then reload the selected file
// DONE: zipHelper to deal with archives, can use Swift Data to mmap content if needed
//   mmap the zip, list out the files and locations, and then defalte the content somewhere
//   only then can data be handed off toe Pefertto or CBA.  And CBA needs all files.
//   Maybe extend CBA to read a zip file.  Can just use ZipHelper.

// TODO: fix duration update modding the List item and nav title after it updates
// Currently select list, it updates, then duration is calcualated.
//   there is a objectWillChange.send() Could maybe send that from The File
// https://www.hackingwithswift.com/forums/swiftui/update-content-of-existing-row-of-list/3029

// WKWebView
// TODO: can't block drop onto the WKWebView
// TODO: can't overide "delete" or "shift+Delete" key doing a back/fwd in the WKWebView history
//    Perfetto warns that content will be lost

// Perfetto Bugs
// TODO: fix the js wait, even with listener, there's still a race
//    maybe there's some ServiceWorker still loading the previous json?
//    Perfetto is using a ServiceWorker, Safari uses those now, and ping/pong unware.
// TODO: switch to Perfetto dark mode

// Multi-window
// TODO: support WindowGroup and multiwindow, each needs own webView, problem
//   is that onOpenURL opens a new window always.
// TODO: look in to hosting Remotery web and/or Tracy server, Tracy is imgui
//   but these don't review traces offline, and are live profilers
// TODO: add Metal capture and imgui backend to Tracy
// TODO: add Metal capture to Remotery (this isn't a flamegraph)

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
    
    // I made this plublished so sort would also cause update to filesSearched
    // but the search field keeps re-focusing
    // @Published var filesSorted: [File] = []
    var filesSorted: [File] = []
    
    // duplicate code, but init() doesn't have self defined
    func updateFilesSorted(_ sortByDuration: Bool = false) {
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
        
        filesSorted = sortedResults
        
        // TODO: important or filesSearched isn't updated in the list when
        // the sort occurs.  This is causing filter to re-focus since
        // it thinks the searchText which is also Published changed.
        objectWillChange.send()
    }
       
    var filesSearched: [File] {
        
        if searchText.isEmpty || filesSorted.count <= 1  {
            return filesSorted
        }
        else if searchText.count == 1 {
            let lowercaseSearchText = searchText.lowercased()
            let uppercaseSearchText = searchText.uppercased()

            return filesSorted.filter {
                $0.name.starts(with: uppercaseSearchText) ||
                $0.name.starts(with: lowercaseSearchText)
            }
        }
        else {
            // is search text multistring?
            return filesSorted.filter {
                $0.name.localizedCaseInsensitiveContains(searchText)
            }
        }
    }
}

// https://stackoverflow.com/questions/24074479/how-to-create-a-string-with-format
extension String.StringInterpolation {

    /// Quick formatting for *floating point* values.
    mutating func appendInterpolation(float: Float, decimals: UInt = 2, zero: Bool = true) {
        let floatDescription = String(format:"%.\(decimals)f%", float)
//        if stripTrailingZeros && decimals > 0 {
//            // https://stackoverflow.com/questions/29560743/swift-remove-trailing-zeros-from-double
//            floatDescription = floatDescription.replacingOccurrences(of: "^([\\d,]+)$|^([\\d,]+)\\.0*$|^([\\d,]+\\.[0-9]*?)0*$", with: "$1$2$3", options: .regularExpression)
//        }
        appendLiteral(floatDescription)
    }
    
    mutating func appendInterpolation(double: Double, decimals: UInt = 2, zero: Bool = true) {
        let floatDescription = String(format:"%.\(decimals)f%", double)
//        if stripTrailingZeros && decimals > 0 {
//            // https://stackoverflow.com/questions/29560743/swift-remove-trailing-zeros-from-double
//            floatDescription = floatDescription.replacingOccurrences(of: "^([\\d,]+)$|^([\\d,]+)\\.0*$|^([\\d,]+\\.[0-9]*?)0*$", with: "$1$2$3", options: .regularExpression)
//        }
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

private let log = Log("kram/App")

public func clamp<T>(_ value: T, _ minValue: T, _ maxValue: T) -> T where T : Comparable {
    return min(max(value, minValue), maxValue)
}


//-------------

class MyWebView : WKWebView {
    
    // So that keyboard events are routed
    override var acceptsFirstResponder: Bool { true }
    
        // https://developer.apple.com/library/archive/documentation/Cocoa/Conceptual/EventOverview/HandlingKeyEvents/HandlingKeyEvents.html
    
// https://nshipster.com/wkwebview/
    
    func isKeyHandled(_ event: NSEvent) -> Bool {
    
        // can't block delete or shift+delete
        // so the WKWebView goes back/foward through it's 1 page history.
        // that loses all context for the user.
        
        // prevent super annoying bonk/NSBeep
        // if don't check modifier flags (can't check isEmpty since 256 is often set
        // then the cmd+S stops working
        if !(event.modifierFlags.contains(.command) ||
             event.modifierFlags.contains(.control) ||
             event.modifierFlags.contains(.option)) {
            // wasd
            if event.keyCode == Keycode.w || event.keyCode == Keycode.a || event.keyCode == Keycode.s || event.keyCode == Keycode.d {
                return true
            }
        }
        return false
    }
    
    // Apple doesn't want this to be overridden by user, but key handling
    // just doesn't work atop the WKWebView without this.  KeyUp/KeyDown
    // overrides don't matter, since we need the WKWebView to forward them
    override func performKeyEquivalent(with event: NSEvent) -> Bool {
        if !isKeyHandled(event) {
            return super.performKeyEquivalent(with: event)
        }
        return true
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
    
    duration = 10.0
    
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
                keepApiOpen: true,
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
    
    // These are data computed from the events
    var durSub: Int?
    var parentIndex: Int?
    
    // can't have setters on a Struct, only init
    init(_ tid: Int, _ name: String, _ dur: Int) {
        self.ts = 0
        self.tid = tid
        self.dur = dur
        self.name = name
        self.ph = "X"
    }
    
    init(tid: Int, threadName: String) {
        self.ts = 0
        self.dur = 0
        self.name = "thread_name"
        self.ph = "M"
        self.tid = tid
        self.args = [:]
        self.args!["name"] = AnyCodable(threadName)
    }

    // only encode/decode some of the keys
    enum CodingKeys: String, CodingKey {
        case cat, pid, tid, ph, ts, dur, name, args
    }
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
            return "\(id) '\(threadName)' \(double: duration, decimals:6)s \(count)x"
        }
        else {
            return "\(id) '\(threadName)' \(double: duration, decimals:6)s \(double:percentage, decimals:0)% \(count)x"
        }
    }
    
}

// Could also process each build timings in a threaded task.  That what CBA is doing.
class BuildTiming: NSCopying {
    var name = ""
    var count = 0
    var duration = 0
    var durationSub = 0
    var durationSelf: Int { return max(0, duration - durationSub) }
    var type = ""
    
    func combine(_ duration: Int, _ durationSub: Int) {
        self.duration += duration
        self.durationSub += durationSub
        self.count += 1
    }
    
    // This is annoying in Swift
    func copy(with zone: NSZone? = nil) -> Any {
        let copy = BuildTiming()
        copy.name = name
        copy.count = count
        copy.duration = duration
        copy.durationSub = durationSub
        copy.type = type
        return copy
      }
}

func updateFileBuildTimings(_ events: [CatapultEvent]) -> [String:BuildTiming] {
    var buildTimings: [String:BuildTiming] = [:]
    
    // DONE: would be nice to compute the self times.  This involves
    // sorting the startTime on a track, then by largest duration on ties
    // and then subtracting the immediate children.
    // See what CBA and Perfetto do to establish this.
    
    // Would be good to establish this nesting once and store the level
    // with each event.d
    
    // run through each file, and build a local map of name to size count
    for i in 0..<events.count {
        let event = events[i]
        
        if  event.name == "Source" || // will be .h
            event.name == "OptModule" // will be .c/.cpp
        {
            // This is a path
            let sourceFile = event.args!["detail"]!.value as! String
            
            if let buildTiming = buildTimings[sourceFile] {
                buildTiming.combine(event.dur!, event.durSub ?? 0)
            }
            else {
                let buildTiming = BuildTiming()
                buildTiming.name = sourceFile
                buildTiming.combine(event.dur!, event.durSub ?? 0)
                buildTiming.type = event.name!
                
                buildTimings[sourceFile] = buildTiming
            }
        }
    }
    
    return buildTimings
}

func findFilesForBuildTimings(files: [File], selection: String) -> [File] {
    let selectedFile = lookupFile(url:URL(string:selection)!)
    let isArchive = selectedFile.archive != nil
    
    let filteredFiles = files.filter { file in
        if isArchive {
            return file.archive != nil && file.archive! == selectedFile.archive!
        }
        else {
            return file.parentFolders == selectedFile.parentFolders
        }
    }
    
    return filteredFiles;
}

func postBuildTimingsReport(files: [File]) -> String? {
    let buildTimings = mergeFileBuildTimings(files: files)
    if buildTimings.isEmpty { return nil }
    let buildStats = mergeFileBuildStats(files:files)
    
    let buildJsonBase64 = generateBuildReport(buildTimings: buildTimings, buildStats: buildStats)
    let buildJS = postLoadFileJS(fileContentBase64: buildJsonBase64, title: "BuildTimings")
    return buildJS
}

func mergeFileBuildStats(files: [File]) -> BuildStats {
    let buildStats = BuildStats()
    for file in files {
        if file.buildStats != nil {
            buildStats.combine(file.buildStats!)
        }
    }
    
    buildStats.frontendStart = 0
    buildStats.backendStart = buildStats.totalFrontend
    
    // This will scale way beyond the graph, so make it an average
    // But will have lost the totals doing this.  Could embed them in string.
    buildStats.divideBy(10)
    
    return buildStats
}
    
func mergeFileBuildTimings(files: [File]) -> [String:BuildTiming] {
    var buildTimings: [String:BuildTiming] = [:]
    
    // run through all files, and zip the maps together
    // then turn that into build events that can be shown.
    for file in files {
        // merge and combine duplicates
        for buildTiming in file.buildTimings {
            if buildTimings[buildTiming.key] == nil {
                buildTimings[buildTiming.key] = (buildTiming.value.copy() as! BuildTiming)
            }
            else {
                let v = buildTiming.value
                buildTimings[buildTiming.key]!.combine(v.duration, v.durationSub)
            }
        }
        // buildTimings.merge didn't work, combine src values
    }
    
    return buildTimings
}

func generateBuildReport(buildTimings: [String:BuildTiming], buildStats: BuildStats) -> String {
    // now convert those timings back into a perfetto displayable report
    // So just need to build up the json above into events on tracks
    var events: [CatapultEvent] = []

    // Also sort or assign a sort_index to the tracks.  Sort biggest to smallest.
    // Make the threadName for the track be the short filename.
    
    // add the thread names, only using 3 threads
    if true {
        let names = ["ParseTime", "ParseCount", "ParseSelf", "OptimizeTime"]
        for i in 0..<names.count {
            let event = CatapultEvent(tid: i+1, threadName: names[i])
            events.append(event)
        }
    }
    
    for buildTiming in buildTimings {
        let t = buildTiming.value
        
        // TODO: could throw average time in too
        let shortFilename = URL(string: buildTiming.key)!.lastPathComponent
        
        let dur = Double(t.duration) * 1e-6
        let durSelf = Double(t.durationSelf) * 1e-6
        var event = CatapultEvent(0, "", t.duration)
        
        // Need to see this in the name due to multiple sorts
        
        let isHeader = t.type == "Source"
        
        // add count in seconds, so can view sorted by count below the duration above
        if isHeader {
            event.name = "\(shortFilename) \(t.count)x \(double: dur, decimals:2, zero: false)s"
            
            // ParseTime
            event.tid = 1
            events.append(event)
            
            // ParseCount
            event.tid = 2
            event.dur = t.count * 10000 // in 0.1 secs, but high counts dominate the range then
            events.append(event)
            
            let selfTime = t.durationSelf
            if selfTime > 0 {
                event.name = "\(shortFilename) \(t.count)x \(double: durSelf, decimals:2, zero: false)s"
                
                // ParseSelf
                event.tid = 3
                event.dur = t.durationSelf
                events.append(event)
            }
        }
        else {
            event.name = "\(shortFilename) \(double: dur, decimals:2, zero: false)s"
            
            // OptimizeTime
            event.tid = 4
            events.append(event)
        }
    }
    
    events.sort {
        // want threadnames first, could just prepend these to array?
        if $0.ph! != $1.ph! {
            return $0.ph! < $1.ph!
        }
        
        // then thread id
        if $0.tid! != $1.tid! {
            return $0.tid! < $1.tid!
        }
        
        // then duration
        // has to be > to work as a single tid
        if $0.dur != $1.dur! {
            return $0.dur! > $1.dur!
        }
        
        // then name
        return $0.name! < $1.name!
    }
   
    // add in the summary of % spent across the build
    let totalTrackEvents = convertStatsToTotalTrack(buildStats)
    events += totalTrackEvents
    
    let catapultProfile = CatapultProfile(traceEvents: events)
    
    do {
        // json encode, compress, and then base64 encode that
        let encoder = JSONEncoder()
        let fileContentFixed = try encoder.encode(catapultProfile)
        
        // gzip compress the data before sending it over
        guard let compressedData = fileContentFixed.gzip() else { return "" }
        let fileContentBase64 = compressedData.base64EncodedString()
        
        return fileContentBase64
    }
    catch {
        log.error(error.localizedDescription)
    }
    
    return ""
}


// TODO: Hook this up for memory traces, build more efficient array of thread events
func sortThreadsByName(_ catapultProfile: inout CatapultProfile) {
    
    var threads: [Int: [Int]] = [:]
    
    // first sort each thread
    for i in 0..<catapultProfile.traceEvents!.count {
        let event = catapultProfile.traceEvents![i]
        
        guard let tid = event.tid else { continue }
        if event.ts == nil || event.dur == nil { continue }
        
        if event.ph! == "M" && event.name != nil && (event.name! == "thread_name" || event.name! == "process_name") {
            continue
        }
        
        if threads[tid] == nil {
            threads[tid] = []
        }
        // just store the event index
        threads[tid]!.append(i)
    }
    
    for var thread in threads.values {
        // TODO: want to buble the top allocators by count or mem
        // to the front of the list.  Once have names sorted
        // then can group totals
        
        // sort each thread by name then dur
        thread.sort {
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

// these are per thread min/max for memory reports
func updateThreadInfo(_ catapultProfile: CatapultProfile, _ file: inout File) {
    // was using Set<>, but having trouble with lookup
    var threadInfos: [Int: ThreadInfo] = [:]
    
    for i in 0..<catapultProfile.traceEvents!.count {
        let event = catapultProfile.traceEvents![i]
        
        // have to have tid to associate with ThreadInfo
        guard let tid = event.tid, 
              let phase = event.ph else { continue }
        
        if threadInfos[tid] == nil {
            let info = ThreadInfo()
            info.id = tid
            
            threadInfos[tid] = info
        }
        
        if phase == "M" {
            if event.name != nil && event.name! == "thread_name" {
                let threadName = event.args!["name"]!.value as! String
                threadInfos[tid]!.threadName = threadName
            }
        }
        else if phase == "X" {
            if event.ts != nil && event.dur != nil {
                let s = event.ts!
                let d = event.dur!
                
                threadInfos[tid]!.combine(s, d, event.name)
            }
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
}

func updateDuration(_ events: [CatapultEvent]) -> Double {
    var startTime = Int.max
    var endTime = Int.min
    
    for i in 0..<events.count {
        let event = events[i]
        
        if event.ph != nil && event.ph! == "X" && event.ts != nil && event.dur != nil {
            let s = event.ts!
            let d = event.dur!
            
            startTime = min(startTime, s)
            endTime = max(endTime, s+d)
        }
    }
    
    var duration = 0.0
    if startTime <= endTime {
        // for now assume micros
        duration = Double(endTime - startTime) * 1e-6
    }
    return duration
}

// After calling this, can compute the self time, and have the parent hierarchy to draw
// events as a flamegraph.
func computeEventParentsAndDurSub(_ events: inout [CatapultEvent]) {
    // see CBA FindParentChildrenIndices for the adaption here
    // Clang Build Analyzer https://github.com/aras-p/ClangBuildAnalyzer
    // SPDX-License-Identifier: Unlicense
    // https://github.com/aras-p/ClangBuildAnalyzer/blob/main/src/Analysis.cpp
    
    // copy the events, going to replace this array with more data
    //var events = catapultProfile.traceEvents!
    
    var sortedIndices: [Int] = []
    for i in 0..<events.count {
        sortedIndices.append(i)
    }
    
    sortedIndices.sort {
        let e0 = events[$0]
        let e1 = events[$1]
        
        if e0.ph! != e1.ph! {
            return e0.ph! < e1.ph!
        }
        
        // for thread names, just sort by tid
        if e0.ph! == "M" {
            return e0.tid! < e1.tid!
        }
        
        // build events do have tid for the totals, but they only have 1 event
        if e0.tid! != e1.tid! {
            return e0.tid! < e1.tid!
        }
        if e0.ts! != e1.ts! {
            return e0.ts! < e1.ts!
        }
        
        // sort larger to smaller dur
        if e0.dur! != e1.dur! {
            return e0.ts! > e1.ts!
        }
        
        // later events assumed to be parent with "X" events written out when done
        return $0 > $1
    }
    
    // so now can look at the range of a node vs. next node
    var root = 0;
    
    // skip the thread names
    var buildThreadId = Int.max
    while events[sortedIndices[root]].ph! == "M" {
        buildThreadId = min(buildThreadId, events[sortedIndices[root]].tid!)
        root += 1
    }
    
    var evRootIndex = sortedIndices[root]
    var evRoot = events[evRootIndex];
    events[evRootIndex].parentIndex = Int(-1)
    
    for i in (root+1)..<events.count {
        let ev2Index = sortedIndices[i]
        let ev2 = events[ev2Index]
        
        // This only works on a per thread basis, but build events only have a single thread
        // annoying that it's not tid == 0.  So this is the build event specific code.  Would
        // need to run this logic for each unique tid in the events array.
        // TODO: generalize so can use on perf effects.  Memory doesn't need this call.
        if ev2.tid != buildThreadId { continue }
        
        // walk up the stack of parents, to find one that this event is within
        while root != -1 {
            
            // add slice if within bounds
            if ev2.ts! >= evRoot.ts! && ev2.ts!+ev2.dur! <= evRoot.ts!+evRoot.dur! {
                events[ev2Index].parentIndex = Int(root)
                
                // Could store children for full hierarchy, but don't need this
                //evRoot.children.append(evt2Index);
                
                // All flamegraph really needs is for events to store a level
                // of how deep they are on a given thread.  Having to make this up is kinda costly.
                
                // Can create selfTime by subtracting durations of all children
                // if the name matches (so Source from Source)
                if ev2.name == evRoot.name {
                    if events[evRootIndex].durSub == nil {
                        events[evRootIndex].durSub = Int() // 0
                    }
                    events[evRootIndex].durSub! += ev2.dur!
                }
                break;
            }

            // walk up to the next parent
            root = evRoot.parentIndex!
            if root != -1 {
                evRootIndex = sortedIndices[root]
                evRoot = events[evRootIndex]
            }
        }
        if root == -1 {
            events[ev2Index].parentIndex = -1
        }
        
        // move the root to the one we found
        root = i;
        evRootIndex = sortedIndices[i]
        evRoot = events[evRootIndex]
    }
}

// Fire this off any time the list changes and there
// are build events in it.  This will update the data within,
// so that the user doesn't have to visit every file manually.

// TODO: move to timer class
var kTickToSeconds = 0.0

func updateTimebase() {
    if kTickToSeconds != 0.0 { return }
    
    var machTimebase = mach_timebase_info(numer: 0, denom: 0)
    mach_timebase_info(&machTimebase)
    kTickToSeconds = 1e-9 * Double(machTimebase.numer) / Double(machTimebase.denom) // 125/3
}

func updateBuildTimingsTask(_ files: [File]) {
    // Can use counter for progress.  Could add up size instead of just count.
    var counter = 0
    for file in files {
        if !file.buildTimings.isEmpty { return }
        
        counter += 1
    }
    
    if counter == 0 { return }
    
    
    updateTimebase()
    
    let _ = Task(priority: .medium, operation: {
        var time = -Double(mach_absolute_time()) * kTickToSeconds
        
        for file in files {
            if file.fileType == .Build {
                do {
                    try updateBuildTimingTask(file)
                }
                catch {
                    log.error(error.localizedDescription)
                }
            }
        }
        
        time += Double(mach_absolute_time()) * kTickToSeconds
        log.info("finished updating build timings in \(double:time, decimals:3)s")
    })
}
    
func updateBuildTimingTask(_ file: File) throws {
    assert(file.fileType == .Build)
    
    // only checking this, and not duration == 0
    if !file.buildTimings.isEmpty { return }
    
    let fileContent = loadFileContent(file)
    
    var json : Data
    
    if file.containerType == .Compressed {
        guard let unzippedContent = fileContent.gunzip() else {
            return
        }
        json = unzippedContent
    }
    else if file.containerType == .Archive {
        // this has already been extracted and decrypted
        json = fileContent
    }
    else {
        json = fileContent
    }
    
    let decoder = JSONDecoder()
    let catapultProfile = try decoder.decode(CatapultProfile.self, from: json)
    if catapultProfile.traceEvents == nil { // an array
        return
    }
    
    var events = catapultProfile.traceEvents!
    
    // demangle the OptFunction name
    for i in 0..<events.count {
        let event = events[i]
        if event.name == "OptFunction" {
            let detail = event.args!["detail"]!.value as! String
            let symbolName = String(cString: demangleSymbolName(detail))
            
            events[i].args!["detail"] = AnyCodable(symbolName)
        }
    }
    
    // walk the file and compute the duration if we don't already have it
    if file.duration == 0.0 {
        file.duration = updateDuration(events)
    }
    
    // Do this before the names are replaced below
    if file.buildTimings.isEmpty {
        file.buildStats = generateStatsForTotalTrack(events)
        
        // update the durSub in the events, can then track self time
        computeEventParentsAndDurSub(&events)
        
        file.buildTimings = updateFileBuildTimings(events)
    }
}


/* These are types CBA is looking at.  It's not looking at any totals
 DebugType isn't in this.
 
 if (StrEqual(name, "ExecuteCompiler"))
 event.type = BuildEventType::kCompiler;
 else if (StrEqual(name, "Frontend"))
 event.type = BuildEventType::kFrontend;
 else if (StrEqual(name, "Backend"))
 event.type = BuildEventType::kBackend;
 else if (StrEqual(name, "Source"))
 event.type = BuildEventType::kParseFile;
 else if (StrEqual(name, "ParseTemplate"))
 event.type = BuildEventType::kParseTemplate;
 else if (StrEqual(name, "ParseClass"))
 event.type = BuildEventType::kParseClass;
 else if (StrEqual(name, "InstantiateClass"))
 event.type = BuildEventType::kInstantiateClass;
 else if (StrEqual(name, "InstantiateFunction"))
 event.type = BuildEventType::kInstantiateFunction;
 else if (StrEqual(name, "OptModule"))
 event.type = BuildEventType::kOptModule;
 else if (StrEqual(name, "OptFunction"))
 event.type = BuildEventType::kOptFunction;
 
 // here are totals that are in the file
 // Total ExecuteCompiler = Total Frontend + Total Backend
 // 2 frontend blocks though,
 //  1. Source, InstantiateFunction, CodeGenFunction, ...
 //  2. CodeGenFunction, DebugType, and big gaps
 //
 // 1 backend block
 //   OptModule
 
 "Total ExecuteCompiler" <- important (total of frontend + backend)
 
 //----------
 // frontend
 "Total Frontend" <- total
 
 "Total Source" <- parsing
 "Total ParseClass"
 "Total InstantiateClass"
 
 "Total PerformPendingInstantiations"
 "Total InstantiateFunction"
 "InstantiateClass"
 "Total InstantiatePass"
 
 "CodeGen Function"
 "Debug Type"
 
 //----------
 // backend
 "Total Backend" <- total, usually all OptModule
 "Total Optimizer"
 "Total CodeGenPasses"
 "Total OptModule" <- important, time to optimize source file
 "Total OptFunction"
 "Total RunPass"
 
 */

// add a single track with hierarchical totals
func generateStatsForTotalTrack(_ events: [CatapultEvent]) -> BuildStats {
    let stats = BuildStats()
    
    // useful totals to track, many more in the files
    for i in 0..<events.count {
        let event = events[i]
        
        // total
        if event.name == "Total ExecuteCompiler" {
            stats.totalExecuteCompiler = event.dur!
        }
        
        // frontend
        else if event.name == "Total Frontend" {
            stats.totalFrontend = event.dur!
        }
        else if event.name == "Frontend" {
            stats.frontendStart = min(stats.frontendStart, event.ts!)
        }
        
        else if event.name == "Total Source" {
            stats.totalSource = event.dur!
        }
        else if event.name == "Total InstantiateFunction" {
            stats.totalInstantiateFunction = event.dur!
        }
        else if event.name == "Total InstantiateClass" {
            stats.totalInstantiateClass = event.dur!
        }
        else if event.name == "Total CodeGen Function" {
            stats.totalCodeGenFunction = event.dur!
        }
        
        // backend
        else if event.name == "Total Backend" {
            stats.totalBackend = event.dur!
        }
        else if event.name == "Backend" {
            stats.backendStart = min(stats.backendStart, event.ts!)
        }
        else if event.name == "Total Optimizer" {
            stats.totalOptimizer = event.dur!
        }
        else if event.name == "Total CodeGenPasses" {
            stats.totalCodeGenPasses = event.dur!
        }
        else if event.name == "Total OptFunction" {
            stats.totalOptFunction = event.dur!
        }
    }
    
    // fix these up in case they're missing
    if stats.frontendStart == Int.max {
        stats.frontendStart = 0
    }
    
    stats.backendStart = max(stats.backendStart, stats.frontendStart + stats.totalFrontend)
    
    return stats
}

func convertStatsToTotalTrack(_ stats: BuildStats) -> [CatapultEvent] {
    
    var totalEvents: [CatapultEvent] = []
    
    // This is really ugly, change to using class?
    
    let tid = 0
    let trackEvent = CatapultEvent(tid: tid, threadName: "Build Totals")
    totalEvents.append(trackEvent)
    
    // This is a struct, so can modify copy and add
    var event: CatapultEvent
    
    func makeDurEvent(_ tid: Int, _ name: String, _ dur: Int, _ total: Int) -> CatapultEvent {
        let percent = 100.0 * Double(dur) / Double(total)
        return CatapultEvent(tid, "\(name) \(double:percent, decimals:0)%", dur)
    }
    let total = stats.totalExecuteCompiler
    
    event = makeDurEvent(tid, "Total ExecuteCompiler", stats.totalExecuteCompiler, total)
    totalEvents.append(event)
    
    event = makeDurEvent(tid, "Total Frontend", stats.totalFrontend, total)
    event.ts = stats.frontendStart
    totalEvents.append(event)
    
    // sub-areas of frontend
    event = makeDurEvent(tid, "Total Source", stats.totalSource, total)
    event.ts = stats.frontendStart
    totalEvents.append(event)
    
    event = makeDurEvent(tid, "Total InstantiateFunction", stats.totalInstantiateFunction, total)
    event.ts = stats.frontendStart + stats.totalSource
    totalEvents.append(event)
    
    // This is nearly always bigger than its parent total 14% vs. 16%, so are totals wrong
    var totalInstantiateClass = stats.totalInstantiateClass
    if totalInstantiateClass > stats.totalInstantiateFunction {
        totalInstantiateClass = stats.totalInstantiateFunction
    }
    
    // This overlaps with some Source, and some InstantiateFunction, so it's sort of double
    // counted, so clamp it for now so Perfetto doesn't freak out and get the event order wrong.
    event = makeDurEvent(tid, "Total InstantiateClass", totalInstantiateClass, total)
    event.ts = stats.frontendStart + stats.totalSource
    totalEvents.append(event)
    
    
    // This total can exceed when backend start, so clamp it too
    let tsCodeGenFunction = stats.frontendStart + stats.totalSource + stats.totalInstantiateFunction
    
    var totalCodeGenFunction = stats.totalCodeGenFunction
    if tsCodeGenFunction + totalCodeGenFunction > stats.backendStart {
        totalCodeGenFunction = stats.backendStart - tsCodeGenFunction
    }
    
    event = makeDurEvent(tid, "Total CodeGen Function", totalCodeGenFunction, total)
    event.ts = tsCodeGenFunction
    totalEvents.append(event)
    
    // backend
    event = makeDurEvent(tid, "Total Backend", stats.totalBackend, total)
    event.ts = stats.backendStart
    totalEvents.append(event)
    
    event = makeDurEvent(tid, "Total Optimizer", stats.totalOptimizer, total)
    event.ts = stats.backendStart
    totalEvents.append(event)
    
    // event = makeDurEvent(tid, "Total OptModule", stats.totalOptModule, total)
    // event.ts = stats.backendStart + stats.totalOptimizer
    // totalEvents.append(event)
    
    event = makeDurEvent(tid, "Total CodeGenPasses", stats.totalCodeGenPasses, total)
    event.ts = stats.backendStart + stats.totalOptimizer
    totalEvents.append(event)
    
    event = makeDurEvent(tid, "Total OptFunction", stats.totalOptFunction, total)
    event.ts = stats.backendStart + stats.totalOptimizer
    totalEvents.append(event)
    
    /*
    "Total ExecuteCompiler"
    "Total Frontend"
      "Total Source"
      "Total InstantiateFunction"
        "Total InstantiateClass"
      "Total Codegen Function"


    "Total Backend"
      "Total Optimizer"
      "Total CodeGenPasses"
        "Total OptModule"
          "Total OptFunction"
     */
    
    return totalEvents
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
        
        // decompress archive from zip, since Perfetto can't yet decompress zip
        // Note this will typically be fileType unknown, but have to flatten
        // content within to the list.  This just means part of a zip archive.
        let fileContent = loadFileContent(file)
        
        let isBuildFile = file.fileType == .Build
        
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
            
                    file.duration = updateDuration(catapultProfile.traceEvents!)
                    
                    // For now, just log the per-thread info
                    if file.fileType == .Memory {
                        updateThreadInfo(catapultProfile, &file)
                    }
                    
                    // This mods the catapult profile to store parentIndex and durSub
                    // the call has build specific code right now
                    //else if file.fileType == .Perf {
                    //    computeEventParentsAndDurSub(&catapultProfile)
                    //}
                }
            }
        }
        else {
            // The data for these is being generated in an async task
            // So that the build report can be generated.
            
            // Clang has some build totals as durations on fake threads
            // but those are smaller than the full duration.
            var json : Data
            
            if file.containerType == .Compressed {
                guard let unzippedContent = fileContent.gunzip() else {
                    return nil
                }
                json = unzippedContent
            }
            else if file.containerType == .Archive {
                // this has already been extracted and decrypted
                json = fileContent
            }
            else {
                json = fileContent
            }
            
            // here having to ungzip and decode just to display the content
            // have already processed the build files in an async task
            let decoder = JSONDecoder()
            var catapultProfile = try decoder.decode(CatapultProfile.self, from: json)
            
            if catapultProfile.traceEvents == nil {
                return nil
            }
            
            // demangle the OptFunction name
            for i in 0..<catapultProfile.traceEvents!.count {
                let event = catapultProfile.traceEvents![i]
                if event.name == "OptFunction" {
                    let detail = event.args!["detail"]!.value as! String
                    let symbolName = String(cString: demangleSymbolName(detail))
                    
                    catapultProfile.traceEvents![i].args!["detail"] = AnyCodable(symbolName)
                }
            }
            
            // Replace Source with actual file name on Clang.json files
            // That's just for the parse phase, probably need for optimization
            // phase too.
            for i in 0..<catapultProfile.traceEvents!.count {
                let event = catapultProfile.traceEvents![i]
                if  event.name == "Source" ||
                        event.name == "OptModule"
                {
                    // This is a path
                    let detail = event.args!["detail"]!.value as! String
                    let url = URL(string:detail)!
                    
                    // stupid immutable struct.  Makes this code untempable
                    // maybe can use class instead of struct?
                    catapultProfile.traceEvents![i].name = url.lastPathComponent
                }
                else if event.name == "InstantiateFunction" ||
                            event.name == "InstantiateClass" ||
                            event.name == "OptFunction" ||
                            event.name == "ParseClass" ||
                            event.name == "DebugType" || // these take a while
                            event.name == "CodeGen Function" ||
                            event.name == "RunPass"
                {
                    // This is a symbol name
                    let detail = event.args!["detail"]!.value as! String
                    catapultProfile.traceEvents![i].name = detail
                }
                
                // knock out the pid.  There are "M" events setting the process_name
                // Otherwise, display will collapse pid sections since totalTrack has no pid
                catapultProfile.traceEvents![i].pid = nil
            }
            
            if file.buildStats != nil {
                let totalEvents = convertStatsToTotalTrack(file.buildStats!)
                
                // combine these onto the end, could remove the individual tracks storing these
                catapultProfile.traceEvents! += totalEvents
            }
            
            let encoder = JSONEncoder()
            let fileContentFixed = try encoder.encode(catapultProfile)
            
            // gzip compress the data before sending it over
            guard let compressedData = fileContentFixed.gzip() else { return nil }
            fileContentBase64 = compressedData.base64EncodedString()
        }
        
        return postLoadFileJS(fileContentBase64: fileContentBase64, title:fileURL.lastPathComponent)
    }
    catch {
        log.error(error.localizedDescription)
        return nil
    }
}

func postLoadFileJS(fileContentBase64: String, title: String) -> String?
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
     
   
    // What is used when Inter isn't installed.  Can this be bundled?
    //let customFont = Font.custom("Inter Variable", size: 14)
                
    func openFileFromURLs(urls: [URL]) {
        droppedFileCache = urls
        reopenFileFromURLs()
        
        // update the document list
        let documentController = NSDocumentController.shared
        if urls.count >= 1 {
            for url in urls {
                if url.hasDirectoryPath || isSupportedFilename(url) {
                    documentController.noteNewRecentDocumentURL(url)
                }
            }
        }
    }
    
    func reopenFileFromURLs() {
        let urls = droppedFileCache
        
        if urls.count >= 1 {
            let filesNew = listFilesFromURLs(urls)
            
            // for now wipe the old list
            if filesNew.count > 0 {
                // turning this off for now, File must impl Hashable
                let mergeFiles = NSEvent.modifierFlags.contains(.option);

                if mergeFiles {
                    fileSearcher.files = Array(Set(fileSearcher.files + filesNew))
                }
                else
                {
                    // reset the list
                    fileSearcher.files = filesNew
                }
                
                fileSearcher.updateFilesSorted()
                
                // task to update any build timings
                // this saves having to manually visit every file
                updateBuildTimingsTask(fileSearcher.files)
                
                log.debug("found \(fileSearcher.files.count) files")
                
                // preserve the original selection if still present
                if selection != nil {
                    var found = false
                    for file in fileSearcher.filesSorted {
                        if file.id == selection {
                            found = true
                            break;
                        }
                    }
                    
                    // load first file in the list
                    if !found {
                        selection = fileSearcher.filesSorted[0].id
                    }
                }
                else {
                    // load first file in the list
                    selection = fileSearcher.filesSorted[0].id
                }
            }
        }
    }
    
    // This isn't so valuable to open a file, but opening a referenced header from build
    // would be.  But would to respond to/retrieve selection in JS side.
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
    
    let fileTypes: [UTType] = [
        // This is what macOS generates when doing "compress file".  But could be archive.
        // These are 12x smaller often times.  Decompression lib only handles zlib.
        .zip,
       
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
        
        // TOOD: fix this for search, where the selected item may no longer be
        // in the list, find element in the list bounding it
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
    // WindowGroup is supposed to make new state, but doesn't.
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
    
    func recentDocumentsMenu() -> some View {
        let documentController = NSDocumentController.shared
        let urls = documentController.recentDocumentURLs
        
        return Menu("Recent Documents…") {
            ForEach(0..<urls.count, id: \.self) { i in
                Button(urls[i].lastPathComponent) {
                    openFileFromURLs(urls: [urls[i]])
                }
            }
        }
    }
    
    var body: some Scene {
        
        // TODO: WindowGroup brings up old windows which isn't really what I want
    
        Window("Main", id: "main") {
        //WindowGroup() {
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
                                .frame(maxWidth: .infinity, alignment: .leading)
                            
                            Text(generateDuration(file: file))
                                .frame(maxWidth: 70, alignment: .trailing)
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
                        
                        // add other buttons
                    }
                    WebView(webView: myWebView)
                        .focused($focusedField, equals: .webView)
                        .focusable()
                }
            }
            
            // This only seems to display with List, not with Table.  Or it's under Table
            // This also keeps stealing focus back to itself.  Ugh.  Can't even tab out.
            // Also this interferes with type search in the list and arrows when focused.
            // Can select a list item with cursor, but then focus is set back to search.
            // .searchableOptional(text: $fileSearcher.searchText, isPresented: $fileSearcher.searchIsActive, placement: .sidebar, prompt: "Filter")
            
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
            // These are in Edit menu
            CommandGroup(after: .newItem) {
                Button("Open…") {
                    openFile()
                }
                .keyboardShortcut("O", modifiers:[.command])
                
                recentDocumentsMenu()
                
                // Really want to go to .h selected in flamegraph, but that would violate sandbox.
                // This just goes to the trace json file somewhere in DerviceData which is less useful.
                // For selected file can only put on clipboard.
                Button("Go to File") {
                    if selection != nil {
                        openContainingFolder(selection!);
                    }
                }
                .keyboardShortcut("G", modifiers:[.command])
            
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
                
                Divider()
                
                Button("Sort Name") {
                    fileSearcher.updateFilesSorted(false)
                }
                .keyboardShortcut("T", modifiers:[.shift, .command])
                
                Button("Sort Range") {
                    fileSearcher.updateFilesSorted(true)
                }
                .keyboardShortcut("T", modifiers:[.command])
                
                Divider()
                
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
            
            // only way to get non-empty View menu, and for fn+F to automagically add (fn+f)
            // https://forums.developer.apple.com/forums/thread/740591
            
            CommandGroup(after: .toolbar) {
                // TODO: only enable if build files are present
                Button("Build Report All") {
                    let buildFiles = fileSearcher.files
                    let buildJS = postBuildTimingsReport(files: buildFiles)
                    if buildJS != nil {
                        runJavascript(myWebView, buildJS!)
                    }
                }
                .disabled(selection == nil)
                
                Button("Build Report Selected") {
                    let buildFiles = findFilesForBuildTimings(files: fileSearcher.files, selection: selection!)
                    let buildJS = postBuildTimingsReport(files: buildFiles)
                    if buildJS != nil {
                        runJavascript(myWebView, buildJS!)
                    }
                }
                .disabled(selection == nil)
                
                
                // must call through NSWindow
                Button("See Below") {
                    // Window isn't set in AppDelegate, so menu item is skipped.
                    // But add fn+F menu item into app.  So many stupid hacks.
                    // Can also go NSApp.windows.first, but not good w/multiple windows.
                    
                    appDelegate.window?.toggleFullScreen(nil)
                }
            }
            
            CommandGroup(replacing: .appInfo) {
                Button("About kram-profile") {
                    aboutPanel()
                }
            }
        }
    }
}

