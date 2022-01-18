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

os.environ['CFLAGS'] = f"""
-I{allo_home("lib")}
""".replace ("\n", " ")
os.environ['LDFLAGS'] = f"""
-L{ALLONET_BIN_PATH}
""".replace ("\n", " ")

ffi = FFI()
# ffi.include(queue)
ffi.set_source("allonet", f"""
 #include "{allo_home("include/allonet/allonet.h")}"
""", libraries = ["allonet"])

ffi.compile(verbose=True)
ffi.cdef(
    open(script_home("client.h"), "r").read()
)


@ffi.callback("void (void *, int, const char *)")
def cb(client, error, message):
    print("cb {client}, {error}, {message}")


@ffi.callback("void (void *, double, double)")
def clock(client, latency, server_delta):
    print("clock {client}, {latency}, {server_delta}")


allonet = ffi.dlopen(os.path.join(ALLONET_BIN_PATH, "liballonet.dylib"))
