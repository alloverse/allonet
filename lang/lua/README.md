# Allonet Lua bridge

You can use Allonet from your Lua 5.1 script or application, just as you
would from C, with the same API. Here's how:

1. Copy liballonet.so or liballonet.dll to next to your lua script
2. `require "liballonet"` (no extension needed)
3. Use the functions from `allonet`, like `allonet.connect(...)`.

See `sample.lua` in this directory for an example. Here's a snippet to
get you started:

    require("liballonet")
    local json = require("json")
    local client = allonet.create()
    client:connect(
        "alloplace://nevyn.places.alloverse.com",
        json.encode({display_name = "lua-sample"}),
        json.encode({})
    )
    while true do
        client:poll(1.0/20.0) -- poll at 20hz
    end

## Full API

### `allonet.create()`

Allocate a client that you can then configure, and then connect.

### `client:connect(url: string, identity: json-string, avatar: json-string)`

 * url: URL to an alloplace server, like alloplace://nevyn.places.alloverse.com
 * identity: JSON dict describing user, as per https://github.com/alloverse/docs/blob/master/specifications/README.md#agent-identity
 * avatar_desc: JSON dict describing components, as per "components" of https://github.com/alloverse/docs/blob/master/specifications/README.md#entity

### `client:disconnect(reason: integer)`

Disconnect from an alloplace and free all internal state.
`client` is free()d by this call. Call this to deallocate
even if you're already disconnected remotely (and you
notice from the disconnect callback)

### `client:poll(timeout)`

Sends and receives network data, and performs other housekeeping tasks.
Must be called at least at 20hz, preferrably more often.

### `client:simulate(dt: float)`

Perform client-side world state interpolation, running physics and other
entitiy movement. Call every frame for smoother movements.

### `client:send_interaction(interaction: interaction)`

Have one of your entites interact with another entity.
Use this same method to send back a response when you get a request.

`interaction` is a table with these keys:

* type
* sender_entity_id
* receiver_entity_id
* request_id
* body

All are strings according to the `interactions` specification, found here:
https://github.com/alloverse/docs/blob/master/specifications/interactions.md

### `client:set_intent(intent: intent)`

Change this client's movement/action intent.

`intent` is a table with these keys:

* zmovement
* xmovement
* yaw
* pitch

See https://github.com/alloverse/docs/blob/master/specifications/README.md#entity-intent
for description of the fields.

### `state = client:get_state()`

Get the current state of the place, i e a list of all entities
and their components. Returns a table with:

* `revision`: current revision of world, as number
* `entities`: table of unsorted entities

Each entity has:

* `id` as string, uniquely identifying it. Use this to e g send interactions to it.
* `components`, table of components as per components specification.

See more at https://github.com/alloverse/docs/blob/master/specifications/README.md#entity .

### `interaction = client:pop_interaction()`

If you don't like callbacks and haven't `set_interaction_callback`, you can also
call this method until it returns null after each time you call `client:poll` to
fetch interactions. See `send_interaction` for description of the interaction
table.

### `client:set_state_callback(callback: state_callback_fn)`

Set this to get a callback when state changes. Callback function receives
the new state as a parameter.

### `client:set_interaction_callback(callback: interaction_callback_fn)`

Set this to get a callback whenever another entity sends you an interaction.
The callback function receives an `interaction` table immediately upon
interaction.

If you set this, `pop_interaction` will stop working. Choose either method
to receive interactions.

### `client:set_disconnected_callback(callback: disconnected_callback_fn)`

Set this to get a callback when your client is disconnected from the place.

You MUST call `client:disconnect()` in this callback to deallocate all
relevant resources.

This would be a good time to trigger reconnection logic, or to quit your app.

### `client:send_audio(track_id: int16, pcm: string)`

NOTE: You MUST 
[allocate a track](https://github.com/alloverse/docs/blob/master/specifications/interactions.md#entity-wishes-to-transmit-live-media)
before you can send audio on it.

Send audio from one of your entities. The `track_id` should match
the track in your `live_media` component of that component.

For format of `pcm`, see `client.h`. At the time of writing, it had to be:
 * 16-bit signed integer samples
 * at 48khz
 * mono
 * 480 or 960 samples (10 or 20ms worth of audio)

