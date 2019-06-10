#include <allonet/allonet.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <math.h>
#include <cJSON/cJSON.h>
#include "../../src/util.h"

#define _WIN32

#ifndef _WIN32
#include <termios.h>
#include <unistd.h>
#include <sys/select.h>
struct termios orig_termios;

void reset_terminal_mode()
{
    tcsetattr(0, TCSANOW, &orig_termios);
}

void set_conio_terminal_mode()
{
    struct termios new_termios;

    /* take two copies - one for now, one for later */
    tcgetattr(0, &orig_termios);
    memcpy(&new_termios, &orig_termios, sizeof(new_termios));

    /* register cleanup handler, and set the new terminal mode */
    atexit(reset_terminal_mode);
    cfmakeraw(&new_termios);
    tcsetattr(0, TCSANOW, &new_termios);
}
int kbhit()
{
    struct timeval tv = { 0L, 0L };
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(0, &fds);
    return select(1, &fds, NULL, NULL, &tv);
}

int getch()
{
    int r;
    unsigned char c;
    if ((r = read(0, &c, sizeof(c))) < 0) {
        return r;
    } else {
        return c;
    }
}

#endif //ndef _WIN32

static const char *me;

static void interaction(
    alloclient *client, 
    const char *type,
    const char *sender_entity_id,
    const char *receiver_entity_id,
    const char *request_id,
    const char *bodystr
)
{
    cJSON *body = cJSON_Parse(bodystr);
    const char *interaction_name = cJSON_GetArrayItem(body, 0)->valuestring;

    printf(
        "INTERACTION\n\tType: %s\n\tSender: %s\n\tReceiver: %s\n\tID: %s\n\tBody: %s\n", 
        type, sender_entity_id, receiver_entity_id, request_id, bodystr
    );
    if(strcmp(interaction_name, "announce") == 0) {
        me = strdup(cJSON_GetArrayItem(body, 1)->valuestring);

        client->interact(client, "request", me, "place", "123", "[\"lol\", 1, 2, 3]");
    }

    cJSON_Delete(body);
}


int main(int argc, char **argv)
{
    if(!allo_initialize(false)) {
        fprintf(stderr, "Unable to initialize allonet");
        return -1;
    }

    if(argc != 3) {
        fprintf(stderr, "Usage: allodummyclient username alloplace://hostname:port\n");
        return -2;
    }

    printf("hello microverse\n");

    cJSON *avatardesco = cjson_create_object(
        "geometry", cjson_create_object(
            "type", cJSON_CreateString("tristrip"),
            "tris", cjson_create_list(
                cJSON_CreateNumber(0.0), cJSON_CreateNumber(1.0), cJSON_CreateNumber(0.0),
                cJSON_CreateNumber(1.0), cJSON_CreateNumber(0.0), cJSON_CreateNumber(0.0),
                cJSON_CreateNumber(-1.0), cJSON_CreateNumber(0.0), cJSON_CreateNumber(0.0),
                NULL
            ),
            "color", cjson_create_list(
                cJSON_CreateNumber(1.0), cJSON_CreateNumber(0.0), cJSON_CreateNumber(0.0), cJSON_CreateNumber(1.0), NULL
            ),
            NULL
        ),
        NULL
    );
    const char *avatardesc = cJSON_Print(avatardesco);

    char *identity = (char*)calloc(1, 255);
    snprintf(identity, 255, "{\"display_name\": \"%s\"}", argv[1]);
    alloclient *client = allo_connect(argv[2], identity, avatardesc);
    cJSON_Delete(avatardesco);
    free((void*)avatardesc);

    if(!client) {
        return -3;
    }

#ifndef _WIN32
    set_conio_terminal_mode();
#endif
    client->interaction_callback = interaction;

    for(;;)
    {
#ifndef _WIN32
        if(kbhit()) {
            int ch = getch();
            allo_client_intent intent = {
                .zmovement = ch=='w' ? 1 : ch == 's' ? -1 : 0,
                .xmovement = ch=='d' ? 1 : ch == 'a' ? -1 : 0,
            };
            client->set_intent(client, intent);
        }
#endif
        allo_client_intent intent = {
            .zmovement = 1,
            .xmovement = 0,
        };
        client->set_intent(client, intent);
        client->poll(client);
    }
    return 0;
}
