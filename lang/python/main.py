from cffi import FFI
from allo import lib as allonet

if __name__ == "__main__":
    client = allonet.alloclient_create(False)
    print(client)
    allonet.alloclient_connect(client, b"alloplace://localhost", b"""{"display_name": "The snek language"}""", b"""
    {
	"children": [
        {
            "geometry": {
                "type": "hardcoded-model",
                "name": "head"
            },
            "intent": {
                "actuate_pose": "head"
            }
        }
	],
	"collider": {
		"type": "box",
		"width": 1,
		"height": 1,
		"depth": 1
	},
	"geometry": {
		"type": "inline",
		"vertices": [
			[0, 0, 0],
			[1, 0, 0],
			[1, 1, 0],
			[0, 1, 0],
			[1, 1, 1]
		],
		"uvs": [
			[0, 0],
			[1, 0],
			[1, 1],
			[0, 1],
			[1, 1]
		],
		"triangles": [
			[0, 1, 2],
			[0, 2, 3],
			[4, 0, 1],
			[4, 2, 1]
		],
		"texture": "iVBORw0KGgoAAAANSUhEUgAAABAAAAAQCAYAAAAf8/9hAAAAAXNSR0IArs4c6QAAAAlwSFlzAAAOxAAADsQBlSsOGwAAAVlpVFh0WE1MOmNvbS5hZG9iZS54bXAAAAAAADx4OnhtcG1ldGEgeG1sbnM6eD0iYWRvYmU6bnM6bWV0YS8iIHg6eG1wdGs9IlhNUCBDb3JlIDUuNC4wIj4KICAgPHJkZjpSREYgeG1sbnM6cmRmPSJodHRwOi8vd3d3LnczLm9yZy8xOTk5LzAyLzIyLXJkZi1zeW50YXgtbnMjIj4KICAgICAgPHJkZjpEZXNjcmlwdGlvbiByZGY6YWJvdXQ9IiIKICAgICAgICAgICAgeG1sbnM6dGlmZj0iaHR0cDovL25zLmFkb2JlLmNvbS90aWZmLzEuMC8iPgogICAgICAgICA8dGlmZjpPcmllbnRhdGlvbj4xPC90aWZmOk9yaWVudGF0aW9uPgogICAgICA8L3JkZjpEZXNjcmlwdGlvbj4KICAgPC9yZGY6UkRGPgo8L3g6eG1wbWV0YT4KTMInWQAAAttJREFUOBFtU0tsjFEUPvfx//O3TJ+mbToe1RSRSJQfRYqJZFTSxkYqsbBgMUQ0jddCBLMRQbGxaVhgIRGWFiIaDYsmfaALbUiakopH+phW6Mz8/3049zczunCSm3PuOd+553kBchSLJXleNlxrTSD2kidjwAM5Z9TJGG8HYAuxNJbUBef6tpvH6luvHVkIMPKZjXWHDzRUteT1gwnXQpmQvAL5Drej71bFyq0uKAW/Jgb6+obfnDrVezwbb2++3VgT3j6X8eDtt9knB58OnUf8R+NrHihbd+jRjSVrWo4UlZWCEuArTFBlwZob6IL4zBXoWL0NFtvaz2rCihilo1Nz6tXnySune0cvk8bE83e1bny9TINUGnTGk5x9eQ3R711+JJy25lk1ZFJT/n6WtjZVONqxbaEBeNjm5PHIxAvulC6txmgghQJveoxHPt2BGvoQ7PItlq/C2kFjqKLcepBeBIM/Zsm+sLCWhh0/K5UVLSleRrXy0xRTznhZEh27ACuKe4AtioOQHKj2iCKUgJRQ53D4VFkFd1NaCyEIVgKekFnTfQtTwrkpsIqqQJEMaKyHEKwHqLEEvfawvlIMFApZROrAAzHEooSwDCiDQqXykePB4fyFGP0/kgaCDxkyDUNvoCKdcngxAGW2h1ngU2YwCDJRCifwCSySUIVRRdixIC1lhAkpRuzi+s28pLZmSaqHOHTSl9rBHFRhRzBVs5kaGPMd4fGtjuL9X1Mf7r8ZP5EHkVD8+sk9of7OhlqxXJMQ9k346GO2zVQkKGWUcU6nP0/M14y/v3R1ZKYrsLlutzU0dNQUb4g1N204W7cscrE6UlmE3VYYV1g2t3/+/A0TX77de9Y7cA5x3w242w3W2YhA3MRgEC24AUT37m56kDi0T3cm2vX+tl3DqN+Zs0F37h/k7ws5Sfw1BrpV9dHWJnft8TwgiT8R5dxs89r/c5YDF6yxWOBcuOeFP4paJwh7m0NPAAAAAElFTkSuQmCC"
	}
}
    """)

while True:
    allonet.alloclient_poll(client, 100)
