//  FilePicker.swift
//
//  MIT License
//
//  Copyright (c) 2021 Mark Renaud
//
//  Permission is hereby granted, free of charge, to any person obtaining a copy
//  of this software and associated documentation files (the "Software"), to deal
//  in the Software without restriction, including without limitation the rights
//  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//  copies of the Software, and to permit persons to whom the Software is
//  furnished to do so, subject to the following conditions:
//
//  The above copyright notice and this permission notice shall be included in all
//  copies or substantial portions of the Software.
//
//  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
//  SOFTWARE.

// from https://github.com/markrenaud/FilePicker/blob/main/Sources/FilePicker/FilePicker.swift
// That version include iOS support, but I don't need that.
import SwiftUI
import UniformTypeIdentifiers

public struct FilePicker<LabelView: View>: View {
    
    public typealias PickedURLsCompletionHandler = (_ urls: [URL]) -> Void
    public typealias LabelViewContent = () -> LabelView
    
    // only allow one panel up at a time.  But Panel shhould already enforce?
    @State private var isPresented: Bool = false
    
    public let types: [UTType]
    //public let allowMultiple: Bool
    public let pickedCompletionHandler: PickedURLsCompletionHandler
    public let labelViewContent: LabelViewContent
    
    public init(types: [UTType], /* allowMultiple: Bool, */ onPicked completionHandler: @escaping PickedURLsCompletionHandler, @ViewBuilder label labelViewContent: @escaping LabelViewContent) {
        self.types = types
        //self.allowMultiple = allowMultiple
        self.pickedCompletionHandler = completionHandler
        self.labelViewContent = labelViewContent
    }

    public init(types: [UTType], /*allowMultiple: Bool,*/ title: String, onPicked completionHandler: @escaping PickedURLsCompletionHandler) where LabelView == Text {
        self.init(types: types, /*allowMultiple: allowMultiple,*/ onPicked: completionHandler) { Text(title) }
    }

    public var body: some View {
        // Move the button into a Menu option.  action should be able to fire this.
        Button(
            action: {
                if !isPresented { isPresented = true }
            },
            label: {
                labelViewContent()
            }
        )
        .keyboardShortcut("o") // open with cmd+O
        .disabled(isPresented)
        .onChange(of: isPresented, perform: { presented in
            // binding changed from false to true
            if presented == true {
                let panel = NSOpenPanel()
                panel.allowsMultipleSelection = false
                panel.canChooseDirectories = true
                panel.canChooseFiles = true
                panel.allowedContentTypes = types.map { $0 }
                panel.begin { reponse in
                    if reponse == .OK {
                        pickedCompletionHandler(panel.urls)
                    }
                    // reset the isPresented variable to false
                    isPresented = false
               }
            }
        })
    }
}
