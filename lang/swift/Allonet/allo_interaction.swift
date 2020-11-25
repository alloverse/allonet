//
//  allo_interaction.swift
//  Example
//
//  Created by Patrik on 2020-11-25.
//  Copyright © 2020 voxar. All rights reserved.
//

import Foundation
import CAllonet

extension allo_interaction: CustomDebugStringConvertible {
    public var debugDescription: String {
        let type = str(self.type) ?? "nil"
        let sender = str(self.sender_entity_id) ?? "nil"
        let receiver = str(self.receiver_entity_id) ?? "nil"
        let request = str(self.request_id) ?? "nil"
        let body = str(self.body) ?? "nil"
        return "allo_interaction(type: '\(type)', sender_entity_id: '\(sender)', receiver_entity_id: '\(receiver)', request_id: '\(request)', body: '\(body)'"
    }
}
