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

// TODO: add divider in some sort modes
// TODO: add sort mode for name, time and incorporating dir or not
// TODO: fix the js wait on the page to load before doing loadFile

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

struct File: Identifiable, Hashable, Equatable, Comparable
{
    var id: String { url.absoluteString }
    var name: String { url.lastPathComponent }
    let url: URL
    let shortDirectory: String
    
    var duration: Double?
    var modStamp: Date?
    
    init(url: URL) {
        self.url = url
        self.modStamp = fileModificationDate(url:url)
        self.shortDirectory = buildShortDirectory(url:url)
    }
    
    public static func == (lhs: File, rhs: File) -> Bool {
        return lhs.id == rhs.id
    }
    static func < (lhs: File, rhs: File) -> Bool {
        return lhs.id < rhs.id
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

// Note: if a file is deleted which happens often with builds,
// then want to identify that and update the list.  At least
// indicate the item is gone, and await its return.

var fileCache : [URL:File] = [:]

func lookupFile(url: URL) -> File {
    let file = File(url:url)
    
    // This preseves the duration previously parsed and stored
    
    if let fileOld = fileCache[file.url] {
        if fileOld.modStamp! == file.modStamp! {
            return fileOld
        }
    }
    fileCache[file.url] = file
    
    return file
}

// This one won't be one in the list, though
func updateFileCache(file: File) {
    fileCache[file.url] = file
}
        
struct MyWKWebView : NSViewRepresentable {
    // This is set by caller to the url for the request
    let webView: WKWebView
    let request: URLRequest
    let selection: String?
    let firstRequest: Bool
    
    func makeNSView(context: Context) -> WKWebView {
        // TODO: hook this up, but need WKScriptMessageHandler protocl
        //configuration.userContentController.addScriptMessageHandler(..., name: "postMessageListener")
        return webView
    }
    
    func userContentController(_ userContentController: WKUserContentController, didReceive message: WKScriptMessage) {
       if message.name == "postMessageListener" {
           // Manage your Data
       }
    }
    
    // two copies of this function
    func runJavascript(_ webView: WKWebView, _ script: String) {
        webView.evaluateJavaScript(script) { (result, error) in
            if error != nil {
                print("problem running script")
            }
        }
    }
    
    func updateNSView(_ webView: WKWebView, context: Context) {
        // DONE: skip reloading the request, it doesn't change?
        // TODO: add a reload button for the current selection, since Perfetto can't reload
        
        // The first selection loads, but then subsequent do not
        if firstRequest {
            webView.load(request)
        }
        
        // really need to ping until ui is loaded
        //sleep(2)
        
        if let sel = selection {
            var str = loadFileJS(sel)
            if str != nil {
                runJavascript(webView, str!)
            }
            // now based on the type, set a reasonable range of time
            // don't really want start/end, since we don't know start
            // works for Flutter, but not for this app.  Also would
            // have to parse timeStart/End from file.  May want that for
            // sorting anyways.
            //
            // Note have duration on some files now, so could pull that
            // or adjust the timing range across all known durations
            
            if false {
                str = showTimeRangeJS(filenameToTimeRange(sel))
                if str != nil {
                    runJavascript(webView, str!)
                }
            }
            
        }
    }
}

//#Preview {
//    MyWKWebView()
//}

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

    
// What if the start time in the file isn't 0.0 based for the start
struct TimeRange {
    var timeStart: Double = 0.0
    var timeEnd: Double   = 1.0
        
    // The time range should take up 80% of the visible window.
    var viewPercentage: Double = 0.8
}

enum FileType {
    case Build
    case Memory
    case Perf
}

func filenameToType(_ filename: String) -> FileType {
    let url = URL(string: filename)!

    if url.pathExtension == "json" { // build
        return .Build
    }
    else if url.pathExtension == "vmatrace" { // memory
        return .Memory
    }
    else if url.pathExtension == "trace" { // profile
        return .Perf
    }
    return .Build
}

func filenameToTimeRange(_ filename: String) -> TimeRange {
    var duration = 1.0
    
    switch filenameToType(filename) {
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
            // TODO: this is droppping {} ?
            perfettoEncode = String(encodedString.dropLast().dropFirst())
            perfettoEncode = perfettoEncode.replacingOccurrences(of: "\u{2028}", with: "\\u2028")
                .replacingOccurrences(of: "\u{2029}", with: "\\u2029")
        }
        
        // TODO: this also needs to wait on page to load
        let script = """
            // convert from string -> Uint8Array -> ArrayBuffer
            var obj = JSON.parse('{\(perfettoEncode)}');
        
            window.postMessage(obj,'\(ORIGIN)');
        """
        
        //runJavascript(script)
        return script
    }
    catch {
        print(error)
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
    
    print(path)
    
    
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
    
    do {
        // use this for binary data, but need to fixup some json before it's sent
        // TODO: work on sending a more efficient form.  Could use Perfetto SDK to write to prototbuf.  The Catapult json format is overly verbose.  Need some thread and scope strings, some open/close timings that reference a scope string and thread.
        
        var fileContentBase64 = ""
        
        let type = filenameToType(fileURL.absoluteString)
        
        if type != FileType.Build {
            let fileContent = try Data(contentsOf: fileURL)
            fileContentBase64 = fileContent.base64EncodedString()
            
            // TODO: for perf traces, compute duration between frame
            // markers.  Multiple frames in a file, then show max frame duration
            // instead of the entire file.
            
            // walk the file and compute the duration if we don't already have ti
            if file.duration == nil {
                let decoder = JSONDecoder()
                
                var catapultProfile = try decoder.decode(CatapultProfile.self, from: fileContent)
                
                if catapultProfile.traceEvents != nil {
                    // TODO: need to honor the unit scale
                    var startTime = Double.infinity
                    var endTime = -Double.infinity
                    
                    for i in 0..<catapultProfile.traceEvents!.count {
                        let event = catapultProfile.traceEvents![i]
                        
                        if event.ts != nil && event.dur != nil {
                            let s = Double(event.ts!)
                            let d = Double(event.dur!)
                            
                            startTime = min(startTime, s)
                            endTime = max(endTime, s+d)
                        }
                    }
                    
                    if startTime <= endTime {
                        // TODO: for now assume ms
                        file.duration = (endTime - startTime) * 1e-6
                        
                        updateFileCache(file: file)
                    }
                }
            }
        }
        else {
            // replace Source with actual file name on Clang.json files
            // That's just for the parse phase, probably need for optimization
            // phase too.  The optimized function names need demangled - ugh.
            
            // Clang has come build data as durations on fake threads
            // but those are smaller than the full duration.
            
            let fileContent = try String(contentsOf: fileURL)
            let json = fileContent.data(using: .utf8)!
            
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
                    
                    // TODO: need to honor the unit scale
                    var startTime = Double.infinity
                    var endTime = -Double.infinity
                    
                    for i in 0..<catapultProfile.traceEvents!.count {
                        let event = catapultProfile.traceEvents![i]
                        
                        if event.ts != nil && event.dur != nil {
                            let s = Double(event.ts!)
                            let d = Double(event.dur!)
                            
                            startTime = min(startTime, s)
                            endTime = max(endTime, s+d)
                        }
                    }
                    
                    if startTime <= endTime {
                        // TODO: for now assume ms
                        file.duration = (endTime - startTime) * 1e-6
                        
                        updateFileCache(file: file)
                    }
                }
            }
            
            let encoder = JSONEncoder()
            let fileContentFixed = try encoder.encode(catapultProfile)
            fileContentBase64 = fileContentFixed.base64EncodedString()
        }
        
        let perfetto = Perfetto(perfetto: PerfettoFile(buffer: "",
                                                       title: fileURL.lastPathComponent,
                                                       keepApiOpen: true))
        var perfettoEncode = ""
        
        if true {
            let encoder = JSONEncoder()
            let data = try encoder.encode(perfetto)
            let encodedString = String(decoding: data, as: UTF8.self)
            // TODO: this is droppping {} ?
            perfettoEncode = String(encodedString.dropLast().dropFirst())
            perfettoEncode = perfettoEncode.replacingOccurrences(of: "\u{2028}", with: "\\u2028")
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
        function openTrace(obj)
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
        
        openTrace(obj);
        """
        
        return script
    } catch {
      print(error)
        return nil
    }
}

func initWebView() -> WKWebView {
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
    
    return webView
}



class AppDelegate: NSObject, NSApplicationDelegate {
    // don't rename params in this class. These are the function signature
    func applicationShouldTerminateAfterLastWindowClosed(_ application: NSApplication) -> Bool {
        return true
    }
    
    // May remove this, using openURL instead. User might drag mulitiple
    // files onto app, so need to handle that case too...
    // when double-clicking files in Finder, this is called
//    func application(_ application: NSApplication, open urls: [URL]) {
//        // TODO: can use this insted of defining Document model
//
//    }
}

// Install this just for appFromURL, need to be associate this app with traces
// Instead of these see AppDelegate below.
//struct KramDocument: NSDocument {
//    // when trying to open a file, getting an error that this needed
//    // readFromData:ofType:error: is a subclass responsibility but has not been overridden.
//    func read(from data: Data, ofType typeName: String) throws {
//        // TODO:, but this receives DATA instead of URL?
//        
//        return true // if read was success
//    }
//}

@main
struct kram_profileApp: App /* DropDelegate */ {
    @State private var files: [File] = []
    @State private var selection: String?
    @State private var firstRequest = true
    //@State private var dragOver = false

    // close app when last windowi s
    @NSApplicationDelegateAdaptor private var appDelegate: AppDelegate
    
    private var webView = initWebView()
    
    func runJavascript(_ script: String) {
        webView.evaluateJavaScript(script) { (result, error) in
            if error != nil {
                print("problem running script")
            }
        }
    }
            
    func focusFindTextEdit() {
        let script = """
            window.editText.requestFocus();
        """
        runJavascript(script)
    }
           
    
    // Here
    // https://stackoverflow.com/questions/68583357/stumped-on-drag-and-drop-in-swiftui
    // This doesn't seem to work at all?
    /*
    func performDrop(info: DropInfo) -> Bool {
        
        if !info.hasItemsConforming(to: droppedFileURLTypes) {
           return false
        }

        var urls: [URL] = []

        // This is kind of an annoying api
        let providers = info.itemProviders(for: droppedFileURLTypes)
        for provider in providers {
            for droppedFileURLType in droppedFileURLTypes {
                provider.loadItem(forTypeIdentifier: droppedFileURLType.identifier, options: nil) { (data, error) in
                    if let url = data as? String {
                        // can't filter here, since might be directory
                        urls.append(URL(string: url)!)
                    }
                }
            }
        }
        
        openFilesFromURLs(urls: urls, mergeFiles: false)
        return true
        //return false
    }
    */
    
    func isSupportedFilename(_ url: URL) -> Bool {
      
        // clang build files use genertic .json format
        if url.pathExtension == "json" {
            let filename = url.lastPathComponent
            
            // filter out some by name, so don't have to open files
            if filename == "build-description.json" ||
                filename == "build-request.json" ||
                filename == "manifest.json" ||
                filename.hasSuffix("diagnostic-filename-map.json")
            {
                return false
            }
            return true
        }
        // profiling
        if url.pathExtension == "trace" {
            return true
        }
        // memory
        if url.pathExtension == "vmatrace" {
            return true
        }
        return false
    }
    
    func listFilesFromURLs(_ urls: [URL]) -> [File]
    {
        //print("selected \(url)")
        
        // wipe them all
        // TODO: have mode where the url get added
        // instead of wiping the array out.  Note FileCache
        // has cached duration data
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
                // TODO: list out zip archive
                
                let isSupported = isSupportedFilename(url)
                if isSupported {
                    files.append(lookupFile(url:url))
                }
            }
        }
    
        return files
    }
    
    let durationFont = Font
            .system(size: 12) // not sure what default font size is
            .monospaced()
    
    func openFilesFromURLs(urls: [URL], mergeFiles : Bool = false) {
        if urls.count >= 1 {
            let filesNew = listFilesFromURLs(urls)
            
            // for now wipe the old list
            if filesNew.count > 0 {
                if mergeFiles {
                    // TODO: how does this pick which one to keep
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
                
                print("found \(files.count) files")
                
                // load first file in the list
                // TODO: preserve the original selection if still present
                if !mergeFiles || selection == nil {
                    if files[0].url.isFileURL { selection = files[0].id }
                }
            }
        }
        
        // Not sure where to set this false, so do it here
        // this is to avoid reloading the request
        firstRequest = false
    }
    
    func shortFilename(_ str: String) -> String {
        let url = URL(string: str)!
        return url.lastPathComponent
    }
    
    func openContainingFolder(_ str: String) {
        let url = URL(string: str)!
        NSWorkspace.shared.activateFileViewerSelecting([url]);
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
                        NSAttributedString.Key.font: NSFont.boldSystemFont(
                            ofSize: NSFont.smallSystemFontSize)
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
        // .plainText, .zip
        .json, // clang build files
        UTType(filenameExtension:"trace", conformingTo:.data)!,
        UTType(filenameExtension:"vmatrace", conformingTo:.data)!,
    ]
    
    // TODO: have
    let droppedFileURLTypes = [
        UTType.fileURL, // "public.file-url"
        // This isn't a url
        // UTType.folder,  // public.folder
    ]
    
    var body: some Scene {
        WindowGroup {
            NavigationSplitView {
                VStack {
                    List(files, selection:$selection) { file in
                        // compare to previous file (use active sort comparator)
                        // if it differs, then toggle the button bg colors
//                        if lastUrl && url.path != lastUrl!.path {
//                            files.append(Divider())
//                        }
                        
                        HStack() {
                            Text(generateDuration(file: file))
                                .frame(maxWidth: 70)
                                .font(durationFont)
                            // name gets truncated too soo if it's first
                            // and try to align the text with trailing
                            Text(generateName(file: file))
                                .help(file.shortDirectory)
                                .truncationMode(.tail)
                        }
                    }
                }
            }
            detail: {
                // About hideSidebar
                // https://github.com/google/perfetto/issues/716
                MyWKWebView(webView:webView, request: URLRequest(url:URL(string: ORIGIN + "/?hideSidebar=true")!), selection:selection, firstRequest:firstRequest)
                
            }
            // TODO: show data, and selected file here
            .navigationTitle(selection != nil ? shortFilename(selection!) : "")
            .onOpenURL { url in
                // TODO: this isn't called, find out why?
                print(url)
                
                openFilesFromURLs(urls: [url])
            }
            .dropDestination(for: URL.self) { (items, _) in
                // This acutally works!
                openFilesFromURLs(urls: items, mergeFiles: false)
                                return true
            }
            /*
             //.onDrop(of:droppedFileURLTypes, delegate:self)
             // handle drop of files onto app
             // actually want the url here, so may want to change fileTypes to URL
             .onDrop(of:droppedFileURLTypes, isTargeted: $dragOver) { providers in
                // https://stackoverflow.com/questions/60831260/swiftui-drag-and-drop-files
                var urls: [URL] = []
                    
                // TODO: may need to switch to url or file+dirURL
                    for urlProvider in providers {
                        for droppedFileURLType in droppedFileURLTypes {
                            //urlProvider.loadItem(forTypeIdentifier: droppedFileURLType.identifier, options: nil) { (data, error) in
                            
                            // This is async
                           urlProvider.loadDataRepresentation(forTypeIdentifier:
                                                        droppedFileURLType.identifier, completionHandler: { (data, error) in
                            if error == nil {
                
                                let urlString = String(decoding: data!, as: UTF8.self)
                                let url = URL(string: urlString)!
                                
                                // can't filter here, since might be directory
                                urls.append(url)
                            }
                            else {
                                // getting case of 7 drops, but only get 6 or less
                                print(error)
                            })
                        }
                    }
                }
               
            
                // sometimes gets called with 1-5 files on a drop of 7
                // This seems to be a Swift bug.  The urls have 7 here, and in the call have 6.
                openFilesFromURLs(urls: urls, mergeFiles: false)
                return true
            }
             
            .border(dragOver ? Color.green : Color.clear)
             */
        }
        // https://nilcoalescing.com/blog/CustomiseAboutPanelOnMacOSInSwiftUI/
        .commands {
            CommandGroup(after: .newItem) {
                Button("Open File") {
                    openFile()
                }
                .keyboardShortcut("O")
                
                Button("Go to File") {
                    if selection != nil {
                        openContainingFolder(selection!);
                    }
                }
                .keyboardShortcut("G")
                
                // TODO: make it easy to focus the editText in the Pefetto view
//                Button("Find") {
//                    if selection != nil {
//                        focusFindTextEdit();
//                    }
//                }
//                .keyboardShortcut("F")
            }
            CommandGroup(replacing: .appInfo) {
                Button("About kram-profile") {
                    aboutPanel()
                }
            }
        }
    }
    
    // https://stackoverflow.com/questions/49882933/pass-jsonobject-from-swift4-to-wkwebview-javascript-function
    
    // all need a list of files, and File open dialog
    // For a user folder, find all clang.json files, .trace files, and vma.json files
    // and then need to send the selected one over to MyWKWebView so it can open
    // the data from posting a message to the page.
}

