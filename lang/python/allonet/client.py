from .lib import allonet, ffi
import json

def b(string):
    return ffi.new('char[]', string.encode('ascii'))

def e(string):
    return string.replace('"','\\"')

class Client(object):

    def __init__(self, threaded = False):
        allonet.alloclient_simple_init(False)

    def call(self, command):
        if not isinstance(command, str):
            command = json.dumps(command)
        result = ffi.string(allonet.alloclient_simple_communicate(b(command))).decode('utf-8')
        print(result)
        return json.loads(result)
        
    """ 
    Connect to a place
    
    Blocking. Returns true if connection was successful
    """
    def connect(self, host, identity, avatar_spec):
        self.call({
            "op": "connect",
            "url": host,
            "identity": identity,
            "avatar_spec": avatar_spec
        })
        
    """
    Poll the internal message queues
    """
    def poll(self, timeout = 100):
        self.call({
            "op": 0,
            "timeout": 100
        })
        

    def onDisconnected(self):
        print("did connect")