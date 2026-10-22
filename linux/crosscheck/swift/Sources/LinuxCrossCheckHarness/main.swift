import Foundation
import TokenBarCore

func fail(_ message: String, code: Int32) -> Never {
    FileHandle.standardError.write(Data("\(message)\n".utf8))
    exit(code)
}

guard ProcessInfo.processInfo.environment["TZ"] == "Asia/Taipei" else {
    fail("error: TZ must be Asia/Taipei", code: 1)
}

// Match the root harness: strip only the NSUserDefaults language option,
// wherever it occurs, and reject unknown flags or an unpinned language.
var args: [String] = []
var appleLanguages: String?
var index = 1
while index < CommandLine.arguments.count {
    let token = CommandLine.arguments[index]
    if token.hasPrefix("-"), token.count > 1 {
        guard token == "-AppleLanguages", index + 1 < CommandLine.arguments.count else {
            fail("error: unsupported option \(token); only -AppleLanguages <value> is recognized", code: 2)
        }
        appleLanguages = CommandLine.arguments[index + 1]
        index += 2
    } else {
        args.append(token)
        index += 1
    }
}
guard appleLanguages == "(en)", (Bundle.main.preferredLocalizations.first ?? "en") == "en" else {
    fail("error: language must be pinned with -AppleLanguages \"(en)\"", code: 1)
}
guard (2...3).contains(args.count) else {
    fail("usage: linux-crosscheck-harness <fixtures-dir> <out-dir> [selector|all] -AppleLanguages \"(en)\"", code: 2)
}

// Only selectors absent from Sources/CrossCheckHarness belong here. Each
// handler reads synthetic fixtures and calls shipping TokenBarCore/Format.
// Each extension invokes the shipping core with synthetic inputs.
typealias Selector = (URL) throws -> [String: Any]
let selectors: [String: Selector] = ["core-leaf": coreLeaf, "quota-trend": quotaTrendSelector, "reports": reports, "charts": charts, "agent-usage": agentUsage]

let selector = args.count == 3 ? args[2] : "all"
guard selector == "all" || selectors[selector] != nil else {
    fail("error: unknown selector \(selector)", code: 2)
}
let fixturesDir = URL(fileURLWithPath: args[0], isDirectory: true)
let outDir = URL(fileURLWithPath: args[1], isDirectory: true)
let selected = selector == "all" ? selectors.keys.sorted() : [selector]
do {
    try FileManager.default.createDirectory(at: outDir, withIntermediateDirectories: true)
    for name in selected {
        let output = try selectors[name]!(fixturesDir)
        let data = try JSONSerialization.data(withJSONObject: output, options: [.prettyPrinted, .sortedKeys])
        try data.write(to: outDir.appendingPathComponent("\(name).actual.json"), options: .atomic)
        print("\(name).actual.json")
    }
} catch {
    fail("error: \(error.localizedDescription)", code: 1)
}
