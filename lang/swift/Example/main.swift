//
//  main.swift
//  allonet_swift
//
//  Created by Patrik Sjöberg on 2019-11-15.
//  Copyright © 2019 voxar. All rights reserved.
//

import Foundation


print("Starting...")
let client = Client.connect(
    url: URL(string: "alloplace://localhost")!,
    identity: Client.AgentIdentity(display_name: "Voxar via Swift"),
    avatar: Client.Entity(id: UUID().uuidString, components: [:])
)

print(client)
for _ in 0...50 {
    client?.poll()
    sleep(1)
}


