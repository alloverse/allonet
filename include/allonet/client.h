#ifndef ALLONET_CLIENT_H
#define ALLONET_CLIENT_H
#include "state.h"
#include "net.h"

typedef enum alloerror
{
    alloerror_connection_lost = 1000,
    alloerror_client_disconnected = 1001,
    alloerror_initialization_failure = 1002,
} alloerror;

typedef struct alloclient alloclient;
typedef struct alloclient {
    /** set this to get a callback when state changes. data in 'state'
      * is valid only during duration of callback.
      */
    void (*state_callback)(alloclient *client, allo_state *state);

    /** Set this to get a callback when another entity is trying to 
      * interact with one of your entities.
      * 
      * @param interaction: interaction received. Freed after callback;
      *                     copy it if you need to keep it.
      * @see https://github.com/alloverse/docs/blob/master/specifications/interactions.md
      */
    void (*interaction_callback)(
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
     */
    void (*audio_callback)(
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
    void  (*disconnected_callback)(
       alloclient *client,
       alloerror code,
       const char *message
    );

    /*! 
     * Someone is asking you if you have asset `asset_id`.
     * @param asset_id sha identifier of the asset
     * @param entity_id Hint of which entity might have the asset
     * @return size_t How big is this asset? Return 0 if you don't have the asset.
     * @discussion If you return >0 from this asset, you will later get
     *             asset_transmission_callback asking you for the data for this asset.
     */
    size_t (*asset_request_callback)(
      alloclient* client,
      const char* asset_id,
      const char* entity_id
    );

    /*! 
     * Please provide data into `buf` starting at `offset` and of at most `chunk_length` bytes
     * to send to the server. Return the number of bytes you actually put into
     * `buf`. You must not provide more or less bytes in total than what you returned from
     * `asset_request_callback`. If you are not ready to provide data, you can return
     * 0 to get a callback slightly later.
     */
    size_t (*asset_send_callback)(
      alloclient* client,
      const char* asset_id,
      char *buf,
      size_t offset,
      size_t chunk_length
    );

    /*!
     * You have received data for an asset; write it to your cache or whatever.
     * `chunk_length` is the number of bytes available in `buf`.
     * When `offset+chunk_length == total_length`, this is your last callback 
     * for this asset.
     */
    void (*asset_receive_callback)(
      alloclient* client,
      const char* asset_id,
      char* buf,
      size_t offset,
      size_t chunk_length,
      size_t total_length
    );

    // internal
    allo_state state;
    void *_internal;
    void *_backref; // use this as a backref for callbacks  
} alloclient;


alloclient *alloclient_create(void);


/** Connect to an alloplace. Must be called once and only once on the returned alloclient from allo_create()
* @param url: URL to an alloplace server, like alloplace://nevyn.places.alloverse.com
* @param identity: JSON dict describing user, as per https://github.com/alloverse/docs/blob/master/specifications/README.md#agent-identity
* @param avatar_desc: JSON dict describing components, as per "components" of https://github.com/alloverse/docs/blob/master/specifications/README.md#entity
*/
bool allo_connect(alloclient *client, const char *url, const char *identity, const char *avatar_desc);

/** Disconnect from an alloplace and free all internal state.
 *  `client` is free()d by this call. Call this to deallocate
 *  even if you're already disconnected remotely (and you
 *  notice from the disconnect callback).
 */
void alloclient_disconnect(alloclient *client, int reason);

/** Call regularly at 20hz to process incoming and outgoing network traffic.
 */
void alloclient_poll(alloclient *client);

/** Have one of your entites interact with another entity.
  * Use this same method to send back a response when you get a request. 
  * 
  * @param interaction: interaction to send. Will not be held, will not be
  *                     freed.
  * @see https://github.com/alloverse/docs/blob/master/specifications/interactions.md
  */
void alloclient_send_interaction(
    alloclient *client,
    allo_interaction *interaction
);

/** Change this client's movement/action intent.
 *  @see https://github.com/alloverse/docs/blob/master/specifications/README.md#entity-intent
 */
void alloclient_set_intent(alloclient *client, allo_client_intent *intent);


/** You can also poll for interactions instead of setting a callback.
 *  If no callback is set, they'll queue up so do pop them after every poll().
 */
allo_interaction *alloclient_pop_interaction(alloclient *client);

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
void alloclient_request_asset(alloclient* client, const char* asset_id, const char* entity_id);


/**
  * Run allo_simulate() on the internal world state with our latest intent, so that we get local interpolation
  * of hand movement etc
  */
void alloclient_simulate(alloclient* client, double dt);

#endif
