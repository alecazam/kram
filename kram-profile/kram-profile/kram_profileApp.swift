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
    
    if f.duration != nil {
        return "\(f.name) " + String(format:"%0.3f", f.duration!) // sec vis to ms for now
    }
    else {
        return f.name
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
            loadFile(webView, sel)
        
            // now based on the type, set a reasonable range of time
            // don't really want start/end, since we don't know start
            // works for Flutter, but not for this app.  Also would
            // have to parse timeStart/End from file.  May want that for
            // sorting anyways.
            //
            // Note have duration on some files now, so could pull that
            // or adjust the timing range across all known durations
            // showTimeRange(webView, filenameToTimeRange(sel))
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
func showTimeRange(_ webView: WKWebView, _ timeRange: TimeRange) /*async*/ {

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
        
        let json = """
            // convert from string -> Uint8Array -> ArrayBuffer
            var obj = JSON.parse('{\(perfettoEncode)}');
            window.postMessage(obj,'\(ORIGIN)');
        """
        
        webView.evaluateJavaScript(json) { (result, error) in
            if error != nil {
                print("\(error)! \(result)")
            }
            else {
                print("\(result)")
            }
        }
    }
    catch {
        print(error)
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
    var beginningOfTime: Int?
}

func loadFile(_ webView: WKWebView, _ path: String) /*async*/ {
    
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
        
        // TODO: this works, but can't fixup the json on either end as easily.
        
        var fileContentBase64 = ""
        
        let type = filenameToType(fileURL.absoluteString)
        
        if type != FileType.Build {
            let fileContent = try Data(contentsOf: fileURL)
            fileContentBase64 = fileContent.base64EncodedString()
        }
        else {
            // replace Source with actual file name on Clang.json files
            // That's just for the parse phase, probably need for optimization
            // phase too.  The optimized function names need demangled - ugh.
            
            let fileContent = try String(contentsOf: fileURL)
            let json = fileContent.data(using: .utf8)!
            
            let decoder = JSONDecoder()
            var catapultProfile = try decoder.decode(CatapultProfile.self, from: json)
            
            // trying to change the objects, but it's not applying
            if catapultProfile.traceEvents != nil { // an array
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
        
        let json = """
        
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
    
        window.postMessage(obj,'\(ORIGIN)');
    """
       
    /*
    """
        // this isn't working, to avoid race condition with when perfetto loads
        // and when the loadFile() passes over the file contents.
     
        function openTrace(obj)
        {
            // https://jsfiddle.net/vrsofx1p/
            const timer = setInterval(() => window.postMessage('PING', '\(ORIGIN)'), 50);
            
            const onMessageHandler = (evt) => {
                if (evt.data !== 'PONG') return;
                
                // We got a PONG, the UI is ready.
                window.clearInterval(timer);
                window.removeEventListener('message', onMessageHandler);
                
                window.postMessage(obj,'\(ORIGIN)');
            }
        }
        
        openTrace(obj);
        """
      */
        
        // print(json)
        
        webView.evaluateJavaScript(json) { (result, error) in
            if error != nil {
                print("\(error)! \(result)")
            }
            else {
                print("\(result)")
            }
        }

        
    } catch {
      print(error)
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

@main
struct kram_profileApp: App {
    @State private var files: [File] = []
    @State private var selection: String?
    @State private var firstRequest = true
   
    private var webView = initWebView()
    
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
    
    func listFilesFromURL(_ url: URL) -> [File]
    {
        print("selected \(url)")
        
        // wipe them all
        var files: [File] = []
        
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
        
        // for some reason, their listed out in pretty random order
        files.sort()
        
        print("found \(files.count) files")
        
        return files
    }
    
    func shortFilename(_ str: String) -> String {
        let url = URL(string: str)!
        return url.lastPathComponent
    }
    
    func openContainingFolder(_ str: String) {
        let url = URL(string: str)!
        NSWorkspace.shared.activateFileViewerSelecting([url]);
    }
    
    // DONE: have files ending in .vmatrace, .trace, and .json
    // TODO: archives in the zip file.
    var fileTypes: [UTType] = [
        // .plainText, .zip
        .json, // clang build files
        UTType(filenameExtension:"trace", conformingTo:.data)!,
        UTType(filenameExtension:"vmatrace", conformingTo:.data)!
    ]
    
    // TODO: have
    
    var body: some Scene {
        WindowGroup {
            NavigationSplitView {
                VStack {
                    List(files, selection:$selection) { file in
                        // compare url to the previous dir
                        // if it differs, then add divider
//                        if lastUrl && url.path != lastUrl!.path {
//                            files.append(Divider())
//                        }
                        
                        Text(generateName(file: file))
                            .help(file.shortDirectory)
                    }
                }
            }
            detail: {
                // About hideSidebar
                // https://github.com/google/perfetto/issues/716
                MyWKWebView(webView:webView, request: URLRequest(url:URL(string: ORIGIN + "/?hideSidebar=true")!), selection:selection, firstRequest:firstRequest)
                
            }
            .navigationTitle(selection != nil ? shortFilename(selection!) : "")
        }
        // https://nilcoalescing.com/blog/CustomiseAboutPanelOnMacOSInSwiftUI/
        .commands {
            CommandGroup(after: .newItem) {
                Button("Open File") {
                    let panel = NSOpenPanel()
                    panel.allowsMultipleSelection = false
                    panel.canChooseDirectories = true
                    panel.canChooseFiles = true
                    panel.allowedContentTypes = fileTypes
                    
                    panel.begin { reponse in
                        if reponse == .OK {
                            let urls = panel.urls
                            if urls.count == 1 {
                                let url = urls[0]
                                let filesNew = listFilesFromURL(url)
                        
                                // for now wipe the old list
                                if filesNew.count > 0 {
                                    files = filesNew
                                    
                                    // if single file opened, then load it immediately
                                    if files[0].url.isFileURL { selection = files[0].id }
                                }
                            }
                            
                            // Not sure where to set this false, so do it here
                            // this is to avoid reloading the request
                            firstRequest = false
                        }
                    }
                }
                .keyboardShortcut("O")
                
                Button("Goto File") {
                    if selection != nil {
                        openContainingFolder(selection!);
                    }
                }
                .keyboardShortcut("G")
            }
            CommandGroup(replacing: .appInfo) {
                    Button("About kram-profile") {
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
                }
            }

        
    }
    
    // https://stackoverflow.com/questions/49882933/pass-jsonobject-from-swift4-to-wkwebview-javascript-function
    
    // all need a list of files, and File open dialog
    // For a user folder, find all clang.json files, .trace files, and vma.json files
    // and then need to send the selected one over to MyWKWebView so it can open
    // the data from posting a message to the page.
}

