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
// TODO: add sort mode for name, time and incorporating dir or not
// TODO: fix the js wait, even with listener, there's still a race
//    maybe there's some ServiceWorker still loading the previous json?
//    Perfetto is using a ServiceWorker, Safari uses those now, and ping/pong unware.
// TODO: still getting race condition.  Perfetto is trying to
//  load the previous file, and we’re sending a new one.
// TODO: update recent document list
// TODO: have a way to reload dropped folder (not just subfiles)
// TODO: nav title and list item text is set before duration is computed
//  need some way to update that.
// TODO: support WindowGroup and multiwindow, each needs own webView, problem
//   is that onOpenURL opens a new window always.
// DONE: fn+F doesn't honor fullscreen
// TODO: be nice to focus the search input on cmd+F just to make me happy.
//  Browser goes to its own search which doesn’t help.
// TODO: work on sending a more efficient form.  Could use Perfetto SDK to write to prototbuf.  The Catapult json format is overly verbose.  Need some thread and scope strings, some open/close timings that reference a scope string and thread.
// DONE: Perfetto can only read .gz files, and not .zip files.
//   But could decode zip files here, and send over gz compressed.
//   Would need to idenfity zip archive > 1 file vs. zip single file.
// DONE: add gz compression to all file data.  Use libCompression
//   but it only has zlib compression.  Use DataCompression which
//   messages zlib deflate to gzip.
// TODO: switch font to Inter, bundle that with the app?
//   .environment(\.font, Font.custom("CustomFont", size: 14))
// TODO: for perf traces, compute duration between frame
//   markers.  Multiple frames in a file, then show max frame duration
//   instead of the entire file.
// TODO: can't type ahead search in the list while the webview is loading (f.e. e will advance)
//    but arrow keys work to move to next
// TODO: if list hidden, then can't advance
// TODO: can't overide "delete" key doing a back in the WKWebView history
//    Perfetto warns that content will be lost
// TODO: track duration would be useful (esp. for memory traces)
//    Would have to modify the thread_name, and process the tid and timings
//    Better if Perfetto could display this
// TODO: list view type ahead search doesn't work unless name is the first Text entry
// TODO track when files change or get deleted, update the list item then
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

private let log = Log("kram-profile")


func fileModificationDate(url: URL) -> Date? {
    do {
        let attr = try FileManager.default.attributesOfItem(atPath: url.path)
        return attr[FileAttributeKey.modificationDate] as? Date
    } catch {
        return nil
    }
}

