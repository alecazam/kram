// kram - Copyright 2020-2024 by Alec Miller. - MIT License
// The license and copyright notice shall be included
// in all copies or substantial portions of the Software.

import Foundation

import os.log
import Darwin

/*
 Can strip away logs or add adititional data to messages more easily

 To use, in the Swift source file specify the following variable:
   private let log = Log() -> Log(#file) <- paths are stripped
 or
   private let log = Log("File/Groupname")

   log.debug("debug text")
   log.info("info text")
   log.error("error text")

 A few boolean flags control the output:
   configure(prints, timestamps, stacktraces)

 For more expensive functions, use the isInfoEnabled() call to skip blocks.
   if log.isInfoEnabled() {
      log.info("ComputeWorldPopulation:", countPeopleInAllCountries())
   }

 Output:
 prints = true (via print)
   14:40:21.185 D[GameSceneViewController] debug text
   14:40:21.201 I[GameSceneViewController] info text
   14:40:21.321 E[GameSceneViewController] error text
     at GameSceneViewController:75@init(_:file:)
     on thread:queue

 or
   0.001s D[GameSceneViewController] debug text
   0.002s I[GameSceneViewController] info text
   0.003s E[GameSceneViewController] error text
     at GameSceneViewController:75@init(_:file:)
     on thread:queue

 prints = false (via os_log)
   2018-04-11 14:59:07.122127-0700 SwiftShot[581:21310] [GameSceneViewController] debug text
   2018-04-11 14:59:07.122166-0700 SwiftShot[581:21310] [GameSceneViewController] info text
   2018-04-11 14:59:07.122186-0700 SwiftShot[581:21310] [GameSceneViewController] error text
*/

class Log {
    // verbose: Whether os_log or print is used to report logs.
    static var prints = false
    // stacktrace: Whether stack trace is logged on errors.
    static var stacktraces = false
    // timestamp: Show timestamps on all entries when printing statements.
    static var timestamps = false
    // absoluteTimestamps: Show relative or absolute timestampes.
    static var absoluteTimestamps = true
    
    // Custom logging group - usually based on source filename.
    // This has a very limited output, but does go to console
    private var log: OSLog
    
    // Cache the filename for reporting it on errors.
    private var file: String
    // All logs go to this category for filtering.
    private var category: String
    // This can be filtered from command line arguments.
    private static var subsystem = Bundle.main.bundleIdentifier!
    
    // Store data for timestamps.
    private static var timestampToSeconds: Double = 0
    private static var timestampStart = timestamp()
    private static var timestampStartDate = Date()
    private static var timestampFormatter = initTimestampFormatter()
    
    init(_ category: String = #file, file: String = #file) {
        // Strip the path, but allow hierachical category f.e. "Group/Subgroup" wihtout .swift.
        self.category = category
        if category.hasSuffix(".swift") {
            self.category = Log.stripFilePathAndExtension(category)
        }
        
        // Compute once for use in logs.
        self.file = Log.stripFilePathAndExtension(file)
        
        self.log = OSLog(subsystem: Log.subsystem, category: self.category)
    }
    
    // Test whether messages are logged for the given levels
    func isWarnEnabled() -> Bool {
        return log.isEnabled(type: .default)
    }
    func isInfoEnabled() -> Bool {
        return log.isEnabled(type: .info)
    }
    func isDebugEnabled() -> Bool {
        #if DEBUG
        return log.isEnabled(type: .debug)
        #else
        return false
        #endif
    }
    
    private func logToOSLog(_ text: String, _ type: OSLogType) {
        // TODO: this needs to split the string up, since os_log limits length to
        // some paltry 1023 chars.
        os_log("%s", log: log, type: type, text)
    }
    
    func error(_ message: @autoclosure () -> String, _ function: String = #function, _ line: Int = #line) {
        let text = formatMessage(message(), .error, function, line)
        if Log.prints {
            print(text)
        } else {
            logToOSLog(text, .error)
        }
    }
    
    // os_log left out warnings, so reuse default type for that
    func warn(_ message: @autoclosure () -> String, _ function: String = #function, _ line: Int = #line) {
        let text = formatMessage(message(), .default, function, line)
        if Log.prints {
            print(text)
        } else {
            logToOSLog(text, .default) // this doesn't get colored yellow like a warning
        }
    }
    
