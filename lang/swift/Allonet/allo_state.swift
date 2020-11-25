//
//  allo_state.swift
//  Example
//
//  Created by Patrik on 2020-11-25.
//  Copyright © 2020 voxar. All rights reserved.
//

import Foundation
import CAllonet


extension allo_state: CustomDebugStringConvertible {
    public var debugDescription: String {
        return "allo_state(revision: \(revision), entities: \(Array(entitiesSequence)))"
    }
    
    /// self.entities as a swify sequence
    var entitiesSequence: IteratorSequence<EntityIterator> {
        return IteratorSequence(EntityIterator(first: self.entities.lh_first))
    }
}

/// Iterates allo_entity_list
struct EntityIterator: IteratorProtocol {
    typealias Element = allo_entity
    
    var ent: UnsafePointer<allo_entity>?
    
    init(first: UnsafePointer<allo_entity>) {
        self.ent = first
    }
    
    mutating func next() -> Element? {
        defer {
            if let next = self.ent?.pointee.pointers.le_next {
                self.ent = UnsafePointer(next)
            } else {
                self.ent = nil
            }
        }
        return self.ent?.pointee
    }
}
