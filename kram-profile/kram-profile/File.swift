// kram - Copyright 2020-2024 by Alec Miller. - MIT License
// The license and copyright notice shall be included
// in all copies or substantial portions of the Software.

import Foundation

private let log = Log("kram/File")

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

class BuildStats {
    var frontendStart = Int.max
    var backendStart = Int.max

    var totalExecuteCompiler = 0
    
    var totalFrontend = 0
    var totalSource = 0
    var totalInstantiateFunction = 0
    var totalInstantiateClass = 0
    var totalCodeGenFunction = 0
    
    var totalBackend = 0
    var totalOptimizer = 0
    var totalCodeGenPasses = 0
    var totalOptFunction = 0
    
    func combine(_ rhs: BuildStats) {
        totalExecuteCompiler += rhs.totalExecuteCompiler
        
        totalFrontend += rhs.totalFrontend
        totalSource += rhs.totalSource
        totalInstantiateFunction += rhs.totalInstantiateFunction
        totalInstantiateClass += rhs.totalInstantiateClass
        totalCodeGenFunction += rhs.totalCodeGenFunction
        
        totalBackend += rhs.totalBackend
        totalOptimizer += rhs.totalOptimizer
        totalCodeGenPasses += rhs.totalCodeGenPasses
        totalOptFunction += rhs.totalOptFunction
    }
    
    func divideBy(_ s: Int) {
        frontendStart /= s
        backendStart /= s

        totalExecuteCompiler /= s
        
        totalFrontend /= s
        totalSource /= s
        totalInstantiateFunction /= s
        totalInstantiateClass /= s
        totalCodeGenFunction /= s
        
        totalBackend /= s
        totalOptimizer /= s
        totalCodeGenPasses /= s
        totalOptFunction /= s
    }
}

class File: Identifiable, Hashable, Equatable, Comparable
{
    // TODO: archive url relative to archive so not unique if multiple archives dropped
    // but currently all lookup is by url, and not url + archive.  Just make sure to
    // include unique dir when building archives.  zip has max 512 char path.
    
    var id: String { url.absoluteString }
    var name: String { url.lastPathComponent }
    let url: URL
    let shortDirectory: String
    let parentFolders: String
    let fileType: FileType
    
    // optional container
    let containerType: ContainerType
    var archive: Archive?
    
    var duration = 0.0 // in seconds
    
    var fileContent: Data?
    var modStamp: Date?
    var loadStamp: Date?
    
    // This is only updated for Build fileType
    var buildTimings: [String:BuildTiming] = [:]
    var buildStats: BuildStats?
    
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
    
    // Hashable
    public func hash(into hasher: inout Hasher) {
        hasher.combine(id)
    }
    
    // call this when the file is loaded
    public func setLoadStamp()  {
        loadStamp = modStamp
    }
    public func isReloadNeeded() -> Bool {
        return modStamp != loadStamp
    }
    
    public func eraseFileContent() {
        // fileContent should get reloaded
        fileContent = nil
        
        // Perfetto should reload the fileContent
        loadStamp = nil
    }
    
    public func eraseCaches() {
        duration = 0.0
        
        if fileType == .Build {
            // for build fileType
            buildTimings.removeAll()
            buildStats = nil
        }
        else if fileType == .Memory {
            // for memory fileType
            threadInfo.removeAll()
        }
    }
    
    public static func fileModificationDate(url: URL) -> Date? {
        do {
            let attr = try FileManager.default.attributesOfItem(atPath: url.path)
            return attr[FileAttributeKey.modificationDate] as? Date
        } catch {
            return nil
        }
    }

