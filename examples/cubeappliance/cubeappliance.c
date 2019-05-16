#include <allonet/allonet.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <math.h>

static void interaction(alloclient *client, const char *type, const char *sender_entity_id, const char *receiver_entity_id, const char *request_id, const char *body)
{
    printf("INTERACTION cmd: %s\n", body);
}

int main(int argc, char **argv)
{
    if(!allo_initialize(false)) {
        fprintf(stderr, "Unable to initialize allonet");
        return -1;
    }

    if(argc != 2) {
        fprintf(stderr, "Usage: allodummyclient alloplace://hostname:port\n");
        return -2;
    }

    printf("hello microverse\n");
    // TODO: fill in identity and avatar
    alloclient *client = allo_connect(argv[1], NULL, NULL);
    if(!client) {
        return -3;
    }

    client->interaction_callback = interaction;
    for(;;)
    {
        client->poll(client);
    }
    return 0;
}
