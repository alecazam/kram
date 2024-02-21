//
//  ContentView.swift
//  kram-profile
//
//  Created by Alec on 2/18/24.
//

import SwiftUI

/* generic template for lists
struct EditableList<Element: Identifiable, Content: View>: View {
    @Binding var data: [Element]
    var content: (Binding<Element>) -> Content

    init(_ data: Binding<[Element]>,
         content: @escaping (Binding<Element>) -> Content) {
        self._data = data
        self.content = content
    }

    var body: some View {
        List {
            ForEach($data, content: content)
                .onMove { indexSet, offset in
                    data.move(fromOffsets: indexSet, toOffset: offset)
                }
                .onDelete { indexSet in
                    data.remove(atOffsets: indexSet)
                }
        }
        // macOS doesn't have EditButton()
        //.toolbar { EditButton() }
    }
}
*/
 
/*
struct FileList: View {
    
    @Binding var items : [File]
    
    // https://www.swiftbysundell.com/articles/building-editable-swiftui-lists/
    
    // want to be able to obtain the selection from the list
    // or else need to define NavigationList items
    // this is same as id type
    @State public var selection: String?
    
    //public typealias SelectionCompletionHandler = (_ selection: String?) -> Void
    //public let selectionCompletionHandler: SelectionCompletionHandler
    
//    public init(items: [File], onSelected completionHandler: @escaping SelectionCompletionHandler) {
//        self.items = items
//        self.selection = nil
//        self.selectionCompletionHandler = completionHandler
//    }
    
    var body: some View {
        NavigationSplitView {
            List(items, selection:$selection) { item in
                Text(item.name)
            }
        }
        detail: {
            EmployeeDetails(for: employeeIds)
        }
    }
}
*/

// These apply to ListItems?
//        .onMove { indexSet, offset in
//            items.move(fromOffsets: indexSet, toOffset: offset)
//        }
//        .onDelete { indexSet in
//            items.remove(atOffsets: indexSet)
//        }
//        .onChange(of: selection, perform: { newValue in
//            selectionCompletionHandler(newValue)
//        })

// This is pretty painful to fix.  Why is this so hard
// Wrapper classes online.
//#Preview {
//    @State var files = [File(url: URL(string:"file://test")!)]
//    FileList(items:$files)
//}

