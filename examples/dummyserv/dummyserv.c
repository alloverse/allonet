#include <allonet/allonet.h>
#include <stdio.h>
#include <time.h>

double gettime() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec/(double)1e9;
}

int main(int argc, char **argv) {
    printf("hello world\n");
    alloserver *serv = allo_listen();

    double time_per_frame = 1/20.0;

    for(;;) {
        double frame_start = gettime();
        for(;;) {
            double elapsed = gettime() - frame_start;
            double remaining = time_per_frame - elapsed;
            if(elapsed > time_per_frame) break;
            serv->interbeat(serv, remaining*1000);
        }
        serv->beat(serv);
    }

    return 0;    
}