    // show some of dir file is in, TODO: 2 levels not enough?
    public static func buildShortDirectory(url: URL) -> String {
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

func generateDuration(file: File) -> String {
    if file.duration == 0.0 { return "" }
    
    let unitText = file.fileType == .Memory ? "m" : "s"
    return "\(double:file.duration, decimals:3)\(unitText)"
}

func generateTotalDuration(_ file: File, _ buildFiles: [File]) -> String {
    if buildFiles.isEmpty { return "" }
    
    // add up duration of all files related to selection
    var totalDuration = 0.0
    for buildFile in buildFiles {
        totalDuration += buildFile.duration
    }
    
    if totalDuration == 0.0 { return "" }
    var text = "/ \(double:totalDuration, decimals:3)s "
    
    // only show percent if high enough
    let totalPercent = file.duration * 100.0 / totalDuration
    if totalPercent >= 1 {
        text += "\(double:totalPercent, decimals:0)% "
    }
    return text
}

func generateNavigationTitle(_ sel: String?, _ files: [File]) -> String {
    if sel == nil { return "" }
    
    let f = lookupFile(selection: sel!)
    var text = generateDuration(file: f) + " "
    
    // total the durations matching the selection
    if f.fileType == .Build {
        let buildFiles = findFilesForBuildTimings(files: files, selection: sel!)
        text += generateTotalDuration(f, buildFiles)
    }
    
    // add the shortened filename
    text += f.name
    
    // add the archive name
    if let fileArchive = f.archive {
        text += " in (" + fileArchive.name + ")"
    }
    
    return text
}

//-------------
// Note: if a file is deleted which happens often with builds,
// then want to identify that and update the list.  At least
// indicate the item is gone, and await its return.

// Holds supported files dropped or opened from Finder, reload reparses this
var droppedFileCache : [URL] = []

// Flattened list of supported files from folders and archives
var fileCache : [URL:File] = [:]

func updateFile(url: URL) -> File {
    let file = File(url:url)
    
    // This preseves the duration previously parsed and stored
    
    if let fileOld = fileCache[file.url] {
        if file.modStamp == nil || // means file and/or dir went away, so return fileOld
            file.modStamp! == fileOld.modStamp! {
            return fileOld
        }
    }
    
    // This wipes the duration, so it can be recomputed
    // TODO: may want to check crc32 if present before wiping all data
    
    if file.archive == nil {
        file.eraseFileContent()
        file.eraseCaches()
    }
    
    fileCache[file.url] = file
    
    return file
}

func lookupFile(url: URL) -> File {
    let file = File(url:url)
    if let fileOld = fileCache[file.url] {
        return fileOld
    }
    return file
}

func lookupFile(selection: String) -> File {
    return lookupFile(url:URL(string:selection)!)
}

//-------------

class Archive: Identifiable, /*Hashable, */ Equatable, Comparable {
    // this doesn't change on reload
    var id: String { url.absoluteString }
    var name: String { url.lastPathComponent }
    let url: URL
    let shortDirectory: String
    let parentFolders: String
   
    // This can call change
    var modStamp: Date?
    var loadStamp: Date?
    
    var archiveContent: Data?
    var archive: ZipHelperW?
      
    init(_ url: URL) {
        self.url = url
        self.modStamp = File.fileModificationDate(url:url)
        self.shortDirectory = File.buildShortDirectory(url:url)
        self.parentFolders = url.deletingLastPathComponent().absoluteString
    }
    
    func open() {
        if loadStamp == nil {
            loadStamp = modStamp
            
            do {
                archiveContent = try Data(contentsOf: url, options: [.mappedIfSafe])
                archive = ZipHelperW(data: archiveContent!)
            }
            catch {
                log.error(error.localizedDescription)
            }
        }
    }
    
    public static func == (lhs: Archive, rhs: Archive) -> Bool {
        return lhs.id == rhs.id
    }
    public static func < (lhs: Archive, rhs: Archive) -> Bool {
        return lhs.id < rhs.id
    }
    
    public func isReloadNeeded() -> Bool {
        return modStamp != loadStamp
    }
}

// cache of archives to avoid creating these each time
var archiveCache: [URL:Archive] = [:]

func lookupArchive(_ url: URL) -> Archive {
    let archive = Archive(url)
    
    // This preseves the content in the archive, and across all files with held content
    if let archiveOld = archiveCache[archive.url] {
        if archive.modStamp == nil || // means file and/or dir went away, so return fileOld
            archive.modStamp! == archiveOld.modStamp! {
            return archiveOld
        }
        
        archive.open()
        
        // replace any archives with this one
        for file in fileCache.values {
            if file.archive == archiveOld {
                
                // Only need to release caches if hash differs
                let filename = file.url.absoluteString
               
                let oldEntry = archiveOld.archive!.zipEntry(byName: filename)
                let newEntry = archive.archive!.zipEntry(byName: filename)
                
                let isNewEntryMissing = String(cString:newEntry.filename) == ""
                
                if isNewEntryMissing {
                    // TODO: handle new archive missing the file
                    // need to release file
                }
                else {
                    // update the archive
                    file.archive = archive
                    
                    // convert zip modStamp to Data object (only valid to seconds)
                    file.modStamp = Date(timeIntervalSince1970: Double(newEntry.modificationDate))
                }
                
                if !isNewEntryMissing && (oldEntry.crc32 == newEntry.crc32) {
                    
                    // erase fileContent since it may alias mmap going away
                    file.eraseFileContent()
                    
                    // keep any caches
                }
                else {
                    // erase fileContent
                    file.loadStamp = nil
                    file.fileContent = nil
                    
                    file.eraseFileContent()
                    file.eraseCaches()
                }
            }
        }
    }
    else {
        archive.open()
    }
    
    // Files will need to reopen content, but only if the hash is different.
    archiveCache[archive.url] = archive
    
    return archive
}

//-------------

func loadFileContent(_ file: File) -> Data {
    if file.fileContent != nil {
        return file.fileContent!
    }
    
    if file.archive != nil {
        // this will point to a section of an mmaped zip archive
        // but it may have to decompress content to a Data object
        file.fileContent = file.archive!.archive!.extract(file.url.absoluteString)
    }
    else {
        // This uses mmap if safe.  Does not count towars memory totals, since can be paged out
        do {
            file.fileContent = try Data(contentsOf: file.url, options: [.mappedIfSafe])
        }
        catch {
            log.error(error.localizedDescription)
        }
    }
    
    return file.fileContent!
}

func isSupportedFilename(_ url: URL) -> Bool {
    let ext = url.pathExtension
    
    // what ext does trace.zip, or trace.gz come in as ?
    // should this limit compressed files to the names supported below
    
    if ext == "gz" {
        return true
    }
    if ext == "zip" {
        return true
    }
        
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

func listFilesFromArchive(_ urlArchive: URL) -> [File] {
    
    let archive = lookupArchive(urlArchive)
    var files: [File] = []
    
    let arc = archive.archive!
    for i in 0..<arc.zipEntrysCount() {
        let filename = String(cString: arc.zipEntry(i).filename)
        let url = URL(string: filename)!
        
        let isFileSupported = isSupportedFilename(url)
        if !isFileSupported {
            continue
        }
        
        // don't support archives within archives
        let isArchive  = File.filenameToContainerType(url) == .Archive
        if isArchive {
            continue
        }
            
        // TODO: archives don't have full paths, so lookup can get confused
        // if there are multiple archives with same paths.
        let file = updateFile(url:url)
        if file.archive != archive {
            file.archive = archive
        }
        files.append(file)
    }
    return files
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
                if fileURL.hasDirectoryPath { continue }
                
                let isSupported = isSupportedFilename(fileURL)
                if isSupported {
                    let isArchive  = File.filenameToContainerType(fileURL) == .Archive
                    if isArchive {
                       files += listFilesFromArchive(fileURL)
                    }
                    else {
                        files.append(updateFile(url:fileURL));
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
                    files.append(updateFile(url:url))
                }
            }
        }
    }

    return files
}
