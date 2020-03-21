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
    identity: AgentIdentity(display_name: "Voxar via Swift"),
    avatar: Entity(children: [])
)

class Delegate: ClientDelegate {
    func clientDidDisconnect(_ client: Client) {
        print("Disconnected")
    }
    
    func client(_ client: Client, received state: Client.State) {
        print("Did receive state:", state)
    }
    
    func client(_ client: Client, received interaction: Client.Interaction) {
        print("Did receive interaction:", interaction)
    }
}

let delegate = Delegate()
client?.delegate = delegate

print(client)
for _ in 0...50 {
    client?.poll()
    sleep(1)
}


