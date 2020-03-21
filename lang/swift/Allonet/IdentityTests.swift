//
//  IdentityTests.swift
//  Example
//
//  Created by Patrik on 2020-03-21.
//  Copyright © 2020 voxar. All rights reserved.
//

import XCTest

class IdentityTests: XCTestCase {
    func testExample() {
        let identity = AgentIdentity(display_name: "Hello, World")

        test(identity, #"{"display_name":"Hello, World"}"#)
    }
}
