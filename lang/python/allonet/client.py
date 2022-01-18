from mimetypes import init
from .lib import allonet, ffi, cb, clock

def b(str):
    return ffi.new('char[]', str.encode('ascii'))


class Client(object):

    def __init__(self, threaded = False):
        self.client = allonet.alloclient_create(threaded)
        # self.client.disconnected_callback = cb
        # self.client.clock_callback = clock
        print(self.client.disconnected_callback)
        
    """ 
    Connect to a place
    
    Blocking. Returns true if connection was successful
    """
    def connect(self, host, display_name, avatar_spec):
        return allonet.alloclient_connect(self.client, b(host), b(display_name), b(avatar_spec))

    """
    Poll the internal message queues
    """
    def poll(self, timeout = 100):
        allonet.alloclient_poll(self.client, timeout)

    def onDisconnected(self):
        print("did connect")