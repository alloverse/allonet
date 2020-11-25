//
//  component.swift
//  Example
//
//  Created by Patrik on 2020-11-25.
//  Copyright © 2020 voxar. All rights reserved.
//

import Foundation
import CAllonet

public protocol Component: Codable {
    static var key: String { get }
    var key: String { get }
}

extension Component {
    public var key: String { type(of: self).key }
}

public struct AnyComponent: Encodable {
    public let base: Component
    
    public var key: String { base.key }
    
    public init(base: Component) {
        self.base = base
    }
    
    public func encode(to encoder: Encoder) throws {
        try base.encode(to: encoder)
    }
}

public struct Transform: Component {
    public static var key: String = "transform"
    
    /// 16 double values defining the transform.
    public var matrix: [Double]
    
    public init() {
        self.init(matrix: [1.0,0.0,0.0,0.0, 0.0,1.0,0.0,0.0, 0.0,0.0,1.0,0.0, 0.0,0.0,0.0,1.0])
    }
    
    public init(matrix: [Double]) {
        self.matrix = matrix
    }
    
    public func offset(x: Double = 0, y: Double = 0, z: Double = 0) -> Transform {
        var m = matrix
        m[12] += x
        m[13] += y
        m[14] += z
        return Transform(matrix: m)
    }
}

public struct HardcodedGeometry: Component {
    public static var key: String = "geometry"
    
    enum KnownNames: String {
        case hand = "hand"
    }
    
    var type: String = "hardcoded-model"
    public var name: String
    
    init(name: String) {
        self.name = name
    }
    
    init(name: KnownNames) {
        self.init(name: name.rawValue)
    }
}

public struct InlineGeometry: Component {
    public static var key: String = "geometry"
    
    /// vertices is a required list of lists, each sub-list containing x, y, z coordinates for your vertices
    public var vertices: [[Double]]
    /// normals is an optional list of lists, each sub-list containing the x, y, z coords for the normal at the corresponding vertex.
    public var normals: [[Double]]?
    /// uvs is an optional list of lists, each sub-list containing the u, v texture coordinates at the corresponding vertex
    public var uvs: [[Double]]?
    /// triangles is a required list of lists. Each sub-list is three integers, which are indices into the above arrays, forming a triangle
    public var triangles: [[Int]]
    
    public init(
        vertices: [[Double]],
        normals: [[Double]]? = nil,
        uvs: [[Double]]? = nil,
        triangles: [[Int]]
    ) {
        self.vertices = vertices
        self.normals = normals
        self.uvs = uvs
        self.triangles = triangles
    }
    
    public static let box = InlineGeometry(
        vertices:   [[0, 0, 0], [1, 0, 0], [1, 0, 1], [0, 0, 1],
                     [0, 1, 0], [1, 1, 0], [1, 1, 1], [0, 1, 1]],
//        uvs:        [[0, 0], [1, 0], [1, 1], [0, 1], [1, 1]],
        triangles:  [[0, 2, 1], [0, 3, 2],//bottom
                     [0, 1, 7], [0, 7, 4],//facing
                     [0, 4, 3], [0, 4, 5],//left
                     [1, 2, 7], [1, 6, 2],//right
                     [4, 6, 5], [4, 7, 6],//top
                     [3, 5, 6], [3, 6, 2],//back
        ]
    )
}

public struct Material: Component {
    public static var key: String = "material"
    
    /// An array of R, G, B and A value to set as base color for the thing being rendered. Default is white
    public var color: [Double] = [1.0, 1.0, 1.0, 1.0]
    /// Optional ame of hard-coded shader to use for this object. Currently allows plain and pbr. Default is plain
    public var shader_name: String = "plain"
    /// optional texture data as base64encoded png. Default is none
    public var texture: String?
    
    public init(color: [Double] = [1.0, 1.0, 1.0, 1.0], shaderName: String = "plain", texture: String? = nil) {
        self.color = color
        self.shader_name = shaderName
        self.texture = texture
    }
}

public struct Text: Component {
    public static var key: String = "text"
    
    public var string: String
    /// height: The height of the text. 1 unit = 1 meter.
    public var height: Double
    /// wrap: The width in meters at which to wrap the text, if at all.
    public var wrap: Double?
    /// halign: Horizontal alignment; "center", "left" or "right".
    public var halign: HAlign
    
    public enum HAlign: String, Codable {
        case left, right, center
    }
    
    public init(string: String, height: Double, wrap: Double, halign: HAlign) {
        self.string = string
        self.height = height
        self.wrap = wrap
        self.halign = halign
    }
}

public struct Collider: Component {
    public static var key: String = "collider"
    
    public var type: String = "box"
    public var width: Double
    public var height: Double
    public var depth: Double
    
    public static let box = Collider(type: "box", width: 1, height: 1, depth: 1)
}