func buildShortDirectory(url: URL) -> String {
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

// TODO: may want to make a class.
struct File: Identifiable, Hashable, Equatable, Comparable
{
    var id: String { url.absoluteString }
    var name: String { url.lastPathComponent }
    let url: URL
    let shortDirectory: String
    
    var duration: Double?
    var modStamp: Date?
    var loadStamp: Date?
    
    init(url: URL) {
        self.url = url
        self.modStamp = fileModificationDate(url:url)
        self.shortDirectory = buildShortDirectory(url:url)
    }
    
    public static func == (lhs: File, rhs: File) -> Bool {
        return lhs.id == rhs.id
    }
    public static func < (lhs: File, rhs: File) -> Bool {
        return lhs.id < rhs.id
    }
    
    // call this when the file is loaded
    public mutating func setLoadStamp()  {
        loadStamp = modStamp
    }
    public func isReloadNeeded() -> Bool {
        return modStamp != loadStamp
    }
}

func generateName(file: File) -> String {
    // need to do lookup to get duration
    let f = lookupFile(url: file.url)
    return f.name
}

func generateDuration(file: File) -> String {
    let f = lookupFile(url: file.url)
    if f.duration != nil {
        return String(format:"%0.3f", f.duration!) // sec vis to ms for now
    }
    else {
        return ""
    }
}

func generateNavigationTitle(_ str: String?) -> String {
    if str == nil {
        return ""
    }
    
    let f = lookupFile(url: URL(string:str!)!)
    return generateDuration(file: f) + " " + generateName(file: f)
}

// Note: if a file is deleted which happens often with builds,
// then want to identify that and update the list.  At least
// indicate the item is gone, and await its return.
// Does macOS have a FileWatcher?

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

// This one won't be one in the list, though
func updateFileCache(file: File) {
    fileCache[file.url] = file
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
    let webView = WKWebView(frame: .zero, configuration: configuration)
    
    // The page is complaining it's going to lose the data if fwd/back hit
    webView.allowsBackForwardNavigationGestures = false
   
    webView.load(request)
    return webView
}

// This is just an adaptor to allow WkWebView to interop with SwiftUI.
// It's unclear if WindowGroup can even have this hold state.
struct MyWKWebView : NSViewRepresentable {
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

// has to be https to work for some reason, but all data is previewed locally
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
                        .sidebar, prompt: prompt)
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

enum FileType {
    case Archive // zip of 1+ files, can't enforce
    case Compressed // gzip of 1 file, can't enforce
    
    case Build
    case Memory
    case Perf
}

func filenameToType(_ filename: String) -> FileType {
    let url = URL(string: filename)!
    let ext = url.pathExtension
    
    if ext == "zip" {
        return .Archive
    }
    if ext == "gz" {
        return .Compressed
    }
    
    if ext == "json" { // build
        return .Build
    }
    else if ext == "vmatrace" { // memory
        return .Memory
    }
    else if ext == "trace" { // profile
        return .Perf
    }
    return .Build
}

func filenameToTimeRange(_ filename: String) -> TimeRange {
    var duration = 1.0
    
    switch filenameToType(filename) {
        case .Archive: fallthrough
        case .Compressed: fallthrough
        case .Build: duration = 1.0
        case .Memory: duration = 64.0
        case .Perf: duration = 0.1 // 100ms
    }
    
    return TimeRange(timeStart:0.0, timeEnd:duration)
}

// Flutter uses this to jump to a time range
func showTimeRangeJS(_ timeRange: TimeRange) -> String? {

    do {
        struct PerfettoTimeRange: Codable {
            // Pass the values to Perfetto in seconds.
            var timeStart: Double // in seconds
            var timeEnd: Double
                
            // The time range should take up 80% of the visible window.
            var viewPercentage: Double
        }
        
        struct Perfetto: Codable {
            var perfetto: PerfettoTimeRange
        }
        
        let perfetto = Perfetto(perfetto:PerfettoTimeRange(timeStart: timeRange.timeStart, timeEnd: timeRange.timeEnd, viewPercentage:timeRange.viewPercentage))
        
        var perfettoEncode = ""
        
        if true {
            let encoder = JSONEncoder()
            let data = try encoder.encode(perfetto)
            let encodedString = String(decoding: data, as: UTF8.self)
            perfettoEncode = String(encodedString.dropLast().dropFirst())
            perfettoEncode = perfettoEncode.replacingOccurrences(of: "\u{2028}", with: "\\u2028")
                .replacingOccurrences(of: "\u{2029}", with: "\\u2029")
        }
        
        let script = """
            // convert from string -> Uint8Array -> ArrayBuffer
            var obj = JSON.parse('{\(perfettoEncode)}');
        
            // https://jsfiddle.net/vrsofx1p/
            function waitForUI(obj)
            {
                // have already opened and loaded the inwdow
                const win = window; // .open('\(ORIGIN)');
                if (!win) { return; }
            
                const timer = setInterval(() => win.postMessage('PING', '\(ORIGIN)'), 50);
                
                const onMessageHandler = (evt) => {
                    if (evt.data !== 'PONG') return;
                    
                    // We got a PONG, the UI is ready.
                    window.clearInterval(timer);
                    window.removeEventListener('message', onMessageHandler);
                    
                    // was win, but use window instead
                    win.postMessage(obj,'\(ORIGIN)');
                }
            
                window.addEventListener('message', onMessageHandler);
            }
                
            waitForUI(obj);
        """
        
        return script
    }
    catch {
        log.error(error.localizedDescription)
        return nil
    }
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

func loadFileJS(_ path: String) -> String? {
    
    let fileURL = URL(string: path)!
    
    // Note may need to modify directly
    var file = lookupFile(url: fileURL)
    
    log.debug(path)
    
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
    
    do {
        // use this for binary data, but need to fixup some json before it's sent
        var fileContentBase64 = ""
        
        let type = filenameToType(fileURL.absoluteString)
        
        let isFileGzip = type == .Compressed
        //let isFileZip = type == .Archive
        
        // Note: json.gz and json.zip build files are not marked as Build
        // but need to rewrite them.
        var isBuildFile = type == FileType.Build
        
        let filename = fileURL.lastPathComponent
        
        if filename.hasSuffix("json.gz") || filename.hasSuffix("json.zip") {
            isBuildFile = true
        }
        
        var fileContent = try Data(contentsOf: fileURL)
        
        // decompress archive from zip, since Perfetto can't yet decompress zip
        if type == .Archive {
            // unzlib is for a zlib file and not a zip archive,
            // so need new call.  Have this in kram as C++ helper.
            // This is unzlib() to avoid confusion.
            //if guard let unzippedContent = fileContent.unzlib() else {
                return nil
            //}
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
                let firstSixChars = fileContentBase64.prefix(6)
                let isJson = firstSixChars == jsonDetector
                
                // convert to gzip format, so send less data across to Safari
                if doCompress {
                    guard let compressedData: Data = fileContent.gzip() else { return nil }
                    fileContentBase64 = compressedData.base64EncodedString()
                }
                
                // walk the file and compute the duration if we don't already have it
                if isJson && file.duration == nil {
                    let decoder = JSONDecoder()
                    let catapultProfile = try decoder.decode(CatapultProfile.self, from: fileContent)
                    
                    if catapultProfile.traceEvents == nil {
                        return nil
                    }
                    
                    updateDuration(catapultProfile, &file)
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
            
            if type == .Compressed {
                guard let unzippedContent = fileContent.gunzip() else {
                    return nil
                }
                json = unzippedContent
            }
            else if type == .Archive {
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
                                event.name == "ParseClass"
                    {
                        // This is a name
                        let detail = event.args!["detail"]!.value as! String
                        catapultProfile.traceEvents![i].name = detail
                    }
                }
                
                // walk the file and compute the duration if we don't already have ti
                if file.duration == nil {
                    updateDuration(catapultProfile, &file)
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
        
        let perfetto = Perfetto(perfetto: PerfettoFile(buffer: "",
                                                       title: fileURL.lastPathComponent,
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
            // have already opened and loaded the inwdow
            const win = window; // .open('\(ORIGIN)');
            if (!win) { return; }
        
            const timer = setInterval(() => win.postMessage('PING', '\(ORIGIN)'), 50);
            
            const onMessageHandler = (evt) => {
                if (evt.data !== 'PONG') return;
                
                // We got a PONG, the UI is ready.
                window.clearInterval(timer);
                window.removeEventListener('message', onMessageHandler);
                
                // was win, but use window instead
                win.postMessage(obj,'\(ORIGIN)');
            }
        
            window.addEventListener('message', onMessageHandler);
        }
        
        waitForUI(obj);
        """
        
        return script
    } catch {
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
    @State private var files: [File] = []
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
        // should this limit compressed files to the names supported below - json, trace, vmatrace?
        
        if ext == "gz" {
            return true
        }
//        if ext == "zip" {
//            return true
//        }
            
        // clang build files use generic .json format
        if ext == "json" {
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
        if ext == "trace" {
            return true
        }
        // memory
        if ext == "vmatrace" {
            return true
        }
        return false
    }
    
    func listFilesFromURLs(_ urls: [URL]) -> [File]
    {
        var files: [File] = []
       
        for url in urls {
            // now filter a list of all the files under the dir
            if url.hasDirectoryPath {
                // list out all matching files
                // also these [.skipsHiddenFiles, .skipsSubdirectoryDescendants]
                
                // This doesn't default to recursive, see what kram does
                let directoryEnumerator = FileManager.default.enumerator(
                    at: url,
                    includingPropertiesForKeys: nil
                    // options: [.skipsHiddenFiles]
                )
                
                while let fileURL = directoryEnumerator?.nextObject() as? URL {
                    let isSupported = isSupportedFilename(fileURL)
                    if isSupported {
                        files.append(lookupFile(url:fileURL));
                    }
                }
            }
            else if url.isFileURL {
                let isSupported = isSupportedFilename(url)
                if isSupported {
                    files.append(lookupFile(url:url))
                }
            }
        }
    
        return files
    }
    
    // What is used when Inter isn't installed.  Can this be bundled?
    let customFont = Font.custom("Inter Variable", size: 14)
                
    let durationFont =
        Font.custom("Inter Variable", size: 14)
        .monospaced()
    
    func openFilesFromURLs(urls: [URL]) {
        // turning this off for now
        let mergeFiles = false
        
        if urls.count >= 1 {
            let filesNew = listFilesFromURLs(urls)
            
            // for now wipe the old list
            if filesNew.count > 0 {
                if mergeFiles {
                    files = Array(Set(files + filesNew))
                }
                else {
                    // reset the list
                    files = filesNew
                }
                
                // for some reason, their listed out in pretty random order
                // TODO: add different sorts - id, name, size.  id is default
                // which is the full url
                files.sort()
                
                log.debug("found \(files.count) files")
                
                // preserve the original selection if still present
                if selection != nil {
                    var found = false
                    for file in files {
                        if file.id == selection {
                            found = true
                            break;
                        }
                    }
                    
                    // load first file in the list
                    if !found {
                        selection = files[0].id
                    }
                }
                else {
                    // load first file in the list
                    selection = files[0].id
                }
            }
        }
    }
    
    func shortFilename(_ str: String) -> String {
        let url = URL(string: str)!
        return url.lastPathComponent
    }
    
    func openContainingFolder(_ str: String) {
        let url = URL(string: str)!
        NSWorkspace.shared.activateFileViewerSelecting([url]);
    }
    
    func isReloadEnabled(_ selection: String?) -> Bool {
        guard let sel = selection else { return false }
        let file = lookupFile(url:URL(string: sel)!)
        return file.isReloadNeeded()
    }
    
    func openFile() {
        let panel = NSOpenPanel()
        panel.allowsMultipleSelection = true
        panel.canChooseDirectories = true
        panel.canChooseFiles = true
        panel.allowedContentTypes = fileTypes
        
        panel.begin { reponse in
            if reponse == .OK {
                openFilesFromURLs(urls: panel.urls)
            }
        }
    }
    
    func openFileSelection(_ webView: WKWebView) {
        if let sel = selection {
            
            // This should only reload if selection previously loaded
            // to a valid file, or if modstamp changed on current selection
            
            var str = loadFileJS(sel)
            if str != nil {
                runJavascript(webView, str!)
                
                var file = lookupFile(url: URL(string: sel)!)
                file.setLoadStamp()
                updateFileCache(file: file)
            }
            
            // now based on the type, set a reasonable range of time
            // don't really want start/end, since we don't know start
            // works for Flutter, but not for this app.  Also would
            // have to parse timeStart/End from file.  May want that for
            // sorting anyways.
            //
            // Note have duration on files now, so could pull that
            // or adjust the timing range across all known durations
            
            if false {
                str = showTimeRangeJS(filenameToTimeRange(sel))
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
    
    // DONE: have files ending in .vmatrace, .trace, and .json
    // TODO: archives in the zip file.
    
    let fileTypes: [UTType] = [
        // single-file zip, not dealing with archives yet, but have C++ code to
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
        UTType(filenameExtension:"vmatrace", conformingTo:.data)!,
    ]
    
    func selectFile(_ selection: String?, _ advanceList: Bool) -> String? {
        guard let sel = selection else { return nil }
        if files.count == 1 { return selection }
        
        var index = 0
        for i in 0..<files.count {
            let file = files[i]
            if file.id == sel {
                index = i
                break
            }
        }
        
        index = (index + files.count + (advanceList == true ? 1:-1)) % files.count
        return files[index].id
    }
    
    // about hideSideBar
    // https://github.com/google/perfetto/issues/716
    // Allocating here only works for a single Window, not for WindowGroup
    @State var myWebView = newWebView(request: URLRequest(url:URL(string: ORIGIN + "/?hideSidebar=true")!))
    
    enum Field: Hashable {
        case webView
        //case listView
    }
    @FocusState private var focusedField: Field?

    // https://developer.apple.com/documentation/swiftui/adding-a-search-interface-to-your-app
    // can filter list items off this
    // Rename to filterText, ...
    @State private var searchText: String = ""
    @State private var searchIsActive = false
    var searchResults: [File] {
        if searchText.isEmpty {
            return files
        }
        else if searchText.count == 1 {
            // Some items with k in the rest of the name will be filtered
            // but will appear with more characters
            let lowercaseSearchText = searchText.lowercased()
            let uppercaseSearchText = searchText.uppercased()
            
            return files.filter {
                $0.name.starts(with: uppercaseSearchText) ||
                $0.name.starts(with: lowercaseSearchText)
            }
        }
        else {
            // TODO: should be case insensitive
            return files.filter {
                // Here use the name, or else the directory will have say "kram" in it
                $0.name.localizedCaseInsensitiveContains(searchText)
            }
        }
    }

    /* These are also macOS 14, using Extension form
    
    @ViewBuilder func MyNavigationSplitView() -> some View {
        if #available(macOS 14.0, *) {
            NavigationSplitView()
            .searchable(text: $searchText, isPresented: $searchIsActive, placement:
                    .sidebar, prompt: "Filter")
            
            .onKeyPress(.upArrow, action: {
                // don't change focus, just advance the selection
                if focusedField == .webView {
                    //$selection = selectFile(selection, false)
                    return .handled
                }
                return .ignored
            })
            .onKeyPress(.downArrow, action: {
                if focusedField == .webView {
                    //$selection = selectFile(selection, false)
                    return .handled
                }
                return .ignored
            })
            .onKeyPress(.delete, action: {
                // block this so WKWebView doesn't page back
                if focusedField == .webView {
                    // Unlcar if this should be 0 or 1
                    myWebView.go(to: myWebView.backForwardList.item(at:0)!)
                }
                return .handled
            })
            
        }
        else {
            NavigationSplitView()
        }
    }
    */
    
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
    
    var body: some Scene {
        
        // WindowGroup brings up old windows which isn't really what I want
    
        Window("Main", id: "main") {
        //WindowGroup {
            NavigationSplitView() {
                VStack {
                    List(searchResults, selection:$selection) { file in
                        HStack() {
                            // If number is first, then that's all SwiftUI
                            // uses for typeahead list search.  Seems to
                            // be no control over that.
                            
                            // name gets truncated too soon if it's first
                            // and try to align the text with trailing.
                            
                            Text(generateName(file: file))
                                .help(file.shortDirectory)
                                .truncationMode(.tail)
                                
                            
                            Text(generateDuration(file: file))
                                .frame(maxWidth: 70)
                                //.alignment(.trailing)
                                .font(durationFont)
                            
                        }
                        .listRowSeparator(isSeparatorVisible(file, searchResults) ? .visible : .hidden)
                        .listRowSeparatorTint(.white)
                        
                    }
                    //.focused($focusedField, equals: .listView)
                }
                //.focusable(false)
            }
            detail: {
                MyWKWebView(webView: myWebView)
                //.focusable()
                //.focused($focusedField, equals: .webView)
            }
            .searchableOptional(text: $searchText, isPresented: $searchIsActive, placement: .sidebar, prompt: "Filter")
            
            /* this wrapper doesn't work
            // need these macOS 14 calls to advance the list when list is closed
            .onKeyPressOptional(.upArrow, action: {
                if focusedField == .webView {
                    //$selection = selectFile(selection, false)
                    return .handled
                }
                return .ignored
            })
            .onKeyPressOptional(.upArrow, action: {
                if focusedField == .webView {
                    //$selection = selectFile(selection, false)
                    return .handled
                }
                return .ignored
            })
            */
            
            .onChange(of: selection /*, initial: true*/) { newState in
                openFileSelection(myWebView)
                focusedField = .webView
            }
            .navigationTitle(generateNavigationTitle(selection))
            .onOpenURL { url in
                openFilesFromURLs(urls: [url])
                focusedField = .webView
            }
            .dropDestination(for: URL.self) { (items, _) in
                // This acutally works!
                openFilesFromURLs(urls: items)
                focusedField = .webView
                return true
            }
        }
        // This is causing an error on GitHub CI build, but not when building locally
        //.environment(\.font, customFont)
        // https://nilcoalescing.com/blog/CustomiseAboutPanelOnMacOSInSwiftUI/
        .commands {
            CommandGroup(after: .newItem) {
                Button("Open File") {
                    openFile()
                }
                .keyboardShortcut("O")
                
                // Really want to go to .h selected in flamegraph, but that would violate sandbox.
                // This just goes to the trace json file somewhere in DerviceData which is less useful.
                // For selected file can only put on clipboard.
                Button("Go to File") {
                    if selection != nil {
                        openContainingFolder(selection!);
                    }
                }
                .keyboardShortcut("G")
                
                Button("Reload File") {
                    openFileSelection(myWebView)
                }
                .keyboardShortcut("R")
                .disabled(!isReloadEnabled(selection))
                
            
                // TODO: these may need to be attached to detail view
                // The list view eats them, and doesn't fwd onto the web view
                
                // Can see the commands.  Wish I could remap this.
                // https://github.com/google/perfetto/blob/98921c2a0c99fa8e97f5e6c369cc3e16473c695e/ui/src/frontend/app.ts#L718
                // Perfetto Command
                Button("Search") {
                    // Don't need to do anything
                }
                .keyboardShortcut("S")
                .disabled(selection != nil)
                          
                // Perfetto command
                Button("Parse Command") {
                    // don't need to do anything
                }
                .keyboardShortcut("P", modifiers:[.shift, .command])
                .disabled(selection != nil) // what if selection didn't load
            }
            CommandGroup(after: .toolbar) {
                // TODO: this automatically inserts the fn+F menu item
                // which is then redundant with this menu item that lack the shortcut
                Button("Toggle Fullscreen") {
                    Task { @MainActor in
                        NSApplication.shared.windows.last?.toggleFullScreen(nil)
                    }
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

