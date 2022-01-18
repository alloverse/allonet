from cffi import FFI
import os

Here = os.path.dirname(os.path.abspath(__file__))
def path(file):
    return os.path.join(Here, file)

os.environ['CFLAGS'] = f"""
-I{path("../../lib")}
""".replace ("\n", " ")
os.environ['LDFLAGS'] = f"""
-L{path("../../build/Debug")}
""".replace ("\n", " ")

queue = FFI()
queue.set_source("queue", f"""
#include "{path("../../lib/inlinesys/queue.h")}"
""")

ffi = FFI()
# ffi.include(queue)
ffi.set_source("allonet", f"""
 #include "{path("../../include/allonet/allonet.h")}"
""", libraries = ["allonet"])



# ffi.compile(verbose=True)
ffi.cdef(open(path("client.h"), "r").read())
lib = ffi.dlopen(path("../../build/Debug/liballonet.dylib"))
lib