#include <allonet/allonet.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <math.h>

int main(int argc, char **argv) {
    printf("hello microverse\n");
    alloclient *client = allo_connect("alloplace://localhost");

    return 0;
}
