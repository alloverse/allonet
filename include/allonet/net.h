#ifndef ALLONET_NET_H
#define ALLONET_NET_H

typedef enum {
    allo_unreliable = 1,
    allo_reliable = 2,
} allo_sendmode;

typedef enum allochannel {
    CHANNEL_STATEDIFFS = 0,
    CHANNEL_COMMANDS,

    CHANNEL_COUNT
} allochannel;

#endif
