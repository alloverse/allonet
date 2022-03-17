#ifndef ALLONET_CLIENT_H
#define ALLONET_CLIENT_H
#include "state.h"
#include "net.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum alloerror
{
    alloerror_connection_lost = 1000,
    alloerror_client_disconnected = 1001,
    alloerror_initialization_failure = 1002,
    alloerror_outdated_version = 1003,
    alloerror_failed_to_connect = 1004,
    alloerror_kicked_by_admin = 1005,
} alloerror;

typedef enum {
    /// Asset became available
    client_asset_state_now_available,
    /// Asset became unavailable
    client_asset_state_now_unavailable,
    /// Asset was requested but was not available
    client_asset_state_requested_unavailable,
} client_asset_state;

typedef struct allopixel {
  uint8_t r, g, b, a;
} allopixel;

typedef enum allopicture_format {
  allopicture_format_rgba8888,
  allopicture_format_bgra8888,
  allopicture_format_rgb1555,
  allopicture_format_xrgb8888,
  allopicture_format_rgb565,
} allopicture_format;

#define ALLOPICTURE_MAX_PLANE_COUNT 4
typedef struct allopicture {
  // actual pixel data (separately allocated).
  // If non-planar, set only planes[0] and set plane_count=1
  union {
    allopixel *rgba;
    uint16_t *rgb1555;
    uint32_t *xrgb;
    uint16_t *rgb565;
    uint8_t *monochrome;
  } planes[ALLOPICTURE_MAX_PLANE_COUNT];
  allopicture_format format;
  int width, height;
  int plane_count;
  // how many bytes per plane?
  int plane_byte_lengths[ALLOPICTURE_MAX_PLANE_COUNT];
  // how many bytes per row for a given plane?
  int plane_strides[ALLOPICTURE_MAX_PLANE_COUNT];
  // if null, planes[1-4] and the picture itself is free()d after use
  void (*free)(struct allopicture *p); 
  void *userdata;
} allopicture;
void allopicture_free(allopicture *picture);
int allopicture_bpp(allopicture_format fmt);


typedef struct alloclient alloclient;
typedef struct alloclient {
    /** set this to get a callback when state changes. you don't own any of the
     *  data in the callbacks, and you must copy or use any data you need before
     *  the callback returns.
     * @param state     Full world state. Only valid during duration of callback.
     * @param diff      Changes in entities and components since last callback.
     */
    void (*state_callback)(
      alloclient *client, 
      allo_state *state, 
      allo_state_diff *diff
    );

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

    bool (*video_callback)(
        alloclient *client,
        uint32_t track_id,
        allopixel pixels[],
        int32_t pixels_wide,
        int32_t pixels_high
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
     * Please provide the asset bytes between offset and offset+length using the `alloclient_asset_send` method.
     * You an respond by calling `alloclient_asset_send` either directly or at a later time. If you do not have any data for the requested asset then call `alloclient_asset_send` with NULL data and 0 length.
     * @example
     * void _asset_needs_data(alloclient *client, const char *asset_id, size_t offset, size_t length) {
     *  if (app_has_asset(asset_id)) {
     *   // get a pointer to the asset bytes
     *   alloclient_asset_send(client, asset_id, pointer + offset, offset, length, asset_size);
     *  } else {
     *   alloclient_asset_send(client, asset_id, NULL, offset, 0, 0);
     *  }
     * }
     * @note You may return less bytes than requested but you must respond with the requested `offset`
     * @param client The client object
     * @param asset_id An asset identifier
     * @param offset An offset into the data to start reading from
     * @param length The size requested
     */
    void (*asset_request_bytes_callback)(
      alloclient* client,
      const char* asset_id,
      size_t offset,
      size_t length
    );

    /*!
     * You have received data for an asset; write it to your cache.
     * You may request the next chunk of data as a response to this callback.
     * @note If there is an error in response to a request then this method will not be called.
     * @param client The client object
     * @param asset_id The asset identifier
     * @param buffer Bytes
     * @param length The number of bytes available in `buffer`.
     * @param total_size The total size of the asset.
     */
    void (*asset_receive_callback)(
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
    void (*alloclient_send_video)(alloclient *client, int32_t track_id, allopicture *picture);
    void (*alloclient_simulate)(alloclient* client);
    double (*alloclient_get_time)(alloclient* client);
    void (*alloclient_get_stats)(alloclient* client, char *buffer, size_t bufferlen);
    
    // -- Assets
        
    /// Let client know an asset is needed
    /// `asset_state_callback` will let you know when the asset is available or not found.
    /// @param asset_id The asset
    /// @param entity_id Optional entity that needs the asset.
    void (*alloclient_asset_request)(alloclient* client, const char* asset_id, const char* entity_id);
    
    void (*alloclient_asset_send)(alloclient *client, const char *asset_id, const uint8_t *data, size_t offset, size_t length, size_t total_size);
    
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

/** Transmit audio from an entity, e g microphone audio for
  * voice communication. You must send the interaction `allocate_track` to receive a
  * track_id to send from beforehand, to associate a particular audio stream with a specific
  * entity.
  * Everyone nearby your entity will hear the audio.
  * @param track_id Track allocated from `allocate_track` interaction on which to send audio
  * @param pcm 48000 Hz mono PCM audio data
  * @param sample_count Number of samples in `pcm`. Must be 480 or 960 (10ms or 20ms worth of audio)
  *   
  */
void alloclient_send_audio(alloclient *client, int32_t track_id, const int16_t *pcm, size_t sample_count);

/** Transmit video from an entity, e g camera video or screen sharing.
 *  Like alloclient_send_audio, you must `allocate_track` first to receive
 *  a track_id.
 * 
 * @param track_id  Track allocated from `allocate_track` interaction on which to send video
 * @param picture   Picture data for this frame. On memory management: calling this method transfers
 *                  memory ownership to this library. The picture will later either be free()'d if
 *                  the `free` callback is null, or the callback is called to give you a chance to
 *                  recycle the memory or use some custom method of freeing it.
 */
void alloclient_send_video(alloclient *client, int32_t track_id, allopicture *picture);

/*!
 * Request an asset. This might be a texture, a model, a sound or something that
 * you need. You might need it because it's referenced from a component in an entity
 * that you want to draw. If you know which entity is referencing it, you can
 * send it as `entity_id`, but that's optional.
 */
void alloclient_asset_request(alloclient* client, const char* asset_id, const char* entity_id);


/*!
 * Respond to an asset_request_callback
 * Send NULL as `data` if you do not have the requested byte range
 */
void alloclient_asset_send(alloclient *client, const char *asset_id, const uint8_t *data, size_t offset, size_t length, size_t total_size);

/**
  * Run allo_simulate() on the internal world state with our latest intent, so that we get local interpolation
  * of hand movement etc
  */
void alloclient_simulate(alloclient* client);

void alloclient_simulate_root_pose(alloclient *client, const char *avatar_id, float dt, allo_client_intent *intent, allo_m4x4 *outMat);

/** Get current estimated alloplace time that is hopefully uniform across all
  * connected clients; or best-effort if it's out of sync.
  * @return seconds since some arbitrary point in the past
  */
double alloclient_get_time(alloclient* client);

void alloclient_get_stats(alloclient* client, char *buffer, size_t bufferlen);

/** Returns a pointer to the state
 */
allo_state *alloclient_get_state(alloclient *client);

#ifdef __cplusplus
}
#endif

#endif

