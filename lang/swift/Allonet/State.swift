//
//  State.swift
//  Allonet Example
//
//  Created by Patrik on 2020-03-21.
//  Copyright © 2020 voxar. All rights reserved.
//

import Foundation

public struct State: Codable, Equatable {
    var revision: Int
    var entities: [String: Entity]
}
