#include <allonet/state/state_write.h>
#include <stdlib.h>
#include <string.h>
#include "../util.h"

using namespace Alloverse;
using namespace std;

allo_mutable_state::allo_mutable_state()
{
    revision = 1;
    flatlength = 0;
    flat = NULL;
    _cur = NULL;
}

