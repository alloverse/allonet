#ifndef ALLONET_SERVER_H
#define ALLONET_SERVER_H
#include "inlinesys/queue.h"
#include "state.h"
#include "net.h"

static const int allo_udp_port = 21337;
static const int allo_client_count_max = 128;

// excluding null terminating byte
#define AGENT_ID_LENGTH 16

typedef struct alloserver_client {
    allo_client_intent *intent;
    char *avatar_entity_id;
    char agent_id[AGENT_ID_LENGTH+1];
    cJSON *identity;

    // private
    void *_internal;
    void *_backref;
    LIST_ENTRY(alloserver_client) pointers;
} alloserver_client;

typedef struct alloserver alloserver;
struct alloserver {
    // handle incoming events for at most duration_ms. returns true if event was handled
    bool (*interbeat)(alloserver *serv, int duration_ms);
    
    // raw json as delivered from client (intent or interaction)
    void (*raw_indata_callback)(alloserver *serv, alloserver_client *client, allochannel channel, const uint8_t *data, size_t data_length);
    
    // list of clients changed; either `added` or `removed` is set.
    void (*clients_callback)(alloserver *serv, alloserver_client *added, alloserver_client *removed);

    // internal
    void (*send)(alloserver *serv, alloserver_client *client, allochannel channel, const uint8_t *buf, int len);
    allo_state *state;
    LIST_HEAD(alloserver_client_list, alloserver_client) clients;

    void *_backref; // use this as a backref for callbacks
    void *_internal; // used within server.c to hide impl
    int _port;
};

// send 0 for any host or any port
alloserver *allo_listen(int listenhost, int port);

struct _ENetPacket;

void alloserv_send_enet(alloserver *serv, alloserver_client *client, allochannel channel, struct _ENetPacket *packet);

// immediately shutdown the server
void alloserv_stop(alloserver* serv);

// disconnect one client for one reason, first transmitting all its messages.
// clients_callback() is called once the disconnection is successful.
void alloserv_disconnect(alloserver *serv, alloserver_client *client, int reason_code);

size_t alloserv_get_client_stats(alloserver* serv, alloserver_client *client, char *buffer, size_t bufferlen, bool header);

void alloserv_get_stats(alloserver* serv, char *buffer, size_t bufferlen);

// run a minimal standalone C server. returns when it shuts down. false means it broke.
bool alloserv_run_standalone(int listenhost, int port, const char *placename);

// start it but don't run it. returns allosocket.
alloserver *alloserv_start_standalone(int listenhost, int port, const char *placename);
// call this frequently to run it. returns false if server has broken and shut down; then you should call stop on it to clean up.
bool alloserv_poll_standalone(int allosocket);
// and then call this to stop and clean up state.
void alloserv_stop_standalone();

const char *alloserv_describe_client(alloserver_client *client);


// internal
int allo_socket_for_select(alloserver *server);

#endif
