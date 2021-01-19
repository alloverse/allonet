#ifndef ALLONET_CLIENT_H
#define ALLONET_CLIENT_H
#include "state.h"
#include "net.h"

typedef enum alloerror
{
    alloerror_connection_lost = 1000,
    alloerror_client_disconnected = 1001,
    alloerror_initialization_failure = 1002,
    alloerror_outdated_version = 1003,
    alloerror_failed_to_connect = 1004,
} alloerror;

typedef enum {
    /// Asset became available
    client_asset_state_now_available,
    /// Asset became unavailable
    client_asset_state_now_unavailable,
    /// Asset was requested but was not available
    client_asset_state_requested_unavailable,
} client_asset_state;

typedef struct alloclient alloclient;
typedef struct alloclient {
    /** set this to get a callback when state changes. 
     * @param state Full world state. Only valid during duration of callback.
     * 
     */
    void (*state_callback)(alloclient *client, allo_state *state);

    /** Set this to get a callback when another entity is trying to 
      * interact with one of your entities.
      * 
      * @param interaction: interaction received. Freed after callback;
      *                     copy it if you need to keep it.
      * @return bool: whether the caller should free the interaction afterwards (if you return false,
      *               you have to allo_interaction_free(interaction) yourself later).
      * @see https://github.com/alloverse/docs/blob/master/specifications/interactions.md
      */
    bool (*interaction_callback)(
        alloclient *client, 
        allo_interaction *interaction
    );

    /** Set this to get a callback when there is audio data available
     *  in an incoming audio stream track. Match it to a live_media component
     *  to figure out which entity is transmitting it, and thus at what
     *  location in 3d space to play it at. 
     * 
     *  @param track_id: which track/entity is transmitting this audio
     *  @param pcm: n samples of 48000 Hz mono PCM audio data (most often 480 samples, 10ms, 960 bytes)
     *  @param samples_decoded: 'n': how many samples in pcm
     *  @return bool: whether the caller should free the pcm afterwards (if you return false,
     *                you have to free(pcm) yourself later).
     */
    bool (*audio_callback)(
        alloclient *client,
        uint32_t track_id,
        int16_t pcm[],
        int32_t samples_decoded
    );


    /** You were disconnected from the server. This is
     *  never called in response to a local alloclient_disconnect;
     *  
     *  To free up resources, you must call alloclient_disconnect() after
     *  receiving this callback.
     */
    void (*disconnected_callback)(
       alloclient *client,
       alloerror code,
       const char *message
    );

    /*! 
     * Please provide data into `buf` starting at `offset` and of at most `chunk_length` bytes
     * to send to the server.
     * @discussion You may write less than `length` bytes to the `buffer`, but must never write more.
     * @param client The client object
     * @param asset_id An asset identifier
     * @param buffer Buffer to write data to
     * @param offset An offset into the data to start reading from
     * @param length The size requested
     * @param out_total_size Write the total number of bytes of the asset
     * @return The number of bytes you actually put into the `buffer`, or 0 if you do not have any data to send for the requested asset and range.
     */
    size_t (*asset_send_callback)(
      alloclient* client,
      const char* asset_id,
      uint8_t *buffer,
      size_t offset,
      size_t length,
      size_t *out_total_size
    );

    /*!
     * You have received data for an asset; write it to your cache or whatever.
     * @param client The client object
     * @param asset_id The asset identifier
     * @param buffer Bytes
     * @param length Is the number of bytes available in `buffer`.
     * @param total_size The total size of the asset.
     * @return true if you want to continue receving bytes for this asset.
     */
    bool (*asset_receive_callback)(
      alloclient* client,
      const char* asset_id,
      const uint8_t* buffer,
      size_t offset,
      size_t length,
      size_t total_size
    );
    
    /*!
     * The state of an asset has changed
     */
    void (*asset_state_callback)(
      alloclient *client,
      const char *asset_id,
      client_asset_state state
    );
    
    // internal
    allo_state _state;
    void *_internal;
    void *_internal2;
    void *_backref; // use this as a backref for callbacks  

    void (*raw_state_delta_callback)(alloclient *client, cJSON *delta);
    void (*clock_callback)(alloclient *client, double latency, double server_delta);
    double clock_latency;
    double clock_deltaToServer;

    bool (*alloclient_connect)(alloclient *client, const char *url, const char *identity, const char *avatar_desc);
    void (*alloclient_disconnect)(alloclient *client, int reason);
    bool (*alloclient_poll)(alloclient *client, int timeout_ms);
    void (*alloclient_send_interaction)(alloclient *client, allo_interaction *interaction);
    void (*alloclient_set_intent)(alloclient *client, const allo_client_intent *intent);
    void (*alloclient_send_audio)(alloclient *client, int32_t track_id, const int16_t *pcm, size_t sample_count);
    void (*alloclient_simulate)(alloclient* client);
    double (*alloclient_get_time)(alloclient* client);
    void (*alloclient_get_stats)(alloclient* client, char *buffer, size_t bufferlen);
    
    // -- Assets
        
    /// Let client know an asset is needed
    /// `asset_state_callback` will let you know when the asset is available or not found.
    /// @param asset_id The asset
    /// @param entity_id Optional entity that needs the asset.
    void (*alloclient_asset_request)(alloclient* client, const char* asset_id, const char* entity_id);
    
} alloclient;

