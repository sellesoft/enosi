// swift-tools-version:5.3
import PackageDescription

let package = Package(
    name: "TreeSitterLpp",
    products: [
        .library(name: "TreeSitterLpp", targets: ["TreeSitterLpp"]),
    ],
    dependencies: [
        .package(url: "https://github.com/ChimeHQ/SwiftTreeSitter", from: "0.8.0"),
    ],
    targets: [
        .target(
            name: "TreeSitterLpp",
            dependencies: [],
            path: ".",
            sources: [
                "src/parser.c",
                // NOTE: if your language has an external scanner, add it here.
            ],
            resources: [
                .copy("queries")
            ],
            publicHeadersPath: "bindings/swift",
            cSettings: [.headerSearchPath("src")]
        ),
        .testTarget(
            name: "TreeSitterLppTests",
            dependencies: [
                "SwiftTreeSitter",
                "TreeSitterLpp",
            ],
            path: "bindings/swift/TreeSitterLppTests"
        )
    ],
    cLanguageStandard: .c11
)
