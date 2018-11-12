#include <allonet/allonet.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <sys/select.h>
#include <termios.h>

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

int main(int argc, char **argv)
{
    set_conio_terminal_mode();

    printf("hello microverse\n");
    alloclient *client = allo_connect("alloplace://localhost");
    for(;;)
    {
        if(kbhit()) {
            int ch = getch();
            allo_client_intent intent = {
                .zmovement = ch=='w' ? 1 : ch == 's' ? -1 : 0,
                .xmovement = ch=='d' ? 1 : ch == 'a' ? -1 : 0,
            };
            client->set_intent(client, intent);
        }
        client->poll(client);
    }
    return 0;
}
