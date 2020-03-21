//
//  Json.swift
//  Example
//
//  Created by Patrik on 2020-03-21.
//  Copyright © 2020 voxar. All rights reserved.
//

import Foundation

let alloJsonEncoder: JSONEncoder = {
    var encoder = JSONEncoder()
    encoder.dataEncodingStrategy = .base64
    return encoder
}()

let alloJsonDecoder: JSONDecoder = {
    let decoder = JSONDecoder()
    decoder.dataDecodingStrategy = .base64
    return decoder
}()

extension Encodable {
    var jsonString: String {
        let data = try! alloJsonEncoder.encode(self)
        return String(data: data, encoding: .utf8)!
    }
}

extension Decodable {
    static func from(json: String) -> Self {
        .from(json: json.data(using: .utf8)!)
    }
    
    static func from(json: Data) -> Self {
        try! alloJsonDecoder.decode(self, from: json)
    }
}
