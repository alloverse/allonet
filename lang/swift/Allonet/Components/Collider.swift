//
//  Collider.swift
//  Allonet Example
//
//  Created by Patrik on 2020-03-21.
//  Copyright © 2020 voxar. All rights reserved.
//

import Foundation

public struct Collider: Codable, Equatable {
    public var height: Float
    public var width: Float
    public var depth: Float
    public var type: String
}
