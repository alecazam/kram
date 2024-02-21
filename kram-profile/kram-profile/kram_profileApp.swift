//
//  kram_profileApp.swift
//  kram-profile
//
//  Created by Alec on 2/18/24.
//

import SwiftUI
import WebKit

// https://github.com/gualtierofrigerio/WkWebViewJavascript/blob/master/WkWebViewJavascript/WebViewHandler.swift

// https://levelup.gitconnected.com/how-to-use-wkwebview-on-mac-with-swiftui-10266989ed11
// Signing & Capabilites set App Sanbox (allow outgoing connections)

// This is really just a wrapper to turn WKWebView into something SwiftUI
// can interop with.  SwiftUI has not browser widget.

struct File: Identifiable, Hashable
{
    var id: String { url.absoluteString }
    var name: String { url.lastPathComponent }
    let url: URL
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
        
        if selection != nil {
            loadFile(webView, selection!)
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

func loadFile(_ webView: WKWebView, _ path: String) /*async*/ {
    
    let fileURL = URL(string: path)!
    
    print(path)
    
    
    // https://stackoverflow.com/questions/62035494/how-to-call-postmessage-in-wkwebview-to-js
    /* need to PostMessage a json object with the folloinwg
     {
     'perfetto': {
     buffer: ArrayBuffer; // content of the file
     title: string;
     fileName?: string;   // Optional
     url?: string;        // Optional
     }
     }
     */
    
    //open func evaluateJavaScript(_ javaScriptString: String, completionHandler: ((Any?, Error?) -> Void)? = nil)
    //
    //open func evaluateJavaScript(_ javaScriptString: String) async throws -> Any
    
    struct PerfettoFile : Codable {
        var buffer: String // really ArrayBuffer, but will get replaced
        var title: String
        
        // optional fields
        //var fileName: String?
        //var url: String?
    }
    
    struct Perfetto : Codable {
        var perfetto: PerfettoFile
    }
    
    do {
        // Data converted to an Array[UInt8] which isn't quite same
        // as an ArrayBuffer.  Can ArrayBuffer be serialized.  Does
        // it have a count or what is it exactly.  See if Perfetto
        // can take a String in the packet instead.
        
        let fileContent = try Data(contentsOf: fileURL)
        
        let fileContentBase64 = fileContent.base64EncodedString()
        
        // perfetto says they don't take file:// urls
        let file = PerfettoFile(buffer: "",
                                title: fileURL.standardizedFileURL.lastPathComponent)
        let perfetto = Perfetto(perfetto: file)
        
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
        
        // https://developer.mozilla.org/en-US/docs/Glossary/Base64#the_unicode_problem
        //https://stackoverflow.com/questions/30106476/using-javascripts-atob-to-decode-base64-doesnt-properly-decode-utf-8-strings
        
        let json = """
        
        //function bytesToBase64(bytes) {
        //  const binString = String.fromCodePoint(...bytes);
        //  return btoa(binString);
    
        function base64ToBytes(base64) {
          const binString = atob(base64);
          return Uint8Array.from(binString, (m) => m.codePointAt(0));
        }
        
        //location.reload();
                
        var fileData = '\(fileContentBase64)';
        
        // convert from string -> Uint8Array -> ArrayBuffer
        var obj = JSON.parse('{\(perfettoEncode)}');
        
        // convert base64 back
        obj.perfetto.buffer = base64ToBytes(fileData).buffer;
        
        // How to set this command, it's only available onTraceLoad()
        // but calls through to a sidebar object on the ctx
        //window.postMessage('dev.perfetto.CoreCommands#ToggleLeftSidebar','\(ORIGIN)')
    
        window.postMessage(obj,'\(ORIGIN)');
    """
       
    /*
    """
        // this isn't working
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
        
        /*
        // Data or String instead of ArrayBuffer, Data seems more appropo
        var fileContent = try String(contentsOf: fileURL)
        //let fileContent = try Data(contentsOf: fileURL) // use for binary
        
        // code does show a way to respond to ArrayBuffer
        //let encoder = JSONEncoder()
        //let data = try encoder.encode(fileContent)
        // var json = String(data: data, encoding: .utf8)!
        
        let file = PerfettoFile(buffer: "",
                                title: fileURL.standardizedFileURL.lastPathComponent)
        let perfetto = Perfetto(perfetto: file)
        
        let encoder = JSONEncoder()
        //encoder.outputFormatting = .prettyPrinted
       
        let data = try encoder.encode([perfetto])
        let dataEncodedString = String(decoding: data, as: UTF8.self)
        let dataEscaped = String(dataEncodedString.dropLast().dropFirst())

        // This is to avoid JSON.stringify
        // var json = String(data: data, encoding: .utf8)!
         
        // someone was doing this before
        //json = json.replacingOccurrences(of: "\n", with: "\\\n")
        
       
        //print(json)

        // https://gist.github.com/chromy/170c11ce30d9084957d7f3aa065e89f8
        // need to post this JavaScript
        
        // https://stackoverflow.com/questions/32113933/how-do-i-pass-a-swift-object-to-javascript-wkwebview-swift
        
        
        // There is this too, added to the URL to reopen it
        // const reopenUrl = new URL(location.href);
        // reopenUrl.hash = `#reopen=${traceUrl}`;
        
        // doesn't have error w/Data but doesn't do anything
        //json = "window.postMessage(" + json
        //json += ", \"" + ORIGIN + "\")"
    
        //json = json = "window.postMessage(" + json
        //json += ", \"" + ORIGIN + "\")"
        
        // This encodes to Data
        
        // someone was doing this before
        //var fileContentString = String(data: fileContent, encoding: .utf8)!
        
        // https://gist.github.com/pwightman/64c57076b89c5d7f8e8c
        // Because JSON is not a subset of JavaScript, the LINE_SEPARATOR and PARAGRAPH_SEPARATOR unicode
                // characters embedded in (valid) JSON will cause the webview's JavaScript parser to error. So we
                // must encode them first. See here: http://timelessrepo.com/json-isnt-a-javascript-subset
                // Also here: http://media.giphy.com/media/wloGlwOXKijy8/giphy.gif
        fileContent = fileContent.replacingOccurrences(of: "\n", with: "\\\n")
        
        fileContent = fileContent
                .replacingOccurrences(of: "\u{2028}", with: "\\u2028")
                .replacingOccurrences(of: "\u{2029}", with: "\\u2029")
                
        // now escape the string
        //let encoder = JSONEncoder()
        let fileContentData = try encoder.encode([fileContent])
        let encodedString = String(decoding: fileContentData, as: UTF8.self)
        let fileContentEscaped = String(encodedString.dropLast().dropFirst())

        // this is a string
        var json = ""
        json += "var fileData = " + fileContentEscaped + ";"
       
        // convert from string -> Uint8Array -> ArrayBuffer
        json += "var perfetto = " + dataEscaped + ";"
        json += "var enc = new TextEncoder();"
        json += "perfetto.buffer = enc.encode(fileData).buffer;"
        json += "window.postMessage(JSON.parse(perfetto), \"" + ORIGIN + "\");"
    
        //json = "window.postMessage(JSON.parse(" + json
        //json += "), \"" + ORIGIN + "\")"
        
        // https://stackoverflow.com/questions/37820666/how-can-i-send-data-from-swift-to-javascript-and-display-them-in-my-web-view
        // just make sure the JSON string doesn't contain unescaped newline characters. evaluateJavaScript won't work unless the newline characters are escaped.  Ugh!
        // Also, in NSJSONSerialization don't use the .prettyPrinted option, use [] as per above, or it also won't work
        
 */
        print(json)
        
        webView.evaluateJavaScript(json) { (result, error) in
            if error != nil {
                print("\(error)! \(result)")
            }
            else {
                print("\(result)")
            }
        }

        
    } catch {
     // handle error
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
                filename == "manifest.json"
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
                    files.append(File(url:fileURL));
                }
            }
        }
        else if url.isFileURL {
            // TODO: list out zip archive
        
            let isSupported = isSupportedFilename(url)
            if isSupported {
                files.append(File(url:url))
            }
        }
        
