//
//  Geometry.swift
//  Allonet Example
//
//  Created by Patrik on 2020-03-21.
//  Copyright © 2020 voxar. All rights reserved.
//

import Foundation


public struct Geometry: Codable, Equatable {
    public var type: GeometryType
    public var vertices: [Vertex]?
    public var uvs: [UV]?
    public var triangles: [Triangle]?
    public var texture: Data?
    
    public enum GeometryType: String, Codable, Equatable {
        case inline = "inline"
        case hardcoded = "hardcoded-model"
    }
    
    public enum HardcodedModel: String, Codable, Equatable {
        case head
    }
    
    
    public struct Vertex: Codable, Equatable, CustomDebugStringConvertible {
        var data: [Float]
        var x: Float { data[0] }
        var y: Float { data[1] }
        var z: Float { data[2] }
        
        struct DecodingError: Error {}
        
        public init(x: Float, y: Float, z: Float) {
            data = [x, y, z]
        }
        
        public init(from decoder: Decoder) throws {
            let container = try decoder.singleValueContainer()
            data = try container.decode([Float].self)
            guard data.count == 3 else { throw DecodingError() }
        }
        
        public func encode(to encoder: Encoder) throws {
            var container = encoder.singleValueContainer()
            try container.encode(data)
        }
        
        public var debugDescription: String {
            "Vertex(\(x), \(y), \(z))"
        }
    }
    
    public struct Triangle: Codable, Equatable, CustomDebugStringConvertible {
        var data: [Int]
        var a: Int { data[0] }
        var b: Int { data[1] }
        var c: Int { data[2] }
        
        struct DecodingError: Error {}
        
        public init(a: Int, b: Int, c: Int) {
            data = [a, b, c]
        }
        
        public init(from decoder: Decoder) throws {
            let container = try decoder.singleValueContainer()
            data = try container.decode([Int].self)
            guard data.count == 3 else { throw DecodingError() }
        }
        
        public func encode(to encoder: Encoder) throws {
            var container = encoder.singleValueContainer()
            try container.encode(data)
        }
        
        public var debugDescription: String {
            "Triangle(\(a), \(b), \(c))"
        }
        
    }
    
    public struct UV: Codable, Equatable, CustomDebugStringConvertible {
        var data: [Float]
        var u: Float { data[0] }
        var v: Float { data[1] }
        
        struct DecodingError: Error {}
        
        public init(u: Float, v: Float) {
            data = [u, v]
        }
        
        public init(from decoder: Decoder) throws {
            let container = try decoder.singleValueContainer()
            data = try container.decode([Float].self)
            guard data.count == 2 else { throw DecodingError() }
        }
        
        public func encode(to encoder: Encoder) throws {
            var container = encoder.singleValueContainer()
            try container.encode(data)
        }
        
        public var debugDescription: String {
            "UV(\(u), \(v))"
        }
    }
}
