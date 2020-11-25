
import CAllonet
import Foundation

public extension Encodable {
    var jsonString: String {
        let encoder = JSONEncoder()
        let data = try! encoder.encode(self)
        return String(data: data, encoding: .utf8)!
    }
}

public protocol ClientDelegate: class {
    func client(_ client: Client, received state: allo_state)
    func client(_ client: Client, received interaction: allo_interaction)
    func clientDidDisconnect(_ client: Client)
}

fileprivate func stateCallback(client: UnsafeMutablePointer<alloclient>!, state: UnsafeMutablePointer<allo_state>!) {
    guard let state = state?.pointee else { return }
    let client = Unmanaged<Client>.fromOpaque(client.pointee._backref!).takeUnretainedValue()
    client.delegate?.client(client, received: state)
}

fileprivate func interactionCallback(client: UnsafeMutablePointer<alloclient>!, interaction: UnsafeMutablePointer<allo_interaction>!) -> Bool {
    guard let interaction = interaction?.pointee else { return false }
    let client = Unmanaged<Client>.fromOpaque(client.pointee._backref!).takeUnretainedValue()
    client.delegate?.client(client, received: interaction)
    return true
}

fileprivate func disconnectedCallback(client: UnsafeMutablePointer<alloclient>!, code: alloerror, message: UnsafePointer<Int8>?) -> Void {
    let client = Unmanaged<Client>.fromOpaque(client.pointee._backref!).takeUnretainedValue()
    client.delegate?.clientDidDisconnect(client)
}


public class Client {
    
    public weak var delegate: ClientDelegate? = nil
    
    public struct AgentIdentity: Codable {
        public var display_name: String
    }
    
    public func connect(url: String, displayName: String, components: [Component]) -> Bool {
        let json = Dictionary<String, AnyComponent>(uniqueKeysWithValues: components.map { component in
            (component.key, AnyComponent(base: component))
        }).jsonString
        return connect(
            url: url,
            identity: ["display_name": displayName].jsonString,
            avatar: json
        )
    }
    
    
    /**
     Connect to an alloplace.
     
     - returns:
        A Client object, or nil on error
     
     - parameters:
        - url: URL to an alloplace server, like alloplace://nevyn.places.alloverse.com
        - identity: JSON dict describing user, as per https://github.com/alloverse/docs/blob/master/specifications/README.md#agent-identity
        - avatar: JSON dict describing components, as per "components" of https://github.com/alloverse/docs/blob/master/specifications/README.md#entity
     
    */
    public func connect(url: String, identity: String, avatar: String) -> Bool {
        return url.withCString { urlPointer in
            identity.withCString { identity in
                avatar.withCString { avatar in
                    return alloclient_connect(client, url, identity, avatar)
                }
            }
        }
    }
    
    var client: UnsafeMutablePointer<alloclient>
    
    public init(threaded: Bool = false) {
        
        self.client = alloclient_create(threaded)
        self.client.pointee._backref = Unmanaged.passUnretained(self).toOpaque()
        self.client.pointee.state_callback = stateCallback
        self.client.pointee.disconnected_callback = disconnectedCallback
        self.client.pointee.interaction_callback = interactionCallback
    }
    
    deinit {
        client.pointee.disconnected_callback = nil
        client.pointee.interaction_callback = nil
        client.pointee.state_callback = nil
        disconnect()
    }

    
    /**
     Disconnect from the alloverse.
     
     - Important:
     After calling disconnect you must not call any other methods on the instance
     */
    func disconnect() {
        withUnsafeMutablePointer(to: &client) { client in
            alloclient_disconnect(client.pointee, 0)
        }
    }
    
    /**
     Process incoming and outgoing network traffic
     
     Call regularly at 20hz to process incoming and outgoing network traffic.
    */
    func poll() {
        alloclient_poll(client, 10)
    }
    
    /**
     Have one of your entites interact with another entity.
     
     Use this same method to send back a response when you get a request.
    
     - parameters:
        - interaction: interaction to send.
     
     - seealso:
     [Interaction Specification]](https://github.com/alloverse/docs/blob/master/specifications/interactions.md)
    */
    func send(interaction: allo_interaction) {
        var interaction = interaction
        withUnsafeMutablePointer(to: &client) { client in
            withUnsafeMutablePointer(to: &interaction) { interaction in
                alloclient_send_interaction(client.pointee, interaction)
            }
        }
    }
    
    /** Change this client's movement/action intent.
    - seealso:
        [Intent Specification](https://github.com/alloverse/docs/blob/master/specifications/README.md#entity-intent)
    */
    func set(intent: allo_client_intent) {
        withUnsafeMutablePointer(to: &client) { client in
            withUnsafePointer(to: intent) { intent in
                alloclient_set_intent(client.pointee, intent)
            }
        }
    }
}
