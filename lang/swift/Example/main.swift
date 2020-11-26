//
//  main.swift
//  allonet_swift
//
//  Created by Patrik Sjöberg on 2019-11-15.
//  Copyright © 2019 voxar. All rights reserved.
//

import Foundation
import CAllonet

class MyClient: ClientDelegate {
    
    func client(_ client: Client, received interaction: allo_interaction) {
        print("I got an interaction: \(interaction)")
    }
    
    func client(_ client: Client, received state: allo_state) {
        print("I got a state: \(state.debugDescription)")
    }
    
    func clientDidDisconnect(_ client: Client) {
        print("I was disconnected")
    }
}

print("Starting...")
let delegate = MyClient()
let client = Client()
client.delegate = delegate
    
let didConnect = client.connect(
    url: "alloplace://localhost",
    displayName: "Voxar via Swift",
    components: [
//        InlineGeometry.box,
        HardcodedGeometry(name: "avatars/animal/left-hand"),
        Collider.box,
        Transform().offset(y: 1)
    ]
)

guard didConnect else {
    print("Connection failed");
    exit(1)
}

print(client)

client.request(asset: "hello");

for _ in 0...5 {
    print("polling")
    client.poll()
    sleep(1)
}

client.disconnect()

print("done")
