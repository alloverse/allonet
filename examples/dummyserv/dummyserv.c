#include <allonet/allonet.h>
#include <stdio.h>
#include <time.h>

int main(int argc, char **argv) {
    printf("hello world\n");
    alloserver *serv = allo_listen();

    clock_t time_per_frame = CLOCKS_PER_SEC/20;

    for(;;) {
        clock_t frame_start = clock();
        for(;;) {
            clock_t remaining = (frame_start + time_per_frame) - clock();
            if(remaining < 0) break;
            serv->interbeat(serv, remaining / CLOCKS_PER_SEC);
        }
        serv->beat(serv);
    }

    return 0;    
}