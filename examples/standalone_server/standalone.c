#include <allonet/server.h>

int main()
{
  return alloserv_run_standalone("localhost", 0, 21337, "Standalone");
}
