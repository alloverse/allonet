
import CAllonet
import Foundation

extension Encodable {
    var jsonString: String {
        let data = try! JSONEncoder().encode(self)
        return String(data: data, encoding: .utf8)!
    }
}

public protocol ClientDelegate: class {
    func client(_ client: Client, received state: Client.State)
    func client(_ client: Client, received interaction: Client.Interaction)
    func clientDidDisconnect(_ client: Client)
}

func stateCallback(client: UnsafeMutablePointer<alloclient>!, state: UnsafeMutablePointer<allo_state>!) {
    guard let state = state?.pointee else { return }
    let client = Unmanaged<Client>.fromOpaque(client.pointee._backref!).takeUnretainedValue()
    client.delegate?.client(client, received: state)
}

func interactionCallback(client: UnsafeMutablePointer<alloclient>!, interaction: UnsafeMutablePointer<allo_interaction>!) {
    guard let interaction = interaction?.pointee else { return }
    let client = Unmanaged<Client>.fromOpaque(client.pointee._backref!).takeUnretainedValue()
    client.delegate?.client(client, received: interaction)
}

func disconnectedCallback(client: UnsafeMutablePointer<alloclient>!) {
    let client = Unmanaged<Client>.fromOpaque(client.pointee._backref!).takeUnretainedValue()
    client.delegate?.clientDidDisconnect(client)
}

public class Client {
    
    public weak var delegate: ClientDelegate? = nil
    
    public typealias State = allo_state
    public typealias Interaction = allo_interaction
    
    public struct AgentIdentity: Codable {
        var display_name: String
    }
    
    public struct Entity: Codable {
        var id: String
        var components: [Component.Key: Component]
    }
    
    public struct Component: Codable {
        public enum Key: String, Codable {
            case transform
        }
        
        public struct Transform: Codable {
            public var position: Vector
            public var rotation: Vector
            
            public struct Vector: Codable {
                var x, y, z: Float
            }
        }
    }
    
    public static func connect(url: URL, identity: AgentIdentity, avatar: Entity) -> Client? {
        return self.connect(
            url: url.absoluteString,
            identity: identity.jsonString,
            avatar: avatar.jsonString
        )
    }
    
    public static func connect(url: String, identity: String, avatar: String) -> Client? {
    
        return url.withCString { urlPointer in
            identity.withCString { identity in
                avatar.withCString { avatar in
                    return allo_connect(url, identity, avatar)?.pointee
                }
            }
        }.flatMap(Client.init)
    }
    
    var client: alloclient
    let uuid = UUID().uuidString
    
    init(client: alloclient) {
        self.client = client
        self.client._backref = Unmanaged.passUnretained(self).toOpaque()
        
        self.client.state_callback = stateCallback
        self.client.disconnected_callback = disconnectedCallback
        self.client.interaction_callback = interactionCallback
    }
    
    deinit {
        client.disconnected_callback = nil
        client.interaction_callback = nil
        client.state_callback = nil
        disconnect(reason: 0)
    }
    
    
    func disconnect(reason: Int) {
        withUnsafeMutablePointer(to: &client) { client in
            alloclient_disconnect(client, 0)
        }
    }
    
    func poll() {
        withUnsafeMutablePointer(to: &client) { client in
            alloclient_poll(client)
        }
    }
    
    func send(interaction: allo_interaction) {
        var interaction = interaction
        withUnsafeMutablePointer(to: &client) { client in
            withUnsafeMutablePointer(to: &interaction) { interaction in
                alloclient_send_interaction(client, interaction)
            }
        }
    }
    
    func set(intent: allo_client_intent) {
        withUnsafeMutablePointer(to: &client) { client in
            alloclient_set_intent(client, intent)
        }
    }
}
