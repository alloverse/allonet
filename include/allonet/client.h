#ifndef ALLONET_CLIENT_H
#define ALLONET_CLIENT_H
#include "state.h"
#include "net.h"

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
        alloclient *client
    );

    // internal
    allo_state state;
    void *_internal;
    void *_backref; // use this as a backref for callbacks  
} alloclient;

/** Connect to an alloplace.
 * @param url: URL to an alloplace server, like alloplace://nevyn.places.alloverse.com
 * @param identity: JSON dict describing user, as per https://github.com/alloverse/docs/blob/master/specifications/README.md#agent-identity
 * @param avatar_desc: JSON dict describing components, as per "components" of https://github.com/alloverse/docs/blob/master/specifications/README.md#entity
 */
alloclient *allo_connect(const char *url, const char *identity, const char *avatar_desc);

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
void alloclient_set_intent(alloclient *client, allo_client_intent intent);


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

/**
  * Run allo_simulate() on the internal world state with our latest intent, so that we get local interpolation
  * of hand movement etc
  */
void alloclient_simulate(alloclient* client, double dt);

#endif