    func info(_ message: @autoclosure () -> String) {
        let text = formatMessage(message(), .info)
        if Log.prints {
            print(text)
        } else {
            logToOSLog(text, .info)
        }
    }
    
    func debug(_ message: @autoclosure () -> String) {
        // debug logs are stripped from release builds
        #if DEBUG
        let text = formatMessage(message(), .debug)
        if Log.prints {
            print(text)
        } else {
            logToOSLog(text, .debug)
        }
        #endif
    }
    
    private func formatLevel(_ level: OSLogType) -> String {
        switch level {
            case .debug:    return ""
            case .info:     return ""
            case .default:  return "⚠️"
            case .error:    return "🛑"
            default:        return ""
        }
    }
    
    // Customize this printing as desired.
    private func formatMessage(_ message: String, _ level: OSLogType, _ function: String = "", _ line: Int = 0) -> String {
        var text = ""
        
        let levelText = formatLevel(level)
        
        if Log.prints {
            let timestamp = Log.formatTimestamp()
            
            // These messages never go out to the system console, just the debugger.
            switch level {
            case .debug:
                text += "\(timestamp)\(levelText)D[\(category)] \(message)"
            case .info:
                text += "\(timestamp)\(levelText)I[\(category)] \(message)"
            case .default: // not a keyword
                text += "\(timestamp)\(levelText)W[\(category)] \(message)"
                text += Log.formatLocation(file, line, function)
            case .error:
                text += "\(timestamp)\(levelText)E[\(category)] \(message)\n"
                text += Log.formatLocation(file, line, function)
            default:
                text += message
            }
        } else {
            // Consider reporting the data above to os_log.
            // os_log reports data, time, app, threadId and message to stderr.
            text += levelText
            text += message
            
            // os_log can't show correct file/line, since it uses addrReturnAddress - ugh
            switch level {
                case .default: fallthrough
                case .error:
                    text += Log.formatLocation(file, line, function)
                default:
                    break
            }
        }
        
        if Log.stacktraces && (level == .error) {
            text += "\n"
            
            // Improve this - these are mangled symbols without file/line of where
            Thread.callStackSymbols.forEach { text += $0 + "\n" }
        }
        
        return text
    }
    
    // location support
    private static func formatLocation(_ file: String, _ line: Int, _ function: String) -> String {
        var text = ""
        let threadName = Thread.current.name ?? ""
        var queueName = OperationQueue.current?.name ?? ""
        if !queueName.isEmpty {
            queueName = ":" + queueName
        }
        
        text += " at \(file):\(line)@\(function)\n"
        text += " on \(threadName)\(queueName)"
        return text
    }
    
    private static func stripFilePathAndExtension(_ path: String) -> String {
        let str = path as NSString
        return (str.deletingPathExtension as NSString).lastPathComponent
    }
    
    // timestamp support
    private static func initTimestampFormatter() -> DateFormatter {
        let formatter = DateFormatter()
        formatter.locale = Locale.current
        formatter.setLocalizedDateFormatFromTemplate("HH:mm:ss.SSS") // ms resolution
        return formatter
    }
    
    private static func timeFromStart() -> Double {
        return max(0.0, Log.timestamp() - Log.timestampStart)
    }
    
    private static func timeAbsolute() -> String {
        let timestamp = Log.timeFromStart()
        let date = Date(timeInterval: timestamp, since: Log.timestampStartDate)
        return timestampFormatter.string(from: date)
    }
    
    private static func formatTimestamp() -> String {
        var timestamp = ""
        if Log.timestamps {
            if Log.absoluteTimestamps {
                timestamp = Log.timeAbsolute() + " "
            } else {
                timestamp = String(format: "%.3fs ", Log.timeFromStart())
            }
        }
        return timestamp
    }
    
    // need timestamps in other parts of the app
    static func timestamp() -> Double {
        if Log.timestampToSeconds == 0 {
            // Cache the conversion.  Note that clock rate can change with cpu throttling.
            // These are high-resolution timestamps taken from the system timer.
            var info = mach_timebase_info(numer: 0, denom: 0)
            mach_timebase_info(&info)
            let numer = Double(info.numer)
            let denom = Double(info.denom)
            Log.timestampToSeconds = 1e-9 * (numer / denom) // inverse so we can multiply
        }
        
        let timestamp = Double(mach_absolute_time())
        let time = timestamp * Log.timestampToSeconds
        return time
    }
}

