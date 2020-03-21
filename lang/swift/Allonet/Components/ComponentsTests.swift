//
//  ComponentsTests.swift
//  Allonet.SwiftTests
//
//  Created by Patrik on 2020-03-21.
//  Copyright © 2020 voxar. All rights reserved.
//

import XCTest

func test<T: Codable & Equatable>(_ obj: T, _ json: String, file: StaticString = #file, line: UInt = #line) {
    let jsonString = obj.jsonString
    XCTAssertEqual(jsonString, json, file: file, line: line)
    let unpacked = T.from(json: jsonString)
    XCTAssertEqual(unpacked, obj, file: file, line: line)
}

class ComponentsTests: XCTestCase {

    func testComponents() {
        var components = Components()
        
        XCTAssertEqual(components.jsonString, #"{}"#)
        
        components.geometry = Geometry(
            type: .inline,
            vertices: [
                .init(x: 1, y: 2, z: 3)
            ],
            uvs: [
                .init(u: 0, v: 0)
            ],
            triangles: [
                .init(a: 0, b: 1, c: 2)
            ]
        )
        test(
            components,
            #"{"geometry":{"uvs":[[0,0]],"vertices":[[1,2,3]],"type":"inline","triangles":[[0,1,2]]}}"#
        )
    }
}
