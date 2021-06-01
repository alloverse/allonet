#include "allonet_version.h"

#define STR_VALUE(arg)      #arg
#define STR(name) STR_VALUE(name)

const char *GetAllonetVersion()
{
    return STR(ALLONET_VERSION);
}
const char *GetAllonetNumericVersion()
{
    return STR(ALLONET_NUMERIC_VERSION);
}
const char *GetAllonetGitHash()
{
    return STR(ALLONET_HASH);
}

int GetAllonetProtocolVersion()
{
    return ALLONET_MAJOR_VERSION;
}