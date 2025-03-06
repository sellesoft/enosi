import XCTest
import SwiftTreeSitter
import TreeSitterLpp

final class TreeSitterLppTests: XCTestCase {
    func testCanLoadGrammar() throws {
        let parser = Parser()
        let language = Language(language: tree_sitter_lpp())
        XCTAssertNoThrow(try parser.setLanguage(language),
                         "Error loading Lpp grammar")
    }
}
