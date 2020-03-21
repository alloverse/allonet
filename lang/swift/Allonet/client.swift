
import CAllonet
import Foundation

fileprivate extension Encodable {
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

fileprivate func stateCallback(client: UnsafeMutablePointer<alloclient>!, state: UnsafeMutablePointer<allo_state>!) {
    guard let state = state?.pointee else { return }
    let client = Unmanaged<Client>.fromOpaque(client.pointee._backref!).takeUnretainedValue()
    client.delegate?.client(client, received: state)
}

fileprivate func interactionCallback(client: UnsafeMutablePointer<alloclient>!, interaction: UnsafeMutablePointer<allo_interaction>!) {
    guard let interaction = interaction?.pointee else { return }
    let client = Unmanaged<Client>.fromOpaque(client.pointee._backref!).takeUnretainedValue()
    client.delegate?.client(client, received: interaction)
}

fileprivate func disconnectedCallback(client: UnsafeMutablePointer<alloclient>!) {
    let client = Unmanaged<Client>.fromOpaque(client.pointee._backref!).takeUnretainedValue()
    client.delegate?.clientDidDisconnect(client)
}

public class Client {
    
    public weak var delegate: ClientDelegate? = nil
    
    public typealias State = allo_state
    public typealias Interaction = allo_interaction
    public typealias Intent = allo_client_intent
    
    public static func connect(url: URL, identity: AgentIdentity, avatar: Entity) -> Client? {
        return self.connect(
            url: url.absoluteString,
            identity: identity.jsonString,
            avatar: avatar.jsonString
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
    
    private init(client: alloclient) {
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
        disconnect()
    }

    
    /**
     Disconnect from the alloverse.
     
     - Important:
     After calling disconnect you must not call any other methods on the instance
     */
    func disconnect() {
        withUnsafeMutablePointer(to: &client) { client in
            alloclient_disconnect(client, 0)
        }
    }
    
    /**
     Process incoming and outgoing network traffic
     
     Call regularly at 20hz to process incoming and outgoing network traffic.
    */
    func poll() {
        withUnsafeMutablePointer(to: &client) { client in
            alloclient_poll(client)
        }
    }
    
    /**
     Have one of your entites interact with another entity.
     
     Use this same method to send back a response when you get a request.
    
     - parameters:
        - interaction: interaction to send.
     
     - seealso:
     [Interaction Specification]](https://github.com/alloverse/docs/blob/master/specifications/interactions.md)
    */
    func send(interaction: Interaction) {
        var interaction = interaction
        withUnsafeMutablePointer(to: &client) { client in
            withUnsafeMutablePointer(to: &interaction) { interaction in
                alloclient_send_interaction(client, interaction)
            }
        }
    }
    
    /** Change this client's movement/action intent.
    - seealso:
        [Intent Specification](https://github.com/alloverse/docs/blob/master/specifications/README.md#entity-intent)
    */
    func set(intent: Intent) {
        withUnsafeMutablePointer(to: &client) { client in
            alloclient_set_intent(client, intent)
        }
    }
}