        print("found \(files.count) files")
        
        return files
    }
    
    func shortFilename(_ str: String) -> String {
        let url = URL(string: str)!
        return url.lastPathComponent
    }
    
    var body: some Scene {
        WindowGroup {
            // Don't really like this behavior, want a panel to come up and not
            // cause the main view to resize.  Also once collapsed, it's unclear
            // how to get NavigationView back, and app restartes collapsed.
            // See what I did in other tools.
            
            NavigationSplitView {
                VStack {
                    // TODO: have files ending in -vma.trace, .trace, and .json
                    // also archives in the zip file.
                    
                    // TODO: turn Open button into a menu, FilePicker already tied to Cmd+O
                    
                    FilePicker(types: [/*.plainText*/ .json /*, .zip */], title: "Open") { urls in
                        // TODO: not allowing multiselect for now
                        // but can pick dir or archive
                        // Do a filtered search under that
                        // for appropriate files.  And then
                        // track if picked again which files
                        // changed.
                        
                        //self.filenames = urls
                        if urls.count == 1 {
                            let filesNew = listFilesFromURL(urls[0])
                            
                            // for now wipe the old list
                            if filesNew.count > 0 {
                                files = filesNew
                            }
                        }
                        
                        // Not sure where to set this false, so do it here
                        // this is to avoid reloading the request
                        firstRequest = false
                    }
                    
                    List(files, selection:$selection) { file in
                        Text(file.name)
                    }
                }
            }
            detail: {
                // This is a pretty easy way to make Safari open to a link
                // But really want that embedded into app.   Assuming Perfetto
                // runs under Safari embedded WKWebView.
                // Link("Perfetto", destination: URL(string: ORIGIN)!)
                
                // TODO: really only need to load this once, url doesn't chang w/selection
                MyWKWebView(webView:webView, request: URLRequest(url:URL(string: ORIGIN)!), selection:selection, firstRequest:firstRequest)
                
            }
            .navigationTitle(selection != nil ? shortFilename(selection!) : "")
        }
    }
    
    // https://stackoverflow.com/questions/49882933/pass-jsonobject-from-swift4-to-wkwebview-javascript-function
    
    // all need a list of files, and File open dialog
    // For a user folder, find all clang.json files, .trace files, and vma.json files
    // and then need to send the selected one over to MyWKWebView so it can open
    // the data from posting a message to the page.
}

