#include <allonet/allonet.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

double gettime()
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec/(double)1e9;
}

allo_state dummystate;

static void move_stuff_around(alloserver *serv, double delta)
{
    alloserver_client *client = NULL;
    LIST_FOREACH(client, &serv->clients, pointers) {
        
        // fetch entity, or create if it doesn't have one
        // move it based on its intent
    }
}

int main(int argc, char **argv) {
    printf("hello world\n");
    alloserver *serv = allo_listen();
    dummystate.entities = (allo_entity*)calloc(128, sizeof(allo_entity));

    double time_per_frame = 1/20.0;

    for(;;) {
        double frame_start = gettime();
        for(;;) {
            double elapsed = gettime() - frame_start;
            double remaining = time_per_frame - elapsed;
            if(elapsed > time_per_frame) break;
            serv->interbeat(serv, remaining*1000);
        }
        move_stuff_around(serv, time_per_frame);
        serv->beat(serv);
    }

    return 0;    
}