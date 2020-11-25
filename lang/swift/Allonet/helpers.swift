//
//  helpers.swift
//  Example
//
//  Created by Patrik on 2020-11-25.
//  Copyright © 2020 voxar. All rights reserved.
//

import Foundation

internal func str(_ ptr: UnsafePointer<Int8>?) -> String? {
    return ptr.flatMap(String.init(cString:))
}
