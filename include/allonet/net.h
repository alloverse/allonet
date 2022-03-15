#ifndef ALLONET_NET_H
#define ALLONET_NET_H

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus


typedef enum {
    allo_unreliable = 1,
    allo_reliable = 2,
} allo_sendmode;

typedef enum allochannel {
    CHANNEL_AUDIO = 0,      // unreliable
    CHANNEL_COMMANDS = 1,   // reliable
    CHANNEL_STATEDIFFS = 2, // unreliable
    CHANNEL_ASSETS = 3,     // reliable
    CHANNEL_VIDEO = 4,      // unreliable
    CHANNEL_CLOCK = 5,      // unreliable
    
    CHANNEL_COUNT
} allochannel;

extern const char *GetAllonetVersion(); // 3.1.4.g123abc
extern const char *GetAllonetNumericVersion(); // 3.1.4
extern const char *GetAllonetGitHash(); // g123abc
extern int GetAllonetProtocolVersion(); // 3

#ifdef __cplusplus
}
#endif // __cplusplus

#endif
