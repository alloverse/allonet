from cffi import FFI
import os

MODULE_PATH = os.path.dirname(os.path.abspath(__file__))
ALLONET_PATH = os.path.join(MODULE_PATH, "../../..")
ALLONET_BIN_PATH = os.path.join(ALLONET_PATH, "build/Debug")

# The allonet python module directory path
def script_home(file):
    return os.path.join(MODULE_PATH, file)

# The allonet source directory path
def allo_home(file):
    return os.path.join(ALLONET_PATH, file)

ffi = FFI()
ffi.cdef(
"""
int alloclient_simple_init(int threaded);
int alloclient_simple_free();
char *alloclient_simple_communicate(const char * const command);
"""
)

allonet = ffi.dlopen(os.path.join(ALLONET_BIN_PATH, "liballonet.dylib"))