/**
 * @param threaded: whether to run the network code inline and blocking on this thread, or on its own thread
 */
alloclient *alloclient_create(bool threaded);


/** Connect to an alloplace. Must be called once and only once on the returned alloclient from allo_create()
* @param url: URL to an alloplace server, like alloplace://nevyn.places.alloverse.com
* @param identity: JSON dict describing user, as per https://github.com/alloverse/docs/blob/master/specifications/README.md#agent-identity
* @param avatar_desc: JSON dict describing components, as per "components" of https://github.com/alloverse/docs/blob/master/specifications/README.md#entity
*/
bool alloclient_connect(alloclient *client, const char *url, const char *identity, const char *avatar_desc);

/** Disconnect from an alloplace and free all internal state.
 *  `client` is free()d by this call. Call this to deallocate
 *  even if you're already disconnected remotely (and you
 *  notice from the disconnect callback).
 */
void alloclient_disconnect(alloclient *client, int reason);

/** Send and receive buffered data synchronously now. Loops over all queued
 network messages until the queue is empty.
 @param timeout_ms how many ms to wait for incoming messages before giving up. Default 10.
 @discussion Call regularly at 20hz to process incoming and outgoing network traffic.
 @return bool whether any messages were parsed
 */
bool alloclient_poll(alloclient *client, int timeout_ms);


/** Have one of your entites interact with another entity.
  * Use this same method to send back a response when you get a request. 
  * 
  * @param interaction: interaction to send. Will not be held, will not be
  *                     freed.
  * @see https://github.com/alloverse/docs/blob/master/specifications/interactions.md
  */
void alloclient_send_interaction(alloclient *client, allo_interaction *interaction);

/** Change this client's movement/action intent.
 *  @see https://github.com/alloverse/docs/blob/master/specifications/README.md#entity-intent
 */
void alloclient_set_intent(alloclient *client, const allo_client_intent *intent);

/** Transmit audio from your avatar, e g microphone audio for
  * voice communication.
  * Everyone nearby you avatar will hear the audio.
  * @param track_id Track allocated from `allocate_track` interaction on which to send audio
  * @param pcm 48000 Hz mono PCM audio data
  * @param sample_count Number of samples in `pcm`. Must be 480 or 960 (10ms or 20ms worth of audio)
  *   
  */
void alloclient_send_audio(alloclient *client, int32_t track_id, const int16_t *pcm, size_t sample_count);

/*!
 * Request an asset. This might be a texture, a model, a sound or something that
 * you need. You might need it because it's referenced from a component in an entity
 * that you want to draw. If you know which entity is referencing it, you can
 * send it as `entity_id`, but that's optional.
 */
void alloclient_asset_request(alloclient* client, const char* asset_id, const char* entity_id);


/**
  * Run allo_simulate() on the internal world state with our latest intent, so that we get local interpolation
  * of hand movement etc
  */
void alloclient_simulate(alloclient* client);

/** Get current estimated alloplace time that is hopefully uniform across all
  * connected clients; or best-effort if it's out of sync.
  * @return seconds since some arbitrary point in the past
  */
double alloclient_get_time(alloclient* client);

void alloclient_get_stats(alloclient* client, char *buffer, size_t bufferlen);

#endif
