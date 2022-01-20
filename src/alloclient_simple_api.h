
#ifndef alloclient_simple_api_h
#define alloclient_simple_api_h

int alloclient_simple_init(int threaded);
int alloclient_simple_free();

/// Issues a command.
/// Returns a json string that is valid until the next call to this method.
/// If the issued command has a return value then you find it under the "return" key in the json.
/// If any events has occurred since the last call to this method then
///  the "events" key contains a chronoligical list of those events.
///  sommand: a json object telling allonet what to do
///
///
/// Poll the allonet connection for data and events
///  {
///     "op": "poll"
///  }
/// Connect to a place
///  {
///     "op": "connect",
///     "url": "alloplace://hello.place.alloverse.com",
///     "identity": {"display_name": "My App Name"},
///     "avatar_spec": {} -- TODO: Where is this format spec?
///  }
/// Interact with objects
///  {
///     "op": "interaction",
///     "interaction": {} -- TODO: Where is this format spec?
///  }
/// Move about in the world
///  {
///     "op": "intent",
///     "intent": {} -- TODO: Where is this format spec?
///  }
char *alloclient_simple_communicate(const char * const command);

/// Returns the specified chunk. The returned buffer is valid until the next call to this method.
/// request
///  {
///     "name": "asset id",
///     "size": 123,
///     "offset": 0,
///     "total_size": 256
///  }
char *alloclient_simple_asset_receive(const char * const request);

#endif /* alloclient_simple_api_h */
