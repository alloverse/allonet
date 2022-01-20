from sqlite3 import Date
from .lib import allonet, ffi
import json
from time import time

def b(string):
    return ffi.new('char[]', string.encode('ascii'))

def e(string):
    return string.replace('"','\\"')

def j(string_or_obect):
    if isinstance(string_or_obect, str): return string_or_obect
    return json.dumps(string_or_obect)

class Client(object):
    """
    Connects to and communicates with an alloverse place
    """

    _stat = {}

    def __init__(self, threaded = False):
        allonet.alloclient_simple_init(threaded)

    def call(self, command):
        if not isinstance(command, str):
            command = json.dumps(command)
        resultstr = ffi.string(allonet.alloclient_simple_communicate(b(command))).decode('utf-8')
        result = json.loads(resultstr)
        self.handle_events(result.get('events'))
        return result.get('return')

    def handle_events(self, events):
        if not events: return
        for event in events:
            name = event['event']
            self.stats(name)
            if name == "clock":
                self.onClock(event['latency'], event['delta'])
            if name == 'disconnected':
                self.onDisconnected(event['code'], event['message'])
            if name == 'state':
                self.onState(event['state'])
            if name == 'interaction':
                self.onInteraction(event['interaction'])
        
    def connect(self, host, identity, avatar_spec):
        """ 
        Connect to a place
        Blocking. Returns true if connection was successful
        --- 
        host: str 
            'alloplace://' url to connect to
        identity: str
            json describing your identity. 
            Must contain the "display_name" key
            Example: '{"display_name": "Hello"}'
        avatar_spec: str
            Describes your in-world avatar
        """

        self.call({
            "op": "connect",
            "url": host,
            "identity": j(identity),
            "avatar_spec": j(avatar_spec)
        })
        
    """
    Poll the internal message queues
    """
    def poll(self, timeout = 100):
        self.call({
            "op": 0,
            "timeout": timeout
        })


    def intent(self, zmovement = 0, xmovement = 0, yaw = 0, pitch = 0):
        self.call({
            "op": "intent",
            "intent": {
                "zmovement": zmovement,
                "xmovement": xmovement,
                "yaw": yaw,
                "pitch": pitch
            }
        })

    def stats(self, name):
        now = time()
        s = self._stat.get(name) or type('',(object,),{"count": 0, "time": now})()
        s.count = s.count + 1

        d = now - s.time
        if d > 1:
            n = s.count / d
            print("Stats: {}: {:.2f} times per second".format(name, n))
            s.time = now
            s.count = 0

        self._stat[name] = s

    def onDisconnected(self, code, message):
        print("Client was disconnected")

    def onClock(self, latency, delta):
        # print(f"Got clock data {latency}, {delta}")
        pass

    def onState(self, state):
        # print(f"Got state")
        pass

    def onInteraction(self, interaction):
        print(f"Got interaction {interaction}")