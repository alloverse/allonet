#include "allonet_version.h"
#define str(s) #s

const char *GetAllonetVersion()
{
    return str(ALLONET_VERSION);
}
const char *GetAllonetNumericVersion()
{
    return str(ALLONET_NUMERIC_VERSION);
}
const char *GetAllonetGitHash()
{
    return str(ALLONET_HASH);
}

int GetAllonetProtocolVersion()
{
    return ALLONET_MAJOR_VERSION;
}