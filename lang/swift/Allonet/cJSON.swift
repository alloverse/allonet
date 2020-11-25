//
//  cJSON.swift
//  Example
//
//  Created by Patrik on 2020-11-25.
//  Copyright © 2020 voxar. All rights reserved.
//

import Foundation
import CAllonet

extension cJSON: CustomDebugStringConvertible {
    subscript(_ index: Int) -> cJSON? {
        switch self.type {
        case cJSON_Object:
            return withUnsafePointer(to: self) { selfptr in
                cJSON_GetArrayItem(selfptr, Int32(index))
            }?.pointee
        default:
            return nil
        }
    }
    subscript(_ key: String) -> cJSON? {
        switch self.type {
        case cJSON_Object:
            return withUnsafePointer(to: self) { selfptr in
                key.withCString { key in
                    cJSON_GetObjectItemCaseSensitive(selfptr, key)
                }
            }?.pointee
        default:
            return nil
        }
    }
    
    subscript(_ index: Int) -> Double? {
        guard type == cJSON_Number else { return nil }
        return valuedouble
    }
    
    subscript(_ index: Int) -> Int? {
        guard type == cJSON_Number else { return nil }
        return Int(valueint)
    }
    
    public var debugDescription: String {
        withUnsafePointer(to: self) { ptr in
            guard let json = cJSON_Print(ptr) else { return "<Failed to export json>" }
            return String(cString: UnsafePointer(json))
        }
    }
}
