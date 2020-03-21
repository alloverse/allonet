//
//  Components.swift
//  Allonet Example
//
//  Created by Patrik on 2020-03-21.
//  Copyright © 2020 voxar. All rights reserved.
//

import Foundation

public struct Components: Codable, Equatable {
    public var transform: Transform?
    public var geometry: Geometry?
    public var collider: Collider?
    
    // Relationships: fix so empty list can not be a list
    public var relationships: Relationships?
}

extension Components {
    public init(from decoder: Decoder) throws {
        let container = try decoder.container(keyedBy: CodingKeys.self)
        transform = try container.decodeIfPresent(Transform.self, forKey: .transform)
        geometry = try container.decodeIfPresent(Geometry.self, forKey: .geometry)
        collider = try container.decodeIfPresent(Collider.self, forKey: .collider)
        do {
            relationships = try container.decodeIfPresent(Relationships.self, forKey: .relationships)
        } catch {
            print("Could not decode relationships, was probably an empty array instead of dict again..", error)
            relationships = nil
        }
    }
}
