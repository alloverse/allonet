//
//  Identity.swift
//  Example
//
//  Created by Patrik on 2020-03-21.
//  Copyright © 2020 voxar. All rights reserved.
//

import Foundation


public struct AgentIdentity: Codable, Equatable {
    public var display_name: String
}
//%Interaction{body: ["announce", "version", 1, "identity", %{display_name: "Mario"}, "spawn_avatar", %{children: [%{geometry: %{name: "lefthand", type: "hardcoded-model"}, intent: %{actuate_pose: "hand/left"}}, %{collider: %{depth: 0.1, height: 0.1, type: "box", width: 0.1}, geometry: %{name: "head", type: "hardcoded-model"}, intent: %{actuate_pose: "head"}}]}], from_entity: "", request_id: "ANN0", to_entity: "place", type: "request"}
