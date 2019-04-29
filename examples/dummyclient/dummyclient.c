#include <allonet/allonet.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <math.h>

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

static void interaction(
    alloclient *client, 
    const char *type,
    const char *sender_entity_id,
    const char *receiver_entity_id,
    const char *request_id,
    const char *body    
)
{
    printf(
        "INTERACTION\n\tType: %s\n\tSender: %s\n\tReceiver: %s\n\tID: %s\n\tBody: %s\n", 
        type, sender_entity_id, receiver_entity_id, request_id, body
    );
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
    alloclient *client = allo_connect(argv[1]);
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
        client->poll(client);
    }
    return 0;
}
