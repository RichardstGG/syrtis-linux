// swift-tools-version: 6.0
import Foundation
import PackageDescription

// Resolve from the manifest so --package-path works from the repository root.
// Build this archive first with `cargo build --release --locked` at the root.
let rustArchive = URL(fileURLWithPath: #filePath).deletingLastPathComponent()
    .appendingPathComponent("../../../target/release/libtb_core_ffi.a").standardized.path

let package = Package(
    name: "LinuxCrossCheck",
    platforms: [.macOS(.v14)],
    products: [
        .executable(name: "linux-crosscheck-harness", targets: ["LinuxCrossCheckHarness"]),
    ],
    targets: [
        .target(name: "CTB", path: "CTB"),
        .target(name: "TokenBarCore", dependencies: ["CTB"], path: "TokenBarCore"),
        .executableTarget(
            name: "LinuxCrossCheckHarness",
            dependencies: ["TokenBarCore"],
            linkerSettings: [
                .unsafeFlags(["-Xlinker", rustArchive]),
                .linkedFramework("Security"),
                .linkedFramework("SystemConfiguration"),
                .linkedFramework("CoreFoundation"),
                .linkedLibrary("c++"),
                .linkedLibrary("resolv"),
            ]
        ),
    ]
)
