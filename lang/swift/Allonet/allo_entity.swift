//
//  allo_entity.swift
//  Example
//
//  Created by Patrik on 2020-11-25.
//  Copyright © 2020 voxar. All rights reserved.
//

import Foundation
import CAllonet

extension allo_entity: CustomDebugStringConvertible {
    public var debugDescription: String {
        let id = str(self.id) ?? "nil"
        let ownerId = str(self.owner_agent_id) ?? "nil"
        let components = self.components?.pointee.debugDescription ?? "nil"
        return "allo_entity(id: '\(id)', owner_agent_id: '\(ownerId)', components: \(components)"
    }
}

