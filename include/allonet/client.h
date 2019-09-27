#ifndef ALLONET_CLIENT_H
#define ALLONET_CLIENT_H
#include "state.h"
#include "net.h"

typedef struct alloclient alloclient;
typedef struct alloclient {
    // call this to change this client's movement/action intent
    void (*set_intent)(alloclient *client, allo_client_intent intent);

    /** Call this to have one of your entites interact with another entity.
      * Use this same method to send back a response when you get a request. 
      * 
      * @param type: oneway, request, response or publication
      * @param sender_entity_id: ID of your entity, e g your avatar.
      * @param receiver_entity_id: the entity your sender wants to interact with
      * @param request_id: The ID of this request or response
      * @param body: JSON list of interaction message
      * @see https://github.com/alloverse/docs/blob/master/specifications/interactions.md
      */
    void (*interact)(
        alloclient *client,
        const char *interaction_type,
        const char *sender_entity_id,
        const char *receiver_entity_id,
        const char *request_id,
        const char *body
    );
    
    void (*disconnect)(alloclient *client, int reason);

    void (*poll)(alloclient *client);
    
    // set this to get a callback when state changes. data in 'state'
    // is valid only during duration of callback.
    void (*state_callback)(alloclient *client, allo_state *state);

    /** Set this to get a callback when another entity is trying to 
      * interact with one of your entities.
      * 
      * @param type: oneway, request, response or publication
      * @param sender_entity_id: the entity trying to interact with yours
      * @param receiver_entity_id: your entity being interacted with
      * @param request_id: The ID of this request or response
      * @param body: JSON list of interaction message
      * @see https://github.com/alloverse/docs/blob/master/specifications/interactions.md
      */
    void (*interaction_callback)(
        alloclient *client, 
        const char *type,
        const char *sender_entity_id,
        const char *receiver_entity_id,
        const char *request_id,
        const char *body
    );

    // internal
    allo_state state;
    void *_internal;
    void *_backref; // use this as a backref for callbacks  
} alloclient;

/** You can also poll for interactions instead of setting a callback.
 *  If no callback is set, they'll queue up so do pop them after every poll().
 */
allo_interaction *alloclient_pop_interaction();

/** Connect to an alloplace.
 * @param url: URL to an alloplace server, like alloplace://nevyn.places.alloverse.com
 * @param identity: JSON dict describing user, as per https://github.com/alloverse/docs/blob/master/specifications/README.md#agent-identity
 * @param avatar_desc: JSON dict describing components, as per "components" of https://github.com/alloverse/docs/blob/master/specifications/README.md#entity
 */
alloclient *allo_connect(const char *url, const char *identity, const char *avatar_desc);

#endif
