//
//  EntityTests.swift
//  Allonet.SwiftTests
//
//  Created by Patrik on 2020-03-21.
//  Copyright © 2020 voxar. All rights reserved.
//

import XCTest

class EntityTests: XCTestCase {
    func testEntity() {
        let entity = Entity(children: [])
        
        XCTAssertEqual(entity.jsonString, #"{"children":[]}"#)
    }

}
