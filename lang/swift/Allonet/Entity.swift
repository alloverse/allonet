//
//  Entity.swift
//  Example
//
//  Created by Patrik on 2020-03-21.
//  Copyright © 2020 voxar. All rights reserved.
//

import Foundation


public struct Entity: Codable, Equatable {
    public var id: String?
    public var components: Components
}

